
#ifndef S3FS_IO_H_
#define S3FS_IO_H_

int s3fsLocalCreate(const char* path, struct stat* stbuf);

int s3fsLocalRmdir(const char* path);

int s3fsRemoteRmDir(const char* path);
int s3fsRemoteRmFile(const char* path);
int s3fsRemoteRm(const char* path, mode_t mode);

int s3fsRemoteSyncFile(const char* path, mode_t mode);
int s3fsRemoteSyncDir(const char* path, mode_t mode);






#endif // S3FS_IO_H_


