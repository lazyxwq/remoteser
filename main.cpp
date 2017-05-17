#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <event.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "inc.hpp"
#include "httpd.hpp"
#include <time.h>
#include "cJSON.h"
#include <vector>
#include <string>
#include <list>
#include <queue>
#include <map>
#include <regex.h>

using namespace std;

#define PORT        25341
#define BACKLOG     5
#define MEM_SIZE    1024

#define	INFO\( InfoLog\(g,__FILE__,__LINE__,
#define	WARN\( WarnLog\(g,__FILE__,__LINE__,
#define	DEBUG\( DebugLog\(g,__FILE__,__LINE__,
#define	ERR\( ErrorLog(g,__FILE__,__LINE__,

struct event_base* base;
static LOG *g;

//extern int httpd();

//http
const char* http200 = "HTTP/1.0 200 OK\r\n"
		"Server: wz simple httpd 1.0\r\n"
		"Content-Type: text/html\r\n"
		"\r\n";

const char* http400 = "HTTP/1.0 400 BAD REQUEST\r\n"
		"Server: wz simple httpd 1.0\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		"<p>你的请求有问题,请检查语法!</p>\r\n";

const char* http404 = "HTTP/1.0 404 NOT FOUND\r\n"
		"Server: wz simple httpd 1.0\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		"<html>"
		"<head><title>你请求的界面被查水表了!</title></head>\r\n"
		"<body><p>404: 估计是回不来了</p></body>"
		"</html>";

const char* http501 = "HTTP/1.0 501 Method Not Implemented\r\n"
		"Server: wz simple httpd 1.0\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		"<html>"
		"<head><title>小伙子不要乱请求</title></head>\r\n"
		"<body><p>too young too simple, 年轻人别总想弄出个大新闻.</p></body>"
		"</html>";

const char* http500 = "HTTP/1.0 500 Internal Server Error\r\n"
		"Server: wz simple httpd 1.0\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		"<html>"
		"<head><title>Sorry </title></head>\r\n"
		"<body><p>最近有点方了!</p></body>"
		"</html>";

struct sock_ev {
	struct event* read_ev;
	struct event* write_ev;
	char* buffer;
};

typedef struct _tunnel {
	char mac[50] = "\0";	//mac地址
	int port;		//开启的ssh端口号
	time_t time;	//下一次检测ssh通道存活时间
	/*
	 _tunnel()
	 {
	 bzero(mac, 50);
	 port = 0;
	 time = 0;
	 }
	 */
	/*
	 bool operator <(const _tunnel& rhs) const {
	 return memcmp(mac, mac, 20);
	 }
	 */
} TUNNEL;

std::map<const string, TUNNEL*> tun;
std::map<const char[100], TUNNEL*> tun1;

int regex(char *pattern, char *bematch) {
	char errbuf[1024];
	char match[100];
	regex_t reg;
	int err, nm = 10;
	regmatch_t pmatch[nm];
	InfoLog(g, __FILE__, __LINE__, "pattern=%s\n", pattern);
	if (regcomp(&reg, pattern, REG_EXTENDED) < 0) {
		regerror(err, &reg, errbuf, sizeof(errbuf));
		InfoLog(g, __FILE__, __LINE__, "err:%s\n", errbuf);
	}

	err = regexec(&reg, bematch, nm, pmatch, 0);

	if (err == REG_NOMATCH) {
		InfoLog(g, __FILE__, __LINE__, "no match\n");
		exit(-1);
	} else if (err) {
		regerror(err, &reg, errbuf, sizeof(errbuf));
		InfoLog(g, __FILE__, __LINE__, "err:%s\n", errbuf);
		exit(-1);
	}

	for (int i = 0; i < 10 && pmatch[i].rm_so != -1; i++) {
		int len = pmatch[i].rm_eo - pmatch[i].rm_so;
		if (len) {
			memset(match, '\0', sizeof(match));
			memcpy(match, bematch + pmatch[i].rm_so, len);
			InfoLog(g, __FILE__, __LINE__, "%s\n", match);
		}
	}
	return 0;
}

