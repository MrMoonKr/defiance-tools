#include "StdAfx.h"
#include "WadData.h"

#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

extern "C" {
#include "dxt.h"
}

namespace
{
    constexpr uint32_t kRmidMagic = static_cast<uint32_t>('R') |
                                    (static_cast<uint32_t>('M') << 8) |
                                    (static_cast<uint32_t>('I') << 16) |
                                    (static_cast<uint32_t>('D') << 24);

    struct SkiHeader
    {
        uint32_t total_materials;
        uint32_t total_mesh_groups;
        uint32_t mesh_group_table_offset;
        uint32_t unk4;

        uint32_t unk5;
        uint32_t unk6;
        uint32_t unk7;
        uint32_t unk8;

        uint32_t unk9;
        uint32_t unk10;
        uint32_t unk11;
        uint32_t unk12;

        uint64_t mesh_table_offset;
        uint64_t mesh_material_ids_offset;
        uint64_t material_table_offset;
        uint64_t unk13;

        uint32_t unk21;
        uint32_t unk22;
        uint32_t unk23;
        uint32_t unk24;
    };

    struct SkiMeshGroupRecord
    {
        uint32_t total_meshes;
        uint32_t unk2;
        uint32_t unk3;
        uint32_t unk4;

        uint32_t unk5;
        uint32_t unk6;
        uint32_t unk7;
        uint32_t unk8;
    };

    std::vector<uint8_t> CropRgbaBuffer(
        const uint8_t* source,
        uint32_t sourceWidth,
        uint32_t sourceHeight,
        uint32_t targetWidth,
        uint32_t targetHeight)
    {
        const uint32_t copyWidth = std::min(sourceWidth, targetWidth);
        const uint32_t copyHeight = std::min(sourceHeight, targetHeight);
        std::vector<uint8_t> result(static_cast<size_t>(targetWidth) * targetHeight * 4, 0);

        for (uint32_t y = 0; y < copyHeight; ++y)
        {
            const auto* sourceRow = source + static_cast<size_t>(y) * sourceWidth * 4;
            auto* destRow = result.data() + static_cast<size_t>(y) * targetWidth * 4;
            std::memcpy(destRow, sourceRow, static_cast<size_t>(copyWidth) * 4);
        }

        return result;
    }

    std::wstring SanitizeComponent(const std::wstring& value)
    {
        std::wstring result;
        result.reserve(value.size());
        for (wchar_t ch : value)
        {
            switch (ch)
            {
            case L'<':
            case L'>':
            case L':':
            case L'"':
            case L'|':
            case L'?':
            case L'*':
                result.push_back(L'_');
                break;
            default:
                if (ch < 32)
                    result.push_back(L'_');
                else
                    result.push_back(ch);
                break;
            }
        }

        if (result.empty())
            result = L"_";
        return result;
    }

    std::wstring JoinTreeParts(const std::vector<std::wstring>& parts, size_t count)
    {
        std::wstring joined;
        for (size_t index = 0; index < count; ++index)
        {
            if (index > 0)
                joined += L"/";
            joined += parts[index];
        }
        return joined;
    }

    float HalfToFloat(uint16_t value)
    {
        const float sign = (value & 0x8000U) ? -1.0f : 1.0f;
        const uint32_t exponent = (value >> 10) & 0x1FU;
        const uint32_t fraction = value & 0x03FFU;

        if (exponent == 0)
        {
            if (fraction == 0)
                return sign * 0.0f;
            return sign * std::ldexp(static_cast<float>(fraction), -24);
        }

        if (exponent == 31)
        {
            if (fraction == 0)
                return sign * std::numeric_limits<float>::infinity();
            return std::numeric_limits<float>::quiet_NaN();
        }

        return sign * std::ldexp(static_cast<float>(fraction + 1024), static_cast<int>(exponent) - 25);
    }

    void NormalizeVector(float& x, float& y, float& z)
    {
        const float length = std::sqrt((x * x) + (y * y) + (z * z));
        if (length > 1e-6f)
        {
            x /= length;
            y /= length;
            z /= length;
        }
    }

