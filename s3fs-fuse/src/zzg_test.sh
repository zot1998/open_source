#!/bin/bash
g++ s3fs_db.cpp test_s3fs_db.cpp -l sqlite3 -o test_s3fs_db; 
[ $? -ne 0 ] && exit 0

chmod 777 ./test_s3fs_db; 
[ $? -ne 0 ] && exit 0
./test_s3fs_db


