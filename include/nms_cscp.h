/* 
** NetXMS - Network Management System
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
** $module: nms_cscp.h
**
**/

#ifndef _nms_cscp_h_
#define _nms_cscp_h_


//
// Constants
//

#define SERVER_LISTEN_PORT       4701


//
// Data field structure
//

#pragma pack(1)

typedef struct
{
   DWORD dwVarId;       // Variable identifier
   BYTE  bType;         // Data type
   BYTE  bReserved;     // Padding
   union
   {
      struct
      {
         WORD wLen;
         char szValue[1];
      } string;
      struct
      {
         WORD wReserved1;
         DWORD dwInteger;
      };
      struct
      {
         WORD wReserverd2;
         __int64 qwInt64;
      };
      WORD wInt16;
   } data;
} CSCP_DF;


//
// Message structure
//

typedef struct
{
   WORD wCode;    // Message (command) code (lower 12 bits) and flags (higher 4 bits)
   WORD wSize;    // Message size (including header) in bytes
   DWORD dwId;    // Unique message identifier
   CSCP_DF df[1]; // Data fields
} CSCP_MESSAGE;

#pragma pack()


//
// Data types
//

#define DT_INTEGER   0
#define DT_STRING    1
#define DT_INT64     2
#define DT_INT16     3


//
// Message flags
//

#define MF_BINARY                   0x1000


//
// Message (command) codes
//

#define CMD_LOGIN                   0x0001
#define CMD_LOGIN_RESP              0x0002
#define CMD_KEEPALIVE               0x0003
#define CMD_EVENT                   0x0004
#define CMD_GET_OBJECTS             0x0005
#define CMD_OBJECT                  0x0006
#define CMD_DELETE_OBJECT           0x0007
#define CMD_UPDATE_OBJECT           0x0008
#define CMD_OBJECT_LIST_END         0x0009
#define CMD_OBJECT_UPDATE           0x000A
#define CMD_GET_EVENTS              0x000B
#define CMD_EVENT_LIST_END          0x000C
#define CMD_GET_CONFIG_VARLIST      0x000D
#define CMD_SET_CONFIG_VARIABLE     0x000E
#define CMD_CONFIG_VARIABLE         0x000F
#define CMD_CONFIG_VARLIST_END      0x0010
#define CMD_DELETE_CONFIG_VARIABLE  0x0011


//
// Variable identifiers
//

#define VID_LOGIN_NAME              1
#define VID_PASSWORD                2
#define VID_OBJECT_ID               3
#define VID_OBJECT_NAME             4
#define VID_OBJECT_CLASS            5
#define VID_LOGIN_RESULT            6
#define VID_PARENT_CNT              7
#define VID_IP_ADDRESS              8
#define VID_IP_NETMASK              9
#define VID_OBJECT_STATUS           10
#define VID_IF_INDEX                11
#define VID_IF_TYPE                 12
#define VID_FLAGS                   13
#define VID_DISCOVERY_FLAGS         14
#define VID_AGENT_PORT              15
#define VID_AUTH_METHOD             16
#define VID_SHARED_SECRET           17
#define VID_COMMUNITY_STRING        18
#define VID_SNMP_OID                19
#define VID_NAME                    20
#define VID_VALUE                   21
#define VID_ERROR                   22


// Object information can contain variable number of parent objects' ids.
// Because each variable in message have to have unique identifier,
// we reserver a 4000 ids for this variables.
#define VID_PARENT_ID_BASE          4000
#define VID_PARENT_ID_LAST          7999


//
// Inline functions
//

inline BOOL IsBinaryMsg(CSCP_MESSAGE *pMsg)
{
   return ntohs(pMsg->wCode) & MF_BINARY;
}


#endif   /* _nms_cscp_h_ */