//产生指定范围内的随机数
int Random(int m, int n) {
	int pos, dis;
	if (m == n) {
		return m;
	} else if (m > n) {
		pos = n;
		dis = m - n + 1;
		return rand() % dis + pos;
	} else {
		pos = m;
		dis = n - m + 1;
		return rand() % dis + pos;
	}
}

/*
 函数名：getFileSizeSystemCall(char * strFileName)
 功能：获取指定文件的大小
 参数：
 strFileName (char *)：文件名
 返回值：
 size (int)：文件大小
 */
int getFileSizeSystemCall(char * strFileName) {
	struct stat temp;
	stat(strFileName, &temp);
	return temp.st_size;
}

//执行shell一条命令并返回命令结果
//p1:要执行的shell命令 p2:命令结果存放缓冲区 p2:缓冲区长度
//返回值: -1表示popen打开失败 其他为命令结果长度
static int get_system_pipe(char *cmd, char *data, int len) {
	//DebugLog(g, __FILE__, __LINE__, "exec command by pipe:\n%s", cmd);
	//InfoLog(g, __FILE__, __LINE__, "exec command by pipe:%s\n", cmd);
	int m_len = 0;
	//构造(cmd) &>out.txt，将标准输出和标准错误重定向到文件中
	FILE *pp = popen(cmd, "r");
	//DebugLog(g, __FILE__, __LINE__, "get_system_pipe cmd %s(%d)", cmd, strlen(cmd));
	memset(data, 0, len);
	if (!pp) {
		ErrorLog(g, __FILE__, __LINE__, "pp error %s");
		return -1;
	}
	while (!feof(pp)) {
		if (fgets(data + m_len, len - m_len, pp) != NULL) {
			m_len = strlen(data);
		} else {
			break;
		}
	}
	//去掉最后的换行符
	if (data[strlen(data) - 1] == '\n')
		data[strlen(data) - 1] = ' ';
	//DebugLog(g, __FILE__, __LINE__, "get_system_pipe %s (%d)", data, strlen(data));
	//InfoLog(g, __FILE__, __LINE__, "get_system_pipe %s (%d)\n", data, strlen(data));
	pclose(pp);
	if (!strcmp(data, "uci: Entry not found ")) {
		InfoLog(g, __FILE__, __LINE__, "[%s:%d]:%s = %s\n", __FUNCTION__,
				__LINE__, cmd, data);
		strcpy(data, "");
	}
	return strlen(data);
}

//执行shell一条命令并返回命令结果
//p1:要执行的shell命令 p2:命令结果存放缓冲区 p2:缓冲区长度
//返回值: -1表示system执行失败 其他为命令结果长度
static int get_system_cmd(char *cmd, char *data, int len) {
	//DebugLog(g, __FILE__, __LINE__, "enter %s", __FUNCTION__);
	//log(LOG_DEBUG, "exec command by cmd:\n%s", cmd);
	InfoLog(g, __FILE__, __LINE__, "exec command by cmd:\n%s\n", cmd);
	int m_len = 0;
	//构造(cmd) &>out.txt，将标准输出和标准错误重定向到文件中
	FILE *pp = NULL;
	int filesize = 0;
	char cmds[1000];

	//memset(data, 0, len);
	bzero(data, len);
	sprintf(cmds, "(%s) > /tmp/cmds 2&>1", cmd);
	if (system(cmds) < 0) {
		//ErrorLog(g, __FILE__, __LINE__,  "system has faild, errno = %d", errno);
	}
	filesize = getFileSizeSystemCall("/tmp/cmds");
	//InfoLog(g, __FILE__, __LINE__, "exec system func ok, cmds result size = %d\n", filesize);
	pp = fopen("/tmp/cmds", "r");
	if (pp == NULL) {
		//ErrorLog(g, __FILE__, __LINE__,  "system open out file faild, errno = %d", errno);
	}

	if (len < filesize) {
		//sprintf(data, "result buffer size(%d) smaller than cmd result size(%d)!!!");
		//ErrorLog(g, __FILE__, __LINE__, "result buffer size(%d) smaller than cmd result size(%d)!!!",len, filesize);
		return -10;
	} else
		fread(data, sizeof(char), filesize, pp);
	//InfoLog(g, __FILE__, __LINE__, "cmds(%d):%s\n", strlen(data), data);
	//去掉最后的换行符
	if (data[strlen(data) - 1] == '\n')
		data[strlen(data) - 1] = '\0';
	//DebugLog(g, __FILE__, __LINE__, "get_system_cmd %s (%d)", data, strlen(data));
	//pclose(pp);
	fclose(pp);

	if (!strcmp(data, "uci: Entry not found")) {
		//InfoLog(g, __FILE__, __LINE__, "[%s:%d]:%s = %s\n", __FUNCTION__, __LINE__, cmd, data);
		strcpy(data, "");
	}
	return strlen(data);
}

