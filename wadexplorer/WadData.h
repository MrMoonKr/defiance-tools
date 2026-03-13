#ifndef WADEXPLORER_WADDATA_H
#define WADEXPLORER_WADDATA_H

#include "StdAfx.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

extern "C" {
#include "wadlib.h"
}

struct WadImagePreview
{
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba;
    std::wstring description;
};


struct WadTextureInfo
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipmapCount = 0;
    uint32_t format = 0;
    uint32_t bitsPerPixel = 0;
    bool isCubemap = false;
};

struct WadMeshPreview
{
    uint32_t vertexCount = 0;
    uint32_t triangleCount = 0;
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<uint32_t> indices;
    float centerX = 0.0f;
    float centerY = 0.0f;
    float centerZ = 0.0f;
    float radius = 1.0f;
    std::wstring description;
};


struct WadAssetPayload
{
    std::vector<uint8_t> rawBytes;
    std::vector<uint8_t> viewBytes;
    bool isRmid = false;
    bool wasDecompressed = false;
    uint32_t rmidType = 0;
    uint16_t rmidReferenceCount = 0;
    std::optional<WadTextureInfo> textureInfo;
    std::optional<WadImagePreview> imagePreview;
    std::optional<WadMeshPreview> meshPreview;
};


struct WadAsset
{
    std::filesystem::path wadPath;
    std::wstring wadName;
    uint32_t assetId = 0;
    uint32_t assetType = 0;
    uint64_t dataOffset = 0;
    uint64_t dataSize = 0;
    uint64_t modifiedTime = 0;
    std::string name;
    std::wstring displayName;
    std::vector<std::wstring> treeParts;
};


struct LoadedWadFile
{
    std::filesystem::path path;
    std::wstring displayName;
    std::vector<WadAsset> assets;
    uint64_t totalSize = 0;
};


class WadArchiveModel
{
public:
    WadArchiveModel() = default;

    void Clear();
    void LoadFromFolder(const std::filesystem::path& folderPath);
    void LoadFromFile(const std::filesystem::path& filePath);

    const std::filesystem::path& GetRootPath() const { return m_rootPath; }
    const std::vector<LoadedWadFile>& GetWadFiles() const { return m_wadFiles; }
    bool IsLoaded() const { return !m_wadFiles.empty(); }

    WadAssetPayload ReadAssetPayload(const WadAsset& asset) const;

    static std::wstring AssetTypeName(uint32_t assetType);
    static std::wstring FormatSize(uint64_t size);
    static std::wstring FormatTimestamp(uint64_t unixTime);
    static std::wstring HexDump(const std::vector<uint8_t>& data, size_t maxBytes = 512 * 1024);
    static std::vector<std::wstring> SplitTreeParts(const std::string& assetName);

private:
    void LoadDirectoryInternal(const std::filesystem::path& directoryPath, const std::filesystem::path* singleFileFilter);
    static std::filesystem::path NormalizePath(const std::filesystem::path& pathValue);
    static std::string WideToAnsi(const std::wstring& text);
    static std::wstring AnsiToWide(const std::string& text);
    static std::vector<uint8_t> ReadRawBytes(const WadAsset& asset);
    static std::optional<WadTextureInfo> ExtractTextureInfo(const std::vector<uint8_t>& bytes);
    static std::optional<WadImagePreview> ExtractTexturePreview(const std::vector<uint8_t>& bytes);
    static std::optional<WadMeshPreview> ExtractMeshPreview(const std::vector<uint8_t>& bytes);

    std::filesystem::path m_rootPath;
    std::vector<LoadedWadFile> m_wadFiles;
};

#endif
