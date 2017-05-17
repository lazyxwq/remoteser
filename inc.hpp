/*
 * inc.h
 *
 *  Created on: Dec 12, 2016
 *      Author: oracle
 */

#ifndef INC_HPP_
#define INC_HPP_

#include "LOG.h"
#include <string>
#include "files.hpp"

#define GROUP_SER_ADDR	"239.0.0.30"	//服务器所在组播地址
#define GROUP_CLI_ADDR	"239.0.0.30"	//客户端所在组播地址
#define SER_PORT	44444

typedef struct _groupaddr
{
	std::string GROUP_SER_ADDR1;	//服务器所在组播地址
	std::string GROUP_CLI_ADDR1;	//客户端所在组播地址
	unsigned int SERPORT;			//服务器端口号
	std::string REMOTESERADDR;		//备用服务器的IP地址
	std::string CONTROLADDR;		//主控地址，用于给主控发送心跳报文
	unsigned int CONTROLPORT;		//主控端口，用于给主控发送心跳报文
}GROUPADDR;

#define LOG_STYLES_HELLO (LOG_STYLE_DATETIME | LOG_STYLE_PID | LOG_STYLE_LOGLEVEL | LOG_STYLE_SOURCE | LOG_STYLE_FORMAT | LOG_STYLE_NEWLINE)

extern int createLog(LOG **g);
extern int createLogFile(LOG **g, char* filename);

extern int Destroy(LOG *g);

#endif /* INC_HPP_ */
