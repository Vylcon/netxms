/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2013 Victor Kirhenshtein
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
** File: template.cpp
**
**/

#include "nxcore.h"

/**
 * Redefined status calculation for template group
 */
void TemplateGroup::calculateCompoundStatus(BOOL bForcedRecalc)
{
   m_iStatus = STATUS_NORMAL;
}

/**
 * Called by client session handler to check if threshold summary should be shown for this object.
 */
bool TemplateGroup::showThresholdSummary()
{
	return false;
}

/**
 * Template object constructor
 */
Template::Template() : NetObj()
{
	m_dcObjects = new ObjectArray<DCObject>(8, 16, true);
   m_dwDCILockStatus = INVALID_INDEX;
	m_flags = 0;
   m_dwVersion = 0x00010000;  // Initial version is 1.0
	m_applyFilter = NULL;
	m_applyFilterSource = NULL;
   m_iStatus = STATUS_NORMAL;
	m_mutexDciAccess = MutexCreateRecursive();
}

/**
 * Constructor for new template object
 */
Template::Template(const TCHAR *pszName) : NetObj()
{
   nx_strncpy(m_szName, pszName, MAX_OBJECT_NAME);
	m_dcObjects = new ObjectArray<DCObject>(8, 16, true);
   m_dwDCILockStatus = INVALID_INDEX;
	m_flags = 0;
   m_dwVersion = 0x00010000;  // Initial version is 1.0
	m_applyFilter = NULL;
	m_applyFilterSource = NULL;
   m_iStatus = STATUS_NORMAL;
   m_bIsHidden = TRUE;
	m_mutexDciAccess = MutexCreateRecursive();
}

/**
 * Create template object from import file
 */
Template::Template(ConfigEntry *config) : NetObj()
{
   m_bIsHidden = TRUE;
   m_dwDCILockStatus = INVALID_INDEX;
   m_iStatus = STATUS_NORMAL;
	m_mutexDciAccess = MutexCreateRecursive();

	// Name and version
	nx_strncpy(m_szName, config->getSubEntryValue(_T("name"), 0, _T("Unnamed Template")), MAX_OBJECT_NAME);
	m_dwVersion = config->getSubEntryValueUInt(_T("version"), 0, 0x00010000);
	m_flags = config->getSubEntryValueUInt(_T("flags"), 0, 0);

	// Auto-apply filter
	m_applyFilter = NULL;
	m_applyFilterSource = NULL;
	if (m_flags & TF_AUTO_APPLY)
		setAutoApplyFilter(config->getSubEntryValue(_T("filter")));

	// Data collection
	m_dcObjects = new ObjectArray<DCObject>(8, 16, true);
	ConfigEntry *dcRoot = config->findEntry(_T("dataCollection"));
	if (dcRoot != NULL)
	{
		ConfigEntryList *dcis = dcRoot->getSubEntries(_T("dci#*"));
		for(int i = 0; i < dcis->getSize(); i++)
		{
			m_dcObjects->add(new DCItem(dcis->getEntry(i), this));
		}
		delete dcis;
	}
}

/**
 * Destructor
 */
Template::~Template()
{
	delete m_dcObjects;
	delete m_applyFilter;
	safe_free(m_applyFilterSource);
	MutexDestroy(m_mutexDciAccess);
}

/**
 * Destroy all related data collection items.
 */
void Template::destroyItems()
{
	m_dcObjects->clear();
}

/**
 * Set auto apply filter.
 *
 * @param filter new filter script code or NULL to clear filter
 */
void Template::setAutoApplyFilter(const TCHAR *filter)
{
	LockData();
	safe_free(m_applyFilterSource);
	delete m_applyFilter;
	if (filter != NULL)
	{
		TCHAR error[256];

		m_applyFilterSource = _tcsdup(filter);
		m_applyFilter = NXSLCompile(m_applyFilterSource, error, 256);
		if (m_applyFilter == NULL)
			nxlog_write(MSG_TEMPLATE_SCRIPT_COMPILATION_ERROR, EVENTLOG_WARNING_TYPE, "dss", m_dwId, m_szName, error);
	}
	else
	{
		m_applyFilterSource = NULL;
		m_applyFilter = NULL;
	}
	Modify();
	UnlockData();
}

