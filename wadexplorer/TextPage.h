#ifndef WADEXPLORER_TEXTPAGE_H
#define WADEXPLORER_TEXTPAGE_H

#include "StdAfx.h"

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

#endif
