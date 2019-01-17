
#ifndef _S3FS_DB_H_
#define _S3FS_DB_H_

#include <list>
#include <map>
#include <string>
#include <pthread.h>
#include <sqlite3.h>

#define int64_t long long int

#define S3DB_OP_ADD 1
#define S3DB_OP_DEL 2

#define S3DB_STATUS_INIT       1
#define S3DB_STATUS_PROCESSING 2
#define S3DB_STATUS_DONE       3


typedef struct _S3DB_INFO_S {
    std::string  strFile;
    int64_t      n64Id;
    int          nOperator; // S3DB_OP_ADD or S3DB_OP_DEL
    int          nStatus;
    int          nMode;     //mode_t
    int64_t      n64Size;     //file size

    _S3DB_INFO_S() {
        reset();
    }
    _S3DB_INFO_S(const char *p, int nOp, mode_t nMode) {
        reset();
        strFile = p;
        nOperator = nOp;
        nMode = (int)nMode;
    }
    _S3DB_INFO_S(std::string &str, int nOp, mode_t nMode) {
        reset();
        strFile = str;
        nOperator = nOp;
        nMode = (int)nMode;
        nStatus = S3DB_STATUS_INIT;
    }
    void reset() {
        strFile.clear();
        n64Id = 0;
        nOperator = 0;
        nStatus = 0;
        nMode = 0;
        n64Size = 0; 
    }
}S3DB_INFO_S;
typedef std::list<S3DB_INFO_S> S3DB_LIST_S;



/*
˼������1:
   �����̴�����dir��δ���ü��ϴ�ͬ����ossͨ��web������һ��ͬ���ļ���


*/



class S3DB
{
    public:
        static S3DB & Instance(void) {
            return m_instance;
        }
        
        int init(const char *pDBFile);

        int insertDB(S3DB_INFO_S &info);
        int updateDB(int64_t id, int nStatus);
        int removeDB(int64_t id);
        int queryAheadDB(S3DB_LIST_S &list, int count = 10);
        /*
        ����:
            1. ɾ������:
               �����ļ�ɾ����û��ͬ�������� oss���ļ������ڣ���ȡ�ļ�ʱ����������ʾ��������Ҫ��ѯ����
               �����Ƿ��и��ļ������һ�β�����
               
        */
        int queryLastOP(const char *pFile, int & nOperator);
        int queryDB(S3DB_LIST_S &list, const char *pFile = NULL, int nOperator = -1, int nStatus = -1, int nCount = -1);        
    private:
        S3DB();
        ~S3DB();


        static int queryMaxIDCB(void *para, int argc, char **argv, char **azColName);
        static int queryCB(void *para, int argc, char **argv, char **azColName);

        int64_t getNextID(void);
        
        int createTable(void);
        int loadMaxID(void);        
   
    private:
        static S3DB      m_instance;
        std::string      m_strDbFile;
        int64_t         m_n64MaxID;
        pthread_mutex_t  m_stLock;

        sqlite3         *m_pSql3;
};


#endif // _S3FS_DB_H_


