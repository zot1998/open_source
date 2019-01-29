/*
 * s3fs - FUSE-based file system backed by Amazon S3
 *
 * Copyright(C) 2007 Randy Rizun <rrizun@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

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
#include "s3fs.h"
#include "curl.h"
#include "cache.h"
#include "string_util.h"
#include "s3fs_util.h"
#include "fdcache.h"
#include "s3fs_auth.h"
#include "addhead.h"
#include "s3fs_rsync.h"

#include "s3fs_oper.h"
#include "s3fs_stats.h"
#include "autofilelock.h"

using namespace std;


//-------------------------------------------------------------------
// Define
//-------------------------------------------------------------------
enum dirtype {
  DIRTYPE_UNKNOWN = -1,
  DIRTYPE_NEW = 0,
  DIRTYPE_OLD = 1,
  DIRTYPE_FOLDER = 2,
  DIRTYPE_NOOBJ = 3,
};


#if !defined(ENOATTR)
#define ENOATTR				ENODATA
#endif

//-------------------------------------------------------------------
// Structs
//-------------------------------------------------------------------
typedef struct incomplete_multipart_info{
  string key;
  string id;
  string date;
}UNCOMP_MP_INFO;

typedef std::list<UNCOMP_MP_INFO>          uncomp_mp_list_t;
typedef std::list<std::string>             readline_t;
typedef std::map<std::string, std::string> kvmap_t;
typedef std::map<std::string, kvmap_t>     bucketkvmap_t;

//-------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------
bool foreground                   = false;
bool nomultipart                  = false;
bool pathrequeststyle             = false;
bool complement_stat              = false;
std::string program_name;
std::string service_path          = "/";
std::string host                  = "https://s3.amazonaws.com";
std::string bucket                = "";
std::string endpoint              = "us-east-1";
std::string cipher_suites         = "";
std::string instance_name         = "";
s3fs_log_level debug_level        = S3FS_LOG_CRIT;
const char*    s3fs_log_nest[S3FS_LOG_NEST_MAX] = {"", "  ", "    ", "      "};

//-------------------------------------------------------------------
// Static variables
//-------------------------------------------------------------------
static uid_t mp_uid               = 0;    // owner of mount point(only not specified uid opt)
static gid_t mp_gid               = 0;    // group of mount point(only not specified gid opt)
static mode_t mp_mode             = 0;    // mode of mount point
static mode_t mp_umask            = 0;    // umask for mount point
static bool is_mp_umask           = false;// default does not set.
static std::string mountpoint;
static std::string passwd_file    = "";
static bool utility_mode          = false;
static bool noxmlns               = false;
static bool nocopyapi             = false;
static bool norenameapi           = false;
static bool nonempty              = false;
static bool allow_other           = false;
static bool load_iamrole          = false;
static uid_t s3fs_uid             = 0;
static gid_t s3fs_gid             = 0;
static mode_t s3fs_umask          = 0;
static bool is_s3fs_uid           = false;// default does not set.
static bool is_s3fs_gid           = false;// default does not set.
static bool is_s3fs_umask         = false;// default does not set.
static bool is_remove_cache       = false;
static bool is_ecs                = false;
static bool is_ibm_iam_auth       = false;
static bool is_use_xattr          = false;
static bool create_bucket         = false;
static bool check_bucket          = true;
static int64_t singlepart_copy_limit = FIVE_GB;
static bool is_specified_endpoint = false;
static int s3fs_init_deferred_exit_status = 0;
static bool support_compat_dir    = true;// default supports compatibility directory type
static int max_keys_list_object   = 1000;// default is 1000

static const std::string allbucket_fields_type = "";         // special key for mapping(This name is absolutely not used as a bucket name)
static const std::string keyval_fields_type    = "\t";       // special key for mapping(This name is absolutely not used as a bucket name)
static const std::string aws_accesskeyid       = "AWSAccessKeyId";
static const std::string aws_secretkey         = "AWSSecretKey";

//-------------------------------------------------------------------
// Static functions : prototype
//-------------------------------------------------------------------
static void s3fs_usr2_handler(int sig);
static bool set_s3fs_usr2_handler(void);
static s3fs_log_level set_s3fs_log_level(s3fs_log_level level);
static s3fs_log_level bumpup_s3fs_log_level(void);
static int get_object_attribute(const char* path, struct stat* pstbuf, headers_t* pmeta = NULL, bool overcheck = true, bool* pisforce = NULL, bool add_no_truncate_cache = false);
static bool GetXmlNsUrl(xmlDocPtr doc, string& nsurl);
static int remote_mountpath_exists(const char* path);
static xmlChar* get_exp_value_xml(xmlDocPtr doc, xmlXPathContextPtr ctx, const char* exp_key);
static void print_uncomp_mp_list(uncomp_mp_list_t& list);
static bool abort_uncomp_mp_list(uncomp_mp_list_t& list);
static bool get_uncomp_mp_list(xmlDocPtr doc, uncomp_mp_list_t& list);
static int s3fs_utility_mode(void);
static int s3fs_check_service(void);
static int parse_passwd_file(bucketkvmap_t& resmap);
static int check_for_aws_format(const kvmap_t& kvmap);
static int check_passwd_file_perms(void);
static int read_passwd_file(void);
static int get_access_keys(void);
static int set_mountpoint_attribute(struct stat& mpst);
static int set_bucket(const char* arg);
static int my_fuse_opt_proc(void* data, const char* arg, int key, struct fuse_args* outargs);

// fuse interface functions
static int s3fs_getattr(const char* path, struct stat* stbuf) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.getattr(stbuf);
}
static int s3fs_readlink(const char* path, char* buf, size_t size) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.readlink(buf, size);
}
static int s3fs_mknod(const char* path, mode_t mode, dev_t rdev) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.mknod(mode, rdev);
}
static int s3fs_mkdir(const char* path, mode_t mode) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.mkdir(mode);
}
static int s3fs_unlink(const char* path) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.unlink();
}
static int s3fs_rmdir(const char* path) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.rmdir();
}    
static int s3fs_symlink(const char* from, const char* to) {
    S3FS_STATS();
    S3fsOper oper(to, from);
    return oper.symlink();
}
static int s3fs_rename(const char* from, const char* to) {
    S3FS_STATS();
    S3fsOper oper(to, from);
    return oper.rename();
}
static int s3fs_link(const char* from, const char* to) {
    S3FS_STATS();
    S3fsOper oper(to, from);
    return oper.link();
}
static int s3fs_chmod(const char* path, mode_t mode) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.chmod(mode);
}
static int s3fs_chmod_nocopy(const char* path, mode_t mode) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.chmod(mode, false);
}
static int s3fs_chown(const char* path, uid_t uid, gid_t gid) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.chown(uid, gid);
}
static int s3fs_chown_nocopy(const char* path, uid_t uid, gid_t gid) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.chown(uid, gid, false);
}
static int s3fs_utimens(const char* path, const struct timespec ts[2]) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.utimens(ts);
}
static int s3fs_utimens_nocopy(const char* path, const struct timespec ts[2]) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.utimens(ts, false);
}
static int s3fs_truncate(const char* path, off_t size) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.truncate(size);
}
static int s3fs_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.create(mode, fi);
}
static int s3fs_open(const char* path, struct fuse_file_info* fi) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.open(fi);
}
static int s3fs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.read(buf, size, offset, fi);
}
static int s3fs_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.write(buf, size, offset, fi);
}
static int s3fs_statfs(const char* path, struct statvfs* stbuf) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.statfs(stbuf);
}
static int s3fs_flush(const char* path, struct fuse_file_info* fi) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.flush(fi);
}
static int s3fs_fsync(const char* path, int datasync, struct fuse_file_info* fi) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.fsync(datasync, fi);
}
static int s3fs_release(const char* path, struct fuse_file_info* fi) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.release(fi);
}
static int s3fs_opendir(const char* path, struct fuse_file_info* fi) {
    S3FS_STATS();
    return S3fsOper(path).opendir(fi);
}
static int s3fs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.readdir(buf, filler, offset, fi);
}
static int s3fs_access(const char* path, int mask) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.access(mask);
}

static int s3fs_setxattr(const char* path, const char* name, const char* value, size_t size, int flags) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.setxattr(name, value, size, flags);
}
static int s3fs_getxattr(const char* path, const char* name, char* value, size_t size) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.getxattr(name, value, size);
}


static int s3fs_listxattr(const char* path, char* list, size_t size) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.listxattr(list, size);
}


static int s3fs_removexattr(const char* path, const char* name) {
    S3FS_STATS();
    S3fsOper oper(path);
    return oper.removexattr(name);
}

static void* s3fs_init(struct fuse_conn_info* conn);
static void s3fs_destroy(void*);


//-------------------------------------------------------------------
// Functions
//-------------------------------------------------------------------
static void s3fs_usr2_handler(int sig)
{
  if(SIGUSR2 == sig){
    bumpup_s3fs_log_level();
  }
}
static bool set_s3fs_usr2_handler(void)
{
  struct sigaction sa;

  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_handler = s3fs_usr2_handler;
  sa.sa_flags   = SA_RESTART;
  if(0 != sigaction(SIGUSR2, &sa, NULL)){
    return false;
  }
  return true;
}

static s3fs_log_level set_s3fs_log_level(s3fs_log_level level)
{
  if(level == debug_level){
    return debug_level;
  }
  s3fs_log_level old = debug_level;
  debug_level        = level;
  setlogmask(LOG_UPTO(S3FS_LOG_LEVEL_TO_SYSLOG(debug_level)));
  S3FS_PRN_CRIT("change debug level from %sto %s", S3FS_LOG_LEVEL_STRING(old), S3FS_LOG_LEVEL_STRING(debug_level));
  return old;
}

static s3fs_log_level bumpup_s3fs_log_level(void)
{
  s3fs_log_level old = debug_level;
  debug_level        = ( S3FS_LOG_CRIT == debug_level ? S3FS_LOG_ERR :
                         S3FS_LOG_ERR  == debug_level ? S3FS_LOG_WARN :
                         S3FS_LOG_WARN == debug_level ? S3FS_LOG_INFO :
                         S3FS_LOG_INFO == debug_level ? S3FS_LOG_DBG :
                         S3FS_LOG_CRIT );
  setlogmask(LOG_UPTO(S3FS_LOG_LEVEL_TO_SYSLOG(debug_level)));
  S3FS_PRN_CRIT("change debug level from %sto %s", S3FS_LOG_LEVEL_STRING(old), S3FS_LOG_LEVEL_STRING(debug_level));
  return old;
}







//
// Get object attributes with stat cache.
// This function is base for s3fs_getattr().
//
// [NOTICE]
// Checking order is changed following list because of reducing the number of the requests.
// 1) "dir"
// 2) "dir/"
// 3) "dir_$folder$"
//
static int get_object_attribute(const char* path, struct stat* pstbuf, headers_t* pmeta, bool overcheck, bool* pisforce, bool add_no_truncate_cache)
{
  int          result = -1;
  struct stat  tmpstbuf;
  struct stat* pstat = pstbuf ? pstbuf : &tmpstbuf;

  S3FS_PRN_DBG("[path=%s]", path);

  if(!path || '\0' == path[0]){
    return -ENOENT;
  }

  memset(pstat, 0, sizeof(struct stat));
  if(0 == strcmp(path, "/") || 0 == strcmp(path, ".")){
    pstat->st_nlink = 1; // see fuse faq
    pstat->st_mode  = mp_mode;
    pstat->st_uid   = is_s3fs_uid ? s3fs_uid : mp_uid;
    pstat->st_gid   = is_s3fs_gid ? s3fs_gid : mp_gid;
    return 0;
  }

  VfsEnt stVfsEnt(path);
  result = stVfsEnt.init();
  if (stVfsEnt.isExists()) {
      *pstbuf = stVfsEnt.stat();
  } else {
      result = -ENOENT;
  }
  
  return result;
}



//
// ssevalue is MD5 for SSE-C type, or KMS id for SSE-KMS
//
bool get_object_sse_type(const char* path, sse_type_t& ssetype, string& ssevalue)
{
  if(!path){
    return false;
  }

  headers_t meta;
  if(0 != get_object_attribute(path, NULL, &meta)){
    S3FS_PRN_ERR("Failed to get object(%s) headers", path);
    return false;
  }

  ssetype = SSE_DISABLE;
  ssevalue.erase();
  for(headers_t::iterator iter = meta.begin(); iter != meta.end(); ++iter){
    string key = (*iter).first;
    if(0 == strcasecmp(key.c_str(), "x-amz-server-side-encryption") && 0 == strcasecmp((*iter).second.c_str(), "AES256")){
      ssetype  = SSE_S3;
    }else if(0 == strcasecmp(key.c_str(), "x-amz-server-side-encryption-aws-kms-key-id")){
      ssetype  = SSE_KMS;
      ssevalue = (*iter).second;
    }else if(0 == strcasecmp(key.c_str(), "x-amz-server-side-encryption-customer-key-md5")){
      ssetype  = SSE_C;
      ssevalue = (*iter).second;
    }
  }
  return true;
}




static int do_create_bucket(void)
{
  S3FS_PRN_INFO2("/");

  FILE* ptmpfp;
  int   tmpfd;
  if(endpoint == "us-east-1"){
    ptmpfp = NULL;
    tmpfd = -1;
  }else{
    if(NULL == (ptmpfp = tmpfile()) ||
       -1 == (tmpfd = fileno(ptmpfp)) ||
       0 >= fprintf(ptmpfp, "<CreateBucketConfiguration xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n"
        "  <LocationConstraint>%s</LocationConstraint>\n"
        "</CreateBucketConfiguration>", endpoint.c_str()) ||
       0 != fflush(ptmpfp) ||
       -1 == fseek(ptmpfp, 0L, SEEK_SET)){
      S3FS_PRN_ERR("failed to create temporary file. err(%d)", errno);
      if(ptmpfp){
        fclose(ptmpfp);
      }
      return (0 == errno ? -EIO : -errno);
    }
  }

  headers_t meta;

  S3fsCurl s3fscurl(true);
  long     res = s3fscurl.PutRequest("/", meta, tmpfd);
  if(res < 0){
    long responseCode = s3fscurl.GetLastResponseCode();
    if((responseCode == 400 || responseCode == 403) && S3fsCurl::IsSignatureV4()){
      S3FS_PRN_ERR("Could not connect, so retry to connect by signature version 2.");
      S3fsCurl::SetSignatureV4(false);

      // retry to check
      s3fscurl.DestroyCurlHandle();
      res = s3fscurl.PutRequest("/", meta, tmpfd);
    }
  }
  if(ptmpfp != NULL){
    fclose(ptmpfp);
  }
  return res;
}




static bool GetXmlNsUrl(xmlDocPtr doc, string& nsurl)
{
  static time_t tmLast = 0;  // cache for 60 sec.
  static string strNs("");
  bool result = false;

  if(!doc){
    return result;
  }
  if((tmLast + 60) < time(NULL)){
    // refresh
    tmLast = time(NULL);
    strNs  = "";
    xmlNodePtr pRootNode = xmlDocGetRootElement(doc);
    if(pRootNode){
      xmlNsPtr* nslist = xmlGetNsList(doc, pRootNode);
      if(nslist){
        if(nslist[0] && nslist[0]->href){
          strNs  = (const char*)(nslist[0]->href);
        }
        S3FS_XMLFREE(nslist);
      }
    }
  }
  if(0 < strNs.size()){
    nsurl  = strNs;
    result = true;
  }
  return result;
}











static int remote_mountpath_exists(const char* path)
{
  struct stat stbuf;

  S3FS_PRN_INFO1("[path=%s]", path);

  // getattr will prefix the path with the remote mountpoint
  if(0 != get_object_attribute("/", &stbuf, NULL)){
    return -1;
  }
  if(!S_ISDIR(stbuf.st_mode)){
    return -1;
  }
  return 0;
}







   
// s3fs_init calls this function to exit cleanly from the fuse event loop.
//
// There's no way to pass an exit status to the high-level event loop API, so 
// this function stores the exit value in a global for main()
static void s3fs_exit_fuseloop(int exit_status) {
    S3FS_PRN_ERR("Exiting FUSE event loop due to errors\n");
    s3fs_init_deferred_exit_status = exit_status;
    struct fuse_context *ctx = fuse_get_context();
    if (NULL != ctx) {
        fuse_exit(ctx->fuse);
    }
}

static void* s3fs_init(struct fuse_conn_info* conn)
{
  // check loading IAM role name
  if(load_iamrole){
    // load IAM role name from http://169.254.169.254/latest/meta-data/iam/security-credentials
    //
    S3fsCurl s3fscurl;
    if(!s3fscurl.LoadIAMRoleFromMetaData()){
      S3FS_PRN_CRIT("could not load IAM role name from meta data.");
      s3fs_exit_fuseloop(EXIT_FAILURE);
      return NULL;
    }
    S3FS_PRN_INFO("loaded IAM role name = %s", S3fsCurl::GetIAMRole());
  }
  // Investigate system capabilities
  #ifndef __APPLE__
  if((unsigned int)conn->capable & FUSE_CAP_ATOMIC_O_TRUNC){
     conn->want |= FUSE_CAP_ATOMIC_O_TRUNC;
  }
  #endif
  
  if((unsigned int)conn->capable & FUSE_CAP_BIG_WRITES){
     conn->want |= FUSE_CAP_BIG_WRITES;
  }

  S3Stat::instance().setBucket(bucket);
  S3Stat::instance().start();
  AutoFileLock::init();
  int rc = 0;
  rc = S3RSync::Instance().start();
  if (rc) {
    S3FS_PRN_CRIT("S3RSync::init failed(%d).", rc);
    return NULL;  
  }
  

  return NULL;
}

static void s3fs_destroy(void*)
{
  S3FS_PRN_INFO("destroy");

  // cache(remove at last)
  if(is_remove_cache && (!CacheFileStat::DeleteCacheFileStatDirectory() || !FdManager::DeleteCacheDirectory())){
    S3FS_PRN_WARN("Could not remove cache directory.");
  }

  S3RSync::Instance().stop();
  S3Stat::instance().stop();
}


static xmlChar* get_exp_value_xml(xmlDocPtr doc, xmlXPathContextPtr ctx, const char* exp_key)
{
  if(!doc || !ctx || !exp_key){
    return NULL;
  }

  xmlXPathObjectPtr exp;
  xmlNodeSetPtr     exp_nodes;
  xmlChar*          exp_value;

  // search exp_key tag
  if(NULL == (exp = xmlXPathEvalExpression((xmlChar*)exp_key, ctx))){
    S3FS_PRN_ERR("Could not find key(%s).", exp_key);
    return NULL;
  }
  if(xmlXPathNodeSetIsEmpty(exp->nodesetval)){
    S3FS_PRN_ERR("Key(%s) node is empty.", exp_key);
    S3FS_XMLXPATHFREEOBJECT(exp);
    return NULL;
  }
  // get exp_key value & set in struct
  exp_nodes = exp->nodesetval;
  if(NULL == (exp_value = xmlNodeListGetString(doc, exp_nodes->nodeTab[0]->xmlChildrenNode, 1))){
    S3FS_PRN_ERR("Key(%s) value is empty.", exp_key);
    S3FS_XMLXPATHFREEOBJECT(exp);
    return NULL;
  }

  S3FS_XMLXPATHFREEOBJECT(exp);
  return exp_value;
}

static void print_uncomp_mp_list(uncomp_mp_list_t& list)
{
  printf("\n");
  printf("Lists the parts that have been uploaded for a specific multipart upload.\n");
  printf("\n");

  if(!list.empty()){
    printf("---------------------------------------------------------------\n");

    int cnt = 0;
    for(uncomp_mp_list_t::iterator iter = list.begin(); iter != list.end(); ++iter, ++cnt){
      printf(" Path     : %s\n", (*iter).key.c_str());
      printf(" UploadId : %s\n", (*iter).id.c_str());
      printf(" Date     : %s\n", (*iter).date.c_str());
      printf("\n");
    }
    printf("---------------------------------------------------------------\n");

  }else{
    printf("There is no list.\n");
  }
}

static bool abort_uncomp_mp_list(uncomp_mp_list_t& list)
{
  char buff[1024];

  if(list.empty()){
    return true;
  }
  memset(buff, 0, sizeof(buff));

  // confirm
  while(true){
    printf("Would you remove all objects? [Y/N]\n");
    if(NULL != fgets(buff, sizeof(buff), stdin)){
      if(0 == strcasecmp(buff, "Y\n") || 0 == strcasecmp(buff, "YES\n")){
        break;
      }else if(0 == strcasecmp(buff, "N\n") || 0 == strcasecmp(buff, "NO\n")){
        return true;
      }
      printf("*** please put Y(yes) or N(no).\n");
    }
  }

  // do removing their.
  S3fsCurl s3fscurl;
  bool     result = true;
  for(uncomp_mp_list_t::iterator iter = list.begin(); iter != list.end(); ++iter){
    const char* tpath     = (*iter).key.c_str();
    string      upload_id = (*iter).id;

    if(0 != s3fscurl.AbortMultipartUpload(tpath, upload_id)){
      S3FS_PRN_EXIT("Failed to remove %s multipart uploading object.", tpath);
      result = false;
    }else{
      printf("Succeed to remove %s multipart uploading object.\n", tpath);
    }

    // reset(initialize) curl object
    s3fscurl.DestroyCurlHandle();
  }

  return result;
}

static bool get_uncomp_mp_list(xmlDocPtr doc, uncomp_mp_list_t& list)
{
  if(!doc){
    return false;
  }

  xmlXPathContextPtr ctx = xmlXPathNewContext(doc);;

  string xmlnsurl;
  string ex_upload = "//";
  string ex_key    = "";
  string ex_id     = "";
  string ex_date   = "";

  if(!noxmlns && GetXmlNsUrl(doc, xmlnsurl)){
    xmlXPathRegisterNs(ctx, (xmlChar*)"s3", (xmlChar*)xmlnsurl.c_str());
    ex_upload += "s3:";
    ex_key    += "s3:";
    ex_id     += "s3:";
    ex_date   += "s3:";
  }
  ex_upload += "Upload";
  ex_key    += "Key";
  ex_id     += "UploadId";
  ex_date   += "Initiated";

  // get "Upload" Tags
  xmlXPathObjectPtr  upload_xp;
  if(NULL == (upload_xp = xmlXPathEvalExpression((xmlChar*)ex_upload.c_str(), ctx))){
    S3FS_PRN_ERR("xmlXPathEvalExpression returns null.");
    return false;
  }
  if(xmlXPathNodeSetIsEmpty(upload_xp->nodesetval)){
    S3FS_PRN_INFO("upload_xp->nodesetval is empty.");
    S3FS_XMLXPATHFREEOBJECT(upload_xp);
    S3FS_XMLXPATHFREECONTEXT(ctx);
    return true;
  }

  // Make list
  int           cnt;
  xmlNodeSetPtr upload_nodes;
  list.clear();
  for(cnt = 0, upload_nodes = upload_xp->nodesetval; cnt < upload_nodes->nodeNr; cnt++){
    ctx->node = upload_nodes->nodeTab[cnt];

    UNCOMP_MP_INFO  part;
    xmlChar*        ex_value;

    // search "Key" tag
    if(NULL == (ex_value = get_exp_value_xml(doc, ctx, ex_key.c_str()))){
      continue;
    }
    if('/' != *((char*)ex_value)){
      part.key = "/";
    }else{
      part.key = "";
    }
    part.key += (char*)ex_value;
    S3FS_XMLFREE(ex_value);

    // search "UploadId" tag
    if(NULL == (ex_value = get_exp_value_xml(doc, ctx, ex_id.c_str()))){
      continue;
    }
    part.id = (char*)ex_value;
    S3FS_XMLFREE(ex_value);

    // search "Initiated" tag
    if(NULL == (ex_value = get_exp_value_xml(doc, ctx, ex_date.c_str()))){
      continue;
    }
    part.date = (char*)ex_value;
    S3FS_XMLFREE(ex_value);

    list.push_back(part);
  }

  S3FS_XMLXPATHFREEOBJECT(upload_xp);
  S3FS_XMLXPATHFREECONTEXT(ctx);

  return true;
}

static int s3fs_utility_mode(void)
{
  if(!utility_mode){
    return EXIT_FAILURE;
  }
  printf("Utility Mode\n");

  S3fsCurl s3fscurl;
  string   body;
  int      result = EXIT_SUCCESS;
  if(0 != s3fscurl.MultipartListRequest(body)){
    S3FS_PRN_EXIT("Could not get list multipart upload.");
    result = EXIT_FAILURE;
  }else{
    // parse result(incomplete multipart upload information)
    S3FS_PRN_DBG("response body = {\n%s\n}", body.c_str());

    xmlDocPtr doc;
    if(NULL == (doc = xmlReadMemory(body.c_str(), static_cast<int>(body.size()), "", NULL, 0))){
      S3FS_PRN_DBG("xmlReadMemory exited with error.");
      result = EXIT_FAILURE;

    }else{
      // make working uploads list
      uncomp_mp_list_t list;
      if(!get_uncomp_mp_list(doc, list)){
        S3FS_PRN_DBG("get_uncomp_mp_list exited with error.");
        result = EXIT_FAILURE;

      }else{
        // print list
        print_uncomp_mp_list(list);
        // remove
        if(!abort_uncomp_mp_list(list)){
          S3FS_PRN_DBG("an error occurred during removal process.");
          result = EXIT_FAILURE;
        }
      }
      S3FS_XMLFREEDOC(doc);
    }
  }

  // Destroy curl
  if(!S3fsCurl::DestroyS3fsCurl()){
    S3FS_PRN_WARN("Could not release curl library.");
  }

  // ssl
  s3fs_destroy_global_ssl();

  return result;
}

//
// If calling with wrong region, s3fs gets following error body as 400 error code.
// "<Error><Code>AuthorizationHeaderMalformed</Code><Message>The authorization header is 
//  malformed; the region 'us-east-1' is wrong; expecting 'ap-northeast-1'</Message>
//  <Region>ap-northeast-1</Region><RequestId>...</RequestId><HostId>...</HostId>
//  </Error>"
//
// So this is cheep codes but s3fs should get correct region automatically.
//
static bool check_region_error(const char* pbody, string& expectregion)
{
  if(!pbody){
    return false;
  }
  const char* region;
  const char* regionend;
  if(NULL == (region = strcasestr(pbody, "<Message>The authorization header is malformed; the region "))){
    return false;
  }
  if(NULL == (region = strcasestr(region, "expecting \'"))){
    return false;
  }
  region += strlen("expecting \'");
  if(NULL == (regionend = strchr(region, '\''))){
    return false;
  }
  string strtmp(region, (regionend - region));
  if(0 == strtmp.length()){
    return false;
  }
  expectregion = strtmp;

  return true;
}

static int s3fs_check_service(void)
{
  S3FS_PRN_INFO("check services.");

  // At first time for access S3, we check IAM role if it sets.
  if(!S3fsCurl::CheckIAMCredentialUpdate()){
    S3FS_PRN_CRIT("Failed to check IAM role name(%s).", S3fsCurl::GetIAMRole());
    return EXIT_FAILURE;
  }

  S3fsCurl s3fscurl;
  int      res;
  if(0 > (res = s3fscurl.CheckBucket())){
    // get response code
    long responseCode = s3fscurl.GetLastResponseCode();

    // check wrong endpoint, and automatically switch endpoint
    if(responseCode == 400 && !is_specified_endpoint){
      // check region error
      BodyData* body = s3fscurl.GetBodyData();
      string    expectregion;
      if(check_region_error(body->str(), expectregion)){
        // not specified endpoint, so try to connect to expected region.
        S3FS_PRN_CRIT("Could not connect wrong region %s, so retry to connect region %s.", endpoint.c_str(), expectregion.c_str());
        endpoint = expectregion;

        // retry to check with new endpoint
        s3fscurl.DestroyCurlHandle();
        res          = s3fscurl.CheckBucket();
        responseCode = s3fscurl.GetLastResponseCode();
      }
    }

    // try signature v2
    if(0 > res && (responseCode == 400 || responseCode == 403) && S3fsCurl::IsSignatureV4()){
      // switch sigv2
      S3FS_PRN_WARN("Could not connect, so retry to connect by signature version 2.");
      S3fsCurl::SetSignatureV4(false);

      // retry to check with sigv2
      s3fscurl.DestroyCurlHandle();
      res          = s3fscurl.CheckBucket();
      responseCode = s3fscurl.GetLastResponseCode();
    }

    // check errors(after retrying)
    if(0 > res && responseCode != 200 && responseCode != 301){
      if(responseCode == 400){
        S3FS_PRN_CRIT("Bad Request(host=%s) - result of checking service.", host.c_str());

      }else if(responseCode == 403){
        S3FS_PRN_CRIT("invalid credentials(host=%s) - result of checking service.", host.c_str());

      }else if(responseCode == 404){
        S3FS_PRN_CRIT("bucket not found(host=%s) - result of checking service.", host.c_str());

      }else if(responseCode == CURLE_OPERATION_TIMEDOUT){
        // unable to connect
        S3FS_PRN_CRIT("unable to connect bucket and timeout(host=%s) - result of checking service.", host.c_str());
      }else{
        // another error
        S3FS_PRN_CRIT("unable to connect(host=%s) - result of checking service.", host.c_str());
      }
      return EXIT_FAILURE;
    }
  }
  s3fscurl.DestroyCurlHandle();

  // make sure remote mountpath exists and is a directory
  if(mount_prefix.size() > 0){
    if(remote_mountpath_exists(mount_prefix.c_str()) != 0){
      S3FS_PRN_CRIT("remote mountpath %s not found.", mount_prefix.c_str());
      return EXIT_FAILURE;
    }
  }
  S3FS_MALLOCTRIM(0);

  return EXIT_SUCCESS;
}

//
// Read and Parse passwd file
//
// The line of the password file is one of the following formats:
//   (1) "accesskey:secretkey"         : AWS format for default(all) access key/secret key
//   (2) "bucket:accesskey:secretkey"  : AWS format for bucket's access key/secret key
//   (3) "key=value"                   : Content-dependent KeyValue contents
//
// This function sets result into bucketkvmap_t, it bucket name and key&value mapping.
// If bucket name is empty(1 or 3 format), bucket name for mapping is set "\t" or "".
//
// Return:  1 - OK(could parse and set mapping etc.)
//          0 - NG(could not read any value)
//         -1 - Should shutdown immediately
//
static int parse_passwd_file(bucketkvmap_t& resmap)
{
  string line;
  size_t first_pos;
  readline_t linelist;
  readline_t::iterator iter;

  // open passwd file
  ifstream PF(passwd_file.c_str());
  if(!PF.good()){
    S3FS_PRN_EXIT("could not open passwd file : %s", passwd_file.c_str());
    return -1;
  }

  // read each line
  while(getline(PF, line)){
    line = trim(line);
    if(0 == line.size()){
      continue;
    }
    if('#' == line[0]){
      continue;
    }
    if(string::npos != line.find_first_of(" \t")){
      S3FS_PRN_EXIT("invalid line in passwd file, found whitespace character.");
      return -1;
    }
    if(0 == line.find_first_of("[")){
      S3FS_PRN_EXIT("invalid line in passwd file, found a bracket \"[\" character.");
      return -1;
    }
    linelist.push_back(line);
  }

  // read '=' type
  kvmap_t kv;
  for(iter = linelist.begin(); iter != linelist.end(); ++iter){
    first_pos = iter->find_first_of("=");
    if(first_pos == string::npos){
      continue;
    }
    // formatted by "key=val"
    string key = trim(iter->substr(0, first_pos));
    string val = trim(iter->substr(first_pos + 1, string::npos));
    if(key.empty()){
      continue;
    }
    if(kv.end() != kv.find(key)){
      S3FS_PRN_WARN("same key name(%s) found in passwd file, skip this.", key.c_str());
      continue;
    }
    kv[key] = val;
  }
  // set special key name
  resmap[string(keyval_fields_type)] = kv;

  // read ':' type
  for(iter = linelist.begin(); iter != linelist.end(); ++iter){
    first_pos       = iter->find_first_of(":");
    size_t last_pos = iter->find_last_of(":");
    if(first_pos == string::npos){
      continue;
    }
    string bucket;
    string accesskey;
    string secret;
    if(first_pos != last_pos){
      // formatted by "bucket:accesskey:secretkey"
      bucket    = trim(iter->substr(0, first_pos));
      accesskey = trim(iter->substr(first_pos + 1, last_pos - first_pos - 1));
      secret    = trim(iter->substr(last_pos + 1, string::npos));
    }else{
      // formatted by "accesskey:secretkey"
      bucket    = allbucket_fields_type;
      accesskey = trim(iter->substr(0, first_pos));
      secret    = trim(iter->substr(first_pos + 1, string::npos));
    }
    if(resmap.end() != resmap.find(bucket)){
      S3FS_PRN_EXIT("there are multiple entries for the same bucket(%s) in the passwd file.", ("" == bucket ? "default" : bucket.c_str()));
      return -1;
    }
    kv.clear();
    kv[string(aws_accesskeyid)] = accesskey;
    kv[string(aws_secretkey)]   = secret;
    resmap[bucket]              = kv;
  }
  return (resmap.empty() ? 0 : 1);
}

//
// Return:  1 - OK(could read and set accesskey etc.)
//          0 - NG(could not read)
//         -1 - Should shutdown immediately
//
static int check_for_aws_format(const kvmap_t& kvmap)
{
  string str1(aws_accesskeyid);
  string str2(aws_secretkey);

  if(kvmap.empty()){
    return 0;
  }
  if(kvmap.end() == kvmap.find(str1) && kvmap.end() == kvmap.find(str2)){
    return 0;
  }
  if(kvmap.end() == kvmap.find(str1) || kvmap.end() == kvmap.find(str2)){
    S3FS_PRN_EXIT("AWSAccesskey or AWSSecretkey is not specified.");
    return -1;
  }
  if(!S3fsCurl::SetAccessKey(kvmap.at(str1).c_str(), kvmap.at(str2).c_str())){
    S3FS_PRN_EXIT("failed to set access key/secret key.");
    return -1;
  }
  return 1;
}

//
// check_passwd_file_perms
// 
// expect that global passwd_file variable contains
// a non-empty value and is readable by the current user
//
// Check for too permissive access to the file
// help save users from themselves via a security hole
//
// only two options: return or error out
//
static int check_passwd_file_perms(void)
{
  struct stat info;

  // let's get the file info
  if(stat(passwd_file.c_str(), &info) != 0){
    S3FS_PRN_EXIT("unexpected error from stat(%s).", passwd_file.c_str());
    return EXIT_FAILURE;
  }

  // return error if any file has others permissions 
  if( (info.st_mode & S_IROTH) ||
      (info.st_mode & S_IWOTH) || 
      (info.st_mode & S_IXOTH)) {
    S3FS_PRN_EXIT("credentials file %s should not have others permissions.", passwd_file.c_str());
    return EXIT_FAILURE;
  }

  // Any local file should not have any group permissions 
  // /etc/passwd-s3fs can have group permissions 
  if(passwd_file != "/etc/passwd-s3fs"){
    if( (info.st_mode & S_IRGRP) ||
        (info.st_mode & S_IWGRP) || 
        (info.st_mode & S_IXGRP)) {
      S3FS_PRN_EXIT("credentials file %s should not have group permissions.", passwd_file.c_str());
      return EXIT_FAILURE;
    }
  }else{
    // "/etc/passwd-s3fs" does not allow group write.
    if((info.st_mode & S_IWGRP)){
      S3FS_PRN_EXIT("credentials file %s should not have group writable permissions.", passwd_file.c_str());
      return EXIT_FAILURE;
    }
  }
  if((info.st_mode & S_IXUSR) || (info.st_mode & S_IXGRP)){
    S3FS_PRN_EXIT("credentials file %s should not have executable permissions.", passwd_file.c_str());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

//
// read_passwd_file
//
// Support for per bucket credentials
// 
// Format for the credentials file:
// [bucket:]AccessKeyId:SecretAccessKey
// 
// Lines beginning with # are considered comments
// and ignored, as are empty lines
//
// Uncommented lines without the ":" character are flagged as
// an error, so are lines with spaces or tabs
//
// only one default key pair is allowed, but not required
//
static int read_passwd_file(void)
{
  bucketkvmap_t bucketmap;
  kvmap_t       keyval;
  int           result;

  // if you got here, the password file
  // exists and is readable by the
  // current user, check for permissions
  if(EXIT_SUCCESS != check_passwd_file_perms()){
    return EXIT_FAILURE;
  }

  //
  // parse passwd file
  //
  result = parse_passwd_file(bucketmap);
  if(-1 == result){
     return EXIT_FAILURE;
  }

  //
  // check key=value type format.
  //
  if(bucketmap.end() != bucketmap.find(keyval_fields_type)){
    // aws format
    result = check_for_aws_format(bucketmap[keyval_fields_type]);
    if(-1 == result){
       return EXIT_FAILURE;
    }else if(1 == result){
       // success to set
       return EXIT_SUCCESS;
    }
  }

  string bucket_key = allbucket_fields_type;
  if(0 < bucket.size() && bucketmap.end() != bucketmap.find(bucket)){
    bucket_key = bucket;
  }
  if(bucketmap.end() == bucketmap.find(bucket_key)){
    S3FS_PRN_EXIT("Not found access key/secret key in passwd file.");
    return EXIT_FAILURE;
  }
  keyval = bucketmap[bucket_key];
  if(keyval.end() == keyval.find(string(aws_accesskeyid)) || keyval.end() == keyval.find(string(aws_secretkey))){
    S3FS_PRN_EXIT("Not found access key/secret key in passwd file.");
    return EXIT_FAILURE;
  }
  if(!S3fsCurl::SetAccessKey(keyval.at(string(aws_accesskeyid)).c_str(), keyval.at(string(aws_secretkey)).c_str())){
    S3FS_PRN_EXIT("failed to set internal data for access key/secret key from passwd file.");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

//
// get_access_keys
//
// called only when were are not mounting a 
// public bucket
//
// Here is the order precedence for getting the
// keys:
//
// 1 - from the command line  (security risk)
// 2 - from a password file specified on the command line
// 3 - from environment variables
// 4 - from the users ~/.passwd-s3fs
// 5 - from /etc/passwd-s3fs
//
static int get_access_keys(void)
{
  // should be redundant
  if(S3fsCurl::IsPublicBucket()){
     return EXIT_SUCCESS;
  }

  // access key loading is deferred
  if(load_iamrole || is_ecs){
     return EXIT_SUCCESS;
  }

  // 1 - keys specified on the command line
  if(S3fsCurl::IsSetAccessKeys()){
     return EXIT_SUCCESS;
  }

  // 2 - was specified on the command line
  if(passwd_file.size() > 0){
    ifstream PF(passwd_file.c_str());
    if(PF.good()){
       PF.close();
       return read_passwd_file();
    }else{
      S3FS_PRN_EXIT("specified passwd_file is not readable.");
      return EXIT_FAILURE;
    }
  }

  // 3  - environment variables
  char* AWSACCESSKEYID     = getenv("AWSACCESSKEYID");
  char* AWSSECRETACCESSKEY = getenv("AWSSECRETACCESSKEY");
  if(AWSACCESSKEYID != NULL || AWSSECRETACCESSKEY != NULL){
    if( (AWSACCESSKEYID == NULL && AWSSECRETACCESSKEY != NULL) ||
        (AWSACCESSKEYID != NULL && AWSSECRETACCESSKEY == NULL) ){
      S3FS_PRN_EXIT("if environment variable AWSACCESSKEYID is set then AWSSECRETACCESSKEY must be set too.");
      return EXIT_FAILURE;
    }
    if(!S3fsCurl::SetAccessKey(AWSACCESSKEYID, AWSSECRETACCESSKEY)){
      S3FS_PRN_EXIT("if one access key is specified, both keys need to be specified.");
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }

  // 3a - from the AWS_CREDENTIAL_FILE environment variable
  char * AWS_CREDENTIAL_FILE;
  AWS_CREDENTIAL_FILE = getenv("AWS_CREDENTIAL_FILE");
  if(AWS_CREDENTIAL_FILE != NULL){
    passwd_file.assign(AWS_CREDENTIAL_FILE);
    if(passwd_file.size() > 0){
      ifstream PF(passwd_file.c_str());
      if(PF.good()){
         PF.close();
         return read_passwd_file();
      }else{
        S3FS_PRN_EXIT("AWS_CREDENTIAL_FILE: \"%s\" is not readable.", passwd_file.c_str());
        return EXIT_FAILURE;
      }
    }
  }

  // 4 - from the default location in the users home directory
  char * HOME;
  HOME = getenv ("HOME");
  if(HOME != NULL){
     passwd_file.assign(HOME);
     passwd_file.append("/.passwd-s3fs");
     ifstream PF(passwd_file.c_str());
     if(PF.good()){
       PF.close();
       if(EXIT_SUCCESS != read_passwd_file()){
         return EXIT_FAILURE;
       }
       // It is possible that the user's file was there but
       // contained no key pairs i.e. commented out
       // in that case, go look in the final location
       if(S3fsCurl::IsSetAccessKeys()){
          return EXIT_SUCCESS;
       }
     }
   }

  // 5 - from the system default location
  passwd_file.assign("/etc/passwd-s3fs"); 
  ifstream PF(passwd_file.c_str());
  if(PF.good()){
    PF.close();
    return read_passwd_file();
  }
  S3FS_PRN_EXIT("could not determine how to establish security credentials.");

  return EXIT_FAILURE;
}

//
// Check & Set attributes for mount point.
//
static int set_mountpoint_attribute(struct stat& mpst)
{
  mp_uid  = geteuid();
  mp_gid  = getegid();
  mp_mode = S_IFDIR | (allow_other ? (is_mp_umask ? (~mp_umask & (S_IRWXU | S_IRWXG | S_IRWXO)) : (S_IRWXU | S_IRWXG | S_IRWXO)) : S_IRWXU);

  S3FS_PRN_INFO2("PROC(uid=%u, gid=%u) - MountPoint(uid=%u, gid=%u, mode=%04o)",
         (unsigned int)mp_uid, (unsigned int)mp_gid, (unsigned int)(mpst.st_uid), (unsigned int)(mpst.st_gid), mpst.st_mode);

  // check owner
  if(0 == mp_uid || mpst.st_uid == mp_uid){
    return true;
  }
  // check group permission
  if(mpst.st_gid == mp_gid || 1 == is_uid_include_group(mp_uid, mpst.st_gid)){
    if(S_IRWXG == (mpst.st_mode & S_IRWXG)){
      return true;
    }
  }
  // check other permission
  if(S_IRWXO == (mpst.st_mode & S_IRWXO)){
    return true;
  }
  return false;
}

//
// Set bucket and mount_prefix based on passed bucket name.
//
static int set_bucket(const char* arg)
{
  char *bucket_name = (char*)arg;
  if(strstr(arg, ":")){
    if(strstr(arg, "://")){
      S3FS_PRN_EXIT("bucket name and path(\"%s\") is wrong, it must be \"bucket[:/path]\".", arg);
      return -1;
    }
    bucket = strtok(bucket_name, ":");
    char* pmount_prefix = strtok(NULL, ":");
    if(pmount_prefix){
      if(0 == strlen(pmount_prefix) || '/' != pmount_prefix[0]){
        S3FS_PRN_EXIT("path(%s) must be prefix \"/\".", pmount_prefix);
        return -1;
      }
      mount_prefix = pmount_prefix;
      // remove trailing slash
      if(mount_prefix.at(mount_prefix.size() - 1) == '/'){
        mount_prefix = mount_prefix.substr(0, mount_prefix.size() - 1);
      }
    }
  }else{
    bucket = arg;
  }
  return 0;
}


// This is repeatedly called by the fuse option parser
// if the key is equal to FUSE_OPT_KEY_OPT, it's an option passed in prefixed by 
// '-' or '--' e.g.: -f -d -ousecache=/tmp
//
// if the key is equal to FUSE_OPT_KEY_NONOPT, it's either the bucket name 
//  or the mountpoint. The bucket name will always come before the mountpoint
static int my_fuse_opt_proc(void* data, const char* arg, int key, struct fuse_args* outargs)
{
  int ret;
  if(key == FUSE_OPT_KEY_NONOPT){
    // the first NONOPT option is the bucket name
    if(bucket.size() == 0){
      if ((ret = set_bucket(arg))){
        return ret;
      }
      
      return 0;
    }
    else if (!strcmp(arg, "s3fs")) {
      return 0;
    }

    // the second NONPOT option is the mountpoint(not utility mode)
    if(0 == mountpoint.size() && 0 == utility_mode){
      // save the mountpoint and do some basic error checking
      mountpoint = arg;
      struct stat stbuf;

      if(stat(arg, &stbuf) == -1){
        S3FS_PRN_EXIT("unable to access MOUNTPOINT %s: %s", mountpoint.c_str(), strerror(errno));
        return -1;
      }
      if(!(S_ISDIR(stbuf.st_mode))){
        S3FS_PRN_EXIT("MOUNTPOINT: %s is not a directory.", mountpoint.c_str());
        return -1;
      }
      if(!set_mountpoint_attribute(stbuf)){
        S3FS_PRN_EXIT("MOUNTPOINT: %s permission denied.", mountpoint.c_str());
        return -1;
      }

      if(!nonempty){
        struct dirent *ent;
        DIR *dp = opendir(mountpoint.c_str());
        if(dp == NULL){
          S3FS_PRN_EXIT("failed to open MOUNTPOINT: %s: %s", mountpoint.c_str(), strerror(errno));
          return -1;
        }
        while((ent = readdir(dp)) != NULL){
          if(strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0){
            closedir(dp);
            S3FS_PRN_EXIT("MOUNTPOINT directory %s is not empty. if you are sure this is safe, can use the 'nonempty' mount option.", mountpoint.c_str());
            return -1;
          }
        }
        closedir(dp);
      }
      return 1;
    }

    // Unknown option
    if(0 == utility_mode){
      S3FS_PRN_EXIT("specified unknown third optioni(%s).", arg);
    }else{
      S3FS_PRN_EXIT("specified unknown second optioni(%s). you don't need to specify second option(mountpoint) for utility mode(-u).", arg);
    }
    return -1;

  }else if(key == FUSE_OPT_KEY_OPT){
    if(0 == STR2NCMP(arg, "uid=")){
      s3fs_uid = get_uid(strchr(arg, '=') + sizeof(char));
      if(0 != geteuid() && 0 == s3fs_uid){
        S3FS_PRN_EXIT("root user can only specify uid=0.");
        return -1;
      }
      is_s3fs_uid = true;
      return 1; // continue for fuse option
    }
    if(0 == STR2NCMP(arg, "gid=")){
      s3fs_gid = get_gid(strchr(arg, '=') + sizeof(char));
      if(0 != getegid() && 0 == s3fs_gid){
        S3FS_PRN_EXIT("root user can only specify gid=0.");
        return -1;
      }
      is_s3fs_gid = true;
      return 1; // continue for fuse option
    }
    if(0 == STR2NCMP(arg, "umask=")){
      s3fs_umask = strtol(strchr(arg, '=') + sizeof(char), NULL, 0);
      s3fs_umask &= (S_IRWXU | S_IRWXG | S_IRWXO);
      is_s3fs_umask = true;
      return 1; // continue for fuse option
    }
    if(0 == strcmp(arg, "allow_other")){
      allow_other = true;
      return 1; // continue for fuse option
    }
    if(0 == STR2NCMP(arg, "mp_umask=")){
      mp_umask = strtol(strchr(arg, '=') + sizeof(char), NULL, 0);
      mp_umask &= (S_IRWXU | S_IRWXG | S_IRWXO);
      is_mp_umask = true;
      return 0;
    }
    if(0 == STR2NCMP(arg, "default_acl=")){
      const char* acl = strchr(arg, '=') + sizeof(char);
      S3fsCurl::SetDefaultAcl(acl);
      return 0;
    }
    if(0 == STR2NCMP(arg, "retries=")){
      S3fsCurl::SetRetries(static_cast<int>(s3fs_strtoofft(strchr(arg, '=') + sizeof(char))));
      return 0;
    }
    if(0 == STR2NCMP(arg, "use_cache=")){
      string cacheDir = rebuild_path(strchr(arg, '=') + sizeof(char));
      FdManager::SetCacheDir(cacheDir.c_str());
      return 0;
    }
    if(0 == STR2NCMP(arg, "cachedb_dir=")){
      string cacheDir = rebuild_path(strchr(arg, '=') + sizeof(char));
      S3DB::Instance().setDir(cacheDir.c_str());
      return 0;
    }
    if(0 == STR2NCMP(arg, "cachepage_dir=")){
      string cacheDir = rebuild_path(strchr(arg, '=') + sizeof(char));
      FdManager::SetCachePageDir(cacheDir.c_str());
      return 0;
    }
    if(0 == STR2NCMP(arg, "check_cache_dir_exist")){
      FdManager::SetCheckCacheDirExist(true);
      return 0;
    }
    if(0 == strcmp(arg, "del_cache")){
      is_remove_cache = true;
      return 0;
    }
    if (0 == strcmp(arg, "no_check_bucket")) {
        check_bucket = false;
        return 0;
    }
    if(0 == STR2NCMP(arg, "multireq_max=")){
      long maxreq = static_cast<long>(s3fs_strtoofft(strchr(arg, '=') + sizeof(char)));
      S3fsMultiCurl::SetMaxMultiRequest(maxreq);
      return 0;
    }
    if(0 == strcmp(arg, "nonempty")){
      nonempty = true;
      return 1; // need to continue for fuse.
    }
    if(0 == strcmp(arg, "nomultipart")){
      nomultipart = true;
      return 0;
    }
    // old format for storage_class
    if(0 == strcmp(arg, "use_rrs") || 0 == STR2NCMP(arg, "use_rrs=")){
      off_t rrs = 1;
      // for an old format.
      if(0 == STR2NCMP(arg, "use_rrs=")){
        rrs = s3fs_strtoofft(strchr(arg, '=') + sizeof(char));
      }
      if(0 == rrs){
        S3fsCurl::SetStorageClass(STANDARD);
      }else if(1 == rrs){
        S3fsCurl::SetStorageClass(REDUCED_REDUNDANCY);
      }else{
        S3FS_PRN_EXIT("poorly formed argument to option: use_rrs");
        return -1;
      }
      return 0;
    }
    if(0 == STR2NCMP(arg, "storage_class=")){
      const char *storage_class = strchr(arg, '=') + sizeof(char);
      if(0 == strcmp(storage_class, "standard")){
        S3fsCurl::SetStorageClass(STANDARD);
      }else if(0 == strcmp(storage_class, "standard_ia")){
        S3fsCurl::SetStorageClass(STANDARD_IA);
      }else if(0 == strcmp(storage_class, "reduced_redundancy")){
        S3fsCurl::SetStorageClass(REDUCED_REDUNDANCY);
      }else{
        S3FS_PRN_EXIT("unknown value for storage_class: %s", storage_class);
        return -1;
      }
      return 0;
    }
    //
    // [NOTE]
    // use_sse                        Set Server Side Encrypting type to SSE-S3
    // use_sse=1
    // use_sse=file                   Set Server Side Encrypting type to Custom key(SSE-C) and load custom keys
    // use_sse=custom(c):file
    // use_sse=custom(c)              Set Server Side Encrypting type to Custom key(SSE-C)
    // use_sse=kmsid(k):kms-key-id    Set Server Side Encrypting type to AWS Key Management key id(SSE-KMS) and load KMS id
    // use_sse=kmsid(k)               Set Server Side Encrypting type to AWS Key Management key id(SSE-KMS)
    //
    // load_sse_c=file                Load Server Side Encrypting custom keys
    //
    // AWSSSECKEYS                    Loading Environment for Server Side Encrypting custom keys
    // AWSSSEKMSID                    Loading Environment for Server Side Encrypting Key id
    //
    if(0 == STR2NCMP(arg, "use_sse")){
      if(0 == strcmp(arg, "use_sse") || 0 == strcmp(arg, "use_sse=1")){ // use_sse=1 is old type parameter
        // sse type is SSE_S3
        if(!S3fsCurl::IsSseDisable() && !S3fsCurl::IsSseS3Type()){
          S3FS_PRN_EXIT("already set SSE another type, so conflict use_sse option or environment.");
          return -1;
        }
        S3fsCurl::SetSseType(SSE_S3);

      }else if(0 == strcmp(arg, "use_sse=kmsid") || 0 == strcmp(arg, "use_sse=k")){
        // sse type is SSE_KMS with out kmsid(expecting id is loaded by environment)
        if(!S3fsCurl::IsSseDisable() && !S3fsCurl::IsSseKmsType()){
          S3FS_PRN_EXIT("already set SSE another type, so conflict use_sse option or environment.");
          return -1;
        }
        if(!S3fsCurl::IsSetSseKmsId()){
          S3FS_PRN_EXIT("use_sse=kms but not loaded kms id by environment.");
          return -1;
        }
        S3fsCurl::SetSseType(SSE_KMS);

      }else if(0 == STR2NCMP(arg, "use_sse=kmsid:") || 0 == STR2NCMP(arg, "use_sse=k:")){
        // sse type is SSE_KMS with kmsid
        if(!S3fsCurl::IsSseDisable() && !S3fsCurl::IsSseKmsType()){
          S3FS_PRN_EXIT("already set SSE another type, so conflict use_sse option or environment.");
          return -1;
        }
        const char* kmsid;
        if(0 == STR2NCMP(arg, "use_sse=kmsid:")){
          kmsid = &arg[strlen("use_sse=kmsid:")];
        }else{
          kmsid = &arg[strlen("use_sse=k:")];
        }
        if(!S3fsCurl::SetSseKmsid(kmsid)){
          S3FS_PRN_EXIT("failed to load use_sse kms id.");
          return -1;
        }
        S3fsCurl::SetSseType(SSE_KMS);

      }else if(0 == strcmp(arg, "use_sse=custom") || 0 == strcmp(arg, "use_sse=c")){
        // sse type is SSE_C with out custom keys(expecting keys are loaded by environment or load_sse_c option)
        if(!S3fsCurl::IsSseDisable() && !S3fsCurl::IsSseCType()){
          S3FS_PRN_EXIT("already set SSE another type, so conflict use_sse option or environment.");
          return -1;
        }
        // [NOTE]
        // do not check ckeys exists here.
        //
        S3fsCurl::SetSseType(SSE_C);

      }else if(0 == STR2NCMP(arg, "use_sse=custom:") || 0 == STR2NCMP(arg, "use_sse=c:")){
        // sse type is SSE_C with custom keys
        if(!S3fsCurl::IsSseDisable() && !S3fsCurl::IsSseCType()){
          S3FS_PRN_EXIT("already set SSE another type, so conflict use_sse option or environment.");
          return -1;
        }
        const char* ssecfile;
        if(0 == STR2NCMP(arg, "use_sse=custom:")){
          ssecfile = &arg[strlen("use_sse=custom:")];
        }else{
          ssecfile = &arg[strlen("use_sse=c:")];
        }
        if(!S3fsCurl::SetSseCKeys(ssecfile)){
          S3FS_PRN_EXIT("failed to load use_sse custom key file(%s).", ssecfile);
          return -1;
        }
        S3fsCurl::SetSseType(SSE_C);

      }else if(0 == strcmp(arg, "use_sse=")){    // this type is old style(parameter is custom key file path)
        // SSE_C with custom keys.
        const char* ssecfile = &arg[strlen("use_sse=")];
        if(!S3fsCurl::SetSseCKeys(ssecfile)){
          S3FS_PRN_EXIT("failed to load use_sse custom key file(%s).", ssecfile);
          return -1;
        }
        S3fsCurl::SetSseType(SSE_C);

      }else{
        // never come here.
        S3FS_PRN_EXIT("something wrong use_sse option.");
        return -1;
      }
      return 0;
    }
    // [NOTE]
    // Do only load SSE custom keys, care for set without set sse type.
    if(0 == STR2NCMP(arg, "load_sse_c=")){
      const char* ssecfile = &arg[strlen("load_sse_c=")];
      if(!S3fsCurl::SetSseCKeys(ssecfile)){
        S3FS_PRN_EXIT("failed to load use_sse custom key file(%s).", ssecfile);
        return -1;
      }
      return 0;
    }
    if(0 == STR2NCMP(arg, "ssl_verify_hostname=")){
      long sslvh = static_cast<long>(s3fs_strtoofft(strchr(arg, '=') + sizeof(char)));
      if(-1 == S3fsCurl::SetSslVerifyHostname(sslvh)){
        S3FS_PRN_EXIT("poorly formed argument to option: ssl_verify_hostname.");
        return -1;
      }
      return 0;
    }
    if(0 == STR2NCMP(arg, "passwd_file=")){
      passwd_file = strchr(arg, '=') + sizeof(char);
      return 0;
    }
    if(0 == strcmp(arg, "ibm_iam_auth")){
      S3fsCurl::SetIsIBMIAMAuth(true);
      S3fsCurl::SetIAMCredentialsURL("https://iam.bluemix.net/oidc/token");
      S3fsCurl::SetIAMTokenField("access_token");
      S3fsCurl::SetIAMExpiryField("expiration");
      S3fsCurl::SetIAMFieldCount(2);
      is_ibm_iam_auth = true;
      return 0;
    }
    if(0 == STR2NCMP(arg, "ibm_iam_endpoint=")){
      std::string endpoint_url = "";
      std::string iam_endpoint = strchr(arg, '=') + sizeof(char);
      // Check url for http / https protocol string
      if((iam_endpoint.compare(0, 8, "https://") != 0) && (iam_endpoint.compare(0, 7, "http://") != 0)) {
         S3FS_PRN_EXIT("option ibm_iam_endpoint has invalid format, missing http / https protocol");
         return -1;
      }
      endpoint_url = iam_endpoint + "/oidc/token";
      S3fsCurl::SetIAMCredentialsURL(endpoint_url.c_str());
      return 0;
    }
    if(0 == strcmp(arg, "ecs")){
      if (is_ibm_iam_auth) {
        S3FS_PRN_EXIT("option ecs cannot be used in conjunction with ibm");
        return -1;
      }
      S3fsCurl::SetIsECS(true);
      S3fsCurl::SetIAMCredentialsURL("http://169.254.170.2");
      S3fsCurl::SetIAMFieldCount(5);
      is_ecs = true;
      return 0;
    }
    if(0 == STR2NCMP(arg, "iam_role")){
      if (is_ecs || is_ibm_iam_auth) {
        S3FS_PRN_EXIT("option iam_role cannot be used in conjunction with ecs or ibm");
        return -1;
      }
      if(0 == strcmp(arg, "iam_role") || 0 == strcmp(arg, "iam_role=auto")){
        // loading IAM role name in s3fs_init(), because we need to wait initializing curl.
        //
        load_iamrole = true;
        return 0;

      }else if(0 == STR2NCMP(arg, "iam_role=")){
        const char* role = strchr(arg, '=') + sizeof(char);
        S3fsCurl::SetIAMRole(role);
        load_iamrole = false;
        return 0;
      }
    }
    if(0 == STR2NCMP(arg, "public_bucket=")){
      off_t pubbucket = s3fs_strtoofft(strchr(arg, '=') + sizeof(char));
      if(1 == pubbucket){
        S3fsCurl::SetPublicBucket(true);
        // [NOTE]
        // if bucket is public(without credential), s3 do not allow copy api.
        // so s3fs sets nocopyapi mode.
        //
        nocopyapi = true;
      }else if(0 == pubbucket){
        S3fsCurl::SetPublicBucket(false);
      }else{
        S3FS_PRN_EXIT("poorly formed argument to option: public_bucket.");
        return -1;
      }
      return 0;
    }
    if(0 == STR2NCMP(arg, "bucket=")){
      std::string bname = strchr(arg, '=') + sizeof(char);
      if ((ret = set_bucket(bname.c_str()))){
        return ret;
      }
      return 0;
    }
    if(0 == STR2NCMP(arg, "host=")){
      host = strchr(arg, '=') + sizeof(char);
      return 0;
    }
    if(0 == STR2NCMP(arg, "servicepath=")){
      service_path = strchr(arg, '=') + sizeof(char);
      return 0;
    }
    if(0 == strcmp(arg, "no_check_certificate")){
        S3fsCurl::SetCheckCertificate(false);
        return 0;
    }
    if(0 == STR2NCMP(arg, "connect_timeout=")){
      long contimeout = static_cast<long>(s3fs_strtoofft(strchr(arg, '=') + sizeof(char)));
      S3fsCurl::SetConnectTimeout(contimeout);
      return 0;
    }
    if(0 == STR2NCMP(arg, "readwrite_timeout=")){
      time_t rwtimeout = static_cast<time_t>(s3fs_strtoofft(strchr(arg, '=') + sizeof(char)));
      S3fsCurl::SetReadwriteTimeout(rwtimeout);
      return 0;
    }
    if(0 == strcmp(arg, "list_object_max_keys")){
      int max_keys = static_cast<int>(s3fs_strtoofft(strchr(arg, '=') + sizeof(char)));
      if(max_keys < 1000){
        S3FS_PRN_EXIT("argument should be over 1000: list_object_max_keys");
        return -1;
      }
      max_keys_list_object = max_keys;
      return 0;
    }
    if(0 == STR2NCMP(arg, "max_stat_cache_size=")){
      unsigned long cache_size = static_cast<unsigned long>(s3fs_strtoofft(strchr(arg, '=') + sizeof(char)));
      StatCache::getStatCacheData()->SetCacheSize(cache_size);
      return 0;
    }
    if(0 == STR2NCMP(arg, "stat_cache_expire=")){
      time_t expr_time = static_cast<time_t>(s3fs_strtoofft(strchr(arg, '=') + sizeof(char)));
      StatCache::getStatCacheData()->SetExpireTime(expr_time);
      return 0;
    }
    // [NOTE]
    // This option is for compatibility old version.
    if(0 == STR2NCMP(arg, "stat_cache_interval_expire=")){
      time_t expr_time = static_cast<time_t>(s3fs_strtoofft(strchr(arg, '=') + sizeof(char)));
      StatCache::getStatCacheData()->SetExpireTime(expr_time, true);
      return 0;
    }
    if(0 == strcmp(arg, "enable_noobj_cache")){
      StatCache::getStatCacheData()->EnableCacheNoObject();
      return 0;
    }
    if(0 == strcmp(arg, "nodnscache")){
      S3fsCurl::SetDnsCache(false);
      return 0;
    }
    if(0 == strcmp(arg, "nosscache")){
      S3fsCurl::SetSslSessionCache(false);
      return 0;
    }
    if(0 == STR2NCMP(arg, "parallel_count=") || 0 == STR2NCMP(arg, "parallel_upload=")){
      int maxpara = static_cast<int>(s3fs_strtoofft(strchr(arg, '=') + sizeof(char)));
      if(0 >= maxpara){
        S3FS_PRN_EXIT("argument should be over 1: parallel_count");
        return -1;
      }
      S3fsCurl::SetMaxParallelCount(maxpara);
      return 0;
    }
    if(0 == STR2NCMP(arg, "fd_page_size=")){
      S3FS_PRN_ERR("option fd_page_size is no longer supported, so skip this option.");
      return 0;
    }
    if(0 == STR2NCMP(arg, "multipart_size=")){
      off_t size = static_cast<off_t>(s3fs_strtoofft(strchr(arg, '=') + sizeof(char)));
      if(!S3fsCurl::SetMultipartSize(size)){
        S3FS_PRN_EXIT("multipart_size option must be at least 5 MB.");
        return -1;
      }
      return 0;
    }
    if(0 == STR2NCMP(arg, "ensure_diskfree=")){
      size_t dfsize = static_cast<size_t>(s3fs_strtoofft(strchr(arg, '=') + sizeof(char))) * 1024 * 1024;
      if(dfsize < static_cast<size_t>(S3fsCurl::GetMultipartSize())){
        S3FS_PRN_WARN("specified size to ensure disk free space is smaller than multipart size, so set multipart size to it.");
        dfsize = static_cast<size_t>(S3fsCurl::GetMultipartSize());
      }
      FdManager::SetEnsureFreeDiskSpace(dfsize);
      return 0;
    }
    if(0 == STR2NCMP(arg, "singlepart_copy_limit=")){
      singlepart_copy_limit = static_cast<int64_t>(s3fs_strtoofft(strchr(arg, '=') + sizeof(char))) * 1024;
      return 0;
    }
    if(0 == STR2NCMP(arg, "ahbe_conf=")){
      string ahbe_conf = strchr(arg, '=') + sizeof(char);
      if(!AdditionalHeader::get()->Load(ahbe_conf.c_str())){
        S3FS_PRN_EXIT("failed to load ahbe_conf file(%s).", ahbe_conf.c_str());
        return -1;
      }
      AdditionalHeader::get()->Dump();
      return 0;
    }
    if(0 == strcmp(arg, "noxmlns")){
      noxmlns = true;
      return 0;
    }
    if(0 == strcmp(arg, "nocopyapi")){
      nocopyapi = true;
      return 0;
    }
    if(0 == strcmp(arg, "norenameapi")){
      norenameapi = true;
      return 0;
    }
    if(0 == strcmp(arg, "complement_stat")){
      complement_stat = true;
      return 0;
    }
    if(0 == strcmp(arg, "notsup_compat_dir")){
      support_compat_dir = false;
      return 0;
    }
    if(0 == strcmp(arg, "enable_content_md5")){
      S3fsCurl::SetContentMd5(true);
      return 0;
    }
    if(0 == STR2NCMP(arg, "url=")){
      host = strchr(arg, '=') + sizeof(char);
      // strip the trailing '/', if any, off the end of the host
      // string
      size_t found, length;
      found  = host.find_last_of('/');
      length = host.length();
      while(found == (length - 1) && length > 0){
         host.erase(found);
         found  = host.find_last_of('/');
         length = host.length();
      }
      // Check url for http / https protocol string
      if((host.compare(0, 8, "https://") != 0) && (host.compare(0, 7, "http://") != 0)) {
         S3FS_PRN_EXIT("option url has invalid format, missing http / https protocol");
         return -1;
      }
      return 0;
    }
    if(0 == strcmp(arg, "sigv2")){
      S3fsCurl::SetSignatureV4(false);
      return 0;
    }
    if(0 == strcmp(arg, "createbucket")){
      create_bucket = true;
      return 0;
    }
    if(0 == STR2NCMP(arg, "endpoint=")){
      endpoint              = strchr(arg, '=') + sizeof(char);
      is_specified_endpoint = true;
      return 0;
    }
    if(0 == strcmp(arg, "use_path_request_style")){
      pathrequeststyle = true;
      return 0;
    }
    if(0 == STR2NCMP(arg, "noua")){
      S3fsCurl::SetUserAgentFlag(false);
      return 0;
    }
    if(0 == strcmp(arg, "use_xattr")){
      is_use_xattr = true;
      return 0;
    }else if(0 == STR2NCMP(arg, "use_xattr=")){
      const char* strflag = strchr(arg, '=') + sizeof(char);
      if(0 == strcmp(strflag, "1")){
        is_use_xattr = true;
      }else if(0 == strcmp(strflag, "0")){
        is_use_xattr = false;
      }else{
        S3FS_PRN_EXIT("option use_xattr has unknown parameter(%s).", strflag);
        return -1;
      }
      return 0;
    }
    if(0 == STR2NCMP(arg, "cipher_suites=")){
      cipher_suites = strchr(arg, '=') + sizeof(char);
      return 0;
    }
    if(0 == STR2NCMP(arg, "instance_name=")){
      instance_name = strchr(arg, '=') + sizeof(char);
      instance_name = "[" + instance_name + "]";
      return 0;
    }
    //
    // debug option for s3fs
    //
    if(0 == STR2NCMP(arg, "dbglevel=")){
      const char* strlevel = strchr(arg, '=') + sizeof(char);
      if(0 == strcasecmp(strlevel, "silent") || 0 == strcasecmp(strlevel, "critical") || 0 == strcasecmp(strlevel, "crit")){
        set_s3fs_log_level(S3FS_LOG_CRIT);
      }else if(0 == strcasecmp(strlevel, "error") || 0 == strcasecmp(strlevel, "err")){
        set_s3fs_log_level(S3FS_LOG_ERR);
      }else if(0 == strcasecmp(strlevel, "wan") || 0 == strcasecmp(strlevel, "warn") || 0 == strcasecmp(strlevel, "warning")){
        set_s3fs_log_level(S3FS_LOG_WARN);
      }else if(0 == strcasecmp(strlevel, "inf") || 0 == strcasecmp(strlevel, "info") || 0 == strcasecmp(strlevel, "information")){
        set_s3fs_log_level(S3FS_LOG_INFO);
      }else if(0 == strcasecmp(strlevel, "dbg") || 0 == strcasecmp(strlevel, "debug")){
        set_s3fs_log_level(S3FS_LOG_DBG);
      }else{
        S3FS_PRN_EXIT("option dbglevel has unknown parameter(%s).", strlevel);
        return -1;
      }
      return 0;
    }
    //
    // debug option
    //
    // debug_level is S3FS_LOG_INFO, after second -d is passed to fuse.
    //
    if(0 == strcmp(arg, "-d") || 0 == strcmp(arg, "--debug")){
      if(!IS_S3FS_LOG_INFO() && !IS_S3FS_LOG_DBG()){
        set_s3fs_log_level(S3FS_LOG_INFO);
        return 0;
      }
      if(0 == strcmp(arg, "--debug")){
        // fuse doesn't understand "--debug", but it understands -d.
        // but we can't pass -d back to fuse.
        return 0;
      }
    }
    // "f2" is not used no more.
    // (set S3FS_LOG_DBG)
    if(0 == strcmp(arg, "f2")){
      set_s3fs_log_level(S3FS_LOG_DBG);
      return 0;
    }
    if(0 == strcmp(arg, "curldbg")){
      S3fsCurl::SetVerbose(true);
      return 0;
    }

    if(0 == STR2NCMP(arg, "accessKeyId=")){
      S3FS_PRN_EXIT("option accessKeyId is no longer supported.");
      return -1;
    }
    if(0 == STR2NCMP(arg, "secretAccessKey=")){
      S3FS_PRN_EXIT("option secretAccessKey is no longer supported.");
      return -1;
    }
  }
  return 1;
}

int main(int argc, char* argv[])
{
  int ch;
  int fuse_res;
  int option_index = 0; 
  struct fuse_operations s3fs_oper;

  static const struct option long_opts[] = {
    {"help",    no_argument, NULL, 'h'},
    {"version", no_argument, 0,     0},
    {"debug",   no_argument, NULL, 'd'},
    {0, 0, 0, 0}
  };

  // init syslog(default CRIT)
  openlog("s3fs", LOG_PID | LOG_ODELAY | LOG_NOWAIT, LOG_USER);
  set_s3fs_log_level(debug_level);

  // init xml2
  xmlInitParser();
  LIBXML_TEST_VERSION

  // get program name - emulate basename
  program_name.assign(argv[0]);
  size_t found = program_name.find_last_of("/");
  if(found != string::npos){
    program_name.replace(0, found+1, "");
  }

  while((ch = getopt_long(argc, argv, "dho:fsu", long_opts, &option_index)) != -1){
    switch(ch){
    case 0:
      if(strcmp(long_opts[option_index].name, "version") == 0){
        show_version();
        exit(EXIT_SUCCESS);
      }
      break;
    case 'h':
      show_help();
      exit(EXIT_SUCCESS);
    case 'o':
      break;
    case 'd':
      break;
    case 'f':
      foreground = true;
      break;
    case 's':
      break;
    case 'u':
      utility_mode = 1;
      break;
    default:
      exit(EXIT_FAILURE);
    }
  }

  // Load SSE environment
  if(!S3fsCurl::LoadEnvSse()){
    S3FS_PRN_EXIT("something wrong about SSE environment.");
    exit(EXIT_FAILURE);
  }

  // ssl init
  if(!s3fs_init_global_ssl()){
    S3FS_PRN_EXIT("could not initialize for ssl libraries.");
    exit(EXIT_FAILURE);
  }

  // init curl
  if(!S3fsCurl::InitS3fsCurl("/etc/mime.types")){
    S3FS_PRN_EXIT("Could not initiate curl library.");
    s3fs_destroy_global_ssl();
    exit(EXIT_FAILURE);
  }

  // clear this structure
  memset(&s3fs_oper, 0, sizeof(s3fs_oper));

  // This is the fuse-style parser for the arguments
  // after which the bucket name and mountpoint names
  // should have been set
  struct fuse_args custom_args = FUSE_ARGS_INIT(argc, argv);
  if(0 != fuse_opt_parse(&custom_args, NULL, NULL, my_fuse_opt_proc)){
    S3fsCurl::DestroyS3fsCurl();
    s3fs_destroy_global_ssl();
    exit(EXIT_FAILURE);
  }

  // [NOTE]
  // exclusive option check here.
  //
  if(REDUCED_REDUNDANCY == S3fsCurl::GetStorageClass() && !S3fsCurl::IsSseDisable()){
    S3FS_PRN_EXIT("use_sse option could not be specified with storage class reduced_redundancy.");
    S3fsCurl::DestroyS3fsCurl();
    s3fs_destroy_global_ssl();
    exit(EXIT_FAILURE);
  }
  if(!S3fsCurl::FinalCheckSse()){
    S3FS_PRN_EXIT("something wrong about SSE options.");
    S3fsCurl::DestroyS3fsCurl();
    s3fs_destroy_global_ssl();
    exit(EXIT_FAILURE);
  }

  // The first plain argument is the bucket
  if(bucket.size() == 0){
    S3FS_PRN_EXIT("missing BUCKET argument.");
    show_usage();
    S3fsCurl::DestroyS3fsCurl();
    s3fs_destroy_global_ssl();
    exit(EXIT_FAILURE);
  }

  // bucket names cannot contain upper case characters in virtual-hosted style
  if((!pathrequeststyle) && (lower(bucket) != bucket)){
    S3FS_PRN_EXIT("BUCKET %s, name not compatible with virtual-hosted style.", bucket.c_str());
    S3fsCurl::DestroyS3fsCurl();
    s3fs_destroy_global_ssl();
    exit(EXIT_FAILURE);
  }

  // check bucket name for illegal characters
  found = bucket.find_first_of("/:\\;!@#$%^&*?|+=");
  if(found != string::npos){
    S3FS_PRN_EXIT("BUCKET %s -- bucket name contains an illegal character.", bucket.c_str());
    S3fsCurl::DestroyS3fsCurl();
    s3fs_destroy_global_ssl();
    exit(EXIT_FAILURE);
  }

  // The second plain argument is the mountpoint
  // if the option was given, we all ready checked for a
  // readable, non-empty directory, this checks determines
  // if the mountpoint option was ever supplied
  if(utility_mode == 0){
    if(mountpoint.size() == 0){
      S3FS_PRN_EXIT("missing MOUNTPOINT argument.");
      show_usage();
      S3fsCurl::DestroyS3fsCurl();
      s3fs_destroy_global_ssl();
      exit(EXIT_FAILURE);
    }
  }

  // error checking of command line arguments for compatibility
  if(S3fsCurl::IsPublicBucket() && S3fsCurl::IsSetAccessKeys()){
    S3FS_PRN_EXIT("specifying both public_bucket and the access keys options is invalid.");
    S3fsCurl::DestroyS3fsCurl();
    s3fs_destroy_global_ssl();
    exit(EXIT_FAILURE);
  }
  if(passwd_file.size() > 0 && S3fsCurl::IsSetAccessKeys()){
    S3FS_PRN_EXIT("specifying both passwd_file and the access keys options is invalid.");
    S3fsCurl::DestroyS3fsCurl();
    s3fs_destroy_global_ssl();
    exit(EXIT_FAILURE);
  }
  if(!S3fsCurl::IsPublicBucket() && !load_iamrole && !is_ecs){
    if(EXIT_SUCCESS != get_access_keys()){
      S3fsCurl::DestroyS3fsCurl();
      s3fs_destroy_global_ssl();
      exit(EXIT_FAILURE);
    }
    if(!S3fsCurl::IsSetAccessKeys()){
      S3FS_PRN_EXIT("could not establish security credentials, check documentation.");
      S3fsCurl::DestroyS3fsCurl();
      s3fs_destroy_global_ssl();
      exit(EXIT_FAILURE);
    }
    // More error checking on the access key pair can be done
    // like checking for appropriate lengths and characters  
  }

  // check cache dir permission
  if(!FdManager::CheckCacheDirExist() || !FdManager::CheckCacheTopDir() || !CacheFileStat::CheckCacheFileStatTopDir()){
    S3FS_PRN_EXIT("could not allow cache directory permission, check permission of cache directories.");
    S3fsCurl::DestroyS3fsCurl();
    s3fs_destroy_global_ssl();
    exit(EXIT_FAILURE);
  }

  // check IBM IAM requirements
  if(is_ibm_iam_auth){

    // check that default ACL is either public-read or private
    string defaultACL = S3fsCurl::GetDefaultAcl();
    if(defaultACL == "private"){
      // IBM's COS default ACL is private
      // set acl as empty string to avoid sending x-amz-acl header
      S3fsCurl::SetDefaultAcl("");
    }else if(defaultACL != "public-read"){
      S3FS_PRN_EXIT("can only use 'public-read' or 'private' ACL while using ibm_iam_auth");
      S3fsCurl::DestroyS3fsCurl();
      s3fs_destroy_global_ssl();
      exit(EXIT_FAILURE);
    }

    if(create_bucket && !S3fsCurl::IsSetAccessKeyID()){
      S3FS_PRN_EXIT("missing service instance ID for bucket creation");
      S3fsCurl::DestroyS3fsCurl();
      s3fs_destroy_global_ssl();
      exit(EXIT_FAILURE);
    }
  }

  // set user agent
  S3fsCurl::InitUserAgent();

  // There's room for more command line error checking

  // Check to see if the bucket name contains periods and https (SSL) is
  // being used. This is a known limitation:
  // http://docs.amazonwebservices.com/AmazonS3/latest/dev/
  // The Developers Guide suggests that either use HTTP of for us to write
  // our own certificate verification logic.
  // For now, this will be unsupported unless we get a request for it to
  // be supported. In that case, we have a couple of options:
  // - implement a command line option that bypasses the verify host 
  //   but doesn't bypass verifying the certificate
  // - write our own host verification (this might be complex)
  // See issue #128strncasecmp
  /* 
  if(1 == S3fsCurl::GetSslVerifyHostname()){
    found = bucket.find_first_of(".");
    if(found != string::npos){
      found = host.find("https:");
      if(found != string::npos){
        S3FS_PRN_EXIT("Using https and a bucket name with periods is unsupported.");
        exit(1);
      }
    }
  }
  */

  if(utility_mode){
    int exitcode = s3fs_utility_mode();

    S3fsCurl::DestroyS3fsCurl();
    s3fs_destroy_global_ssl();
    exit(exitcode);
  }

  // check free disk space
  if(!FdManager::IsSafeDiskSpace(NULL, S3fsCurl::GetMultipartSize() * S3fsCurl::GetMaxParallelCount())){
    S3FS_PRN_EXIT("There is no enough disk space for used as cache(or temporary) directory by s3fs.");
    S3fsCurl::DestroyS3fsCurl();
    s3fs_destroy_global_ssl();
    exit(EXIT_FAILURE);
  }
  
  S3FS_PRN_INIT_INFO("init v%s(commit:%s) with %s", VERSION, COMMIT_HASH_VAL, s3fs_crypt_lib_name());

  if(is_remove_cache && (!CacheFileStat::DeleteCacheFileStatDirectory() || !FdManager::DeleteCacheDirectory())){
    S3FS_PRN_DBG("Could not initialize cache directory.");
  }

  if (create_bucket) {
    int result = do_create_bucket();
    if (result) {
        S3FS_PRN_DBG("do_create_bucket failed");
        S3fsCurl::DestroyS3fsCurl();
        s3fs_destroy_global_ssl();
        exit(EXIT_FAILURE);
    }
  }

  if (check_bucket) {
        // Check Bucket
    if (EXIT_SUCCESS != s3fs_check_service()) {
        S3FS_PRN_DBG("s3fs_check_service failed");
        S3fsCurl::DestroyS3fsCurl();
        s3fs_destroy_global_ssl();
        exit(EXIT_FAILURE);
    }
  }

  s3fs_oper.getattr   = s3fs_getattr;
  s3fs_oper.readlink  = s3fs_readlink;
  s3fs_oper.mknod     = s3fs_mknod;
  s3fs_oper.mkdir     = s3fs_mkdir;
  s3fs_oper.unlink    = s3fs_unlink;
  s3fs_oper.rmdir     = s3fs_rmdir;
  s3fs_oper.symlink   = s3fs_symlink;
  s3fs_oper.rename    = s3fs_rename;
  s3fs_oper.link      = s3fs_link;
  if(!nocopyapi){
    s3fs_oper.chmod   = s3fs_chmod;
    s3fs_oper.chown   = s3fs_chown;
    s3fs_oper.utimens = s3fs_utimens;
  }else{
    s3fs_oper.chmod   = s3fs_chmod_nocopy;
    s3fs_oper.chown   = s3fs_chown_nocopy;
    s3fs_oper.utimens = s3fs_utimens_nocopy;
  }
  s3fs_oper.truncate  = s3fs_truncate;
  s3fs_oper.open      = s3fs_open;
  s3fs_oper.read      = s3fs_read;
  s3fs_oper.write     = s3fs_write;
  s3fs_oper.statfs    = s3fs_statfs;
  s3fs_oper.flush     = s3fs_flush;
  s3fs_oper.fsync     = s3fs_fsync;
  s3fs_oper.release   = s3fs_release;
  s3fs_oper.opendir   = s3fs_opendir;
  s3fs_oper.readdir   = s3fs_readdir;
  s3fs_oper.init      = s3fs_init;
  s3fs_oper.destroy   = s3fs_destroy;
  s3fs_oper.access    = s3fs_access;
  s3fs_oper.create    = s3fs_create;
  // extended attributes
  if(is_use_xattr){
    s3fs_oper.setxattr    = s3fs_setxattr;
    s3fs_oper.getxattr    = s3fs_getxattr;
    s3fs_oper.listxattr   = s3fs_listxattr;
    s3fs_oper.removexattr = s3fs_removexattr;
  }

  // set signal handler for debugging
  if(!set_s3fs_usr2_handler()){
    S3FS_PRN_EXIT("could not set signal handler for SIGUSR2.");
    S3fsCurl::DestroyS3fsCurl();
    s3fs_destroy_global_ssl();
    exit(EXIT_FAILURE);
  }
  
  // now passing things off to fuse, fuse will finish evaluating the command line args
  fuse_res = fuse_main(custom_args.argc, custom_args.argv, &s3fs_oper, NULL);
  fuse_opt_free_args(&custom_args);

  
  // Destroy curl
  if(!S3fsCurl::DestroyS3fsCurl()){
    S3FS_PRN_WARN("Could not release curl library.");
  }
  s3fs_destroy_global_ssl();

  // cleanup xml2
  xmlCleanupParser();
  S3FS_MALLOCTRIM(0);

  exit(fuse_res);
}

/*
* Local variables:
* tab-width: 4
* c-basic-offset: 4
* End:
* vim600: noet sw=4 ts=4 fdm=marker
* vim<600: noet sw=4 ts=4
*/
