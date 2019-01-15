
#ifndef S3FS_IO_H_
#define S3FS_IO_H_

int s3fs_generate_cachefile(const char* path, struct stat* stbuf);

int s3fs_remove_cachedir(const char* path);



#endif // S3FS_IO_H_


