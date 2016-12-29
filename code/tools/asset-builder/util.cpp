#include "util.h"
#include <ShlObj.h>
#include <cstdio>

void printWindowsError(HRESULT error) {
	WCHAR *message = 0;
	auto length = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, error, 0, (LPWSTR)&message, 0, NULL);
	fprintf(stderr, "%ls\n", message);
	HeapFree(GetProcessHeap(), 0, message);
}

std::vector<WCHAR> toWide(const char *str) {
	auto len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
	std::vector<WCHAR> wide(len);
	MultiByteToWideChar(CP_UTF8, 0, str, -1, wide.data(), len);
	return wide;
}

std::vector<char> fromWide(const WCHAR *str) {
	auto len = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
	std::vector<char> text(len);
	WideCharToMultiByte(CP_UTF8, 0, str, -1, text.data(), (int)text.size(), NULL, NULL);
	return text;
}

HRESULT getEnv(const char *name, std::vector<char> *value) {
	auto nameWide = toWide(name);
	auto len = GetEnvironmentVariable(nameWide.data(), NULL, 0);
	if (len == 0) {
		return GetLastError();
	}

	std::vector<WCHAR> valueWide(len);
	GetEnvironmentVariable(nameWide.data(), valueWide.data(), (DWORD)valueWide.size());
	*value = fromWide(valueWide.data());
	return S_OK;
}

HRESULT makeDir(const char *path) {
	auto pathWide = toWide(path);
	auto len = GetFullPathName(pathWide.data(), 0, NULL, NULL);

	std::vector<WCHAR> pathFull(len);
	GetFullPathName(pathWide.data(), (DWORD)pathFull.size(), pathFull.data(), NULL);

	auto result = SHCreateDirectory(NULL, pathFull.data());
	if (result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS) {
		return S_OK;
	} else {
		return GetLastError();
	}
}

HRESULT getFileExists(const char *path, bool *fileExists) {
	auto pathWide = toWide(path);

	WIN32_FIND_DATA wfd = {};
	HANDLE file = FindFirstFile(pathWide.data(), &wfd);
	if (file == INVALID_HANDLE_VALUE) {
		HRESULT hr = GetLastError();
		if (hr == ERROR_FILE_NOT_FOUND || hr == ERROR_PATH_NOT_FOUND) {
			*fileExists = false;
			return S_OK;
		} else {
			return hr;
		}
	}
	if (!FindClose(file)) {
		return GetLastError();
	}

	*fileExists = true;
	return S_OK;
}

HRESULT getLastWriteTime(const char *path, uint64_t *lastWriteTime) {
	auto pathWide = toWide(path);

	WIN32_FIND_DATA wfd = {};
	HANDLE file = FindFirstFile(pathWide.data(), &wfd);
	if (file == INVALID_HANDLE_VALUE) {
		return GetLastError();
	}
	if (!FindClose(file)) {
		return GetLastError();
	}

	ULARGE_INTEGER lwt = {};
	lwt.HighPart = wfd.ftLastWriteTime.dwHighDateTime;
	lwt.LowPart = wfd.ftLastWriteTime.dwLowDateTime;

	*lastWriteTime = lwt.QuadPart;
	return S_OK;
}
