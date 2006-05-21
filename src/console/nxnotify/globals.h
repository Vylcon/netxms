/* 
** NetXMS - Network Management System
** Alarm Viewer
** Copyright (C) 2004 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** $module: globals.h
**
**/

#ifndef _globals_h_
#define _globals_h_


//
// Custom windows messages
//

#define NXNM_REQUEST_COMPLETED   (WM_USER + 1)
#define NXNM_SET_INFO_TEXT       (WM_USER + 2)
#define NXNM_ALARM_UPDATE        (WM_USER + 3)
#define NXNM_TASKBAR_CALLBACK    (WM_USER + 4)


//
// Login options
//

#define OPT_MATCH_SERVER_VERSION    0x0001
#define OPT_ENCRYPT_CONNECTION      0x0002


//
// Popup actions
//

#define NXNOTIFY_ACTION_ACK_ALARM      0
#define NXNOTIFY_ACTION_DISMISS        1
#define NXNOTIFY_ACTION_OPEN_LIST      2
#define NXNOTIFY_ACTION_OPEN_CONSOLE   3
#define NXNOTIFY_ACTION_NONE           4


//
// Functions
//

DWORD DoLogin(void);
DWORD DoRequest(DWORD (* pFunc)(void), char *pszInfoText);
DWORD DoRequestArg1(void *pFunc, void *pArg1, char *pszInfoText);
DWORD DoRequestArg2(void *pFunc, void *pArg1, void *pArg2, char *pszInfoText);
DWORD DoRequestArg3(void *pFunc, void *pArg1, void *pArg2, void *pArg3, char *pszInfoText);
DWORD DoRequestArg4(void *pFunc, void *pArg1, void *pArg2, void *pArg3, void *pArg4, char *pszInfoText);
DWORD DoRequestArg6(void *pFunc, void *pArg1, void *pArg2, void *pArg3, void *pArg4, 
                    void *pArg5, void *pArg6, char *pszInfoText);
BOOL FileFromResource(UINT nResId, TCHAR *pszFileName);
void CreateMonitorList(void);


//
// Variables
//

extern NXC_SESSION g_hSession;
extern TCHAR g_szServer[];
extern TCHAR g_szLogin[];
extern TCHAR g_szPassword[];
extern TCHAR *g_szStatusText[];
extern TCHAR *g_szStatusTextSmall[];
extern TCHAR g_szWorkDir[];
extern DWORD g_dwOptions;
extern CImageList g_imgListSeverity;
extern DWORD g_dwPopupTimeout;
extern int g_nActionLeft;
extern int g_nActionRight;
extern int g_nActionDblClk;


#endif