    bool AppendMeshPreviewRange(
        WadMeshPreview& preview,
        const uint8_t* base,
        size_t baseSize,
        const mes_ski_mesh_record* meshRecords,
        uint32_t meshStart,
        uint32_t meshCount,
        uint32_t& appendedMeshes)
    {
        for (uint32_t meshListIndex = 0; meshListIndex < meshCount; ++meshListIndex)
        {
            const uint32_t meshIndex = meshStart + meshListIndex;
            const auto& meshRecord = meshRecords[meshIndex];
            if (meshRecord.offset + sizeof(mes_ski_mesh_header) > baseSize)
                continue;

            const auto* meshHeader = reinterpret_cast<const mes_ski_mesh_header*>(base + meshRecord.offset);
            if (meshHeader->num_vertices1 == 0 || meshHeader->num_indices1 < 3 || meshHeader->bytes_per_vertex == 0)
                continue;

            if (meshHeader->vertex_data_offset >= meshRecord.size || meshHeader->index_data_offset >= meshRecord.size)
                continue;

            const size_t vertexDataOffset = static_cast<size_t>(meshRecord.offset + meshHeader->vertex_data_offset);
            const size_t indexDataOffset = static_cast<size_t>(meshRecord.offset + meshHeader->index_data_offset);
            const size_t vertexBytes = static_cast<size_t>(meshHeader->num_vertices1) * meshHeader->bytes_per_vertex;
            if (vertexDataOffset + vertexBytes > baseSize || indexDataOffset > baseSize)
                continue;

            const auto* vertexData = base + vertexDataOffset;
            const auto* indexData = base + indexDataOffset;
            const size_t recordIndexBytes = static_cast<size_t>(meshRecord.size - meshHeader->index_data_offset);
            const size_t fileIndexBytes = baseSize - indexDataOffset;
            const size_t availableIndexBytes = std::min(recordIndexBytes, fileIndexBytes);
            const bool use32BitIndices =
                availableIndexBytes >= static_cast<size_t>(meshHeader->num_indices1) * sizeof(uint32_t);
            const bool use16BitIndices = !use32BitIndices &&
                availableIndexBytes >= static_cast<size_t>(meshHeader->num_indices1) * sizeof(uint16_t);
            if (!use32BitIndices && !use16BitIndices)
                continue;

            const uint32_t vertexOffset = preview.vertexCount;
            preview.positions.reserve(preview.positions.size() + (static_cast<size_t>(meshHeader->num_vertices1) * 3));
            preview.normals.reserve(preview.normals.size() + (static_cast<size_t>(meshHeader->num_vertices1) * 3));

            for (uint32_t vertexIndex = 0; vertexIndex < meshHeader->num_vertices1; ++vertexIndex)
            {
                const auto* vertex = vertexData + (static_cast<size_t>(vertexIndex) * meshHeader->bytes_per_vertex);

                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;
                size_t offset = 0;

                if ((meshHeader->vertex_format & UNCOMPRESSED) != 0)
                {
                    x = *reinterpret_cast<const float*>(vertex + 0);
                    y = *reinterpret_cast<const float*>(vertex + 4);
                    z = *reinterpret_cast<const float*>(vertex + 8);
                    offset = 12;
                }
                else
                {
                    x = HalfToFloat(*reinterpret_cast<const uint16_t*>(vertex + 0));
                    y = HalfToFloat(*reinterpret_cast<const uint16_t*>(vertex + 2));
                    z = HalfToFloat(*reinterpret_cast<const uint16_t*>(vertex + 4));
                    offset = 8;
                }

                float nx = 0.0f;
                float ny = 1.0f;
                float nz = 0.0f;
                if ((meshHeader->vertex_format & NORMAL) != 0 && offset + 12 <= meshHeader->bytes_per_vertex)
                {
                    nx = *reinterpret_cast<const float*>(vertex + offset + 0);
                    ny = *reinterpret_cast<const float*>(vertex + offset + 4);
                    nz = *reinterpret_cast<const float*>(vertex + offset + 8);
                    NormalizeVector(nx, ny, nz);
                }

                preview.positions.push_back(x);
                preview.positions.push_back(y);
                preview.positions.push_back(z);
                preview.normals.push_back(nx);
                preview.normals.push_back(ny);
                preview.normals.push_back(nz);
            }

            for (uint32_t faceIndex = 0; faceIndex + 2 < meshHeader->num_indices1; faceIndex += 3)
            {
                uint32_t v1 = 0;
                uint32_t v2 = 0;
                uint32_t v3 = 0;

                if (use32BitIndices)
                {
                    const auto* face = reinterpret_cast<const mes_face_32*>(indexData);
                    v1 = face[faceIndex / 3].v1;
                    v2 = face[faceIndex / 3].v2;
                    v3 = face[faceIndex / 3].v3;
                }
                else
                {
                    const auto* face = reinterpret_cast<const mes_face_16*>(indexData);
                    v1 = face[faceIndex / 3].v1;
                    v2 = face[faceIndex / 3].v2;
                    v3 = face[faceIndex / 3].v3;
                }

                if (v1 >= meshHeader->num_vertices1 || v2 >= meshHeader->num_vertices1 || v3 >= meshHeader->num_vertices1)
                    continue;

                preview.indices.push_back(vertexOffset + v3);
                preview.indices.push_back(vertexOffset + v2);
                preview.indices.push_back(vertexOffset + v1);
            }

            preview.vertexCount += meshHeader->num_vertices1;
            ++appendedMeshes;
        }

        return appendedMeshes != 0;
    }
}

