/* 
** Project X - Network Management System
** Copyright (C) 2003 Victor Kirhenshtein
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
** $module: discovery.cpp
**
**/

#include "nms_core.h"

#ifdef _WIN32

#include <iphlpapi.h>
#include <iprtrmib.h>
#include <rtinfo.h>

#else

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_NET_IF_H
#include <net/if.h>
#endif
//#if HAVE_NET_IF_ARP_H
#include <net/if_arp.h>
//#endif

#endif   /* _WIN32 */


//
// Get local ARP cache
//

ARP_CACHE *GetLocalArpCache(void)
{
   ARP_CACHE *pArpCache = NULL;

#ifdef _WIN32
   MIB_IPNETTABLE *sysArpCache;
   DWORD i, dwError, dwSize;

   sysArpCache = (MIB_IPNETTABLE *)malloc(SIZEOF_IPNETTABLE(4096));
   if (sysArpCache == NULL)
      return NULL;

   dwSize = SIZEOF_IPNETTABLE(4096);
   dwError = GetIpNetTable(sysArpCache, &dwSize, FALSE);
   if (dwError != NO_ERROR)
   {
      WriteLog(MSG_GETIPNETTABLE_FAILED, EVENTLOG_ERROR_TYPE, "e", NULL);
      free(sysArpCache);
      return NULL;
   }

   pArpCache = (ARP_CACHE *)malloc(sizeof(ARP_CACHE));
   pArpCache->dwNumEntries = 0;
   pArpCache->pEntries = (ARP_ENTRY *)malloc(sizeof(ARP_ENTRY) * sysArpCache->dwNumEntries);

   for(i = 0; i < sysArpCache->dwNumEntries; i++)
      if ((sysArpCache->table[i].dwType == 3) ||
          (sysArpCache->table[i].dwType == 4))  // Only static and dynamic entries
      {
         pArpCache->pEntries[pArpCache->dwNumEntries].dwIndex = sysArpCache->table[i].dwIndex;
         pArpCache->pEntries[pArpCache->dwNumEntries].dwIpAddr = sysArpCache->table[i].dwAddr;
         memcpy(pArpCache->pEntries[pArpCache->dwNumEntries].bMacAddr, sysArpCache->table[i].bPhysAddr, 6);
         pArpCache->dwNumEntries++;
      }

   free(sysArpCache);
#else
   /* TODO: Add UNIX code here */
   printf("CODE NOT IMPLEMENTED\n");
#endif

   return pArpCache;
}


//
// Get local interface list
//

