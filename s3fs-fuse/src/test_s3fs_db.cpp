#include <stdio.h>
#include <stdlib.h>
#include <limits>
#include <stdint.h>
#include <string>
#include <string.h>
#include <unistd.h>

#include "s3fs_db.h"
#include "s3fs_rsync.h"

#include "test_util.h"

void test_trim()
{
  
}

void test_base64()
{

}

int add(const char *pFile, mode_t mode) {
    int rc = rand()%2;
    printf("---- add ----: %s, %d\n",pFile, rc);

    return rc;
}
int del(const char *pFile, mode_t mode) {
    int rc = rand()%2;
    printf("---- del ----: %s, %d\n",pFile, rc);

    return rc;
}


int main(int argc, char *argv[])
{
    int rc = 0;

    printf("=======================:%d\n",sizeof(long long int));

    S3RSync::Instance().setBucket("test");
    S3RSync::Instance().setCacheDir("./");

    S3RSync::Instance().setSyncAddFunc(add);
    S3RSync::Instance().setSyncDelFunc(del);

    rc = S3RSync::Instance().init();
    ASSERT_EQUALS(rc, 0);


    S3DB_INFO_S info;	 
    #if 1    
    info.reset();info.strFile = "/root"; info.nOperator = S3DB_OP_ADD;info.nStatus = S3DB_STATUS_INIT;
    S3DB::Instance().insertDB(info);
    info.reset();info.strFile = "/root/2"; info.nOperator = S3DB_OP_DEL;info.nStatus = S3DB_STATUS_INIT;
    S3DB::Instance().insertDB(info);
    info.reset();info.strFile = "/root/3"; info.nOperator = S3DB_OP_ADD;info.nStatus = S3DB_STATUS_INIT;
    S3DB::Instance().insertDB(info);
    info.reset();info.strFile = "/root/4"; info.nOperator = S3DB_OP_ADD;info.nStatus = S3DB_STATUS_INIT;
    S3DB::Instance().insertDB(info);
    info.reset();info.strFile = "/root/3"; info.nOperator = S3DB_OP_DEL;info.nStatus = S3DB_STATUS_INIT;
    S3DB::Instance().insertDB(info);
    info.reset();info.strFile = "/root/3"; info.nOperator = S3DB_OP_ADD;info.nStatus = S3DB_STATUS_INIT;
    S3DB::Instance().insertDB(info);
    #endif

    S3DB_LIST_S m;	
    S3DB::Instance().queryAheadDB(m);		
    S3DB_LIST_S::iterator it = m.begin();	
    for ( ; it != m.end(); it++) 
    {       
        printf(" %s, len:%d #",(*it).strFile.c_str(), strlen((*it).strFile.c_str()));
        printf(" %lld, #",(*it).n64Id);     
        printf(" %d, #",(*it).nOperator);        
        printf(" %d\n",(*it).nStatus);      
    }

    
    
    
    usleep(30 * 1000 * 1000);

    
    
  return 0;
}

