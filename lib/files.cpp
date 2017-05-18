/*
 * files.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: oracle
 */

#include <stdio.h>

#include "inc.hpp"
using namespace std;

files::files()
{
	// TODO Auto-generated constructor stub
	this->fp = 0;
}

bool files::open(std::string filepath)
{
	fp = fopen(filepath.c_str(), "r");
	if (fp == NULL)
	{
		cout << "打开文件失败" << endl;
		return false;
	}

	stat(filepath.c_str(), &(this->st));
	return true;
}

char* files::getAll()
{
	if (this->fp == NULL)
	{
		return 0;
	}
	//if (feof(fp) == -1)
	{
		//DLOG("文件到eof了!");
		fseek(fp, 0, 0);
	}

	char *fbuf = new char[this->st.st_size];
	if (fread(fbuf, sizeof(char), st.st_size, this->fp) != st.st_size)
	{
		cout << "获取文件失败！" << endl;
		delete fbuf;
		return 0;
	}

	return fbuf;
}

files::~files()
{
	// TODO Auto-generated destructor stub
}

int files::writes(char* buf, int buflen)
{
	if (this->fp == NULL)
	{
		cout << "文件未打开" << endl;
		return -1;
	}
	else
	{
		int len = fwrite((void*) buf, sizeof(char), buflen, this->fp);
		if (len != buflen)
		{
			perror("fwrite");
			return len;
		}
		else
		{
			return len;
		}
	}
}

bool files::close()
{
	if (fp == NULL)
	{
		cout << "文件未打开" << endl;
		return false;
	}
	else
	{
		return fclose(this->fp);
	}
}

int files::getSize()
{
	if (fp != NULL)
	{
		return st.st_size;
	}
	else
		return -1;
}

bool files::openW(std::string filepath)
{
	this->fp = fopen(filepath.c_str(), "w");
	if (this->fp == NULL)
	{
		perror("fopen");
		return false;
	}

	return true;
}

bool files::openR(std::string filepath)
{
}
