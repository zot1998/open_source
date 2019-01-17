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


//get_local_fent
int s3fsLocalCreate(const char* path, struct stat* stbuf){
    string cache_path;
    FdManager::MakeCachePath(path, cache_path, false, false);

    if (0 == cache_path.size()) {
        return 0;
    }
  
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

        result = mkdir(path, stbuf->st_mode);
        if (0 != result) {
            S3FS_PRN_ERR("Make local directory(%s, mode:0x%x) errno %d ", path, stbuf->st_mode, errno);
            return result;
        }

        //set stat
        // not opened file yet.
        struct utimbuf n_mtime;
        n_mtime.modtime = time;
        n_mtime.actime  = time;
        if(-1 == utime(cachepath.c_str(), &n_mtime)){
            S3FS_PRN_ERR("utime failed. errno(%d)", errno);
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


int s3fsGetRemoteAttr(const char* path, struct stat* pstbuf)
{
    int          result = -1;
    headers_t    tmpHead;    
    S3fsCurl     s3fscurl;
    bool         forcedir = false;

    memset(pstbuf, 0, sizeof(struct stat));
    if(0 == strcmp(path, "/") || 0 == strcmp(path, ".")){
        pstbuf->st_nlink = 1; // see fuse faq
        pstbuf->st_mode  = mp_mode;
        pstbuf->st_uid   = is_s3fs_uid ? s3fs_uid : mp_uid;
        pstbuf->st_gid   = is_s3fs_gid ? s3fs_gid : mp_gid;
        return 0;
    }

    std::string strpath;
    // At first, check path
    strpath     = rebuild_path(path, false);
    result      = s3fscurl.HeadRequest(strpath.c_str(), &tmpHead);
    s3fscurl.DestroyCurlHandle();
    // if not found target path object, do over checking
    if(0 != result){
        strpath = rebuild_path(path, true);
        result  = s3fscurl.HeadRequest(strpath.c_str(), &tmpHead);
        s3fscurl.DestroyCurlHandle();
        
        if(0 == result){
            //最后一定是'/'
            strpath = strpath.substr(0, strpath.length() - 1);
            if(-ENOTEMPTY == directory_empty(strpath.c_str())){
                // found "no dir object".
                strpath  += "/";                
                result    = 0;
                forcedir = true;
            }
        }
    }else{
        if(is_need_check_obj_detail(tmpHead)){
            // check a case of that "object" does not have attribute and "object" is possible to be directory.
            if(-ENOTEMPTY == directory_empty(strpath.c_str())){
                // found "no dir object".
                strpath  += "/";
                result    = 0;
                forcedir = true;
            }
        }
    }

    // cache size is Zero -> only convert.
    if(!convert_header_to_stat(strpath.c_str(), tmpHead, pstbuf, forcedir)){
      S3FS_PRN_ERR("failed convert headers to stat[path=%s]", strpath.c_str());
      return -ENOENT;
    }
    
    return result;
}

//s3fs_flush
int s3fsRemoteSyncFile(const char* path, mode_t mode) {
    int rc = 0;
    string cache_path;
    headers_t meta;
    FdManager::MakeCachePath(path, cache_path, false, false);

    struct stat stAttr = {0};
    if(0 != (rc = stat(cache_path.c_str(), &stAttr))){
        S3FS_PRN_WARN("Not find local file(%s), rc:%d.", cache_path.c_str(), rc);
        return 0;
    }

    if (S_ISDIR(stAttr.st_mode)) {
        S3FS_PRN_WARN("Try to sync file(%s), but local is dir", cache_path.c_str());
        return 0;
    }

    int fd = open(cache_path.c_str(), O_RDWR);
    if (fd < 0) {
        S3FS_PRN_WARN("failed to open file(%s)", cache_path.c_str());
        return -EIO;
    }

    meta["x-amz-meta-mtime"] = str(stAttr.st_mtime);
    meta["x-amz-meta-uid"]   = str(stAttr.st_uid);
    meta["x-amz-meta-gid"]   = str(stAttr.st_gid);
    
    if(stAttr.st_size >= static_cast<size_t>(2 * S3fsCurl::GetMultipartSize()) && !nomultipart){ // default 20MB
        // Additional time is needed for large files
        time_t backup = 0;
        if(120 > S3fsCurl::GetReadwriteTimeout()){
            backup = S3fsCurl::SetReadwriteTimeout(120);
        }
        rc = S3fsCurl::ParallelMultipartUploadRequest(path, meta, fd);
        if(0 != backup){
            S3fsCurl::SetReadwriteTimeout(backup);
        }
    } else {
        S3fsCurl s3fscurl(true);
        rc = s3fscurl.PutRequest(path, meta, fd);
    }

    close(fd);

    return rc;
}