/**
 * Create template object from database data
 *
 * @param dwId object ID
 */
BOOL Template::CreateFromDB(DWORD dwId)
{
   TCHAR szQuery[256];
   DB_RESULT hResult;
   DWORD i, dwNumNodes, dwNodeId;
   NetObj *pObject;
   BOOL bResult = TRUE;

   m_dwId = dwId;

   if (!loadCommonProperties())
      return FALSE;

   _sntprintf(szQuery, sizeof(szQuery) / sizeof(TCHAR), _T("SELECT version,flags,apply_filter FROM templates WHERE id=%d"), dwId);
   hResult = DBSelect(g_hCoreDB, szQuery);
   if (hResult == NULL)
      return FALSE;     // Query failed

   if (DBGetNumRows(hResult) == 0)
   {
      // No object with given ID in database
      DBFreeResult(hResult);
      return FALSE;
   }

   m_dwVersion = DBGetFieldULong(hResult, 0, 0);
	m_flags = DBGetFieldULong(hResult, 0, 1);
	if (m_flags & TF_AUTO_APPLY)
	{
		m_applyFilterSource = DBGetField(hResult, 0, 2, NULL, 0);
		if (m_applyFilterSource != NULL)
		{
			TCHAR error[256];

			m_applyFilter = NXSLCompile(m_applyFilterSource, error, 256);
			if (m_applyFilter == NULL)
				nxlog_write(MSG_TEMPLATE_SCRIPT_COMPILATION_ERROR, EVENTLOG_WARNING_TYPE, "dss", m_dwId, m_szName, error);
		}
	}
   DBFreeResult(hResult);

   // Load DCI and access list
   loadACLFromDB();
   loadItemsFromDB();
   for(i = 0; i < (DWORD)m_dcObjects->size(); i++)
      if (!m_dcObjects->get(i)->loadThresholdsFromDB())
         bResult = FALSE;

   // Load related nodes list
   if (!m_bIsDeleted)
   {
      _sntprintf(szQuery, sizeof(szQuery) / sizeof(TCHAR), _T("SELECT node_id FROM dct_node_map WHERE template_id=%d"), m_dwId);
      hResult = DBSelect(g_hCoreDB, szQuery);
      if (hResult != NULL)
      {
         dwNumNodes = DBGetNumRows(hResult);
         for(i = 0; i < dwNumNodes; i++)
         {
            dwNodeId = DBGetFieldULong(hResult, i, 0);
            pObject = FindObjectById(dwNodeId);
            if (pObject != NULL)
            {
               if (pObject->Type() == OBJECT_NODE)
               {
                  AddChild(pObject);
                  pObject->AddParent(this);
               }
               else
               {
                  nxlog_write(MSG_DCT_MAP_NOT_NODE, EVENTLOG_ERROR_TYPE, "dd", m_dwId, dwNodeId);
               }
            }
            else
            {
               nxlog_write(MSG_INVALID_DCT_MAP, EVENTLOG_ERROR_TYPE, "dd", m_dwId, dwNodeId);
            }
         }
         DBFreeResult(hResult);
      }
   }

	m_iStatus = STATUS_NORMAL;

   return bResult;
}

/**
 * Save object to database
 */
