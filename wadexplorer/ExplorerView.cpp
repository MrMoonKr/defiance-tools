#include "StdAfx.h"
#include "ExplorerView.h"

#include <iomanip>
#include <sstream>

namespace
{
    constexpr UINT_PTR kSplitterGripWidth = 8;
    constexpr int kToolbarHeight = 36;
    constexpr int kStatusHeight = 28;
    constexpr int kPadding = 8;

    std::wstring NormalizeEditLineEndings(const std::wstring& text)
    {
        std::wstring normalized;
        normalized.reserve(text.size() + 16);

        for (size_t index = 0; index < text.size(); ++index)
        {
            const wchar_t ch = text[index];
            if (ch == L'\r')
            {
                normalized.push_back(L'\r');
                if (index + 1 < text.size() && text[index + 1] == L'\n')
                {
                    normalized.push_back(L'\n');
                    ++index;
                }
                else
                {
                    normalized.push_back(L'\n');
                }
            }
            else if (ch == L'\n')
            {
                normalized.push_back(L'\r');
                normalized.push_back(L'\n');
            }
            else
            {
                normalized.push_back(ch);
            }
        }

        return normalized;
    }

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
}


CTextPage::CTextPage(const std::wstring& initialText, bool fixedFont)
    : m_initialText(initialText), m_useFixedFont(fixedFont)
{
}

void CTextPage::OnAttach()
{
    SetPageText(m_initialText);
    if (m_useFixedFont)
    {
        HDC screenDc = ::GetDC(nullptr);
        const int dpiY = screenDc ? ::GetDeviceCaps(screenDc, LOGPIXELSY) : 96;
        if (screenDc)
            ::ReleaseDC(nullptr, screenDc);

        LOGFONT lf{};
        lf.lfHeight = -MulDiv(10, dpiY, 72);
        lf.lfWeight = FW_NORMAL;
        lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
        wcscpy_s(lf.lfFaceName, L"Consolas");
        m_font.CreateFontIndirect(lf);
        SetFont(m_font, TRUE);
        SetMargins(0, 0);

        const UINT tabStops = 32;
        SendMessage(EM_SETTABSTOPS, 1, reinterpret_cast<LPARAM>(&tabStops));
    }
}

void CTextPage::PreCreate(CREATESTRUCT& cs)
{
    CEdit::PreCreate(cs);
    cs.dwExStyle |= WS_EX_CLIENTEDGE;
    cs.style |= ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_LEFT | ES_MULTILINE | ES_READONLY |
                ES_WANTRETURN | WS_HSCROLL | WS_VSCROLL;
}

void CTextPage::SetPageText(const std::wstring& text)
{
    m_initialText = NormalizeEditLineEndings(text);
    if (IsWindow())
        SetWindowText(m_initialText.c_str());
}


