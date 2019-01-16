#include <stdio.h>
#include <limits>
#include <stdint.h>
#include <string>
#include <string.h>
#include "s3fs_db.h"
#include "test_util.h"

void test_trim()
{
  
}

void test_base64()
{

}

int main(int argc, char *argv[])
{
    int rc = 0;
    
    rc = S3DB::Instance().init("./test/sql.db");	
    ASSERT_EQUALS(rc, 0);

    #if 1
    S3DB::Instance().insertDB("/root",   1, 1);
    S3DB::Instance().insertDB("/root/2", 2, 2);
    S3DB::Instance().insertDB("/root/3", 3, 3);
    S3DB::Instance().insertDB("/root/4", 4, 4);
    S3DB::Instance().insertDB("/root/3", 5, 5);
    S3DB::Instance().insertDB("/root/3", 6, 6);
    #endif
    S3DB::Instance().insertDB("/root/ฮารว", 6, 6);

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

    
    

    

    
    
  return 0;
}