void WadArchiveModel::Clear()
{
    m_rootPath.clear();
    m_wadFiles.clear();
}

void WadArchiveModel::LoadFromFolder(const std::filesystem::path& folderPath)
{
    LoadDirectoryInternal(folderPath, nullptr);
}

void WadArchiveModel::LoadFromFile(const std::filesystem::path& filePath)
{
    auto normalized = NormalizePath(filePath);
    auto parent = normalized.parent_path();
    LoadDirectoryInternal(parent, &normalized);
    m_rootPath = normalized;
}

void WadArchiveModel::LoadDirectoryInternal(const std::filesystem::path& directoryPath, const std::filesystem::path* singleFileFilter)
{
    Clear();

    const auto normalizedDir = NormalizePath(directoryPath);
    if (!std::filesystem::exists(normalizedDir) || !std::filesystem::is_directory(normalizedDir))
        throw std::runtime_error("WAD directory not found.");

    wad_dir wd{};
    const std::string narrowDir = WideToAnsi(normalizedDir.wstring());
    if (WadDirLoad(&wd, narrowDir.c_str()) != 0)
        throw std::runtime_error("WadDirLoad failed.");

    try
    {
        for (uint32_t fileIndex = 0; fileIndex < wd.total_files; ++fileIndex)
        {
            wad_file& wf = wd.files[fileIndex];
            const std::filesystem::path wadPath = NormalizePath(AnsiToWide(wf.filename));
            if (singleFileFilter && wadPath != *singleFileFilter)
                continue;

            LoadedWadFile loaded{};
            loaded.path = wadPath;
            loaded.displayName = wadPath.filename().wstring();

            for (uint32_t recordIndex = 0; recordIndex < wf.total_records; ++recordIndex)
            {
                wad_record& wr = wf.records[recordIndex];
                if (WadRecordResolveName(&wr) != 0)
                    continue;

                WadAsset asset{};
                asset.wadPath = wadPath;
                asset.wadName = wadPath.stem().wstring();
                asset.assetId = wr.id;
                asset.assetType = wr.type;
                asset.dataOffset = wr.data_offset;
                asset.dataSize = wr.data_size;
                asset.modifiedTime = wr.modified_time;
                asset.name = wr.name ? wr.name : "";
                asset.displayName = AnsiToWide(asset.name);
                asset.treeParts = SplitTreeParts(asset.name);

                loaded.totalSize += asset.dataSize;
                loaded.assets.push_back(std::move(asset));
            }

            std::sort(
                loaded.assets.begin(),
                loaded.assets.end(),
                [](const WadAsset& left, const WadAsset& right)
                {
                    return _wcsicmp(left.displayName.c_str(), right.displayName.c_str()) < 0;
                });

            m_wadFiles.push_back(std::move(loaded));
        }
    }
    catch (...)
    {
        WadDirFree(&wd);
        throw;
    }

    WadDirFree(&wd);
    m_rootPath = normalizedDir;
}

