#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

enum ShaderKind {
	SHADER_VERTEX,
	SHADER_PIXEL,
};

HRESULT buildShader(const char *sourcePath, const char *targetPath, ShaderKind kind);
HRESULT buildVertexShader(const char *sourcePath, const char *targetPath);
HRESULT buildPixelShader(const char *sourcePath, const char *targetPath);