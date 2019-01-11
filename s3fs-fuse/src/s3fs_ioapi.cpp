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


static int s3fs_cache_getattr(const char* path, struct stat* stbuf)
{
    return 0;
}
static int s3fs_cache_readlink(const char* path, char* buf, size_t size)
{
    return 0;
}

static int s3fs_cache_mknod(const char *path, mode_t mode, dev_t rdev)
{
    return 0;
}

static int s3fs_cache_mkdir(const char* path, mode_t mode)   
{
    return 0;
}

static int s3fs_cache_unlink(const char* path)   
{
    return 0;
}

static int s3fs_cache_rmdir(const char* path)    
{
    return 0;
}

static int s3fs_cache_symlink(const char* from, const char* to)    
{
    return 0;
}

static int s3fs_cache_rename(const char* from, const char* to)    
{
    return 0;
}

static int s3fs_cache_link(const char* from, const char* to)    
{
    return 0;
}

static int s3fs_cache_chmod(const char* path, mode_t mode)    
{
    return 0;
}

static int s3fs_cache_chown(const char* path, uid_t uid, gid_t gid)    
{
    return 0;
}

static int s3fs_cache_utimens(const char* path, const struct timespec ts[2])    
{
    return 0;
}

static int s3fs_cache_chmod_nocopy(const char* path, mode_t mode)    
{
    return 0;
}

static int s3fs_cache_chown_nocopy(const char* path, uid_t uid, gid_t gid)    
{
    return 0;
}

static int s3fs_cache_utimens(const char* path, const struct timespec ts[2])    
{
    return 0;
}

static int s3fs_cache_truncate(const char* path, off_t size)    
{
    return 0;
}

static int s3fs_cache_open(const char* path, struct fuse_file_info* fi)    
{
    return 0;
}

static int s3fs_cache_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi)    
{
    return 0;
}

static int s3fs_cache_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi)   
{
    return 0;
}

static int s3fs_cache_statfs(const char* path, struct statvfs* stbuf)    
{
    return 0;
}

static int s3fs_cache_flush(const char* path, struct fuse_file_info* fi)    
{
    return 0;
}

static int s3fs_cache_fsync(const char* path, int datasync, struct fuse_file_info* fi)    
{
    return 0;
}

static int s3fs_cache_release(const char* path, struct fuse_file_info* fi)    
{
    return 0;
}

static int s3fs_cache_opendir(const char* path, struct fuse_file_info* fi)    
{
    return 0;
}

static int s3fs_cache_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi)    
{
    return 0;
}

static void* s3fs_cache_init(struct fuse_conn_info* conn)    
{
    return 0;
}

static void s3fs_cache_destroy(void*)    
{
    return 0;
}

static int s3fs_cache_access(const char* path, int mask)    
{
    return 0;
}

static int s3fs_cache_create(const char* path, mode_t mode, struct fuse_file_info* fi)    
{
    return 0;
}

static int s3fs_cache_setxattr(const char* path, const char* name, const char* value, size_t size, int flags)    
{
    return 0;
}

static int s3fs_cache_getxattr(const char* path, const char* name, char* value, size_t size)    
{
    return 0;
}

static int s3fs_cache_listxattr(const char* path, char* list, size_t size)    
{
    return 0;
}

static int s3fs_cache_removexattr(const char* path, const char* name)    
{
    return 0;
}