INTERFACE_LIST *GetLocalInterfaceList(void)
{
   INTERFACE_LIST *pIfList;

#ifdef _WIN32
   MIB_IPADDRTABLE *ifTable;
   MIB_IFROW ifRow;
   DWORD i, dwSize, dwError, dwIfType;
   char szIfName[32], *pszIfName;

   ifTable = (MIB_IPADDRTABLE *)malloc(SIZEOF_IPADDRTABLE(256));
   if (ifTable == NULL)
      return NULL;

   dwSize = SIZEOF_IPADDRTABLE(256);
   dwError = GetIpAddrTable(ifTable, &dwSize, FALSE);
   if (dwError != NO_ERROR)
   {
      WriteLog(MSG_GETIPADDRTABLE_FAILED, EVENTLOG_ERROR_TYPE, "e", NULL);
      free(ifTable);
      return NULL;
   }

   // Allocate and initialize interface list structure
   pIfList = (INTERFACE_LIST *)malloc(sizeof(INTERFACE_LIST));
   pIfList->iNumEntries = ifTable->dwNumEntries;
   pIfList->pInterfaces = (INTERFACE_INFO *)malloc(sizeof(INTERFACE_INFO) * ifTable->dwNumEntries);
   memset(pIfList->pInterfaces, 0, sizeof(INTERFACE_INFO) * ifTable->dwNumEntries);

   // Fill in our interface list
   for(i = 0; i < ifTable->dwNumEntries; i++)
   {
      ifRow.dwIndex = ifTable->table[i].dwIndex;
      if (GetIfEntry(&ifRow) == NO_ERROR)
      {
         dwIfType = ifRow.dwType;
         pszIfName = (char *)ifRow.bDescr;
      }
      else
      {
         dwIfType = IFTYPE_OTHER;
         sprintf(szIfName, "IF-UNKNOWN-%d", ifTable->table[i].dwIndex);
         pszIfName = szIfName;
      }
      pIfList->pInterfaces[i].dwIndex = ifTable->table[i].dwIndex;
      pIfList->pInterfaces[i].dwIpAddr = ifTable->table[i].dwAddr;
      pIfList->pInterfaces[i].dwIpNetMask = ifTable->table[i].dwMask;
      pIfList->pInterfaces[i].dwType = dwIfType;
      strncpy(pIfList->pInterfaces[i].szName, pszIfName, MAX_OBJECT_NAME - 1);
   }

   free(ifTable);

#else

   struct ifconf ifc;
   struct ifreq ifrq;
   int i, iSock, iCount;
   BOOL bIoFailed = FALSE;

   iSock = socket(AF_INET, SOCK_DGRAM, 0);
   if (iSock != -1)
   {
      ifc.ifc_buf = NULL;
      iCount = 10;
      do
      {
         iCount += 10;
         ifc.ifc_len = sizeof(struct ifreq) * iCount;
         ifc.ifc_buf = (char *)realloc(ifc.ifc_buf, ifc.ifc_len);
         if (ioctl(iSock, SIOCGIFCONF, &ifc) < 0)
         {
            WriteLog(MSG_IOCTL_FAILED, EVENTLOG_ERROR_TYPE, "se", "SIOCGIFCONF", errno);
            bIoFailed = TRUE;
            break;
         }
      } while(ifc.ifc_len == sizeof(struct ifreq) * iCount);

      if (!bIoFailed)
      {
         iCount = ifc.ifc_len / sizeof(struct ifreq);

         // Allocate and initialize interface list structure
         pIfList = (INTERFACE_LIST *)malloc(sizeof(INTERFACE_LIST));
         pIfList->iNumEntries = iCount;
         pIfList->pInterfaces = (INTERFACE_INFO *)malloc(sizeof(INTERFACE_INFO) * iCount);
         memset(pIfList->pInterfaces, 0, sizeof(INTERFACE_INFO) * iCount);

         for(i = 0; i < iCount; i++)
         {
            strcpy(ifrq.ifr_name, ifc.ifc_req[i].ifr_name);

            // Interface name
            strcpy(pIfList->pInterfaces[i].szName, ifc.ifc_req[i].ifr_name);

            // IP address
            if (ioctl(iSock, SIOCGIFADDR, &ifrq) == 0)
            {
               memcpy(&pIfList->pInterfaces[i].dwIpAddr,
                      &(((struct sockaddr_in *)&ifrq.ifr_addr)->sin_addr.s_addr),
                      sizeof(DWORD));
            }

            // IP netmask
            if (ioctl(iSock, SIOCGIFNETMASK, &ifrq) == 0)
            {
               memcpy(&pIfList->pInterfaces[i].dwIpNetMask,
                      &(((struct sockaddr_in *)&ifrq.ifr_addr)->sin_addr.s_addr),
                      sizeof(DWORD));
            }

            // Interface type
            if (ioctl(iSock, SIOCGIFHWADDR, &ifrq) == 0)
            {
               switch(ifrq.ifr_hwaddr.sa_family)
               {
                  case ARPHRD_ETHER:
                     pIfList->pInterfaces[i].dwType = IFTYPE_ETHERNET_CSMACD;
                     break;
                  case ARPHRD_SLIP:
                  case ARPHRD_CSLIP:
                  case ARPHRD_SLIP6:
                  case ARPHRD_CSLIP6:
                     pIfList->pInterfaces[i].dwType = IFTYPE_SLIP;
                     break;
                  case ARPHRD_PPP:
                     pIfList->pInterfaces[i].dwType = IFTYPE_PPP;
                     break;
                  case ARPHRD_LOOPBACK:
                     pIfList->pInterfaces[i].dwType = IFTYPE_SOFTWARE_LOOPBACK;
                     break;
                  case ARPHRD_FDDI:
                     pIfList->pInterfaces[i].dwType = IFTYPE_FDDI;
                     break;
                  default:
                     pIfList->pInterfaces[i].dwType = IFTYPE_OTHER;
                     break;
               }
            }

            // Interface index
            pIfList->pInterfaces[i].dwIndex = i + 1;
         }
      }

      close(iSock);
      free(ifc.ifc_buf);
   }
   
#endif

   CleanInterfaceList(pIfList);

   return pIfList;
}


