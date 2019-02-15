
#ifndef __S3FS_ENT_H
#define __S3FS_ENT_H

#include <sys/stat.h>


class Ent
{
    public:
        Ent(const char *path);
        Ent(const std::string &path);
        virtual ~Ent();

        virtual int   init(void) { return 0;}
        virtual bool  isDir(void) { return S_ISDIR(m_stAttr.st_mode);}
        virtual bool  fileType(void) { return m_stAttr.st_mode & S_IFMT;}
        virtual bool  isExists(void) { return m_bExists;}
        virtual bool  isRoot(void) { return m_strPath == "/";}
        struct  stat &getStat(void) { return m_stAttr;}
        virtual int   build(Ent &ent) { return 0;}
        virtual int   build(void) { return 0;}
        virtual int   remove(void) { return 0;}
        virtual const char *path(void) { return m_strPath.c_str();}
        virtual const char *pathDir(void) { return m_strPathDir.c_str();}
        virtual size_t size(void)  { return m_stAttr.st_size;} 

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
        //int build(Ent &ent);
        int build(void); //int s3fsLocalMk(const char* path, struct stat* pstAttr);
        int remove(void);
        //int open(int flags, mode_t mode);
        //int close(int fd);
        int getErrno(void) { return m_errno; }
        const char *cachePath(void) { return m_strCachePath.c_str();}

    private:
        std::string m_strCachePath;
        int         m_errno;

};

class S3Ent:public Ent
{
    public:
        S3Ent(const char *path);
        S3Ent(const std::string &path);
        ~S3Ent();

        int init(void);
        int build(Ent &ent);
        int remove(void);
        bool isEmptyDir(void);
        int directory_empty(const char* path){return 0;}

    private:
        std::string m_strMatchPath;
        bool        m_bEmptyDir;
        bool        m_bEmptyDirValid;     

};


class EntCache
{
    public:


    private:
    
};




#endif // __S3FS_ENT_H


