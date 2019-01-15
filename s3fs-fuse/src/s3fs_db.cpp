#include <stdio.h>
#include <stdlib.h>
#include "sqlite3.h"
#include "s3fs_util.h"
#include "s3fs_db.h"


S3DB S3DB::m_instance;


S3DB::S3DB() {
    m_n64OpIndex = 0;
    m_pSql3 = NULL;
    phread_mutex_init(&m_stLock, NULL);
}

S3DB::~S3DB() {
    phread_mutex_destroy(&m_stLock);

    if (NULL != m_pSql3) {
        sqlite3_close(m_pSql3);
        m_pSql3 = NULL;
    }
    
}

void S3DB::setPath(string &stDir, string &strBucketName) {
    m_strDbDir  = stDir;
    m_strDbFile = m_strDbDir + "/" + strBucketName + "/" + "sql.db"
}

int S3DB::init(void) {
    int rc = 0;
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

    return 0;
}



int S3DB::insertDB(long int id, string &file, int nOperator, int nStatus) {
    int rc = 0;
    char *pcErrMsg = NULL;
    char *pSql = NULL;
    size_t nSqlLen = 1024 * 5;
    pSql = new char[nSqlLen];

    snprintf(pSql, nSqlLen - 1,  "INSERT INTO record values(%lld,'%s',%d,%d);", id, file.c_str(), nOperator, nStatus);

    rc = sqlite3_exec(m_pSql3, pSql, NULL, 0, &pcErrMsg);
    if(SQLITE_OK != rc){
        S3FS_PRN_ERR("SQL insert table failed(id:%lld, file:%s, op:%d,status:%d, rc:%d, msg: %s)",id, file.c_str(), nOperator, nStatus, rc, pcErrMsg);

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
    char sql[1024];

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
    char sql[1024];

    snprintf(sql, sizeof(sql) - 1,  "DELETE from record WHERE ID = %lld;", id);
    
    rc = sqlite3_exec(m_pSql3, sql, NULL, 0, &pcErrMsg);
    if(SQLITE_OK != rc){
        S3FS_PRN_ERR("SQL remove failed(id:%lld, rc:%d, msg: %s)", id,  rc, pcErrMsg);

        sqlite3_free(pcErrMsg);
        return rc;
    }


    return 0;
}

static int queryFileCB(void *para, int argc, char **argv, char **azColName) {
    int i = 0;
    S3DB_OP_LIST *pList = (S3DB_OP_LIST *)para;

    for (i = 0; i < argc; i++) {



    }

    return 0;    
}

int S3DB::queryFileDB(string &file, S3DB_OP_LIST &list) {
    int rc = 0;
    char *pcErrMsg = NULL;
    char *pSql = NULL;
    char sql[1024];

    snprintf(sql, sizeof(sql) - 1,  "SELECT ID, OPERATER, STATUS from record WHERE FILE = %s;", file.c_str());
    
    rc = sqlite3_exec(m_pSql3, sql, queryFileCB, (void *)&list, &pcErrMsg);
    if(SQLITE_OK != rc){
        S3FS_PRN_ERR("SQL remove failed(file:%lld, rc:%d, msg: %s)", file.c_str(),  rc, pcErrMsg);

        sqlite3_free(pcErrMsg);
        return rc;
    }


    return 0;
}



int S3DB::createTable(void) {
    int rc = 0;
    char *pcErrMsg = NULL;
    
    const char *sql = "CREATE TABLE IF NOT EXISTS record("  \
          "ID INTEGER PRIMARY KEY     NOT NULL," \
          "FILE       CHAR(4096)," \
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

static int S3DB::queryMaxIDCB(void *para, int argc, char **argv, char **azColName) {
    int i;
    if (argc >= 1 && NULL != argv[0] && '\0' != argv[0][0]) {
        m_n64OpIndex = strtol(argv[0], NULL, 10);
        if (m_n64OpIndex <= 0) {
            S3FS_PRN_ERR("Query mistaken max id (%lld)", m_n64OpIndex);
        }
    } else {
        m_n64OpIndex = 0;
    }

    S3FS_PRN_WARN("Query max id (%lld)", m_n64OpIndex);

   return 0;
}


int S3DB::loadTable(void) {
    int rc = 0;
    char *pcErrMsg = NULL;
    const char *sql = "SELECT MAX(ID) FROM record;";

    rc = sqlite3_exec(m_pSql3, sql, queryMaxIDCB, 0, &pcErrMsg);
    if(SQLITE_OK != rc || m_n64OpIndex < 0){
        S3FS_PRN_ERR("SQL query table max id failed(rc:%d, msg: %s)", rc, pcErrMsg);
        sqlite3_free(pcErrMsg);
        return rc;
    }

    return 0;
}




int S3DB::addRecord(string stPath) {    
    trim_path(path);

    long int n64Index = getIndex();
    

    

    return 0;
}
int S3DB::delRecord(string stPath) {
    trim_path(stPath);

    return 0;
}


long int S3DB::getIndex(void) {
    long int u64Index = 0;
    phread_mutex_lock(&m_stLock);
    u64Index = ++m_n64OpIndex;
    phread_mutex_unlock(&m_stLock);
    
    return u64Index;
}


