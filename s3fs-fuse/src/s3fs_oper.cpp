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

//s3fs_getattr
int s3fs_cache_getattr(const char* path, struct stat* stbuf)
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

//s3fs_readlink
int s3fs_cache_readlink(const char* path, char* buf, size_t size)
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







