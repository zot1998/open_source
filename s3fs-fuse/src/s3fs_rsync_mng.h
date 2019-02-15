
#ifndef __S3FS_RSYNC_MNG_H
#define __S3FS_RSYNC_MNG_H

#include <list>
#include <map>
#include <string>
#include <pthread.h>
#include "s3fs_db.h"


class S3RSyncMng
{
    public:
        static S3RSyncMng & Instance(void) {
            return m_instance;
        }
        
        int  start(void);
        void stop(void);
        
        int  rsync(void);

    private:
        S3RSyncMng();
        ~S3RSyncMng();
        
        static void * run(void *pArg);


   
    private:
        static S3RSyncMng m_instance;
        static bool       m_bRunFlag;
         
        pthread_t        m_stThreadId;        
};






#endif // __S3FS_RSYNC_H


