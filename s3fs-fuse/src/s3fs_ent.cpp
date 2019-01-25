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
    m_strPath.clear();
    m_strPathDir.clear();    
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
    m_strCachePath.clear();
}

int VfsEnt::init(void)
{    
    int rc = 0;
    if (0 == m_strPath.size()) {
        return 0;
    }
    
    rc = stat(m_strCachePath.c_str(), &m_stAttr);
    m_errno = errno;
    
    if (0 == rc) {
        m_bExists = true;
    } else if (ENOENT != errno) {
        S3FS_PRN_ERR("%s local stat failed(rc:%d,error:%d)", rc, -errno); 
        return -errno;
    }
    
    return 0;
}

int VfsEnt::build(void) 
{
    if (isDir()) {
        int result = 0;
        result = mkdir(m_strCachePath.c_str(), m_stAttr.st_mode);
        m_errno = errno;
        if (0 != result) {
            S3FS_PRN_ERR("Make local directory(%s, mode:0x%x) errno %d ", m_strPath.c_str(), m_strPath.st_mode, -errno);
            return result;
        }

        //set stat
        // not opened file yet.
        struct utimbuf n_mtime;
        n_mtime.modtime = time;
        n_mtime.actime  = time;
        if(-1 == utime(m_strCachePath.c_str(), &n_mtime)){
            S3FS_PRN_ERR("set file (%s) utime failed. errno(%d)", m_strPath.c_str(), -errno);
            return -errno;
        }

    } else {
        FdEntity *ent = NULL;
        if(NULL == (ent = FdManager::get()->Open(m_strPath.c_str(), 
                                                  NULL, 
                                                  static_cast<ssize_t>(stbuf->st_size), 
                                                  m_stAttr.st_mtime, 
                                                  false, 
                                                  true))){
            S3FS_PRN_ERR("Could not open file(%s). errno(%d)", m_strPath.c_str(), -errno);
            
            return -EIO;
        }
        
        ent->Close();
    }

    return 0;
}

int VfsEnt::remove(void)
{
    if (!isExists()) {
        return -ENOENT;
    }

    int rc = 0;
    if (isDir()) {
        //s3fs_rmdir
        rc = rmdir(m_strCachePath.c_str());
        if (0 != result) {
            S3FS_PRN_ERR("remove local directory(%s) error: %d", m_strPath.c_str(), -errno);
            return result;
        }
    } else {
        //s3fs_unlink
        rc = unlink(m_strCachePath.c_str());
        if (rc) {
            S3FS_PRN_ERR("remove local file(%s): errno=%d", m_strPath.c_str(), -errno);
            return -errno;
        }
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
    m_strMatchPath.clear();
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

int S3Ent::build(Ent &ent)
{
    if (!ent.isExists()) {
        return -ENOENT;
    }
    if (isExists()) {
        if (fileType() != ent.fileType()) {
            //TODO...How??
            S3FS_PRN_WARN("%s local fileType(0x%x) is not match with remote fileType(0x%x)", 
            m_strPath.c_str(), ent.fileType(), fileType());
        }
    }

    headers_t meta;
    if (ent.isDir()) {
        meta["Content-Type"]     = string("application/x-directory");
    }
    meta["x-amz-meta-uid"]   = str(ent.stat().st_uid);
    meta["x-amz-meta-gid"]   = str(ent.stat().st_gid);
    meta["x-amz-meta-mode"]  = str(ent.stat().st_mode);
    meta["x-amz-meta-mtime"] = str(ent.stat().st_mtime);

    int rc = 0;
    if (ent.isDir()) {
        S3fsCurl s3fscurl;
        rc = s3fscurl.PutRequest(m_strPathDir.c_str(), meta, -1);    // fd=-1 means for creating zero byte object.
    } else {
        int fd = open(m_strPath.c_str(), O_RDWR);
        if (fd < 0) {
            S3FS_PRN_WARN("failed to open file(%s)", m_strPath.c_str());
            rc = -EIO;
        } else {        
            if(ent.stat().st_size >= static_cast<size_t>(2 * S3fsCurl::GetMultipartSize()) && !nomultipart){ // default 20MB
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

int S3Ent::remove(void)
{
    int rc = 0;
    if (isDir()) {
        if (!m_bEmptyDir) {
            return -ENOTEMPTY;
        }

        S3fsCurl s3fscurl;
        rc = s3fscurl.DeleteRequest(m_strPathDir.c_str());
    } else {
        S3fsCurl s3fscurl;
        rc = s3fscurl.DeleteRequest(m_strMatchPath.c_str());
    }
    
    return 0;
}