void CImagePage::Clear(const std::wstring& message)
{
    m_message = message;
    m_description.clear();
    m_width = 0;
    m_height = 0;
    m_rgba.clear();
    if (IsWindow())
        RedrawWindow(RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
}

void CImagePage::SetPreview(const WadImagePreview& preview)
{
    m_message.clear();
    m_description = preview.description;
    m_width = preview.width;
    m_height = preview.height;
    m_rgba = preview.rgba;
    if (IsWindow())
        RedrawWindow(RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
}

void CImagePage::OnDraw(CDC& dc)
{
    CRect rc = GetClientRect();
    dc.FillRect(rc, CBrush(RGB(28, 28, 30)));
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(RGB(220, 220, 220));

    const size_t expectedBytes = static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 4;
    if (m_rgba.empty() || m_width == 0 || m_height == 0 || m_rgba.size() < expectedBytes)
    {
        dc.DrawText(m_message.empty() ? L"No preview available." : m_message.c_str(), -1, rc,
            DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        return;
    }

    const int margin = 16;
    const int availableWidth = std::max(1, rc.Width() - (margin * 2));
    const int availableHeight = std::max(1, rc.Height() - (margin * 2) - 48);
    const double scaleX = static_cast<double>(availableWidth) / static_cast<double>(m_width);
    const double scaleY = static_cast<double>(availableHeight) / static_cast<double>(m_height);
    const double scale = std::min(scaleX, scaleY);

    const int drawWidth = std::max(1, static_cast<int>(m_width * scale));
    const int drawHeight = std::max(1, static_cast<int>(m_height * scale));
    const int imageLeft = rc.left + (rc.Width() - drawWidth) / 2;
    const int imageTop = rc.top + margin;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(m_width);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(m_height);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint8_t> bgra(expectedBytes);
    for (size_t index = 0; index < expectedBytes; index += 4)
    {
        bgra[index + 0] = m_rgba[index + 2];
        bgra[index + 1] = m_rgba[index + 1];
        bgra[index + 2] = m_rgba[index + 0];
        bgra[index + 3] = m_rgba[index + 3];
    }

    ::SetStretchBltMode(dc, HALFTONE);
    ::SetBrushOrgEx(dc, 0, 0, nullptr);
    ::StretchDIBits(
        dc,
        imageLeft,
        imageTop,
        drawWidth,
        drawHeight,
        0,
        0,
        static_cast<int>(m_width),
        static_cast<int>(m_height),
        bgra.data(),
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY);

    CRect textRect = rc;
    textRect.top = imageTop + drawHeight + 12;
    dc.DrawText(m_description.c_str(), -1, textRect, DT_CENTER | DT_TOP | DT_WORDBREAK);
}

void CImagePage::PreCreate(CREATESTRUCT& cs)
{
    CWnd::PreCreate(cs);
    cs.dwExStyle |= WS_EX_CLIENTEDGE;
}

LRESULT CImagePage::WndProc(UINT msg, WPARAM wparam, LPARAM lparam)
{
    try
    {
        switch (msg)
        {
        case WM_SIZE:
            Invalidate();
            break;
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
    m_glPage = static_cast<CTextPage*>(
        m_tabs.AddTabPage(std::make_unique<CTextPage>(L"GLView placeholder\nFuture renderer surface goes here.", false), L"GLView"));
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
    const int progressWidth = 220;
    const int splitterUpper = std::max(kMinLeftPaneWidth, clientWidth - kMinRightPaneWidth);
    const int splitterX = std::clamp(m_leftPaneWidth, kMinLeftPaneWidth, splitterUpper);

    m_rootLabel.SetWindowPos(HWND_TOP, kPadding, toolbarY + 8, labelWidth, 20, SWP_SHOWWINDOW);
    m_openFileButton.SetWindowPos(HWND_TOP, clientRight - kPadding - buttonWidth, toolbarY, buttonWidth, 28, SWP_SHOWWINDOW);
    m_openFolderButton.SetWindowPos(HWND_TOP, clientRight - kPadding - (buttonWidth * 2) - 8, toolbarY, buttonWidth, 28, SWP_SHOWWINDOW);

    const int editLeft = kPadding + labelWidth + 8;
    const int editRight = clientRight - kPadding - (buttonWidth * 2) - 16;
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

    HTREEITEM rootItem = m_tree.InsertItem(L"WAD Archives", 0, 0);
    m_treeNodeData[rootItem] = {TreeNodeKind::Root, 0, 0, L"", m_model.GetWadFiles().size(), 0};

    const auto& wadFiles = m_model.GetWadFiles();
    for (size_t wadIndex = 0; wadIndex < wadFiles.size(); ++wadIndex)
    {
        const auto& wadFile = wadFiles[wadIndex];
        std::wstring wadLabel = wadFile.displayName + L" (" + std::to_wstring(wadFile.assets.size()) + L")";
        HTREEITEM wadItem = m_tree.InsertItem(wadLabel.c_str(), 0, 0, rootItem);
        m_treeNodeData[wadItem] = {TreeNodeKind::Wad, wadIndex, 0, L"", wadFile.assets.size(), wadFile.totalSize};

        std::unordered_map<std::wstring, std::pair<size_t, uint64_t>> folderStats;
        for (const auto& asset : wadFile.assets)
        {
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

        for (size_t assetIndex = 0; assetIndex < wadFile.assets.size(); ++assetIndex)
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

    std::wstring glText =
        L"GLView Metadata\r\n\r\n"
        L"Asset: " + asset.displayName + L"\r\n" +
        L"Type: " + WadArchiveModel::AssetTypeName(asset.assetType) + L"\r\n";
    if (asset.assetType == RMID_TYPE_MES || asset.assetType == RMID_TYPE_SKI)
        glText += L"\r\nMesh-family asset detected. Geometry rendering integration is the next step.";
    else
        glText += L"\r\nGL rendering is reserved for mesh, skin, and related assets.";
    SetGlText(glText);

    SetStatusText(L"Selected asset: " + asset.displayName);
}

void CExplorerView::ResetRightViews(const std::wstring& message)
{
    if (m_hexPage)
        m_hexPage->SetPageText(message);
    SetImageText(message);
    SetGlText(message);
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

void CExplorerView::SetGlText(const std::wstring& text)
{
    if (m_glPage)
        m_glPage->SetPageText(text);
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

BOOL CExplorerView::OnCommand(WPARAM wparam, LPARAM)
{
    const UINT id = LOWORD(wparam);
    const UINT code = HIWORD(wparam);

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
