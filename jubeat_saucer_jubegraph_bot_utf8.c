/*
This software is released under the  BSD (3-Clause) License, see LICENSE.txt.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <unistd.h>
#include <time.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "version.h"

#define FILE_MAX 6
#define BUF_LEN 256                      /* バッファのサイズ */
#define BOT_UA "User-Agent: Opera/9.80 (Windows NT 6.1; U; ja) Presto/2.10.289 Version/12.02"
#define BOT_UA2 "User-Agent: rearnbot/0.9 (+https://twitter.com/rearn499)"
#define print_line( msg ) fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,msg)

const char *ea_user = "KID=username&pass=passwd&OTP=";	// ※ユーザ名とパスワードを指定
const char *name[] = {"playerData", "musicData1", "musicData2", "musicData3", "musicData4", "musicData5",};
const char *filename[] = {"0.html", "1.html", "2.html", "3.html", "4.html", "5.html",};
const char *form_date[] = {"Content-Disposition: form-data; name=\"", "\"; filename=\"", "\"\r\nContent-Type: text/html\r\n\r\n"};


int make_server(const char *host, struct sockaddr_in *server, const char *protocol_name, int port) {
	struct hostent *servhost;
	struct servent *service;
	
	servhost = gethostbyname(host);
    if(servhost == NULL) {
        fprintf(stderr, "[%s] から IP アドレスへの変換に失敗しました。\n", host);
        return 1;
    }

    bzero((char *)server, sizeof(*server));
	
    server->sin_family = AF_INET;

    bcopy(servhost->h_addr, &(server->sin_addr), servhost->h_length);

	service = getservbyname(protocol_name, "tcp");
	if ( service != NULL ){
		server->sin_port = service->s_port;
	}
	else {
		server->sin_port = htons(port);
	}
	return 0;
}

