#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vector>

#define TRY(expr) do { \
	HRESULT hr = expr; \
	if (FAILED(hr)) { \
		return hr; \
	} \
} while (false)

void printWindowsError(HRESULT error);

std::vector<char> readFile(const char *path);
