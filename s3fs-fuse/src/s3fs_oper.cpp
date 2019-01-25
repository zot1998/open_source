#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/tree.h>
#include <curl/curl.h>
#include <pwd.h>
#include <grp.h>
#include <getopt.h>
#include <signal.h>

#include <fstream>
#include <vector>
#include <algorithm>
#include <map>
#include <string>
#include <list>
#include "autofilelock.h"
#include "s3fs_oper.h"

S3fsOper::S3fsOper(const char *dstFile, const char *srcFile):m_DstEnt(dstFile), m_SrcEnt(srcFile)
{
    m_DstEnt.init();
    m_SrcEnt.init();
}

S3fsOper::~S3fsOper()
{
}


//s3fs_getattr
int S3fsOper::getattr(struct stat* stbuf)
{
    int result;

    S3FS_PRN_INFO("[path=%s]", path);

    // check parent directory attribute.
    if(0 != (result = check_parent_object_access(path, X_OK))){
        return result;
    }
    if(0 != (result = check_object_access(path, F_OK, stbuf))){
        return result;
    }

    if(stbuf){
        stbuf->st_blksize = 4096;
        stbuf->st_blocks  = get_blocks(stbuf->st_size);
    }
    S3FS_PRN_DBG("[path=%s] uid=%u, gid=%u, mode=%04o", path, (unsigned int)(stbuf->st_uid), (unsigned int)(stbuf->st_gid), stbuf->st_mode);
    S3FS_MALLOCTRIM(0);

  return result;
}

int S3fsOper::readlink(const char* path, char* buf, size_t size)
{
    if(!path || !buf || 0 >= size){
        return 0;
    }
    // Open
    FdEntity*   ent;
    if(NULL == (ent = get_local_fent(path))){
        S3FS_PRN_ERR("could not get fent(file=%s)", path);
        return -EIO;
    }
    // Get size
    size_t readsize;
    if(!ent->GetSize(readsize)){
        S3FS_PRN_ERR("could not get file size(file=%s)", path);
        FdManager::get()->Close(ent);
        return -EIO;
    }
    if(size <= readsize){
        readsize = size - 1;
    }
    // Read
    ssize_t ressize;
    if(0 > (ressize = ent->Read(buf, 0, readsize))){
        S3FS_PRN_ERR("could not read file(file=%s, ressize=%jd)", path, (intmax_t)ressize);
        FdManager::get()->Close(ent);
        return static_cast<int>(ressize);
    }
    buf[ressize] = '\0';

    // check buf if it has space words.
    string strTmp = trim(string(buf));
    strcpy(buf, strTmp.c_str());

    FdManager::get()->Close(ent);
    S3FS_MALLOCTRIM(0);

    return 0;
}

int S3fsOper::mknod(const char *path, mode_t mode, dev_t rdev)
{
    S3FS_PRN_INFO("Don't support mknod (path:%s,mode:0x%x)", path, mode);
    return -EPERM;
}

int S3fsOper::mkdir(mode_t mode)
{
    int result;
    struct fuse_context* pcxt;

    S3FS_PRN_INFO("[path=%s][mode=%04o]", path, mode);

    if(NULL == (pcxt = fuse_get_context())){
        return -EIO;
    }

    // check parent directory attribute.
    if(0 != (result = check_parent_object_access(path, W_OK | X_OK))){
        return result;
    }
    if(-ENOENT != (result = check_object_access(path, F_OK, NULL))){
        if(0 == result){
            result = -EEXIST;
        }
        return result;
    }

    result = create_directory_object(path, mode, time(NULL), pcxt->uid, pcxt->gid);
    S3FS_MALLOCTRIM(0);

    if (0 == result) {
        S3DB_INFO_S record(rebuild_path(path, false), S3DB_OP_ADD, S_IFDIR|mode);
        S3DB::Instance().insertDB(record);
    }

    return result;
}


int S3fsOper::unlink(const char* path)
{
  int result;

  S3FS_PRN_INFO("[path=%s]", path);

  if(0 != (result = check_parent_object_access(path, W_OK | X_OK))){
    return result;
  }

  //TODO.

  return result;
}





