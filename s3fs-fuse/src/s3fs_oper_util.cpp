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

#include "s3fs_util.h"
#include "autofilelock.h"
#include "s3fs_var.h"
#include "s3fs_oper.h"

using namespace std;


S3fsOper::S3fsOper(const char *name, const char *dstFile, const char *srcFile):m_pName(name), 
                                                                                  m_stat(name), 
                                                                                  m_dstEnt(dstFile), 
                                                                                  m_srcEnt(srcFile)
{
    S3FS_PRN_INFO(" >>>>> %s >>>>> para:%s,%s", m_pName, m_dstEnt.path(), m_srcEnt.path());
    
    m_nResult = -EPERM;
    m_dstEnt.init();
    m_srcEnt.init();
}

S3fsOper::~S3fsOper()
{
    if (m_nResult) {
        S3FS_PRN_INFO(" _____ %s result:%d", m_pName, m_nResult);
    } else {
        S3FS_PRN_INFO(" %s :result:%d", m_pName, m_nResult);
    }
}

int S3fsOper::check_parent_object_access(VfsEnt &ent, int mask)
{
    const char* path = ent.path();
    string parent;
    int result;

    S3FS_PRN_DBG("[path=%s]", path);

    if(0 == strcmp(path, "/") || 0 == strcmp(path, ".")){
        // path is mount point.
        return 0;
    }
    
    if(X_OK == (mask & X_OK)){
        for(parent = mydirname(path); 0 < parent.size(); parent = mydirname(parent)){
            if(parent == "."){
                parent = "/";
            }

            //这个地方可以使用cache优化
            VfsEnt obj(parent);
            result = obj.init();
            if (result) {
                return result;
            }
            if(0 != (result = check_object_access(obj, X_OK))){
                return result;
            }
            if(parent == "/" || parent == "."){
                break;
            }
        }
    }
    
    mask = (mask & ~X_OK);
    if(0 != mask){
        parent = mydirname(path);
        if(parent == "."){
            parent = "/";
        }

        //这个地方可以使用cache优化
        VfsEnt obj(parent);
        result = obj.init();
        if (result) {
            return result;
        }
        if(0 != (result = check_object_access(obj, mask))){
            return result;
        }
    }
    return 0;
}


int S3fsOper::check_object_access(VfsEnt &ent, int mask)
{
    struct stat & st = ent.getStat();
    struct fuse_context* pcxt;

    if(NULL == (pcxt = fuse_get_context())){
        return -EIO;
    }

    if (0 != ent.getErrno()) {
        return -ent.getErrno();
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
    struct fuse_context* pcxt;


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
    
    if(pcxt->uid == ent.getStat().st_uid){
        return 0;
    }
    
    return -EPERM;
}