std::filesystem::path WadArchiveModel::NormalizePath(const std::filesystem::path& pathValue)
{
    std::error_code ec;
    auto normalized = std::filesystem::weakly_canonical(pathValue, ec);
    return ec ? pathValue.lexically_normal() : normalized;
}

std::string WadArchiveModel::WideToAnsi(const std::wstring& text)
{
    if (text.empty())
        return {};

    const int sizeNeeded = ::WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 0)
        throw std::runtime_error("WideCharToMultiByte failed.");

    std::string buffer(static_cast<size_t>(sizeNeeded), '\0');
    ::WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, buffer.data(), sizeNeeded, nullptr, nullptr);
    std::string result(buffer.c_str());
    return result;
}

std::wstring WadArchiveModel::AnsiToWide(const std::string& text)
{
    if (text.empty())
        return {};

    const int sizeNeeded = ::MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, nullptr, 0);
    if (sizeNeeded <= 0)
        return std::wstring(text.begin(), text.end());

    std::wstring buffer(static_cast<size_t>(sizeNeeded), L'\0');
    ::MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, buffer.data(), sizeNeeded);
    std::wstring result(buffer.c_str());
    return result;
}

std::vector<uint8_t> WadArchiveModel::ReadRawBytes(const WadAsset& asset)
{
    std::ifstream stream(asset.wadPath, std::ios::binary);
    if (!stream)
        throw std::runtime_error("Failed to open WAD file for reading.");

    stream.seekg(static_cast<std::streamoff>(asset.dataOffset), std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(asset.dataSize));
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream && !bytes.empty())
        throw std::runtime_error("Failed to read asset bytes.");

    return bytes;
}

std::optional<WadTextureInfo> WadArchiveModel::ExtractTextureInfo(const std::vector<uint8_t>& bytes)
{
    if (bytes.size() < sizeof(rmid_header) + sizeof(rmid_tex_header))
        return std::nullopt;

    const auto* header = reinterpret_cast<const rmid_header*>(bytes.data());
    if (header->magic != kRmidMagic || header->type != RMID_TYPE_TEX)
        return std::nullopt;

    const auto* textureHeader = reinterpret_cast<const rmid_tex_header*>(bytes.data() + sizeof(rmid_header));
    WadTextureInfo info{};
    info.width = textureHeader->mmh1.width;
    info.height = textureHeader->mmh1.height;
    info.mipmapCount = textureHeader->mmh1.mipmap_count;
    info.format = textureHeader->format;
    info.bitsPerPixel = textureHeader->bits_per_pixel;
    info.isCubemap = textureHeader->unk2 == 1;
    return info;
}

