#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <map>
#include <string>
#include <list>

#include "common.h"
#include <s3fs_util.h>
#include "autofilelock.h"

std::map<std::string, AutoLockRef *> AutoFileLock::m_mapFile;
pthread_mutex_t AutoFileLock::m_lock; 

AutoFileLock::AutoFileLock(const char *file)
{
    m_strFile = rebuild_path(file);
    m_pAutoLockRef = get(m_strFile.c_str());
    m_pAutoLockRef->lock();
}


AutoFileLock::~AutoFileLock()
{    
    bool isDestory = false;
    pthread_mutex_lock(&m_lock);
    if (m_pAutoLockRef->dec_ref() <= 0) {
        m_mapFile.erase(m_strFile);
        isDestory = true;
    }
    pthread_mutex_unlock(&m_lock);
    m_pAutoLockRef->unlock();
    if (isDestory) {
        delete m_pAutoLockRef;
    }
}

AutoLockRef *AutoFileLock::get(const char *file)
{
    AutoLockRef *p = NULL;
    std::map<std::string, AutoLockRef *>::iterator it;
    
    pthread_mutex_lock(&m_lock);
    it = m_mapFile.find(m_strFile);
    if (it == m_mapFile.end()) {
        p = new AutoLockRef;
        m_mapFile[m_strFile] = p;
    } else {
        p = (*it).second;
        p->inc_ref();
    }
    pthread_mutex_unlock(&m_lock);
    
    return p;    
}



