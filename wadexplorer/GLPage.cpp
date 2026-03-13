#include "StdAfx.h"
#include "GLPage.h"

#include <gl/GL.h>

namespace
{
    constexpr UINT_PTR kGlTimerId = 1;

    void DrawGrid(float extent, float step)
    {
        glColor3f(0.18f, 0.20f, 0.24f);
        glBegin(GL_LINES);
        for (float position = -extent; position <= extent; position += step)
        {
            glVertex3f(position, -1.4f, -extent);
            glVertex3f(position, -1.4f, extent);
            glVertex3f(-extent, -1.4f, position);
            glVertex3f(extent, -1.4f, position);
        }
        glEnd();
    }

    void DrawAxes(float length)
    {
        glBegin(GL_LINES);
        glColor3f(0.90f, 0.28f, 0.24f);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glVertex3f(length, 0.0f, 0.0f);
        glColor3f(0.24f, 0.78f, 0.34f);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glVertex3f(0.0f, length, 0.0f);
        glColor3f(0.26f, 0.48f, 0.94f);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glVertex3f(0.0f, 0.0f, length);
        glEnd();
    }

    void DrawWireCube(float halfExtent)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor3f(0.85f, 0.88f, 0.92f);
        glBegin(GL_QUADS);
        glVertex3f(-halfExtent, -halfExtent, halfExtent);
        glVertex3f(halfExtent, -halfExtent, halfExtent);
        glVertex3f(halfExtent, halfExtent, halfExtent);
        glVertex3f(-halfExtent, halfExtent, halfExtent);

        glVertex3f(-halfExtent, -halfExtent, -halfExtent);
        glVertex3f(-halfExtent, halfExtent, -halfExtent);
        glVertex3f(halfExtent, halfExtent, -halfExtent);
        glVertex3f(halfExtent, -halfExtent, -halfExtent);

        glVertex3f(-halfExtent, -halfExtent, -halfExtent);
        glVertex3f(-halfExtent, -halfExtent, halfExtent);
        glVertex3f(-halfExtent, halfExtent, halfExtent);
        glVertex3f(-halfExtent, halfExtent, -halfExtent);

        glVertex3f(halfExtent, -halfExtent, -halfExtent);
        glVertex3f(halfExtent, halfExtent, -halfExtent);
        glVertex3f(halfExtent, halfExtent, halfExtent);
        glVertex3f(halfExtent, -halfExtent, halfExtent);

        glVertex3f(-halfExtent, halfExtent, -halfExtent);
        glVertex3f(-halfExtent, halfExtent, halfExtent);
        glVertex3f(halfExtent, halfExtent, halfExtent);
        glVertex3f(halfExtent, halfExtent, -halfExtent);

        glVertex3f(-halfExtent, -halfExtent, -halfExtent);
        glVertex3f(halfExtent, -halfExtent, -halfExtent);
        glVertex3f(halfExtent, -halfExtent, halfExtent);
        glVertex3f(-halfExtent, -halfExtent, halfExtent);
        glEnd();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    void DrawPlaceholderPrimitive()
    {
        glBegin(GL_TRIANGLES);
        glColor3f(0.96f, 0.53f, 0.18f);
        glVertex3f(0.0f, 1.05f, 0.0f);
        glColor3f(0.94f, 0.24f, 0.32f);
        glVertex3f(-1.0f, -0.8f, 0.7f);
        glColor3f(0.26f, 0.61f, 0.97f);
        glVertex3f(1.0f, -0.8f, 0.7f);

        glColor3f(0.93f, 0.79f, 0.23f);
        glVertex3f(0.0f, 1.05f, 0.0f);
        glColor3f(0.26f, 0.61f, 0.97f);
        glVertex3f(1.0f, -0.8f, 0.7f);
        glColor3f(0.18f, 0.80f, 0.44f);
        glVertex3f(0.0f, -0.8f, -1.0f);

        glColor3f(0.93f, 0.79f, 0.23f);
        glVertex3f(0.0f, 1.05f, 0.0f);
        glColor3f(0.18f, 0.80f, 0.44f);
        glVertex3f(0.0f, -0.8f, -1.0f);
        glColor3f(0.94f, 0.24f, 0.32f);
        glVertex3f(-1.0f, -0.8f, 0.7f);
        glEnd();
    }

