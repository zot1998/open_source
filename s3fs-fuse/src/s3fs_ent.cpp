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
#include "curl.h"
#include "cache.h"
#include "string_util.h"
#include "s3fs_util.h"
#include "fdcache.h"
#include "s3fs_auth.h"
#include "addhead.h"




Ent::Ent(const char *path):m_strPath(rebuild_path(path, false)), 
                                 m_strPathDir(m_strPath + "/")
                                  
{
    m_bExists = false;
    memset(&m_stAttr, 0, sizeof(m_stAttr));
}
Ent::Ent(const std::string &path):m_strPath(rebuild_path(path.c_str(), false)), 
                                        m_strPathDir(m_strPath + "/")
{
    m_bExists = false;
    memset(&m_stAttr, 0, sizeof(m_stAttr));
}
Ent::~Ent()
{
    
}
int Ent::init(void)
{    
    return 0;
}





VfsEnt::VfsEnt(const char *path):Ent(path)
                                  
{
    m_strCachePath.clear();
}
VfsEnt::VfsEnt(const std::string &path):Ent(path)
{
    m_strCachePath.clear();
}
VfsEnt::~VfsEnt()
{
    
}

int VfsEnt::init(void)
{    
    int rc = 0;
    rc = stat(m_strCachePath.c_str(), &m_stAttr);
    if (0 == rc) {
        m_bExists = true;
    } else if (ENOENT != errno) {
        S3FS_PRN_ERR("%s local stat failed(rc:%d,error:%d)", rc, -errno); 
        return -errno;
    }
    
    return 0;
}

int VfsEnt::create(void) 
{
    if (S_ISDIR(stbuf->st_mode)) {

    } else {


    }

    return 0;
}



S3Ent::S3Ent(const char *path):Ent(path),m_strMatchPath(m_strPath)
                                  
{
    m_bEmptyDir = false;
}
S3Ent::S3Ent(const std::string &path):Ent(path),m_strMatchPath(m_strPath)                             
{
    m_bEmptyDir = false;
}
S3Ent::~S3Ent()
{
    
}
int S3Ent::init(void)
{
    int          result = -1;
    headers_t    meta;    
    S3fsCurl     s3fscurl;
    bool         forcedir = false;

    if(0 == strcmp(m_strPath.c_str(), "/") || 0 == strcmp(m_strPath.c_str(), ".")){
        m_stAttr.st_nlink = 1; // see fuse faq
        m_stAttr.st_mode  = mp_mode;
        m_stAttr.st_uid   = is_s3fs_uid ? s3fs_uid : mp_uid;
        m_stAttr.st_gid   = is_s3fs_gid ? s3fs_gid : mp_gid;

        m_bExists  = true;

        return 0;
    } 

    result = s3fscurl.HeadRequest(m_strPath.c_str(), meta);
    s3fscurl.DestroyCurlHandle();
    // if not found target path object, do over checking
    if(0 != result){
        result  = s3fscurl.HeadRequest(m_strPathDir.c_str(), meta);
        s3fscurl.DestroyCurlHandle();
        if (0 == result) {
            m_strMatchPath = m_strPathDir;
        } else {
            if (-ENOTEMPTY == directory_empty(m_strPath.c_str())){
                result    = 0;
                forcedir = true;
            }
        }
    }else{
        if(is_need_check_obj_detail(meta)){
            // check a case of that "object" does not have attribute and "object" is possible to be directory.
            if (-ENOTEMPTY == directory_empty(m_strPath.c_str())){
                result    = 0;
                forcedir = true;
            }
        }
    }

    if (0 == result) {
        convert_header_to_stat(m_strMatchPath.c_str(), meta, &m_stAttr, forcedir);        
        if (forcedir) {
            m_bEmptyDir = false;
        } else {
            m_bEmptyDir = true;
            if (S_ISDIR(m_stAttr.st_mode)) {
                if (-ENOTEMPTY == directory_empty(m_strMatchPath.c_str())){
                    m_bEmptyDir = false;
                }
            }
        }
        
        m_bExists = true;
        
    } else if (-ENOENT == result) {
        m_bExists = false;

        result = 0;
    } 
    
    return result;
}




int S3Ent::syncAddToRemote(void)
{
    if (!m_bLocalExists) {
        return -ENOENT;
    }

    if (m_bRemoteExists) {
        //TODO...
        if ((S_ISDIR(m_stLocalAttr.st_mode) && (!S_ISDIR(m_stRemoteAttr.st_mode)))
            || (!S_ISDIR(m_stLocalAttr.st_mode) && (S_ISDIR(m_stRemoteAttr.st_mode))))
        {
            //TODO... how ??
            S3FS_PRN_WARN("%s local mode(dir:%d) is not match remote(dir:%d)", 
            m_strPath.c_str(), S_ISDIR(m_stLocalAttr.st_mode), S_ISDIR(m_stRemoteAttr.st_mode));
        }
    }

    headers_t meta;
    if (S_ISDIR(m_stLocalAttr.st_mode)) {
        meta["Content-Type"]     = string("application/x-directory");
    }
    meta["x-amz-meta-uid"]   = str(stAttr.st_uid);
    meta["x-amz-meta-gid"]   = str(stAttr.st_gid);
    meta["x-amz-meta-mode"]  = str(stAttr.st_mode);
    meta["x-amz-meta-mtime"] = str(stAttr.st_mtime);

    int rc = 0;
    if (S_ISDIR(m_stLocalAttr.st_mode)) {
        S3fsCurl s3fscurl;
        rc = s3fscurl.PutRequest(m_strPathDir.c_str(), meta, -1);    // fd=-1 means for creating zero byte object.
    } else {
        int fd = open(m_strPath.c_str(), O_RDWR);
        if (fd < 0) {
            S3FS_PRN_WARN("failed to open file(%s)", m_strPath.c_str());
            rc = -EIO;            
        } else {        
            if(m_stLocalAttr.st_size >= static_cast<size_t>(2 * S3fsCurl::GetMultipartSize()) && !nomultipart){ // default 20MB
                // Additional time is needed for large files
                time_t backup = 0;
                if(120 > S3fsCurl::GetReadwriteTimeout()){
                    backup = S3fsCurl::SetReadwriteTimeout(120);
                }
                rc = S3fsCurl::ParallelMultipartUploadRequest(m_strPath.c_str(), meta, fd);
                if(0 != backup){
                    S3fsCurl::SetReadwriteTimeout(backup);
                }
            } else {
                S3fsCurl s3fscurl(true);
                rc = s3fscurl.PutRequest(m_strPath.c_str(), meta, fd);
            }

            close(fd);
        }
    }
    
    return rc;
}