BOOL Template::SaveToDB(DB_HANDLE hdb)
{
   LockData();

   if (!saveCommonProperties(hdb))
	{
		UnlockData();
		return FALSE;
	}

	DB_STATEMENT hStmt;
   if (IsDatabaseRecordExist(hdb, _T("templates"), _T("id"), m_dwId))
	{
		hStmt = DBPrepare(hdb, _T("UPDATE templates SET version=?,flags=?,apply_filter=? WHERE id=?"));
	}
   else
	{
		hStmt = DBPrepare(hdb, _T("INSERT INTO templates (version,flags,apply_filter,id) VALUES (?,?,?,?)"));
	}
	if (hStmt == NULL)
	{
		UnlockData();
		return FALSE;
	}

	DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_dwVersion);
	DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, m_flags);
	DBBind(hStmt, 3, DB_SQLTYPE_TEXT, m_applyFilterSource, DB_BIND_STATIC);
	DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, m_dwId);
	BOOL success = DBExecute(hStmt);
	DBFreeStatement(hStmt);

	if (success)
	{
		TCHAR query[256];

		// Update members list
		_sntprintf(query, 256, _T("DELETE FROM dct_node_map WHERE template_id=%d"), m_dwId);
		DBQuery(hdb, query);
		LockChildList(FALSE);
		for(DWORD i = 0; i < m_dwChildCount; i++)
		{
			_sntprintf(query, 256, _T("INSERT INTO dct_node_map (template_id,node_id) VALUES (%d,%d)"), m_dwId, m_pChildList[i]->Id());
			DBQuery(hdb, query);
		}
		UnlockChildList();

		// Save access list
		saveACLToDB(hdb);
	}

   UnlockData();

   // Save data collection items
	lockDciAccess();
   for(int i = 0; i < m_dcObjects->size(); i++)
      m_dcObjects->get(i)->saveToDB(hdb);
	unlockDciAccess();

   // Clear modifications flag
	LockData();
   m_bIsModified = FALSE;
	UnlockData();

   return success;
}

/**
 * Delete object from database
 */
BOOL Template::DeleteFromDB()
{
   TCHAR szQuery[256];
   BOOL bSuccess;

   bSuccess = NetObj::DeleteFromDB();
   if (bSuccess)
   {
      if (Type() == OBJECT_TEMPLATE)
      {
         _sntprintf(szQuery, sizeof(szQuery) / sizeof(TCHAR), _T("DELETE FROM templates WHERE id=%d"), m_dwId);
         QueueSQLRequest(szQuery);
         _sntprintf(szQuery, sizeof(szQuery) / sizeof(TCHAR), _T("DELETE FROM dct_node_map WHERE template_id=%d"), m_dwId);
         QueueSQLRequest(szQuery);
      }
      else
      {
         _sntprintf(szQuery, sizeof(szQuery) / sizeof(TCHAR), _T("DELETE FROM dct_node_map WHERE node_id=%d"), m_dwId);
         QueueSQLRequest(szQuery);
      }
      _sntprintf(szQuery, sizeof(szQuery) / sizeof(TCHAR), _T("DELETE FROM items WHERE node_id=%d"), m_dwId);
      QueueSQLRequest(szQuery);
      _sntprintf(szQuery, sizeof(szQuery) / sizeof(TCHAR), _T("UPDATE items SET template_id=0 WHERE template_id=%d"), m_dwId);
      QueueSQLRequest(szQuery);
   }
   return bSuccess;
}

/**
 * Load data collection items from database
 */
