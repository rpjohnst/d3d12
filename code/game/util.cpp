#include "util.h"
#include <fstream>

void printWindowsError(HRESULT error) {
	WCHAR *message = 0;
	auto length = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, error, 0, (LPWSTR)&message, 0, NULL);
	OutputDebugString(message);
	HeapFree(GetProcessHeap(), 0, message);
}

std::vector<char> readFile(const char *path) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	auto size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> data(size);
	file.read(data.data(), size);

	return data;
}
