#include "StdAfx.h"
#include "ExplorerView.h"

#include <gl/GL.h>

#include <iomanip>
#include <sstream>

namespace
{
    constexpr UINT_PTR kSplitterGripWidth = 8;
    constexpr int kToolbarHeight = 36;
    constexpr int kStatusHeight = 28;
    constexpr int kPadding = 8;
    constexpr UINT_PTR kGlTimerId = 1;

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
    m_detail = detail + L"\r\n\r\n" + preview.description;
    m_meshCandidate = true;
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
    m_detail = detail;
    m_meshCandidate = meshCandidate;
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
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float sceneRadius = std::max(0.5f, m_meshRadius);
    const float modelScale = 1.8f / sceneRadius;
    const float rotationY = m_rotationDegrees * 3.1415926535f / 180.0f;
    const float rotationX = 14.0f * 3.1415926535f / 180.0f;
    const float sinY = std::sin(rotationY);
    const float cosY = std::cos(rotationY);
    const float sinX = std::sin(rotationX);
    const float cosX = std::cos(rotationX);
    const float nearPlane = 0.8f;
    const float frustumX = aspect * 0.55f;
    const float frustumY = 0.55f;

    struct ProjectedVertex
    {
        POINT point{};
        bool visible = false;
    };

    std::vector<ProjectedVertex> projected(static_cast<size_t>(m_meshPositions.size() / 3));
    for (size_t vertexIndex = 0; vertexIndex < projected.size(); ++vertexIndex)
    {
        const size_t source = vertexIndex * 3;
        float x = (m_meshPositions[source + 0] - m_meshCenterX) * modelScale;
        float y = (m_meshPositions[source + 1] - m_meshCenterY) * modelScale;
        float z = (m_meshPositions[source + 2] - m_meshCenterZ) * modelScale;

        const float yawX = (x * cosY) + (z * sinY);
        const float yawZ = (-x * sinY) + (z * cosY);
        x = yawX;
        z = yawZ;

        const float pitchY = (y * cosX) - (z * sinX);
        const float pitchZ = (y * sinX) + (z * cosX);
        y = pitchY - 0.10f;
        z = pitchZ - 5.0f;

        if (-z <= nearPlane)
            continue;

        const float ndcX = (nearPlane * x) / (frustumX * -z);
        const float ndcY = (nearPlane * y) / (frustumY * -z);
        if (ndcX < -1.4f || ndcX > 1.4f || ndcY < -1.4f || ndcY > 1.4f)
            continue;

        ProjectedVertex pv{};
        pv.point.x = static_cast<LONG>(((ndcX * 0.5f) + 0.5f) * static_cast<float>(width));
        pv.point.y = static_cast<LONG>(((-ndcY * 0.5f) + 0.5f) * static_cast<float>(height));
        pv.visible = true;
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
    glTranslatef(0.0f, -0.10f, -5.0f);
    glRotatef(14.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(m_rotationDegrees, 0.0f, 1.0f, 0.0f);

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
                m_rotationDegrees = std::fmod(m_rotationDegrees + (m_meshCandidate ? 0.8f : 1.3f), 360.0f);
                Invalidate(FALSE);
                return 0;
            }
            break;
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