void Template::loadItemsFromDB()
{
	DB_STATEMENT hStmt = DBPrepare(g_hCoreDB, 
	           _T("SELECT item_id,name,source,datatype,polling_interval,retention_time,")
              _T("status,delta_calculation,transformation,template_id,description,")
              _T("instance,template_item_id,flags,resource_id,")
              _T("proxy_node,base_units,unit_multiplier,custom_units_name,")
	           _T("perftab_settings,system_tag,snmp_port,snmp_raw_value_type,")
				  _T("instd_method,instd_data,instd_filter,samples FROM items WHERE node_id=?"));
	if (hStmt != NULL)
	{
		DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_dwId);
		DB_RESULT hResult = DBSelectPrepared(hStmt);
		if (hResult != NULL)
		{
			int count = DBGetNumRows(hResult);
			for(int i = 0; i < count; i++)
				m_dcObjects->add(new DCItem(hResult, i, this));
			DBFreeResult(hResult);
		}
		DBFreeStatement(hStmt);
	}

	hStmt = DBPrepare(g_hCoreDB, 
	           _T("SELECT item_id,template_id,template_item_id,name,instance_column,")
				  _T("description,flags,source,snmp_port,polling_interval,retention_time,")
              _T("status,system_tag,resource_id,proxy_node,perftab_settings FROM dc_tables WHERE node_id=?"));
	if (hStmt != NULL)
	{
		DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_dwId);
		DB_RESULT hResult = DBSelectPrepared(hStmt);
		if (hResult != NULL)
		{
			int count = DBGetNumRows(hResult);
			for(int i = 0; i < count; i++)
				m_dcObjects->add(new DCTable(hResult, i, this));
			DBFreeResult(hResult);
		}
		DBFreeStatement(hStmt);
	}
}

/**
 * Add data collection object to node
 */
bool Template::addDCObject(DCObject *object, bool alreadyLocked)
{
   int i;
   bool success = false;

   if (!alreadyLocked)
      lockDciAccess(); // write lock

   // Check if that object exists
   for(i = 0; i < m_dcObjects->size(); i++)
      if (m_dcObjects->get(i)->getId() == object->getId())
         break;   // Object with specified id already exist
   
   if (i == m_dcObjects->size())     // Add new item
   {
		m_dcObjects->add(object);
      object->setLastPollTime(0);    // Cause item to be polled immediatelly
      if (object->getStatus() != ITEM_STATUS_DISABLED)
         object->setStatus(ITEM_STATUS_ACTIVE, false);
      object->setBusyFlag(FALSE);
      success = true;
   }

   if (!alreadyLocked)
      unlockDciAccess();

	if (success)
	{
		LockData();
      Modify();
		UnlockData();
	}
   return success;
}

/**
 * Delete data collection object from node
 */
bool Template::deleteDCObject(DWORD dcObjectId, bool needLock)
{
   bool success = false;

	if (needLock)
		lockDciAccess();  // write lock

   // Check if that item exists
   for(int i = 0; i < m_dcObjects->size(); i++)
	{
		DCObject *object = m_dcObjects->get(i);
      if (object->getId() == dcObjectId)
      {
         // Destroy item
			DbgPrintf(7, _T("Template::DeleteDCObject: deleting DCObject %d from object %d"), (int)dcObjectId, (int)m_dwId);
         if (object->prepareForDeletion())
			{
				// Physically delete DCI only if it is not busy
				// Busy DCIs will be deleted by data collector
				object->deleteFromDB();
				m_dcObjects->remove(i);
			}
			else
			{
				m_dcObjects->unlink(i);
				DbgPrintf(7, _T("Template::DeleteItem: destruction of DCO %d delayed"), (int)dcObjectId);
			}
         success = true;
			DbgPrintf(7, _T("Template::DeleteDCObject: DCO deleted from object %d"), (int)m_dwId);
         break;
      }
	}

	if (needLock)
	   unlockDciAccess();
   return success;
}

/**
 * Modify data collection object from NXCP message
 */
bool Template::updateDCObject(DWORD dwItemId, CSCPMessage *pMsg, DWORD *pdwNumMaps, DWORD **ppdwMapIndex, DWORD **ppdwMapId)
{
   bool success = false;

   lockDciAccess();

   // Check if that item exists
   for(int i = 0; i < m_dcObjects->size(); i++)
	{
		DCObject *object = m_dcObjects->get(i);
      if (object->getId() == dwItemId)
      {
			if (object->getType() == DCO_TYPE_ITEM)
			{
				((DCItem *)object)->updateFromMessage(pMsg, pdwNumMaps, ppdwMapIndex, ppdwMapId);
			}
			else
			{
				object->updateFromMessage(pMsg);
			}
			success = true;
			m_bIsModified = TRUE;
         break;
      }
	}

   unlockDciAccess();
   return success;
}