std::optional<WadImagePreview> WadArchiveModel::ExtractTexturePreview(const std::vector<uint8_t>& bytes)
{
    if (bytes.size() < sizeof(rmid_header) + sizeof(rmid_tex_header))
        return std::nullopt;

    const auto* header = reinterpret_cast<const rmid_header*>(bytes.data());
    if (header->magic != kRmidMagic || header->type != RMID_TYPE_TEX)
        return std::nullopt;

    const auto* textureHeader = reinterpret_cast<const rmid_tex_header*>(bytes.data() + sizeof(rmid_header));
    const auto* body = bytes.data() + sizeof(rmid_header) + sizeof(rmid_tex_header);
    const size_t bodySize = bytes.size() - sizeof(rmid_header) - sizeof(rmid_tex_header);

    WadImagePreview preview{};
    preview.width = textureHeader->mmh1.width;
    preview.height = textureHeader->mmh1.height;

    std::wostringstream description;
    description << L"Texture " << preview.width << L"x" << preview.height
                << L", format=" << static_cast<unsigned int>(textureHeader->format)
                << L", mipmaps=" << textureHeader->mmh1.mipmap_count;
    if (textureHeader->unk2 == 1)
        description << L", cubemap";
    preview.description = description.str();

    if (preview.width == 0 || preview.height == 0)
        return std::nullopt;

    if (textureHeader->format == 0)
    {
        const size_t required = static_cast<size_t>(preview.width) * preview.height * 4;
        if (bodySize < required)
            return std::nullopt;

        preview.rgba.assign(body, body + required);
        return preview;
    }

    if (textureHeader->format == 6 && textureHeader->bits_per_pixel == 64)
    {
        const size_t pixelCount = static_cast<size_t>(preview.width) * preview.height;
        const size_t required = pixelCount * 8;
        if (bodySize < required)
            return std::nullopt;

        preview.rgba.resize(pixelCount * 4);
        for (size_t index = 0; index < pixelCount; ++index)
        {
            const size_t source = index * 8;
            const size_t target = index * 4;
            preview.rgba[target + 0] = body[source + 1];
            preview.rgba[target + 1] = body[source + 3];
            preview.rgba[target + 2] = body[source + 5];
            preview.rgba[target + 3] = body[source + 7];
        }
        return preview;
    }

    if (textureHeader->format == 1 || textureHeader->format == 3 || textureHeader->format == 8)
    {
        const uint32_t decodeWidth = std::max<uint32_t>(preview.width, 4);
        const uint32_t decodeHeight = std::max<uint32_t>(preview.height, 4);
        const size_t blockSize = (textureHeader->format == 1) ? 8 : 16;
        const size_t required =
            static_cast<size_t>((decodeWidth + 3) / 4) * ((decodeHeight + 3) / 4) * blockSize;
        if (bodySize < required)
            return std::nullopt;

        std::vector<uint32_t> pixels32(static_cast<size_t>(decodeWidth) * decodeHeight, 0);
        if (textureHeader->format == 1)
            DecompressDXT1(decodeWidth, decodeHeight, const_cast<uint8_t*>(body), pixels32.data());
        else
            DecompressDXT5(decodeWidth, decodeHeight, const_cast<uint8_t*>(body), pixels32.data());

        const auto* sourceBytes = reinterpret_cast<const uint8_t*>(pixels32.data());
        preview.rgba = CropRgbaBuffer(sourceBytes, decodeWidth, decodeHeight, preview.width, preview.height);
        return preview;
    }

    return std::nullopt;
}

