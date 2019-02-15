
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/tree.h>
#include <curl/curl.h>
#include <pwd.h>
#include <grp.h>
#include <getopt.h>
#include <signal.h>

#include <fstream>
#include <vector>
#include <algorithm>
#include <map>
#include <string>
#include <list>


#include "common.h"
#include "curl.h"
#include "string_util.h"

#include "s3fs_rsync_util.h"

using namespace std;

S3RSyncUtil::S3RSyncUtil(S3DB_INFO_S &record):m_record(record),m_vfsEnt(record.strFile),m_s3Ent(record.strFile)
{
    
}

S3RSyncUtil::~S3RSyncUtil()
{
}

int S3RSyncUtil::init(void)
{
    int rc = 0;

    rc = m_vfsEnt.init();
    if (rc) {
        return rc;
    }

    rc = m_s3Ent.init();
    if (rc) {
        S3FS_PRN_ERR("m_s3Ent.init failed(%d)", rc);
        return rc;
    }

    return 0;
}

int S3RSyncUtil::sync(void)
{
    if (S3DB_OP_ADD == m_record.nOperator) {
        return put();
    } else if (S3DB_OP_DEL == m_record.nOperator) {
        return remove();
    } else {
        return -1;
    }
}

int S3RSyncUtil::put(void)
{
    if (S_ISDIR(m_record.nMode)){
        return putdir();
    } else {
        return putfile();
    }
}

int S3RSyncUtil::remove(void)
{
    if (S_ISDIR(m_record.nMode)){
        return removedir();
    } else {
        return removefile();
    }
}


int  S3RSyncUtil::putdir(void)
{
    int rc = 0;
    
    if (!m_vfsEnt.isExists()) {
        S3FS_PRN_WARN("[RSYNC]put dir(%s), but local is not exist", m_record.strFile.c_str());
        return 0;
    }
    
    if (!m_vfsEnt.isDir()) {
        S3FS_PRN_WARN("[RSYNC]put dir(%s), but local is not directory", m_record.strFile.c_str());
        return 0;
    }

    if (m_s3Ent.isExists()) {
        S3FS_PRN_WARN("[RSYNC]put dir(%s), but remote is exists, isdir(%d)", m_record.strFile.c_str(), m_s3Ent.isDir());
        return 0;
    }

    headers_t meta;
    struct stat & st = m_vfsEnt.getStat();
    meta["Content-Type"]     = string("application/x-directory");
    meta["x-amz-meta-uid"]   = str(st.st_uid);
    meta["x-amz-meta-gid"]   = str(st.st_gid);
    meta["x-amz-meta-mode"]  = str(st.st_mode);
    meta["x-amz-meta-mtime"] = str(st.st_ctime);

    S3fsCurl s3fscurl;
    rc = s3fscurl.PutRequest(m_vfsEnt.pathDir(), meta, -1);
    S3FS_PRN_INFO("[RSYNC]put dir(%s), result(%d)", m_record.strFile.c_str(), rc);
    return rc;
}

int  S3RSyncUtil::putfile(void)
{
    S3FS_PRN_ERR("put file(%s)", m_record.strFile.c_str());
    return 0;
}

int  S3RSyncUtil::removedir(void)
{
    int rc = 0;
    
    if (m_vfsEnt.isExists() && m_vfsEnt.isDir()) {
        S3FS_PRN_WARN("[RSYNC]remove dir(%s), but local is exist", m_record.strFile.c_str());
        return 0;
    }
    
    if (!m_s3Ent.isExists()) {
        S3FS_PRN_WARN("[RSYNC]remove dir(%s), but remote is not exist", m_record.strFile.c_str());
        return 0;
    }

    //如果s3 dir为非空 , rc 仍然会返回0
    S3fsCurl s3fscurl;
    rc = s3fscurl.DeleteRequest(m_s3Ent.pathDir());
    s3fscurl.DestroyCurlHandle();
    
    S3FS_PRN_INFO("[RSYNC]remove dir(%s), result(%d)", m_record.strFile.c_str(), rc);
    return rc;
}


int  S3RSyncUtil::removefile(void)
{
    int rc = 0;
    
    if (m_vfsEnt.isExists() && !m_vfsEnt.isDir()) {
        S3FS_PRN_WARN("[RSYNC]remove file(%s), but local is exist", m_record.strFile.c_str());
        return 0;
    }
    
    if (!m_s3Ent.isExists()) {
        S3FS_PRN_WARN("[RSYNC]remove file(%s), but remote is not exist", m_record.strFile.c_str());
        return 0;
    }

    S3fsCurl s3fscurl;
    rc = s3fscurl.DeleteRequest(m_s3Ent.path());
    s3fscurl.DestroyCurlHandle();
    
    S3FS_PRN_INFO("[RSYNC]remove file(%s), result(%d)", m_record.strFile.c_str(), rc);
    return rc;
}