/*
 */
void release_sock_event(struct sock_ev* ev) {
	event_del(ev->read_ev);
	free(ev->read_ev);
	free(ev->write_ev);
	free(ev->buffer);
	free(ev);
}

char mode[200] = {
		"{\"mac\":\"%s\", \"stoken\":\"%s\", \"action\":\"%s\", \"port\":%d}" };

char action[200] = { "{\"mac\":%s, \"stoken\":%s, \"action\":%s, \"port\":%s}" };

//获得一个字段的值
//p1:json字符串 p2:要查询字段名 p3:查询到的字段值存放缓冲区 p4:缓冲区大小
//返回值:值存在则返回值长度，值不存在返回0
static int get_json_item(char *recvdata, char *name, char *out, int len) {
	char *json_data = recvdata;
	cJSON *oem = NULL;
	cJSON *item = NULL;
	int getlen = 0;
	//InfoLog(g, __FILE__, __LINE__, "get_json_item\n");
	oem = cJSON_Parse(json_data);
	//InfoLog(g, __FILE__, __LINE__, "cJSON_Parse\n");
	if (oem != NULL) {
		item = cJSON_GetObjectItem(oem, name);
		if (item != NULL && item->type == cJSON_String) {
			getlen = strlen(item->valuestring);
			if (getlen < len) {
				strcpy(out, item->valuestring);
				//DebugLog(g, __FILE__, __LINE__, " item %s = %s", name, out);
			} else {
				//ErrorLog(g, __FILE__, __LINE__,	"recv get %s  len %d , exceed %d .", name, getlen, len);
			}
		} else {
			ErrorLog(g, __FILE__, __LINE__, "recv can not get %s string ",
					name);
		}
	}
	cJSON_Delete(oem);
	//cJSON_Delete(item);
	return getlen;
}

//得到json数组中的一个项的值
//p1:要处理的json数据 p2:项所在数组名 p3:项在数组的索引 p4:项名称 p5:存放值的缓冲区 p6:缓冲区长度
//返回值 0表示未搜索到 非零表示值的长度
static int get_json_array_item(char *recvdata, char *arrayname, int index,
		char *name, char *out, int len) {
	char *json_data = recvdata;
	cJSON *oem = NULL;
	cJSON *item = NULL;
	cJSON *arrayitem = NULL;
	cJSON *array = NULL;
	int getlen = 0;
	oem = cJSON_Parse(json_data);
	if (oem != NULL) {
		array = cJSON_GetObjectItem(oem, arrayname);
		if (array != NULL && array->type == cJSON_Array) {
			arrayitem = cJSON_GetArrayItem(array, index);
			if (arrayitem != NULL) {
				item = cJSON_GetObjectItem(arrayitem, name);
				if (item != NULL && item->type == cJSON_String) {
					//	DebugLog(g, __FILE__, __LINE__,"array item  %s", item->valuestring);
					getlen = strlen(item->valuestring);
					if (getlen < len) {
						strcpy(out, item->valuestring);
					} else {
						ErrorLog(g, __FILE__, __LINE__,
								"%s get %s  len %d , exceed %d .", __FUNCTION__,
								name, getlen, len);
					}
				} else {
					ErrorLog(g, __FILE__, __LINE__,
							"%s can not get arrayitem %s string ", __FUNCTION__,
							name);
				}
			} else {
				ErrorLog(g, __FILE__, __LINE__,
						"%s can not get %s arrayitem index %d ", __FUNCTION__,
						index);
			}
		} else {
			ErrorLog(g, __FILE__, __LINE__, "%s can not array  %s ",
					__FUNCTION__, arrayname);
		}
	}
	cJSON_Delete(oem);
	/*
	 cJSON_Delete(item);
	 cJSON_Delete(arrayitem);
	 cJSON_Delete(array);
	 */
	return getlen;
}

