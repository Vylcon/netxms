/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2013 Victor Kirhenshtein
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
**
** File: table.cpp
**
**/

#include "libnetxms.h"

/**
 * Create empty table
 */
Table::Table() : RefCountObject()
{
   m_nNumRows = 0;
   m_nNumCols = 0;
   m_data = NULL;
	m_title = NULL;
   m_source = DS_INTERNAL;
   m_columns = new ObjectArray<TableColumnDefinition>(8, 8, true);
}

/**
 * Create table from NXCP message
 */
Table::Table(CSCPMessage *msg) : RefCountObject()
{
   m_columns = new ObjectArray<TableColumnDefinition>(8, 8, true);
	createFromMessage(msg);
}

/**
 * Copy constructor
 */
Table::Table(Table *src) : RefCountObject()
{
   m_nNumRows = src->m_nNumRows;
   m_nNumCols = src->m_nNumCols;
   m_data = (TCHAR **)malloc(sizeof(TCHAR *) * m_nNumRows * m_nNumCols);
	int i;
   for(i = 0; i < m_nNumCols * m_nNumRows; i++)
      m_data[i] = _tcsdup(CHECK_NULL_EX(src->m_data[i]));
   m_title = (src->m_title != NULL) ? _tcsdup(src->m_title) : NULL;
   m_source = src->m_source;
   m_columns = new ObjectArray<TableColumnDefinition>(m_nNumCols, 8, true);
	for(i = 0; i < m_nNumCols; i++)
      m_columns->add(new TableColumnDefinition(src->m_columns->get(i)));
}

/**
 * Table destructor
 */
Table::~Table()
{
	destroy();
   delete m_columns;
}

/**
 * Destroy table data
 */
void Table::destroy()
{
   int i;

   m_columns->clear();

   for(i = 0; i < m_nNumRows * m_nNumCols; i++)
      safe_free(m_data[i]);
   safe_free(m_data);

	safe_free(m_title);
}

/**
 * Create table from NXCP message
 */
void Table::createFromMessage(CSCPMessage *msg)
{
	int i;
	UINT32 dwId;

	m_nNumRows = msg->GetVariableLong(VID_TABLE_NUM_ROWS);
	m_nNumCols = msg->GetVariableLong(VID_TABLE_NUM_COLS);
	m_title = msg->GetVariableStr(VID_TABLE_TITLE);
   m_source = (int)msg->GetVariableShort(VID_DCI_SOURCE_TYPE);

	for(i = 0, dwId = VID_TABLE_COLUMN_INFO_BASE; i < m_nNumCols; i++, dwId += 10)
	{
      m_columns->add(new TableColumnDefinition(msg, dwId));
	}
   if (msg->IsVariableExist(VID_INSTANCE_COLUMN))
   {
      TCHAR name[MAX_COLUMN_NAME];
      msg->GetVariableStr(VID_INSTANCE_COLUMN, name, MAX_COLUMN_NAME);
      for(i = 0; i < m_columns->size(); i++)
      {
         if (!_tcsicmp(m_columns->get(i)->getName(), name))
         {
            m_columns->get(i)->setInstanceColumn(true);
            break;
         }
      }
   }

	m_data = (TCHAR **)malloc(sizeof(TCHAR *) * m_nNumCols * m_nNumRows);
	for(i = 0, dwId = VID_TABLE_DATA_BASE; i < m_nNumCols * m_nNumRows; i++)
		m_data[i] = msg->GetVariableStr(dwId++);
}

/**
 * Update table from NXCP message
 */
void Table::updateFromMessage(CSCPMessage *msg)
{
	destroy();
	createFromMessage(msg);
}

/**
 * Fill NXCP message with table data
 */
