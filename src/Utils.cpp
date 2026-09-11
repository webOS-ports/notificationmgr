// Copyright (c) 2013-2024 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "Utils.h"
#include <sys/stat.h>
#include <sys/statfs.h>
#include <new>
#include <Logging.h>

namespace Utils {

//! Upper bound for readFile(), which only ever reads small json files.
static const long kMaxReadFileSize = 4 * 1024 * 1024;

bool verifyFileExist(const char *pathAndFile) {

	if(pathAndFile == NULL) {
		return false;
	}

	struct stat buf;

	if(-1 == ::stat(pathAndFile, &buf)) {
		return false;
	}
	//File exist on the local file system.
	return true;
}

bool getLS2ServiceDomainPart(const std::string& url, std::string& domainPart) {
	//TODO: Add code.
	return true;
}

char* readFile(const char* filePath)
{
	if(!filePath)
		return 0;

	FILE* fp = fopen(filePath, "r");
	if(!fp)
		return 0;

	if(fseek(fp, 0L, SEEK_END) != 0)
	{
		fclose(fp);
		return 0;
	}

	long sz = ftell(fp);
	rewind(fp);

	if(sz <= 0)
	{
		fclose(fp);
		return 0;
	}

	/* Every caller hands the result straight to the json parser, and the
	 * things read here are a config file and a preferences file. A file
	 * larger than this is not one of those, and reading it would only mean
	 * allocating whatever someone put on the filesystem.
	 */
	if(sz > kMaxReadFileSize)
	{
		LOG_WARNING(MSGID_READ_FILE_TOO_LARGE, 1,
			PMLOGKS("PATH", filePath),
			"File is larger than %ld bytes in %s", kMaxReadFileSize, __PRETTY_FUNCTION__);
		fclose(fp);
		return 0;
	}

	/* One byte more than the file: the terminator goes after the content
	 * rather than on top of its last byte, which is what this used to do -
	 * every file read here silently lost its final character.
	 */
	char* ptr = new (std::nothrow) char[sz + 1];
	if(!ptr)
	{
		fclose(fp);
		return 0;
	}

	size_t result = fread(ptr, 1, static_cast<size_t>(sz), fp);
	fclose(fp);

	if(result != static_cast<size_t>(sz))
	{
		delete[] ptr;
		return 0;
	}

	ptr[sz] = 0;
	return ptr;
}

std::string extractTimestampFromId(const std::string& id)
{
    std::string time_stamp;
    std::size_t pos = id.rfind("-");
    if (pos != std::string::npos)
        time_stamp = id.substr(pos + 1);
    return time_stamp;
}

std::string extractSourceIdFromCaller(const std::string& id)
{
    std::string sourceId;
    std::size_t pos = id.rfind(" ");
    if (pos != std::string::npos)
    {
        sourceId = id.substr(0,pos);
        LOG_DEBUG("==== blank extractSourceIdFromCaller ==== %s", sourceId.c_str());
    }
    else
    {
        sourceId = id;
        LOG_DEBUG("==== no blank extractSourceIdFromCaller ==== %s", sourceId.c_str());
    }
    return sourceId;
}

void createTimestamp(std::string& timestamp)
{
	long long timevalue;
	struct timeval tp;
	std::stringstream ss;

	//Get the timestamp and add it to message
	gettimeofday(&tp, NULL);
    timevalue = tp.tv_sec * 1000LL + tp.tv_usec / 1000; //Get the milliseconds.
	ss << timevalue;

	timestamp = ss.str();

	return;
}

bool isValidURI(const std::string& uri)
{
	if(uri.find("://",0) == std::string::npos)
		return false;
	return true;
}

bool isEscapeChar(char c)
{
	const std::string escape_chars = "\n\t\v\f\r";
	return (escape_chars.find(c) != std::string::npos);
}

namespace Private
{
    gboolean cbAsync(gpointer data)
    {
        IAsyncCall *p = reinterpret_cast<IAsyncCall*>(data);
        if (!p) return false;

        p->Call();

        delete p;

        return false;
    }
}

}
