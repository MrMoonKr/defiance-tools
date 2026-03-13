#ifndef WADEXPLORER_GLPAGE_H
#define WADEXPLORER_GLPAGE_H

#include "StdAfx.h"
#include "WadData.h"

class TrackballCamera
{
public:
    enum class DragMode
    {
        None,
        Orbit,
        Pan,
    };

    void ResetForMesh(float radius);
    void ResetForScene(bool meshCandidate);
    void BeginOrbit(int x, int y);
    void BeginPan(int x, int y);
    void EndDrag();
    void UpdateDrag(int x, int y);
    void Zoom(short wheelDelta);
    bool ProjectPoint(
        float x,
        float y,
        float z,
        float targetX,
        float targetY,
        float targetZ,
        int width,
        int height,
        POINT& point) const;
    void ApplyOpenGl(float targetX, float targetY, float targetZ) const;

private:
    float m_yawDegrees = 32.0f;
    float m_pitchDegrees = 16.0f;
    float m_distance = 5.0f;
    float m_panX = 0.0f;
    float m_panY = -0.10f;
    float m_referenceRadius = 1.0f;
    DragMode m_dragMode = DragMode::None;
    POINT m_lastPoint{ 0, 0 };
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
    TrackballCamera m_camera;
    HGLRC m_glrc = nullptr;
    UINT_PTR m_timerId = 0;
    float m_rotationDegrees = 0.0f;
    int m_clientWidth = 1;
    int m_clientHeight = 1;
};

#endif
