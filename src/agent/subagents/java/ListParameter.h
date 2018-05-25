/* 
 ** Java-Bridge NetXMS subagent
 ** Copyright (C) 2013 TEMPEST a.s.
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
 ** File: ListParameter.hxx
 **
 **/

#ifndef __ORG_NETXMS_AGENT_LISTPARAMETER__
#define __ORG_NETXMS_AGENT_LISTPARAMETER__

namespace org_netxms_agent
{
   class ListParameter
   {

      private:
         JavaVM * jvm;

      protected:
                                 // cache method id
         jmethodID jstringgetNameID;
                                 // cache method id
         jmethodID jobjectArray_getValuesjstringjava_lang_StringID;

         jobject instance;
         jclass instanceClass;   // cache class

         // Caching (if any)

         /**
          * Get the environment matching to the current thread.
          */
         virtual JNIEnv * getCurrentEnv();

      public:
         // Constructor
         /**
          * Create a wrapping of the object from a JNIEnv.
          * It will call the default constructor
          * @param JEnv_ the Java Env
          */
         ListParameter(JavaVM * jvm_);

         /**
          * Create a wrapping of an already existing object from a JNIEnv.
          * The object must have already been instantiated
          * @param JEnv_ the Java Env
          * @param JObj the object
          */
         ListParameter(JavaVM * jvm_, jobject JObj);

         // Destructor
         ~ListParameter();

         // Generic method
         // Synchronization methods
         /**
          * Enter monitor associated with the object.
          * Equivalent of creating a "synchronized(obj)" scope in Java.
          */
         void synchronize();

         /**
          * Exit monitor associated with the object.
          * Equivalent of ending a "synchronized(obj)" scope.
          */
         void endSynchronize();

         // Methods
         TCHAR* getName();

         TCHAR** getValues(TCHAR const* param, int *lenRow);

         /**
          * Get class name to use for static methods
          * @return class name to use for static methods
          */

         static const char* className()
         {
            return "org/netxms/agent/ListParameter";
         }

   };

}
#endif