#include "zipstream.h"

#include "utils.h"
#include "zipformat.h"
#include <cassert>
#include <time.h>
#include <sys/stat.h>

static inline void error(const std::string &msg)
{
	fprintf(stderr, "**Error: %s", (msg + "\n").c_str());
	exit(1);
}

ZipStream::ZipStream(const std::string &zipName) : zipName(zipName), f{zipName}
{
	//fp = fopen(zipName.c_str(), "rb");
	//if (!fp)
	//	return;
	uint32_t id = 0;
    auto* fp = f.filePointer();
	fseek_x(fp, -22 + 5, SEEK_END);
	int counter = 64*1024+8;
	while (id != EndOfCD_SIG && counter > 0)
	{
		fseek_x(fp, -5, SEEK_CUR);
		id = read<uint32_t>();
		counter--;
	}
	if(counter <= 0)
	{
        f.close();
		return;
	}

	auto start = ftell_x(fp);
	fseek_x(fp, 4, SEEK_CUR);
	int64_t entryCount = read<uint16_t>();
	fseek_x(fp, 2, SEEK_CUR);
	auto cdSize = read<uint32_t>();
	int64_t cdOffset = read<uint32_t>();
	auto commentLen = read<uint16_t>();
	if (commentLen > 0)
	{
		comment = new char [commentLen+1];
		fread(comment, 1, commentLen, fp);
		comment[commentLen] = 0;
	}

	if(entryCount == 0xffff || cdOffset == 0xffffffff)
	{
		fseek_x(fp, start - 6*4, SEEK_SET);
		auto id = read<uint32_t>();
		if(id != EndOfCD64Locator_SIG)
		{
			fp = nullptr;
			return;
		}
		fseek_x(fp, 4, SEEK_CUR);
		auto cdStart = read<int64_t>();
		fseek_x(fp, cdStart, SEEK_SET);
		auto eocd64 = read<EndOfCentralDir64>();

		cdOffset = eocd64.cdoffset;	
		entryCount = eocd64.entries;
	}

	entries.reserve(entryCount);

	fseek_x(fp, cdOffset, SEEK_SET);
	char fileName[65536];
	CentralDirEntry cd;
	for (auto i = 0L; i < entryCount; i++)
	{
		fread(&cd, 1, sizeof(CentralDirEntry), fp);
		auto rc = fread(&fileName, 1, cd.nameLen , fp);
		fileName[rc] = 0;
		fseek_x(fp, cd.nameLen - rc, SEEK_CUR);
		int64_t offset = cd.offset;
		int exLen = cd.exLen;
		Extra extra;
		while(exLen > 0)
		{
			fread(&extra, 1, 4, fp);
			fread(extra.data, 1, extra.size, fp);
			if(extra.id == 0x01)
			{
				offset = extra.zip64.offset;
			} else 
				printf("Ignoring extra block %04x", extra.id);

			exLen -= (extra.size+4);
		}

		fseek_x(fp, cd.commLen, SEEK_CUR);
		auto flags = cd.attr1 >> 16;
		if((flags & S_IFREG) == 0) // Some archives have broken attributes
			flags = 0;
		entries.emplace_back(fileName, offset + 0, flags >> 16);
	}
}

ZipStream::~ZipStream()
{
	if (comment)
		delete [] comment;
}
