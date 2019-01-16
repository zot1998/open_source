
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "s3fs_rsync.h"

//#include "s3fs_util.h"
#define trim_path(x)
#define S3FS_PRN_ERR printf
#define S3FS_PRN_WARN printf
#define S3FS_PRN_INFO printf

S3DB S3DB::m_instance;
bool       m_bRunFlag = true;


S3RSync::S3RSync() {
    m_strCacheDir.clear();
    m_strBucketName.clear();

    m_stThreadId = (pthread_t)-1;
    
}

S3RSync::~S3RSync() {
}

int S3RSync::init(void){
    int rc = 0;
    if (0 == m_strCacheDir.size() || 0 != m_strBucketName.size()) {
        S3FS_PRN_ERR("Not config cachedir(%s) or bucket(%s).", m_strCacheDir.c_str(), m_strBucketName.c_str());
        return -1;
    }

    std::string strDbFile = m_strCacheDir + "/db/" + m_strBucketName + "/sql.db";
    rc = S3DB::Instance().init(strDbFile.c_str());
    if (rc) {
        return rc;
    }

    rc = startThread();
    if (rc) {
        return rc;
    }

    resetStatus();
    
    return 0;
}

void S3RSync::exit(void) {
    m_bRunFlag = false;
    if ((pthread_t)-1 != m_stThreadId) {
        pthread_join(m_stThreadId, NULL);
    }
}


void S3RSync::resetStatus(void) {
    S3DB_LIST_S list;
    
    S3DB::Instance().queryDB(list, NULL, -1, S3DB_STATUS_PROCESSING);
    S3DB_LIST_S::iterator it;
    S3DB_INFO_S *pData = NULL;
    for (it = list.begin(); it != list.end(); ++it) {
        pData = &(*it);

        S3DB::Instance().updateDB(pData->n64Id, S3DB_STATUS_INIT);
    }
    list.clear();

    return 0;
}


int S3RSync::startThread(void) {
    int rc = 0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    m_bRunFlag = true;
    
    rc = pthread_create(&m_stThreadId, &attr, ThreadLoop, NULL);
    if (rc) {
        m_stThreadId = -1;
        S3FS_PRN_ERR("Failed to create thread(%d)", rc);
        return rc;
    }

    return 0;
}


void *S3RSync::ThreadLoop(void *pArg) {
    int rc = 0;
    while (m_bRunFlag) {
        rc = S3RSync::Instance().rsync();
        
        usleep(10*1000);
    }
}




int S3RSync::rsync(void){
    int rc = 0;
    S3DB_LIST_S lst;
    rc = S3DB::Instance().queryAheadDB(lst, 1); 
    if (rc) {
        S3FS_PRN_ERR("Sync queryAheadDB failed(%d)", rc);
        return rc;
    }
    
    if (0 == lst.size()) {
        S3FS_PRN_INFO("Sync no record");
        return 0;
    }
    
    S3DB_INFO_S info = *lst.begin();
    rc = S3DB::Instance().queryDB(lst, info.strFile.c_str());
    if (rc) {
        S3FS_PRN_ERR("Sync queryDB failed(%d)", rc);
        return rc;
    }

    if (0 == lst.size()) {
        S3FS_PRN_ERR("Sync queryDB Exception: no data");
        return 0;
    }

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

        if (S3DB_OP_ADD == info.nOperator && m_syncAddFunc) {
            rc = m_syncAddFunc(info.strFile.c_str());
            if (rc) {
                //...?
                S3FS_PRN_ERR("Sync add file(%s, id:%lld) failed(%d)", info.strFile.c_str(), info.n64Id, rc);
                return rc;
            }
        } 

        if (S3DB_OP_DEL == info.nOperator && m_syncDelFunc) {
            rc = m_syncDelFunc(info.strFile.c_str());
            if (rc) {
                //...?
                S3FS_PRN_ERR("Sync del file(%s, id:%lld) failed(%d)", info.strFile.c_str(), info.n64Id, rc);
                return rc;
            }
        }
        
        rc = S3DB::Instance().removeDB(info.n64Id);
        if (rc) {
            S3FS_PRN_ERR("Sync file(%s, id:%lld) success , but remove db failed(%d)", 
                info.strFile.c_str(), info.n64Id, rc);
        }

        return 0;
    }
}