/**
 * Set status for group of DCIs
 */
bool Template::setItemStatus(DWORD dwNumItems, DWORD *pdwItemList, int iStatus)
{
   bool success = true;

   lockDciAccess();
   for(DWORD i = 0; i < dwNumItems; i++)
   {
		int j;
      for(j = 0; j < m_dcObjects->size(); j++)
      {
         if (m_dcObjects->get(j)->getId() == pdwItemList[i])
         {
            m_dcObjects->get(j)->setStatus(iStatus, true);
            break;
         }
      }
      if (j == m_dcObjects->size())
         success = false;     // Invalid DCI ID provided
   }
   unlockDciAccess();
   return success;
}

/**
 * Lock data collection items list
 */
BOOL Template::lockDCIList(DWORD dwSessionId, const TCHAR *pszNewOwner, TCHAR *pszCurrOwner)
{
   BOOL bSuccess;

   LockData();
   if (m_dwDCILockStatus == INVALID_INDEX)
   {
      m_dwDCILockStatus = dwSessionId;
      m_bDCIListModified = FALSE;
      nx_strncpy(m_szCurrDCIOwner, pszNewOwner, MAX_SESSION_NAME);
      bSuccess = TRUE;
   }
   else
   {
      if (pszCurrOwner != NULL)
         _tcscpy(pszCurrOwner, m_szCurrDCIOwner);
      bSuccess = FALSE;
   }
   UnlockData();
   return bSuccess;
}

/**
 * Unlock data collection items list
 */
BOOL Template::unlockDCIList(DWORD dwSessionId)
{
   BOOL bSuccess = FALSE;

   LockData();
   if (m_dwDCILockStatus == dwSessionId)
   {
      m_dwDCILockStatus = INVALID_INDEX;
      if (m_bDCIListModified && (Type() == OBJECT_TEMPLATE))
      {
         m_dwVersion++;
         Modify();
      }
      m_bDCIListModified = FALSE;
      bSuccess = TRUE;
   }
   UnlockData();
   return bSuccess;
}

/**
 * Send DCI list to client
 */
void Template::sendItemsToClient(ClientSession *pSession, DWORD dwRqId)
{
   CSCPMessage msg;

   // Prepare message
   msg.SetId(dwRqId);
   msg.SetCode(CMD_NODE_DCI);

   lockDciAccess();

   // Walk through items list
   for(int i = 0; i < m_dcObjects->size(); i++)
   {
		if ((_tcsnicmp(m_dcObjects->get(i)->getDescription(), _T("@system."), 8)) || (Type() == OBJECT_TEMPLATE))
		{
			m_dcObjects->get(i)->createMessage(&msg);
			pSession->sendMessage(&msg);
			msg.DeleteAllVariables();
		}
   }

   unlockDciAccess();

   // Send end-of-list indicator
	msg.SetEndOfSequence();
   pSession->sendMessage(&msg);
}

/**
 * Get DCI's data type
 */
int Template::getItemType(DWORD dwItemId)
{
   int iType = -1;

   lockDciAccess();
   // Check if that item exists
   for(int i = 0; i < m_dcObjects->size(); i++)
	{
		DCObject *object = m_dcObjects->get(i);
      if (object->getId() == dwItemId)
      {
			if (object->getType() == DCO_TYPE_ITEM)
			{
				iType = ((DCItem *)object)->getDataType();
			}
         break;
      }
	}

   unlockDciAccess();
   return iType;
}

/**
 * Get item by it's id
 */
DCObject *Template::getDCObjectById(DWORD dwItemId)
{
   DCObject *object = NULL;

   lockDciAccess();
   // Check if that item exists
   for(int i = 0; i < m_dcObjects->size(); i++)
	{
		DCObject *curr = m_dcObjects->get(i);
      if (curr->getId() == dwItemId)
		{
			object = curr;
         break;
		}
	}

   unlockDciAccess();
   return object;
}