std::optional<WadMeshPreview> WadArchiveModel::ExtractMeshPreview(const std::vector<uint8_t>& bytes)
{
    if (bytes.size() < sizeof(rmid_header) + sizeof(mes_ski_header))
        return std::nullopt;

    const auto* header = reinterpret_cast<const rmid_header*>(bytes.data());
    if (header->magic != kRmidMagic || (header->type != RMID_TYPE_MES && header->type != RMID_TYPE_SKI))
        return std::nullopt;

    const auto* base = bytes.data();
    const size_t baseSize = bytes.size();
    WadMeshPreview preview{};
    uint32_t appendedMeshes = 0;
    std::wstring previewLabel = L"Static mesh";

    if (header->type == RMID_TYPE_MES)
    {
        const auto* meshFileHeader = reinterpret_cast<const mes_ski_header*>(base + sizeof(rmid_header));
        if (meshFileHeader->mesh_table_offset +
                (static_cast<uint64_t>(meshFileHeader->total_meshes) * sizeof(mes_ski_mesh_record)) > baseSize)
            return std::nullopt;

        const auto* meshRecords =
            reinterpret_cast<const mes_ski_mesh_record*>(base + meshFileHeader->mesh_table_offset);
        AppendMeshPreviewRange(preview, base, baseSize, meshRecords, 0, meshFileHeader->total_meshes, appendedMeshes);
    }
    else
    {
        if (bytes.size() < sizeof(rmid_header) + sizeof(SkiHeader))
            return std::nullopt;

        const auto* skinHeader = reinterpret_cast<const SkiHeader*>(base + sizeof(rmid_header));
        if (skinHeader->total_mesh_groups == 0)
            return std::nullopt;

        if (skinHeader->mesh_group_table_offset +
                (static_cast<uint64_t>(skinHeader->total_mesh_groups) * sizeof(SkiMeshGroupRecord)) > baseSize)
            return std::nullopt;

        const auto* meshGroups =
            reinterpret_cast<const SkiMeshGroupRecord*>(base + skinHeader->mesh_group_table_offset);
        const uint32_t lodMeshCount = meshGroups[0].total_meshes;
        if (lodMeshCount == 0)
            return std::nullopt;

        if (skinHeader->mesh_table_offset +
                (static_cast<uint64_t>(lodMeshCount) * sizeof(mes_ski_mesh_record)) > baseSize)
            return std::nullopt;

        const auto* meshRecords =
            reinterpret_cast<const mes_ski_mesh_record*>(base + skinHeader->mesh_table_offset);
        AppendMeshPreviewRange(preview, base, baseSize, meshRecords, 0, lodMeshCount, appendedMeshes);
        previewLabel = L"Skin mesh (LOD 0)";
    }

    if (preview.positions.empty() || preview.indices.empty())
        return std::nullopt;

    preview.triangleCount = static_cast<uint32_t>(preview.indices.size() / 3);

    float minX = preview.positions[0];
    float minY = preview.positions[1];
    float minZ = preview.positions[2];
    float maxX = minX;
    float maxY = minY;
    float maxZ = minZ;
    for (size_t index = 0; index < preview.positions.size(); index += 3)
    {
        minX = std::min(minX, preview.positions[index + 0]);
        minY = std::min(minY, preview.positions[index + 1]);
        minZ = std::min(minZ, preview.positions[index + 2]);
        maxX = std::max(maxX, preview.positions[index + 0]);
        maxY = std::max(maxY, preview.positions[index + 1]);
        maxZ = std::max(maxZ, preview.positions[index + 2]);
    }

    preview.centerX = (minX + maxX) * 0.5f;
    preview.centerY = (minY + maxY) * 0.5f;
    preview.centerZ = (minZ + maxZ) * 0.5f;

    float maxDistanceSq = 0.0f;
    for (size_t index = 0; index < preview.positions.size(); index += 3)
    {
        const float dx = preview.positions[index + 0] - preview.centerX;
        const float dy = preview.positions[index + 1] - preview.centerY;
        const float dz = preview.positions[index + 2] - preview.centerZ;
        maxDistanceSq = std::max(maxDistanceSq, (dx * dx) + (dy * dy) + (dz * dz));
    }
    preview.radius = std::max(0.5f, std::sqrt(maxDistanceSq));

    bool needsGeneratedNormals = true;
    for (size_t index = 0; index < preview.normals.size(); index += 3)
    {
        const float nx = preview.normals[index + 0];
        const float ny = preview.normals[index + 1];
        const float nz = preview.normals[index + 2];
        if (std::fabs(nx) > 1e-4f || std::fabs(ny - 1.0f) > 1e-4f || std::fabs(nz) > 1e-4f)
        {
            needsGeneratedNormals = false;
            break;
        }
    }

    if (needsGeneratedNormals)
    {
        std::fill(preview.normals.begin(), preview.normals.end(), 0.0f);
        for (size_t index = 0; index + 2 < preview.indices.size(); index += 3)
        {
            const uint32_t i0 = preview.indices[index + 0];
            const uint32_t i1 = preview.indices[index + 1];
            const uint32_t i2 = preview.indices[index + 2];

            const float ax = preview.positions[static_cast<size_t>(i0) * 3 + 0];
            const float ay = preview.positions[static_cast<size_t>(i0) * 3 + 1];
            const float az = preview.positions[static_cast<size_t>(i0) * 3 + 2];
            const float bx = preview.positions[static_cast<size_t>(i1) * 3 + 0];
            const float by = preview.positions[static_cast<size_t>(i1) * 3 + 1];
            const float bz = preview.positions[static_cast<size_t>(i1) * 3 + 2];
            const float cx = preview.positions[static_cast<size_t>(i2) * 3 + 0];
            const float cy = preview.positions[static_cast<size_t>(i2) * 3 + 1];
            const float cz = preview.positions[static_cast<size_t>(i2) * 3 + 2];

            const float ux = bx - ax;
            const float uy = by - ay;
            const float uz = bz - az;
            const float vx = cx - ax;
            const float vy = cy - ay;
            const float vz = cz - az;

            float nx = (uy * vz) - (uz * vy);
            float ny = (uz * vx) - (ux * vz);
            float nz = (ux * vy) - (uy * vx);
            NormalizeVector(nx, ny, nz);

            for (uint32_t vertex : {i0, i1, i2})
            {
                preview.normals[static_cast<size_t>(vertex) * 3 + 0] += nx;
                preview.normals[static_cast<size_t>(vertex) * 3 + 1] += ny;
                preview.normals[static_cast<size_t>(vertex) * 3 + 2] += nz;
            }
        }

        for (size_t index = 0; index < preview.normals.size(); index += 3)
            NormalizeVector(preview.normals[index + 0], preview.normals[index + 1], preview.normals[index + 2]);
    }

    std::wostringstream description;
    description << previewLabel
                << L"\r\nVertices: " << preview.vertexCount
                << L"\r\nTriangles: " << preview.triangleCount
                << L"\r\nSubmeshes: " << appendedMeshes
                << L"\r\nRadius: " << std::fixed << std::setprecision(2) << preview.radius;
    preview.description = description.str();
    return preview;
}

