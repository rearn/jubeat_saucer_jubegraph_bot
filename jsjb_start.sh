#!/bin/sh


./jsjb > ./jsjb.stdout 2> ./jsjb.stderr
/bin/cat ./jsjb.stderr | mail -s "jubegraph (`date -d "1 day ago" +"%Y-%m-%d"`)" mail@example.com
