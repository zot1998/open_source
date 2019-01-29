
#ifndef __S3FS_FILELOCK_H
#define __S3FS_FILELOCK_H
#include <pthread.h>
#include <string>
#include <map>

class AutoLockRef
{
    public:
        AutoLockRef(){ m_ref = 1;pthread_mutex_init(&m_lock, NULL); }
        ~AutoLockRef(){ pthread_mutex_destroy(&m_lock); }
        int inc_ref(void){ return ++m_ref;}
        int dec_ref(void){ return --m_ref;}
        void lock(void) { pthread_mutex_lock(&m_lock); }
        void unlock(void) { pthread_mutex_unlock(&m_lock); }
    private:
        int             m_ref;
        pthread_mutex_t m_lock;
};


class AutoFileLock
{
    public:
        AutoFileLock(const char *file);
        ~AutoFileLock();
        static void init(void) { m_mapFile.clear(); pthread_mutex_init(&m_lock, NULL);}
    private:
        AutoLockRef * get(const char *file);
    private:
        std::string  m_strFile;
        AutoLockRef *m_pAutoLockRef;
        static std::map<std::string, AutoLockRef *> m_mapFile;
        static pthread_mutex_t m_lock;
};




#endif // __S3FS_FILELOCK_H


