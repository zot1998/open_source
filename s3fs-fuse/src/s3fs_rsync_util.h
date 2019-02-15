
#ifndef __S3FS_RSYNC_UTIL_H
#define __S3FS_RSYNC_UTIL_H

#include <list>
#include <map>
#include <string>
#include <pthread.h>
#include "s3fs_db.h"
#include "s3fs_ent.h"

class S3RSyncUtil
{
    public:
        S3RSyncUtil(S3DB_INFO_S &record);
        ~S3RSyncUtil();

        int  init(void);
        int  sync(void);
        
    private:
        int  put(void);
        int  remove(void);

        int  putdir(void);
        int  putfile(void);
        int  removedir(void);
        int  removefile(void);



   
    private:
        S3DB_INFO_S m_record;
        VfsEnt      m_vfsEnt;
        S3Ent       m_s3Ent;
         
        
};









#endif // __S3FS_RSYNC_UTIL_H