/**
 * Get item by it's name (case-insensetive)
 */
DCObject *Template::getDCObjectByName(const TCHAR *pszName)
{
   DCObject *object = NULL;

   lockDciAccess();
   // Check if that item exists
   for(int i = 0; i < m_dcObjects->size(); i++)
	{
		DCObject *curr = m_dcObjects->get(i);
      if (!_tcsicmp(curr->getName(), pszName))
		{
			object = curr;
         break;
		}
	}
   unlockDciAccess();
   return object;
}

/**
 * Get item by it's description (case-insensetive)
 */
DCObject *Template::getDCObjectByDescription(const TCHAR *pszDescription)
{
   DCObject *object = NULL;

   lockDciAccess();
   // Check if that item exists
   for(int i = 0; i < m_dcObjects->size(); i++)
	{
		DCObject *curr = m_dcObjects->get(i);
      if (!_tcsicmp(curr->getDescription(), pszDescription))
		{
			object = curr;
         break;
		}
	}
	unlockDciAccess();
   return object;
}

/**
 * Get item by it's index
 */
DCObject *Template::getDCObjectByIndex(int index)
{
   lockDciAccess();
	DCObject *object = m_dcObjects->get(index);
   unlockDciAccess();
   return object;
}

/**
 * Redefined status calculation for template
 */
void Template::calculateCompoundStatus(BOOL bForcedRecalc)
{
   m_iStatus = STATUS_NORMAL;
}

/**
 * Create NXCP message with object's data
 */
void Template::CreateMessage(CSCPMessage *pMsg)
{
   NetObj::CreateMessage(pMsg);
   pMsg->SetVariable(VID_TEMPLATE_VERSION, m_dwVersion);
	pMsg->SetVariable(VID_FLAGS, m_flags);
	pMsg->SetVariable(VID_AUTOBIND_FILTER, CHECK_NULL_EX(m_applyFilterSource));
}

/**
 * Modify object from NXCP message
 */
DWORD Template::ModifyFromMessage(CSCPMessage *pRequest, BOOL bAlreadyLocked)
{
   if (!bAlreadyLocked)
      LockData();

   // Change template version
   if (pRequest->IsVariableExist(VID_TEMPLATE_VERSION))
      m_dwVersion = pRequest->GetVariableLong(VID_TEMPLATE_VERSION);

   // Change flags
   if (pRequest->IsVariableExist(VID_FLAGS))
		m_flags = pRequest->GetVariableLong(VID_FLAGS);

   // Change apply filter
	if (pRequest->IsVariableExist(VID_AUTOBIND_FILTER))
	{
		safe_free(m_applyFilterSource);
		delete m_applyFilter;
		m_applyFilterSource = pRequest->GetVariableStr(VID_AUTOBIND_FILTER);
		if (m_applyFilterSource != NULL)
		{
			TCHAR error[256];

			m_applyFilter = NXSLCompile(m_applyFilterSource, error, 256);
			if (m_applyFilter == NULL)
				nxlog_write(MSG_TEMPLATE_SCRIPT_COMPILATION_ERROR, EVENTLOG_WARNING_TYPE, "dss", m_dwId, m_szName, error);
		}
		else
		{
			m_applyFilter = NULL;
		}
	}

   return NetObj::ModifyFromMessage(pRequest, TRUE);
}

/**
 * Apply template to data collection target
 */
BOOL Template::applyToTarget(DataCollectionTarget *target)
{
   DWORD *pdwItemList;
   BOOL bErrors = FALSE;

   // Link node to template
   if (!isChild(target->Id()))
   {
      AddChild(target);
      target->AddParent(this);
   }

   pdwItemList = (DWORD *)malloc(sizeof(DWORD) * m_dcObjects->size());
   DbgPrintf(2, _T("Apply %d items from template \"%s\" to target \"%s\""),
             m_dcObjects->size(), m_szName, target->Name());

   // Copy items
   for(int i = 0; i < m_dcObjects->size(); i++)
   {
		DCObject *object = m_dcObjects->get(i);
      pdwItemList[i] = object->getId();
      if (!target->applyTemplateItem(m_dwId, object))
      {
         bErrors = TRUE;
      }
   }

   // Clean items deleted from template
   target->cleanDeletedTemplateItems(m_dwId, m_dcObjects->size(), pdwItemList);

   // Cleanup
   free(pdwItemList);

   return bErrors;
}