//
// Check if management server node presented in node list
//

void CheckForMgmtNode(void)
{
   INTERFACE_LIST *pIfList;
   Node *pNode;
   int i;

   pIfList = GetLocalInterfaceList();
   if (pIfList != NULL)
   {
      for(i = 0; i < pIfList->iNumEntries; i++)
         if ((pNode = FindNodeByIP(pIfList->pInterfaces[i].dwIpAddr)) != NULL)
         {
            g_dwMgmtNode = pNode->Id();   // Set local management node ID
            break;
         }
      if (i == pIfList->iNumEntries)   // No such node
      {
         // Find interface with IP address
         for(i = 0; i < pIfList->iNumEntries; i++)
            if (pIfList->pInterfaces[i].dwIpAddr != 0)
            {
               pNode = new Node(pIfList->pInterfaces[i].dwIpAddr, NF_IS_LOCAL_MGMT, DF_DEFAULT);
               NetObjInsert(pNode, TRUE);
               if (!pNode->NewNodePoll(0))
               {
                  ObjectsGlobalLock();
                  NetObjDelete(pNode);
                  ObjectsGlobalUnlock();
                  delete pNode;     // Node poll failed, delete it
               }
               else
               {
                  g_dwMgmtNode = pNode->Id();   // Set local management node ID
                  /* DEBUG */
                  pNode->AddItem(new DCItem(CreateUniqueId(IDG_ITEM), "Status", DS_INTERNAL, DTYPE_INTEGER, 60, 30, pNode));
               }
               break;
            }
      }
      DestroyInterfaceList(pIfList);
   }
}


//
// Network discovery thread
//

void DiscoveryThread(void *arg)
{
   DWORD dwNewNodeId = 1, dwWatchdogId;
   Node *pNode;

   dwWatchdogId = WatchdogAddThread("Network Discovery Thread");

   // Flush new nodes table
   DBQuery(g_hCoreDB, "DELETE FROM new_nodes");

   while(!ShutdownInProgress())
   {
      if (SleepAndCheckForShutdown(30))
         break;      // Shutdown has arrived
      WatchdogNotify(dwWatchdogId);

      CheckForMgmtNode();

      // Walk through nodes and poll for ARP tables
      MutexLock(g_hMutexNodeIndex, INFINITE);
      for(DWORD i = 0; i < g_dwNodeAddrIndexSize; i++)
      {
         pNode = (Node *)g_pNodeIndexByAddr[i].pObject;
         if (pNode->ReadyForDiscoveryPoll())
         {
            ARP_CACHE *pArpCache;

            // Retrieve and analize node's ARP cache
            pArpCache = pNode->GetArpCache();
            if (pArpCache != NULL)
            {
               for(DWORD j = 0; j < pArpCache->dwNumEntries; j++)
               {
                  if (FindNodeByIP(pArpCache->pEntries[j].dwIpAddr) == NULL)
                  {
                     Interface *pInterface = pNode->FindInterface(pArpCache->pEntries[j].dwIndex,
                                                                  pArpCache->pEntries[j].dwIpAddr);
                     if (pInterface != NULL)
                        if (!IsBroadcastAddress(pArpCache->pEntries[j].dwIpAddr,
                                                pInterface->IpNetMask()))
                        {
                           char szQuery[256];

                           sprintf(szQuery, "INSERT INTO new_nodes (id,ip_addr,ip_netmask,discovery_flags) VALUES (%d,%d,%d,%d)",
                                   dwNewNodeId++, pArpCache->pEntries[j].dwIpAddr,
                                   pInterface->IpNetMask(), DF_DEFAULT);
                           DBQuery(g_hCoreDB, szQuery);
                        }
                  }
               }
               DestroyArpCache(pArpCache);
            }

            pNode->SetDiscoveryPollTimeStamp();
         }
      }
      MutexUnlock(g_hMutexNodeIndex);
   }
}
