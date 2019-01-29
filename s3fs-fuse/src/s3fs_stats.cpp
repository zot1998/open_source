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

#include "s3fs_stats.h"

S3Stat S3Stat::m_instance;
pthread_t  S3Stat::m_ThreadId;
bool    S3Stat::m_bRunFlag = true;

static uint64 getmsec()
{
  struct timeval now;
  if(0 != gettimeofday(&now, NULL)){
    return 0;
  }
  return ((uint64)now.tv_sec) * 1000 + (now.tv_usec/1000);
}


S3OperStat::S3OperStat(const char *name):m_strName(name)
{
    memset(&m_stats, 0, sizeof(m_stats));
    pthread_mutex_init(&m_lock, NULL);
}

S3OperStat::~S3OperStat(void)
{
    pthread_mutex_destroy(&m_lock);
}

oper_id S3OperStat::enter(void){
    oper_id id;
    oper_info_t info = {0};
    
    pthread_mutex_lock(&m_lock);
    m_stats.u64HistoryInTotalCnt++;
    id = m_stats.u64HistoryInTotalCnt;
    info.u64BeginTime = getmsec();
    m_mapId[id] = info;
    pthread_mutex_unlock(&m_lock);
    
    return id;
}

void S3OperStat::exit(oper_id & id){
    pthread_mutex_lock(&m_lock);
    m_stats.u64HistoryOutTotalCnt++;
    
    OPER_ID_MAP::iterator it = m_mapId.find(id);
    if (it != m_mapId.end()) {
        (*it).second.u32EndFlag = 1;
        (*it).second.u32DelayTime = getmsec() - (*it).second.u64BeginTime;

        m_stats.u64HistoryTotalDelay = m_stats.u64HistoryTotalDelay + (*it).second.u32DelayTime;
        UPDATE_MAX(m_stats.u32HistoryMaxDelay, (*it).second.u32DelayTime);
        UPDATE_MIN(m_stats.u32HistoryMinDelay, (*it).second.u32DelayTime);
        
        if (m_mapId.size() >= OPER_LAST_COUNT) {
            OPER_ID_MAP::iterator itBegin = m_mapId.begin();
            while (itBegin != m_mapId.end()){
                if ((*itBegin).second.u32EndFlag == 0){
                    break;
                }
                if (m_mapId.size() <= OPER_LAST_COUNT) {
                    break;
                }
                //itBegin = m_mapOper.erase(itBegin);
                m_mapId.erase(itBegin);
                itBegin = m_mapId.begin();
            }
        }
    }
    pthread_mutex_unlock(&m_lock);
}

oper_statinfo_t S3OperStat::get(void) {
    oper_statinfo_t result;
    
    pthread_mutex_lock(&m_lock);
    m_stats.u32RecentPendCnt = 0;
    m_stats.u32RecentMinDelay = 0;
    m_stats.u32RecentMaxDelay = 0;
    m_stats.u32RecentAvgDelay = 0;
    m_stats.u32RecentTotalDelay = 0;
    m_stats.u32CurrentTotalPendDelay = 0;
    m_stats.u32CurrentMaxPendDelay = 0;
    m_stats.u32RecentCnt = m_mapId.size();

    OPER_ID_MAP::iterator it = m_mapId.begin();
    while (it != m_mapId.end())
    {
        oper_info_t *p = &((*it).second);
        
        if (p->u32EndFlag == 0) {
            p->u32DelayTime = getmsec() - p->u64BeginTime;
            
            m_stats.u32RecentPendCnt++;
            if (m_stats.u32CurrentMaxPendDelay == 0)
            {
                m_stats.u32CurrentMaxPendDelay = p->u32DelayTime;
            }

            m_stats.u32CurrentTotalPendDelay += p->u32DelayTime;
        } 
        else {
            UPDATE_MIN(m_stats.u32RecentMinDelay,  p->u32DelayTime);
        }
        
        UPDATE_MAX(m_stats.u32RecentMaxDelay,  p->u32DelayTime);
        UPDATE_MAX(m_stats.u32HistoryMaxDelay, p->u32DelayTime);
        
        m_stats.u32RecentTotalDelay += p->u32DelayTime;
        ++it;
    }
    
    if (m_stats.u32RecentCnt - m_stats.u32RecentPendCnt) {
        m_stats.u32RecentAvgDelay = (m_stats.u32RecentTotalDelay - m_stats.u32CurrentTotalPendDelay)/(m_stats.u32RecentCnt - m_stats.u32RecentPendCnt);
    }
    
    if (m_stats.u64HistoryInTotalCnt - m_stats.u32RecentPendCnt) {
        m_stats.u32HistoryAvgDelay = m_stats.u64HistoryTotalDelay/(m_stats.u64HistoryInTotalCnt - m_stats.u32RecentPendCnt);
    }
    
    result = m_stats;
    pthread_mutex_unlock(&m_lock);

    return result;
}

