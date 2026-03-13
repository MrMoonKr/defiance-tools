#include "StdAfx.h"
#include "ExplorerView.h"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
    constexpr UINT_PTR kSplitterGripWidth = 8;
    constexpr int kToolbarHeight = 36;
    constexpr int kStatusHeight = 28;
    constexpr int kPadding = 8;

    std::wstring JoinTreeKey(const std::vector<std::wstring>& parts, size_t count)
    {
        std::wstring key;
        for (size_t index = 0; index < count; ++index)
        {
            if (index > 0)
                key += L"/";
            key += parts[index];
        }
        return key;
    }

    std::wstring SanitizeCacheComponent(const std::wstring& value)
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
            case L'/':
            case L'\\':
            case L'|':
            case L'?':
            case L'*':
                result.push_back(L'_');
                break;
            default:
                result.push_back((ch < 32) ? L'_' : ch);
                break;
            }
        }

        if (result.empty())
            result = L"_";
        return result;
    }

}

void CAssetTreeView::OnAttach()
{
    DWORD style = GetStyle();
    style |= TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS;
    SetStyle(style);
    ResetPlaceholderTree();
}

void CAssetTreeView::ResetPlaceholderTree()
{
    if (!IsWindow())
        return;

    DeleteAllItems();
    HTREEITEM root = InsertItem(L"No WAD loaded", 0, 0);
    InsertItem(L"Use Open Folder or Open File to begin.", 0, 0, root);
    Expand(root, TVE_EXPAND);
}

CExplorerView::CExplorerView()
    : m_hexPage(nullptr),
      m_imagePage(nullptr),
      m_glPage(nullptr),
      m_assetFilter(AssetFilter::All),
      m_leftPaneWidth(320),
      m_splitterWidth(6),
      m_isDraggingSplitter(false)
{
}

int CExplorerView::OnCreate(CREATESTRUCT& cs)
{
    CWnd::OnCreate(cs);
    CreateChildWindows();
    return 0;
}

void CExplorerView::OnInitialUpdate()
{
    LayoutChildren();
    SetRootPath(L"(not loaded)");
    SetStatusText(L"Ready");
    m_progress.SetRange(0, 100);
    m_progress.SetPos(0);
    ResetRightViews(L"Open a WAD folder or a single .wad file to inspect assets.");
}

void CExplorerView::PreCreate(CREATESTRUCT& cs)
{
    CWnd::PreCreate(cs);
    cs.dwExStyle |= WS_EX_CONTROLPARENT;
}

void CExplorerView::CreateChildWindows()
{
    m_rootLabel.Create(*this);
    m_rootLabel.SetWindowText(L"WAD Root");

    m_pathEdit.CreateEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
        0, 0, 0, 0, *this, reinterpret_cast<HMENU>(IDC_PATH_EDIT), nullptr);

    m_filterLabel.Create(*this);
    m_filterLabel.SetWindowText(L"Filter");

    m_filterCombo.CreateEx(0, WC_COMBOBOX, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        0, 0, 0, 0, *this, reinterpret_cast<HMENU>(IDC_FILTER_COMBO), nullptr);
    m_filterCombo.AddString(L"All");
    m_filterCombo.AddString(L"Mesh");
    m_filterCombo.AddString(L"Skin");
    m_filterCombo.AddString(L"Texture");
    m_filterCombo.AddString(L"Actor");
    m_filterCombo.AddString(L"Sound");
    m_filterCombo.AddString(L"Other");
    m_filterCombo.SetCurSel(static_cast<int>(m_assetFilter));

    m_openFolderButton.Create(*this);
    m_openFolderButton.SetWindowText(L"Open Folder");
    m_openFolderButton.SetDlgCtrlID(IDC_OPEN_FOLDER);

    m_openFileButton.Create(*this);
    m_openFileButton.SetWindowText(L"Open File");
    m_openFileButton.SetDlgCtrlID(IDC_OPEN_FILE);

    m_tree.CreateEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
        TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        0, 0, 0, 0, *this, reinterpret_cast<HMENU>(IDC_ASSET_TREE), nullptr);

    m_tabs.CreateEx(0, WC_TABCONTROL, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
        0, 0, 0, 0, *this, reinterpret_cast<HMENU>(IDC_TAB_VIEW), nullptr);

    m_hexPage = static_cast<CTextPage*>(
        m_tabs.AddTabPage(std::make_unique<CTextPage>(L"HexView placeholder", true), L"HexView"));
    m_imagePage = static_cast<CImagePage*>(
        m_tabs.AddTabPage(std::make_unique<CImagePage>(), L"ImageView"));
    m_glPage = static_cast<CGlPage*>(
        m_tabs.AddTabPage(std::make_unique<CGlPage>(), L"GLView"));
    m_tabs.SelectPage(0);

    m_statusText.Create(*this);
    m_statusText.SetWindowText(L"Ready");

    m_progress.Create(*this);
}

