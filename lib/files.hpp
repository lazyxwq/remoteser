/*
 * files.h
 *
 *  Created on: Dec 12, 2016
 *      Author: oracle
 */

#ifndef FILES_HPP_
#define FILES_HPP_

#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

class files
{
public:
	files();

	bool open(std::string filepath);

	bool openW(std::string filepath);

	bool openR(std::string filepath);

	int writes(char* buf, int buflen);

	bool close();

	char *getAll();

	int getSize();

	virtual ~files();

	struct stat st;
	FILE *fp;
};

#endif /* FILES_HPP_ */
