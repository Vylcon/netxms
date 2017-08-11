/*
** NetXMS - Network Management System
** Copyright (C) 2003-2016 Victor Kirhenshtein
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
** File: sensor.cpp
**/

#include "nxcore.h"

/**
 * Default empty Sensor class constructior
 */
Sensor::Sensor() : DataCollectionTarget()
{
   m_proxyNodeId = 0;
	m_flags = 0;
	m_deviceClass = SENSOR_CLASS_UNKNOWN;
	m_vendor = NULL;
	m_commProtocol = SENSOR_PROTO_UNKNOWN;
	m_xmlRegConfig = NULL;
	m_xmlConfig = NULL;
	m_serialNumber = NULL;
	m_deviceAddress = NULL;
	m_metaType = NULL;
	m_description = NULL;
	m_lastConnectionTime = 0;
	m_frameCount = 0; //zero when no info
   m_signalStrenght = 1; //+1 when no information(cannot be +)
   m_signalNoise = MAX_INT32; //*10 from origin number
   m_frequency = 0; //*10 from origin number
   m_lastStatusPoll = 0;
   m_lastConfigurationPoll = 0;
   m_dwDynamicFlags = 0; // TODO save to and retreive from DB
   m_hPollerMutex = MutexCreate();
   m_hAgentAccessMutex = MutexCreate();
   m_status = STATUS_UNKNOWN;
   m_proxyAgentConn = NULL;
}

/**
 * Constructor with all fields for Sensor class
 */
Sensor::Sensor(TCHAR *name, UINT32 flags, MacAddress macAddress, UINT32 deviceClass, TCHAR *vendor,
               UINT32 commProtocol, TCHAR *xmlRegConfig, TCHAR *xmlConfig, TCHAR *serialNumber, TCHAR *deviceAddress,
               TCHAR *metaType, TCHAR *description, UINT32 proxyNode) : DataCollectionTarget(name)
{
   m_flags = flags;
   m_macAddress = macAddress;
	m_deviceClass = deviceClass;
	m_vendor = vendor;
	m_commProtocol = commProtocol;
	m_xmlRegConfig = xmlRegConfig;
	m_xmlConfig = xmlConfig;
	m_serialNumber = serialNumber;
	m_deviceAddress = deviceAddress;
	m_metaType = metaType;
	m_description = description;
	m_proxyNodeId = proxyNode;
   m_lastConnectionTime = 0;
	m_frameCount = 0; //zero when no info
   m_signalStrenght = 1; //+1 when no information(cannot be +)
   m_signalNoise = MAX_INT32; //*10 from origin number
   m_frequency = 0; //*10 from origin number
   m_lastStatusPoll = 0;
   m_lastConfigurationPoll = 0;
   m_dwDynamicFlags = 0;
   m_hPollerMutex = MutexCreate();
   m_hAgentAccessMutex = MutexCreate();
   m_status = STATUS_UNKNOWN;
   m_proxyAgentConn = NULL;
}

Sensor *Sensor::createSensor(TCHAR *name, NXCPMessage *request)
{
   Sensor *sensor = new Sensor(name,
                          request->getFieldAsUInt32(VID_SENSOR_FLAGS),
                          request->getFieldAsMacAddress(VID_MAC_ADDR),
                          request->getFieldAsUInt32(VID_DEVICE_CLASS),
                          request->getFieldAsString(VID_VENDOR),
                          request->getFieldAsUInt32(VID_COMM_PROTOCOL),
                          request->getFieldAsString(VID_XML_REG_CONFIG),
                          request->getFieldAsString(VID_XML_CONFIG),
                          request->getFieldAsString(VID_SERIAL_NUMBER),
                          request->getFieldAsString(VID_DEVICE_ADDRESS),
                          request->getFieldAsString(VID_META_TYPE),
                          request->getFieldAsString(VID_DESCRIPTION),
                          request->getFieldAsUInt32(VID_SENSOR_PROXY));

   switch(request->getFieldAsUInt32(VID_COMM_PROTOCOL))
   {
      case COMM_LORAWAN:
         sensor->generateGuid();
         if (registerLoraDevice(sensor) == NULL)
         {
            delete sensor;
            return NULL;
         }
         break;
   }
   return sensor;
}