char* getpostinfo(char* buffer) {
	char cmd[3000];
	char *buf = (char*) malloc(sizeof(char) * 100);
	files f;
	if (f.openW("/tmp/ports")) {
		f.writes(buffer, strlen(buffer));
		f.close();
	}
	sprintf(cmd, "grep \"mac\" /tmp/ports | awk '{print $2}'");
	get_system_pipe(cmd, buf, 50);
	InfoLog(g, __FILE__, __LINE__, "匹配搭配到mac:%s通道数%d", buf, tun.size());
	int i = 0, re = 1;
	if (strcmp(buf, "") == 0) {
		free(buf);
		return 0;
	}
	return buf;
}

//线程轮询map中的所有mac开的通道，通过
void* updateiptable(void* arg) {
	while (1) {
		InfoLog(g, __FILE__, __LINE__, "更新线程还存活");
		sleep(3);
	}
}

//查找并插入
int search(std::map<const string, TUNNEL*>& t, const char* smac) {
	std::pair<std::map<const string, TUNNEL*>::iterator, bool> ret;
	//t.insert(pair<const string, TUNNEL*>(smac, new TUNNEL));
	string s;
	s = smac;
	ret = t.insert(make_pair(s, new TUNNEL));
	if (ret.second) {
		InfoLog(g, __FILE__, __LINE__, "插入成功, 现在map数%d", t.size());
		return 1;
	} else {
		InfoLog(g, __FILE__, __LINE__, "插入失败, 现在map数%d", t.size());
		return 0;
	}
	/*
	 std::map<const string, TUNNEL*>::iterator iter;
	 for (iter = t.begin(); iter != t.end(); iter++)
	 {
	 InfoLog(g, __FILE__, __LINE__, "map.smac=%s", (iter->first).c_str());
	 }
	 */
}

int openport() {
	char cmd[300];
	char buf[30000];

	while (1) {
		int port = Random(900, 1000);
		//InfoLog(g, __FILE__, __LINE__, "找到端口号:%d", port);
		sprintf(cmd, "./addrule.sh %d", port);
		get_system_pipe(cmd, buf, 30000);
		//InfoLog(g, __FILE__, __LINE__, "addrule返回值:%s\n", buf);
		if (strncmp("1", buf, 1) == 0) {
			InfoLog(g, __FILE__, __LINE__, "放行端口失败!!\n");
			continue;
		} else {
			InfoLog(g, __FILE__, __LINE__, "放行端口成功,端口号%d!!", port);
			return port;
		}
		get_system_pipe("sh  ./showrule.sh ", buf, 30000);
		InfoLog(g, __FILE__, __LINE__, "\n\查看规则:\n%s\n", buf);
	}
}

int closeport(int port) {
	char cmd[300];
	char buf[30000];
	//InfoLog(g, __FILE__, __LINE__, "删除规则\n");
	sprintf(cmd, "sh  ./delrule.sh %d", port);
	get_system_pipe(cmd, buf, 30000);
	get_system_pipe("sh  ./showrule.sh ", buf, 30000);
	InfoLog(g, __FILE__, __LINE__, "\n\查看规则:\n%s\n", buf);
}

