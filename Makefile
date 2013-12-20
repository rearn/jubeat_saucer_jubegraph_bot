jsjb: jubeat_saucer_jubegraph_bot_utf8.c version.h
	gcc -o jsjb jubeat_saucer_jubegraph_bot_utf8.c -lssl -lcrypto -Wall
	
.PHONY: version.h
version.h:
	revh="#define JSJB_VERSION \"`git describe --always 2> /dev/null`\"\n#define JSJB_BRANCH  \"`git branch -a 2> /dev/null | grep "^*" | tr -d '\* '`\"\n" ; \
	if [ -n "$$revh" ] ; then \
		echo -e "#ifndef VERSION_H\n#define VERSION_H\n" > $@; \
		echo -e "$$revh" >> $@; \
		echo -e "#endif /* #ifndef VERSION_H */\n" >> $@; \
	else \
		cat version.h.txt > $@; \
	fi
