
#ifndef __S3FS_OPER_H
#define __S3FS_OPER_H

#include <sys/stat.h>
#include "autofilelock.h"
#include "s3fs_ent.h"

class S3fsOper
{
    public:
        S3fsOper(const char *dstFile, const char *srcFile = NULL); 
        virtual ~S3fsOper();
        
    public: //io operation api        
        int getattr(struct stat* stbuf);
        int readlink(char* buf, size_t size);
        int mknod(mode_t mode, dev_t rdev);
        int mkdir(mode_t mode);
        int unlink(void);
        int rmdir(void);
        int symlink(void);
        int rename(void);
        int link(void);
        int chmod(mode_t mode, bool iscopy = true);
        int chown(uid_t uid, gid_t gid, bool iscopy = true);
        int utimens(const struct timespec ts[2], bool iscopy = true);
        int truncate(off_t size);
        int create(mode_t mode, struct fuse_file_info* fi);
        int open(struct fuse_file_info* fi);
        int read(char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
        int write(const char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
        int statfs(struct statvfs* stbuf);
        int flush(struct fuse_file_info* fi);
        int fsync(int datasync, struct fuse_file_info* fi);
        int release(struct fuse_file_info* fi);
        int opendir(struct fuse_file_info* fi);
        int readdir(void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi);
        int access(int mask);
        int setxattr(const char* name, const char* value, size_t size, int flags);
        int getxattr(const char* name, char* value, size_t size);
        int listxattr(char* list, size_t size);
        int removexattr(const char* name);
    private:
        int checkaccess(VfsEnt &ent, int mask);
        int checkowner(VfsEnt &ent);
        
    private:
        VfsEnt m_dstEnt;
        VfsEnt m_srcEnt;
};


















#endif // __S3FS_OPER_H