void CExplorerView::LayoutChildren()
{
    if (!IsWindow())
        return;

    CRect rc = GetClientRect();
    const int clientWidth = rc.Width();
    const int clientRight = static_cast<int>(rc.right);
    const int clientBottom = static_cast<int>(rc.bottom);
    const int toolbarY = kPadding;
    const int buttonWidth = 110;
    const int labelWidth = 68;
    const int filterLabelWidth = 42;
    const int filterComboWidth = 128;
    const int progressWidth = 220;
    const int splitterUpper = std::max(kMinLeftPaneWidth, clientWidth - kMinRightPaneWidth);
    const int splitterX = std::clamp(m_leftPaneWidth, kMinLeftPaneWidth, splitterUpper);

    m_rootLabel.SetWindowPos(HWND_TOP, kPadding, toolbarY + 8, labelWidth, 20, SWP_SHOWWINDOW);
    m_openFileButton.SetWindowPos(HWND_TOP, clientRight - kPadding - buttonWidth, toolbarY, buttonWidth, 28, SWP_SHOWWINDOW);
    m_openFolderButton.SetWindowPos(HWND_TOP, clientRight - kPadding - (buttonWidth * 2) - 8, toolbarY, buttonWidth, 28, SWP_SHOWWINDOW);

    const int filterComboLeft = clientRight - kPadding - (buttonWidth * 2) - 8 - filterComboWidth - 16;
    const int filterLabelLeft = filterComboLeft - filterLabelWidth - 6;
    m_filterLabel.SetWindowPos(HWND_TOP, filterLabelLeft, toolbarY + 8, filterLabelWidth, 20, SWP_SHOWWINDOW);
    m_filterCombo.SetWindowPos(HWND_TOP, filterComboLeft, toolbarY, filterComboWidth, 240, SWP_SHOWWINDOW);

    const int editLeft = kPadding + labelWidth + 8;
    const int editRight = filterLabelLeft - 10;
    const int editWidth = std::max(80, editRight - editLeft);
    m_pathEdit.SetWindowPos(HWND_TOP, editLeft, toolbarY, editWidth, 28, SWP_SHOWWINDOW);

    const int contentTop = toolbarY + kToolbarHeight;
    const int contentBottom = clientBottom - kStatusHeight - kPadding;
    const int contentHeight = std::max(120, contentBottom - contentTop);
    const int tabsWidth = std::max(160, clientRight - splitterX - m_splitterWidth - kPadding);
    const int statusWidth = std::max(120, clientWidth - progressWidth - 24);

    m_tree.SetWindowPos(HWND_TOP, kPadding, contentTop, splitterX - kPadding, contentHeight, SWP_SHOWWINDOW);
    m_tabs.SetWindowPos(HWND_TOP, splitterX + m_splitterWidth, contentTop, tabsWidth, contentHeight, SWP_SHOWWINDOW);

    m_statusText.SetWindowPos(HWND_TOP, kPadding, clientBottom - kStatusHeight, statusWidth, 20, SWP_SHOWWINDOW);
    m_progress.SetWindowPos(HWND_TOP, clientRight - progressWidth - kPadding, clientBottom - kStatusHeight - 2, progressWidth, 20, SWP_SHOWWINDOW);
}

void CExplorerView::OpenFolder()
{
    CFolderDialog dialog;
    dialog.SetTitle(L"Select a folder containing WAD files");
    if (dialog.DoModal(*this) == IDOK)
        PopulateForFolder(std::filesystem::path(dialog.GetFolderPath().c_str()));
}

