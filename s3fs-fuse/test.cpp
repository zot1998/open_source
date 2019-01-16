#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "sqlite3.h"
using namespace std;

#define S3FS_PRN_ERR printf
#define S3FS_PRN_WARN printf 
sqlite3 *m_pSql3 = NULL;

typedef struct _S3DB_INFO_S {
    long int  n64Id;
    int       nOperator;
    int       nStatus;   

    _S3DB_INFO_S() {
        n64Id = 0;
        nOperator = 0;
        nStatus = 0;
    }
}S3DB_INFO_S;

typedef struct _S3DB_OP_KEY {
    long int n64Id;
    string   strPath;
}S3DB_OP_KEY;

typedef list<S3DB_INFO_S>             S3DB_OP_LIST;
typedef map<S3DB_OP_KEY, S3DB_INFO_S> S3DB_OP_KEY_MAP;

long long m_u64OpIndex = 0;

int QueryMaxIDCB(void *NotUsed, int argc, char **argv, char **azColName){
    int i;
    if (argc >= 1) {
        m_u64OpIndex = strtol(argv[0], NULL, 10);
        if (m_u64OpIndex <= 0) {
            S3FS_PRN_ERR("Query mistaken max id (%lld)", m_u64OpIndex);
            m_u64OpIndex = 0;
			return -10;
        }
    } else {
        m_u64OpIndex = 0;
    }

    S3FS_PRN_WARN("Query max id (%lld)", m_u64OpIndex);

   return 0;
}

int loadTable(void) {
    int rc = 0;
    char *pcErrMsg = NULL;
    const char *sql = "SELECT MAX(ID) FROM record;";

    rc = sqlite3_exec(m_pSql3, sql, QueryMaxIDCB, 0, &pcErrMsg);
    if(SQLITE_OK != rc){
        S3FS_PRN_ERR("SQL query table max id failed(rc:%d, msg: %s)", rc, pcErrMsg);
        sqlite3_free(pcErrMsg);
        return rc;
    }

    return 0;
}



int insertDB(unsigned long long key, string file, int nOperator, int nStatus) {
    int rc = 0;
    char *pcErrMsg = NULL;
    char *pSql = NULL;
    size_t nSqlLen = 1024 * 5;
    pSql = new char[nSqlLen];

    snprintf(pSql, nSqlLen - 1,  "INSERT INTO record values(%lld,'%s',%d,%d);", key, file.c_str(), nOperator, nStatus);

    rc = sqlite3_exec(m_pSql3, pSql, NULL, 0, &pcErrMsg);
    if(SQLITE_OK != rc){
        S3FS_PRN_ERR("SQL create table failed(rc:%d, msg: %s)", rc, pcErrMsg);
        sqlite3_free(pcErrMsg);
        return rc;
    }


    delete [] pSql;
    pSql = NULL;

    return 0;
}

int updateDB(long int id, int nStatus) {
    int rc = 0;
    char *pcErrMsg = NULL;
    char *pSql = NULL;
    size_t nSqlLen = 1024 * 5;
    pSql = new char[nSqlLen];

    snprintf(pSql, nSqlLen - 1,  "UPDATE record set STATUS = %d WHERE ID = %lld;", nStatus, id);

    rc = sqlite3_exec(m_pSql3, pSql, NULL, 0, &pcErrMsg);
    if(SQLITE_OK != rc){
        S3FS_PRN_ERR("SQL update table failed(id:%lld, status:%d, rc:%d, msg: %s)",id,  nStatus, rc, pcErrMsg);

        delete [] pSql;
        sqlite3_free(pcErrMsg);
        return rc;
    }


    delete [] pSql;
    pSql = NULL;

    return 0;
}

int removeDB(long int id) {
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
    S3DB_INFO_S data;

    for (i = 0; i < argc; i++) {
        if (NULL == azColName[i] || NULL == argv[i]) {
            S3FS_PRN_ERR("SQL mistaken data, key is NULL");
            return 0;
        }
        if (0 == strcmp(azColName[i], "ID")) {
            data.n64Id = strtol(argv[i], NULL, 10);
        } else if (0 == strcmp(azColName[i], "OPERATER")) {
            data.nOperator = atoi(argv[i]);
        } else if (0 == strcmp(azColName[i], "STATUS")) {
            data.nStatus = atoi(argv[i]);
        } else {
            S3FS_PRN_ERR("SQL mistaken data, unkown key:%s", azColName[i]);
            return 0;
        }

        pList->push_back(data);
    }

    return 0;    
}

int queryFileDB(string &file, S3DB_OP_LIST &list) {
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

int main(int n, char *x[]) {
    int rc = 0;
	
	printf("=====%d\n",sizeof(long int));
    
    rc = sqlite3_open("db", &m_pSql3);
	if (rc)
	{
		printf("open failed\n");
		return rc;
	}
	

    char *pcErrMsg = NULL;
    
    const char *sql = "CREATE TABLE IF NOT EXISTS record("  \
          "ID INTEGER PRIMARY KEY     NOT NULL," \
          "FILE       CHAR(4096)," \
          "STATUS     INT,"\
          "TIME       TEXT    NOT NULL);";

    rc = sqlite3_exec(m_pSql3, sql, NULL, 0, &pcErrMsg);
    if(SQLITE_OK != rc){
        S3FS_PRN_ERR("SQL create table failed(rc:%d, msg: %s)", rc, pcErrMsg);
        sqlite3_free(pcErrMsg);
        return rc;
    }
	
	//insertDB(2,"/root", 10,20);
	//insertDB(11,"/root", 10,20);
	
	//updateDB(11, 30);
	
	removeDB(11);
	
	
	loadTable();
	
	S3DB_OP_LIST m;
	queryFileDB("/root", m);
	
	S3DB_OP_LIST::iterator it = m.begin();
	for ( ; it != m.end(); it++) { 
		printf(" %lld\n",(*it).n64Id);
		printf(" %d\n",(*it).nOperator);
		printf(" %d\n",(*it).nStatus);	
	}
	
	
	
    
    sqlite3_close(m_pSql3);
    
    return 0;
}
