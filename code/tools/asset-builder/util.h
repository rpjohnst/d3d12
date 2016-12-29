#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vector>

void printWindowsError(HRESULT error);

std::vector<WCHAR> toWide(const char *str);
std::vector<char> fromWide(const WCHAR *str);

HRESULT getEnv(const char *name, std::vector<char> *value);
HRESULT makeDir(const char *path);

HRESULT getFileExists(const char *path, bool *fileExists);
HRESULT getLastWriteTime(const char *path, uint64_t *lastWriteTime);
