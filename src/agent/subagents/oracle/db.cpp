/*
 ** NetXMS - Network Management System
 ** Subagent for Oracle monitoring
 ** Copyright (C) 2009-2014 Raden Solutions
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU Lesser General Public License as published
 ** by the Free Software Foundation; either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public License
 ** along with this program; if not, write to the Free Software
 ** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **/

#include "oracle_subagent.h"

/**
 * Create new database instance object
 */
DatabaseInstance::DatabaseInstance(DatabaseInfo *info)
{
   memcpy(&m_info, info, sizeof(DatabaseInfo));
	m_pollerThread = INVALID_THREAD_HANDLE;
	m_session = NULL;
	m_connected = false;
	m_version = 0;
   m_data = NULL;
	m_mutex = MutexCreate();
   m_stopCondition = ConditionCreate(TRUE);
}

/**
 * Destructor
 */
DatabaseInstance::~DatabaseInstance()
{
   stop();
   MutexDestroy(m_mutex);
   ConditionDestroy(m_stopCondition);
   delete m_data;
}

/**
 * Run
 */
void DatabaseInstance::run()
{
   m_pollerThread = ThreadCreateEx(DatabaseInstance::pollerThreadStarter, 0, this);
}

/**
 * Stop
 */
void DatabaseInstance::stop()
{
   ConditionSet(m_stopCondition);
   ThreadJoin(m_pollerThread);
   m_pollerThread = INVALID_THREAD_HANDLE;
   if (m_session != NULL)
   {
      DBDisconnect(m_session);
      m_session = NULL;
   }
}

/**
 * Detect Oracle DBMS version
 */
int DatabaseInstance::getOracleVersion() 
{
	DB_RESULT hResult = DBSelect(m_session, _T("SELECT version FROM v$instance"));
	if (hResult == NULL)	
	{
		return 700;		// assume Oracle 7.0 by default
	}

	TCHAR versionString[32];
	DBGetField(hResult, 0, 0, versionString, 32);
	int major = 0, minor = 0;
	_stscanf(versionString, _T("%d.%d"), &major, &minor);
	DBFreeResult(hResult);

	return (major << 8) | minor;
}

/**
 * Poller thread starter
 */
THREAD_RESULT THREAD_CALL DatabaseInstance::pollerThreadStarter(void *arg)
{
   ((DatabaseInstance *)arg)->pollerThread();
   return THREAD_OK;
}

/**
 * Poller thread
 */
void DatabaseInstance::pollerThread()
{
   AgentWriteDebugLog(3, _T("ORACLE: poller thread for database %s started"), m_info.id);
   while(!ConditionWait(m_stopCondition, 30000))   // reconnect every 30 seconds
   {
      TCHAR errorText[DBDRV_MAX_ERROR_TEXT];
		m_session = DBConnect(g_driverHandle, m_info.name, NULL, m_info.username, m_info.password, NULL, errorText);
      if (m_session == NULL)
      {
         AgentWriteDebugLog(6, _T("ORACLE: cannot connect to database %s: %s"), m_info.id, errorText);
         continue;
      }

      m_connected = true;
		DBEnableReconnect(m_session, false);
      m_version = getOracleVersion();
      AgentWriteLog(NXLOG_INFO, _T("ORACLE: connection with database %s restored (version %d.%d)"), 
         m_info.id, m_version >> 8, m_version &0xFF);

      UINT32 sleepTime;
      do
      {
         INT64 startTime = GetCurrentTimeMs();
         if (!poll())
         {
            AgentWriteLog(NXLOG_WARNING, _T("ORACLE: connection with database %s lost"), m_info.id);
            break;
         }
         INT64 elapsedTime = GetCurrentTimeMs() - startTime;
         sleepTime = (UINT32)((elapsedTime >= 60000) ? 60000 : (60000 - elapsedTime));
      }
      while(!ConditionWait(m_stopCondition, sleepTime));

      m_connected = false;
      DBDisconnect(m_session);
      m_session = NULL;
   }
   AgentWriteDebugLog(3, _T("ORACLE: poller thread for database %s stopped"), m_info.id);
}

/**
 * Do actual database polling. Should return false if connection is broken.
 */
bool DatabaseInstance::poll()
{
   StringMap *data = new StringMap();

   int count = 0;
   int failures = 0;

   for(int i = 0; g_queries[i].name != NULL; i++)
   {
      if (g_queries[i].minVersion > m_version)
         continue;   // not supported by this database

      count++;
      DB_RESULT hResult = DBSelect(m_session, g_queries[i].query);
      if (hResult == NULL)
      {
         failures++;
         continue;
      }

      int numColumns = DBGetColumnCount(hResult);
      if (g_queries[i].instanceColumns > 0)
      {
         int col;

         // Process instance columns
         for(col = 0; col < g_queries[i].instanceColumns; col++)
         {
         }
      }
      else
      {
         TCHAR tag[256];
         _tcscpy(tag, g_queries[i].name);
         int tagLen = (int)_tcslen(tag);
         tag[tagLen++] = _T('/');
         for(int col = 0; col < numColumns; col++)
         {
            DBGetColumnName(hResult, col, &tag[tagLen], 256 - tagLen);
            data->setPreallocated(_tcsdup(tag), DBGetField(hResult, 0, col, NULL, 0));
         }
      }

      DBFreeResult(hResult);
   }

   // update cached data
   MutexLock(m_mutex);
   delete m_data;
   m_data = data;
   MutexUnlock(m_mutex);

   return failures < count;
}

/**
 * Get collected data
 */
bool DatabaseInstance::getData(const TCHAR *tag, TCHAR *value)
{
   bool success = false;
   MutexLock(m_mutex);
   if (m_data != NULL)
   {
      const TCHAR *v = m_data->get(tag);
      if (v != NULL)
      {
         ret_string(value, v);
         success = true;
      }
   }
   MutexUnlock(m_mutex);
   return success;
}