/*
 */
void on_write(int sock, short event, void* arg) {
	char* buffer = (char*) arg;
	char* bu;
	char buf[30000];
	int i;
	try {
		//判断数据类型
		if (strncmp("GET", buffer, 3) == 0) {
			InfoLog(g, __FILE__, __LINE__, "%d,GET类型不支持\n", sock);
			send(sock, http200, strlen(http200), 0);
			send(sock, "method not support", strlen("method not support"), 0);
			close(sock);
		} else if (strncmp("POST", buffer, 4) == 0) {
			//只接收post
			//1、判断mac参数是否正确
			//2、判断mac是否在map中存在
			//3、给mac分配一个端口，同时打开防火墙
			//4、等待设备返回消息
			//5、返回设备发送的消息
			send(sock, http200, strlen(http200), 0);
			//1
			bu = getpostinfo(buffer);
			InfoLog(g, __FILE__, __LINE__, "%d,POST请求:%s", sock, bu);
			//2
			i = search(tun, bu);
			string s;
			s = bu;
			std::map<const string, TUNNEL*>::iterator iter;
			iter = tun.find(s);
			InfoLog(g, __FILE__, __LINE__, "mmmm=%s", (iter->first).c_str());
			//查看规则
			//InfoLog(g, __FILE__, __LINE__, "查看规则\n");
			get_system_pipe("sh  ./showrule.sh ", buf, 30000);
			//InfoLog(g, __FILE__, __LINE__, "\n\查看规则:\n%s\n", buf);
			//添加规则
			int port = 0;
			char cmd[200];
			//for (int i = 0; i < 90; )
			{
				port = Random(900, 1000);
				InfoLog(g, __FILE__, __LINE__, "找到端口号:%d", port);
				sprintf(cmd, "./addrule.sh %d", port);
				get_system_pipe(cmd, buf, 30000);
				//InfoLog(g, __FILE__, __LINE__, "addrule返回值:%s\n", buf);
				if (strncmp("1", buf, 1) == 0) {
					InfoLog(g, __FILE__, __LINE__, "放行端口失败!!\n");
					//continue;
				} else {
					TUNNEL newt;
				}
				get_system_pipe("sh  ./showrule.sh ", buf, 30000);
				//InfoLog(g, __FILE__, __LINE__, "\n\查看规则:\n%s\n", buf);
				//i++;
			}
			//删除规则
			//InfoLog(g, __FILE__, __LINE__, "删除规则\n");
			sprintf(cmd, "sh  ./delrule.sh %d", port);
			get_system_pipe(cmd, buf, 30000);
			get_system_pipe("sh  ./showrule.sh ", buf, 30000);
			//InfoLog(g, __FILE__, __LINE__, "\n\查看规则:\n%s\n", buf);
			send(sock, "mac=xxx", strlen("mac=xxx"), 0);
			close(sock);
		} else {
			//设备端的上报请求，解析json
			cJSON *root;
			char buf[200];
			char mac[20], stoken[40], method[20];

			root = cJSON_Parse(buffer);
			if (root == 0) {
				InfoLog(g, __FILE__, __LINE__, "不是json字符\n");
				write(sock, "{\"message\":\"no json data\"}", strlen("{\"message\":\"no json data\"}"));
				close(sock);
			} else {
				InfoLog(g, __FILE__, __LINE__, "是%d设备发送的json字符上报信息\n", sock);
				InfoLog(g, __FILE__, __LINE__, "%s3\n", cJSON_Print(root),
						cJSON_GetArraySize(root));
				if (get_json_item(buffer, "mac", mac, 200) > 0) {
					//InfoLog(g, __FILE__, __LINE__, "mac=%s", mac);
				}
				if (get_json_item(buffer, "random", stoken, 200) > 0) {
					//InfoLog(g, __FILE__, __LINE__, "random=%s", stoken);
				}
				sprintf(buf,
						"{\"mac\":\"%s\", \"stoken\":\"%s\", \"action\":\"%s\", \"port\":%d}",
						mac, stoken, "tunnel", 123);
				send(sock, buf, strlen(buf), 0);
			}
		}
		free(buffer);
	} catch (...) {
		InfoLog(g, __FILE__, __LINE__, "出现异常");
	}
}

