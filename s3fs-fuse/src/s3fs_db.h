
#ifndef __S3FS_DB_H
#define __S3FS_DB_H

#include <list>
#include <map>
#include <string>
#include <pthread.h>
#include <sqlite3.h>

#define int64_t long long int

#define S3DB_OP_ADD 1
#define S3DB_OP_DEL 2

#define S3DB_DIRTY_DATA   0   //需要同步数据
#define S3DB_DIRTY_META   1   //只需要同步元数据

#define S3DB_RSYNC_INIT   0
#define S3DB_RSYNC_DONE   1


typedef struct _S3DB_INFO_S {
    int64_t      n64Id;
    
    std::string  strFile;
    int          nOperator; // S3DB_OP_ADD or S3DB_OP_DEL
    int          nMode;     //mode_t
    int          nDirty;
    int          nStatus;   //reserve. not used
    int64_t      n64Size;   //file size

    _S3DB_INFO_S() {
        reset();
    }
    _S3DB_INFO_S(const struct _S3DB_INFO_S &obj) {
        n64Id = obj.n64Id;
        strFile = obj.strFile;
        nOperator = obj.nOperator;
        nMode = obj.nMode;
        nDirty = obj.nDirty;
        nStatus = obj.nStatus;
        n64Size = obj.n64Size;
    }
    _S3DB_INFO_S(const char *p, int nOp, mode_t mode, int dirty = S3DB_DIRTY_DATA, int64_t size = 0) {
        reset();
        strFile = p;
        nOperator = nOp;
        nMode = (int)mode;
        nDirty = dirty;
        n64Size = size;
    }
    void reset() {
        strFile.clear();
        n64Id = 0;
        nOperator = 0;
        nMode = 0;
        nDirty = S3DB_DIRTY_DATA;
        nStatus = 0;
        n64Size = 0; 
    }
}S3DB_INFO_S;
typedef std::list<S3DB_INFO_S> S3DB_LIST_S;



/*
思考场景1:
   本地盘创建了dir，未来得及上传同步。oss通过web创建了一个同名文件。


*/



class S3DB
{
    public:
        static S3DB & Instance(void) {
            return m_instance;
        }

        void setDir(const char *pDir);
            
        int init(void);

        int insertDB(S3DB_INFO_S &info);
        int removeDB(int64_t id);
        int queryAheadDB(S3DB_LIST_S &list, int count = 10);
        /*
        场景:
            1. 删除场景:
               本地文件删除，没有同步，由于 oss上文件还存在，获取文件时，不能又显示出来，需要查询数据
               库种是否有该文件的最后一次操作。
               
        */
        int queryLastOP(const char *pFile, int & nOperator);
        int queryDB(S3DB_LIST_S &list, const char *pFile = NULL, int nOperator = -1, int nCount = -1);        
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
        std::string      m_strDbDir;
        std::string      m_strDbFile;
        int64_t          m_n64MaxID;
        pthread_mutex_t  m_stLock;

        sqlite3         *m_pSql3;
};


#endif // __S3FS_DB_H


