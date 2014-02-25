----------------------------------------------------------------------------
-- LuaJIT compiler dump module.
--
-- Copyright (C) 2005-2013 Mike Pall. All rights reserved.
-- Released under the MIT license. See Copyright Notice in luajit.h
----------------------------------------------------------------------------
--
-- This module can be used to debug the JIT compiler itself. It dumps the
-- code representations and structures used in various compiler stages.
--
-- Example usage:
--
--   luajit -jdump -e "local x=0; for i=1,1e6 do x=x+i end; print(x)"
--   luajit -jdump=im -e "for i=1,1000 do for j=1,1000 do end end" | less -R
--   luajit -jdump=is myapp.lua | less -R
--   luajit -jdump=-b myapp.lua
--   luajit -jdump=+aH,myapp.html myapp.lua
--   luajit -jdump=ixT,myapp.dump myapp.lua
--
-- The first argument specifies the dump mode. The second argument gives
-- the output file name. Default output is to stdout, unless the environment
-- variable LUAJIT_DUMPFILE is set. The file is overwritten every time the
-- module is started.
--
-- Different features can be turned on or off with the dump mode. If the
-- mode starts with a '+', the following features are added to the default
-- set of features; a '-' removes them. Otherwise the features are replaced.
--
-- The following dump features are available (* marks the default):
--
--  * t  Print a line for each started, ended or aborted trace (see also -jv).
--  * b  Dump the traced bytecode.
--  * i  Dump the IR (intermediate representation).
--    r  Augment the IR with register/stack slots.
--    s  Dump the snapshot map.
--  * m  Dump the generated machine code.
--    x  Print each taken trace exit.
--    X  Print each taken trace exit and the contents of all registers.
--
-- The output format can be set with the following characters:
--
--    T  Plain text output.
--    A  ANSI-colored text output
--    H  Colorized HTML + CSS output.
--
-- The default output format is plain text. It's set to ANSI-colored text
-- if the COLORTERM variable is set. Note: this is independent of any output
-- redirection, which is actually considered a feature.
--
-- You probably want to use less -R to enjoy viewing ANSI-colored text from
-- a pipe or a file. Add this to your ~/.bashrc: export LESS="-R"
--
------------------------------------------------------------------------------

-- Cache some library functions and objects.
local jit = require("jit")
assert(jit.version_num == 20002, "LuaJIT core/library version mismatch")
local jutil = require("jit.util")
local vmdef = require("jit.vmdef")
local funcinfo, funcbc = jutil.funcinfo, jutil.funcbc
local traceinfo, traceir, tracek = jutil.traceinfo, jutil.traceir, jutil.tracek
local tracemc, tracesnap = jutil.tracemc, jutil.tracesnap
local traceexitstub, ircalladdr = jutil.traceexitstub, jutil.ircalladdr
local bit = require("bit")
local band, shl, shr = bit.band, bit.lshift, bit.rshift
local sub, gsub, format = string.sub, string.gsub, string.format
local byte, char, rep = string.byte, string.char, string.rep
local type, tostring = type, tostring
local stdout, stderr = io.stdout, io.stderr

-- Load other modules on-demand.
local bcline, disass

-- Active flag, output file handle and dump mode.
local active, out, dumpmode

------------------------------------------------------------------------------

local symtabmt = { __index = false }
local symtab = {}
local nexitsym = 0

-- Fill nested symbol table with per-trace exit stub addresses.
local function fillsymtab_tr(tr, nexit)
  local t = {}
  symtabmt.__index = t
  if jit.arch == "mips" or jit.arch == "mipsel" then
    t[traceexitstub(tr, 0)] = "exit"
    return
  end
  for i=0,nexit-1 do
    local addr = traceexitstub(tr, i)
    t[addr] = tostring(i)
  end
  local addr = traceexitstub(tr, nexit)
  if addr then t[addr] = "stack_check" end
end

-- Fill symbol table with trace exit stub addresses.
local function fillsymtab(tr, nexit)
  local t = symtab
  if nexitsym == 0 then
    local ircall = vmdef.ircall
    for i=0,#ircall do
      local addr = ircalladdr(i)
      if addr ~= 0 then t[addr] = ircall[i] end
    end
  end
  if nexitsym == 1000000 then -- Per-trace exit stubs.
    fillsymtab_tr(tr, nexit)
  elseif nexit > nexitsym then -- Shared exit stubs.
    for i=nexitsym,nexit-1 do
      local addr = traceexitstub(i)
      if addr == nil then -- Fall back to per-trace exit stubs.
	fillsymtab_tr(tr, nexit)
	setmetatable(symtab, symtabmt)
	nexit = 1000000
	break
      end
      t[addr] = tostring(i)
    end
    nexitsym = nexit
  end
  return t