/*
 */
void on_read(int sock, short event, void* arg) {
	struct event* write_ev;
	int size;
	struct sock_ev* ev = (struct sock_ev*) arg;
	ev->buffer = (char*) malloc(MEM_SIZE);
	bzero(ev->buffer, MEM_SIZE);
	size = recv(sock, ev->buffer, MEM_SIZE, 0);
	//InfoLog(g, __FILE__, __LINE__, "receive data:%s\n", ev->buffer);
	if (size == 0) {
		release_sock_event(ev);
		close(sock);
		return;
	}

	//ev->buffer[4]='\0';
	event_set(ev->write_ev, sock, EV_WRITE, on_write, ev->buffer);
	event_base_set(base, ev->write_ev);
	event_add(ev->write_ev, NULL);

}

/*
 */
void on_accept(int sock, short event, void* arg) {
	struct sockaddr_in cli_addr;
	int newfd, sin_size;
	struct sock_ev* ev = (struct sock_ev*) malloc(sizeof(struct sock_ev));
	ev->read_ev = (struct event*) malloc(sizeof(struct event));
	ev->write_ev = (struct event*) malloc(sizeof(struct event));
	sin_size = sizeof(struct sockaddr_in);
	newfd = accept(sock, (struct sockaddr*) &cli_addr, (socklen_t*) &sin_size);
	event_set(ev->read_ev, newfd, EV_READ | EV_PERSIST, on_read, ev);
	event_base_set(base, ev->read_ev);
	event_add(ev->read_ev, NULL);
}

/*
 */
int main(int argc, char* argv[]) {
	struct sockaddr_in my_addr;
	int sock;
	int port = atoi(argv[1]);

	createLog(&g);
	InfoLog(g, __FILE__, __LINE__, "init ok");
	WarnLog(g, __FILE__, __LINE__, "init ok");
	DebugLog(g, __FILE__, __LINE__, "init ok");
	ErrorLog(g, __FILE__, __LINE__, "init ok");
	srand((int) time(0));

	//创建套接字
	sock = socket(AF_INET, SOCK_STREAM, 0);
	int yes = 1;
	//套接字端口复用
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port);
	my_addr.sin_addr.s_addr = INADDR_ANY;
	//绑定并监听套接字
	bind(sock, (struct sockaddr*) &my_addr, sizeof(struct sockaddr));
	listen(sock, BACKLOG);

	//创建监听事件
	struct event listen_ev;
	//初始化时间基
	base = event_base_new();
	//为套接字添加监听事件与接收事件
	event_set(&listen_ev, sock, EV_READ | EV_PERSIST, on_accept, NULL);
	//为时间基添加监听事件
	event_base_set(base, &listen_ev);
	//添加监听事件
	event_add(&listen_ev, NULL);

	pthread_t pupdate;
	if (0 == pthread_create(&pupdate, 0, updateiptable, (void*) &sock)) {
		pthread_detach(pupdate);
		InfoLog(g, __FILE__, __LINE__, "开启定时清理线程");
	}

	//调度
	//test();
	//"HTTP/1.0 200 OK\r\nServer: wz simple httpd 1.0\r\nmac=1234\r\nContent-Type: text/html\r\n\r\n";
	regex(argv[2],
			"HTTP/1.0 200 OK\r\nServer: wz simple httpd 1.0\r\nmac=1234\r\nContent-Type: text/html\r\n\r\n");
	event_base_dispatch(base);

	//httpd();

	return 0;
}
