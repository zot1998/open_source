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
int s3fs_generate_cachefile(const char* path, struct stat* stbuf){
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

int s3fs_remove_cachedir(const char* path) {
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




