#include <cstdlib>
#include <filesystem>
#include <cassert>
namespace fs = std::filesystem;
#include <chrono>
namespace chrono = std::chrono;
#include <string>
using std::wstring;
#include <sstream>
using std::wstringstream;
#include "md5.h"
static bool g_verbose;
/** @return null-ternimated c-string of the entire file's contents */
static char* readEntireFile(const fs::path::value_type* fileName, 
                            uintmax_t fileSizeInBytes)
{
#if _MSC_VER
	FILE* file = _wfopen(fileName, L"rb");
#else
	FILE* file = fopen(fileName, "rb");
#endif
	if(file)
	{
		char* data = static_cast<char*>(malloc(fileSizeInBytes + 1));
		if(!data)
		{
			fprintf(stderr, "Failed to alloc %lli bytes for '%ws'!\n", 
			        fileSizeInBytes + 1, fileName);
			return nullptr;
		}
		const size_t bytesRead = 
			fread(data, sizeof(char), fileSizeInBytes, file);
		if(bytesRead != fileSizeInBytes)
		{
			free(data);
			fprintf(stderr, "Failed to completely read '%ws'!\n", fileName);
			return nullptr;
		}
		if(fclose(file) != 0)
		{
			fprintf(stderr, "Failed to close '%ws'!\n", fileName);
		}
		data[fileSizeInBytes] = '\0';
		return data;
	}
	else
	{
		fprintf(stderr, "Failed to open '%ws'!\n", fileName);
		return nullptr;
	}
}
static bool writeEntireFile(const fs::path::value_type* fileName, 
                            const wchar_t* nullTerminatedFileData, 
                            bool appendWriteMode)
{
#if _MSC_VER
	FILE* file = _wfopen(fileName, appendWriteMode ? L"ab" : L"wb");
#else
	FILE* file = fopen(fileName, appendWriteMode ? "ab" : "wb");
#endif
	if(file)
	{
		const size_t elementSize = sizeof(wchar_t);
		const size_t fileDataElementCount = 
			wstring(nullTerminatedFileData).size();
		const size_t elementsWritten = 
			fwrite(nullTerminatedFileData, elementSize, 
			       fileDataElementCount, file);
		if(fclose(file) != 0)
		{
			fprintf(stderr, "Failed to close '%ws'!\n", fileName);
		}
		if(elementsWritten != fileDataElementCount)
		{
			fprintf(stderr, "Failed to write '%ws'!\n", fileName);
			return false;
		}
	}
	else
	{
		fprintf(stderr, "Failed to open '%ws'!\n", fileName);
		return false;
	}
	return true;
}
int main(int argc, char** argv)
{
	const auto timeMainStart = chrono::high_resolution_clock::now();
	if(argc <= 1)
	{
		printf("Usage: kmd5 input_directory output_directory output_file_name "
		       " [--verbose] [--append]\n");
		return EXIT_SUCCESS;
	}
	static const int REQUIRED_ARG_COUNT = 4;
	if(argc < REQUIRED_ARG_COUNT)
	{
		fprintf(stderr, "Incorrect # of arguments!\n");
		return EXIT_FAILURE;
	}
	const fs::path pathInputDirectory(argv[1]);
	const fs::path pathOutputDirectory(argv[2]);
	const char*const outputFileName = argv[3];
	bool appendFileMode = false;
	for(int a = REQUIRED_ARG_COUNT; a < argc; a++)
	{
		if(strcmp(argv[a], "--verbose") == 0)
		{
			g_verbose = true;
		}
		else if(strcmp(argv[a], "--append") == 0)
		{
			appendFileMode = true;
		}
		else
		{
			fprintf(stderr, "ERROR: incorrect usage on param[%i]=='%s'\n",
			        a, argv[a]);
			return EXIT_FAILURE;
		}
	}
	if(g_verbose)
	{
		printf("Computing MD5 on all files recursively in directory '%ws'...\n", 
		       pathInputDirectory.c_str());
	}
	MD5_CTX mdContext;
	wstring output;
	for(const fs::directory_entry& entry : 
		fs::recursive_directory_iterator(pathInputDirectory))
	{
		if(!entry.is_regular_file())
		{
			continue;
		}
		const uintmax_t fileSizeInBytes = fs::file_size(entry.path());
		char*const fileData = 
			readEntireFile(entry.path().c_str(), fileSizeInBytes);
		// @HACK: `fileData` can just leak memory here because I'm not actually 
		//        hashing that many files.  WHO CARES?
		// Compute the MD5 checksum of `fileData` //
		{
			MD5Init(&mdContext);
			MD5Update(&mdContext, 
			          reinterpret_cast<unsigned char *>(fileData), 
			          fileSizeInBytes);
			MD5Final(&mdContext);
		}
		if(g_verbose)
		{
			printf("'%ws';",entry.path().c_str());
			for (unsigned i = 0; i < 16; i++)
				printf ("%02x", mdContext.digest[i]);
			printf("\n");
		}
		wstringstream wss;
		wss << entry.path().c_str() << ";";
		wss << std::setfill(L'0') << std::hex;
		for (unsigned i = 0; i < 16; i++)
			wss << std::setw(2) << mdContext.digest[i];
		wss << "\n";
		output.append(wss.str());
	}
	// write the output file //
	fs::create_directories(pathOutputDirectory);
	const fs::path outPath = pathOutputDirectory / outputFileName;
	writeEntireFile(outPath.c_str(), output.c_str(), appendFileMode);
	// calculate the program execution time //
	const auto timeMainEnd = chrono::high_resolution_clock::now();
	const auto timeMainDuration = chrono::duration_cast<chrono::microseconds>(
		timeMainEnd - timeMainStart);
	const float secondsMainDuration = timeMainDuration.count() / 1000000.f;
	printf("kmd5 complete! Seconds elapsed=%f\n", secondsMainDuration);
	return EXIT_SUCCESS;
}
#include "md5.c"