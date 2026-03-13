#ifndef WADEXPLORER_EXPLORERVIEW_H
#define WADEXPLORER_EXPLORERVIEW_H

#include "StdAfx.h"
#include "resource.h"
#include "WadData.h"

class CTextPage : public CEdit
{
public:
    CTextPage(const std::wstring& initialText, bool fixedFont = false);
    virtual ~CTextPage() override = default;

    void SetPageText(const std::wstring& text);

protected:
    virtual void OnAttach() override;
    virtual void PreCreate(CREATESTRUCT& cs) override;

private:
    std::wstring m_initialText;
    bool m_useFixedFont;
    CFont m_font;
};


class CImagePage : public CWnd
{
public:
    CImagePage() = default;
    virtual ~CImagePage() override = default;

    void Clear(const std::wstring& message);
    void SetPreview(const WadImagePreview& preview);

protected:
    virtual void OnDraw(CDC& dc) override;
    virtual void PreCreate(CREATESTRUCT& cs) override;
    virtual LRESULT WndProc(UINT msg, WPARAM wparam, LPARAM lparam) override;

private:
    CImagePage(const CImagePage&) = delete;
    CImagePage& operator=(const CImagePage&) = delete;

    std::wstring m_message;
    std::wstring m_description;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::vector<uint8_t> m_rgba;
};


class CGlPage : public CWnd
{
public:
    CGlPage() = default;
    virtual ~CGlPage() override = default;

    void Clear(const std::wstring& message);
    void SetMeshPreview(const WadMeshPreview& preview, const std::wstring& title, const std::wstring& detail);
    void SetScene(const std::wstring& title, const std::wstring& detail, bool meshCandidate);

protected:
    virtual void OnAttach() override;
    virtual void PreCreate(CREATESTRUCT& cs) override;
    virtual void PreRegisterClass(WNDCLASS& wc) override;
    virtual LRESULT WndProc(UINT msg, WPARAM wparam, LPARAM lparam) override;

private:
    CGlPage(const CGlPage&) = delete;
    CGlPage& operator=(const CGlPage&) = delete;

    bool InitializeOpenGl();
    void DestroyOpenGl();
    void RenderFrame(HDC hdc);
    void DrawOverlay(HDC hdc) const;
    void DrawMeshWireframeFallback(HDC hdc) const;

    bool m_hasMeshPreview = false;
    std::wstring m_title;
    std::wstring m_detail;
    bool m_meshCandidate = false;
    bool m_glReady = false;
    std::vector<float> m_meshPositions;
    std::vector<float> m_meshNormals;
    std::vector<uint32_t> m_meshIndices;
    float m_meshCenterX = 0.0f;
    float m_meshCenterY = 0.0f;
    float m_meshCenterZ = 0.0f;
    float m_meshRadius = 1.0f;
    HGLRC m_glrc = nullptr;
    UINT_PTR m_timerId = 0;
    float m_rotationDegrees = 0.0f;
    int m_clientWidth = 1;
    int m_clientHeight = 1;
};


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
