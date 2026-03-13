#ifndef WADEXPLORER_IMAGEPAGE_H
#define WADEXPLORER_IMAGEPAGE_H

#include "StdAfx.h"
#include "WadData.h"

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

#endif
