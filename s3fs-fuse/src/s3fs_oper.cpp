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

#include "common.h"
#include "s3fs.h"
#include "s3fs_util.h"
#include "autofilelock.h"
#include "s3fs_var.h"
#include "s3fs_db.h"
#include "s3fs_oper.h"



//s3fs_getattr
int S3fsOper::getattr(struct stat* stbuf)
{
    // check parent directory attribute.
    if(0 != (m_nResult= check_parent_object_access(m_dstEnt, X_OK))){
        return m_nResult;
    }
    
    if(0 != (m_nResult = check_object_access(m_dstEnt, F_OK))){
        return m_nResult;
    }

    if(stbuf){
        memcpy(stbuf, &m_dstEnt.getStat(), sizeof(struct stat));
        stbuf->st_blksize = 4096;
        stbuf->st_blocks  = get_blocks(stbuf->st_size);
    }
    
    S3FS_PRN_INFO("[path=%s] uid=%u, gid=%u, mode=%04o", 
        m_dstEnt.path(), (unsigned int)(m_dstEnt.getStat().st_uid), (unsigned int)(m_dstEnt.getStat().st_gid), m_dstEnt.getStat().st_mode);
    S3FS_MALLOCTRIM(0);

    return m_nResult;
}

int S3fsOper::readlink(char* buf, size_t size)
{
    #if 0
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
    #endif

    return -EPERM;;
}

int S3fsOper::mknod(mode_t mode, dev_t rdev)
{
    S3FS_PRN_INFO("Don't support mknod (path:%s,mode:0x%x)", path(), mode);
    return m_nResult;
}

int S3fsOper::mkdir(mode_t mode)
{
    struct fuse_context* pcxt = NULL;

    S3FS_PRN_INFO("[path=%s][mode=%04o]", path(), mode);

    if(NULL == (pcxt = fuse_get_context())){
        return -EIO;
    }

    // check parent directory attribute.
    if(0 != (m_nResult = check_parent_object_access(m_dstEnt, W_OK | X_OK))){
        return m_nResult;
    }
    if(-ENOENT != (m_nResult = check_object_access(m_dstEnt, F_OK))){
        if(0 == m_nResult){
            m_nResult = -EEXIST;
        }
        return m_nResult;
    }

    struct stat &st = m_dstEnt.getStat();

    st.st_uid   = pcxt->uid;
    st.st_gid   = pcxt->gid;
    st.st_mode  = mode | S_IFDIR;
    st.st_mtime = time(NULL);
    st.st_nlink = 2;

    m_nResult = m_dstEnt.build();
    if (0 == m_nResult) {

        S3DB_INFO_S record(m_dstEnt.path(), S3DB_OP_ADD, st.st_mode);
        S3DB::Instance().insertDB(record);
    }

    return m_nResult;
}

int S3fsOper::rmdir(void)
{    
    if(0 != (m_nResult = check_parent_object_access(m_dstEnt, W_OK | X_OK))){
        return m_nResult;
    }

    if (!m_dstEnt.isExists()) {
        return -ENOENT;
    }

    
    m_nResult = m_dstEnt.remove();
    if (0 == m_nResult) {
        S3DB_INFO_S record(m_dstEnt.path(), S3DB_OP_DEL, m_dstEnt.getStat().st_mode);
        S3DB::Instance().insertDB(record);
    }

    return m_nResult;
}



int S3fsOper::unlink(void)
{
    #if 0

    if(0 != (result = check_parent_object_access(m_dstEnt.path(), W_OK | X_OK))){
        return result;
    }
    
    result = m_dstEnt.remove();
    if (0 == result) {
        S3DB_INFO_S record(m_dstEnt.path(), S3DB_OP_DEL, m_dstEnt.stat().st_mode);
        S3DB::Instance().insertDB(record);
    }
    #endif

    return m_nResult;
}