/**
 * Create agent connection
 */
UINT32 Sensor::connectToAgent()
{
   UINT32 rcc = ERR_CONNECT_FAILED;
   if (IsShutdownInProgress())
      return rcc;

   NetObj *proxy = FindObjectById(m_proxyNodeId, OBJECT_NODE);
   if(proxy == NULL)
      return ERR_INVALID_OBJECT;

   // Create new agent connection object if needed
   if (m_proxyAgentConn == NULL)
   {
      m_proxyAgentConn = ((Node *)proxy)->createAgentConnection();
      if (m_proxyAgentConn != NULL)
      {
         nxlog_debug(2, _T("Sensor::connectToAgent(%s [%d]): new agent connection created"), m_name, m_id);
         rcc = ERR_SUCCESS;
      }
   }
   else if (m_proxyAgentConn->nop() == ERR_SUCCESS)
   {
      DbgPrintf(2, _T("Sensor::connectToAgent(%s [%d]): already connected"), m_name, m_id);
      rcc = ERR_SUCCESS;
   }
   else
   {
      deleteAgentConnection();
      connectToAgent(); // retry to connect
   }

   return rcc;
}

/**
 * Delete agent connection
 */
void Sensor::deleteAgentConnection()
{
   if (m_proxyAgentConn != NULL)
   {
      m_proxyAgentConn->decRefCount();
      m_proxyAgentConn = NULL;
   }
}

/**
 * Register LoRa WAN device
 */
Sensor *Sensor::registerLoraDevice(Sensor *sensor)
{
   Config regConfig;
   char regXml[MAX_CONFIG_VALUE];
   WideCharToMultiByte(CP_UTF8, 0, sensor->getXmlRegConfig(), -1, regXml, MAX_CONFIG_VALUE, NULL, NULL);
   nxlog_debug(2, _T("regConfig: %hs"), regXml);
   regConfig.loadXmlConfigFromMemory(regXml, MAX_CONFIG_VALUE, NULL, "config", false);

   Config config;
   char xml[MAX_CONFIG_VALUE];
   WideCharToMultiByte(CP_UTF8, 0, sensor->getXmlConfig(), -1, xml, MAX_CONFIG_VALUE, NULL, NULL);
   nxlog_debug(2, _T("Config: %hs"), xml);
   config.loadXmlConfigFromMemory(xml, MAX_CONFIG_VALUE, NULL, "config", false);

   sensor->agentLock();
   UINT32 rcc = sensor->connectToAgent();
   if(rcc == ERR_INVALID_OBJECT)
   {
      sensor->agentUnlock();
      return NULL;
   }
   else if (rcc == ERR_CONNECT_FAILED)
   {
      sensor->agentUnlock();
      return sensor; //Unprovisoned sensor - will try to provison it on next connect
   }

   NXCPMessage msg(sensor->getAgentConnection()->getProtocolVersion());
   msg.setCode(CMD_REGISTER_LORAWAN_SENSOR);
   msg.setId(sensor->getAgentConnection()->generateRequestId());
   msg.setField(VID_DEVICE_ADDRESS, sensor->getDeviceAddress());
   msg.setField(VID_MAC_ADDR, sensor->getMacAddress());
   msg.setField(VID_GUID, sensor->getGuid());
   msg.setField(VID_DECODER, config.getValueAsInt(_T("/decoder"), 0));
   msg.setField(VID_REG_TYPE, regConfig.getValueAsInt(_T("/registrationType"), 0));
   if(regConfig.getValueAsInt(_T("/registrationType"), 0) == 0)
   {
      msg.setField(VID_LORA_APP_EUI, regConfig.getValue(_T("/appEUI")));
      msg.setField(VID_LORA_APP_KEY, regConfig.getValue(_T("/appKey")));
   }
   else
   {
      msg.setField(VID_LORA_APP_S_KEY, regConfig.getValue(_T("/appSKey")));
      msg.setField(VID_LORA_NWK_S_KWY, regConfig.getValue(_T("/nwkSKey")));
   }
   NXCPMessage *response = sensor->getAgentConnection()->customRequest(&msg);
   if (response != NULL)
   {
      if(response->getFieldAsUInt32(VID_RCC) == RCC_SUCCESS)
         sensor->setProvisoned();

      delete response;
   }
   sensor->agentUnlock();

   return sensor;
}

