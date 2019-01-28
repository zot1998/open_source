#ifndef __MONITOR_H
#define __MONITOR_H
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#include <string>
#include <list>
#include <map>
#include <iostream> 
#include <fstream> 

#include "rapidjson/prettywriter.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/memorystream.h"

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


static uint64 getmsec()
{
  struct timeval now;
  if(0 != gettimeofday(&now, NULL)){
    return 0;
  }
  return ((uint64)now.tv_sec) * 1000 + (now.tv_usec/1000);
}

//���1000��io��ʱ��ͳ��
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


typedef std::map<const char *, oper *> OPER_MAP;

class S3Stat{
    public:
        static S3Stat & instance() {
            return m_instance;
        }
        
        void bucket(std::string & bucket) { m_strBucketName = bucket;}
        S3OperStat &getoper(const char *name);
        std::string get(void);
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
        AutoS3Stat(const char *name) { m_pOper = S3Stat::instance().getoper(name); m_id = m_pOper->enter();}
        ~AutoS3Stat() { if (NULL != m_pOper) { m_pOper.exit(m_id)}}
        
    private:
        S3OperStat *m_pOper;
        oper_id     m_id;

};

#define S3FS_STATS() AutoS3Stat  stats(__FUNCTION__)
#define HTTP_STATS() AutoS3Stat  stats("http_request")








#endif


