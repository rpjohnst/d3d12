#include "mesh.h"
#include "shader.h"
#include "util.h"

template <typename F>
HRESULT build(
	const char *sourceDir, const char *targetDir,
	const char *sourceExt, const char *targetExt,
	uint64_t builderTime,
	F f, const char *assets[], size_t numAssets
) {
	HRESULT hr;

	HRESULT error = S_OK;
	for (size_t i = 0; i < numAssets; i++) {
		auto sourceLen = strlen(assets[i]) + 1 + strlen(sourceExt);
		auto targetLen = strlen(assets[i]) + 1 + strlen(targetExt);

		std::vector<char> sourcePath(strlen(sourceDir) + sourceLen + 1);
		snprintf(sourcePath.data(), sourcePath.size(), "%s%s.%s", sourceDir, assets[i], sourceExt);

		std::vector<char> targetPath(strlen(targetDir) + targetLen + 1);
		snprintf(targetPath.data(), targetPath.size(), "%s%s.%s", targetDir, assets[i], targetExt);

		bool sourceExists;
		if (FAILED(hr = getFileExists(sourcePath.data(), &sourceExists))) {
			error = hr;
			printWindowsError(error);
			continue;
		}
		if (!sourceExists) {
			error = ERROR_FILE_NOT_FOUND;
			printWindowsError(error);
			continue;
		}

		bool targetExists;
		if (FAILED(hr = getFileExists(targetPath.data(), &targetExists))) {
			error = hr;
			printWindowsError(error);
			continue;
		}
		if (targetExists) {
			uint64_t sourceTime;
			if (FAILED(hr = getLastWriteTime(sourcePath.data(), &sourceTime))) {
				error = hr;
				printWindowsError(error);
				continue;
			}

			uint64_t targetTime;
			if (FAILED(hr = getLastWriteTime(targetPath.data(), &targetTime))) {
				error = hr;
				printWindowsError(error);
				continue;
			}

			if (targetTime > sourceTime && targetTime > builderTime) {
				continue;
			}
		}

		fprintf(stderr, "%s.%s\n", assets[i], sourceExt);
		if (FAILED(hr = f(sourcePath.data(), targetPath.data()))) {
			error = hr;
			printWindowsError(error);
			continue;
		}
	}

	return error;
}

int main(int argc, const char *argv[]) {
	HRESULT hr;

	std::vector<char> assetDir;
	if ((hr = getEnv("AssetDir", &assetDir)) != S_OK) {
		printWindowsError(hr);
		return 1;
	}

	std::vector<char> dataDir;
	if ((hr = getEnv("DataDir", &dataDir)) != S_OK) {
		printWindowsError(hr);
		return 1;
	}

	if (FAILED(hr = makeDir(dataDir.data()))) {
		printWindowsError(hr);
		return 1;
	}

	uint64_t builderTime;
	if (FAILED(hr = getLastWriteTime(argv[0], &builderTime))) {
		printWindowsError(hr);
		return 1;
	}

	HRESULT error = S_OK;

	const char *vertexShaders[] = { "vertex" };
	auto numVertexShaders = sizeof(vertexShaders) / sizeof(*vertexShaders);
	if (FAILED(hr = build(
		assetDir.data(), dataDir.data(), "hlsl", "cso", builderTime,
		buildVertexShader, vertexShaders, numVertexShaders
	))) {
		error = hr;
	}

	const char *pixelShaders[] = { "pixel" };
	auto numPixelShaders = sizeof(pixelShaders) / sizeof(*pixelShaders);
	if (FAILED(hr = build(
		assetDir.data(), dataDir.data(), "hlsl", "cso", builderTime,
		buildPixelShader, pixelShaders, numPixelShaders
	))) {
		error = hr;
	}

	const char *meshes[] = { "human" };
	auto numMeshes = sizeof(meshes) / sizeof(*meshes);
	if (FAILED(hr = build(
		assetDir.data(), dataDir.data(), "obj", "mesh", builderTime,
		buildMesh, meshes, numMeshes
	))) {
		error = hr;
	}

	return FAILED(hr);
}