int S3fsOper::symlink(void)
{
    #if 0
    struct fuse_context* pcxt;
    
    if(NULL == (pcxt = fuse_get_context())){
        return -EIO;
    }
    if(0 != (result = check_parent_object_access(m_dstEnt.path(), W_OK | X_OK))){
        return result;
    }
    if(-ENOENT != (result = checkaccess(m_dstEnt, F_OK))){
        if(0 == result){
            result = -EEXIST;
        }
        return result;
    }

    headers_t headers;
    headers["Content-Type"]     = string("application/octet-stream"); // Static
    headers["x-amz-meta-mode"]  = str(S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO);
    headers["x-amz-meta-mtime"] = str(time(NULL));
    headers["x-amz-meta-uid"]   = str(pcxt->uid);
    headers["x-amz-meta-gid"]   = str(pcxt->gid);

    // open tmpfile
    FdEntity* ent;
    if(NULL == (ent = FdManager::get()->Open(m_dstEnt.path(), &headers, 0, -1, false, true))){
        S3FS_PRN_ERR("could not open tmpfile(errno=%d)", errno);
        return -errno;
    }
    // write(without space words)
    string  strFrom   = trim(m_srcEnt.path());
    ssize_t from_size = static_cast<ssize_t>(strFrom.length());
    if(from_size != ent->Write(strFrom.c_str(), 0, from_size)){
        S3FS_PRN_ERR("could not write tmpfile(errno=%d)", errno);
        FdManager::get()->Close(ent);
        return -errno;
    }    
    FdManager::get()->Close(ent);
    S3FS_MALLOCTRIM(0);

    
    S3DB_INFO_S record(m_dstEnt.path(), S3DB_OP_ADD, m_dstEnt.stat().st_mode);
    S3DB::Instance().insertDB(record);
    #endif
    

    return m_nResult;
}


int S3fsOper::rename(void)
{
    #if 0
    
    if(0 != (result = check_parent_object_access(to, W_OK | X_OK))){
        // not permit writing "to" object parent dir.
        return result;
    }
    if(0 != (result = check_parent_object_access(from, W_OK | X_OK))){
        // not permit removing "from" object parent dir.
        return result;
    }

    if (0 != m_srcEnt.getErrno()){
        return m_srcEnt.getErrno();
    }

    // files larger than 5GB must be modified via the multipart interface
    if(m_srcEnt.isDir()){
        result = rename_directory(m_srcEnt.path(), m_dstEnt.path());
    }else if(!nomultipart && m_srcEnt.size() >= singlepart_copy_limit){
        result = rename_large_object(m_srcEnt.path(), m_dstEnt.path());
    }else{
        if(!nocopyapi && !norenameapi){
            result = rename_object(m_srcEnt.path(), m_dstEnt.path());
        }else{
            result = rename_object_nocopy(m_srcEnt.path(), m_dstEnt.path());
        }
    }
    S3FS_MALLOCTRIM(0);
    #endif

    return m_nResult;
}

int S3fsOper::link(void)
{
    return -EPERM;
}


