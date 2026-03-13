#ifndef WADEXPLORER_APP_H
#define WADEXPLORER_APP_H

#include "Mainfrm.h"

class CWadExplorerApp : public CWinApp
{
public:
    CWadExplorerApp() = default;
    virtual ~CWadExplorerApp() override = default;

protected:
    virtual BOOL InitInstance() override;

private:
    CWadExplorerApp(const CWadExplorerApp&) = delete;
    CWadExplorerApp& operator=(const CWadExplorerApp&) = delete;

    CMainFrame m_frame;
};

#endif
