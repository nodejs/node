::-------------------------------------------------------------------------------------------------------
:: Copyright (C) Microsoft. All rights reserved.
:: Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
::-------------------------------------------------------------------------------------------------------

call %*
cd %JSCRIPT_ROOT%
build -cz -dir inetcore\jscript\manifests;inetcore\jscript\lib\author;inetcore\jscript\lib\backend;inetcore\jscript\lib\common;inetcore\jscript\lib\parser;inetcore\jscript\lib\runtime\bytecode;inetcore\jscript\lib\runtime\math;inetcore\jscript\lib\runtime\language;inetcore\jscript\lib\runtime\library;inetcore\jscript\lib\runtime\types;inetcore\jscript\lib\winrt;inetcore\jscript\dll\jscript\test;inetcore\jscript\exe\common;inetcore\jscript\exe\jshost\release
