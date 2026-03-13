#include "StdAfx.h"
#include "ImagePage.h"

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
