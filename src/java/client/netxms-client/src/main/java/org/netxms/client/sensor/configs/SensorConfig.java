/**
 * NetXMS - open source network management system
 * Copyright (C) 2017 Victor Kirhenshtein
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
package org.netxms.client.sensor.configs;

import java.io.StringWriter;
import java.io.Writer;
import org.simpleframework.xml.Root;
import org.simpleframework.xml.Serializer;
import org.simpleframework.xml.core.Persister;

/**
 * Base for all sensor configurations
 * This is class should be extended by any sensor config class
 */
@Root(name="config")
public class SensorConfig
{
   /**
    * Create DCI list object from XML document
    * 
    * @param xml XML document
    * @return deserialized object
    * @throws Exception if the object cannot be fully deserialized
    */
   public static SensorConfig createFromXml(Class<? extends SensorConfig> retClass, final String xml) throws Exception
   {
      Serializer serializer = new Persister();
      return serializer.read(retClass, xml);
   }
   
   /**
    * Create XML from configuration.
    * 
    * @return XML document
    * @throws Exception if the schema for the object is not valid
    */
   public String createXml() throws Exception
   {
      Serializer serializer = new Persister();
      Writer writer = new StringWriter();
      serializer.write(this, writer);
      return writer.toString();
   }
}
