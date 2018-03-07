/*
** NetXMS - Network Management System
** Copyright (C) 2003-2018 Raden Solutions
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
** File: manager.cpp
**
**/

#include "libnxpython.h"
#include <nxqueue.h>

#define DEBUG_TAG _T("python")

/**
 * "netxms" module initialization function
 */
PyObject *PyInit_netxms();

/**
 * Extra modules
 */
static NXPYTHON_MODULE_INFO *s_extraModules = NULL;

/**
 * Manager request queue
 */
static Queue s_managerRequestQueue;

/**
 * Manager request
 */
struct PythonManagerRequest
{
   int command;
   PyThreadState *interpreter;
   CONDITION condition;

   PythonManagerRequest()
   {
      command = 0;   // create interpreter
      interpreter = NULL;
      condition = ConditionCreate(true);
   }

   PythonManagerRequest(PyThreadState *pi)
   {
      command = 1;   // destroy interpreter
      interpreter = pi;
      condition = ConditionCreate(true);
   }

   ~PythonManagerRequest()
   {
      ConditionDestroy(condition);
   }

   void wait()
   {
      ConditionWait(condition, INFINITE);
   }

   void notify()
   {
      ConditionSet(condition);
   }
};

/**
 * Manager thread
 */
static THREAD_RESULT THREAD_CALL PythonManager(void *arg)
{
   PyImport_AppendInittab("netxms", PyInit_netxms);
   if (s_extraModules != NULL)
   {
      for(int i = 0; s_extraModules[i].name != NULL; i++)
      {
         PyImport_AppendInittab(s_extraModules[i].name, s_extraModules[i].initFunction);
      }
   }
   Py_InitializeEx(0);
   char version[128];
   strlcpy(version, Py_GetVersion(), 128);
   char *p = strchr(version, ' ');
   if (p != NULL)
      *p = 0;

   PyEval_InitThreads();
   PyThreadState *mainPyThread = PyEval_SaveThread();
   ConditionSet(static_cast<CONDITION>(arg));
   nxlog_debug_tag(DEBUG_TAG, 1, _T("Embedded Python interpreter (version %hs) initialized"), version);

   while(true)
   {
      PythonManagerRequest *request = static_cast<PythonManagerRequest*>(s_managerRequestQueue.getOrBlock());
      if (request == INVALID_POINTER_VALUE)
         break;

      PyEval_AcquireThread(mainPyThread);
      switch(request->command)
      {
         case 0:  // create interpreter
            request->interpreter = Py_NewInterpreter();
            if (request->interpreter != NULL)
            {
               PyThreadState_Swap(mainPyThread);
               nxlog_debug_tag(DEBUG_TAG, 6, _T("Created Python interpreter instance %p"), request->interpreter);
            }
            else
            {
               nxlog_debug_tag(DEBUG_TAG, 6, _T("Cannot create Python interpreter instance"));
            }
            break;
         case 1:  // destroy interpreter
            PyThreadState_Swap(request->interpreter);
            Py_EndInterpreter(request->interpreter);
            PyThreadState_Swap(mainPyThread);
            nxlog_debug_tag(DEBUG_TAG, 6, _T("Destroyed Python interpreter instance %p"), request->interpreter);
            break;
      }
      PyEval_ReleaseThread(mainPyThread);
      request->notify();
   }

   PyEval_RestoreThread(mainPyThread);
   Py_Finalize();
   nxlog_debug_tag(DEBUG_TAG, 1, _T("Embedded Python interpreter terminated"));
   return THREAD_OK;
}

/**
 * Create Python sub-interpreter
 */
PythonInterpreter *PythonInterpreter::create()
{
   PythonManagerRequest r;
   s_managerRequestQueue.put(&r);
   r.wait();
   return (r.interpreter != NULL) ? new PythonInterpreter(r.interpreter) : NULL;
}

/**
 * Internal constructor
 */
