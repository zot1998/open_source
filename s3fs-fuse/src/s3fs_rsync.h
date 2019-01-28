
#ifndef __S3FS_RSYNC_H
#define __S3FS_RSYNC_H

#include <list>
#include <map>
#include <string>
#include <pthread.h>
#include "s3fs_db.h"


typedef int (*RSYNC_ADD)(const char *pFile, mode_t mode);
typedef int (*RSYNC_DEL)(const char *pFile, mode_t mode);


class S3RSync
{
    public:
        static S3RSync & Instance(void) {
            return m_instance;
        }
        
        int  start(void);
        void stop(void);

        void setSyncAddFunc(RSYNC_ADD func) { m_syncAddFunc = func;}
        void setSyncDelFunc(RSYNC_DEL func) { m_syncDelFunc = func;}
        
        int  rsync(void);

    private:
        S3RSync();
        ~S3RSync();

        void resetStatus(void);


        static void * run(void *pArg);


   
    private:
        static S3RSync   m_instance;
        static bool      m_bRunFlag;
         
        pthread_t        m_stThreadId;        
        RSYNC_ADD        m_syncAddFunc;
        RSYNC_DEL        m_syncDelFunc;
        
};






#endif // __S3FS_RSYNC_H


