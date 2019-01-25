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

S3fsOper::S3fsOper(const char *dstFile, const char *srcFile):m_dstEnt(dstFile), m_srcEnt(srcFile)
{
    m_dstEnt.init();
    m_srcEnt.init();
}

S3fsOper::~S3fsOper()
{
}


//s3fs_getattr
int S3fsOper::getattr(struct stat* stbuf)
{
    int result;

    // check parent directory attribute.
    if(0 != (result = check_parent_object_access(m_dstEnt.path(), X_OK))){
        return result;
    }
    
    if(0 != (result = checkaccess(m_dstEnt, F_OK))){
        return result;
    }

    if(stbuf){
        stbuf->st_blksize = 4096;
        stbuf->st_blocks  = get_blocks(stbuf->st_size);
    }
    
    S3FS_PRN_DBG("[path=%s] uid=%u, gid=%u, mode=%04o", m_dstEnt.path(), (unsigned int)(stbuf->st_uid), (unsigned int)(stbuf->st_gid), stbuf->st_mode);
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
    if(-ENOENT != (result = checkaccess(m_dstEnt, F_OK))){
        if(0 == result){
            result = -EEXIST;
        }
        return result;
    }

    struct stat &st = m_dstEnt.stat();

    st.st_uid   = pcxt->uid;
    st.st_gid   = pcxt->gid;
    st.st_mode  = mode | S_IFDIR;
    st.st_mtime = time(NULL);
    st.st_nlink = 2;

    result = m_dstEnt.build();
    if (0 == result) {
        S3DB_INFO_S record(m_dstEnt.path(), S3DB_OP_ADD, st.st_mode);
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













int S3fsOper::checkaccess(VfsEnt &ent, int mask)
{
    int result;
    struct stat & st = ent.stat();
    struct fuse_context* pcxt;

    if(NULL == (pcxt = fuse_get_context())){
        return -EIO;
    }

    if (0 != ent.errno())
    {
        return -ent.errno();
    }

    if(0 == pcxt->uid){
        // root is allowed all accessing.
        return 0;
    }
    if(is_s3fs_uid && s3fs_uid == pcxt->uid){
        // "uid" user is allowed all accessing.
        return 0;
    }
    if(F_OK == mask){
        // if there is a file, always return allowed.
        return 0;
    }

    // for "uid", "gid" option
    uid_t  obj_uid = (is_s3fs_uid ? s3fs_uid : st.st_uid);
    gid_t  obj_gid = (is_s3fs_gid ? s3fs_gid : st.st_gid);

    // compare file mode and uid/gid + mask.
    mode_t mode;
    mode_t base_mask = S_IRWXO;
    if(is_s3fs_umask){
        // If umask is set, all object attributes set ~umask.
        mode = ((S_IRWXU | S_IRWXG | S_IRWXO) & ~s3fs_umask);
    }else{
        mode = st.st_mode;
    }
    if(pcxt->uid == obj_uid){
        base_mask |= S_IRWXU;
    }
    if(pcxt->gid == obj_gid){
        base_mask |= S_IRWXG;
    }
    if(1 == is_uid_include_group(pcxt->uid, obj_gid)){
        base_mask |= S_IRWXG;
    }
    mode &= base_mask;

    if(X_OK == (mask & X_OK)){
        if(0 == (mode & (S_IXUSR | S_IXGRP | S_IXOTH))){
            return -EPERM;
        }
    }
    if(W_OK == (mask & W_OK)){
        if(0 == (mode & (S_IWUSR | S_IWGRP | S_IWOTH))){
            return -EACCES;
        }
    }
    if(R_OK == (mask & R_OK)){
        if(0 == (mode & (S_IRUSR | S_IRGRP | S_IROTH))){
            return -EACCES;
        }
    }
    if(0 == mode){
        return -EACCES;
    }
    return 0;
}



