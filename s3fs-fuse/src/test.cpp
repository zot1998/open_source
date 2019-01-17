#include <stdio.h>
#include <stdlib.h>
#include <limits>
#include <stdint.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sys/file.h>
#include <errno.h>

using namespace std;

int main(int argn, char *argv[]) {
    int i = 0;
    char v = 0;
    int wsize = 0;

    int fd = open(argv[1], O_CREAT|O_RDWR|O_TRUNC);
    if (fd < 0)
    {
        return -1;
    }
    
    for ( i = 0; i < atoi(argv[2]); i++ )
    {
        if(-1 == (wsize = pwrite(fd, &v, 1, i)))
        {
            printf("pwrite failed. errno(%d)", errno);
            close(fd);  
            return -errno;
        }
        
        v++;

        if (i % (4096 * 10) == 0) {
            fsync(fd);

            pwrite(fd, "haha", 4, i-8192);
        }
    }
    fsync(fd);
    close(fd);  


    return 0;
}






