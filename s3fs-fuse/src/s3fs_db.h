
#ifndef S3FS_DB_H_
#define S3FS_DB_H_


/*
format:
op:   file    status   
type: string  status
*/

class S3DB
{
    public:
        static S3DB & Instance(void) {
            return m_instance;
        }

    private:
        S3DB();
        ~S3DB();
    private:
        static S3DB m_instance;
        
};
#endif // S3FS_DB_H_


