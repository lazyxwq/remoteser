#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <event.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <vector>
#include <string>
#include <list>
#include <queue>
#include <map>
#include <hash_map>
#include <regex.h>
#include <signal.h>

#include "lib/inc.hpp"
#include "lib/httpd.hpp"
#include "lib/cJSON.h"
#include "lib/tmap.hpp"
#include "lib/tqueue.hpp"
using namespace std;

#define PORT        25341
#define BACKLOG     5
#define MEM_SIZE    1024

/*
 #define	INFO\( InfoLog\(g,__FILE__,__LINE__,
 #define	WARN\( WarnLog\(g,__FILE__,__LINE__,
 #define	DEBUG\( DebugLog\(g,__FILE__,__LINE__,
 #define	ERR\( ErrorLog(g,__FILE__,__LINE__,
 */

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
	struct event* read_ev;	//读事件
	struct event* write_ev;	//写事件
	struct event* process;	//处理入队消息
	char* buffer;
};

enum {
	ACTION_OPEN,	//打开请求
	ACTION_ESTABLISHED,	//通道建立
	ACTION_CLOSE,	//通道在关闭
	ACTION_FIN,	//通道已经关闭
	ACTION_NONE,
};

typedef struct _tunnel {
	string mac = "";	//mac地址
	int port;		//开启的ssh端口号
	time_t time;	//下一次检测ssh通道存活时间
	int action = ACTION_NONE;
	int websock = 0;

	_tunnel() {
		mac = "";
		port = 0;
		time = 0;
		action = ACTION_NONE;
		websock = 0;
	}

	/*
	 bool operator <(const _tunnel& rhs) const {
	 return memcmp(mac, mac, 20);
	 }
	 */
} TUNNEL;

mymap::map<const string, TUNNEL*> tun;//存储了已开启的ssh通道的设备的所有信息，以MAC地址作为键,mymap应该是线程安全的函数
//t_queue<TUNNEL*> todevice;	//队列，存放了web端下发给设备指令
//mymap::map<const string, TUNNEL*> mtu;

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

#define OPENMODEL "{\"mac\":\"%s\", \"stoken\":\"%s\", \"action\":\"%s\", \"port\":\"%d\", \"passwd\":\"%s\"}"
#define CLOSEMODEL "{\"mac\":\"%s\", \"stoken\":\"%s\", \"action\":\"%s\", \"port\":\"%d\"}"

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
			//ErrorLog(g, __FILE__, __LINE__, "recv can not get %s string", name);
			out[0] = '\0';
			return 0;
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

//从post请求中获取指定的字段值
char* getpostinfo(char* buffer, char* name) {
	char cmd[3000];
	char *buf = (char*) malloc(sizeof(char) * 100);
	files f;
	if (f.openW("/tmp/ports")) {
		f.writes(buffer, strlen(buffer));
		f.close();
	}
	sprintf(cmd, "sh ../matchmac", name);
	get_system_pipe(cmd, buf, 50);
	//if (buf[strlen(buf)] == ' ')
	buf[strlen(buf) - 1] = '\0';
	//InfoLog(g, __FILE__, __LINE__, "x=%x %x", buf[strlen(buf)], buf[strlen(buf)-1]);
	InfoLog(g, __FILE__, __LINE__, "匹配搭配到%s:%s通道数%d, 大小%d", name, buf,
			tun.size_s(), strlen(buf));
	int i = 0;
	if (strcmp(buf, "") == 0) {
		free(buf);
		return 0;
	}
	return buf;
}

//线程轮询map中的所有mac开的通道，通过
void* updateiptable(void* arg) {
	mymap::map<const string, TUNNEL*>::iterator itr;
	itr = tun.begin_s();
	tun._mutex_stats.TryUnLock();
	while (1) {
		if (itr == tun.end_s()) {
			tun._mutex_stats.TryUnLock();
			InfoLog(g, __FILE__, __LINE__, "更新线程已经轮询完一次");
			itr = tun.begin_s();
			tun._mutex_stats.TryUnLock();
			sleep(3);
		}
		itr++;
		InfoLog(g, __FILE__, __LINE__, "更新线程还存活");
	}
}

