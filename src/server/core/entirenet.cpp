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
** $module: entirenet.cpp
**
**/

#include "nms_core.h"


//
// Network class default constructor
//

Network::Network()
        :NetObj()
{
   m_dwId = 1;
   strcpy(m_szName, "Entire Network");
}


//
// Network class destructor
//

Network::~Network()
{
}


//
// Save object to database
//

BOOL Network::SaveToDB(void)
{
   Lock();

   // Save access list
   SaveACLToDB();

   // Unlock object and clear modification flag
   Unlock();
   m_bIsModified = FALSE;
   return TRUE;
}


//
// Load properties from database
//

void Network::LoadFromDB(void)
{
   Lock();
   LoadACLFromDB();
   Unlock();
}
