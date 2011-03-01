/* 
** NetXMS multiplatform core agent
** Copyright (C) 2003-2011 Victor Kirhenshtein
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
** File: subagent.cpp
**
**/

#include "nxagentd.h"

#ifdef _NETWARE
#include <fsio.h>
#include <library.h>
#endif


//
// Static data
//

static DWORD m_dwNumSubAgents = 0;
static SUBAGENT *m_pSubAgentList = NULL;


//
// Initialize subagent
// Note: pszEntryPoint ignored on all pltforms except NetWare
//

BOOL InitSubAgent(HMODULE hModule, TCHAR *pszModuleName,
                  BOOL (* SubAgentRegister)(NETXMS_SUBAGENT_INFO **, Config *),
                  TCHAR *pszEntryPoint)
{
   NETXMS_SUBAGENT_INFO *pInfo;
   BOOL bSuccess = FALSE, bInitOK;

   if (SubAgentRegister(&pInfo, g_config))
   {
      // Check if information structure is valid
      if (pInfo->magic == NETXMS_SUBAGENT_INFO_MAGIC)
      {
         DWORD i;

         // Check if subagent with given name alreay loaded
         for(i = 0; i < m_dwNumSubAgents; i++)
            if (!_tcsicmp(m_pSubAgentList[i].pInfo->name, pInfo->name))
               break;
         if (i == m_dwNumSubAgents)
         {
				// Initialize subagent
				bInitOK = (pInfo->init != NULL) ? pInfo->init(g_config) : TRUE;
				if (bInitOK)
				{
					// Add subagent to subagent's list
					m_pSubAgentList = (SUBAGENT *)realloc(m_pSubAgentList, 
													sizeof(SUBAGENT) * (m_dwNumSubAgents + 1));
					m_pSubAgentList[m_dwNumSubAgents].hModule = hModule;
					nx_strncpy(m_pSubAgentList[m_dwNumSubAgents].szName, pszModuleName, MAX_PATH);
					m_pSubAgentList[m_dwNumSubAgents].pInfo = pInfo;
#ifdef _NETWARE
	            nx_strncpy(m_pSubAgentList[m_dwNumSubAgents].szEntryPoint, pszEntryPoint, 256);
#endif
					m_dwNumSubAgents++;

					// Add parameters provided by this subagent to common list
					for(i = 0; i < pInfo->numParameters; i++)
						AddParameter(pInfo->parameters[i].name, 
										 pInfo->parameters[i].handler,
										 pInfo->parameters[i].arg,
										 pInfo->parameters[i].dataType,
										 pInfo->parameters[i].description);

					// Add push parameters provided by this subagent to common list
					for(i = 0; i < pInfo->numPushParameters; i++)
						AddPushParameter(pInfo->pushParameters[i].name, 
						                 pInfo->pushParameters[i].dataType,
					                    pInfo->pushParameters[i].description);

					// Add lists provided by this subagent to common list
					for(i = 0; i < pInfo->numLists; i++)
						AddList(pInfo->lists[i].name, 
								  pInfo->lists[i].handler,
								  pInfo->lists[i].arg);

					// Add tables provided by this subagent to common list
					for(i = 0; i < pInfo->numTables; i++)
						AddTable(pInfo->tables[i].name, 
								   pInfo->tables[i].handler,
								   pInfo->tables[i].arg);

					// Add actions provided by this subagent to common list
					for(i = 0; i < pInfo->numActions; i++)
						AddAction(pInfo->actions[i].name,
									 AGENT_ACTION_SUBAGENT,
									 pInfo->actions[i].arg,
									 pInfo->actions[i].handler,
									 pInfo->name,
									 pInfo->actions[i].description);

					nxlog_write(MSG_SUBAGENT_LOADED, EVENTLOG_INFORMATION_TYPE,
								   "s", pszModuleName);
					bSuccess = TRUE;
				}
				else
				{
					nxlog_write(MSG_SUBAGENT_INIT_FAILED, EVENTLOG_ERROR_TYPE,
								   "s", pszModuleName);
					DLClose(hModule);
				}
         }
         else
         {
            nxlog_write(MSG_SUBAGENT_ALREADY_LOADED, EVENTLOG_WARNING_TYPE,
                        "ss", pInfo->name, m_pSubAgentList[i].szName);
            // On NetWare, DLClose will unload module, and if first instance
            // was loaded from the same module, it will be killed, so
            // we don't call DLClose on NetWare
#ifndef _NETWARE
            DLClose(hModule);
#endif
         }
      }
      else
      {
         nxlog_write(MSG_SUBAGENT_BAD_MAGIC, EVENTLOG_ERROR_TYPE,
                     "s", pszModuleName);
         DLClose(hModule);
      }
   }
   else
   {
      nxlog_write(MSG_SUBAGENT_REGISTRATION_FAILED, EVENTLOG_ERROR_TYPE,
                  "s", pszModuleName);
      DLClose(hModule);
   }

   return bSuccess;
}