void CExplorerView::OpenFile()
{
    CString filter = L"WAD Files (*.wad)|*.wad|All Files (*.*)|*.*||";
    CFileDialog dialog(TRUE, L"wad", nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, filter);
    if (dialog.DoModal(*this) == IDOK)
        PopulateForFile(std::filesystem::path(dialog.GetPathName().c_str()));
}

void CExplorerView::PopulateForFolder(const std::filesystem::path& folderPath)
{
    try
    {
        SetStatusText(L"Loading WAD folder...");
        m_progress.SetPos(10);
        m_model.LoadFromFolder(folderPath);
        PopulateTreeFromModel();

        const auto& wadFiles = m_model.GetWadFiles();
        size_t totalAssets = 0;
        for (const auto& wadFile : wadFiles)
            totalAssets += wadFile.assets.size();

        SetRootPath(m_model.GetRootPath().wstring());
        std::wstring status = L"Loaded " + std::to_wstring(wadFiles.size()) +
                              L" WAD file(s), " + std::to_wstring(totalAssets) + L" asset(s).";
        SetStatusText(status);
        m_progress.SetPos(100);
        ResetRightViews(L"Select a WAD, folder, or asset node.");
    }
    catch (const std::exception& e)
    {
        m_model.Clear();
        m_treeNodeData.clear();
        m_tree.ResetPlaceholderTree();
        ResetRightViews(L"Load failed.");
        SetStatusText(L"Load failed");
        m_progress.SetPos(0);
        ::MessageBoxA(*this, e.what(), "WAD load error", MB_ICONERROR);
    }
}

void CExplorerView::PopulateForFile(const std::filesystem::path& filePath)
{
    try
    {
        SetStatusText(L"Loading WAD file...");
        m_progress.SetPos(10);
        m_model.LoadFromFile(filePath);
        PopulateTreeFromModel();

        const auto& wadFiles = m_model.GetWadFiles();
        size_t totalAssets = 0;
        for (const auto& wadFile : wadFiles)
            totalAssets += wadFile.assets.size();

        SetRootPath(filePath.wstring());
        std::wstring status = L"Loaded " + std::to_wstring(wadFiles.size()) +
                              L" WAD file(s), " + std::to_wstring(totalAssets) + L" asset(s).";
        SetStatusText(status);
        m_progress.SetPos(100);
        ResetRightViews(L"Select a WAD, folder, or asset node.");
    }
    catch (const std::exception& e)
    {
        m_model.Clear();
        m_treeNodeData.clear();
        m_tree.ResetPlaceholderTree();
        ResetRightViews(L"Load failed.");
        SetStatusText(L"Load failed");
        m_progress.SetPos(0);
        ::MessageBoxA(*this, e.what(), "WAD load error", MB_ICONERROR);
    }
}

