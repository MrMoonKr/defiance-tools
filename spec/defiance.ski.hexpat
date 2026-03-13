#pragma description Defiance RMID Skin (.ski)
#pragma endian little

import std.core;
import std.string;
import std.mem;
import type.float16;

struct float2 {
    float x, y ;
};

struct float3 {
    float x, y, z ;
};

struct float4 {
    float x, y, z, w ;
};

struct float4x4 {
    float4 r1, r2, r3, r4 ;
};

struct half2 {
    type::float16  x, y;
};

struct half3 {
    type::float16 x, y, z ;
};

struct half4 {
    type::float16 x, y, z, w ;
};

enum RmidType : u16 {
    Raw         = 0x0001,
    Shader      = 0x0002,
    Texture     = 0x0003,
    Mesh        = 0x0004,
    Skin        = 0x0005,
    Actor       = 0x0006,
    Skeleton    = 0x0007,
    Animation   = 0x0008,
    Sound       = 0x0009,
    Movie       = 0x0011,
    Speech      = 0x0012,
    Container   = 0x0016,
    LipSync     = 0x0023
};

struct Blob<auto Size> {
    u8 bytes[Size];
} [[sealed]];

bitfield VertexFormatFlags {
    uncompressed    : 1;
    unk2            : 1;
    unk3            : 1;
    normal          : 1;
    tangent         : 1;
    bitangent       : 1;
    unknown4        : 1;
    bit8            : 1;
    reserved        : 8;
};

struct RmidHeader {
    u32         m_ID;
    u16         m_Version;
    u16         m_Minor;
    u16         m_ReferenceCount;
    RmidType    m_Type;
    char        m_Magic[4];
    
} [[name(RmidHeader_Name(this))]];

fn RmidHeader_Name(auto data) {
    return std::format("RMID (Type:{})(Refs:{})(Ver{}.{})", 
        data.m_Type, 
        data.m_ReferenceCount,
        data.m_Version,
        data.m_Minor 
    );
};

struct SkiMeshGroupRecord {
    u32 m_TotalMeshes;
    u32 m_Unk2;
    u32 m_Unk3;
    u32 m_Unk4;
    u32 m_Unk5;
    u32 m_Unk6;
    u32 m_Unk7;
    u32 m_Unk8;
    
} [[name(SkiMeshGroupRecord_Name(this))]] ;

fn SkiMeshGroupRecord_Name( auto data ) {
    return std::format("MeshGroup (Meshes:{})", 
        data.m_TotalMeshes
    ) ;
} ;


struct SkiMaterialHeader {
    u32 m_Unk1;
    u32 m_Unk2;
    u32 m_ShaderID;
    u32 m_Unk4;

    u32 m_Unk5;
    u32 m_Unk6;
    u32 m_Unk7;
    u32 m_Unk8;

    u32 m_TotalMaterialParams;
    u32 m_Unk9;
    u32 m_HeaderSize ;
    u32 m_Unk11;

    u32 m_MaterialSize;
    u32 m_Unk13;
    u32 m_Unk14;
    u32 m_Unk15;

    u32 m_Unk16;
    u32 m_Unk17;
    u32 m_Unk18;
    u32 m_Unk19;
    
    //Blob< m_MaterialSize - 16*5 > m_Data;
    
} [[name(SkiMaterialHeader_Name(this)), sealed]];

fn SkiMaterialHeader_Name(auto data) {
    return std::format("MaterialHeader (Shader:0x{:08X})(Params:{})(HeaderSize:{})",
        data.m_ShaderID ,
        data.m_TotalMaterialParams ,
        data.m_HeaderSize 
        );
};

struct SkiMaterialParam {
    u32 m_Unk1;
    u32 m_Unk2;
    u32 m_Unk3;
    u32 m_Unk4;

    u32 m_ParamType;
    u32 m_TextureID;
    u32 m_Unk7;
    u32 m_Unk8;

    u32 m_Unk9;
    u32 m_Unk10;
    u32 m_Unk11;
    u32 m_Unk12;

    u32 m_Unk13;
    u32 m_Unk14;
    u32 m_Unk15;
    u32 m_Unk16;

    u32 m_Unk17;
    u32 m_Unk18;
    u32 m_Unk19;
    u32 m_Unk20;

    u32 m_Unk21;
    u32 m_Unk22;
    u32 m_Unk23;
    u32 m_Unk24;

    u32 m_Unk25;
    u32 m_Unk26;
    u32 m_Unk27;
    u32 m_Unk28;

