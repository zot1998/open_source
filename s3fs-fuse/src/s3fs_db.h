
#ifndef S3FS_DB_H_
#define S3FS_DB_H_


/*
format:
opindex file    status time
u64     string  int    string

*/

typedef struct _S3DB_INFO_S {
    unsigned long long u64Index;
    int       nOperator;
    int       nStatus;    
}S3DB_INFO_S;

typedef struct _S3DB_OP_KEY {
    unsigned long long u64Index;
    string path;
}S3DB_OP_KEY;

typedef list<S3DB_INFO_S>             S3DB_OP_LIST;
typedef map<S3DB_OP_KEY, S3DB_INFO_S> S3DB_OP_KEY_MAP;


class S3DB
{
    public:
        static S3DB & Instance(void) {
            return m_instance;
        }
        int init(void);        
        
        void setPath(string &stDir, string &strBucketName);

        

        int addRecord(string stPath);
        int delRecord(string stPath);

        
        

    private:
        S3DB();
        ~S3DB();

        int insertDB(unsigned long long key, string &file, int nOperator, int nStatus);
        int updateDB(unsigned long long key, int nStatus);
        int removeDB(unsigned long long key);
        int queryDB(string &file, S3DB_OP_LIST &list);

        static int queryMaxIDCB(void *para, int argc, char **argv, char **azColName);
        

        int createTable(void);
        int loadTable(void);
        unsigned long long getIndex(void);
        

    private:
        static S3DB m_instance;
        string      m_strDbDir;
        string      m_strDbFile;
        long int    m_n64OpIndex;
        pthread_mutex_t    m_stLock;

        sqlite3    *m_pSql3;
        
        
        
        
        
        
        
};
#endif // S3FS_DB_H_