    std::wstring FormatWin32ErrorMessage(DWORD errorCode)
    {
        if (errorCode == 0)
            return L"Unknown error";

        LPWSTR buffer = nullptr;
        const DWORD length = ::FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            errorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&buffer),
            0,
            nullptr);

        std::wstring message = (length != 0 && buffer) ? std::wstring(buffer, length) : L"Unknown error";
        if (buffer)
            ::LocalFree(buffer);

        while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n'))
            message.pop_back();
        return message;
    }
}

void TrackballCamera::ResetForMesh(float radius)
{
    m_referenceRadius = std::max(0.5f, radius);
    m_yawDegrees = 34.0f;
    m_pitchDegrees = 18.0f;
    m_distance = std::max(3.0f, m_referenceRadius * 3.4f);
    m_panX = 0.0f;
    m_panY = -m_referenceRadius * 0.08f;
    m_dragMode = DragMode::None;
}

void TrackballCamera::ResetForScene(bool meshCandidate)
{
    m_referenceRadius = meshCandidate ? 1.8f : 1.2f;
    m_yawDegrees = meshCandidate ? 28.0f : 22.0f;
    m_pitchDegrees = meshCandidate ? 16.0f : 12.0f;
    m_distance = meshCandidate ? 5.4f : 4.2f;
    m_panX = 0.0f;
    m_panY = meshCandidate ? -0.12f : 0.0f;
    m_dragMode = DragMode::None;
}

void TrackballCamera::BeginOrbit(int x, int y)
{
    m_dragMode = DragMode::Orbit;
    m_lastPoint = POINT{ x, y };
}

void TrackballCamera::BeginPan(int x, int y)
{
    m_dragMode = DragMode::Pan;
    m_lastPoint = POINT{ x, y };
}

void TrackballCamera::EndDrag()
{
    m_dragMode = DragMode::None;
}

void TrackballCamera::UpdateDrag(int x, int y)
{
    const int deltaX = x - m_lastPoint.x;
    const int deltaY = y - m_lastPoint.y;
    m_lastPoint = POINT{ x, y };

    if (m_dragMode == DragMode::Orbit)
    {
        m_yawDegrees += static_cast<float>(deltaX) * 0.55f;
        m_pitchDegrees = std::clamp(m_pitchDegrees + (static_cast<float>(deltaY) * 0.45f), -89.0f, 89.0f);
    }
    else if (m_dragMode == DragMode::Pan)
    {
        const float panScale = std::max(0.0025f, m_distance * 0.0020f);
        m_panX += static_cast<float>(deltaX) * panScale;
        m_panY -= static_cast<float>(deltaY) * panScale;
    }
}

void TrackballCamera::Zoom(short wheelDelta)
{
    if (wheelDelta == 0)
        return;

    const float zoomStep = (wheelDelta > 0) ? 0.88f : 1.12f;
    m_distance = std::clamp(
        m_distance * zoomStep,
        std::max(1.0f, m_referenceRadius * 0.35f),
        std::max(8.0f, m_referenceRadius * 18.0f));
}

bool TrackballCamera::ProjectPoint(
    float x,
    float y,
    float z,
    float targetX,
    float targetY,
    float targetZ,
    int width,
    int height,
    POINT& point) const
{
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float nearPlane = 0.8f;
    const float frustumX = aspect * 0.55f;
    const float frustumY = 0.55f;
    const float yaw = m_yawDegrees * 3.1415926535f / 180.0f;
    const float pitch = m_pitchDegrees * 3.1415926535f / 180.0f;
    const float sinYaw = std::sin(yaw);
    const float cosYaw = std::cos(yaw);
    const float sinPitch = std::sin(pitch);
    const float cosPitch = std::cos(pitch);

    x -= targetX;
    y -= targetY;
    z -= targetZ;

    const float yawX = (x * cosYaw) + (z * sinYaw);
    const float yawZ = (-x * sinYaw) + (z * cosYaw);
    x = yawX;
    z = yawZ;

    const float pitchY = (y * cosPitch) - (z * sinPitch);
    const float pitchZ = (y * sinPitch) + (z * cosPitch);
    y = pitchY + m_panY;
    z = pitchZ - m_distance;
    x += m_panX;

    if (-z <= nearPlane)
        return false;

    const float ndcX = (nearPlane * x) / (frustumX * -z);
    const float ndcY = (nearPlane * y) / (frustumY * -z);
    if (ndcX < -1.6f || ndcX > 1.6f || ndcY < -1.6f || ndcY > 1.6f)
        return false;

    point.x = static_cast<LONG>(((ndcX * 0.5f) + 0.5f) * static_cast<float>(width));
    point.y = static_cast<LONG>(((-ndcY * 0.5f) + 0.5f) * static_cast<float>(height));
    return true;
}

