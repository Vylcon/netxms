/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2009 NetXMS Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
package org.netxms.client;

import java.util.ArrayList;
import java.util.List;

import org.netxms.base.NXCPCodes;
import org.netxms.base.NXCPMessage;

/**
 * Generic class for holding data in tabular format. Table has named columns. All data stored as strings.
 *
 */
public class Table
{
	private List<String> columns;
	private List<List<String>> data;

	/**
	 * Create empty table
	 */
	public Table()
	{
		columns = new ArrayList<String>(0);
		data = new ArrayList<List<String>>(0);
	}

	/**
	 * Create table from data in NXCP message
	 *
	 * @param msg NXCP message
	 */
	public Table(final NXCPMessage msg)
	{
		final int columnCount = msg.getVariableAsInteger(NXCPCodes.VID_TABLE_NUM_COLS);
		columns = new ArrayList<String>(columnCount);
		long varId = NXCPCodes.VID_TABLE_COLUMN_INFO_BASE;
		for(int i = 0; i < columnCount; i++, varId += 9L)
		{
			columns.add(msg.getVariableAsString(varId++));
		}

		final int rowCount = msg.getVariableAsInteger(NXCPCodes.VID_TABLE_NUM_ROWS);
		data = new ArrayList<List<String>>(rowCount);
		varId = NXCPCodes.VID_TABLE_DATA_BASE;
		for(int i = 0; i < rowCount; i++)
		{
			final List<String> row = new ArrayList<String>(columnCount);
			for(int j = 0; j < columnCount; j++)
			{
				row.add(msg.getVariableAsString(varId++));
			}
			data.add(row);
		}
	}

	/**
	 * Add data from additional messages
	 * @param msg
	 */
	public void addDataFromMessage(final NXCPMessage msg)
	{
		final int rowCount = msg.getVariableAsInteger(NXCPCodes.VID_TABLE_NUM_ROWS);
		long varId = NXCPCodes.VID_TABLE_DATA_BASE;
		for(int i = 0; i < rowCount; i++)
		{
			final List<String> row = new ArrayList<String>(columns.size());
         //noinspection ForLoopReplaceableByForEach
         for(int j = 0; j < columns.size(); j++)
         {
             row.add(msg.getVariableAsString(varId++));
         }
			data.add(row);
		}
	}

	/**
	 * Get number of columns in table
	 *
	 * @return Number of columns
	 */
	public int getColumnCount()
	{
		return columns.size();
	}

	/**
	 * Get number of rows in table
	 *
	 * @return Number of rows
	 */
	public int getRowCount()
	{
		return data.size();
	}

	/**
	 * Get column name
	 *
	 * @param column Column index (zero-based)
	 * @return Column name
	 * @throws IndexOutOfBoundsException if column index is out of range (column < 0 || column >= getColumnCount())
	 */
	public String getColumnName(final int column) throws IndexOutOfBoundsException
	{
		return columns.get(column);
	}

	/**
	 * Get column index by name
	 *
	 * @param name Column name
	 * @return 0-based column index or -1 if column with given name does not exist
	 */
	public int getColumnIndex(final String name)
	{
		return columns.indexOf(name);
	}

	/**
	 * Get cell value at given row and column
	 *
	 * @param row Row index (zero-based)
	 * @param column Column index (zero-based)
	 * @return Data from given cell
	 * @throws IndexOutOfBoundsException if column index is out of range (column < 0 || column >= getColumnCount())
	 *         or row index is out of range (row < 0 || row >= getRowCount())
	 */
	public String getCell(final int row, final int column) throws IndexOutOfBoundsException
	{
		final List<String> rowData = data.get(row);
		return rowData.get(column);
	}

	/**
	 * Get row.
	 *
	 * @param row Row index (zero-based)
	 * @return List of all values for given row
	 * @throws IndexOutOfBoundsException if row index is out of range (row < 0 || row >= getRowCount())
	 */
	public List<String> getRow(final int row) throws IndexOutOfBoundsException
	{
		return data.get(row);
	}

	/* (non-Javadoc)
	 * @see java.lang.Object#toString()
	 */
	@Override
	public String toString()
	{
		final StringBuilder sb = new StringBuilder();
		sb.append("Table");
		sb.append("{columns=").append(columns);
		sb.append(", data=").append(data);
		sb.append('}');
		return sb.toString();
	}
}
