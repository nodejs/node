Summary
-------
This directory contains ASM implementations of the functions
longest_match() and inflate_fast(), for 64 bits x86 (both AMD64 and Intel EM64t),
for use with Microsoft Macro Assembler (x64) for AMD64 and Microsoft C++ 64 bits.

gvmat64.asm is written by Gilles Vollant (2005), by using Brian Raiter 686/32 bits
   assembly optimized version from Jean-loup Gailly original longest_match function

inffasx64.asm and inffas8664.c were written by Chris Anderson, by optimizing
   original function from Mark Adler

Use instructions
----------------
Assemble the .asm files using MASM and put the object files into the zlib source
directory.  You can also get object files here:

     http://www.winimage.com/zLibDll/zlib124_masm_obj.zip

define ASMV and ASMINF in your project. Include inffas8664.c in your source tree,
and inffasx64.obj and gvmat64.obj as object to link.


Build instructions
------------------
run bld_64.bat with Microsoft Macro Assembler (x64) for AMD64 (ml64.exe)

ml64.exe is given with Visual Studio 2005, Windows 2003 server DDK

You can get Windows 2003 server DDK with ml64 and cl for AMD64 from
  http://www.microsoft.com/whdc/devtools/ddk/default.mspx for low price)
