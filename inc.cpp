#include "inc.hpp"

int createLog(LOG **g)
{
	*g = CreateLogHandle();
	if (g == NULL)
	{
		perror("创建日志环境失败");
		return -1;
	}
	else
	{
		printf("创建日志环境成功!\n");
	}

	//设置日志输出文件名称
	SetLogOutput(*g, LOG_OUTPUT_STDOUT, NULL, LOG_NO_OUTPUTFUNC);
	//设置当前日志过滤等级
	SetLogLevel(*g, LOG_LEVEL_DEBUG);
	//设置当前行日志风格方案
	SetLogStyles(*g, LOG_STYLES_HELLO, LOG_NO_STYLEFUNC);

	return 1;
}

extern int createLogFile(LOG** g, char* filename)
{
	*g = CreateLogHandle();
	if (g == NULL)
	{
		perror("创建日志环境失败");
		return -1;
	}
	else
	{
		printf("创建日志环境成功!\n");
	}

	//设置日志输出文件名称
	SetLogOutput(*g, LOG_OUTPUT_FILE, filename, LOG_NO_OUTPUTFUNC);
	//设置当前日志过滤等级
	SetLogLevel(*g, LOG_LEVEL_DEBUG);
	//设置当前行日志风格方案
	SetLogStyles(*g, LOG_STYLES_HELLO, LOG_NO_STYLEFUNC);

	SetLogRotateMode(*g, LOG_ROTATEMODE_SIZE );
	SetLogRotateSize( *g , 1*1024*1024 );
	//SetLogRotatePressureFactor(*g, 20 );


	return 1;
}

int Destroy(LOG *g)
{
	DestroyLogHandle(g);
	printf("释放日志句柄\n");
}