end

local function dumpwrite(s)
  out:write(s)
end

-- Disassemble machine code.
local function dump_mcode(tr)
  local info = traceinfo(tr)
  if not info then return end
  local mcode, addr, loop = tracemc(tr)
  if not mcode then return end
  if not disass then disass = require("jit.dis_"..jit.arch) end
  out:write("---- TRACE ", tr, " mcode ", #mcode, "\n")
  local ctx = disass.create(mcode, addr, dumpwrite)
  ctx.hexdump = 0
  ctx.symtab = fillsymtab(tr, info.nexit)
  if loop ~= 0 then
    symtab[addr+loop] = "LOOP"
    ctx:disass(0, loop)
    out:write("->LOOP:\n")
    ctx:disass(loop, #mcode-loop)
    symtab[addr+loop] = nil
  else
    ctx:disass(0, #mcode)
  end
end

------------------------------------------------------------------------------

local irtype_text = {
  [0] = "nil",
  "fal",
  "tru",
  "lud",
  "str",
  "p32",
  "thr",
  "pro",
  "fun",
  "p64",
  "cdt",
  "tab",
  "udt",
  "flt",
  "num",
  "i8 ",
  "u8 ",
  "i16",
  "u16",
  "int",
  "u32",
  "i64",
  "u64",
  "sfp",
}

local colortype_ansi = {
  [0] = "%s",
  "%s",
  "%s",
  "\027[36m%s\027[m",
  "\027[32m%s\027[m",
  "%s",
  "\027[1m%s\027[m",
  "%s",
  "\027[1m%s\027[m",
  "%s",
  "\027[33m%s\027[m",
  "\027[31m%s\027[m",
  "\027[36m%s\027[m",
  "\027[34m%s\027[m",
  "\027[34m%s\027[m",
  "\027[35m%s\027[m",
  "\027[35m%s\027[m",
  "\027[35m%s\027[m",
  "\027[35m%s\027[m",
  "\027[35m%s\027[m",
  "\027[35m%s\027[m",
  "\027[35m%s\027[m",
  "\027[35m%s\027[m",
  "\027[35m%s\027[m",
}

local function colorize_text(s, t)
  return s
end

local function colorize_ansi(s, t)
  return format(colortype_ansi[t], s)
end

local irtype_ansi = setmetatable({},
  { __index = function(tab, t)
      local s = colorize_ansi(irtype_text[t], t); tab[t] = s; return s; end })

local html_escape = { ["<"] = "&lt;", [">"] = "&gt;", ["&"] = "&amp;", }

local function colorize_html(s, t)
  s = gsub(s, "[<>&]", html_escape)
  return format('<span class="irt_%s">%s</span>', irtype_text[t], s)
end

local irtype_html = setmetatable({},
  { __index = function(tab, t)
      local s = colorize_html(irtype_text[t], t); tab[t] = s; return s; end })

local header_html = [[
<style type="text/css">
background { background: #ffffff; color: #000000; }
pre.ljdump {
font-size: 10pt;
background: #f0f4ff;
color: #000000;
border: 1px solid #bfcfff;
padding: 0.5em;
margin-left: 2em;
margin-right: 2em;
}
span.irt_str { color: #00a000; }
span.irt_thr, span.irt_fun { color: #404040; font-weight: bold; }
span.irt_tab { color: #c00000; }
span.irt_udt, span.irt_lud { color: #00c0c0; }
span.irt_num { color: #4040c0; }
span.irt_int, span.irt_i8, span.irt_u8, span.irt_i16, span.irt_u16 { color: #b040b0; }
</style>
]]

local colorize, irtype

-- Lookup tables to convert some literals into names.
local litname = {
  ["SLOAD "] = setmetatable({}, { __index = function(t, mode)
    local s = ""
    if band(mode, 1) ~= 0 then s = s.."P" end
    if band(mode, 2) ~= 0 then s = s.."F" end
    if band(mode, 4) ~= 0 then s = s.."T" end
    if band(mode, 8) ~= 0 then s = s.."C" end
    if band(mode, 16) ~= 0 then s = s.."R" end
    if band(mode, 32) ~= 0 then s = s.."I" end
    t[mode] = s
    return s
  end}),
  ["XLOAD "] = { [0] = "", "R", "V", "RV", "U", "RU", "VU", "RVU", },
  ["CONV  "] = setmetatable({}, { __index = function(t, mode)
    local s = irtype[band(mode, 31)]
    s = irtype[band(shr(mode, 5), 31)].."."..s
    if band(mode, 0x400) ~= 0 then s = s.." trunc"
    elseif band(mode, 0x800) ~= 0 then s = s.." sext" end
    local c = shr(mode, 14)
    if c == 2 then s = s.." index" elseif c == 3 then s = s.." check" end
    t[mode] = s
    return s
  end}),
  ["FLOAD "] = vmdef.irfield,
  ["FREF  "] = vmdef.irfield,
  ["FPMATH"] = vmdef.irfpm,
}

local function ctlsub(c)
  if c == "\n" then return "\\n"
  elseif c == "\r" then return "\\r"
  elseif c == "\t" then return "\\t"
  else return format("\\%03d", byte(c))
  end
end

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

local function formatk(tr, idx)
  local k, t, slot = tracek(tr, idx)
  local tn = type(k)
  local s
  if tn == "number" then
    if k == 2^52+2^51 then
      s = "bias"
    else
      s = format("%+.14g", k)
    end
  elseif tn == "string" then
    s = format(#k > 20 and '"%.20s"~' or '"%s"', gsub(k, "%c", ctlsub))
  elseif tn == "function" then
    s = fmtfunc(k)
  elseif tn == "table" then
    s = format("{%p}", k)
  elseif tn == "userdata" then
    if t == 12 then
      s = format("userdata:%p", k)
    else
      s = format("[%p]", k)
      if s == "[0x00000000]" then s = "NULL" end
    end
  elseif t == 21 then -- int64_t
    s = sub(tostring(k), 1, -3)
    if sub(s, 1, 1) ~= "-" then s = "+"..s end
  else
    s = tostring(k) -- For primitives.
  end
  s = colorize(format("%-4s", s), t)
  if slot then
    s = format("%s @%d", s, slot)
  end
  return s
end

local function printsnap(tr, snap)
  local n = 2
  for s=0,snap[1]-1 do
    local sn = snap[n]
    if shr(sn, 24) == s then
      n = n + 1
      local ref = band(sn, 0xffff) - 0x8000 -- REF_BIAS
      if ref < 0 then
	out:write(formatk(tr, ref))
      elseif band(sn, 0x80000) ~= 0 then -- SNAP_SOFTFPNUM
	out:write(colorize(format("%04d/%04d", ref, ref+1), 14))
      else
	local m, ot, op1, op2 = traceir(tr, ref)
	out:write(colorize(format("%04d", ref), band(ot, 31)))
      end
      out:write(band(sn, 0x10000) == 0 and " " or "|") -- SNAP_FRAME
    else
      out:write("---- ")
    end
  end
  out:write("]\n")
end

-- Dump snapshots (not interleaved with IR).
local function dump_snap(tr)
  out:write("---- TRACE ", tr, " snapshots\n")
  for i=0,1000000000 do
    local snap = tracesnap(tr, i)
    if not snap then break end
    out:write(format("#%-3d %04d [ ", i, snap[0]))
    printsnap(tr, snap)
  end
end

-- Return a register name or stack slot for a rid/sp location.
local function ridsp_name(ridsp, ins)
  if not disass then disass = require("jit.dis_"..jit.arch) end
  local rid, slot = band(ridsp, 0xff), shr(ridsp, 8)
  if rid == 253 or rid == 254 then
    return (slot == 0 or slot == 255) and " {sink" or format(" {%04d", ins-slot)
  end
  if ridsp > 255 then return format("[%x]", slot*4) end
  if rid < 128 then return disass.regname(rid) end
  return ""
end

-- Dump CALL* function ref and return optional ctype.
local function dumpcallfunc(tr, ins)
  local ctype
  if ins > 0 then
    local m, ot, op1, op2 = traceir(tr, ins)
    if band(ot, 31) == 0 then -- nil type means CARG(func, ctype).
      ins = op1
      ctype = formatk(tr, op2)
    end
  end
  if ins < 0 then
    out:write(format("[0x%x](", tonumber((tracek(tr, ins)))))
  else
    out:write(format("%04d (", ins))
  end
  return ctype
end

-- Recursively gather CALL* args and dump them.
local function dumpcallargs(tr, ins)
  if ins < 0 then
    out:write(formatk(tr, ins))
  else
    local m, ot, op1, op2 = traceir(tr, ins)
    local oidx = 6*shr(ot, 8)
    local op = sub(vmdef.irnames, oidx+1, oidx+6)
    if op == "CARG  " then
      dumpcallargs(tr, op1)
      if op2 < 0 then
	out:write(" ", formatk(tr, op2))
      else
	out:write(" ", format("%04d", op2))
      end
    else
      out:write(format("%04d", ins))
    end
  end
end

-- Dump IR and interleaved snapshots.
local function dump_ir(tr, dumpsnap, dumpreg)
  local info = traceinfo(tr)
  if not info then return end
  local nins = info.nins
  out:write("---- TRACE ", tr, " IR\n")
  local irnames = vmdef.irnames
  local snapref = 65536
  local snap, snapno
  if dumpsnap then
    snap = tracesnap(tr, 0)
    snapref = snap[0]
    snapno = 0
  end
  for ins=1,nins do
    if ins >= snapref then
      if dumpreg then
	out:write(format("....              SNAP   #%-3d [ ", snapno))
      else
	out:write(format("....        SNAP   #%-3d [ ", snapno))
      end
      printsnap(tr, snap)
      snapno = snapno + 1
      snap = tracesnap(tr, snapno)
      snapref = snap and snap[0] or 65536
    end
    local m, ot, op1, op2, ridsp = traceir(tr, ins)
    local oidx, t = 6*shr(ot, 8), band(ot, 31)
    local op = sub(irnames, oidx+1, oidx+6)
    if op == "LOOP  " then
      if dumpreg then
	out:write(format("%04d ------------ LOOP ------------\n", ins))
      else
	out:write(format("%04d ------ LOOP ------------\n", ins))
      end
    elseif op ~= "NOP   " and op ~= "CARG  " and
	   (dumpreg or op ~= "RENAME") then
      local rid = band(ridsp, 255)
      if dumpreg then
	out:write(format("%04d %-6s", ins, ridsp_name(ridsp, ins)))
      else
	out:write(format("%04d ", ins))
      end
      out:write(format("%s%s %s %s ",
		       (rid == 254 or rid == 253) and "}" or
		       (band(ot, 128) == 0 and " " or ">"),
		       band(ot, 64) == 0 and " " or "+",
		       irtype[t], op))
      local m1, m2 = band(m, 3), band(m, 3*4)
      if sub(op, 1, 4) == "CALL" then
	local ctype
	if m2 == 1*4 then -- op2 == IRMlit
	  out:write(format("%-10s  (", vmdef.ircall[op2]))
	else
	  ctype = dumpcallfunc(tr, op2)
	end
	if op1 ~= -1 then dumpcallargs(tr, op1) end
	out:write(")")
	if ctype then out:write(" ctype ", ctype) end
      elseif op == "CNEW  " and op2 == -1 then
	out:write(formatk(tr, op1))
      elseif m1 ~= 3 then -- op1 != IRMnone
	if op1 < 0 then
	  out:write(formatk(tr, op1))
	else
	  out:write(format(m1 == 0 and "%04d" or "#%-3d", op1))
	end
	if m2 ~= 3*4 then -- op2 != IRMnone
	  if m2 == 1*4 then -- op2 == IRMlit
	    local litn = litname[op]
	    if litn and litn[op2] then
	      out:write("  ", litn[op2])
	    elseif op == "UREFO " or op == "UREFC " then
	      out:write(format("  #%-3d", shr(op2, 8)))
	    else
	      out:write(format("  #%-3d", op2))
	    end
	  elseif op2 < 0 then
	    out:write("  ", formatk(tr, op2))
	  else
	    out:write(format("  %04d", op2))
	  end
	end
      end
      out:write("\n")
    end
  end
  if snap then
    if dumpreg then
      out:write(format("....              SNAP   #%-3d [ ", snapno))
    else
      out:write(format("....        SNAP   #%-3d [ ", snapno))
    end
    printsnap(tr, snap)
  end
end

------------------------------------------------------------------------------

local recprefix = ""
local recdepth = 0

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
  if what == "stop" or (what == "abort" and dumpmode.a) then
    if dumpmode.i then dump_ir(tr, dumpmode.s, dumpmode.r and what == "stop")
    elseif dumpmode.s then dump_snap(tr) end
    if dumpmode.m then dump_mcode(tr) end
  end
  if what == "start" then
    if dumpmode.H then out:write('<pre class="ljdump">\n') end
    out:write("---- TRACE ", tr, " ", what)
    if otr then out:write(" ", otr, "/", oex) end
    out:write(" ", fmtfunc(func, pc), "\n")
    recprefix = ""
  elseif what == "stop" or what == "abort" then
    out:write("---- TRACE ", tr, " ", what)
    recprefix = nil
    if what == "abort" then
      out:write(" ", fmtfunc(func, pc), " -- ", fmterr(otr, oex), "\n")
    else
      local info = traceinfo(tr)
      local link, ltype = info.link, info.linktype
      if link == tr or link == 0 then
	out:write(" -> ", ltype, "\n")
      elseif ltype == "root" then
	out:write(" -> ", link, "\n")
      else
	out:write(" -> ", link, " ", ltype, "\n")
      end
    end
    if dumpmode.H then out:write("</pre>\n\n") else out:write("\n") end
  else
    out:write("---- TRACE ", what, "\n\n")
  end
  out:flush()
end

-- Dump recorded bytecode.
local function dump_record(tr, func, pc, depth, callee)
  if depth ~= recdepth then
    recdepth = depth
    recprefix = rep(" .", depth)
  end
  local line
  if pc >= 0 then
    line = bcline(func, pc, recprefix)
    if dumpmode.H then line = gsub(line, "[<>&]", html_escape) end
  else
    line = "0000 "..recprefix.." FUNCC      \n"
    callee = func
  end
  if pc <= 0 then
    out:write(sub(line, 1, -2), "         ; ", fmtfunc(func), "\n")
  else
    out:write(line)
  end
  if pc >= 0 and band(funcbc(func, pc), 0xff) < 16 then -- ORDER BC
    out:write(bcline(func, pc+1, recprefix)) -- Write JMP for cond.
  end
end

------------------------------------------------------------------------------

-- Dump taken trace exits.
local function dump_texit(tr, ex, ngpr, nfpr, ...)
  out:write("---- TRACE ", tr, " exit ", ex, "\n")
  if dumpmode.X then
    local regs = {...}
    if jit.arch == "x64" then
      for i=1,ngpr do
	out:write(format(" %016x", regs[i]))
	if i % 4 == 0 then out:write("\n") end
      end
    else
      for i=1,ngpr do
	out:write(format(" %08x", regs[i]))
	if i % 8 == 0 then out:write("\n") end
      end
    end
    if jit.arch == "mips" or jit.arch == "mipsel" then
      for i=1,nfpr,2 do
	out:write(format(" %+17.14g", regs[ngpr+i]))
	if i % 8 == 7 then out:write("\n") end
      end
    else
      for i=1,nfpr do
	out:write(format(" %+17.14g", regs[ngpr+i]))
	if i % 4 == 0 then out:write("\n") end
      end
    end
  end
end

------------------------------------------------------------------------------

-- Detach dump handlers.
local function dumpoff()
  if active then
    active = false
    jit.attach(dump_texit)
    jit.attach(dump_record)
    jit.attach(dump_trace)
    if out and out ~= stdout and out ~= stderr then out:close() end
    out = nil
  end
end

-- Open the output file and attach dump handlers.
local function dumpon(opt, outfile)
  if active then dumpoff() end

  local colormode = os.getenv("COLORTERM") and "A" or "T"
  if opt then
    opt = gsub(opt, "[TAH]", function(mode) colormode = mode; return ""; end)
  end

  local m = { t=true, b=true, i=true, m=true, }
  if opt and opt ~= "" then
    local o = sub(opt, 1, 1)
    if o ~= "+" and o ~= "-" then m = {} end
    for i=1,#opt do m[sub(opt, i, i)] = (o ~= "-") end
  end
  dumpmode = m

  if m.t or m.b or m.i or m.s or m.m then
    jit.attach(dump_trace, "trace")
  end
  if m.b then
    jit.attach(dump_record, "record")
    if not bcline then bcline = require("jit.bc").line end
  end
  if m.x or m.X then
    jit.attach(dump_texit, "texit")
  end

  if not outfile then outfile = os.getenv("LUAJIT_DUMPFILE") end
  if outfile then
    out = outfile == "-" and stdout or assert(io.open(outfile, "w"))
  else
    out = stdout
  end

  m[colormode] = true
  if colormode == "A" then
    colorize = colorize_ansi
    irtype = irtype_ansi
  elseif colormode == "H" then
    colorize = colorize_html
    irtype = irtype_html
    out:write(header_html)
  else
    colorize = colorize_text
    irtype = irtype_text
  end

  active = true
end

-- Public module functions.
module(...)

on = dumpon
off = dumpoff
start = dumpon -- For -j command line option.

