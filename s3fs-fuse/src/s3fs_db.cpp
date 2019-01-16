#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
//#include "s3fs_util.h"
#define trim_path(x)
#define S3FS_PRN_ERR printf
#define S3FS_PRN_WARN printf
#define S3FS_PRN_INFO printf




#include "s3fs_db.h"


S3DB S3DB::m_instance;


S3DB::S3DB() {
    m_n64MaxID = 0;
    m_pSql3 = NULL;
    pthread_mutex_init(&m_stLock, NULL);
}

S3DB::~S3DB() {
    pthread_mutex_destroy(&m_stLock);

    if (NULL != m_pSql3) {
        sqlite3_close(m_pSql3);
        m_pSql3 = NULL;
    }    
}



int S3DB::init(const char *pDBFile) {
    int rc = 0;
    if (NULL == pDBFile) {
        S3FS_PRN_ERR("pDBFile is NULL");
        return -1;
    }
    
    m_strDbFile = pDBFile;
    
    rc = sqlite3_open(m_strDbFile.c_str(), &m_pSql3);
    if (rc) {
        S3FS_PRN_ERR("Can't open db(%s), failed(%d)", m_strDbFile.c_str(), rc);
        return rc;
    }

    rc = createTable();
    if (rc) {
        S3FS_PRN_ERR("Can't create db(%s), failed(%d)", m_strDbFile.c_str(), rc);
        return rc;
    }

    rc = loadMaxID();
    if (rc) {
        S3FS_PRN_ERR("Can't load maxid db(%s), failed(%d)", m_strDbFile.c_str(), rc);
        return rc;
    }

    return 0;
}

int S3DB::insertDB(const char *pFile, int nOperator, int nStatus) {
    return insertDB(getNextID(), pFile, nOperator, nStatus);
}

int S3DB::insertDB(long int id, const char *pFile, int nOperator, int nStatus) {
    int rc = 0;
    char *pcErrMsg = NULL;
    char *pSql = NULL;
    size_t nSqlLen = 1024 * 5;
    pSql = new char[nSqlLen];

    snprintf(pSql, nSqlLen - 1,  "INSERT INTO record values(%lld,'%s',%d,%d);", id, pFile, nOperator, nStatus);

    rc = sqlite3_exec(m_pSql3, pSql, NULL, 0, &pcErrMsg);
    if(SQLITE_OK != rc){
        S3FS_PRN_ERR("SQL insert table failed(id:%lld, file:%s, op:%d,status:%d, rc:%d, msg: %s)",id, pFile, nOperator, nStatus, rc, pcErrMsg);

        delete [] pSql;
        sqlite3_free(pcErrMsg);
        return rc;
    }

    delete [] pSql;

    return 0;
}

int S3DB::updateDB(long int id, int nStatus) {
    int rc = 0;
    char *pcErrMsg = NULL;
    char *pSql = NULL;
    char sql[128];

    //即使ID不存在，也不会报错
    snprintf(sql, sizeof(sql) - 1,  "UPDATE record set STATUS = %d WHERE ID = %lld;", nStatus, id);
    
    rc = sqlite3_exec(m_pSql3, sql, NULL, 0, &pcErrMsg);
    if(SQLITE_OK != rc){
        S3FS_PRN_ERR("SQL update failed(id:%lld, status:%d, rc:%d, msg: %s)", id,  nStatus, rc, pcErrMsg);

        sqlite3_free(pcErrMsg);
        return rc;
    }

    return 0;
}

int S3DB::removeDB(long int id) {
    int rc = 0;
    char *pcErrMsg = NULL;
    char *pSql = NULL;
    char sql[128];

    snprintf(sql, sizeof(sql) - 1,  "DELETE from record WHERE ID = %lld;", id);
    
    rc = sqlite3_exec(m_pSql3, sql, NULL, 0, &pcErrMsg);
    if(SQLITE_OK != rc){
        S3FS_PRN_ERR("SQL remove failed(id:%lld, rc:%d, msg: %s)", id,  rc, pcErrMsg);

        sqlite3_free(pcErrMsg);
        return rc;
    }


    return 0;
}

