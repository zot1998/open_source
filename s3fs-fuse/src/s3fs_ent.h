
#ifndef __S3FS_ENT_H
#define __S3FS_ENT_H

#include <sys/stat.h>


class Ent
{
    public:
        Ent(const char *path);
        Ent(const std::string &path);
        virtual ~Ent();

        virtual int   init(void);        { return 0;}
        virtual bool  isDir(void)       { return S_ISDIR(m_stAttr.st_mode);}
        virtual bool  fileType(void)  { return m_stAttr.st_mode & S_IFMT;}
        virtual bool  isExists(void)  { return m_bExists;}
        struct  stat &stat(void)        { return m_stAttr;}
        virtual int   build(Ent &ent)   { return 0;}
        virtual int   build(void)       { return 0;}
        virtual int   remove(void)     { return 0;}

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
        

    private:
        std::string m_strCachePath;

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

    private:
        std::string m_strMatchPath;
        bool        m_bEmptyDir;

};

#endif // __S3FS_ENT_H