void CExplorerView::PopulateTreeFromModel()
{
    m_treeNodeData.clear();
    m_tree.DeleteAllItems();

    if (!m_model.IsLoaded())
    {
        m_tree.ResetPlaceholderTree();
        return;
    }

    size_t totalVisibleAssets = 0;
    HTREEITEM rootItem = m_tree.InsertItem(L"WAD Archives", 0, 0);
    m_treeNodeData[rootItem] = {TreeNodeKind::Root, 0, 0, L"", m_model.GetWadFiles().size(), 0};

    const auto& wadFiles = m_model.GetWadFiles();
    for (size_t wadIndex = 0; wadIndex < wadFiles.size(); ++wadIndex)
    {
        const auto& wadFile = wadFiles[wadIndex];
        std::vector<size_t> visibleAssetIndices;
        visibleAssetIndices.reserve(wadFile.assets.size());
        uint64_t visibleTotalSize = 0;
        for (size_t assetIndex = 0; assetIndex < wadFile.assets.size(); ++assetIndex)
        {
            const auto& asset = wadFile.assets[assetIndex];
            if (!AssetMatchesFilter(asset))
                continue;

            visibleAssetIndices.push_back(assetIndex);
            visibleTotalSize += asset.dataSize;
        }

        if (visibleAssetIndices.empty())
            continue;

        totalVisibleAssets += visibleAssetIndices.size();
        std::wstring wadLabel = wadFile.displayName + L" (" + std::to_wstring(visibleAssetIndices.size()) + L")";
        HTREEITEM wadItem = m_tree.InsertItem(wadLabel.c_str(), 0, 0, rootItem);
        m_treeNodeData[wadItem] = {TreeNodeKind::Wad, wadIndex, 0, L"", visibleAssetIndices.size(), visibleTotalSize};

        std::unordered_map<std::wstring, std::pair<size_t, uint64_t>> folderStats;
        for (size_t assetIndex : visibleAssetIndices)
        {
            const auto& asset = wadFile.assets[assetIndex];
            for (size_t depth = 1; depth < asset.treeParts.size(); ++depth)
            {
                const auto key = JoinTreeKey(asset.treeParts, depth);
                auto& stats = folderStats[key];
                stats.first += 1;
                stats.second += asset.dataSize;
            }
        }

        std::unordered_map<std::wstring, HTREEITEM> folderItems;
        folderItems[L""] = wadItem;

        for (size_t assetIndex : visibleAssetIndices)
        {
            const auto& asset = wadFile.assets[assetIndex];
            HTREEITEM parentItem = wadItem;

            for (size_t depth = 0; depth + 1 < asset.treeParts.size(); ++depth)
            {
                const auto key = JoinTreeKey(asset.treeParts, depth + 1);
                const auto found = folderItems.find(key);
                if (found == folderItems.end())
                {
                    HTREEITEM folderItem = m_tree.InsertItem(asset.treeParts[depth].c_str(), 0, 0, parentItem);
                    const auto stats = folderStats[key];
                    TreeNodeData node{};
                    node.kind = TreeNodeKind::Folder;
                    node.wadIndex = wadIndex;
                    node.folderKey = key;
                    node.assetCount = stats.first;
                    node.totalSize = stats.second;
                    m_treeNodeData[folderItem] = node;
                    folderItems[key] = folderItem;
                    parentItem = folderItem;
                }
                else
                {
                    parentItem = found->second;
                }
            }

            std::wstring assetLabel = asset.treeParts.empty() ? asset.displayName : asset.treeParts.back();
            HTREEITEM assetItem = m_tree.InsertItem(assetLabel.c_str(), 0, 0, parentItem);
            TreeNodeData node{};
            node.kind = TreeNodeKind::Asset;
            node.wadIndex = wadIndex;
            node.assetIndex = assetIndex;
            node.assetCount = 1;
            node.totalSize = asset.dataSize;
            m_treeNodeData[assetItem] = node;
        }
    }

    if (totalVisibleAssets == 0)
    {
        m_tree.DeleteAllItems();
        HTREEITEM root = m_tree.InsertItem(L"No assets match the current filter.", 0, 0);
        m_tree.InsertItem(L"Choose a different asset type filter.", 0, 0, root);
        m_tree.Expand(root, TVE_EXPAND);
        return;
    }

    m_tree.Expand(rootItem, TVE_EXPAND);
    m_tree.SelectItem(rootItem);
}

void CExplorerView::UpdateRightPaneForSelection()
{
    HTREEITEM item = m_tree.GetSelection();
    if (!item)
        return;

    const auto found = m_treeNodeData.find(item);
    if (found == m_treeNodeData.end())
        return;

    const auto& node = found->second;
    switch (node.kind)
    {
    case TreeNodeKind::Root:
        ResetRightViews(L"Select a WAD, folder, or asset node.");
        SetStatusText(L"Selected: WAD Archives");
        break;
    case TreeNodeKind::Wad:
        ShowWadNode(node.wadIndex);
        break;
    case TreeNodeKind::Folder:
        ShowFolderNode(node.wadIndex, node.folderKey, node.assetCount, node.totalSize);
        break;
    case TreeNodeKind::Asset:
        ShowAssetNode(node.wadIndex, node.assetIndex);
        break;
    default:
        break;
    }
}

void CExplorerView::ShowWadNode(size_t wadIndex)
{
    const auto& wadFile = m_model.GetWadFiles().at(wadIndex);
    std::wstring text =
        L"WAD File\r\n\r\n"
        L"Path: " + wadFile.path.wstring() + L"\r\n" +
        L"Assets: " + std::to_wstring(wadFile.assets.size()) + L"\r\n" +
        L"Stored Size: " + WadArchiveModel::FormatSize(wadFile.totalSize);

    if (m_hexPage)
        m_hexPage->SetPageText(text);
    SetImageText(L"Select an asset to inspect texture-related information.");
    SetGlText(L"Select a mesh or skin asset to inspect future GLView metadata.");
    SetStatusText(L"Selected WAD: " + wadFile.displayName);
}

