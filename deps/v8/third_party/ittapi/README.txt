Intel® The Instrumentation and Tracing Technology (ITT) and Just-In-Time (JIT) API
**********************************************************************************

This ITT/JIT open source profiling API includes:

  Instrumentation and Tracing Technology(ITT) API
  Just-In-Time(JIT) Profiling API

The Instrumentation and Tracing Technology (ITT) API enables your application
to generate and control the collection of trace data during its execution 
across different Intel tools.

ITT API consists of two parts: static part and dynamic part. 
Dynamic part is specific for a tool and distributed only with a particular tool.
Static part is a common part shared between tools.
Currently static part of ITT API is distributed as static library and opened 
sources under BSD/GPLv2 dual license with every tool supporting ITT API.  

 Build the library:
     On Windows, Linux and OSX: requires cmake (https://cmake.org) to be set in PATH
         python buildall.py
         Windows: requires Visual Studio installed or requires Ninja (https://github.com/ninja-build/ninja/releases) to be set in PATH

 Load up the library:
     On Windows and Linux:
         Set environment variable INTEL_LIBITTNOTIFY32/INTEL_LIBITTNOTIFY64 to the full path to the libittnotify[32/64].[lib/a]
     On OSX:
         Set environment variable DYLD_INSERT_LIBRARIES to the full path to the libittnotify.dylib