//
// Load subagent
//

BOOL LoadSubAgent(TCHAR *szModuleName)
{
   HMODULE hModule;
   BOOL bSuccess = FALSE;
   TCHAR szErrorText[256];

   hModule = DLOpen(szModuleName, szErrorText);
   if (hModule != NULL)
   {
      BOOL (* SubAgentRegister)(NETXMS_SUBAGENT_INFO **, Config *);

      // Under NetWare, we have slightly different subagent
      // initialization procedure. Because normally two NLMs
      // cannot export symbols with identical names, we cannot 
      // simply export NxSubAgentRegister in each subagent. Instead,
      // agent expect to find symbol called NxSubAgentRegister_<module_file_name>
      // in every subagent. Note that module file name should
      // be in capital letters.
#ifdef _NETWARE
      char *pExt, szFileName[MAX_PATH], szEntryPoint[MAX_PATH];
      int iElem, iFlags;

      deconstruct(szModuleName, NULL, NULL, NULL, szFileName, NULL, &iElem, &iFlags);
      pExt = strrchr(szFileName, '.');
      if (pExt != NULL)
         *pExt = 0;
      strupr(szFileName);
      sprintf(szEntryPoint, "NxSubAgentRegister_%s", szFileName);
      SubAgentRegister = (BOOL (*)(NETXMS_SUBAGENT_INFO **, Config *))DLGetSymbolAddr(hModule, szEntryPoint, szErrorText);
#else
      SubAgentRegister = (BOOL (*)(NETXMS_SUBAGENT_INFO **, Config *))DLGetSymbolAddr(hModule, _T("NxSubAgentRegister"), szErrorText);
#endif

      if (SubAgentRegister != NULL)
      {
#ifdef _NETWARE
         bSuccess = InitSubAgent(hModule, szModuleName, SubAgentRegister, szEntryPoint);
         if (bSuccess)
            setdontunloadflag(hModule);
#else
         bSuccess = InitSubAgent(hModule, szModuleName, SubAgentRegister, NULL);
#endif
      }
      else
      {
         nxlog_write(MSG_NO_SUBAGENT_ENTRY_POINT, EVENTLOG_ERROR_TYPE, "s", szModuleName);
         DLClose(hModule);
      }
   }
   else
   {
      nxlog_write(MSG_SUBAGENT_LOAD_FAILED, EVENTLOG_ERROR_TYPE, "ss", szModuleName, szErrorText);
   }

   return bSuccess;
}


//
// Unload all subagents.
// This function should be called on shutdown, so we don't care
// about deregistering parameters and so on.
//

void UnloadAllSubAgents()
{
   DWORD i;

   for(i = 0; i < m_dwNumSubAgents; i++)
   {
#ifdef _NETWARE
      UnImportPublicObject(m_pSubAgentList[i].hModule, m_pSubAgentList[i].szEntryPoint);
      cleardontunloadflag(m_pSubAgentList[i].hModule);
#endif
      if (m_pSubAgentList[i].pInfo->shutdown != NULL)
         m_pSubAgentList[i].pInfo->shutdown();
#ifndef _NETWARE
      DLClose(m_pSubAgentList[i].hModule);
#endif
   }
}


//
// Enumerate loaded subagents
//

LONG H_SubAgentList(const TCHAR *cmd, const TCHAR *arg, StringList *value)
{
   DWORD i;
   TCHAR szBuffer[MAX_PATH + 32];

   for(i = 0; i < m_dwNumSubAgents; i++)
   {
#ifdef __64BIT__
      _sntprintf(szBuffer, MAX_PATH + 32, _T("%s %s 0x") UINT64X_FMT(_T("016")) _T(" %s"), 
                 m_pSubAgentList[i].pInfo->name, m_pSubAgentList[i].pInfo->version,
                 m_pSubAgentList[i].hModule, m_pSubAgentList[i].szName);
#else
      _sntprintf(szBuffer, MAX_PATH + 32, _T("%s %s 0x%08X %s"), 
                 m_pSubAgentList[i].pInfo->name, m_pSubAgentList[i].pInfo->version,
                 m_pSubAgentList[i].hModule, m_pSubAgentList[i].szName);
#endif
      value->add(szBuffer);
   }
   return SYSINFO_RC_SUCCESS;
}


//
// Process unknown command by subagents
//

BOOL ProcessCmdBySubAgent(DWORD dwCommand, CSCPMessage *pRequest, CSCPMessage *pResponse, void *session)
{
   BOOL bResult = FALSE;
   DWORD i;

   for(i = 0; (i < m_dwNumSubAgents) && (!bResult); i++)
   {
      if (m_pSubAgentList[i].pInfo->commandHandler != NULL)
         bResult = m_pSubAgentList[i].pInfo->commandHandler(dwCommand, pRequest, pResponse, session);
   }
   return bResult;
}