int  S3Ent::syncDelToRemote(void)
{


}





//get_local_fent
int s3fsLocalMkFile(const char* path, struct stat* pstAttr){

}

int s3fsLocalMkDir(const char* path, struct stat* pstAttr){
    
}



int s3fsLocalMk(const char* path, struct stat* pstAttr){
    if (S_ISDIR(stbuf->st_mode)) {
        return s3fsLocalMkDir(path, pstAttr);
    } else {
        return s3fsLocalMkFile(path, pstAttr);
    }
}
    string cache_path;
    FdManager::MakeCachePath(path, cache_path, false, false);
  
    if (S_ISREG(stbuf->st_mode)) {
        FdEntity *ent = NULL;
        
        S3FS_PRN_INFO("Make cache file(%s)", path);
  
        if(NULL == (ent = FdManager::get()->Open(path, NULL, static_cast<ssize_t>(stbuf->st_size), stbuf->st_mtime, false, true))){
            S3FS_PRN_ERR("Could not open file(%s). errno(%d)", path, errno);
            
            return -EIO;
        }
        
        ent->Close();
    } else if (S_ISDIR(stbuf->st_mode)) {
        int result = 0;
        S3FS_PRN_INFO("Make cache directory(%s)", path);

        result = mkdir(cache_path.c_str(), stbuf->st_mode);
        if (0 != result) {
            S3FS_PRN_ERR("Make local directory(%s, mode:0x%x) errno %d ", path, stbuf->st_mode, errno);
            return result;
        }

        //set stat
        // not opened file yet.
        struct utimbuf n_mtime;
        n_mtime.modtime = time;
        n_mtime.actime  = time;
        if(-1 == utime(cache_path.c_str(), &n_mtime)){
            S3FS_PRN_ERR("utime failed. errno(%d)", -errno);
            return -errno;
        }
    
    } else {
        S3FS_PRN_ERR("Don't support operation(%s, mode:0x%x)", path, stbuf->st_mode);
        return -EIO;
    }

   
    
    return 0;
}

int s3fsLocalRmdir(const char* path) {
    string cache_path;
    FdManager::MakeCachePath(path, cache_path, false, false);
    if (0 == cache_path.size()) {
        return 0;
    }

    int result = 0;
    result = rmdir(cache_path.c_str());
    if (0 != result) {
        S3FS_PRN_ERR("remove local directory(%s) error: %d", path, -errno);
        return result;
    }
    
    return 0;
}



//s3fs_rmdir
int s3fsRemoteRmDir(const char* path) {
    int rc = 0;

    // directory must be empty
    if(directory_empty(path) != 0){
        return -ENOTEMPTY;
    }

    struct stat stbuf = {0};
    rc = s3fsGetRemoteAttr(path, &stbuf);
    if (rc) {
        return rc;
    }

    if (!S_ISDIR(stbuf.st_mode)) {
        S3FS_PRN_WARN("remove remote dir(%s), but remote is not dir.", path);
        return 0;
    }

    std::string strPath = rebuild_path(path, true);

    S3fsCurl s3fscurl;
    rc = s3fscurl.DeleteRequest(strPath.c_str());
    s3fscurl.DestroyCurlHandle();

    return rc;
}

//s3fs_unlink
int s3fsRemoteRmFile(const char* path) {
    int rc = 0;

    struct stat stbuf = {0};
    rc = s3fsGetRemoteAttr(path, &stbuf);
    if (rc) {
        return rc;
    }

    if (S_ISDIR(stbuf.st_mode)) {
        S3FS_PRN_WARN("remove remote file(%s), but remote is dir.", path);
        return 0;
    }

    std::string strPath = rebuild_path(path, false);
    S3fsCurl s3fscurl;
    rc = s3fscurl.DeleteRequest(strPath.c_str());

    return rc;
}
int s3fsRemoteRm(const char* path, mode_t mode) {    
    if (S_ISDIR(mode)) {
        return s3fsRemoteRmDir(path);
    } else {
        return s3fsRemoteRmFile(path);
    }
}





int s3fsRemoteMk(const char* path, mode_t mode) {
    if (S_ISDIR(mode)) {
        return s3fsRemoteMkDir(path);
    } else {
        return s3fsRemoteMkFile(path);
    }
}




