#ifndef _nxuilib_h_
#define _nxuilib_h_

#include <nms_common.h>
#include <nxclapi.h>
#include <nxwinui.h>
#include "resource.h"


//
// Defines
//

#define NXUILIB_BASE_REGISTRY_KEY	_T("Software\\NetXMS\\NetXMS UI Library\\")


//
// Global variables
//

extern TCHAR *g_pszSoundNames[];
extern int g_nSoundId[];
extern HINSTANCE g_hInstance;


#endif