/**
 * Queue template update
 */
void Template::queueUpdate()
{
   DWORD i;
   TEMPLATE_UPDATE_INFO *pInfo;

   LockData();
   for(i = 0; i < m_dwChildCount; i++)
      if ((m_pChildList[i]->Type() == OBJECT_NODE) || (m_pChildList[i]->Type() == OBJECT_MOBILEDEVICE))
      {
         incRefCount();
         pInfo = (TEMPLATE_UPDATE_INFO *)malloc(sizeof(TEMPLATE_UPDATE_INFO));
         pInfo->iUpdateType = APPLY_TEMPLATE;
         pInfo->pTemplate = this;
         pInfo->targetId = m_pChildList[i]->Id();
         g_pTemplateUpdateQueue->Put(pInfo);
      }
   UnlockData();
}

/**
 * Queue template remove from node
 */
void Template::queueRemoveFromTarget(DWORD targetId, BOOL bRemoveDCI)
{
   TEMPLATE_UPDATE_INFO *pInfo;

   LockData();
   incRefCount();
   pInfo = (TEMPLATE_UPDATE_INFO *)malloc(sizeof(TEMPLATE_UPDATE_INFO));
   pInfo->iUpdateType = REMOVE_TEMPLATE;
   pInfo->pTemplate = this;
   pInfo->targetId = targetId;
   pInfo->bRemoveDCI = bRemoveDCI;
   g_pTemplateUpdateQueue->Put(pInfo);
   UnlockData();
}

/**
 * Get list of events used by DCIs
 */
DWORD *Template::getDCIEventsList(DWORD *pdwCount)
{
   DWORD i, j, *pdwList;
   DCItem *pItem = NULL;

   pdwList = NULL;
   *pdwCount = 0;

   lockDciAccess();
   for(i = 0; i < (DWORD)m_dcObjects->size(); i++)
   {
      m_dcObjects->get(i)->getEventList(&pdwList, pdwCount);
   }
   unlockDciAccess();

   // Clean list from duplicates
   for(i = 0; i < *pdwCount; i++)
   {
      for(j = i + 1; j < *pdwCount; j++)
      {
         if (pdwList[i] == pdwList[j])
         {
            (*pdwCount)--;
            memmove(&pdwList[j], &pdwList[j + 1], sizeof(DWORD) * (*pdwCount - j));
            j--;
         }
      }
   }

   return pdwList;
}

/**
 * Create management pack record
 */
void Template::CreateNXMPRecord(String &str)
{
   str.addFormattedString(_T("\t\t<template id=\"%d\">\n\t\t\t<name>%s</name>\n\t\t\t<flags>%d</flags>\n\t\t\t<dataCollection>\n"),
	                       m_dwId, (const TCHAR *)EscapeStringForXML2(m_szName), m_flags);

   lockDciAccess();
   for(int i = 0; i < m_dcObjects->size(); i++)
      m_dcObjects->get(i)->createNXMPRecord(str);
   unlockDciAccess();

   str += _T("\t\t\t</dataCollection>\n");
	LockData();
	if (m_applyFilterSource != NULL)
	{
		str += _T("\t\t\t<filter>");
		str.addDynamicString(EscapeStringForXML(m_applyFilterSource, -1));
		str += _T("</filter>\n");
	}
	UnlockData();
	str += _T("\t\t</template>\n");
}

/**
 * Enumerate all DCIs
 */