void CExplorerView::ShowFolderNode(size_t wadIndex, const std::wstring& folderKey, size_t assetCount, uint64_t totalSize)
{
    const auto& wadFile = m_model.GetWadFiles().at(wadIndex);
    std::wstring text =
        L"Folder Node\r\n\r\n"
        L"WAD: " + wadFile.displayName + L"\r\n" +
        L"Folder: " + folderKey + L"\r\n" +
        L"Assets: " + std::to_wstring(assetCount) + L"\r\n" +
        L"Stored Size: " + WadArchiveModel::FormatSize(totalSize);

    if (m_hexPage)
        m_hexPage->SetPageText(text);
    SetImageText(L"Folder selected. Choose an asset for texture information.");
    SetGlText(L"Folder selected. Choose an asset for GL metadata.");
    SetStatusText(L"Selected folder: " + folderKey);
}

void CExplorerView::ShowAssetNode(size_t wadIndex, size_t assetIndex)
{
    const auto& wadFile = m_model.GetWadFiles().at(wadIndex);
    const auto& asset = wadFile.assets.at(assetIndex);
    const auto payload = m_model.ReadAssetPayload(asset);

    std::wstring header =
        L"Asset\r\n\r\n"
        L"Name: " + asset.displayName + L"\r\n" +
        L"WAD: " + wadFile.displayName + L"\r\n" +
        L"ID: 0x" + [&]() {
            std::wostringstream stream;
            stream << std::uppercase << std::hex << std::setw(8) << std::setfill(L'0') << asset.assetId;
            return stream.str();
        }() + L"\r\n" +
        L"Type: " + WadArchiveModel::AssetTypeName(asset.assetType) + L"\r\n" +
        L"Stored Size: " + WadArchiveModel::FormatSize(asset.dataSize) + L"\r\n" +
        L"Modified: " + WadArchiveModel::FormatTimestamp(asset.modifiedTime) + L"\r\n";

    if (payload.isRmid)
    {
        header += L"RMID: yes\r\n";
        header += L"RMID Type: " + WadArchiveModel::AssetTypeName(payload.rmidType) + L"\r\n";
        header += L"References: " + std::to_wstring(payload.rmidReferenceCount) + L"\r\n";
        header += L"Decompressed View: " + std::wstring(payload.wasDecompressed ? L"yes" : L"no") + L"\r\n";
    }
    else
    {
        header += L"RMID: no\r\n";
    }

    header += L"\r\nHex Dump\r\n\r\n";
    header += WadArchiveModel::HexDump(payload.viewBytes);
    if (m_hexPage)
        m_hexPage->SetPageText(header);

    std::wstring imageText =
        L"ImageView Metadata\r\n\r\n"
        L"Asset: " + asset.displayName + L"\r\n" +
        L"Type: " + WadArchiveModel::AssetTypeName(asset.assetType) + L"\r\n";
    if (payload.textureInfo)
    {
        const auto& info = *payload.textureInfo;
        imageText += L"Texture Width: " + std::to_wstring(info.width) + L"\r\n";
        imageText += L"Texture Height: " + std::to_wstring(info.height) + L"\r\n";
        imageText += L"Mipmaps: " + std::to_wstring(info.mipmapCount) + L"\r\n";
        imageText += L"Format: " + std::to_wstring(info.format) + L"\r\n";
        imageText += L"Bits Per Pixel: " + std::to_wstring(info.bitsPerPixel) + L"\r\n";
        imageText += L"Cubemap: " + std::wstring(info.isCubemap ? L"yes" : L"no") + L"\r\n";
        imageText += L"\r\nPixel preview is shown when the texture format is supported by the decoder.";
    }
    else
    {
        imageText += L"\r\nNo texture metadata available for this asset.";
    }
    if (payload.imagePreview)
    {
        auto preview = *payload.imagePreview;
        preview.description += L"\r\n\r\n";
        preview.description += imageText;
        SetImagePreview(preview);
    }
    else
    {
        SetImageText(imageText);
    }

    const bool isMeshCandidate = asset.assetType == RMID_TYPE_MES || asset.assetType == RMID_TYPE_SKI;
    std::wstring glDetail =
        L"Asset: " + asset.displayName + L"\r\n" +
        L"Type: " + WadArchiveModel::AssetTypeName(asset.assetType) + L"\r\n";
    if (payload.meshPreview)
    {
        glDetail += L"\r\nActual static mesh geometry loaded from RMID payload.";
        SetGlMeshPreview(*payload.meshPreview, asset.displayName, glDetail);
    }
    else if (isMeshCandidate)
    {
        glDetail += L"\r\nMesh-family asset detected, but direct geometry extraction is not implemented for this asset layout yet.";
        SetGlScene(asset.displayName, glDetail, true);
    }
    else
    {
        glDetail += L"\r\nThis is not a mesh-family asset. GLView is showing the default diagnostic scene.";
        SetGlScene(asset.displayName, glDetail, false);
    }

    SetStatusText(L"Selected asset: " + asset.displayName);
}

