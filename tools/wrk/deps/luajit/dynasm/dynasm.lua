------------------------------------------------------------------------------
-- DynASM. A dynamic assembler for code generation engines.
-- Originally designed and implemented for LuaJIT.
--
-- Copyright (C) 2005-2013 Mike Pall. All rights reserved.
-- See below for full copyright notice.
------------------------------------------------------------------------------

-- Application information.
local _info = {
  name =	"DynASM",
  description =	"A dynamic assembler for code generation engines",
  version =	"1.3.0",
  vernum =	 10300,
  release =	"2011-05-05",
  author =	"Mike Pall",
  url =		"http://luajit.org/dynasm.html",
  license =	"MIT",
  copyright =	[[
Copyright (C) 2005-2013 Mike Pall. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

[ MIT license: http://www.opensource.org/licenses/mit-license.php ]
]],
}

-- Cache library functions.
local type, pairs, ipairs = type, pairs, ipairs
local pcall, error, assert = pcall, error, assert
local _s = string
local sub, match, gmatch, gsub = _s.sub, _s.match, _s.gmatch, _s.gsub
local format, rep, upper = _s.format, _s.rep, _s.upper
local _t = table
local insert, remove, concat, sort = _t.insert, _t.remove, _t.concat, _t.sort
local exit = os.exit
local io = io
local stdin, stdout, stderr = io.stdin, io.stdout, io.stderr

------------------------------------------------------------------------------

-- Program options.
local g_opt = {}

-- Global state for current file.
local g_fname, g_curline, g_indent, g_lineno, g_synclineno, g_arch
local g_errcount = 0

-- Write buffer for output file.
local g_wbuffer, g_capbuffer

------------------------------------------------------------------------------

-- Write an output line (or callback function) to the buffer.
local function wline(line, needindent)
  local buf = g_capbuffer or g_wbuffer
  buf[#buf+1] = needindent and g_indent..line or line
  g_synclineno = g_synclineno + 1
end

-- Write assembler line as a comment, if requestd.
local function wcomment(aline)
  if g_opt.comment then
    wline(g_opt.comment..aline..g_opt.endcomment, true)
  end
end

-- Resync CPP line numbers.
local function wsync()
  if g_synclineno ~= g_lineno and g_opt.cpp then
    wline("# "..g_lineno..' "'..g_fname..'"')
    g_synclineno = g_lineno
  end
end

-- Dummy action flush function. Replaced with arch-specific function later.
local function wflush(term)
end

-- Dump all buffered output lines.
local function wdumplines(out, buf)
  for _,line in ipairs(buf) do
    if type(line) == "string" then
      assert(out:write(line, "\n"))
    else
      -- Special callback to dynamically insert lines after end of processing.
      line(out)
    end
  end
end

------------------------------------------------------------------------------

-- Emit an error. Processing continues with next statement.
local function werror(msg)
  error(format("%s:%s: error: %s:\n%s", g_fname, g_lineno, msg, g_curline), 0)
end

-- Emit a fatal error. Processing stops.
local function wfatal(msg)
  g_errcount = "fatal"
  werror(msg)
end

-- Print a warning. Processing continues.
local function wwarn(msg)
  stderr:write(format("%s:%s: warning: %s:\n%s\n",
    g_fname, g_lineno, msg, g_curline))
end

-- Print caught error message. But suppress excessive errors.
local function wprinterr(...)
  if type(g_errcount) == "number" then
    -- Regular error.
    g_errcount = g_errcount + 1
    if g_errcount < 21 then -- Seems to be a reasonable limit.
      stderr:write(...)
    elseif g_errcount == 21 then
      stderr:write(g_fname,
	":*: warning: too many errors (suppressed further messages).\n")
    end
  else
    -- Fatal error.
    stderr:write(...)
    return true -- Stop processing.
  end
end

------------------------------------------------------------------------------

-- Map holding all option handlers.
local opt_map = {}
local opt_current

-- Print error and exit with error status.
local function opterror(...)
  stderr:write("dynasm.lua: ERROR: ", ...)
  stderr:write("\n")
  exit(1)
end

-- Get option parameter.
local function optparam(args)
  local argn = args.argn
  local p = args[argn]
  if not p then
    opterror("missing parameter for option `", opt_current, "'.")
  end
  args.argn = argn + 1
  return p
end

------------------------------------------------------------------------------

-- Core pseudo-opcodes.
local map_coreop = {}
-- Dummy opcode map. Replaced by arch-specific map.
local map_op = {}

-- Forward declarations.
local dostmt
local readfile

------------------------------------------------------------------------------

-- Map for defines (initially empty, chains to arch-specific map).
local map_def = {}

-- Pseudo-opcode to define a substitution.
map_coreop[".define_2"] = function(params, nparams)
  if not params then return nparams == 1 and "name" or "name, subst" end
  local name, def = params[1], params[2] or "1"
  if not match(name, "^[%a_][%w_]*$") then werror("bad or duplicate define") end
  map_def[name] = def
end
map_coreop[".define_1"] = map_coreop[".define_2"]

-- Define a substitution on the command line.
function opt_map.D(args)
  local namesubst = optparam(args)
  local name, subst = match(namesubst, "^([%a_][%w_]*)=(.*)$")
  if name then
    map_def[name] = subst
  elseif match(namesubst, "^[%a_][%w_]*$") then
    map_def[namesubst] = "1"
  else
    opterror("bad define")
  end
end

-- Undefine a substitution on the command line.
function opt_map.U(args)
  local name = optparam(args)
  if match(name, "^[%a_][%w_]*$") then
    map_def[name] = nil
  else
    opterror("bad define")
  end
end

-- Helper for definesubst.
local gotsubst

local function definesubst_one(word)
  local subst = map_def[word]
  if subst then gotsubst = word; return subst else return word end
end

-- Iteratively substitute defines.
local function definesubst(stmt)
  -- Limit number of iterations.
  for i=1,100 do
    gotsubst = false
    stmt = gsub(stmt, "#?[%w_]+", definesubst_one)
    if not gotsubst then break end
  end
  if gotsubst then wfatal("recursive define involving `"..gotsubst.."'") end
  return stmt
end

-- Dump all defines.
local function dumpdefines(out, lvl)
  local t = {}
  for name in pairs(map_def) do
    t[#t+1] = name
  end
  sort(t)
  out:write("Defines:\n")
  for _,name in ipairs(t) do
    local subst = map_def[name]
    if g_arch then subst = g_arch.revdef(subst) end
    out:write(format("  %-20s %s\n", name, subst))
  end
  out:write("\n")
end

------------------------------------------------------------------------------

-- Support variables for conditional assembly.
local condlevel = 0
local condstack = {}

-- Evaluate condition with a Lua expression. Substitutions already performed.
local function cond_eval(cond)
  local func, err
  if setfenv then
    func, err = loadstring("return "..cond, "=expr")
  else
    -- No globals. All unknown identifiers evaluate to nil.
    func, err = load("return "..cond, "=expr", "t", {})
  end
  if func then
    if setfenv then
      setfenv(func, {}) -- No globals. All unknown identifiers evaluate to nil.
    end
    local ok, res = pcall(func)
    if ok then
      if res == 0 then return false end -- Oh well.
      return not not res
    end
    err = res
  end
  wfatal("bad condition: "..err)
end

-- Skip statements until next conditional pseudo-opcode at the same level.
local function stmtskip()
  local dostmt_save = dostmt
  local lvl = 0
  dostmt = function(stmt)
    local op = match(stmt, "^%s*(%S+)")
    if op == ".if" then
      lvl = lvl + 1
    elseif lvl ~= 0 then
      if op == ".endif" then lvl = lvl - 1 end
    elseif op == ".elif" or op == ".else" or op == ".endif" then
      dostmt = dostmt_save
      dostmt(stmt)
    end
  end
end

-- Pseudo-opcodes for conditional assembly.
map_coreop[".if_1"] = function(params)
  if not params then return "condition" end
  local lvl = condlevel + 1
  local res = cond_eval(params[1])
  condlevel = lvl
  condstack[lvl] = res
  if not res then stmtskip() end
end

map_coreop[".elif_1"] = function(params)
  if not params then return "condition" end
  if condlevel == 0 then wfatal(".elif without .if") end
  local lvl = condlevel
  local res = condstack[lvl]
  if res then
    if res == "else" then wfatal(".elif after .else") end
  else
    res = cond_eval(params[1])
    if res then
      condstack[lvl] = res
      return
    end
  end
  stmtskip()
end

map_coreop[".else_0"] = function(params)
  if condlevel == 0 then wfatal(".else without .if") end
  local lvl = condlevel
  local res = condstack[lvl]
  condstack[lvl] = "else"
  if res then
    if res == "else" then wfatal(".else after .else") end
    stmtskip()
  end
end

map_coreop[".endif_0"] = function(params)
  local lvl = condlevel
  if lvl == 0 then wfatal(".endif without .if") end
  condlevel = lvl - 1
end

-- Check for unfinished conditionals.
local function checkconds()
  if g_errcount ~= "fatal" and condlevel ~= 0 then
    wprinterr(g_fname, ":*: error: unbalanced conditional\n")
  end
end

------------------------------------------------------------------------------

-- Search for a file in the given path and open it for reading.
local function pathopen(path, name)
  local dirsep = package and match(package.path, "\\") and "\\" or "/"
  for _,p in ipairs(path) do
    local fullname = p == "" and name or p..dirsep..name
    local fin = io.open(fullname, "r")
    if fin then
      g_fname = fullname
      return fin
    end
  end
end

-- Include a file.
map_coreop[".include_1"] = function(params)
  if not params then return "filename" end
  local name = params[1]
  -- Save state. Ugly, I know. but upvalues are fast.
  local gf, gl, gcl, gi = g_fname, g_lineno, g_curline, g_indent
  -- Read the included file.
  local fatal = readfile(pathopen(g_opt.include, name) or
			 wfatal("include file `"..name.."' not found"))
  -- Restore state.
  g_synclineno = -1
  g_fname, g_lineno, g_curline, g_indent = gf, gl, gcl, gi
  if fatal then wfatal("in include file") end
end

-- Make .include and conditionals initially available, too.
map_op[".include_1"] = map_coreop[".include_1"]
map_op[".if_1"] = map_coreop[".if_1"]
map_op[".elif_1"] = map_coreop[".elif_1"]
map_op[".else_0"] = map_coreop[".else_0"]
map_op[".endif_0"] = map_coreop[".endif_0"]

------------------------------------------------------------------------------

-- Support variables for macros.
local mac_capture, mac_lineno, mac_name
local mac_active = {}
local mac_list = {}

-- Pseudo-opcode to define a macro.
map_coreop[".macro_*"] = function(mparams)
  if not mparams then return "name [, params...]" end
  -- Split off and validate macro name.
  local name = remove(mparams, 1)
  if not name then werror("missing macro name") end
  if not (match(name, "^[%a_][%w_%.]*$") or match(name, "^%.[%w_%.]*$")) then
    wfatal("bad macro name `"..name.."'")
  end
  -- Validate macro parameter names.
  local mdup = {}
  for _,mp in ipairs(mparams) do
    if not match(mp, "^[%a_][%w_]*$") then
      wfatal("bad macro parameter name `"..mp.."'")
    end
    if mdup[mp] then wfatal("duplicate macro parameter name `"..mp.."'") end
    mdup[mp] = true
  end
  -- Check for duplicate or recursive macro definitions.
  local opname = name.."_"..#mparams
  if map_op[opname] or map_op[name.."_*"] then
    wfatal("duplicate macro `"..name.."' ("..#mparams.." parameters)")
  end
  if mac_capture then wfatal("recursive macro definition") end

  -- Enable statement capture.
  local lines = {}
  mac_lineno = g_lineno
  mac_name = name
  mac_capture = function(stmt) -- Statement capture function.
    -- Stop macro definition with .endmacro pseudo-opcode.
    if not match(stmt, "^%s*.endmacro%s*$") then
      lines[#lines+1] = stmt
      return
    end
    mac_capture = nil
    mac_lineno = nil
    mac_name = nil
    mac_list[#mac_list+1] = opname
    -- Add macro-op definition.
    map_op[opname] = function(params)
      if not params then return mparams, lines end
      -- Protect against recursive macro invocation.
      if mac_active[opname] then wfatal("recursive macro invocation") end
      mac_active[opname] = true
      -- Setup substitution map.
      local subst = {}
      for i,mp in ipairs(mparams) do subst[mp] = params[i] end
      local mcom
      if g_opt.maccomment and g_opt.comment then
	mcom = " MACRO "..name.." ("..#mparams..")"
	wcomment("{"..mcom)
      end
      -- Loop through all captured statements
      for _,stmt in ipairs(lines) do
	-- Substitute macro parameters.
	local st = gsub(stmt, "[%w_]+", subst)
	st = definesubst(st)
	st = gsub(st, "%s*%.%.%s*", "") -- Token paste a..b.
	if mcom and sub(st, 1, 1) ~= "|" then wcomment(st) end
	-- Emit statement. Use a protected call for better diagnostics.
	local ok, err = pcall(dostmt, st)
	if not ok then
	  -- Add the captured statement to the error.
	  wprinterr(err, "\n", g_indent, "|  ", stmt,
		    "\t[MACRO ", name, " (", #mparams, ")]\n")
	end
      end
      if mcom then wcomment("}"..mcom) end
      mac_active[opname] = nil
    end
  end
end

-- An .endmacro pseudo-opcode outside of a macro definition is an error.
map_coreop[".endmacro_0"] = function(params)
  wfatal(".endmacro without .macro")
end

-- Dump all macros and their contents (with -PP only).
local function dumpmacros(out, lvl)
  sort(mac_list)
  out:write("Macros:\n")
  for _,opname in ipairs(mac_list) do
    local name = sub(opname, 1, -3)
    local params, lines = map_op[opname]()
    out:write(format("  %-20s %s\n", name, concat(params, ", ")))
    if lvl > 1 then
      for _,line in ipairs(lines) do
	out:write("  |", line, "\n")
      end
      out:write("\n")
    end
  end
  out:write("\n")
end

-- Check for unfinished macro definitions.
local function checkmacros()
  if mac_capture then
    wprinterr(g_fname, ":", mac_lineno,
	      ": error: unfinished .macro `", mac_name ,"'\n")
  end
end

------------------------------------------------------------------------------

-- Support variables for captures.
local cap_lineno, cap_name
local cap_buffers = {}
local cap_used = {}

-- Start a capture.
map_coreop[".capture_1"] = function(params)
  if not params then return "name" end
  wflush()
  local name = params[1]
  if not match(name, "^[%a_][%w_]*$") then
    wfatal("bad capture name `"..name.."'")
  end
  if cap_name then
    wfatal("already capturing to `"..cap_name.."' since line "..cap_lineno)
  end
  cap_name = name
  cap_lineno = g_lineno
  -- Create or continue a capture buffer and start the output line capture.
  local buf = cap_buffers[name]
  if not buf then buf = {}; cap_buffers[name] = buf end
  g_capbuffer = buf
  g_synclineno = 0
end

-- Stop a capture.
map_coreop[".endcapture_0"] = function(params)
  wflush()
  if not cap_name then wfatal(".endcapture without a valid .capture") end
  cap_name = nil
  cap_lineno = nil
  g_capbuffer = nil
  g_synclineno = 0
end

-- Dump a capture buffer.
map_coreop[".dumpcapture_1"] = function(params)
  if not params then return "name" end
  wflush()
  local name = params[1]
  if not match(name, "^[%a_][%w_]*$") then
    wfatal("bad capture name `"..name.."'")
  end
  cap_used[name] = true
  wline(function(out)
    local buf = cap_buffers[name]
    if buf then wdumplines(out, buf) end
  end)
  g_synclineno = 0
end

-- Dump all captures and their buffers (with -PP only).
local function dumpcaptures(out, lvl)
  out:write("Captures:\n")
  for name,buf in pairs(cap_buffers) do
    out:write(format("  %-20s %4s)\n", name, "("..#buf))
    if lvl > 1 then
      local bar = rep("=", 76)
      out:write("  ", bar, "\n")
      for _,line in ipairs(buf) do
	out:write("  ", line, "\n")
      end
      out:write("  ", bar, "\n\n")
    end
  end
  out:write("\n")
end

-- Check for unfinished or unused captures.
local function checkcaptures()
  if cap_name then
    wprinterr(g_fname, ":", cap_lineno,
	      ": error: unfinished .capture `", cap_name,"'\n")
    return
  end
  for name in pairs(cap_buffers) do
    if not cap_used[name] then
      wprinterr(g_fname, ":*: error: missing .dumpcapture ", name ,"\n")
    end
  end
end

------------------------------------------------------------------------------

-- Sections names.
local map_sections = {}

-- Pseudo-opcode to define code sections.
-- TODO: Data sections, BSS sections. Needs extra C code and API.
map_coreop[".section_*"] = function(params)
  if not params then return "name..." end
  if #map_sections > 0 then werror("duplicate section definition") end
  wflush()
  for sn,name in ipairs(params) do
    local opname = "."..name.."_0"
    if not match(name, "^[%a][%w_]*$") or
       map_op[opname] or map_op["."..name.."_*"] then
      werror("bad section name `"..name.."'")
    end
    map_sections[#map_sections+1] = name
    wline(format("#define DASM_SECTION_%s\t%d", upper(name), sn-1))
    map_op[opname] = function(params) g_arch.section(sn-1) end
  end
  wline(format("#define DASM_MAXSECTION\t\t%d", #map_sections))
end

-- Dump all sections.
local function dumpsections(out, lvl)
  out:write("Sections:\n")
  for _,name in ipairs(map_sections) do
    out:write(format("  %s\n", name))
  end
  out:write("\n")
end

------------------------------------------------------------------------------

-- Replacement for customized Lua, which lacks the package library.
local prefix = ""
if not require then
  function require(name)
    local fp = assert(io.open(prefix..name..".lua"))
    local s = fp:read("*a")
    assert(fp:close())
    return assert(loadstring(s, "@"..name..".lua"))()
  end
end

-- Load architecture-specific module.
local function loadarch(arch)
  if not match(arch, "^[%w_]+$") then return "bad arch name" end
  local ok, m_arch = pcall(require, "dasm_"..arch)
  if not ok then return "cannot load module: "..m_arch end
  g_arch = m_arch
  wflush = m_arch.passcb(wline, werror, wfatal, wwarn)
  m_arch.setup(arch, g_opt)
  map_op, map_def = m_arch.mergemaps(map_coreop, map_def)
end

-- Dump architecture description.
function opt_map.dumparch(args)
  local name = optparam(args)
  if not g_arch then
    local err = loadarch(name)
    if err then opterror(err) end
  end

  local t = {}
  for name in pairs(map_coreop) do t[#t+1] = name end
  for name in pairs(map_op) do t[#t+1] = name end
  sort(t)

  local out = stdout
  local _arch = g_arch._info
  out:write(format("%s version %s, released %s, %s\n",
    _info.name, _info.version, _info.release, _info.url))
  g_arch.dumparch(out)

  local pseudo = true
  out:write("Pseudo-Opcodes:\n")
  for _,sname in ipairs(t) do
    local name, nparam = match(sname, "^(.+)_([0-9%*])$")
    if name then
      if pseudo and sub(name, 1, 1) ~= "." then
	out:write("\nOpcodes:\n")
	pseudo = false
      end
      local f = map_op[sname]
      local s
      if nparam ~= "*" then nparam = nparam + 0 end
      if nparam == 0 then
	s = ""
      elseif type(f) == "string" then
	s = map_op[".template__"](nil, f, nparam)
      else
	s = f(nil, nparam)
      end
      if type(s) == "table" then
	for _,s2 in ipairs(s) do
	  out:write(format("  %-12s %s\n", name, s2))
	end
      else
	out:write(format("  %-12s %s\n", name, s))
      end
    end
  end
  out:write("\n")
  exit(0)
end

-- Pseudo-opcode to set the architecture.
-- Only initially available (map_op is replaced when called).
map_op[".arch_1"] = function(params)
  if not params then return "name" end
  local err = loadarch(params[1])
  if err then wfatal(err) end
end

-- Dummy .arch pseudo-opcode to improve the error report.
map_coreop[".arch_1"] = function(params)
  if not params then return "name" end
  wfatal("duplicate .arch statement")
end

------------------------------------------------------------------------------

-- Dummy pseudo-opcode. Don't confuse '.nop' with 'nop'.
map_coreop[".nop_*"] = function(params)
  if not params then return "[ignored...]" end
end

-- Pseudo-opcodes to raise errors.
map_coreop[".error_1"] = function(params)
  if not params then return "message" end
  werror(params[1])
end

map_coreop[".fatal_1"] = function(params)
  if not params then return "message" end
  wfatal(params[1])
end

-- Dump all user defined elements.
local function dumpdef(out)
  local lvl = g_opt.dumpdef
  if lvl == 0 then return end
  dumpsections(out, lvl)
  dumpdefines(out, lvl)
  if g_arch then g_arch.dumpdef(out, lvl) end
  dumpmacros(out, lvl)
  dumpcaptures(out, lvl)
end

------------------------------------------------------------------------------

-- Helper for splitstmt.
local splitlvl

local function splitstmt_one(c)
  if c == "(" then
    splitlvl = ")"..splitlvl
  elseif c == "[" then
    splitlvl = "]"..splitlvl
  elseif c == "{" then
    splitlvl = "}"..splitlvl
  elseif c == ")" or c == "]" or c == "}" then
    if sub(splitlvl, 1, 1) ~= c then werror("unbalanced (), [] or {}") end
    splitlvl = sub(splitlvl, 2)
  elseif splitlvl == "" then
    return " \0 "
  end
  return c
end

-- Split statement into (pseudo-)opcode and params.
local function splitstmt(stmt)
  -- Convert label with trailing-colon into .label statement.
  local label = match(stmt, "^%s*(.+):%s*$")
  if label then return ".label", {label} end

  -- Split at commas and equal signs, but obey parentheses and brackets.
  splitlvl = ""
  stmt = gsub(stmt, "[,%(%)%[%]{}]", splitstmt_one)
  if splitlvl ~= "" then werror("unbalanced () or []") end

  -- Split off opcode.
  local op, other = match(stmt, "^%s*([^%s%z]+)%s*(.*)$")
  if not op then werror("bad statement syntax") end

  -- Split parameters.
  local params = {}
  for p in gmatch(other, "%s*(%Z+)%z?") do
    params[#params+1] = gsub(p, "%s+$", "")
  end
  if #params > 16 then werror("too many parameters") end

  params.op = op
  return op, params
end

-- Process a single statement.
dostmt = function(stmt)
  -- Ignore empty statements.
  if match(stmt, "^%s*$") then return end

  -- Capture macro defs before substitution.
  if mac_capture then return mac_capture(stmt) end
  stmt = definesubst(stmt)

  -- Emit C code without parsing the line.
  if sub(stmt, 1, 1) == "|" then
    local tail = sub(stmt, 2)
    wflush()
    if sub(tail, 1, 2) == "//" then wcomment(tail) else wline(tail, true) end
    return
  end

  -- Split into (pseudo-)opcode and params.
  local op, params = splitstmt(stmt)

  -- Get opcode handler (matching # of parameters or generic handler).
  local f = map_op[op.."_"..#params] or map_op[op.."_*"]
  if not f then
    if not g_arch then wfatal("first statement must be .arch") end
    -- Improve error report.
    for i=0,9 do
      if map_op[op.."_"..i] then
	werror("wrong number of parameters for `"..op.."'")
      end
    end
    werror("unknown statement `"..op.."'")
  end

  -- Call opcode handler or special handler for template strings.
  if type(f) == "string" then
    map_op[".template__"](params, f)
  else
    f(params)
  end
end

-- Process a single line.
local function doline(line)
  if g_opt.flushline then wflush() end

  -- Assembler line?
  local indent, aline = match(line, "^(%s*)%|(.*)$")
  if not aline then
    -- No, plain C code line, need to flush first.
    wflush()
    wsync()
    wline(line, false)
    return
  end

  g_indent = indent -- Remember current line indentation.

  -- Emit C code (even from macros). Avoids echo and line parsing.
  if sub(aline, 1, 1) == "|" then
    if not mac_capture then
      wsync()
    elseif g_opt.comment then
      wsync()
      wcomment(aline)
    end
    dostmt(aline)
    return
  end

  -- Echo assembler line as a comment.
  if g_opt.comment then
    wsync()
    wcomment(aline)
  end

  -- Strip assembler comments.
  aline = gsub(aline, "//.*$", "")

  -- Split line into statements at semicolons.
  if match(aline, ";") then
    for stmt in gmatch(aline, "[^;]+") do dostmt(stmt) end
  else
    dostmt(aline)
  end
end

------------------------------------------------------------------------------

-- Write DynASM header.
local function dasmhead(out)
  out:write(format([[
/*
** This file has been pre-processed with DynASM.
** %s
** DynASM version %s, DynASM %s version %s
** DO NOT EDIT! The original file is in "%s".
*/

#if DASM_VERSION != %d
#error "Version mismatch between DynASM and included encoding engine"
#endif

]], _info.url,
    _info.version, g_arch._info.arch, g_arch._info.version,
    g_fname, _info.vernum))
end

-- Read input file.
readfile = function(fin)
  g_indent = ""
  g_lineno = 0
  g_synclineno = -1

  -- Process all lines.
  for line in fin:lines() do
    g_lineno = g_lineno + 1
    g_curline = line
    local ok, err = pcall(doline, line)
    if not ok and wprinterr(err, "\n") then return true end
  end
  wflush()

  -- Close input file.
  assert(fin == stdin or fin:close())
end

-- Write output file.
local function writefile(outfile)
  local fout

  -- Open output file.
  if outfile == nil or outfile == "-" then
    fout = stdout
  else
    fout = assert(io.open(outfile, "w"))
  end

  -- Write all buffered lines
  wdumplines(fout, g_wbuffer)

  -- Close output file.
  assert(fout == stdout or fout:close())

  -- Optionally dump definitions.
  dumpdef(fout == stdout and stderr or stdout)
end

-- Translate an input file to an output file.
local function translate(infile, outfile)
  g_wbuffer = {}
  g_indent = ""
  g_lineno = 0
  g_synclineno = -1

  -- Put header.
  wline(dasmhead)

  -- Read input file.
  local fin
  if infile == "-" then
    g_fname = "(stdin)"
    fin = stdin
  else
    g_fname = infile
    fin = assert(io.open(infile, "r"))
  end
  readfile(fin)

  -- Check for errors.
  if not g_arch then
    wprinterr(g_fname, ":*: error: missing .arch directive\n")
  end
  checkconds()
  checkmacros()
  checkcaptures()

  if g_errcount ~= 0 then
    stderr:write(g_fname, ":*: info: ", g_errcount, " error",
      (type(g_errcount) == "number" and g_errcount > 1) and "s" or "",
      " in input file -- no output file generated.\n")
    dumpdef(stderr)
    exit(1)
  end

  -- Write output file.
  writefile(outfile)
end

------------------------------------------------------------------------------

-- Print help text.
function opt_map.help()
  stdout:write("DynASM -- ", _info.description, ".\n")
  stdout:write("DynASM ", _info.version, " ", _info.release, "  ", _info.url, "\n")
  stdout:write[[

Usage: dynasm [OPTION]... INFILE.dasc|-

  -h, --help           Display this help text.
  -V, --version        Display version and copyright information.

  -o, --outfile FILE   Output file name (default is stdout).
  -I, --include DIR    Add directory to the include search path.

  -c, --ccomment       Use /* */ comments for assembler lines.
  -C, --cppcomment     Use // comments for assembler lines (default).
  -N, --nocomment      Suppress assembler lines in output.
  -M, --maccomment     Show macro expansions as comments (default off).

  -L, --nolineno       Suppress CPP line number information in output.
  -F, --flushline      Flush action list for every line.

  -D NAME[=SUBST]      Define a substitution.
  -U NAME              Undefine a substitution.

  -P, --dumpdef        Dump defines, macros, etc. Repeat for more output.
  -A, --dumparch ARCH  Load architecture ARCH and dump description.
]]
  exit(0)
end

-- Print version information.
function opt_map.version()
  stdout:write(format("%s version %s, released %s\n%s\n\n%s",
    _info.name, _info.version, _info.release, _info.url, _info.copyright))
  exit(0)
end

-- Misc. options.
function opt_map.outfile(args) g_opt.outfile = optparam(args) end
function opt_map.include(args) insert(g_opt.include, 1, optparam(args)) end
function opt_map.ccomment() g_opt.comment = "/*|"; g_opt.endcomment = " */" end
function opt_map.cppcomment() g_opt.comment = "//|"; g_opt.endcomment = "" end
function opt_map.nocomment() g_opt.comment = false end
function opt_map.maccomment() g_opt.maccomment = true end
function opt_map.nolineno() g_opt.cpp = false end
function opt_map.flushline() g_opt.flushline = true end
function opt_map.dumpdef() g_opt.dumpdef = g_opt.dumpdef + 1 end

------------------------------------------------------------------------------

-- Short aliases for long options.
local opt_alias = {
  h = "help", ["?"] = "help", V = "version",
  o = "outfile", I = "include",
  c = "ccomment", C = "cppcomment", N = "nocomment", M = "maccomment",
  L = "nolineno", F = "flushline",
  P = "dumpdef", A = "dumparch",
}

-- Parse single option.
local function parseopt(opt, args)
  opt_current = #opt == 1 and "-"..opt or "--"..opt
  local f = opt_map[opt] or opt_map[opt_alias[opt]]
  if not f then
    opterror("unrecognized option `", opt_current, "'. Try `--help'.\n")
  end
  f(args)
end

-- Parse arguments.
local function parseargs(args)
  -- Default options.
  g_opt.comment = "//|"
  g_opt.endcomment = ""
  g_opt.cpp = true
  g_opt.dumpdef = 0
  g_opt.include = { "" }

  -- Process all option arguments.
  args.argn = 1
  repeat
    local a = args[args.argn]
    if not a then break end
    local lopt, opt = match(a, "^%-(%-?)(.+)")
    if not opt then break end
    args.argn = args.argn + 1
    if lopt == "" then
      -- Loop through short options.
      for o in gmatch(opt, ".") do parseopt(o, args) end
    else
      -- Long option.
      parseopt(opt, args)
    end
  until false

  -- Check for proper number of arguments.
  local nargs = #args - args.argn + 1
  if nargs ~= 1 then
    if nargs == 0 then
      if g_opt.dumpdef > 0 then return dumpdef(stdout) end
    end
    opt_map.help()
  end

  -- Translate a single input file to a single output file
  -- TODO: Handle multiple files?
  translate(args[args.argn], g_opt.outfile)
end

------------------------------------------------------------------------------

-- Add the directory dynasm.lua resides in to the Lua module search path.
local arg = arg
if arg and arg[0] then
  prefix = match(arg[0], "^(.*[/\\])")
  if package and prefix then package.path = prefix.."?.lua;"..package.path end
end

-- Start DynASM.
parseargs{...}

------------------------------------------------------------------------------