int Table::fillMessage(CSCPMessage &msg, int offset, int rowLimit)
{
	int i, row, col;
	UINT32 id;

	msg.SetVariable(VID_TABLE_TITLE, CHECK_NULL_EX(m_title));
   msg.SetVariable(VID_DCI_SOURCE_TYPE, (WORD)m_source);

	if (offset == 0)
	{
		msg.SetVariable(VID_TABLE_NUM_ROWS, (UINT32)m_nNumRows);
		msg.SetVariable(VID_TABLE_NUM_COLS, (UINT32)m_nNumCols);

      for(i = 0, id = VID_TABLE_COLUMN_INFO_BASE; i < m_columns->size(); i++, id += 10)
         m_columns->get(i)->fillMessage(&msg, id);
	}
	msg.SetVariable(VID_TABLE_OFFSET, (UINT32)offset);

	int stopRow = (rowLimit == -1) ? m_nNumRows : min(m_nNumRows, offset + rowLimit);
	for(i = offset * m_nNumCols, row = offset, id = VID_TABLE_DATA_BASE; row < stopRow; row++)
	{
		for(col = 0; col < m_nNumCols; col++) 
		{
			TCHAR *tmp = m_data[i++];
			msg.SetVariable(id++, CHECK_NULL_EX(tmp));
		}
	}
	msg.SetVariable(VID_NUM_ROWS, (UINT32)(row - offset));

	if (row == m_nNumRows)
		msg.setEndOfSequence();
	return row;
}

/**
 * Add new column
 */
int Table::addColumn(const TCHAR *name, INT32 dataType, const TCHAR *displayName, bool isInstance)
{
   m_columns->add(new TableColumnDefinition(name, displayName, dataType, isInstance));
   if (m_nNumRows > 0)
   {
      TCHAR **ppNewData;
      int i, nPosOld, nPosNew;

      ppNewData = (TCHAR **)malloc(sizeof(TCHAR *) * m_nNumRows * (m_nNumCols + 1));
      for(i = 0, nPosOld = 0, nPosNew = 0; i < m_nNumRows; i++)
      {
         memcpy(&ppNewData[nPosNew], &m_data[nPosOld], sizeof(TCHAR *) * m_nNumCols);
         ppNewData[nPosNew + m_nNumCols] = NULL;
         nPosOld += m_nNumCols;
         nPosNew += m_nNumCols + 1;
      }
      safe_free(m_data);
      m_data = ppNewData;
   }

   m_nNumCols++;
	return m_nNumCols - 1;
}

/**
 * Get column index by name
 *
 * @param name column name
 * @return column index or -1 if there are no such column
 */
int Table::getColumnIndex(const TCHAR *name)
{
   for(int i = 0; i < m_columns->size(); i++)
      if (!_tcsicmp(name, m_columns->get(i)->getName()))
         return i;
   return -1;
}

/**
 * Add new row
 */
int Table::addRow()
{
   if (m_nNumCols > 0)
   {
      m_data = (TCHAR **)realloc(m_data, sizeof(TCHAR *) * (m_nNumRows + 1) * m_nNumCols);
      memset(&m_data[m_nNumRows * m_nNumCols], 0, sizeof(TCHAR *) * m_nNumCols);
   }
   m_nNumRows++;
	return m_nNumRows - 1;
}

/**
 * Delete row
 */
void Table::deleteRow(int row)
{
   if ((row < 0) || (row >= m_nNumRows))
      return;

   for(int i = 0; i < m_nNumCols; i++)
      safe_free(m_data[row * m_nNumCols + i]);
   m_nNumRows--;
   memmove(&m_data[row * m_nNumCols], &m_data[(row + 1) * m_nNumCols], sizeof(TCHAR *) * (m_nNumRows - row) * m_nNumCols);
}

/**
 * Delete column
 */
void Table::deleteColumn(int col)
{
   if ((col < 0) || (col >= m_nNumCols))
      return;

   TCHAR **data = (TCHAR **)malloc(sizeof(TCHAR *) * m_nNumRows * (m_nNumCols - 1));
   int spos = 0, dpos = 0;
   for(int i = 0; i < m_nNumRows; i++)
   {
      for(int j = 0; j < m_nNumCols; j++)
      {
         if (j == col)
         {
            safe_free(m_data[spos]);
         }
         else
         {
            data[dpos++] = m_data[spos];
         }
         spos++;
      }
   }
   m_columns->remove(col);
   m_nNumCols--;
   safe_free(m_data);
   m_data = data;
}

/**
 * Set data at position
 */