WadAssetPayload WadArchiveModel::ReadAssetPayload(const WadAsset& asset) const
{
    WadAssetPayload payload{};
    payload.rawBytes = ReadRawBytes(asset);
    payload.viewBytes = payload.rawBytes;

    rmid_file rf{};
    const auto narrowPath = WideToAnsi(asset.wadPath.wstring());
    if (RmidLoadFromFile(narrowPath.c_str(), asset.dataOffset, asset.dataSize, &rf) == 0)
    {
        payload.isRmid = rf.header != nullptr;
        payload.wasDecompressed = rf.size != asset.dataSize;
        payload.rmidType = rf.header ? rf.header->type : 0;
        payload.rmidReferenceCount = rf.header ? rf.header->num_references : 0;

        payload.viewBytes.assign(
            static_cast<const uint8_t*>(rf.data),
            static_cast<const uint8_t*>(rf.data) + static_cast<size_t>(rf.size));
        payload.textureInfo = ExtractTextureInfo(payload.viewBytes);
        payload.imagePreview = ExtractTexturePreview(payload.viewBytes);
        payload.meshPreview = ExtractMeshPreview(payload.viewBytes);
        RmidFree(&rf);
    }

    return payload;
}

std::wstring WadArchiveModel::AssetTypeName(uint32_t assetType)
{
    switch (assetType)
    {
    case RMID_TYPE_RAW: return L"Raw";
    case RMID_TYPE_SHD: return L"Shader";
    case RMID_TYPE_TEX: return L"Texture";
    case RMID_TYPE_MES: return L"Mesh";
    case RMID_TYPE_SKI: return L"Skin";
    case RMID_TYPE_ACT: return L"Actor";
    case RMID_TYPE_SKE: return L"Skeleton";
    case RMID_TYPE_ANI: return L"Animation";
    case RMID_TYPE_SND: return L"Sound";
    case RMID_TYPE_MOV: return L"Movie";
    case RMID_TYPE_SPK: return L"Speech";
    case RMID_TYPE_CON: return L"Container";
    case RMID_TYPE_LIP: return L"LipSync";
    default:
    {
        std::wostringstream stream;
        stream << L"0x" << std::uppercase << std::hex << assetType;
        return stream.str();
    }
    }
}

