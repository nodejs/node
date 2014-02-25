----------------------------------------------------------------------------
-- Verbose mode of the LuaJIT compiler.
--
-- Copyright (C) 2005-2013 Mike Pall. All rights reserved.
-- Released under the MIT license. See Copyright Notice in luajit.h
----------------------------------------------------------------------------
--
-- This module shows verbose information about the progress of the
-- JIT compiler. It prints one line for each generated trace. This module
-- is useful to see which code has been compiled or where the compiler
-- punts and falls back to the interpreter.
--
-- Example usage:
--
--   luajit -jv -e "for i=1,1000 do for j=1,1000 do end end"
--   luajit -jv=myapp.out myapp.lua
--
-- Default output is to stderr. To redirect the output to a file, pass a
-- filename as an argument (use '-' for stdout) or set the environment
-- variable LUAJIT_VERBOSEFILE. The file is overwritten every time the
-- module is started.
--
-- The output from the first example should look like this:
--
-- [TRACE   1 (command line):1 loop]
-- [TRACE   2 (1/3) (command line):1 -> 1]
--
-- The first number in each line is the internal trace number. Next are
-- the file name ('(command line)') and the line number (':1') where the
-- trace has started. Side traces also show the parent trace number and
-- the exit number where they are attached to in parentheses ('(1/3)').
-- An arrow at the end shows where the trace links to ('-> 1'), unless
-- it loops to itself.
--
-- In this case the inner loop gets hot and is traced first, generating
-- a root trace. Then the last exit from the 1st trace gets hot, too,
-- and triggers generation of the 2nd trace. The side trace follows the
-- path along the outer loop and *around* the inner loop, back to its
-- start, and then links to the 1st trace. Yes, this may seem unusual,
-- if you know how traditional compilers work. Trace compilers are full
-- of surprises like this -- have fun! :-)
--
-- Aborted traces are shown like this:
--
-- [TRACE --- foo.lua:44 -- leaving loop in root trace at foo:lua:50]
--
-- Don't worry -- trace aborts are quite common, even in programs which
-- can be fully compiled. The compiler may retry several times until it
-- finds a suitable trace.
--
-- Of course this doesn't work with features that are not-yet-implemented
-- (NYI error messages). The VM simply falls back to the interpreter. This
-- may not matter at all if the particular trace is not very high up in
-- the CPU usage profile. Oh, and the interpreter is quite fast, too.
--
-- Also check out the -jdump module, which prints all the gory details.
--
------------------------------------------------------------------------------

-- Cache some library functions and objects.
local jit = require("jit")
assert(jit.version_num == 20002, "LuaJIT core/library version mismatch")
local jutil = require("jit.util")
local vmdef = require("jit.vmdef")
local funcinfo, traceinfo = jutil.funcinfo, jutil.traceinfo
local type, format = type, string.format
local stdout, stderr = io.stdout, io.stderr

-- Active flag and output file handle.
local active, out

------------------------------------------------------------------------------

local startloc, startex

local function fmtfunc(func, pc)
  local fi = funcinfo(func, pc)
  if fi.loc then
    return fi.loc
  elseif fi.ffid then
    return vmdef.ffnames[fi.ffid]
  elseif fi.addr then
    return format("C:%x", fi.addr)
  else
    return "(?)"
  end
end

-- Format trace error message.
local function fmterr(err, info)
  if type(err) == "number" then
    if type(info) == "function" then info = fmtfunc(info) end
    err = format(vmdef.traceerr[err], info)
  end
  return err
end

-- Dump trace states.
local function dump_trace(what, tr, func, pc, otr, oex)
  if what == "start" then
    startloc = fmtfunc(func, pc)
    startex = otr and "("..otr.."/"..oex..") " or ""
  else
    if what == "abort" then
      local loc = fmtfunc(func, pc)
      if loc ~= startloc then
	out:write(format("[TRACE --- %s%s -- %s at %s]\n",
	  startex, startloc, fmterr(otr, oex), loc))
      else
	out:write(format("[TRACE --- %s%s -- %s]\n",
	  startex, startloc, fmterr(otr, oex)))
      end
    elseif what == "stop" then
      local info = traceinfo(tr)
      local link, ltype = info.link, info.linktype
      if ltype == "interpreter" then
	out:write(format("[TRACE %3s %s%s -- fallback to interpreter]\n",
	  tr, startex, startloc))
      elseif link == tr or link == 0 then
	out:write(format("[TRACE %3s %s%s %s]\n",
	  tr, startex, startloc, ltype))
      elseif ltype == "root" then
	out:write(format("[TRACE %3s %s%s -> %d]\n",
	  tr, startex, startloc, link))
      else
	out:write(format("[TRACE %3s %s%s -> %d %s]\n",
	  tr, startex, startloc, link, ltype))
      end
    else
      out:write(format("[TRACE %s]\n", what))
    end
    out:flush()
  end
end

------------------------------------------------------------------------------

-- Detach dump handlers.
local function dumpoff()
  if active then
    active = false
    jit.attach(dump_trace)
    if out and out ~= stdout and out ~= stderr then out:close() end
    out = nil
  end
end

-- Open the output file and attach dump handlers.
local function dumpon(outfile)
  if active then dumpoff() end
  if not outfile then outfile = os.getenv("LUAJIT_VERBOSEFILE") end
  if outfile then
    out = outfile == "-" and stdout or assert(io.open(outfile, "w"))
  else
    out = stderr
  end
  jit.attach(dump_trace, "trace")
  active = true
end

-- Public module functions.
module(...)

on = dumpon
off = dumpoff
start = dumpon -- For -j command line option.