void CExplorerView::ResetRightViews(const std::wstring& message)
{
    if (m_hexPage)
        m_hexPage->SetPageText(message);
    SetImageText(message);
    SetGlText(message);
}

void CExplorerView::RebuildFilteredTree()
{
    PopulateTreeFromModel();

    if (!m_model.IsLoaded())
        return;

    std::wstring filterName = L"All";
    switch (m_assetFilter)
    {
    case AssetFilter::Mesh: filterName = L"Mesh"; break;
    case AssetFilter::Skin: filterName = L"Skin"; break;
    case AssetFilter::Texture: filterName = L"Texture"; break;
    case AssetFilter::Actor: filterName = L"Actor"; break;
    case AssetFilter::Sound: filterName = L"Sound"; break;
    case AssetFilter::Other: filterName = L"Other"; break;
    case AssetFilter::All:
    default:
        break;
    }

    ResetRightViews(L"Select a WAD, folder, or asset node.");
    SetStatusText(L"Asset filter: " + filterName);
}

void CExplorerView::SetImageText(const std::wstring& text)
{
    if (m_imagePage)
        m_imagePage->Clear(text);
}

void CExplorerView::SetImagePreview(const WadImagePreview& preview)
{
    if (m_imagePage)
        m_imagePage->SetPreview(preview);
}

void CExplorerView::SetGlMeshPreview(const WadMeshPreview& preview, const std::wstring& title, const std::wstring& detail)
{
    if (m_glPage)
        m_glPage->SetMeshPreview(preview, title, detail);
}

void CExplorerView::SetGlText(const std::wstring& text)
{
    if (m_glPage)
        m_glPage->Clear(text);
}

void CExplorerView::SetGlScene(const std::wstring& title, const std::wstring& detail, bool meshCandidate)
{
    if (m_glPage)
        m_glPage->SetScene(title, detail, meshCandidate);
}

void CExplorerView::SetRootPath(const std::wstring& pathText)
{
    m_pathEdit.SetWindowText(pathText.c_str());
}

void CExplorerView::SetStatusText(const std::wstring& text)
{
    m_statusText.SetWindowText(text.c_str());
}

bool CExplorerView::IsOnSplitter(int x) const
{
    return x >= (m_leftPaneWidth - static_cast<int>(kSplitterGripWidth)) &&
           x <= (m_leftPaneWidth + static_cast<int>(kSplitterGripWidth));
}

bool CExplorerView::AssetMatchesFilter(const WadAsset& asset) const
{
    switch (m_assetFilter)
    {
    case AssetFilter::All:
        return true;
    case AssetFilter::Mesh:
        return asset.assetType == RMID_TYPE_MES;
    case AssetFilter::Skin:
        return asset.assetType == RMID_TYPE_SKI;
    case AssetFilter::Texture:
        return asset.assetType == RMID_TYPE_TEX;
    case AssetFilter::Actor:
        return asset.assetType == RMID_TYPE_ACT;
    case AssetFilter::Sound:
        return asset.assetType == RMID_TYPE_SND ||
               asset.assetType == RMID_TYPE_SPK ||
               asset.assetType == RMID_TYPE_MOV;
    case AssetFilter::Other:
        return asset.assetType != RMID_TYPE_MES &&
               asset.assetType != RMID_TYPE_SKI &&
               asset.assetType != RMID_TYPE_TEX &&
               asset.assetType != RMID_TYPE_ACT &&
               asset.assetType != RMID_TYPE_SND &&
               asset.assetType != RMID_TYPE_SPK &&
               asset.assetType != RMID_TYPE_MOV;
    default:
        return true;
    }
}