void Table::setAt(int nRow, int nCol, const TCHAR *pszData)
{
   if ((nRow < 0) || (nRow >= m_nNumRows) ||
       (nCol < 0) || (nCol >= m_nNumCols))
      return;

   safe_free(m_data[nRow * m_nNumCols + nCol]);
   m_data[nRow * m_nNumCols + nCol] = _tcsdup(pszData);
}

/**
 * Set pre-allocated data at position
 */
void Table::setPreallocatedAt(int nRow, int nCol, TCHAR *pszData)
{
   if ((nRow < 0) || (nRow >= m_nNumRows) ||
       (nCol < 0) || (nCol >= m_nNumCols))
      return;

   safe_free(m_data[nRow * m_nNumCols + nCol]);
   m_data[nRow * m_nNumCols + nCol] = pszData;
}

/**
 * Set integer data at position
 */
void Table::setAt(int nRow, int nCol, INT32 nData)
{
   TCHAR szBuffer[32];

   _sntprintf(szBuffer, 32, _T("%d"), (int)nData);
   setAt(nRow, nCol, szBuffer);
}

/**
 * Set unsigned integer data at position
 */
void Table::setAt(int nRow, int nCol, UINT32 dwData)
{
   TCHAR szBuffer[32];

   _sntprintf(szBuffer, 32, _T("%u"), (unsigned int)dwData);
   setAt(nRow, nCol, szBuffer);
}

/**
 * Set 64 bit integer data at position
 */
void Table::setAt(int nRow, int nCol, INT64 nData)
{
   TCHAR szBuffer[32];

   _sntprintf(szBuffer, 32, INT64_FMT, nData);
   setAt(nRow, nCol, szBuffer);
}

/**
 * Set unsigned 64 bit integer data at position
 */
void Table::setAt(int nRow, int nCol, UINT64 qwData)
{
   TCHAR szBuffer[32];

   _sntprintf(szBuffer, 32, UINT64_FMT, qwData);
   setAt(nRow, nCol, szBuffer);
}

/**
 * Set floating point data at position
 */
void Table::setAt(int nRow, int nCol, double dData)
{
   TCHAR szBuffer[32];

   _sntprintf(szBuffer, 32, _T("%f"), dData);
   setAt(nRow, nCol, szBuffer);
}

/**
 * Get data from position
 */
const TCHAR *Table::getAsString(int nRow, int nCol)
{
   if ((nRow < 0) || (nRow >= m_nNumRows) ||
       (nCol < 0) || (nCol >= m_nNumCols))
      return NULL;

   return m_data[nRow * m_nNumCols + nCol];
}

INT32 Table::getAsInt(int nRow, int nCol)
{
   const TCHAR *pszVal;

   pszVal = getAsString(nRow, nCol);
   return pszVal != NULL ? _tcstol(pszVal, NULL, 0) : 0;
}

UINT32 Table::getAsUInt(int nRow, int nCol)
{
   const TCHAR *pszVal;

   pszVal = getAsString(nRow, nCol);
   return pszVal != NULL ? _tcstoul(pszVal, NULL, 0) : 0;
}

INT64 Table::getAsInt64(int nRow, int nCol)
{
   const TCHAR *pszVal;

   pszVal = getAsString(nRow, nCol);
   return pszVal != NULL ? _tcstoll(pszVal, NULL, 0) : 0;
}

UINT64 Table::getAsUInt64(int nRow, int nCol)
{
   const TCHAR *pszVal;

   pszVal = getAsString(nRow, nCol);
   return pszVal != NULL ? _tcstoull(pszVal, NULL, 0) : 0;
}

double Table::getAsDouble(int nRow, int nCol)
{
   const TCHAR *pszVal;

   pszVal = getAsString(nRow, nCol);
   return pszVal != NULL ? _tcstod(pszVal, NULL) : 0;
}

/**
 * Add all rows from another table.
 * Identical table format assumed.
 *
 * @param src source table
 */