    u32 m_Unk29;
    u32 m_Unk30;
    u32 m_Unk31;
    u32 m_Unk32;
    
} [[name(SkiMaterialParam_Name(this)),sealed]];

fn SkiMaterialParam_Name( auto data ) {
    return std::format("Mat Param (Type:0x{:08X})(Tex:0x{:08X})" ,
        data.m_ParamType ,
        data.m_TextureID 
    );
};

struct SkiMaterialRecord {
    u64 m_Offset;
    u64 m_Size;

    SkiMaterialHeader m_Header @ m_Offset ;
    /*if ( m_Offset != 0 && m_Size >= sizeof(SkiMaterialHeader) ) {
        SkiMaterialHeader m_Header @ m_Offset;
    }*/
    SkiMaterialParam m_Params[m_Header.m_TotalMaterialParams] @ m_Offset + 16*5 ;
    /*if (m_Offset != 0 && m_Size >= sizeof(SkiMaterialHeader)) {
        SkiMaterialParam m_Params[m_Header.m_TotalMaterialParams] @ (m_Offset + sizeof(SkiMaterialHeader));
    }*/
    
} [[name(SkiMaterialRecord_Name(this))]];

fn SkiMaterialRecord_Name(auto data) {
    return std::format("Material (Off:0x{:X})(Size:0x{:X})",
        data.m_Offset,
        data.m_Size);
};

struct SkiMeshHeader {
    u32                 m_Unk1;
    u32                 m_Unk2;
    u32                 m_Unk3;
    u32                 m_Unk4;

    u32                 m_Unk5;
    u32                 m_Unk6;
    u32                 m_Unk7;
    u32                 m_Unk8;

    u32                 m_Unk9;
    u32                 m_Unk10;
    u32                 m_Unk11;
    u32                 m_Unk12;

    u32                 m_Unk13;
    u32                 m_Unk14;
    u32                 m_Unk15;
    u32                 m_Unk16;

    u32                 m_Unk17;
    u32                 m_Unk18;
    u32                 m_Unk19;
    u32                 m_Unk20;

    VertexFormatFlags   m_VertexFormat;
    u8                  m_UncompressedTexcoordFlags;
    u16                 m_Unk21;
    u8                  m_CompressedTexcoordFlags;
    u16                 m_Unk22;
    u32                 m_Unk23;
    u32                 m_Unk24;

    u32                 m_BytesPerVertex;
    u32                 m_Unk26;
    u32                 m_Unk27;
    u32                 m_Unk28;

    u32                 m_NumVertices1;
    u32                 m_NumIndices1;
    u32                 m_NumVertices2;
    u32                 m_NumIndices2;

    u32                 m_Unk33;
    u32                 m_Unk34;
    u32                 m_Unk35;
    u32                 m_Unk36;

    u32                 m_Unk37;
    u32                 m_Unk38;
    u32                 m_Unk39;
    u32                 m_Unk40;

    u32                 m_Unk41;
    u32                 m_Unk42;
    u32                 m_Unk43;
    u32                 m_Unk44;

    u32                 m_Unk45;
    u32                 m_Unk46;
    u64                 m_MeshInfoOffset;

    u64                 m_VertexDataOffset;
    u64                 m_IndexDataOffset;

    u32                 m_Unk53;
    u32                 m_Unk54;
    u32                 m_NumVertices3;
    u32                 m_NumIndices3;

    u32                 m_Unk57;
    u32                 m_Unk58;
    u32                 m_Unk59;
    u32                 m_Unk60;
    
} [[name(SkiMeshHeader_Name(this))]];

fn SkiMeshHeader_Name(auto data) {
    return std::format("MeshHeader (BPV:{})(V:{})(I:{})",
        data.m_BytesPerVertex,
        data.m_NumVertices1,
        data.m_NumIndices1);
};

struct SkiMeshInfo {
    u32 m_Unk1;
    u32 m_Unk2;
    u32 m_TotalVertices;
    u32 m_TotalIndices;
    u32 m_Unk6;
    u32 m_Unk58;
    u32 m_Unk59;
    u32 m_Unk60;
    
} [[name(SkiMeshInfo_Name(this))]];

fn SkiMeshInfo_Name(auto data) {
    return std::format("MeshInfo (Vertices:{})(Indices:{})",
        data.m_TotalVertices,
        data.m_TotalIndices);
};

struct Vertex60 {
    half4       m_Position ;
    float3      m_Normal ;
    float3      m_Tangent ;
    float3      m_Binormal ;
    u32         m_Color ;
    half2       m_UV ;
    u8          m_BoneIndices[4] ;
    u8          m_BoneWeights[4] ;
} ;