S3Stat::S3Stat(void){
    int rc = 0;	
    S3Stat::m_ThreadId = (pthread_t)-1;

    rc = pthread_mutex_init(&m_lock, NULL);
    if (rc) {
        S3FS_PRN_WARN("Monitor:pthread_rwlock_init failed.\n");
    }
}
S3Stat::~S3Stat() {
    int rc = 0;
    rc = pthread_lock_destroy(&m_lock);
    if (rc) {
        S3FS_PRN_WARN("Monitor:pthread_rwlock_destroy failed.\n");
    }
}

void S3Stat::start(void) {
    int ret = 0;
    pthread_attr_t attr_thr;
    pthread_attr_init(&attr_thr);
    m_bRunFlag = true;
    ret = pthread_create(&m_ThreadId, &attr_thr, S3Stat::run, NULL);
    if (ret) {
        m_ThreadId = (pthread_t)-1;
        S3FS_PRN_WARN("----create s3stat thread failed.");
    }

}
void S3Stat::stop(void) {
    m_bRunFlag = false;
    
    if ((pthread_t)-1 != m_ThreadId) {
        pthread_join(m_ThreadId, NULL);
    }
}
static void *S3Stat::run(void *Arg){
    uint64 u64Tick = 0;
    while (m_bRunFlag) {
        u64Tick++;
        if (u64Tick % 5 == 0) {
            m_instance.flush();
        }
        
        usleep(100000);
    }

    return NULL;
}

        
S3OperStat * S3Stat::getOper(const char *name)
{
    pthread_mutex_lock(&m_lock);
    S3OperStat * &op = m_mapOper[name];
    if (NULL == op) {
        op = new S3OperStat(name);
    }
    pthread_mutex_unlock(&m_lock);
    return op;
}

std::string S3Stat::get(void) {
    if (m_strBucketName.size() == 0) {
        return "";
    }
    
    oper *pOper = NULL;
    OPER_MAP::iterator it;
    std::map<const char *, oper_statinfo_t> mapInfo;
    uint64 u64FuseTotalDelay = 0;
    uint64 u64HttpTotalDelay = 0;
    oper_statinfo_t info = {0};

    pthread_lock_rdlock(&m_lock);
    for(it = m_mapOper.begin(); it != m_mapOper.end(); ++it)
    {
        pOper = (*it).second;
        info = pOper->get();
        mapInfo[(*it).first] = info;
        if (pOper->ishttp()) {
            u64HttpTotalDelay += info.u64HistoryTotalDelay + info.u32CurrentTotalPendDelay;
        }
        else {
            u64FuseTotalDelay += info.u64HistoryTotalDelay + info.u32CurrentTotalPendDelay;
        }
    }
    pthread_lock_unlock(&m_lock);

    rapidjson::StringBuffer s;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);
    
    writer.StartObject();
    
    char nowstr[32] = {0};
    sprintf(nowstr, "%llu", getmsec()/1000);
    writer.Key(nowstr);
    
    writer.StartObject();

    writer.Key("Total Fuse Delay");
    writer.Uint(u64FuseTotalDelay);

    writer.Key("Total Http Delay");
    writer.Uint(u64HttpTotalDelay);
    
    oper_statinfo_t *pstatsinfo = NULL;
    std::map<const char *, oper_statinfo_t>::iterator itTemp = mapInfo.begin();
    while (itTemp != mapInfo.end())
    {
        pstatsinfo = &(*itTemp).second;

        writer.Key((*itTemp).first);
        writer.StartObject();
        
        writer.Key("History Total In Count");
        writer.Uint64(pstatsinfo->u64HistoryInTotalCnt);

        writer.Key("History Total Out Count");
        writer.Uint64(pstatsinfo->u64HistoryOutTotalCnt);

        writer.Key("History Min Delay");
        writer.Uint(pstatsinfo->u32HistoryMinDelay);

        writer.Key("History Max Delay");
        writer.Uint(pstatsinfo->u32HistoryMaxDelay);

        writer.Key("History Avg Delay");
        writer.Uint(pstatsinfo->u32HistoryAvgDelay);

        writer.Key("Recent Min Delay");
        writer.Uint(pstatsinfo->u32RecentMinDelay);

        writer.Key("Recent Max Delay");
        writer.Uint(pstatsinfo->u32RecentMaxDelay);

        writer.Key("Recent Avg Delay");
        writer.Uint(pstatsinfo->u32RecentAvgDelay);

        writer.Key("Current Max Pend Delay");
        writer.Uint(pstatsinfo->u32CurrentMaxPendDelay);

        writer.Key("Current Pend Count");
        writer.Uint(pstatsinfo->u32RecentPendCnt);

        writer.Key("Recent Count");
        writer.Uint(pstatsinfo->u32RecentCnt);

        writer.EndObject();
        
        ++itTemp;
    }
    writer.EndObject();
    writer.EndObject();
    
    return s.GetString();
}
        
void S3Stat::flush(void)
{
    std::string filepath("/etc/sgw/status/");
    filepath += "s3fs.";
    filepath += m_strBucketName;
    filepath += ".status";

    std::ofstream of(filepath.c_str(),std::ios::in | std::ios::out | std::ios::trunc); //ios::app
    if (!of) {
        return;
    }
    
    of<<get().c_str();
    of.close();
}










#endif


