#pragma description Defiance WAD archive
#pragma endian little


import std.string;


enum WadAssetType : u32 {
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

struct WadDataBlob<auto Size> {
    u8 bytes[Size];
} [[sealed]];

struct WadHeader {
    char        m_Magic[4];
    u32         m_Zero1;
    u32         m_ChunkCount;
    u32         m_Zero2;
    u32         m_Zero3[4];
    
}[[name(WadHeader_Name(this))]];

fn WadHeader_Name( auto data ){
    return std::format("m_Header ( {} )( Assets:{} )",
        data.m_Magic,
        data.m_ChunkCount
        );
};

struct WadIndexHeader {
    u32         m_RecordCount;
    u32         m_OffsetNext;
    u32         m_Zero1;
    u32         m_Zero2;
    
}[[name(WadIndexHeader_Name(this))]];

fn WadIndexHeader_Name( auto data ){
    return std::format("m_Header(Records:{})(Next:{})",
        data.m_RecordCount,
        data.m_OffsetNext
        );
};

struct WadIndexRecord {
    u32         m_ID;
    u32         m_Offset;
    u32         m_Size;
    u32         m_NameOffset;
    u64         m_ModifiedTime;
    WadAssetType m_Type;
    u32         m_Zero1;

    std::string::NullString m_Name @ m_NameOffset;
    WadDataBlob<m_Size> m_Data @ m_Offset;
    
}[[name(WadIndexRecord_Name(this))]];

fn WadIndexRecord_Name( auto data ){
    return std::format("(Type:{})({},{})",
        data.m_Type,
        data.m_Offset,
        data.m_Size
        );
};

struct WadIndexBlock {
    WadIndexHeader m_Header;
    WadIndexRecord m_Records[m_Header.m_RecordCount];

    if (m_Header.m_OffsetNext != 0) {
        WadIndexBlock m_Next @ m_Header.m_OffsetNext;
    }
};

struct WadFile {
    WadHeader m_Header;

    if (m_Header.m_ChunkCount != 0) {
        WadIndexBlock m_IndexChain @ $;
    }
}[[name(WadFile_Name(this))]];

fn WadFile_Name( auto data ) {
    return std::format("( {} )( Assets:{} )",
        data.m_Header.m_Magic,
        data.m_Header.m_ChunkCount
        );
};

WadFile wad @ 0x00;
