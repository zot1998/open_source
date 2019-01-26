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

int S3fsOper::rmdir(void)
{
    int result;
    
    if(0 != (result = check_parent_object_access(m_dstEnt.path(), W_OK | X_OK))){
        return result;
    }

    if (!m_dstEnt.isExists()) {
        return -ENOENT;
    }

    
    result = m_dstEnt.remove();
    if (0 == result) {
        S3DB_INFO_S record(m_dstEnt.path(), S3DB_OP_DEL, m_dstEnt.stat().st_mode);
        S3DB::Instance().insertDB(record);
    }

    return result;
}



int S3fsOper::unlink(void)
{
    int result;

    if(0 != (result = check_parent_object_access(m_dstEnt.path(), W_OK | X_OK))){
        return result;
    }
    
    result = m_dstEnt.remove();
    if (0 == result) {
        S3DB_INFO_S record(m_dstEnt.path(), S3DB_OP_DEL, m_dstEnt.stat().st_mode);
        S3DB::Instance().insertDB(record);
    }

    return result;
}

int S3fsOper::symlink(void)
{
    int result;
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

    

    return result;
}


int S3fsOper::rename(void)
{
    int result;
    
    if(0 != (result = check_parent_object_access(to, W_OK | X_OK))){
        // not permit writing "to" object parent dir.
        return result;
    }
    if(0 != (result = check_parent_object_access(from, W_OK | X_OK))){
        // not permit removing "from" object parent dir.
        return result;
    }

    if (0 != m_srcEnt.errno()){
        return m_srcEnt.errno();
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

    return result;
}

int S3fsOper::link(void)
{
    return -EPERM;
}


int S3fsOper::chmod(mode_t mode, bool iscopy)
{
    int result;
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

    if (m_srcEnt.errno()) {
        return -m_srcEnt.errno();
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

    return 0;
}

int S3fsOper::chown(uid_t uid, gid_t gid, bool iscopy = true)
{
    return -EIO;
}
int S3fsOper::utimens(const struct timespec ts[2], bool iscopy = true)
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
    return -EIO;
}
int S3fsOper::readdir(void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi)
{
    return -EIO;
}
int S3fsOper::access(int mask)
{
    return -EIO;
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








int S3fsOper::checkaccess(VfsEnt &ent, int mask)
{
    int result;
    struct stat & st = ent.stat();
    struct fuse_context* pcxt;

    if(NULL == (pcxt = fuse_get_context())){
        return -EIO;
    }

    if (0 != ent.errno()) {
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

int S3fsOper::checkowner(VfsEnt &ent)
{
    int result;
    struct stat st;
    struct stat* pst = (pstbuf ? pstbuf : &st);
    struct fuse_context* pcxt;

    S3FS_PRN_DBG("[path=%s]", path);

    if(NULL == (pcxt = fuse_get_context())){
        return -EIO;
    }
    
    // check owner
    if(0 == pcxt->uid){
        // root is allowed all accessing.
        return 0;
    }
    
    if(is_s3fs_uid && s3fs_uid == pcxt->uid){
        // "uid" user is allowed all accessing.
        return 0;
    }
    
    if(pcxt->uid == ent.stat().st_uid){
        return 0;
    }
    
    return -EPERM;
}