//查找并插入，插入成功返回结构体TUNNEL，包含mac信息，端口号，下次检测存活时间
TUNNEL* search(mymap::map<const string, TUNNEL*>& t, const char* smac) {
	string s;
	s = smac;
	//插入数据
	TUNNEL *nt = new TUNNEL;
	nt->mac = s;
	int ret = t.insert_s(s, nt);
	//判断插入是否成功
	if (ret) {
		InfoLog(g, __FILE__, __LINE__, "插入成功, 现在map数%d", t.size_s());
	} else {
		if (nt)
			delete nt;
		nt = 0;
		InfoLog(g, __FILE__, __LINE__, "插入失败, 现在map数%d", t.size_s());
	}
	return nt;
}

//找到一个可用的端口号进行放行，并返回端口号
int openport() {
	char cmd[300];
	char buf[30000];

	while (1) {
		int port = Random(9000, 10000);
		//InfoLog(g, __FILE__, __LINE__, "找到端口号:%d", port);
		sprintf(cmd, "sh ../addrule.sh %d", port);
		get_system_pipe(cmd, buf, 30000);
		//InfoLog(g, __FILE__, __LINE__, "addrule返回值:%s\n", buf);
		if (strncmp("1", buf, 1) == 0) {
			//InfoLog(g, __FILE__, __LINE__, "放行端口失败!!\n");
			continue;
		} else {
			InfoLog(g, __FILE__, __LINE__, "放行端口成功,端口号%d!!", port);
			return port;
		}
	}
}

//关闭端口，清理连接
int closeport(int port) {
	char cmd[300];
	char buf[30000];
	//InfoLog(g, __FILE__, __LINE__, "删除规则\n");
	sprintf(cmd, "sh ../delrule.sh %d", port);
	get_system_pipe(cmd, buf, 30000);
	get_system_pipe("sh ../showrule.sh ", buf, 30000);
	InfoLog(g, __FILE__, __LINE__, "\n\查看规则:\n%s\n", buf);
}

/*
 */