PythonInterpreter::PythonInterpreter(PyThreadState *s)
{
   m_threadState = s;
   m_mainModule = NULL;
}

/**
 * Destructor
 */
PythonInterpreter::~PythonInterpreter()
{
   if (m_mainModule != NULL)
   {
      PyEval_AcquireThread(m_threadState);
      Py_DECREF(m_mainModule);
      m_mainModule = NULL;
      PyEval_ReleaseThread(m_threadState);
   }
   PythonManagerRequest r(m_threadState);
   s_managerRequestQueue.put(&r);
   r.wait();
}

/**
 * Log execution error
 */
void PythonInterpreter::logExecutionError(const TCHAR *format)
{
   PyObject *type, *value, *traceback;
   PyErr_Fetch(&type, &value, &traceback);
#ifdef UNICODE
   nxlog_debug_tag(DEBUG_TAG, 6, format, (value != NULL) ? PyUnicode_AsUnicode(value) : L"error text not provided");
#else
   nxlog_debug_tag(DEBUG_TAG, 6, format, (value != NULL) ? PyUnicode_AsUTF8(value) : "error text not provided");
#endif
   if (type != NULL)
      Py_DECREF(type);
   if (value != NULL)
      Py_DECREF(value);
   if (traceback != NULL)
      Py_DECREF(traceback);
}

/**
 * Execute script from given source
 */
bool PythonInterpreter::execute(const char *source)
{
   bool success = false;
   PyEval_AcquireThread(m_threadState);

   if (m_mainModule != NULL)
   {
      Py_DECREF(m_mainModule);
      m_mainModule = NULL;
   }

   PyObject *code = Py_CompileString(source, "memory", Py_file_input);
   if (code != NULL)
   {
      m_mainModule = PyImport_ExecCodeModule("__main__", code);
      if (m_mainModule == NULL)
      {
         logExecutionError(_T("Cannot create __main__ module (%s)"));
      }
      Py_DECREF(code);
   }
   else
   {
      logExecutionError(_T("Source compilation failed (%s)"));
   }

   PyEval_ReleaseThread(m_threadState);
   return success;
}

/**
 * Call function within main module
 */
PyObject *PythonInterpreter::call(const char *name, PyObject *args)
{
   if (m_mainModule == NULL)
      return NULL;

   PyObject *result = NULL;
   PyEval_AcquireThread(m_threadState);

   PyObject *func = PyObject_GetAttrString(m_mainModule, name);
   if ((func != NULL) && PyCallable_Check(func))
   {
      result = PyObject_CallObject(func, args);
      if (result == NULL)
      {
         logExecutionError(_T("Function call failed (%s)"));
      }
   }
   else
   {
      nxlog_debug_tag(DEBUG_TAG, 6, _T("Function %hs does not exist"), name);
   }

   PyEval_ReleaseThread(m_threadState);
   return result;
}

/**
 * Manager thread handle
 */
static THREAD s_managerThread = INVALID_THREAD_HANDLE;

/**
 * Initialize embedded Python
 */
void LIBNXPYTHON_EXPORTABLE InitializeEmbeddedPython(NXPYTHON_MODULE_INFO *modules)
{
   s_extraModules = modules;
   CONDITION initCompleted = ConditionCreate(true);
   s_managerThread = ThreadCreateEx(PythonManager, 0, initCompleted);
   ConditionWait(initCompleted, INFINITE);
   ConditionDestroy(initCompleted);
}

/**
 * Shutdown embedded Python
 */
void LIBNXPYTHON_EXPORTABLE ShutdownEmbeddedPython()
{
   s_managerRequestQueue.put(INVALID_POINTER_VALUE);
   ThreadJoin(s_managerThread);
}

/**
 * Create Python interpreter
 */
PythonInterpreter LIBNXPYTHON_EXPORTABLE *CreatePythonInterpreter()
{
   return PythonInterpreter::create();
}