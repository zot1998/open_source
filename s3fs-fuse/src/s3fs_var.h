
#ifndef __S3FS_VAR_H
#define __S3FS_VAR_H
#include <sys/stat.h>


extern uid_t  mp_uid;
extern gid_t  mp_gid;
extern mode_t mp_mode;
extern uid_t s3fs_uid;
extern gid_t s3fs_gid;
extern mode_t s3fs_umask;

extern bool is_s3fs_uid;
extern bool is_s3fs_gid;




#endif // __S3FS_VAR_H