void CExplorerView::ShowTreeContextMenu(POINT screenPoint)
{
    if (!m_tree.IsWindow())
        return;

    POINT clientPoint = screenPoint;
    if (screenPoint.x == -1 && screenPoint.y == -1)
    {
        HTREEITEM selected = m_tree.GetSelection();
        if (!selected)
            return;

        RECT itemRect{};
        if (!m_tree.GetItemRect(selected, itemRect, TRUE))
            return;

        clientPoint.x = itemRect.left + 12;
        clientPoint.y = itemRect.top + ((itemRect.bottom - itemRect.top) / 2);
        screenPoint = clientPoint;
        ::ClientToScreen(m_tree.GetHwnd(), &screenPoint);
    }
    else
    {
        ::ScreenToClient(m_tree.GetHwnd(), &clientPoint);
    }

    TVHITTESTINFO hit{};
    hit.pt = clientPoint;
    HTREEITEM item = m_tree.HitTest(hit);
    if (item)
        m_tree.SelectItem(item);
    else
        item = m_tree.GetSelection();

    if (!item)
        return;

    const auto found = m_treeNodeData.find(item);
    if (found == m_treeNodeData.end() || found->second.kind != TreeNodeKind::Asset)
        return;

    CMenu popupMenu;
    popupMenu.CreatePopupMenu();
    popupMenu.AppendMenu(MF_STRING, ID_EXPORT_RAW, L"Export Raw");

    const UINT command = popupMenu.TrackPopupMenu(TPM_RETURNCMD | TPM_RIGHTBUTTON, screenPoint.x, screenPoint.y, *this);
    if (command == ID_EXPORT_RAW)
        ExportAssetRaw(found->second.wadIndex, found->second.assetIndex);
}

void CExplorerView::ExportAssetRaw(size_t wadIndex, size_t assetIndex)
{
    const auto& wadFile = m_model.GetWadFiles().at(wadIndex);
    const auto& asset = wadFile.assets.at(assetIndex);
    const auto payload = m_model.ReadAssetPayload(asset);
    const auto* exportBytes = !payload.viewBytes.empty() ? &payload.viewBytes : &payload.rawBytes;
    if (exportBytes->empty())
    {
        ::MessageBox(*this, L"No raw payload is available for this asset.", L"Export Raw", MB_ICONWARNING);
        return;
    }

    std::filesystem::path exportPath = std::filesystem::path(WADEXPLORER_PROJECT_DIR) / L".cache" /
        SanitizeCacheComponent(wadFile.path.stem().wstring());

    if (asset.treeParts.size() > 1)
    {
        for (size_t index = 0; index + 1 < asset.treeParts.size(); ++index)
            exportPath /= SanitizeCacheComponent(asset.treeParts[index]);
    }

    const std::wstring fileName = asset.treeParts.empty() ? asset.displayName : asset.treeParts.back();
    const uint32_t exportType = payload.rmidType != 0 ? payload.rmidType : asset.assetType;
    exportPath /= SanitizeCacheComponent(fileName) + WadArchiveModel::AssetTypeExtension(exportType);

    try
    {
        std::filesystem::create_directories(exportPath.parent_path());
        std::ofstream output(exportPath, std::ios::binary | std::ios::trunc);
        output.write(
            reinterpret_cast<const char*>(exportBytes->data()),
            static_cast<std::streamsize>(exportBytes->size()));
        output.close();

        if (!output)
            throw std::runtime_error("Failed to write exported raw asset");

        SetStatusText(L"Exported raw asset: " + asset.displayName);
        ::MessageBox(*this, exportPath.wstring().c_str(), L"Export Raw", MB_OK | MB_ICONINFORMATION);
    }
    catch (const std::exception& e)
    {
        ::MessageBoxA(*this, e.what(), "Export Raw", MB_ICONERROR);
    }
}

