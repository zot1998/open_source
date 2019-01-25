
#ifndef __S3FS_OPER_H
#define __S3FS_OPER_H

#include <sys/stat.h>
#include "autofilelock.h"
#include "s3fs_ent.h"

class S3fsOper
{
    public:
        S3fsOper(const char *dstFile, const char *srcFile = NULL); 
        ~S3fsOper();
        
    public: //io operation api        
        int getattr(struct stat* stbuf);
        int readlink(char* buf, size_t size);
        int mknod(mode_t mode, dev_t rdev);
        int mkdir(mode_t mode);
        int unlink(void);
        int rmdir(void);
        int symlink(void);
    private:
        int checkaccess(VfsEnt &ent, int mask);
        
    private:
        VfsEnt m_dstEnt;
        VfsEnt m_srcEnt;
};









int s3fs_rename(const char* from, const char* to);
int s3fs_link(const char* from, const char* to);
int s3fs_chmod(const char* path, mode_t mode);
int s3fs_chmod_nocopy(const char* path, mode_t mode);
int s3fs_chown(const char* path, uid_t uid, gid_t gid);
int s3fs_chown_nocopy(const char* path, uid_t uid, gid_t gid);
int s3fs_utimens(const char* path, const struct timespec ts[2]);
int s3fs_utimens_nocopy(const char* path, const struct timespec ts[2]);
int s3fs_truncate(const char* path, off_t size);
int s3fs_create(const char* path, mode_t mode, struct fuse_file_info* fi);
int s3fs_open(const char* path, struct fuse_file_info* fi);
int s3fs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
int s3fs_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
int s3fs_statfs(const char* path, struct statvfs* stbuf);
int s3fs_flush(const char* path, struct fuse_file_info* fi);
int s3fs_fsync(const char* path, int datasync, struct fuse_file_info* fi);
int s3fs_release(const char* path, struct fuse_file_info* fi);
int s3fs_opendir(const char* path, struct fuse_file_info* fi);
int s3fs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi);
int s3fs_access(const char* path, int mask);
int s3fs_setxattr(const char* path, const char* name, const char* value, size_t size, int flags);
int s3fs_getxattr(const char* path, const char* name, char* value, size_t size);

int s3fs_listxattr(const char* path, char* list, size_t size);
int s3fs_removexattr(const char* path, const char* name);






#endif // __S3FS_OPER_H