int connect_server(FILE **fp, struct sockaddr_in *server) {
	int s;
	if((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		print_line("ソケットの生成に失敗しました。");
		return -1;
	}
	
	if(connect(s, (struct sockaddr *)server, sizeof(*server)) == -1) {
		print_line("connect に失敗しました。");
		return -1;
	}
	
	if((*fp = fdopen(s, "r+")) == NULL) {
		print_line("fdopen に失敗しました。");
		return -1;
	}
	
	setvbuf(*fp, NULL, _IONBF, 0);
	return s;
}

int connect_ssl_server(struct sockaddr_in *server, SSL *ssl) {
	int s;
	int ret;
	if((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		print_line("ソケットの生成に失敗しました。");
		return -1;
	}

	if(connect(s, (struct sockaddr *)server, sizeof(*server)) == -1) {
		print_line("connect に失敗しました。");
		return -1;
	}
	
	/* ここからが SSL */
	
	ret = SSL_set_fd(ssl, s);
	if(ret == 0) {
		ERR_print_errors_fp(stderr);
		return -1;
	}

	/* PRNG 初期化 */
	RAND_poll();
	while(RAND_status() == 0) {
		unsigned short rand_ret = rand() % 65536;
		RAND_seed(&rand_ret, sizeof(rand_ret));
	}

	/* SSL で接続 */
	ret = SSL_connect(ssl);
	if(ret != 1) {
		ERR_print_errors_fp(stderr);
		return -1;
	}
	return s;
}
int ssl_put_and_get(int s, SSL *ssl, char *request, FILE *tmp_fp) {
	int ret;
	int read_size;
	char buf[BUF_LEN];
	ret = SSL_write(ssl, request, strlen(request));
	if(ret < 1) {
		ERR_print_errors_fp(stderr);
		exit(1);
	}
	
	while(1) {
		read_size = SSL_read(ssl, buf, sizeof(buf)-1);
		if(read_size > 0) {
			fwrite(buf, sizeof(char), read_size, tmp_fp);
		}
		else if(read_size == 0) {
			break;
		}
		else {
			ERR_print_errors_fp(stderr);
			exit(1);
		}
	}
	
	ret = SSL_shutdown(ssl); 
	if ( ret != 1 ){
		ERR_print_errors_fp(stderr);
		exit(1);
	}
	close(s);
	if(fseek(tmp_fp, 0, SEEK_SET) != 0 ){
		print_line("fseek");
		exit(1);
	}
	return 0;
}

int ssl_a(char *cookie) {
	int s;
	struct sockaddr_in server;
	char *content_length, *content_length2, *content_length3;
	int kkk = 0;
	int cc, lc;
	char buf[BUF_LEN];
	

	SSL *ssl;
	SSL_CTX *ctx;

	char request[BUF_LEN];

	char *host = "p.eagate.573.jp";
	char *origin_path = "/gate/p/login.html";
	char path[BUF_LEN];
	FILE *tmp_fp;

	make_server(host, &server, "https", 443);
	strcpy(path, origin_path);
	SSL_load_error_strings();
	SSL_library_init();
	ctx = SSL_CTX_new(SSLv23_client_method());
	if(ctx == NULL) {
		ERR_print_errors_fp(stderr);
		exit(1);
	}

	ssl = SSL_new(ctx);
	if(ssl == NULL) {
		ERR_print_errors_fp(stderr);
		exit(1);
	}
	
	s = connect_ssl_server(&server, ssl);
	sprintf(request, 
"HEAD %s HTTP/1.0\r\n"
"Host: %s\r\n"
"%s\r\n"
"\r\n",
	path, host, BOT_UA);
	
	if((tmp_fp = tmpfile()) == NULL) {
		print_line("tmpfile");
		exit(1);
	}
	
	ssl_put_and_get(s, ssl, request, tmp_fp);
	
	while(1) {
		if(fgets(buf, sizeof(buf), tmp_fp) == NULL) {
			print_line("");
			exit(1);
		}
		fputs(buf, stdout);
		if(buf[0] == '\r' && buf[1] == '\n') {
			print_line("Cookie not found");
			exit(1);
		}
		if((content_length = strstr(buf, "Set-Cookie: ")) != '\0') {
			content_length += strlen("Set-Cookie: ");
			content_length2 = strstr(content_length, ";");
			*content_length2 = '\0';
			strcpy(cookie, content_length);
			break;
		}
	}
	
	fclose(tmp_fp);
	
	s = connect_ssl_server(&server, ssl);
	sprintf(request, 
"POST %s HTTP/1.0\r\n"
"Host: %s\r\n"
"%s\r\n"
"Cookie: %s\r\n"
"Content-Type: application/x-www-form-urlencoded\r\n"
"Content-Length: %d\r\n"
"\r\n"
"%s\r\n",
		  path, host, BOT_UA, cookie, strlen(ea_user), ea_user);
	
	if((tmp_fp = tmpfile()) == NULL) {
		print_line("tmpfile");
		exit(1);
	}
	
	ssl_put_and_get(s, ssl, request, tmp_fp);
	
	if(fgets(buf, sizeof(buf), tmp_fp) == NULL) {
		print_line("");
		exit(1);
		//return 1;
	}
	
	fputs(buf, stdout);
	while(buf[9] == '3' && buf[10] == '0' && buf[11] == '2') {
		cc = 1;
		lc = 1;
		while(cc | lc) {
			if(fgets(buf, sizeof(buf), tmp_fp) == NULL) {
				print_line("");
				exit(1);
			}
			fputs(buf, stdout);
			if(buf[0] == '\r' && buf[1] == '\n') {
				print_line("Location not found");
				return 1;
			}
			if((content_length = strstr(buf, "Location: ")) != '\0') {
				content_length += strlen("Location: ");
				if((content_length3 = strchr(content_length, ':')) != '\0') {
					content_length3 += 3; /* :の後は//なのでそこを飛ばす */
				}
				content_length = strchr(content_length3, '/');
				content_length2 = strstr(content_length, "\r\n");
				*content_length2 = '\0';
				strcpy(path, content_length);
				lc = 0;
			}
			if((content_length = strstr(buf, "Set-Cookie: ")) != '\0') {
				content_length += strlen("Set-Cookie: ");
				content_length2 = strstr(content_length, ";");
				*content_length2 = '\0';
				strcpy(cookie, content_length);
				cc = 0;
			}
		}
		fclose(tmp_fp);
		
		if((strstr(path, "login_complete.html")) != '\0') {
			kkk = 1;
			break;
		}
		s = connect_ssl_server(&server, ssl);
		
		sprintf(request, 
	"GET %s HTTP/1.0\r\n"
	"Host: %s\r\n"
	"%s\r\n"
	"Cookie: %s\r\n"
	"\r\n",
			  path, host, BOT_UA, cookie);
		if((tmp_fp = tmpfile()) == NULL) {
			print_line("tmpfile");
			exit(1);
		}
		
		fprintf(stdout, "認証サーバからのレスポンス\n");
		
		ssl_put_and_get(s, ssl, request, tmp_fp);

		if(fgets(buf, sizeof(buf), tmp_fp) == NULL) {
			print_line("");
			exit(1);
			//return 1;
		}
		
		fputs(buf, stdout);
	}
	
	if(kkk) {
		fprintf(stdout, "\n\nCookieを取得\n%s\n", cookie);
	}
	else {
		while(fgets(buf, sizeof(buf), tmp_fp) != NULL) {
			fputs(buf, stdout);
		}
		fclose(tmp_fp);
		print_line("認証失敗");
		exit(1);
	}
	
	SSL_free(ssl); 
	SSL_CTX_free(ctx);
	ERR_free_strings();
	return kkk;
}

int te(FILE *fp, char *host, char *pass, char *file_buf[]) {
	int i;
	size_t len = 0;
	time_t t = time(NULL);
	char t_str[17];
	
	sprintf(t_str, "%016u", (unsigned int)t);
	for(i = 0; i < FILE_MAX; i++) {
		len += strlen(file_buf[i]);
		len += strlen(name[i]);
		len += strlen(filename[i]);
	}
	len += ((4 + 2 + strlen(form_date[0]) + strlen(form_date[1]) + strlen(form_date[2])) * FILE_MAX);
	/* 4は、"--" + "\r\n" 2は、file終了後の改行*/
	
	len += 4;
	len += (16 * (FILE_MAX + 1));
	
	fprintf(fp,
"POST %s HTTP/1.0\r\n"
"Host: %s\r\n"
"%s\r\n"
"Content-Type: multipart/form-data; boundary=%s\r\n"
"Content-Length: %d\r\n"
"\r\n"
	, pass, host, BOT_UA2, t_str, len);
	
	for(i = 0; i < FILE_MAX; i++) {
		fprintf(fp,
"--%s\r\n"
"%s%s%s%s%s"
"%s\r\n"
		, t_str, form_date[0], name[i], form_date[1], filename[i], form_date[2], file_buf[i]);
		
	}
	
	fprintf(fp, "--%s\r\n" , t_str);
	
	return 0;
}

char *percent_encoding(const char *in, int in_len, char *out, int out_len, int type_bool) {
	int i;
	int j = 0;
	char c[16] = {
		'0', '1', '2', '3',
		'4', '5', '6', '7',
		'8', '9', 'a', 'b',
		'c', 'd', 'e', 'f',
	};
	unsigned char tmp1, tmp2;
	
	if(type_bool) {
		for(i = 0; i < in_len; i++) {
			switch((unsigned char)in[i]) {
#ifdef __GNUC__
				case 'A' ... 'Z':
				case 'a' ... 'z':
				case '0' ... '9':
#endif
				case '-':
				case '.':
				case '_':
				case '~':
				
				case ':':
				case '/':
				case '?':
				case '#':
				case '[':
				case ']':
				case '@':
				case '!':
				case '$':
				case '&':
				case '\'':
				case '(':
				case ')':
				case '*':
				case '+':
				case ',':
				case ';':
				case '=':
					if(out_len > j + 1) {
						out[j++] = in[i];
					} else {
						*out = '\0';
						return NULL;
					}
					break;
				default:
#ifndef __GNUC__
					if(('A' <= in[i] && in[i] <= 'Z') || ('a' <= in[i] && in[i] <= 'z') || ('0' <= in[i] && in[i] <= '9')) {
						if(out_len > j + 1) {
							out[j++] = in[i];
						} else {
							*out = '\0';
							return NULL;
						}
					} else
#endif
					{
						if(out_len > j + 3) {
							out[j++] = '%';
							out[j++] = c[(unsigned char)in[i] / 16];
							out[j++] = c[(unsigned char)in[i] % 16];
						} else {
							*out = '\0';
							return NULL;
						}
					}
			}
		}
	} else {
		for(i = 0; i < in_len; i++) {
			if(out_len > j + 1) {
				if(in[i] != '%') {
					out[j++] = in[i];
				} else {
					i++;
					if('A' <= in[i] && in[i] <= 'F') {
						tmp1 = in[i] - 'A' + 10;
					} else if('a' <= in[i] && in[i] <= 'f') {
						tmp1 = in[i] - 'a' + 10;
					} else if('0' <= in[i] && in[i] <= '9') {
						tmp1 = in[i] - '0';
					} else {
						*out = '\0';
						return NULL;
					}
					i++;
					if('A' <= in[i] && in[i] <= 'F') {
						tmp2 = in[i] - 'A' + 10;
					} else if('a' <= in[i] && in[i] <= 'f') {
						tmp2 = in[i] - 'a' + 10;
					} else if('0' <= in[i] && in[i] <= '9') {
						tmp2 = in[i] - '0';
					} else {
						*out = '\0';
						return NULL;
					}
					out[j++] = tmp1 * 16 + tmp2;
				}
			} else {
				*out = '\0';
				return NULL;
			}
		}
	}
	out[j] = '\0';
	return out;
}

int main(int argc, char *argv[]) {
	int i, j;
    int s;
    struct sockaddr_in server;
    FILE *fp;
    char buf[BUF_LEN];
	char *file_buf[FILE_MAX];
	char *content_length, *content_length2, *content_length3, *content_length4;
	size_t content_length_size[FILE_MAX];
	int content_length_bool = 0;
	FILE *tmp_fp;
	char *kakuninn;
	int kakuninn_len;
	char cookie[1024];
	int cookie_bool = 0;

    char host[BUF_LEN] = "p.eagate.573.jp";
	
    char *path[] = {
		"/game/jubeat/saucer/p/playdata/index.html",
		"/game/jubeat/saucer/p/playdata/music.html?page=1",
		"/game/jubeat/saucer/p/playdata/music.html?page=2",
		"/game/jubeat/saucer/p/playdata/music.html?page=3",
		"/game/jubeat/saucer/p/playdata/music.html?page=4",
		"/game/jubeat/saucer/p/playdata/music.html?page=5",
	};
	char up_host[BUF_LEN] = "jubegraph.dyndns.org";
	char up_path[BUF_LEN] = "/jubeat_saucer/registFile.cgi";
	
#ifdef JSJB_NON_GIT
	fprintf(stderr, "jsjb (%s) build: %s %s\n", JSJB_VERSION, __DATE__, __TIME__);
#else
	fprintf(stderr, "jsjb (%s %s) build: %s %s\n", JSJB_VERSION, JSJB_BRANCH, __DATE__, __TIME__);
#endif

	// コマンドライン引数を解析しているが、あまり詳しく調べていない
	if(argc > 1) {
		if(strstr(argv[1], "-v") || strstr(argv[1], "--version")) {
			exit(0);
		}
	}
	fputc('\n', stderr);

	ssl_a(cookie);
	
	make_server(host, &server, "http", 80);
	
	for(i = 0; i < FILE_MAX; i++) {
		s = connect_server(&fp, &server);
		if(s < 0) {
			print_line("connect_server error");
			return 1;
		}
		content_length_bool = 0;
		fprintf(fp, "GET %s HTTP/1.0\r\n", path[i]);
		fprintf(fp, "Host: %s\r\n", host);
		fprintf(fp, "%s\r\n", BOT_UA);
		fprintf(fp, "Accept: text/html\r\n");
		fprintf(fp, "Accept-Language: ja\r\n");
		fprintf(fp, "Cookie: %s\r\n", cookie);
		fprintf(fp, "\r\n");
		cookie_bool = 1;
		fprintf(stdout, "http://%s%s(%s)を取得\n", host, path[i], name[i]);
		while (1){
			if(fgets(buf, sizeof(buf), fp) == NULL ){
				break;
			}
			if((content_length = strstr(buf, "Content-Length:")) != '\0') {
				content_length += strlen("Content-Length:");
				content_length_size[i] = strtol(content_length, NULL, 0) + 1;
				file_buf[i] = (char *)calloc(content_length_size[i], sizeof(char));
				content_length_bool = 1;
			} else if(strstr(buf, cookie) != '\0') {
				cookie_bool = 0;
			} else if(buf[0] == '\r' && buf[1] == '\n') {
				if(content_length_bool) {
					if(fgets(file_buf[i], content_length_size[i], fp) == NULL ){
						print_line("");
						exit(1);
					}
				}
				
			}
//			fprintf(stdout, "%s", buf);
			fputs(buf, stdout);
		}
		if(cookie_bool) {
			print_line("");
			exit(1);
		}
		fclose(fp);
		close(s);
		
		sleep(15);
		
/*		while (1){
			if(fgets(buf, sizeof(buf), fp) == NULL ){
				break;
			}
			fprintf(stdout, "%s", buf);
		}
		
*/
	}
	
	// ここまで、受信
	for(i = 0; i < FILE_MAX; i++) {
/*		for(j = 100; j < 110; j++) {
			fprintf(stdout, "%c", file_buf[i][j]);
		}*/
//		fprintf(stdout, "\n");
		fprintf(stdout, "file: %d, size: %d\n", i, content_length_size[i]);
//		fprintf(stdout, "\nsize: %d\n", content_length_size[i]);
	}
	// ここから、送信
	make_server(up_host, &server, "http", 80);
	s = connect_server(&fp, &server);
	if(s < 0) {
		print_line("connect_server error");
		return 1;
	}
	
	te(fp, up_host, up_path, file_buf);
	while(1) {
		if(fgets(buf, sizeof(buf), fp) == NULL) {
			break;
		}
		if(buf[0] == '\r' && buf[1] == '\n') break;
//		fprintf(stdout, "%s", buf);
		fputs(buf, stdout);
	}
	
	if((tmp_fp = tmpfile()) == NULL) {
		print_line("tmpfile");
		exit(1);
	}
	
	while(1) {
		if(fgets(buf, sizeof(buf), fp) == NULL) {
			break;
		}
//		fprintf(tmp_fp, "%s", buf);
		fputs(buf, tmp_fp);
	}
	fclose(fp);
	close(s);

	kakuninn_len = ftell(tmp_fp);
	
	
	if(fseek(tmp_fp, 0, SEEK_SET) != 0 ){
		print_line("fseek");
		exit(1);
	}
	if((kakuninn = calloc(kakuninn_len, sizeof(char))) == NULL) {
		print_line("calloc");
		exit(1);
	}
	if(fread(kakuninn, sizeof(char), kakuninn_len, tmp_fp) != kakuninn_len ){
		print_line("fread");
		exit(1);
	}
	fclose(tmp_fp);
//	fprintf(stderr, "%s", kakuninn);
//	fputs(kakuninn, stderr);	// あまりにも長いので表示をやめる
	
#ifdef LIBXML_VALID_ENABLED
// ここに書こう
#else
// --ここから、きめつけが過ぎる
	content_length = strstr(kakuninn, "name=\"time\" value=\"");
	content_length += strlen("name=\"time\" value=\"");
	// input type="hidden" name="time" value="
	// ここでcontent_lengthから、"検索してそこまでを\0にしてpost
	content_length2 = strstr(content_length, "\"");
	*content_length2 = '\0';	// 移植性が悪い
	fprintf(stdout, "%s\n", content_length);
	content_length3 = strstr(kakuninn, "name=\"rid\" value=\"");
	content_length3 += strlen("name=\"rid\" value=\"");
	content_length4 = strstr(content_length3, "\"");
	*content_length4 = '\0';
	fprintf(stdout, "%s\n", content_length3);
	j = strlen(content_length) + strlen(content_length3) + strlen("rid") + strlen("time") + strlen("submit=%e3%83%87%e3%83%bc%e3%82%bf%e7%99%bb%e9%8c%b2") + 6;
// --ここまで
#endif

	s = connect_server(&fp, &server);
	if(s < 0) {
		print_line("connect_server error");
		return 1;
	}

	fprintf(fp,
"POST /jubeat_saucer/registData.cgi HTTP/1.0\r\n"
"Host: %s\r\n"
"%s\r\n"
"Content-Type: application/x-www-form-urlencoded\r\n"
"Referer: http://jubegraph.dyndns.org/jubeat_saucer/registFile.cgi\r\n"
"Content-Length: %d\r\n"
"\r\n"
"rid=%s&time=%s&submit=%%E3%%83%%87%%E3%%83%%BC%%E3%%82%%BF%%E7%%99%%BB%%E9%%8C%%B2\r\n",
	up_host, BOT_UA2, j, content_length3, content_length);
	while(1) {
		if(fgets(buf, sizeof(buf), fp) == NULL ){
			break;
		}
//		fprintf(stderr, "%s", buf);
		fputs(buf, stderr);
	}
	fclose(fp);
	close(s);
    return 0;
}