struct SkiMeshRecord {
    u64             m_Offset ;
    u64             m_Size ;
    
    SkiMeshHeader   m_Header @ m_Offset ;
    
    SkiMeshInfo     m_Info @ m_Header.m_MeshInfoOffset ;

    if ( m_Header.m_BytesPerVertex == 60 ){
        Vertex60    m_Vertices[ m_Header.m_NumVertices1 ] @ m_Offset + m_Header.m_VertexDataOffset;
    }
    else {
        u32 vertexDataSize = m_Header.m_BytesPerVertex * m_Header.m_NumVertices1;
        Blob<vertexDataSize> m_Vertices @ m_Offset + m_Header.m_VertexDataOffset ;
    }

    u32 indexDataSize = 2 * m_Header.m_NumIndices1 ;
    Blob<indexDataSize> m_Indices @ m_Offset + m_Header.m_IndexDataOffset ;
    
} [[name(SkiMeshRecord_Name(this))]];

fn SkiMeshRecord_Name(auto data) {
    return std::format("MeshChunk (Off:0x{:X})(Size:0x{:X})",
        data.m_Offset ,
        data.m_Size 
        );
};

fn Ski_TotalMeshCount(auto data) {
    u32 total = 0;
    
    /*for (u32 i = 0, i < data.m_TotalMeshGroups, i += 1) {
        //total += data.m_MeshGroups[i].m_TotalMeshes;
    }*/
    return total;
};

struct SkiHeader {
    u32         m_TotalMaterials;
    u32         m_TotalMeshGroups;
    
    u64         m_MeshGroupTableOffset;
    float4      m_Max;
    float4      m_Min;

    u64         m_MeshTableOffset;
    u64         m_MeshMaterialIdsOffset;
    u64         m_MaterialTableOffset;
    u64         m_Unk13;

    u32         m_LastMeshDataOffset;
    u32         m_Unk22;
    u32         m_Unk23;
    u32         m_Unk24;

    //u32         m_MeshMaterialIds[m_TotalMeshGroups] @ m_MeshMaterialIdsOffset;
    //SkiMeshGroupRecord m_MeshGroups[m_TotalMeshGroups] @ m_MeshGroupTableOffset;
    
    //SkiMaterialRecord m_MaterialRecords[m_TotalMaterials] @ m_MaterialTableOffset;
    //SkiMeshRecord   m_MeshRecords[Ski_TotalMeshCount(this)] @ m_MeshTableOffset;
    
} [[name(SkiHeader_Name(this))]];

fn SkiHeader_Name(auto data) {
    return std::format("SKI Header (Materials:{})(Groups:{})",
        data.m_TotalMaterials,
        data.m_TotalMeshGroups);
};




struct SkiFileHeader {
    u32         m_ID ;
    u16         m_MajorVersion ;
    u16         m_MinorVersion ;
    u16         m_ReferenceCount ;
    RmidType    m_Type ;
    char        m_Magic[4] ;
    
    u32         m_TotalMaterials;
    u32         m_TotalMeshGroups;
    
    u64         m_MeshGroupTableOffset;
    float4      m_Max;
    float4      m_Min;

    u64         m_MeshTableOffset;
    u64         m_MeshMaterialIdsOffset;
    u64         m_MaterialTableOffset;
    u64         m_Unk13;

} [[name(SkiFileHeader_Name(this))]];

fn SkiFileHeader_Name( auto data ) {
    return std::format("Header (Type:{})(MeshGroups:{})",
        data.m_Type,
        data.m_TotalMeshGroups
    );
};


//---------------------------------------------------------------------------------

SkiFileHeader       g_File @ 0x00 ;

SkiMaterialRecord   g_Materials[g_File.m_TotalMaterials] @ g_File.m_MaterialTableOffset ;
 

SkiMeshGroupRecord  g_MeshGroups[g_File.m_TotalMeshGroups] @ g_File.m_MeshGroupTableOffset ;

auto meshGroups = std::core::member_count( g_MeshGroups ) ; 
auto totalMeshes = 0;
for ( auto i=0 , i < meshGroups , i = i + 1 ) {
    totalMeshes = totalMeshes + g_MeshGroups[i].m_TotalMeshes ;
}

SkiMeshRecord       g_Meshes[totalMeshes] @ g_File.m_MeshTableOffset ;
u32                 g_MeshMaterialIds[totalMeshes] @ g_File.m_MeshMaterialIdsOffset;