int S3fsOper::chmod(mode_t mode, bool iscopy)
{
    #if 0
    string strpath;
    string newpath;
    string nowcache;
    headers_t meta;
    struct stat stbuf;
    dirtype nDirType = DIRTYPE_UNKNOWN;

    if (!iscopy) {
        return -EPERM;
    }

    if(m_dstEnt.isRoot()){
        S3FS_PRN_ERR("Could not change mode for mount point.");
        return -EIO;
    }
    if(0 != (result = check_parent_object_access(m_dstEnt.path(), X_OK))){
        return result;
    }

    if (m_srcEnt.getErrno()) {
        return -m_srcEnt.getErrno();
    }
    
    if(0 != (result = checkowner(m_dstEnt))){
        return result;
    }

    if(m_dstEnt.isDir()){
        result = chk_dir_object_type(path, newpath, strpath, nowcache, &meta, &nDirType);
    }else{
        strpath  = path;
        nowcache = strpath;
        result   = get_object_attribute(strpath.c_str(), NULL, &meta);
    }
    if(0 != result){
        return result;
    }

    if(S_ISDIR(stbuf.st_mode) && IS_REPLACEDIR(nDirType)){
        // Should rebuild directory object(except new type)
        // Need to remove old dir("dir" etc) and make new dir("dir/")

        // At first, remove directory old object
        if(0 != (result = remove_old_type_dir(strpath, nDirType))){
            return result;
        }
        StatCache::getStatCacheData()->DelStat(nowcache);

        // Make new directory object("dir/")
        if(0 != (result = create_directory_object(newpath.c_str(), mode, stbuf.st_mtime, stbuf.st_uid, stbuf.st_gid))){
            return result;
        }
    }else{
        // normal object or directory object of newer version
        meta["x-amz-meta-mode"]          = str(mode);
        meta["x-amz-copy-source"]        = urlEncode(service_path + bucket + get_realpath(strpath.c_str()));
        meta["x-amz-metadata-directive"] = "REPLACE";

        if(put_headers(strpath.c_str(), meta, true) != 0){
            return -EIO;
        }
        StatCache::getStatCacheData()->DelStat(nowcache);

        // check opened file handle.
        //
        // If we have already opened file handle, should set mode to it.
        // And new mode is set when the file handle is closed.
        //
        FdEntity* ent;
        if(NULL != (ent = FdManager::get()->ExistOpen(path))){
            ent->SetMode(mode);      // Set new mode to opened fd.
            FdManager::get()->Close(ent);
        }
    }
    S3FS_MALLOCTRIM(0);
    #endif

    return m_nResult;
}

int S3fsOper::chown(uid_t uid, gid_t gid, bool iscopy)
{
    
    return -EIO;
}
int S3fsOper::utimens(const struct timespec ts[2], bool iscopy)
{
    return -EIO;
}
int S3fsOper::truncate(off_t size)
{
    return -EIO;
}
int S3fsOper::create(mode_t mode, struct fuse_file_info* fi)
{
    return -EIO;
}
int S3fsOper::open(struct fuse_file_info* fi)
{
    return -EIO;
}
int S3fsOper::read(char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    return -EIO;
}
int S3fsOper::write(const char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    return -EIO;
}
int S3fsOper::statfs(struct statvfs* stbuf)
{
    return -EIO;
}
int S3fsOper::flush(struct fuse_file_info* fi)
{
    return -EIO;
}
int S3fsOper::fsync(int datasync, struct fuse_file_info* fi)
{
    return -EIO;
}
int S3fsOper::release(struct fuse_file_info* fi)
{
    return -EIO;
}
int S3fsOper::opendir(struct fuse_file_info* fi)
{
    int mask = (O_RDONLY != (fi->flags & O_ACCMODE) ? W_OK : R_OK) | X_OK;

    if(0 == (m_nResult = check_object_access(m_dstEnt, mask))){
        m_nResult = check_parent_object_access(m_dstEnt, mask);
    }
    S3FS_MALLOCTRIM(0);

    return m_nResult;
}

int S3fsOper::readdir(void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi)
{
    if(0 != (m_nResult = check_object_access(m_dstEnt, X_OK))){
        return m_nResult;
    }

    DIR *dir = NULL;
    struct dirent *ptr = NULL;
    dir = ::opendir(m_dstEnt.cachePath());
    if (NULL == dir) {
        S3FS_PRN_ERR("opendir failed. errno(%d)", m_dstEnt.cachePath(), -errno);
        return -errno;
    }
    else {
        while((ptr = ::readdir(dir)) != NULL)
        {
            //S3FS_PRN_DBG("+++++: %s\n", ptr->d_name);
            filler(buf, ptr->d_name, 0, 0);
        }
        
        closedir(dir);
        dir = NULL;
    }

    S3FS_MALLOCTRIM(0);

    return m_nResult;
}

int S3fsOper::access(int mask)
{
    m_nResult = check_object_access(m_dstEnt, mask);
    return m_nResult;
}
int S3fsOper::setxattr(const char* name, const char* value, size_t size, int flags)
{
    return -EIO;
}
int S3fsOper::getxattr(const char* name, char* value, size_t size)
{
    return -EIO;
}
int S3fsOper::listxattr(char* list, size_t size)
{
    return -EIO;
}
int S3fsOper::removexattr(const char* name)
{
    return -EIO;
}




