#include "shader.h"
#include "util.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>

using Microsoft::WRL::ComPtr;

static const char *shaderKinds[] = { "vs_5_0", "ps_5_0" };

HRESULT buildShader(const char *sourcePath, const char *targetPath, ShaderKind kind) {
	HRESULT hr;

	ComPtr<ID3DBlob> shader;
	{
		auto path = toWide(sourcePath);

		ComPtr<ID3DBlob> errors;
		hr = D3DCompileFromFile(
			path.data(), NULL, NULL, "main", shaderKinds[kind],
#if defined(_DEBUG)
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION |
#endif
			D3DCOMPILE_PACK_MATRIX_ROW_MAJOR, 0,
			&shader, &errors
		);
		if (FAILED(hr)) {
			fprintf(stderr, "%s\n", (char*)errors->GetBufferPointer());
			return hr;
		}
	}

	{
		auto path = toWide(targetPath);
		hr = D3DWriteBlobToFile(shader.Get(), path.data(), TRUE);
		if (FAILED(hr)) {
			fprintf(stderr, "shader : Error: Could not write to file %s\n", targetPath);
			return hr;
		}
	}

	return S_OK;
}

HRESULT buildVertexShader(const char *sourcePath, const char *targetPath) {
	return buildShader(sourcePath, targetPath, SHADER_VERTEX);
}

HRESULT buildPixelShader(const char *sourcePath, const char *targetPath) {
	return buildShader(sourcePath, targetPath, SHADER_PIXEL);
}
