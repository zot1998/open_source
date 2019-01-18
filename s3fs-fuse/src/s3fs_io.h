
#ifndef S3FS_IO_H_
#define S3FS_IO_H_

#include <sys/stat.h>

typedef enum _REMOTE_STATE_E{
    REMOTE_INIT = 0,
    REMOTE_SUCCESS,
    REMOTE_EXCEPTION,
}REMOTE_STATE_E;

class S3Ent
{
    public:
        S3Ent(const char *path, );
        S3Ent(const std::string &path);
        ~S3Ent();

        bool isLocalExist() { return m_bLocalExists;}
        bool isRemoteExist() { return m_bLocalExists;}

        void initRemote(void);

        int  syncAddToRemote(void);
        int  syncDelToRemote(void);

    private:
        int init(void);

    private:
        std::string m_strPath;    //后缀不带'/'
        std::string m_strPathDir; //后缀带'/'
        std::string m_strMatchPath;
        std::string m_strCachePath;
        struct stat m_stLocalAttr;
        struct stat m_stRemoteAttr;
        bool        m_bLocalExists;
        bool        m_bRemoteExists;
        bool        m_bRemoteEmpty;

        REMOTE_STATE_E  m_eRemoteStatus; //是否获取远端， 为true时其他远端状态才有意义
        
};


int s3fsGetRemoteAttr(const char* path, struct stat* pstbuf)


int s3fsLocalMkFile(const char* path, struct stat* pstAttr);
int s3fsLocalMkDir(const char* path, struct stat* pstAttr);
int s3fsLocalMk(const char* path, struct stat* pstAttr);




int s3fsLocalRmdir(const char* path);

int s3fsRemoteRmDir(const char* path);
int s3fsRemoteRmFile(const char* path);
int s3fsRemoteRm(const char* path, mode_t mode);

int s3fsRemoteMkFile(const char* path);
int s3fsRemoteMkDir(const char* path);
int s3fsRemoteMk(const char* path, mode_t mode);






#endif // S3FS_IO_H_