bool Template::enumDCObjects(bool (* pfCallback)(DCObject *, DWORD, void *), void *pArg)
{
	bool success = true;

	lockDciAccess();
	for(int i = 0; i < m_dcObjects->size(); i++)
	{
		if (!pfCallback(m_dcObjects->get(i), i, pArg))
		{
			success = false;
			break;
		}
	}
	unlockDciAccess();
	return success;
}

/**
 * (Re)associate all DCIs
 */
void Template::associateItems()
{
	lockDciAccess();
	for(int i = 0; i < m_dcObjects->size(); i++)
		m_dcObjects->get(i)->changeBinding(0, this, FALSE);
	unlockDciAccess();
}

/**
 * Prepare template for deletion
 */
void Template::PrepareForDeletion()
{
	if (Type() == OBJECT_TEMPLATE)
	{
		DWORD i;
	
		LockChildList(FALSE);
		for(i = 0; i < m_dwChildCount; i++)
		{
			if ((m_pChildList[i]->Type() == OBJECT_NODE) || (m_pChildList[i]->Type() == OBJECT_MOBILEDEVICE))
				queueRemoveFromTarget(m_pChildList[i]->Id(), TRUE);
		}
		UnlockChildList();
	}
	NetObj::PrepareForDeletion();
}

/**
 * Check if template should be automatically applied to node
 */
BOOL Template::isApplicable(Node *node)
{
	NXSL_ServerEnv *pEnv;
	NXSL_Value *value;
	BOOL result = FALSE;

	LockData();
	if ((m_flags & TF_AUTO_APPLY) && (m_applyFilter != NULL))
	{
		pEnv = new NXSL_ServerEnv;
		m_applyFilter->setGlobalVariable(_T("$node"), new NXSL_Value(new NXSL_Object(&g_nxslNodeClass, node)));
		if (m_applyFilter->run(pEnv, 0, NULL) == 0)
		{
			value = m_applyFilter->getResult();
			result = ((value != NULL) && (value->getValueAsInt32() != 0));
		}
		else
		{
			TCHAR buffer[1024];

			_sntprintf(buffer, 1024, _T("Template::%s::%d"), m_szName, m_dwId);
			PostEvent(EVENT_SCRIPT_ERROR, g_dwMgmtNode, "ssd", buffer,
						 m_applyFilter->getErrorText(), m_dwId);
			nxlog_write(MSG_TEMPLATE_SCRIPT_EXECUTION_ERROR, EVENTLOG_WARNING_TYPE, "dss", m_dwId, m_szName, m_applyFilter->getErrorText());
		}
	}
	UnlockData();
	return result;
}

/**
 * Get last (current) DCI values. Moved to Template from DataCollectionTarget to allow 
 * simplified creation of DCI selection dialog in management console. For classes not
 * derived from DataCollectionTarget actual values will always be empty strings
 * with data type DCI_DT_NULL.
 */
DWORD Template::getLastValues(CSCPMessage *msg, bool objectTooltipOnly)
{
   lockDciAccess();

	DWORD dwId = VID_DCI_VALUES_BASE, dwCount = 0;
   for(int i = 0; i < m_dcObjects->size(); i++)
	{
		DCObject *object = m_dcObjects->get(i);
		if (object->hasValue() && 
          _tcsnicmp(object->getDescription(), _T("@system."), 8) &&
          (!objectTooltipOnly || object->isShowOnObjectTooltip()))
		{
			if (object->getType() == DCO_TYPE_ITEM)
			{
				((DCItem *)object)->getLastValue(msg, dwId);
				dwId += 50;
				dwCount++;
			}
			else if (object->getType() == DCO_TYPE_TABLE)
			{
				((DCTable *)object)->getLastValueSummary(msg, dwId);
				dwId += 50;
				dwCount++;
			}
		}
	}
   msg->SetVariable(VID_NUM_ITEMS, dwCount);

   unlockDciAccess();
   return RCC_SUCCESS;
}
