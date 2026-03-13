#include "StdAfx.h"
#include "Mainfrm.h"

HWND CMainFrame::Create(HWND parent)
{
    SetView(m_view);
    LoadRegistrySettings(L"DefianceTools\\WadExplorer");
    return CFrame::Create(parent);
}

int CMainFrame::OnCreate(CREATESTRUCT& cs)
{
    UseMenuStatus(FALSE);
    UseStatusBar(FALSE);
    UseToolBar(FALSE);
    UseReBar(FALSE);
    return CFrame::OnCreate(cs);
}

void CMainFrame::OnInitialUpdate()
{
    SetWindowText(L"Defiance WAD Explorer");
    ShowWindow();
}

void CMainFrame::PreCreate(CREATESTRUCT& cs)
{
    CFrame::PreCreate(cs);
    cs.style &= ~WS_VISIBLE;
}

LRESULT CMainFrame::WndProc(UINT msg, WPARAM wparam, LPARAM lparam)
{
    try
    {
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