int S3DB::queryDB(S3DB_LIST_S &list, const char *pFile, int nOperator, int nStatus, int nCount) {
    int rc = 0;
    char *pcErrMsg = NULL;
    char *pSql = NULL;
    size_t nSqlLen = 1024 * 5;
    size_t nCurLen = 0;
    bool   bCondition = false;

    pSql = new char[nSqlLen];
    memset(pSql, 0, nSqlLen);
    
    snprintf(pSql, nSqlLen - 1,  "SELECT ID, FILE, OPERATER, STATUS from record");
    if (NULL != pFile) {
        nCurLen = strlen(pSql);
        snprintf(pSql + nCurLen, nSqlLen - nCurLen - 1, " WHERE FILE = '%s'", pFile);
        bCondition = true;
    }

    if (-1 != nOperator) {
        nCurLen = strlen(pSql);
        if (bCondition) {
            snprintf(pSql + nCurLen, nSqlLen - nCurLen - 1, " AND OPERATER=%d", nOperator);
        } else {
            snprintf(pSql + nCurLen, nSqlLen - nCurLen - 1, " WHERE OPERATER=%d", nOperator);
        }
        
        bCondition = true;
    }

    if (-1 != nStatus) {
        nCurLen = strlen(pSql);
        if (bCondition) {
            snprintf(pSql + nCurLen, nSqlLen - nCurLen - 1, " AND STATUS=%d", nStatus);
        } else {
            snprintf(pSql + nCurLen, nSqlLen - nCurLen - 1, " WHERE STATUS=%d", nStatus);
        }
        
        bCondition = true;
    }
    
    nCurLen = strlen(pSql);
    snprintf(pSql + nCurLen, nSqlLen - nCurLen - 1, " ORDER BY ID");

    if (nCount > 0) {
        nCurLen = strlen(pSql);
        snprintf(pSql + nCurLen, nSqlLen - nCurLen - 1, " limit 0, %d", nCount);
    }

    nCurLen = strlen(pSql);
    snprintf(pSql + nCurLen, nSqlLen - nCurLen - 1, ";");
    
    rc = sqlite3_exec(m_pSql3, pSql, queryCB, (void *)&list, &pcErrMsg);
    if(SQLITE_OK != rc){
        S3FS_PRN_ERR("SQL query failed(file:%s, rc:%d, msg: %s)", pFile,  rc, pcErrMsg);

        delete [] pSql;
        sqlite3_free(pcErrMsg);
        return rc;
    }

    delete [] pSql;

    return 0;
}

int S3DB::queryAheadDB(S3DB_LIST_S &list, int count) {
    return queryDB(list, NULL, -1, -1, count);
}

int S3DB::queryCB(void *para, int argc, char **argv, char **azColName) {
    int i = 0;
    S3DB_LIST_S *pList = (S3DB_LIST_S *)para;
    S3DB_INFO_S  data;

    if (NULL == pList || NULL == argv || NULL == azColName) {
        S3FS_PRN_ERR("Query ahead invalid para");
        return 0;
    }

    for (i = 0; i < argc; i++) {
        if (NULL == azColName[i] || NULL == argv[i]) {
            S3FS_PRN_ERR("SQL mistaken data, key or value is NULL");
            return 0;
        }
        if (0 == strcmp(azColName[i], "FILE")) {
            data.strFile = argv[i];
        } else if (0 == strcmp(azColName[i], "ID")) {
            data.n64Id = strtol(argv[i], NULL, 10);
        } else if (0 == strcmp(azColName[i], "OPERATER")) {
            data.nOperator = atoi(argv[i]);
        } else if (0 == strcmp(azColName[i], "STATUS")) {
            data.nStatus = atoi(argv[i]);
        } else {
            S3FS_PRN_ERR("SQL mistaken data, unkown key:%s", azColName[i]);
            return 0;
        }
    }

    pList->push_back(data);

    return 0;    
}





int S3DB::createTable(void) {
    int rc = 0;
    char *pcErrMsg = NULL;
    
    const char *sql = "CREATE TABLE IF NOT EXISTS record("  \
          "ID INTEGER PRIMARY KEY     NOT NULL," \
          "FILE       TEXT," \
          "OPERATER   INT," \
          "STATUS     INT);";

    rc = sqlite3_exec(m_pSql3, sql, NULL, 0, &pcErrMsg);
    if(SQLITE_OK != rc){
        S3FS_PRN_ERR("SQL create table failed(rc:%d, msg: %s)", rc, pcErrMsg);
        sqlite3_free(pcErrMsg);
        return rc;
    }

    return 0;
}

int S3DB::queryMaxIDCB(void *para, int argc, char **argv, char **azColName) {
    int i;
    long int *pMaxId = (long int *)para;
    if (NULL == pMaxId || NULL == argv || NULL == azColName) {
        S3FS_PRN_ERR("Query maxid invalid para");
        return 0;
    }
    
    if (argc >= 1 && NULL != argv[0] && '\0' != argv[0][0]) {
        *pMaxId = strtol(argv[0], NULL, 10);
        if (*pMaxId <= 0) {
            S3FS_PRN_ERR("Query mistaken max id (%lld)", *pMaxId);
        }
    } else {
        *pMaxId = 0;
    }

    S3FS_PRN_WARN("Query max id (%lld)", *pMaxId);

   return 0;
}


int S3DB::loadMaxID(void) {
    int rc = 0;
    char *pcErrMsg = NULL;
    const char *sql = "SELECT MAX(ID) FROM record;";

    rc = sqlite3_exec(m_pSql3, sql, queryMaxIDCB, (void *)&m_n64MaxID, &pcErrMsg);
    if(SQLITE_OK != rc || m_n64MaxID < 0){
        S3FS_PRN_ERR("SQL query table max id failed(rc:%d, msg: %s)", rc, pcErrMsg);
        sqlite3_free(pcErrMsg);
        return rc;
    }

    return 0;
}




long int S3DB::getNextID(void) {
    long int id = 0;
    pthread_mutex_lock(&m_stLock);
    id = ++m_n64MaxID;
    pthread_mutex_unlock(&m_stLock);
    
    return id;
}