//set correct status calculation function
//set correct configuration poll - provision if possible, for lora get device name, get all possible DCI's, try to do provisionning
//set status poll - check if connection is on if not generate alarm, check, that proxy is up and running

/**
 * Sensor class destructor
 */
Sensor::~Sensor()
{
   free(m_vendor);
   free(m_xmlRegConfig);
   free(m_xmlConfig);
   free(m_serialNumber);
   free(m_deviceAddress);
   free(m_metaType);
   free(m_description);
   MutexDestroy(m_hPollerMutex);
   MutexDestroy(m_hAgentAccessMutex);
   if (m_proxyAgentConn != NULL)
      m_proxyAgentConn->decRefCount();
}

/**
 * Load from database SensorDevice class
 */
bool Sensor::loadFromDatabase(DB_HANDLE hdb, UINT32 id)
{
   m_id = id;

   if (!loadCommonProperties(hdb))
   {
      DbgPrintf(2, _T("Cannot load common properties for sensor object %d"), id);
      return false;
   }

	TCHAR query[256];
	_sntprintf(query, 256, _T("SELECT flags,mac_address,device_class,vendor,communication_protocol,xml_config,serial_number,device_address,")
                          _T("meta_type,description,last_connection_time,frame_count,signal_strenght,signal_noise,frequency,proxy_node,xml_reg_config FROM sensors WHERE id=%d"), (int)m_id);
	DB_RESULT hResult = DBSelect(hdb, query);
	if (hResult == NULL)
		return false;

   m_flags = DBGetFieldULong(hResult, 0, 0);
   m_macAddress = DBGetFieldMacAddr(hResult, 0, 1);
	m_deviceClass = DBGetFieldULong(hResult, 0, 2);
	m_vendor = DBGetField(hResult, 0, 3, NULL, 0);
	m_commProtocol = DBGetFieldULong(hResult, 0, 4);
	m_xmlConfig = DBGetField(hResult, 0, 5, NULL, 0);
	m_serialNumber = DBGetField(hResult, 0, 6, NULL, 0);
	m_deviceAddress = DBGetField(hResult, 0, 7, NULL, 0);
	m_metaType = DBGetField(hResult, 0, 8, NULL, 0);
	m_description = DBGetField(hResult, 0, 9, NULL, 0);
   m_lastConnectionTime = DBGetFieldULong(hResult, 0, 10);
   m_frameCount = DBGetFieldULong(hResult, 0, 11);
   m_signalStrenght = DBGetFieldLong(hResult, 0, 12);
   m_signalNoise = DBGetFieldLong(hResult, 0, 13);
   m_frequency = DBGetFieldLong(hResult, 0, 14);
   m_proxyNodeId = DBGetFieldLong(hResult, 0, 15);
   m_xmlRegConfig = DBGetField(hResult, 0, 16, NULL, 0);

   // Load DCI and access list
   loadACLFromDB(hdb);
   loadItemsFromDB(hdb);
   for(int i = 0; i < m_dcObjects->size(); i++)
      if (!m_dcObjects->get(i)->loadThresholdsFromDB(hdb))
         return false;

   return true;
}

/**
 * Save to database Sensor class
 */