void on_write(int sock, short event, void* arg) {
	char* buffer = (char*) arg;
	char* bu;
	char buf[30000];
	char response[3000];
	int i;
	try {
		//判断数据类型
		if (strncmp("GET", buffer, 3) == 0) {
			InfoLog(g, __FILE__, __LINE__, "%d,GET类型不支持\n", sock);
			send(sock, http200, strlen(http200), 0);
			sprintf(response, "method not support");
			send(sock, response, strlen(response), 0);
			close(sock);
			return;
		} else if (strncmp("POST", buffer, 4) == 0) {
			//只接收post
			//1、判断mac参数是否正确
			//2、判断mac是否在map中存在
			//3、给mac分配一个端口，同时打开防火墙
			//4、等待设备返回消息
			//5、返回设备发送的消息
			//6、将消息放到队列中
			//0、先发送200 ok报文
			send(sock, http200, strlen(http200), 0);
			//1、检测post请求是否携带所需信息
			bu = getpostinfo(buffer, "mac");
			if (buf == 0) {
				InfoLog(g, __FILE__, __LINE__, "%d,POST请求非法", sock);
				sprintf(response, "POST invalid");
				send(sock, response, strlen(response), 0);
				close(sock);
				return;
			} else {
				InfoLog(g, __FILE__, __LINE__, "%d,POST请求:%s", sock, bu);
				//InfoHexLog(g, __FILE__, __LINE__, bu , 100 , "hello INFO buffer[%ld]" , 100 );
			}
			//2、将mac信息插入，成功则返回结构体，下一步分配端口
			TUNNEL *nt = search(tun, bu);
			if (nt == 0) {
				mymap::map<const string, TUNNEL*>::iterator itr = tun.find_s(
						string(bu));
				int port = itr->second->port;
				InfoLog(g, __FILE__, __LINE__, "POST请求的%s，地址有存在信息，端口号%d", bu,
						port);
				InfoLog(g, __FILE__, __LINE__,
						"信息输出:mac:%s,连接的远程端口:%d,动作:%d,web套接字:%d,定时器:%d",
						itr->second->mac.c_str(), itr->second->port,
						itr->second->action, itr->second->websock,
						itr->second->time);
				/*
				 mymap::map<const string, TUNNEL*>::iterator mitr = mtu.find_s(string(bu));
				 port = mitr->second->port;
				 InfoLog(g, __FILE__, __LINE__, "tPOST请求的%s，地址有存在信息，端口号%d", bu,
				 port);
				 InfoLog(g, __FILE__, __LINE__,
				 "t信息输出:mac:%s,连接的远程端口:%d,动作:%d,web套接字:%d,定时器:%d",
				 mitr->second->mac.c_str(), mitr->second->port,
				 mitr->second->action, mitr->second->websock,
				 mitr->second->time);
				 */
				sprintf(response, "mac:%s already open port %d", bu, port);
				send(sock, response, strlen(response), 0);
				close(sock);
				return;
			}
			//3、查找可用port信息，放行，添加到nt
			while ((nt->port = openport()) == 0)
				;
			InfoLog(g, __FILE__, __LINE__, "%s放行的端口号为%d", nt->mac.c_str(),
					nt->port);
			sprintf(response, "mac:%s allocate new port %d", bu, nt->port);
			nt->websock = sock;
			nt->action = ACTION_OPEN;
			InfoLog(g, __FILE__, __LINE__,
					"信息输出:mac:%s,连接的远程端口:%d,动作:%d,web套接字:%d,定时器:%d",
					nt->mac.c_str(), nt->port, nt->action, nt->websock,
					nt->time);
			//todevice.push(nt);	//将web的请求压入处理队列中，等待设备上报状态后进行连接
			//send(sock, response, strlen(response), 0);	//发送连接成功或失败不在这里发，在另外一个轮询线程发送
			//close(sock);
		} else {
			//设备端的上报请求，解析json
			cJSON *root;
			char buf[200];
			char mac[20], random[50], stoken[40], method[20], response[30],
					message[100], tunnel[4];

			root = cJSON_Parse(buffer);
			if (root == 0) {
				//解析字符串失败
				InfoLog(g, __FILE__, __LINE__, "不是json字符\n");
				send(sock, "{\"message\":\"no json data\"}",
						strlen("{\"message\":\"no json data\"}"), 0);
				close(sock);
			} else {
				//InfoLog(g, __FILE__, __LINE__, "是%d设备发送的json字符上报信息\n", sock);
				InfoLog(g, __FILE__, __LINE__, "%s%d", cJSON_Print(root),
						cJSON_GetArraySize(root));
				if (get_json_item(buffer, "mac", mac, sizeof(mac)) > 0) {
					InfoLog(g, __FILE__, __LINE__, "mac=%s", mac);
				}
				if (get_json_item(buffer, "random", random, sizeof(random))
						> 0) {
					InfoLog(g, __FILE__, __LINE__, "random=%s", random);
				}
				if (get_json_item(buffer, "stoken", stoken, sizeof(stoken))
						> 0) {
					InfoLog(g, __FILE__, __LINE__, "stoken=%s", stoken);
				}
				if (get_json_item(buffer, "response", response,
						sizeof(response)) > 0) {
					InfoLog(g, __FILE__, __LINE__, "response=%s", response);
				}
				if (get_json_item(buffer, "message", message, sizeof(message))
						> 0) {
					InfoLog(g, __FILE__, __LINE__, "message=%s", message);
				}
				if (get_json_item(buffer, "tunnel", tunnel, sizeof(tunnel))
						> 0) {
					InfoLog(g, __FILE__, __LINE__, "tunnel=%s", tunnel);
				}
				mymap::map<const string, TUNNEL*>::iterator itr;
				itr = tun.find_s(string(mac));
				if (mac[0] != '\0' && random[0] != '\0' && tunnel[0] != '\0') {
					InfoLog(g, __FILE__, __LINE__, "设备上报的是定时心跳报文");
					//查找map是否有通道会话信息
					if (itr != tun.end_s()) {
						//存在会话
						switch (itr->second->action) {
						case ACTION_OPEN:
							//开启通道
							sprintf(response, OPENMODEL, mac, random,
									"opentunnel", itr->second->port, "123456");
							break;
						case ACTION_CLOSE:
							sprintf(response, OPENMODEL, mac, random,
									"closetunnel", itr->second->port, "");
							break;
						case ACTION_NONE:
							sprintf(response, OPENMODEL, mac, random, "none", 0,
									"");
							break;
						default:
							sprintf(response, OPENMODEL, mac, random, "none", 0,
									"");
							break;
						}

					} else {
						sprintf(response, OPENMODEL, mac, random, "none", 0,
								"");
					}
					send(sock, response, strlen(response), 0);
				} else if (mac[0] != '\0' && stoken[0] != '\0'
						&& response[0] != '\0' && message[0] != '\0'
						&& tunnel[0] != '\0') {
					InfoLog(g, __FILE__, __LINE__, "设备上报的是开启或则关闭通道的响应信息");
					if (itr != tun.end_s()) {
						//存在会话
						switch (itr->second->action) {
						case ACTION_OPEN:
							//开启通道，判断是否开启成功，开启成功则改变会话状态
							if (strncmp(message, "openok!", sizeof("openok!"))
									== 0) {
								InfoLog(g, __FILE__, __LINE__, "已建立连接状态");
								itr->second->action = ACTION_ESTABLISHED;//已建立连接状态
							}
							sprintf(response, OPENMODEL, mac, random,
									"opentunnel", itr->second->port, "123456");
							break;
						case ACTION_CLOSE:
							if (strcmp(message, "closeok") == 0) {
								itr->second->action = ACTION_FIN;	//已建立连接状态
							}
							sprintf(response, OPENMODEL, mac, random,
									"closetunnel", itr->second->port, "");
							break;
						case ACTION_NONE:
							//sprintf(response, OPENMODEL, mac, random, "none", 0, "");
							break;
						default:
							//sprintf(response, OPENMODEL, mac, random, "none", 0, "");
							break;
						}
						if ((itr->second->websock) > 0) {
							InfoLog(g, __FILE__, __LINE__, "给web:%d发信息,状态是%d",
									itr->second->websock, itr->second->action);
							send(itr->second->websock, response,
									strlen(response), 0);
							close(itr->second->websock);
							itr->second->websock = -1;
						}
					}
				} else {
					InfoLog(g, __FILE__, __LINE__, "既不是定时心跳报文也不是响应报文：报文内容为\n%s",
							cJSON_Print(root));
					//printf("%s %s %s %s %s %s\n", mac, random, stoken, response, message, tunnel);
				}
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
	//接收数据
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

void* event_dispatch(void* arg) {
	struct event_base* base = *(struct event_base**)arg;
	event_base_dispatch(base);
}

/*
 */
int main(int argc, char* argv[]) {
	struct sockaddr_in my_addr;
	int sock;
	int port = atoi(argv[1]);
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;	//设定接受到指定信号后的动作为忽略
	sa.sa_flags = 0;
	if (sigemptyset(&sa.sa_mask) == -1 || //初始化信号集为空
			sigaction(SIGPIPE, &sa, 0) == -1) { //屏蔽SIGPIPE信号
		perror("failed to ignore SIGPIPE; sigaction");
		exit(EXIT_FAILURE);
	}

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
	pthread_t lbevent;
	if (0 == pthread_create(&lbevent, 0, event_dispatch, (void*) &base)) {
		pthread_detach(pupdate);
		InfoLog(g, __FILE__, __LINE__, "开启监听线程");
	}

	//调度
	//test();
	//"HTTP/1.0 200 OK\r\nServer: wz simple httpd 1.0\r\nmac=1234\r\nContent-Type: text/html\r\n\r\n";
	/*
	 regex(argv[2],
	 "HTTP/1.0 200 OK\r\nServer: wz simple httpd 1.0\r\nmac=1234\r\nContent-Type: text/html\r\n\r\n");
	 */
	while (1) {
		printf("主线程还活着\n");
		sleep(3);
	}
	//event_base_dispatch(base);

	//httpd();

	return 0;
}
