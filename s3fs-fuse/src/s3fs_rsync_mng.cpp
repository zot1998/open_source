
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "common.h"

#include "s3fs_util.h"
#include "s3fs_rsync_util.h"
#include "s3fs_rsync_mng.h"

bool       S3RSyncMng::m_bRunFlag = true;
S3RSyncMng S3RSyncMng::m_instance;

S3RSyncMng::S3RSyncMng() {

    m_stThreadId = (pthread_t)-1;
    
}

S3RSyncMng::~S3RSyncMng() {
}

int S3RSyncMng::start(void){
    int rc = 0;

    rc = S3DB::Instance().init();
    if (rc) {
        return rc;
    }


    pthread_attr_t attr;
    pthread_attr_init(&attr);
    m_bRunFlag = true;
    
    rc = pthread_create(&m_stThreadId, &attr, run, NULL);
    if (rc) {
        m_stThreadId = -1;
        S3FS_PRN_ERR("Failed to create thread(%d)", rc);
        return rc;
    }
    
    return 0;
}

void S3RSyncMng::stop(void) {
    m_bRunFlag = false;
    if ((pthread_t)-1 != m_stThreadId) {
        pthread_join(m_stThreadId, NULL);
    }
}






void *S3RSyncMng::run(void *pArg) {
    int rc = 0;
    while (m_bRunFlag) {
        rc = S3RSyncMng::Instance().rsync();
        if (rc) {
        }
           
        
        usleep(1000*1000);
    }

    return NULL;
}




int S3RSyncMng::rsync(void){
    int rc = 0;
    S3DB_LIST_S lst;
    rc = S3DB::Instance().queryAheadDB(lst, 1); 
    if (rc) {
        S3FS_PRN_ERR("Sync queryAheadDB failed(%d)", rc);
        return rc;
    }
    
    if (0 == lst.size()) {
        //S3FS_PRN_INFO("Sync no record");
        return 0;
    }
    
    S3DB_INFO_S info = *lst.begin();
    
    lst.clear();
    rc = S3DB::Instance().queryDB(lst, info.strFile.c_str());
    if (rc) {
        S3FS_PRN_ERR("Sync queryDB failed(%d)", rc);
        return rc;
    }

    if (0 == lst.size()) {
        S3FS_PRN_ERR("Sync queryDB Exception: no data");
        return 0;
    }

    /*
        TODO...
        场景: 1. 数据库列表为 删除目录a, 创建文件a ,那么按下面会忽略删除目录a...

    */
    if (lst.size() > 1) {
        S3DB_INFO_S last = lst.back();
        lst.pop_back();

        while (lst.begin() != lst.end()) {
            info = *lst.begin();

            rc = S3DB::Instance().removeDB(info.n64Id);
            if (rc) {
                S3FS_PRN_ERR("Sync file(%s) last (id:%lld, op:%d), ignore (id:%lld, op:%d), but remove db failed(%d)", 
                    info.strFile.c_str(), last.n64Id, last.nOperator, info.n64Id, info.nOperator, rc);
                
                return rc;
            } else {
                S3FS_PRN_INFO("Sync file(%s) last (id:%lld, op:%d), ignore (id:%lld, op:%d)", 
                    info.strFile.c_str(), last.n64Id, last.nOperator, info.n64Id, info.nOperator);
            }
            
            lst.pop_front();
        }

        return 0;
    }else {
        info = *lst.begin();

        S3RSyncUtil s3(info);
        rc = s3.init();
        if (rc) {
            S3FS_PRN_ERR("sync.init(id:%lld, file:%s,op:%d) failed(%d)", info.n64Id, info.strFile.c_str(), info.nOperator, rc);
            return rc;
        }

        rc = s3.sync();
        if (0 == rc) {
            rc = S3DB::Instance().removeDB(info.n64Id);
            if (rc) {
                S3FS_PRN_ERR("Sync file(%s, id:%lld) success , but remove db failed(%d)", 
                    info.strFile.c_str(), info.n64Id, rc);
            }
        }

        return rc;
    }
}