void Table::addAll(Table *src)
{
   if (m_nNumCols != src->m_nNumCols)
      return;

   m_data = (TCHAR **)realloc(m_data, sizeof(TCHAR *) * (m_nNumRows + src->m_nNumRows) * m_nNumCols);
   int dpos = m_nNumRows * m_nNumCols;
   int spos = 0;
   for(int i = 0; i < src->m_nNumRows; i++)
   {
      for(int j = 0; j < m_nNumCols; j++)
      {
         const TCHAR *value = src->m_data[spos++];
         m_data[dpos++] = (value != NULL) ? _tcsdup(value) : NULL;
      }
   }
   m_nNumRows += src->m_nNumRows;
}

/**
 * Copy one row from source table
 */
void Table::copyRow(Table *src, int row)
{
   if (m_nNumCols != src->m_nNumCols)
      return;

   if ((row < 0) || (row >= src->m_nNumRows))
      return;

   m_data = (TCHAR **)realloc(m_data, sizeof(TCHAR *) * (m_nNumRows + 1) * m_nNumCols);
   int dpos = m_nNumRows * m_nNumCols;
   int spos = row * src->m_nNumCols;
   for(int i = 0; i < m_nNumCols; i++)
   {
      const TCHAR *value = src->m_data[spos++];
      m_data[dpos++] = (value != NULL) ? _tcsdup(value) : NULL;
   }
   m_nNumRows++;
}

/**
 * Build instance string
 */
void Table::buildInstanceString(int row, TCHAR *buffer, size_t bufLen)
{
   if ((row < 0) || (row >= m_nNumRows))
   {
      buffer[0] = 0;
      return;
   }

   TCHAR **data = m_data + row * m_nNumCols;
   String instance;
   bool first = true;
   for(int i = 0; i < m_nNumCols; i++)
   {
      if (m_columns->get(i)->isInstanceColumn())
      {
         if (!first)
            instance += _T("~~~");
         first = false;
         instance += data[i];
      }
   }
   nx_strncpy(buffer, (const TCHAR *)instance, bufLen);
}

/**
 * Find row by instance value
 *
 * @return row number or -1 if no such row
 */
int Table::findRowByInstance(const TCHAR *instance)
{
   for(int i = 0; i < m_nNumRows; i++)
   {
      TCHAR currInstance[256];
      buildInstanceString(i, currInstance, 256);
      if (!_tcscmp(instance, currInstance))
         return i;
   }
   return -1;
}

/**
 * Create new table column definition
 */
TableColumnDefinition::TableColumnDefinition(const TCHAR *name, const TCHAR *displayName, INT32 dataType, bool isInstance)
{
   m_name = _tcsdup(CHECK_NULL(name));
   m_displayName = (displayName != NULL) ? _tcsdup(displayName) : _tcsdup(m_name);
   m_dataType = dataType;
   m_instanceColumn = isInstance;
}

/**
 * Create copy of existing table column definition
 */
TableColumnDefinition::TableColumnDefinition(TableColumnDefinition *src)
{
   m_name = _tcsdup(src->m_name);
   m_displayName = _tcsdup(src->m_displayName);
   m_dataType = src->m_dataType;
   m_instanceColumn = src->m_instanceColumn;
}

/**
 * Create table column definition from NXCP message
 */
TableColumnDefinition::TableColumnDefinition(CSCPMessage *msg, UINT32 baseId)
{
   m_name = msg->GetVariableStr(baseId);
   if (m_name == NULL)
      m_name = _tcsdup(_T("(null)"));
   m_dataType = msg->GetVariableLong(baseId + 1);
   m_displayName = msg->GetVariableStr(baseId + 2);
   if (m_displayName == NULL)
      m_displayName = _tcsdup(m_name);
   m_instanceColumn = msg->GetVariableShort(baseId + 3) ? true : false;
}

/**
 * Destructor for table column definition
 */
TableColumnDefinition::~TableColumnDefinition()
{
   free(m_name);
   free(m_displayName);
}

/**
 * Fill message with table column definition data
 */
void TableColumnDefinition::fillMessage(CSCPMessage *msg, UINT32 baseId)
{
   msg->SetVariable(baseId, m_name);
   msg->SetVariable(baseId + 1, (UINT32)m_dataType);
   msg->SetVariable(baseId + 2, m_displayName);
   msg->SetVariable(baseId + 3, (WORD)(m_instanceColumn ? 1 : 0));
}
