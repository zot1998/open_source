
#ifndef S3FS_IO_H_
#define S3FS_IO_H_

static int s3fs_cache_getattr(const char* path, struct stat* stbuf);
static int s3fs_cache_readlink(const char* path, char* buf, size_t size);
static int s3fs_cache_mknod(const char *path, mode_t mode, dev_t rdev);
static int s3fs_cache_mkdir(const char* path, mode_t mode);
static int s3fs_cache_unlink(const char* path);
static int s3fs_cache_rmdir(const char* path);
static int s3fs_cache_symlink(const char* from, const char* to);
static int s3fs_cache_rename(const char* from, const char* to);
static int s3fs_cache_link(const char* from, const char* to);
static int s3fs_cache_chmod(const char* path, mode_t mode);
static int s3fs_cache_chown(const char* path, uid_t uid, gid_t gid);
static int s3fs_cache_utimens(const char* path, const struct timespec ts[2]);
static int s3fs_cache_chmod_nocopy(const char* path, mode_t mode);
static int s3fs_cache_chown_nocopy(const char* path, uid_t uid, gid_t gid);
static int s3fs_cache_utimens(const char* path, const struct timespec ts[2]);
static int s3fs_cache_truncate(const char* path, off_t size);
static int s3fs_cache_open(const char* path, struct fuse_file_info* fi);
static int s3fs_cache_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
static int s3fs_cache_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
static int s3fs_cache_statfs(const char* path, struct statvfs* stbuf);
static int s3fs_cache_flush(const char* path, struct fuse_file_info* fi);
static int s3fs_cache_fsync(const char* path, int datasync, struct fuse_file_info* fi);
static int s3fs_cache_release(const char* path, struct fuse_file_info* fi);
static int s3fs_cache_opendir(const char* path, struct fuse_file_info* fi);
static int s3fs_cache_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi);
static void* s3fs_cache_init(struct fuse_conn_info* conn);
static void s3fs_cache_destroy(void*);
static int s3fs_cache_access(const char* path, int mask);
static int s3fs_cache_create(const char* path, mode_t mode, struct fuse_file_info* fi);
static int s3fs_cache_setxattr(const char* path, const char* name, const char* value, size_t size, int flags);
static int s3fs_cache_getxattr(const char* path, const char* name, char* value, size_t size);
static int s3fs_cache_listxattr(const char* path, char* list, size_t size);
static int s3fs_cache_removexattr(const char* path, const char* name);



#define S3FS_OPERATOER(cb, op) cb = s3fs_cache_##op



#endif // S3FS_IO_H_