BOOL Sensor::saveToDatabase(DB_HANDLE hdb)
{
   lockProperties();

   BOOL success = saveCommonProperties(hdb);

   if(success)
   {
      DB_STATEMENT hStmt;
      bool isNew = !(IsDatabaseRecordExist(hdb, _T("sensors"), _T("id"), m_id));
      if (isNew)
         hStmt = DBPrepare(hdb, _T("INSERT INTO sensors (flags,mac_address,device_class,vendor,communication_protocol,xml_config,serial_number,device_address,meta_type,description,last_connection_time,frame_count,signal_strenght,signal_noise,frequency,proxy_node,id,xml_reg_config) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
      else
         hStmt = DBPrepare(hdb, _T("UPDATE sensors SET flags=?,mac_address=?,device_class=?,vendor=?,communication_protocol=?,xml_config=?,serial_number=?,device_address=?,meta_type=?,description=?,last_connection_time=?,frame_count=?,signal_strenght=?,signal_noise=?,frequency=?,proxy_node=? WHERE id=?"));
      if (hStmt != NULL)
      {
         DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_flags);
         DBBind(hStmt, 2, DB_SQLTYPE_VARCHAR, m_macAddress);
         DBBind(hStmt, 3, DB_SQLTYPE_INTEGER, (INT32)m_deviceClass);
         DBBind(hStmt, 4, DB_SQLTYPE_VARCHAR, m_vendor, DB_BIND_STATIC);
         DBBind(hStmt, 5, DB_SQLTYPE_INTEGER, (INT32)m_commProtocol);
         DBBind(hStmt, 6, DB_SQLTYPE_VARCHAR, m_xmlConfig, DB_BIND_STATIC);
         DBBind(hStmt, 7, DB_SQLTYPE_VARCHAR, m_serialNumber, DB_BIND_STATIC);
         DBBind(hStmt, 8, DB_SQLTYPE_VARCHAR, m_deviceAddress, DB_BIND_STATIC);
         DBBind(hStmt, 9, DB_SQLTYPE_VARCHAR, m_metaType, DB_BIND_STATIC);
         DBBind(hStmt, 10, DB_SQLTYPE_VARCHAR, m_description, DB_BIND_STATIC);
         DBBind(hStmt, 11, DB_SQLTYPE_INTEGER, (UINT32)m_lastConnectionTime);
         DBBind(hStmt, 12, DB_SQLTYPE_INTEGER, m_frameCount);
         DBBind(hStmt, 13, DB_SQLTYPE_INTEGER, m_signalStrenght);
         DBBind(hStmt, 14, DB_SQLTYPE_INTEGER, m_signalNoise);
         DBBind(hStmt, 15, DB_SQLTYPE_INTEGER, m_frequency);
         DBBind(hStmt, 16, DB_SQLTYPE_INTEGER, m_proxyNodeId);
         DBBind(hStmt, 17, DB_SQLTYPE_INTEGER, m_id);
         if(isNew)
            DBBind(hStmt, 18, DB_SQLTYPE_VARCHAR, m_xmlRegConfig, DB_BIND_STATIC);

         success = DBExecute(hStmt);

         DBFreeStatement(hStmt);
      }
      else
      {
         success = FALSE;
      }
	}

   // Save data collection items
   if (success)
   {
		lockDciAccess(false);
      for(int i = 0; i < m_dcObjects->size(); i++)
         m_dcObjects->get(i)->saveToDatabase(hdb);
		unlockDciAccess();
   }

   // Save access list
   if(success)
      saveACLToDB(hdb);

   // Clear modifications flag and unlock object
	if (success)
		m_isModified = FALSE;
   unlockProperties();

   return success;
}

/**
 * Delete from database
 */
bool Sensor::deleteFromDatabase(DB_HANDLE hdb)
{
   bool success = DataCollectionTarget::deleteFromDatabase(hdb);
   if (success)
      success = executeQueryOnObject(hdb, _T("DELETE FROM sensors WHERE id=?"));
   return success;
}


NXSL_Value *Sensor::createNXSLObject()
{
   return new NXSL_Value(new NXSL_Object(&g_nxslMobileDeviceClass, this));
}

/**
 * Sensor class serialization to json
 */
json_t *Sensor::toJson()
{
   json_t *root = DataCollectionTarget::toJson();
   json_object_set_new(root, "flags", json_integer(m_flags));
   json_object_set_new(root, "macAddr", json_string_t(m_macAddress.toString(MAC_ADDR_FLAT_STRING)));
   json_object_set_new(root, "deviceClass", json_integer(m_deviceClass));
   json_object_set_new(root, "vendor", json_string_t(m_vendor));
   json_object_set_new(root, "commProtocol", json_integer(m_commProtocol));
   json_object_set_new(root, "xmlConfig", json_string_t(m_xmlConfig));
   json_object_set_new(root, "serialNumber", json_string_t(m_serialNumber));
   json_object_set_new(root, "deviceAddress", json_string_t(m_deviceAddress));
   json_object_set_new(root, "metaType", json_string_t(m_metaType));
   json_object_set_new(root, "description", json_string_t(m_description));
   json_object_set_new(root, "proxyNode", json_integer(m_proxyNodeId));
   return root;
}

void Sensor::fillMessageInternal(NXCPMessage *msg)
{
   DataCollectionTarget::fillMessageInternal(msg);
   msg->setField(VID_SENSOR_FLAGS, m_flags);
	msg->setField(VID_MAC_ADDR, m_macAddress);
   msg->setField(VID_DEVICE_CLASS, m_deviceClass);
	msg->setField(VID_VENDOR, CHECK_NULL_EX(m_vendor));
   msg->setField(VID_COMM_PROTOCOL, m_commProtocol);
	msg->setField(VID_XML_CONFIG, CHECK_NULL_EX(m_xmlConfig));
	msg->setField(VID_XML_REG_CONFIG, CHECK_NULL_EX(m_xmlRegConfig));
	msg->setField(VID_SERIAL_NUMBER, CHECK_NULL_EX(m_serialNumber));
	msg->setField(VID_DEVICE_ADDRESS, CHECK_NULL_EX(m_deviceAddress));
	msg->setField(VID_META_TYPE, CHECK_NULL_EX(m_metaType));
	msg->setField(VID_DESCRIPTION, CHECK_NULL_EX(m_description));
	msg->setFieldFromTime(VID_LAST_CONN_TIME, m_lastConnectionTime);
	msg->setField(VID_FRAME_COUNT, m_frameCount);
	msg->setField(VID_SIGNAL_STRENGHT, m_signalStrenght);
	msg->setField(VID_SIGNAL_NOISE, m_signalNoise);
	msg->setField(VID_FREQUENCY, m_frequency);
	msg->setField(VID_SENSOR_PROXY, m_proxyNodeId);
}

UINT32 Sensor::modifyFromMessageInternal(NXCPMessage *request)
{
   if (request->isFieldExist(VID_MAC_ADDR))
      m_macAddress = request->getFieldAsMacAddress(VID_MAC_ADDR);
   if (request->isFieldExist(VID_VENDOR))
   {
      free(m_vendor);
      m_vendor = request->getFieldAsString(VID_VENDOR);
   }
   if (request->isFieldExist(VID_DEVICE_CLASS))
      m_deviceClass = request->getFieldAsUInt32(VID_DEVICE_CLASS);
   if (request->isFieldExist(VID_SERIAL_NUMBER))
   {
      free(m_serialNumber);
      m_serialNumber = request->getFieldAsString(VID_SERIAL_NUMBER);
   }
   if (request->isFieldExist(VID_DEVICE_ADDRESS))
   {
      free(m_deviceAddress);
      m_deviceAddress = request->getFieldAsString(VID_DEVICE_ADDRESS);
   }
   if (request->isFieldExist(VID_META_TYPE))
   {
      free(m_metaType);
      m_metaType = request->getFieldAsString(VID_META_TYPE);
   }
   if (request->isFieldExist(VID_DESCRIPTION))
   {
      free(m_description);
      m_description = request->getFieldAsString(VID_DESCRIPTION);
   }
   if (request->isFieldExist(VID_SENSOR_PROXY))
      m_proxyNodeId = request->getFieldAsUInt32(VID_SENSOR_PROXY);
   if (request->isFieldExist(VID_XML_CONFIG))
   {
      free(m_xmlConfig);
      m_xmlConfig = request->getFieldAsString(VID_XML_CONFIG);
   }

   return DataCollectionTarget::modifyFromMessageInternal(request);
}

/**
 * Calculate sensor status
 */
void Sensor::calculateStatus()
{
   if (m_flags == 0 || m_flags == SENSOR_PROVISIONED)
      m_status = STATUS_UNKNOWN;
   else if (m_flags & SENSOR_ACTIVE)
      m_status = STATUS_NORMAL;
   else if (m_flags & ~SENSOR_ACTIVE)
      m_status = STATUS_CRITICAL;
}

/**
 * Entry point for configuration poller
 */
void Sensor::configurationPoll(PollerInfo *poller)
{
   nxlog_debug(2, _T(":::::     Starting configuration poll for sensor %s (ID: %d)"), m_name, m_id);
   poller->startExecution();
   ObjectTransactionStart();
   configurationPoll(NULL, 0, poller, 0);
   ObjectTransactionEnd();
   delete poller;
}

/**
 * Perform configuration poll on node
 */
void Sensor::configurationPoll(ClientSession *pSession, UINT32 dwRqId, PollerInfo *poller, int maskBits)
{
   if (m_dwDynamicFlags & NDF_DELETE_IN_PROGRESS)
   {
      if (dwRqId == 0)
         m_dwDynamicFlags &= ~NDF_QUEUED_FOR_CONFIG_POLL;
      return;
   }

   poller->setStatus(_T("wait for lock"));
   pollerLock();

   if (IsShutdownInProgress())
   {
      pollerUnlock();
      return;
   }

   nxlog_debug(2, _T("Starting configuration poll for sensor %s (ID: %d), m_flags: %d"), m_name, m_id, m_flags);

   bool hasChanges = false;

   if (!(m_flags & SENSOR_PROVISIONED))
   {
      if ((registerLoraDevice(this) != NULL) && (m_flags & SENSOR_PROVISIONED))
      {
         nxlog_debug(2, _T("ConfPoll(%s [%d}): sensor successfully registered"), m_name, m_id);
         hasChanges = true;
      }
   }
   if ((m_flags & SENSOR_PROVISIONED) && (m_deviceAddress == NULL))
   {
      getItemFromAgent(_T("LoraWAN.DevAddr(*)"), 0, m_deviceAddress);
      if (m_deviceAddress != NULL)
      {
         nxlog_debug(2, _T("ConfPoll(%s [%d}): sensor DevAddr[%s] successfully obtained registered"), m_deviceAddress, m_name, m_id);
         hasChanges = true;
      }
   }
   nxlog_debug(2, _T("ConfPoll(%s [%d}):  DevAddr - %s"), m_name, m_id, m_deviceAddress);

   poller->setStatus(_T("cleanup"));
   if (dwRqId == 0)
      m_dwDynamicFlags &= ~NDF_QUEUED_FOR_CONFIG_POLL;
   m_dwDynamicFlags &= ~NDF_RECHECK_CAPABILITIES;
   m_lastConfigurationPoll = time(NULL);
   DbgPrintf(2, _T("Finished configuration poll for sensor %s (ID: %d)"), m_name, m_id);
   pollerUnlock();

   if (hasChanges)
   {
      lockProperties();
      setModified();
      unlockProperties();
   }
}

/**
 * Status poller entry point
 */
void Sensor::statusPoll(PollerInfo *poller)
{
   poller->startExecution();
   statusPoll(NULL, 0, poller);

   delete poller;
}

/**
 * Perform status poll on sensor
 */
void Sensor::statusPoll(ClientSession *pSession, UINT32 dwRqId, PollerInfo *poller)
{
   if (m_dwDynamicFlags & NDF_DELETE_IN_PROGRESS) // TODO
   {
      if (dwRqId == 0)
         m_dwDynamicFlags &= ~NDF_QUEUED_FOR_STATUS_POLL;
      return;
   }

   Queue *pQueue = new Queue;     // Delayed event queue

   poller->setStatus(_T("wait for lock"));
   pollerLock();

   if (IsShutdownInProgress())
   {
      delete pQueue;
      pollerUnlock();
      return;
   }

   sendPollerMsg(dwRqId, _T("Starting status poll for sensor %s\r\n"), m_name);
   DbgPrintf(2, _T("Starting status poll for sensor %s (ID: %d)"), m_name, m_id);

   DbgPrintf(2, _T("StatusPoll(%s): checking agent"), m_name);
   poller->setStatus(_T("check agent"));
   sendPollerMsg(dwRqId, _T("Checking NetXMS agent connectivity\r\n"));

   agentLock();
   UINT32 rcc = connectToAgent();
   if (rcc == ERR_SUCCESS)
   {
      DbgPrintf(2, _T("StatusPoll(%s): connected to agent"), m_name);
      if (m_dwDynamicFlags & NDF_AGENT_UNREACHABLE)
      {
         m_dwDynamicFlags &= ~NDF_AGENT_UNREACHABLE;
         PostEventEx(pQueue, EVENT_AGENT_OK, m_id, NULL);
         sendPollerMsg(dwRqId, POLLER_INFO _T("Connectivity with NetXMS agent restored\r\n"));
      }
   }
   else
   {
      DbgPrintf(2, _T("StatusPoll(%s): agent unreachable, rcc: %d"), m_name, rcc);
      sendPollerMsg(dwRqId, POLLER_ERROR _T("NetXMS agent unreachable\r\n"));
      if (!(m_dwDynamicFlags & NDF_AGENT_UNREACHABLE))
      {
         m_dwDynamicFlags |= NDF_AGENT_UNREACHABLE;
         PostEventEx(pQueue, EVENT_AGENT_FAIL, m_id, NULL);
      }
   }
   agentUnlock();
   DbgPrintf(2, _T("StatusPoll(%s): agent check finished"), m_name);

   switch(m_commProtocol)
   {
      case COMM_LORAWAN:
         nxlog_debug(2, _T("StatusPoll(%s [%d}): Switch, m_flags: %d"), m_name, m_id, m_flags);
         if (m_flags & SENSOR_PROVISIONED)
         {
            nxlog_debug(2, _T("StatusPoll(%s [%d}): Provisioned"), m_name, m_id);
            TCHAR lastValue[MAX_DCI_STRING_VALUE];
            time_t now;
            getItemFromAgent(_T("LoraWAN.LastContact(*)"), MAX_DCI_STRING_VALUE, lastValue);
            time_t lastConnectionTime = _tcstol(lastValue, NULL, 0);
            if (lastConnectionTime != 0)
               m_lastConnectionTime = lastConnectionTime;

            now = time(NULL);
            nxlog_debug(2, _T("StatusPoll(%s [%d}): Last conn time %d"), m_name, m_id, lastConnectionTime);

            if (!(m_flags & SENSOR_REGISTERED))
            {
               if (m_lastConnectionTime > 0)
               {
                  m_flags |= SENSOR_REGISTERED;
                  nxlog_debug(2, _T("StatusPoll(%s [%d}): Status set to REGISTERED"), m_name, m_id);
               }
            }
            if (m_flags & SENSOR_REGISTERED)
            {
               if (now - m_lastConnectionTime > 3600) // Last contact > 1h
               {
                  m_flags &= ~SENSOR_ACTIVE;
                  nxlog_debug(2, _T("StatusPoll(%s [%d}): Inactive for over 1h, status set to INACTIVE"), m_name, m_id);
               }
               else
               {
                  m_flags |= SENSOR_ACTIVE;
                  nxlog_debug(2, _T("StatusPoll(%s [%d}): Status set to ACTIVE"), m_name, m_id);
                  getItemFromAgent(_T("LoraWAN.RSSI(*)"), MAX_DCI_STRING_VALUE, lastValue);
                  m_signalStrenght = _tcstod(lastValue, NULL);
                  getItemFromAgent(_T("LoraWAN.SNR(*)"), MAX_DCI_STRING_VALUE, lastValue);
                  m_signalNoise = (_tcstod(lastValue, NULL)*10);
                  getItemFromAgent(_T("LoraWAN.Frequency(*)"), MAX_DCI_STRING_VALUE, lastValue);
                  m_frequency = (_tcstod(lastValue, NULL)*10);
               }
            }
            nxlog_debug(2, _T("StatusPoll(%s [%d}): Post, m_flags: %d"), m_name, m_id, m_flags);

            lockProperties();
            calculateStatus();
            setModified();
            unlockProperties();
         }
         break;
      case COMM_DLMS:
         break;
      default:
         break;
   }

   // Send delayed events and destroy delayed event queue
   if (pQueue != NULL)
   {
      ResendEvents(pQueue);
      delete pQueue;
   }

   if (dwRqId == 0)
      m_dwDynamicFlags &= ~NDF_QUEUED_FOR_STATUS_POLL;

   pollerUnlock();
   m_lastStatusPoll = time(NULL);
   DbgPrintf(2, _T("Finished status poll for sensor %s (ID: %d)"), m_name, m_id);
}

/**
 * Get item's value via native agent
 */
UINT32 Sensor::getItemFromAgent(const TCHAR *szParam, UINT32 dwBufSize, TCHAR *szBuffer)
{
   if (m_dwDynamicFlags & NDF_AGENT_UNREACHABLE)
      return DCE_COMM_ERROR;

   UINT32 dwError = ERR_NOT_CONNECTED, dwResult = DCE_COMM_ERROR;
   int retry = 3;

   agentLock();

   nxlog_debug(2, _T("Sensor(%s)->GetItemFromAgent(%s)"), m_name, szParam);
   // Establish connection if needed
   if (connectToAgent() != ERR_SUCCESS)
   {
      agentUnlock();
      return dwResult;
   }

   String parameter(szParam);
   parameter.replace(_T("*"), m_guid.toString());

   // Get parameter from agent
   while(retry-- > 0)
   {
      dwError = m_proxyAgentConn->getParameter(parameter, dwBufSize, szBuffer);
      switch(dwError)
      {
         case ERR_SUCCESS:
            dwResult = DCE_SUCCESS;
            break;
         case ERR_UNKNOWN_PARAMETER:
            dwResult = DCE_NOT_SUPPORTED;
            break;
         case ERR_NO_SUCH_INSTANCE:
            dwResult = DCE_NO_SUCH_INSTANCE;
            break;
         case ERR_NOT_CONNECTED:
         case ERR_CONNECTION_BROKEN:
            break;
         case ERR_REQUEST_TIMEOUT:
            // Reset connection to agent after timeout
            nxlog_debug(2, _T("Sensor(%s)->GetItemFromAgent(%s): timeout; resetting connection to agent..."), m_name, szParam);
            if (!connectToAgent())
               break;
            nxlog_debug(2, _T("Sensor(%s)->GetItemFromAgent(%s): connection to agent restored successfully"), m_name, szParam);
            break;
         case ERR_INTERNAL_ERROR:
            dwResult = DCE_COLLECTION_ERROR;
            break;
      }
   }

   agentUnlock();
   nxlog_debug(7, _T("Sensor(%s)->GetItemFromAgent(%s): dwError=%d dwResult=%d"), m_name, szParam, dwError, dwResult);
   return dwResult;
}

/**
 * Prepare sensor object for deletion
 */
void Sensor::prepareForDeletion()
{
   // Prevent sensor from being queued for polling
   lockProperties();
   m_dwDynamicFlags |= NDF_POLLING_DISABLED | NDF_DELETE_IN_PROGRESS;
   unlockProperties();

   // Wait for all pending polls
   DbgPrintf(2, _T("Sensor::PrepareForDeletion(%s [%d]): waiting for outstanding polls to finish"), m_name, (int)m_id);
   while(1)
   {
      lockProperties();
      if ((m_dwDynamicFlags &
            (NDF_QUEUED_FOR_STATUS_POLL | NDF_QUEUED_FOR_CONFIG_POLL)) == 0)
      {
         unlockProperties();
         break;
      }
      unlockProperties();
      ThreadSleepMs(100);
   }
   DbgPrintf(2, _T("Sensor::PrepareForDeletion(%s [%d]): no outstanding polls left"), m_name, (int)m_id);

   agentLock();
   if(connectToAgent() == ERR_SUCCESS)
   {
      NXCPMessage msg(m_proxyAgentConn->getProtocolVersion());
      msg.setCode(CMD_UNREGISTER_LORAWAN_SENSOR);
      msg.setId(m_proxyAgentConn->generateRequestId());
      msg.setField(VID_GUID, m_guid);
      NXCPMessage *response = m_proxyAgentConn->customRequest(&msg);
      if (response != NULL)
      {
         if(response->getFieldAsUInt32(VID_RCC) == RCC_SUCCESS)
            DbgPrintf(2, _T("Sensor::PrepareForDeletion(%s [%d]): successfully unregistered from LoRaWAN server"), m_name, (int)m_id);

         delete response;
      }
   }
   agentUnlock();

   DataCollectionTarget::prepareForDeletion();
}

