#include "StdAfx.h"
#include "TextPage.h"

namespace
{
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
