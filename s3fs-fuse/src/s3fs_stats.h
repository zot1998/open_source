#ifndef __S3FS_STATS_H
#define __S3FS_STATS_H
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#include <string>
#include <list>
#include <map>

#define UPDATE_MAX(o, v) if ((o) < (v)) { o = (v);}
#define UPDATE_MIN(o, v) if (((0 == (o)) || ((o) > (v))) && (0 != (v))) { o = (v);}


typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef uint64 oper_id;
typedef struct oper_info_s{
    uint64 u64BeginTime;
    uint32 u32DelayTime;
    uint32 u32EndFlag;
}oper_info_t;
typedef std::map<oper_id, oper_info_t> OPER_ID_MAP;


typedef struct oper_statinfo_s{
    uint64 u64HistoryInTotalCnt;
    uint64 u64HistoryOutTotalCnt;
    uint32 u64HistoryTotalDelay;
    uint32 u32HistoryMinDelay;
    uint32 u32HistoryMaxDelay;
    uint32 u32HistoryAvgDelay;

    uint32 u32CurrentTotalPendDelay;
    uint32 u32CurrentMaxPendDelay;

    uint32 u32RecentMinDelay;// ms
    uint32 u32RecentMaxDelay; // ms
    uint32 u32RecentAvgDelay; //ms
    uint32 u32RecentTotalDelay;
    uint32 u32RecentCnt;
    uint32 u32RecentPendCnt;
}oper_statinfo_t;


//最近1000个io的时延统计
#define OPER_LAST_COUNT 1000
class S3OperStat
{
    public:
        S3OperStat(const char *name);
        ~S3OperStat(void);

        bool ishttp(void) { return memcmp(m_strName.c_str(), "http", strlen("http")) == 0;}

        oper_id enter(void);
        void exit(oper_id & id);

        oper_statinfo_t get(void);
        
	private:
        std::string     m_strName;
	    oper_statinfo_t m_stats;
        OPER_ID_MAP     m_mapId;

        pthread_mutex_t m_lock;
};


typedef std::map<const char *, S3OperStat *> OPER_MAP;

class S3Stat{
    public:
        static S3Stat & instance() {
            return m_instance;
        }
        
        void setBucket(std::string & bucket) { m_strBucketName = bucket;}
        S3OperStat * getOper(const char *name);
        std::string serialize(void);
        void flush(void);
        

        void start(void);
        void stop(void);

    private:
        S3Stat(void);
        ~S3Stat();
        static void *run(void *Arg);
        
    private:
        pthread_mutex_t   m_lock;
            
        std::string       m_strBucketName;
        OPER_MAP          m_mapOper;
        static S3Stat     m_instance;
        static pthread_t  m_ThreadId;
        static bool       m_bRunFlag;
};




class AutoS3Stat
{
    public:
        AutoS3Stat(const char *name):m_pName(name) { 
            m_pOper = S3Stat::instance().getOper(name); 
            m_id = m_pOper->enter();
        }
        ~AutoS3Stat() { 
            if (NULL != m_pOper) { 
                m_pOper->exit(m_id);
            }
        }
        
    private:
        S3OperStat *m_pOper;
        oper_id     m_id;
        const char *m_pName;       

};

#define OPNAME   __FUNCTION__
#define HTTP_STATS() AutoS3Stat  stats("http_request")








#endif


