#ifndef WADEXPLORER_EXPLORERVIEW_H
#define WADEXPLORER_EXPLORERVIEW_H

#include "StdAfx.h"
#include "GLPage.h"
#include "ImagePage.h"
#include "resource.h"
#include "TextPage.h"
#include "WadData.h"

class CAssetTreeView : public CTreeView
{
public:
    CAssetTreeView() = default;
    virtual ~CAssetTreeView() override = default;

    void ResetPlaceholderTree();

protected:
    virtual void OnAttach() override;
};


class CExplorerView : public CWnd
{
public:
    CExplorerView();
    virtual ~CExplorerView() override = default;

protected:
    virtual int OnCreate(CREATESTRUCT& cs) override;
    virtual void OnInitialUpdate() override;
    virtual void PreCreate(CREATESTRUCT& cs) override;
    virtual LRESULT WndProc(UINT msg, WPARAM wparam, LPARAM lparam) override;

private:
    CExplorerView(const CExplorerView&) = delete;
    CExplorerView& operator=(const CExplorerView&) = delete;

    BOOL OnCommand(WPARAM wparam, LPARAM lparam);
    LRESULT OnNotify(WPARAM wparam, LPARAM lparam);
    LRESULT OnLButtonDown(UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT OnLButtonUp(UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT OnMouseMove(UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT OnSetCursor(UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT OnSize(UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT OnContextMenu(UINT msg, WPARAM wparam, LPARAM lparam);

    void CreateChildWindows();
    void LayoutChildren();
    void OpenFolder();
    void OpenFile();
    void PopulateForFolder(const std::filesystem::path& folderPath);
    void PopulateForFile(const std::filesystem::path& filePath);
    void PopulateTreeFromModel();
    void UpdateRightPaneForSelection();
    void ShowWadNode(size_t wadIndex);
    void ShowFolderNode(size_t wadIndex, const std::wstring& folderKey, size_t assetCount, uint64_t totalSize);
    void ShowAssetNode(size_t wadIndex, size_t assetIndex);
    void ResetRightViews(const std::wstring& message);
    void RebuildFilteredTree();
    void SetImageText(const std::wstring& text);
    void SetImagePreview(const WadImagePreview& preview);
    void SetGlMeshPreview(const WadMeshPreview& preview, const std::wstring& title, const std::wstring& detail);
    void SetGlScene(const std::wstring& title, const std::wstring& detail, bool meshCandidate);
    void SetGlText(const std::wstring& text);
    void SetRootPath(const std::wstring& pathText);
    void SetStatusText(const std::wstring& text);
    bool IsOnSplitter(int x) const;
    bool AssetMatchesFilter(const WadAsset& asset) const;
    void ShowTreeContextMenu(POINT screenPoint);
    void ExportAssetRaw(size_t wadIndex, size_t assetIndex);

    enum class TreeNodeKind
    {
        Root,
        Wad,
        Folder,
        Asset
    };

    struct TreeNodeData
    {
        TreeNodeKind kind = TreeNodeKind::Root;
        size_t wadIndex = 0;
        size_t assetIndex = 0;
        std::wstring folderKey;
        size_t assetCount = 0;
        uint64_t totalSize = 0;
    };

    enum class AssetFilter
    {
        All = 0,
        Mesh,
        Skin,
        Texture,
        Actor,
        Sound,
        Other,
    };

    static constexpr int kMinLeftPaneWidth = 220;
    static constexpr int kMinRightPaneWidth = 320;

    CStatic m_rootLabel;
    CEdit m_pathEdit;
    CStatic m_filterLabel;
    CComboBox m_filterCombo;
    CButton m_openFolderButton;
    CButton m_openFileButton;
    CAssetTreeView m_tree;
    CTab m_tabs;
    CStatic m_statusText;
    CProgressBar m_progress;

    CTextPage* m_hexPage;
    CImagePage* m_imagePage;
    CGlPage* m_glPage;

    WadArchiveModel m_model;
    std::unordered_map<HTREEITEM, TreeNodeData> m_treeNodeData;
    AssetFilter m_assetFilter;
    int m_leftPaneWidth;
    int m_splitterWidth;
    bool m_isDraggingSplitter;
};

#endif
