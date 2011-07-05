This directory contains Microsoft Visual Studio project files for including v8
in a Visual Studio/Visual C++ Express solution. All these project files have
been created for use with Microsoft Visual Studio 2005. They can however also
be used in both Visual Studio 2008 and Visual C++ 2008 Express Edition. When
using the project files in the 2008 editions minor upgrades to the files will
be performed by Visual Studio.

v8_base.vcproj
--------------
Base V8 library containing all the V8 code but no JavaScript library code.

v8.vcproj
---------
V8 library containing all the V8 and JavaScript library code embedded as source
which is compiled as V8 is running.

v8_mksnapshot.vcproj
--------------------
Executable v8_mksnapshot.exe for building a heap snapshot from a running V8.

v8_snapshot_cc.vcproj
---------------------
Uses v8_mksnapshot.exe to generate snapshot.cc, which is used in
v8_snapshot.vcproj.

v8_snapshot.vcproj
------------------
V8 library containing all the V8 and JavaScript library code embedded as a heap
snapshot instead of source to be compiled as V8 is running. Using this library
provides significantly faster startup time than v8.vcproj.

The property sheets common.vsprops, debug.vsprops and release.vsprops contains
most of the configuration options and are inhireted by the project files
described above. The location of the output directory used are defined in
common.vsprops.

With regard to Platform SDK version V8 has no specific requriments and builds
with either what is supplied with Visual Studio 2005 or the latest Platform SDK
from Microsoft.

When adding these projects to a solution the following dependencies needs to be
in place:

  v8.vcproj depends on v8_base.vcproj
  v8_mksnapshot.vcproj depends on v8.vcproj
  v8_snapshot_cc.vcproj depends on v8_mksnapshot.vcproj
  v8_snapshot.vcproj depends on v8_snapshot_cc.vcproj and v8_base.vcproj

A project which uses V8 should then depend on v8_snapshot.vcproj.

If V8 without snapshot if preferred only v8_base.vcproj and v8.vcproj are
required and a project which uses V8 should depend on v8.vcproj.

Two sample project files are available as well. These are v8_shell_sample.vcproj
for building the sample in samples\shell.cc and v8_process_sample.vcproj for
building the sample in samples\process.cc. Add either of these (or both) to a
solution with v8_base, v8, v8_mksnapshot and v8_snapshot set up as described
solution with v8_base, v8, v8_mksnapshot and v8_snapshot set up as described
above and have them depend on v8_snapshot.

Finally a sample Visual Studio solution file for is provided. This solution file
includes the two sample projects together with the V8 projects and with the
dependencies configured as described above.

Python requirements
-------------------
When using the Microsoft Visual Studio project files Python version 2.4 or later
is required. Make sure that python.exe is on the path before running Visual
Studio. The use of Python is in the command script js2c.cmd which is used in the
Custom Build Step for v8natives.js in the v8.vcproj project.