std::wstring WadArchiveModel::FormatSize(uint64_t size)
{
    double value = static_cast<double>(size);
    const std::array<const wchar_t*, 4> units = {L"B", L"KB", L"MB", L"GB"};
    size_t unitIndex = 0;
    while (value >= 1024.0 && unitIndex + 1 < units.size())
    {
        value /= 1024.0;
        ++unitIndex;
    }

    std::wostringstream stream;
    if (unitIndex == 0)
        stream << static_cast<uint64_t>(value) << L' ' << units[unitIndex];
    else
        stream << std::fixed << std::setprecision(1) << value << L' ' << units[unitIndex];
    return stream.str();
}

std::wstring WadArchiveModel::FormatTimestamp(uint64_t unixTime)
{
    if (unixTime == 0)
        return L"0";

    std::time_t rawTime = static_cast<std::time_t>(unixTime);
    std::tm timeInfo{};
    if (localtime_s(&timeInfo, &rawTime) != 0)
        return std::to_wstring(unixTime);

    wchar_t buffer[64] = {};
    if (wcsftime(buffer, std::size(buffer), L"%Y-%m-%d %H:%M:%S", &timeInfo) == 0)
        return std::to_wstring(unixTime);
    return buffer;
}

std::vector<std::wstring> WadArchiveModel::SplitTreeParts(const std::string& assetName)
{
    std::wstring normalized = AnsiToWide(assetName);
    std::replace(normalized.begin(), normalized.end(), L'\\', L'/');

    std::vector<std::wstring> pathParts;
    std::wstringstream pathStream(normalized);
    std::wstring part;
    while (std::getline(pathStream, part, L'/'))
    {
        if (!part.empty() && part != L".")
            pathParts.push_back(part);
    }

    if (pathParts.empty())
        return {L"unnamed"};

    std::vector<std::wstring> treeParts;
    for (const auto& pathPart : pathParts)
    {
        std::wstringstream tokenStream(pathPart);
        std::vector<std::wstring> tokens;
        std::wstring token;
        while (std::getline(tokenStream, token, L'_'))
        {
            if (!token.empty())
                tokens.push_back(token);
        }

        if (tokens.size() >= 3)
        {
            treeParts.push_back(SanitizeComponent(tokens[1]));
            std::wstring tail = tokens[2];
            for (size_t index = 3; index < tokens.size(); ++index)
            {
                tail += L"_";
                tail += tokens[index];
            }
            treeParts.push_back(SanitizeComponent(tail));
        }
        else
        {
            treeParts.push_back(SanitizeComponent(pathPart));
        }
    }

    return treeParts.empty() ? std::vector<std::wstring>{L"unnamed"} : treeParts;
}

std::wstring WadArchiveModel::HexDump(const std::vector<uint8_t>& data, size_t maxBytes)
{
    const size_t visibleBytes = std::min(data.size(), maxBytes);
    std::ostringstream stream;
    stream << std::uppercase << std::hex << std::setfill('0');

    for (size_t offset = 0; offset < visibleBytes; offset += 16)
    {
        stream << std::setw(8) << offset << "  ";

        for (size_t index = 0; index < 16; ++index)
        {
            if (index == 8)
                stream << " ";

            if (offset + index < visibleBytes)
                stream << std::setw(2) << static_cast<unsigned int>(data[offset + index]) << ' ';
            else
                stream << "   ";
        }

        stream << " ";
        for (size_t index = 0; index < 16 && offset + index < visibleBytes; ++index)
        {
            const uint8_t ch = data[offset + index];
            stream << ((ch >= 32 && ch < 127) ? static_cast<char>(ch) : '.');
        }

        stream << '\n';
    }

    if (data.size() > maxBytes)
    {
        stream << "\n... truncated: showing " << visibleBytes << " of " << data.size() << " bytes ...\n";
    }

    const std::string text = stream.str();
    return std::wstring(text.begin(), text.end());
}
