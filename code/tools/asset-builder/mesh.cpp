#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "mesh.h"
#include <cstdio>
#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>

struct Vector2 { float x, y; };
struct Vector3 { float x, y, z; };
struct Vector4 { float x, y, z, w; };

struct VertexIndices {
	size_t position;
	size_t normal;
	size_t texcoord;
};

template <>
struct std::hash<VertexIndices> {
	size_t operator()(const VertexIndices &key) const {
#if defined(_WIN64)
	const size_t fnvOffsetBasis = 14695981039346656037ULL;
	const size_t fnvPrime = 1099511628211ULL;
#else
	const size_t fnvOffsetBasis = 2166136261U;
	const size_t fnvPrime = 16777619U;
#endif

		const char *data = (const char*)&key;
		size_t hash = fnvOffsetBasis;
		for (size_t i = 0; i < sizeof(key); i++) {
			hash ^= (size_t)data[i];
			hash *= fnvPrime;
		}
		return hash;
	}
};

bool operator==(const VertexIndices &left, const VertexIndices &right) {
	return memcmp((void*)&left, (void*)&right, sizeof(left)) == 0;
}

struct Vertex {
	Vector3 position;
	Vector3 normal;
	Vector2 texcoord;
};

struct Group {
	std::string name;
	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;
};

HRESULT buildMesh(const char *sourcePath, const char *targetPath) {
	HRESULT hr = S_OK;

	std::vector<Vector3> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;

	std::string groupName;
	std::vector<Group> groups;

	std::unordered_map<VertexIndices, uint16_t> vertexIndices;
	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

	FILE *file = fopen(sourcePath, "rb");
	while (!feof(file)) {
		char line[256] = {};
		fgets(line, sizeof(line), file);
		if (ferror(file)) {
			hr = ERROR_FILE_CORRUPT;
			break;
		}

		if (line[0] == '#' || (line[0] == '\r' && line[1] == '\n') || line[0] == '\n') {
			continue;
		}

#define STR2(x) #x
#define STR(x) STR2(x)

		if (strncmp(line, "mtllib ", 7) == 0) {
			char path[MAX_PATH];
			sscanf(line, "mtllib %" STR(MAX_PATH) "s", path);
		} else if (strncmp(line, "v ", 2) == 0) {
			Vector3 v;
			sscanf(line, "v %f %f %f", &v.x, &v.y, &v.z);
			positions.push_back(v);
		} else if (strncmp(line, "vt ", 3) == 0) {
			Vector2 v;
			sscanf(line, "vt %f %f", &v.x, &v.y);
			texcoords.push_back(v);
		} else if (strncmp(line, "vn ", 3) == 0) {
			Vector3 v;
			sscanf(line, "vn %f %f %f", &v.x, &v.y, &v.z);
			normals.push_back(v);
		} else if (strncmp(line, "g ", 2) == 0) {
			std::string oldName = std::move(groupName);

			char name[256];
			sscanf(line, "g %256s", name);
			groupName = name;

			auto group = Group { std::move(oldName), std::move(vertices), std::move(indices) };
			if (group.vertices.empty()) {
				continue;
			}

			groups.emplace_back(std::move(group));
			vertexIndices.clear();
		} else if (strncmp(line, "usemtl ", 7) == 0) {
			char name[256];
			sscanf(line, "usemtl %256s", name);
		} else if (strncmp(line, "s ", 2) == 0) {
			int group;
			sscanf(line, "s %d", &group);
		} else if (strncmp(line, "f ", 2) == 0) {
			VertexIndices vi[4];
			auto count = sscanf(
				line, "f %zd/%zd/%zd %zd/%zd/%zd %zd/%zd/%zd %zd/%zd/%zd",
				&vi[0].position, &vi[0].texcoord, &vi[0].normal,
				&vi[1].position, &vi[1].texcoord, &vi[1].normal,
				&vi[2].position, &vi[2].texcoord, &vi[2].normal,
				&vi[3].position, &vi[3].texcoord, &vi[3].normal
			);

			auto numVertices = count / 3;
			uint16_t faceIndices[4] = {};
			for (int i = 0; i < numVertices; i++) {
				uint16_t index = 0;

				auto vertex = vertexIndices.find(vi[i]);
				if (vertex != vertexIndices.end()) {
					index = vertex->second;
				} else {
					Vertex v = {};
					v.position = positions[vi[i].position - 1];
					v.normal = normals[vi[i].normal - 1];
					v.texcoord = texcoords[vi[i].texcoord - 1];

					assert(vertices.size() < std::numeric_limits<uint16_t>::max());
					index = (uint16_t)vertices.size();
					vertices.push_back(v);

					vertexIndices[vi[i]] = index;
				}

				faceIndices[i] = index;
			}

			for (int i = 0; i < 3; i++) {
				indices.push_back(faceIndices[i]);
			}
			if (numVertices == 4) {
				for (int i = 2; i < 5; i++) {
					indices.push_back(faceIndices[i % 4]);
				}
			}
		}
	}
	fclose(file);

	positions.clear();
	texcoords.clear();
	normals.clear();

	auto group = Group { std::move(groupName), std::move(vertices), std::move(indices) };
	groups.emplace_back(std::move(group));
	vertexIndices.clear();

	file = fopen(targetPath, "wb");
	if (file == NULL) {
		return ERROR_FILE_NOT_FOUND;
	}

	size_t numGroups = groups.size();
	assert(fwrite(&numGroups, sizeof(numGroups), 1, file) == 1);

	for (auto &group : groups) {
		size_t numVertices = group.vertices.size();
		assert(fwrite(&numVertices, sizeof(numVertices), 1, file) == 1);

		size_t numIndices = group.indices.size();
		assert(fwrite(&numIndices, sizeof(numIndices), 1, file) == 1);
	}

	for (auto &group : groups) {
		size_t numVertices = group.vertices.size();
		assert(fwrite(group.vertices.data(), sizeof(vertices[0]), numVertices, file) == numVertices);

		size_t numIndices = group.indices.size();
		assert(fwrite(group.indices.data(), sizeof(indices[0]), numIndices, file) == numIndices);
	}

	fclose(file);

	return hr;
}