void TrackballCamera::ApplyOpenGl(float targetX, float targetY, float targetZ) const
{
    glTranslatef(m_panX, m_panY, -m_distance);
    glRotatef(m_pitchDegrees, 1.0f, 0.0f, 0.0f);
    glRotatef(m_yawDegrees, 0.0f, 1.0f, 0.0f);
    glTranslatef(-targetX, -targetY, -targetZ);
}

void CGlPage::Clear(const std::wstring& message)
{
    m_hasMeshPreview = false;
    m_meshPositions.clear();
    m_meshNormals.clear();
    m_meshIndices.clear();
    m_meshCenterX = 0.0f;
    m_meshCenterY = 0.0f;
    m_meshCenterZ = 0.0f;
    m_meshRadius = 1.0f;
    m_title = L"GLView";
    m_detail = message;
    m_meshCandidate = false;
    m_camera.ResetForScene(false);
    if (IsWindow())
        RedrawWindow(RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
}

void CGlPage::SetMeshPreview(const WadMeshPreview& preview, const std::wstring& title, const std::wstring& detail)
{
    m_hasMeshPreview = true;
    m_meshPositions = preview.positions;
    m_meshNormals = preview.normals;
    m_meshIndices = preview.indices;
    m_meshCenterX = preview.centerX;
    m_meshCenterY = preview.centerY;
    m_meshCenterZ = preview.centerZ;
    m_meshRadius = preview.radius;
    m_title = title;
    m_detail = detail + L"\r\n\r\n" + preview.description +
        L"\r\n\r\nControls: LMB orbit, RMB pan, Wheel zoom, Double-click reset.";
    m_meshCandidate = true;
    m_camera.ResetForMesh(m_meshRadius);
    if (IsWindow())
        RedrawWindow(RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
}

void CGlPage::SetScene(const std::wstring& title, const std::wstring& detail, bool meshCandidate)
{
    m_hasMeshPreview = false;
    m_meshPositions.clear();
    m_meshNormals.clear();
    m_meshIndices.clear();
    m_title = title;
    m_detail = detail + L"\r\n\r\nControls: LMB orbit, RMB pan, Wheel zoom, Double-click reset.";
    m_meshCandidate = meshCandidate;
    m_camera.ResetForScene(meshCandidate);
    if (IsWindow())
        RedrawWindow(RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
}

void CGlPage::OnAttach()
{
    if (m_title.empty())
        m_title = L"GLView";
    if (m_detail.empty())
        m_detail = L"Select a mesh or skin asset to activate the OpenGL preview.";

    m_glReady = InitializeOpenGl();
    if (m_glReady)
        m_timerId = ::SetTimer(GetHwnd(), kGlTimerId, 16, nullptr);
}

void CGlPage::PreCreate(CREATESTRUCT& cs)
{
    CWnd::PreCreate(cs);
    cs.dwExStyle |= WS_EX_CLIENTEDGE;
    cs.style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
}

void CGlPage::PreRegisterClass(WNDCLASS& wc)
{
    CWnd::PreRegisterClass(wc);
    wc.style |= CS_OWNDC;
}

bool CGlPage::InitializeOpenGl()
{
    HDC hdc = ::GetDC(GetHwnd());
    if (!hdc)
    {
        m_detail = L"Failed to acquire an HDC for OpenGL initialization.";
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    const int pixelFormat = ::ChoosePixelFormat(hdc, &pfd);
    if (pixelFormat == 0 || !::SetPixelFormat(hdc, pixelFormat, &pfd))
    {
        const DWORD error = ::GetLastError();
        ::ReleaseDC(GetHwnd(), hdc);
        m_detail = L"Failed to configure the OpenGL pixel format.\r\n" + FormatWin32ErrorMessage(error);
        return false;
    }

    m_glrc = ::wglCreateContext(hdc);
    if (!m_glrc)
    {
        const DWORD error = ::GetLastError();
        ::ReleaseDC(GetHwnd(), hdc);
        m_detail = L"wglCreateContext failed.\r\n" + FormatWin32ErrorMessage(error);
        return false;
    }

    if (!::wglMakeCurrent(hdc, m_glrc))
    {
        const DWORD error = ::GetLastError();
        ::wglDeleteContext(m_glrc);
        m_glrc = nullptr;
        ::ReleaseDC(GetHwnd(), hdc);
        m_detail = L"wglMakeCurrent failed.\r\n" + FormatWin32ErrorMessage(error);
        return false;
    }

    glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);
    glDisable(GL_CULL_FACE);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    ::wglMakeCurrent(nullptr, nullptr);
    ::ReleaseDC(GetHwnd(), hdc);
    return true;
}

void CGlPage::DestroyOpenGl()
{
    if (m_timerId != 0)
    {
        ::KillTimer(GetHwnd(), m_timerId);
        m_timerId = 0;
    }

    if (m_glrc)
    {
        HDC hdc = ::GetDC(GetHwnd());
        ::wglMakeCurrent(nullptr, nullptr);
        ::wglDeleteContext(m_glrc);
        m_glrc = nullptr;
        if (hdc)
            ::ReleaseDC(GetHwnd(), hdc);
    }

    m_glReady = false;
}

void CGlPage::DrawOverlay(HDC hdc) const
{
    if (m_hasMeshPreview && m_glReady)
        return;

    RECT rc{};
    ::GetClientRect(GetHwnd(), &rc);
    RECT textRect = rc;
    textRect.left += 14;
    textRect.top += 12;
    textRect.right -= 14;

    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, RGB(224, 228, 235));

    std::wstring overlay = m_title;
    if (!m_detail.empty())
    {
        overlay += L"\r\n";
        overlay += m_detail;
    }

    ::DrawTextW(hdc, overlay.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
}

void CGlPage::DrawMeshWireframeFallback(HDC hdc) const
{
    if (!m_hasMeshPreview || m_meshPositions.size() < 3 || m_meshIndices.size() < 3)
        return;

    RECT clientRect{};
    ::GetClientRect(GetHwnd(), &clientRect);
    const int width = std::max(1, static_cast<int>(clientRect.right - clientRect.left));
    const int height = std::max(1, static_cast<int>(clientRect.bottom - clientRect.top));
    const float sceneRadius = std::max(0.5f, m_meshRadius);
    const float modelScale = 1.8f / sceneRadius;

    struct ProjectedVertex
    {
        POINT point{};
        bool visible = false;
    };

    std::vector<ProjectedVertex> projected(static_cast<size_t>(m_meshPositions.size() / 3));
    for (size_t vertexIndex = 0; vertexIndex < projected.size(); ++vertexIndex)
    {
        const size_t source = vertexIndex * 3;
        ProjectedVertex pv{};
        pv.visible = m_camera.ProjectPoint(
            m_meshPositions[source + 0] * modelScale,
            m_meshPositions[source + 1] * modelScale,
            m_meshPositions[source + 2] * modelScale,
            m_meshCenterX * modelScale,
            m_meshCenterY * modelScale,
            m_meshCenterZ * modelScale,
            width,
            height,
            pv.point);
        projected[vertexIndex] = pv;
    }

    HPEN wirePen = ::CreatePen(PS_SOLID, 1, RGB(238, 243, 252));
    HPEN pointPen = ::CreatePen(PS_SOLID, 1, RGB(255, 213, 64));
    HGDIOBJ oldPen = ::SelectObject(hdc, wirePen);

    const int pointRadius = 1;
    for (size_t index = 0; index + 2 < m_meshIndices.size(); index += 3)
    {
        const uint32_t i0 = m_meshIndices[index + 0];
        const uint32_t i1 = m_meshIndices[index + 1];
        const uint32_t i2 = m_meshIndices[index + 2];
        if (i0 >= projected.size() || i1 >= projected.size() || i2 >= projected.size())
            continue;

        const auto& p0 = projected[i0];
        const auto& p1 = projected[i1];
        const auto& p2 = projected[i2];
        if (!p0.visible || !p1.visible || !p2.visible)
            continue;

        ::MoveToEx(hdc, p0.point.x, p0.point.y, nullptr);
        ::LineTo(hdc, p1.point.x, p1.point.y);
        ::LineTo(hdc, p2.point.x, p2.point.y);
        ::LineTo(hdc, p0.point.x, p0.point.y);
    }

    ::SelectObject(hdc, pointPen);
    HGDIOBJ oldBrush = ::SelectObject(hdc, ::GetStockObject(DC_BRUSH));
    ::SetDCBrushColor(hdc, RGB(255, 213, 64));
    for (const auto& vertex : projected)
    {
        if (!vertex.visible)
            continue;

        RECT dot{
            vertex.point.x - pointRadius,
            vertex.point.y - pointRadius,
            vertex.point.x + pointRadius + 1,
            vertex.point.y + pointRadius + 1
        };
        ::FillRect(hdc, &dot, static_cast<HBRUSH>(::GetStockObject(DC_BRUSH)));
    }

    ::SelectObject(hdc, oldBrush);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(pointPen);
    ::DeleteObject(wirePen);
}

void CGlPage::RenderFrame(HDC hdc)
{
    if (!m_glReady || !m_glrc)
    {
        RECT rc{};
        ::GetClientRect(GetHwnd(), &rc);
        HBRUSH brush = ::CreateSolidBrush(RGB(24, 25, 29));
        ::FillRect(hdc, &rc, brush);
        ::DeleteObject(brush);
        DrawMeshWireframeFallback(hdc);
        DrawOverlay(hdc);
        return;
    }

    if (!::wglMakeCurrent(hdc, m_glrc))
    {
        m_glReady = false;
        m_detail = L"Render-time wglMakeCurrent failed.\r\n" + FormatWin32ErrorMessage(::GetLastError());
        RECT rc{};
        ::GetClientRect(GetHwnd(), &rc);
        HBRUSH brush = ::CreateSolidBrush(RGB(24, 25, 29));
        ::FillRect(hdc, &rc, brush);
        ::DeleteObject(brush);
        DrawMeshWireframeFallback(hdc);
        DrawOverlay(hdc);
        return;
    }

    RECT clientRect{};
    ::GetClientRect(GetHwnd(), &clientRect);
    const int width = std::max(1, static_cast<int>(clientRect.right - clientRect.left));
    const int height = std::max(1, static_cast<int>(clientRect.bottom - clientRect.top));
    const double aspect = static_cast<double>(width) / static_cast<double>(height);

    glViewport(0, 0, width, height);
    if (m_meshCandidate)
        glClearColor(0.06f, 0.08f, 0.11f, 1.0f);
    else
        glClearColor(0.10f, 0.08f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-aspect * 0.55, aspect * 0.55, -0.55, 0.55, 0.8, 64.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const GLfloat lightPosition[] = { 0.35f, 0.8f, 0.55f, 0.0f };
    const GLfloat lightDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    const GLfloat lightAmbient[] = { 0.35f, 0.35f, 0.38f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);

    const float sceneRadius = m_hasMeshPreview ? std::max(0.5f, m_meshRadius) : 1.4f;
    if (m_hasMeshPreview || m_meshCandidate)
    {
        m_camera.ApplyOpenGl(0.0f, 0.0f, 0.0f);
    }
    else
    {
        glTranslatef(0.0f, -0.10f, -5.0f);
        glRotatef(14.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(m_rotationDegrees, 0.0f, 1.0f, 0.0f);
    }

    glDisable(GL_LIGHTING);
    DrawGrid(std::max(4.0f, sceneRadius * 2.8f), std::max(0.5f, sceneRadius * 0.35f));
    DrawAxes(std::max(1.8f, sceneRadius * 1.25f));

    if (m_hasMeshPreview)
    {
        const float modelScale = 1.8f / std::max(0.5f, m_meshRadius);
        glScalef(modelScale, modelScale, modelScale);
        glTranslatef(-m_meshCenterX, -m_meshCenterY, -m_meshCenterZ);

        glDisable(GL_LIGHTING);
        glColor3f(0.72f, 0.82f, 0.96f);
        glBegin(GL_TRIANGLES);
        for (size_t index = 0; index + 2 < m_meshIndices.size(); index += 3)
        {
            for (size_t corner = 0; corner < 3; ++corner)
            {
                const uint32_t vertex = m_meshIndices[index + corner];
                const size_t positionOffset = static_cast<size_t>(vertex) * 3;
                glNormal3f(
                    m_meshNormals[positionOffset + 0],
                    m_meshNormals[positionOffset + 1],
                    m_meshNormals[positionOffset + 2]);
                glVertex3f(
                    m_meshPositions[positionOffset + 0],
                    m_meshPositions[positionOffset + 1],
                    m_meshPositions[positionOffset + 2]);
            }
        }
        glEnd();

        glPointSize(4.0f);
        glColor3f(1.0f, 0.85f, 0.20f);
        glBegin(GL_POINTS);
        for (size_t index = 0; index + 2 < m_meshPositions.size(); index += 3)
        {
            glVertex3f(
                m_meshPositions[index + 0],
                m_meshPositions[index + 1],
                m_meshPositions[index + 2]);
        }
        glEnd();

        glColor3f(0.98f, 0.98f, 0.98f);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glBegin(GL_TRIANGLES);
        for (size_t index = 0; index + 2 < m_meshIndices.size(); index += 3)
        {
            for (size_t corner = 0; corner < 3; ++corner)
            {
                const uint32_t vertex = m_meshIndices[index + corner];
                const size_t positionOffset = static_cast<size_t>(vertex) * 3;
                glVertex3f(
                    m_meshPositions[positionOffset + 0],
                    m_meshPositions[positionOffset + 1],
                    m_meshPositions[positionOffset + 2]);
            }
        }
        glEnd();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_LINE);
    }
    else if (m_meshCandidate)
    {
        DrawWireCube(1.15f);
    }
    else
    {
        glTranslatef(0.0f, 0.35f, 0.0f);
        DrawPlaceholderPrimitive();
    }

    glFlush();
    if (!::SwapBuffers(hdc))
    {
        m_glReady = false;
        m_detail = L"SwapBuffers failed.\r\n" + FormatWin32ErrorMessage(::GetLastError());
        ::wglMakeCurrent(nullptr, nullptr);
        RECT rc{};
        ::GetClientRect(GetHwnd(), &rc);
        HBRUSH brush = ::CreateSolidBrush(RGB(24, 25, 29));
        ::FillRect(hdc, &rc, brush);
        ::DeleteObject(brush);
        DrawOverlay(hdc);
        return;
    }
    ::wglMakeCurrent(nullptr, nullptr);

    DrawMeshWireframeFallback(hdc);
    DrawOverlay(hdc);
}

LRESULT CGlPage::WndProc(UINT msg, WPARAM wparam, LPARAM lparam)
{
    try
    {
        switch (msg)
        {
        case WM_ERASEBKGND:
            return TRUE;
        case WM_SIZE:
            m_clientWidth = std::max(1, static_cast<int>(LOWORD(lparam)));
            m_clientHeight = std::max(1, static_cast<int>(HIWORD(lparam)));
            Invalidate(FALSE);
            return 0;
        case WM_TIMER:
            if (wparam == kGlTimerId)
            {
                if (!m_hasMeshPreview)
                    m_rotationDegrees = std::fmod(m_rotationDegrees + (m_meshCandidate ? 0.8f : 1.3f), 360.0f);
                Invalidate(FALSE);
                return 0;
            }
            break;
        case WM_LBUTTONDOWN:
            m_camera.BeginOrbit(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            ::SetCapture(GetHwnd());
            return 0;
        case WM_RBUTTONDOWN:
            m_camera.BeginPan(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            ::SetCapture(GetHwnd());
            return 0;
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
            if (m_hasMeshPreview)
                m_camera.ResetForMesh(m_meshRadius);
            else
                m_camera.ResetForScene(m_meshCandidate);
            Invalidate(FALSE);
            return 0;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
            m_camera.EndDrag();
            if (::GetCapture() == GetHwnd())
                ::ReleaseCapture();
            return 0;
        case WM_MOUSEMOVE:
            if ((wparam & MK_LBUTTON) != 0 || (wparam & MK_RBUTTON) != 0)
            {
                m_camera.UpdateDrag(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                Invalidate(FALSE);
            }
            return 0;
        case WM_MOUSEWHEEL:
            m_camera.Zoom(GET_WHEEL_DELTA_WPARAM(wparam));
            Invalidate(FALSE);
            return 0;
        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC hdc = ::BeginPaint(GetHwnd(), &ps);
            RenderFrame(hdc);
            ::EndPaint(GetHwnd(), &ps);
            return 0;
        }
        case WM_DESTROY:
            DestroyOpenGl();
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
