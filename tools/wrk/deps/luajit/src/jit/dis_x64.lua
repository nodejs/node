----------------------------------------------------------------------------
-- LuaJIT x64 disassembler wrapper module.
--
-- Copyright (C) 2005-2013 Mike Pall. All rights reserved.
-- Released under the MIT license. See Copyright Notice in luajit.h
----------------------------------------------------------------------------
-- This module just exports the 64 bit functions from the combined
-- x86/x64 disassembler module. All the interesting stuff is there.
------------------------------------------------------------------------------

local require = require

module(...)

local dis_x86 = require(_PACKAGE.."dis_x86")

create = dis_x86.create64
disass = dis_x86.disass64
regname = dis_x86.regname64