BOOL CExplorerView::OnCommand(WPARAM wparam, LPARAM)
{
    const UINT id = LOWORD(wparam);
    const UINT code = HIWORD(wparam);

    if (id == IDC_FILTER_COMBO && code == CBN_SELCHANGE)
    {
        const int selection = m_filterCombo.GetCurSel();
        if (selection >= 0)
        {
            m_assetFilter = static_cast<AssetFilter>(selection);
            RebuildFilteredTree();
        }
        return TRUE;
    }

    if (code == BN_CLICKED)
    {
        switch (id)
        {
        case IDC_OPEN_FOLDER:
            OpenFolder();
            return TRUE;
        case IDC_OPEN_FILE:
            OpenFile();
            return TRUE;
        default:
            break;
        }
    }

    return FALSE;
}

LRESULT CExplorerView::OnNotify(WPARAM, LPARAM lparam)
{
    const auto* hdr = reinterpret_cast<LPNMHDR>(lparam);
    if (hdr && hdr->hwndFrom == m_tree.GetHwnd() && hdr->code == TVN_SELCHANGED)
    {
        UpdateRightPaneForSelection();
        return 0;
    }

    return 0;
}

LRESULT CExplorerView::OnLButtonDown(UINT msg, WPARAM wparam, LPARAM lparam)
{
    const int x = GET_X_LPARAM(lparam);
    if (IsOnSplitter(x))
    {
        m_isDraggingSplitter = true;
        SetCapture();
        return 0;
    }

    return FinalWindowProc(msg, wparam, lparam);
}

LRESULT CExplorerView::OnLButtonUp(UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (m_isDraggingSplitter)
    {
        m_isDraggingSplitter = false;
        ReleaseCapture();
        return 0;
    }

    return FinalWindowProc(msg, wparam, lparam);
}

LRESULT CExplorerView::OnMouseMove(UINT msg, WPARAM wparam, LPARAM lparam)
{
    const int x = GET_X_LPARAM(lparam);
    if (m_isDraggingSplitter)
    {
        CRect rc = GetClientRect();
        const int splitterUpper = std::max(kMinLeftPaneWidth, rc.Width() - kMinRightPaneWidth);
        m_leftPaneWidth = std::clamp(x, kMinLeftPaneWidth, splitterUpper);
        LayoutChildren();
        return 0;
    }

    return FinalWindowProc(msg, wparam, lparam);
}

LRESULT CExplorerView::OnSetCursor(UINT msg, WPARAM wparam, LPARAM lparam)
{
    CPoint pos = GetCursorPos();
    ScreenToClient(pos);
    if (IsOnSplitter(pos.x))
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_SIZEWE));
        return TRUE;
    }

    return FinalWindowProc(msg, wparam, lparam);
}

LRESULT CExplorerView::OnSize(UINT msg, WPARAM wparam, LPARAM lparam)
{
    LayoutChildren();
    return FinalWindowProc(msg, wparam, lparam);
}

LRESULT CExplorerView::OnContextMenu(UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (reinterpret_cast<HWND>(wparam) == m_tree.GetHwnd())
    {
        POINT screenPoint{
            GET_X_LPARAM(lparam),
            GET_Y_LPARAM(lparam)
        };
        ShowTreeContextMenu(screenPoint);
        return 0;
    }

    return FinalWindowProc(msg, wparam, lparam);
}

LRESULT CExplorerView::WndProc(UINT msg, WPARAM wparam, LPARAM lparam)
{
    try
    {
        switch (msg)
        {
        case WM_COMMAND:   return OnCommand(wparam, lparam);
        case WM_LBUTTONDOWN: return OnLButtonDown(msg, wparam, lparam);
        case WM_LBUTTONUP:   return OnLButtonUp(msg, wparam, lparam);
        case WM_MOUSEMOVE:   return OnMouseMove(msg, wparam, lparam);
        case WM_SETCURSOR:   return OnSetCursor(msg, wparam, lparam);
        case WM_SIZE:        return OnSize(msg, wparam, lparam);
        case WM_CONTEXTMENU: return OnContextMenu(msg, wparam, lparam);
        default:
            break;
        }

        return WndProcDefault(msg, wparam, lparam);
    }
    catch (const CException& e)
    {
        CString message;
        message << e.GetText() << L"\n" << e.GetErrorString();
        ::MessageBox(nullptr, message, L"CException", MB_ICONERROR);
    }
    catch (const std::exception& e)
    {
        ::MessageBoxA(nullptr, e.what(), "std::exception", MB_ICONERROR);
    }

    return 0;
}
