
#ifndef S3FS_IO_H_
#define S3FS_IO_H_

#include <sys/stat.h>


class Ent
{
    public:
        Ent(const char *path);
        Ent(const std::string &path);
        virtual ~Ent();

        virtual int  init(void);
        virtual bool isDir(void)     { return S_ISDIR(m_stAttr.st_mode);}
        virtual bool isExists(void) { return m_bExists;}
        struct stat &stat(void)      { return m_stAttr;}

    protected:
        bool        m_bExists;
        std::string m_strPath;    //后缀不带'/'
        std::string m_strPathDir; //后缀带'/'
        struct stat m_stAttr;
};

class VfsEnt:public Ent
{
    public:
        VfsEnt(const char *path);
        VfsEnt(const std::string &path);
        ~VfsEnt();

        int init(void);

        int create(void);
        

    private:
        std::string m_strCachePath;

};

class S3Ent:public Ent
{
    public:
        S3Ent(const char *path);
        S3Ent(const std::string &path);
        ~S3Ent();

        int  init(void);

    private:
        std::string m_strMatchPath;
        bool        m_bEmptyDir;

};












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


