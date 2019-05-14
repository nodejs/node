                        ZLib for Ada thick binding (ZLib.Ada)
                        Release 1.3

ZLib.Ada is a thick binding interface to the popular ZLib data
compression library, available at http://www.gzip.org/zlib/.
It provides Ada-style access to the ZLib C library.


        Here are the main changes since ZLib.Ada 1.2:

- Attension: ZLib.Read generic routine have a initialization requirement
  for Read_Last parameter now. It is a bit incompartible with previous version,
  but extends functionality, we could use new parameters Allow_Read_Some and
  Flush now.

- Added Is_Open routines to ZLib and ZLib.Streams packages.

- Add pragma Assert to check Stream_Element is 8 bit.

- Fix extraction to buffer with exact known decompressed size. Error reported by
  Steve Sangwine.

- Fix definition of ULong (changed to unsigned_long), fix regression on 64 bits
  computers. Patch provided by Pascal Obry.

- Add Status_Error exception definition.

- Add pragma Assertion that Ada.Streams.Stream_Element size is 8 bit.


        How to build ZLib.Ada under GNAT

You should have the ZLib library already build on your computer, before
building ZLib.Ada. Make the directory of ZLib.Ada sources current and
issue the command:

  gnatmake test -largs -L<directory where libz.a is> -lz

Or use the GNAT project file build for GNAT 3.15 or later:

  gnatmake -Pzlib.gpr -L<directory where libz.a is>


        How to build ZLib.Ada under Aonix ObjectAda for Win32 7.2.2

1. Make a project with all *.ads and *.adb files from the distribution.
2. Build the libz.a library from the ZLib C sources.
3. Rename libz.a to z.lib.
4. Add the library z.lib to the project.
5. Add the libc.lib library from the ObjectAda distribution to the project.
6. Build the executable using test.adb as a main procedure.


        How to use ZLib.Ada

The source files test.adb and read.adb are small demo programs that show
the main functionality of ZLib.Ada.

The routines from the package specifications are commented.


Homepage: http://zlib-ada.sourceforge.net/
Author: Dmitriy Anisimkov <anisimkov@yahoo.com>

Contributors: Pascal Obry <pascal@obry.org>, Steve Sangwine <sjs@essex.ac.uk>
