
#ifndef _S3FS_DB_H_
#define _S3FS_DB_H_

#include <list>
#include <map>
#include <string>
#include <pthread.h>
#include <sqlite3.h>

#define uint64_t long long int


typedef struct _S3DB_INFO_S {
    std::string  strFile;
    uint64_t   n64Id;
    int          nOperator;
    int          nStatus;

    _S3DB_INFO_S() {
        strFile.clear();
        n64Id = 0;
        nOperator = 0;
        nStatus = 0;
    }
}S3DB_INFO_S;
typedef std::list<S3DB_INFO_S> S3DB_LIST_S;

#define S3DB_OP_ADD 1
#define S3DB_OP_DEL 2

#define S3DB_STATUS_INIT       1
#define S3DB_STATUS_PROCESSING 2
#define S3DB_STATUS_DONE       3



class S3DB
{
    public:
        static S3DB & Instance(void) {
            return m_instance;
        }
        
        int init(const char *pDBFile);

        int insertDB(const char *pFile, int nOperator, int nStatus);
        int updateDB(uint64_t id, int nStatus);
        int removeDB(uint64_t id);
        int queryAheadDB(S3DB_LIST_S &list, int count = 10);
        int queryDB(S3DB_LIST_S &list, const char *pFile = NULL, int nOperator = -1, int nStatus = -1, int nCount = -1);
    private:
        S3DB();
        ~S3DB();

        int insertDB(uint64_t id, const char *pFile, int nOperator, int nStatus);

        static int queryMaxIDCB(void *para, int argc, char **argv, char **azColName);
        static int queryCB(void *para, int argc, char **argv, char **azColName);

        uint64_t getNextID(void);
        
        int createTable(void);
        int loadMaxID(void);        
   
    private:
        static S3DB      m_instance;
        std::string      m_strDbFile;
        uint64_t         m_n64MaxID;
        pthread_mutex_t  m_stLock;

        sqlite3         *m_pSql3;
};


#endif // _S3FS_DB_H_


