----------------------------------------------------------------------------
-- Lua script to generate a customized, minified version of Lua.
-- The resulting 'minilua' is used for the build process of LuaJIT.
----------------------------------------------------------------------------
-- Copyright (C) 2005-2013 Mike Pall. All rights reserved.
-- Released under the MIT license. See Copyright Notice in luajit.h
----------------------------------------------------------------------------

local sub, match, gsub = string.sub, string.match, string.gsub

local LUA_VERSION = "5.1.5"
local LUA_SOURCE

local function usage()
  io.stderr:write("Usage: ", arg and arg[0] or "genminilua",
		  " lua-", LUA_VERSION, "-source-dir\n")
  os.exit(1)
end

local function find_sources()
  LUA_SOURCE = arg and arg[1]
  if not LUA_SOURCE then usage() end
  if sub(LUA_SOURCE, -1) ~= "/" then LUA_SOURCE = LUA_SOURCE.."/" end
  local fp = io.open(LUA_SOURCE .. "lua.h")
  if not fp then
    LUA_SOURCE = LUA_SOURCE.."src/"
    fp = io.open(LUA_SOURCE .. "lua.h")
    if not fp then usage() end
  end
  local all = fp:read("*a")
  fp:close()
  if not match(all, 'LUA_RELEASE%s*"Lua '..LUA_VERSION..'"') then
    io.stderr:write("Error: version mismatch\n")
    usage()
  end
end

local LUA_FILES = {
"lmem.c", "lobject.c", "ltm.c", "lfunc.c", "ldo.c", "lstring.c", "ltable.c",
"lgc.c", "lstate.c", "ldebug.c", "lzio.c", "lopcodes.c",
"llex.c", "lcode.c", "lparser.c", "lvm.c", "lapi.c", "lauxlib.c",
"lbaselib.c", "ltablib.c", "liolib.c", "loslib.c", "lstrlib.c", "linit.c",
}

local REMOVE_LIB = {}
gsub([[
collectgarbage dofile gcinfo getfenv getmetatable load print rawequal rawset
select tostring xpcall
foreach foreachi getn maxn setn
popen tmpfile seek setvbuf __tostring
clock date difftime execute getenv rename setlocale time tmpname
dump gfind len reverse
LUA_LOADLIBNAME LUA_MATHLIBNAME LUA_DBLIBNAME
]], "%S+", function(name)
  REMOVE_LIB[name] = true
end)

local REMOVE_EXTINC = { ["<assert.h>"] = true, ["<locale.h>"] = true, }

local CUSTOM_MAIN = [[
typedef unsigned int UB;
static UB barg(lua_State *L,int idx){
union{lua_Number n;U64 b;}bn;
bn.n=lua_tonumber(L,idx)+6755399441055744.0;
if (bn.n==0.0&&!lua_isnumber(L,idx))luaL_typerror(L,idx,"number");
return(UB)bn.b;
}
#define BRET(b) lua_pushnumber(L,(lua_Number)(int)(b));return 1;
static int tobit(lua_State *L){
BRET(barg(L,1))}
static int bnot(lua_State *L){
BRET(~barg(L,1))}
static int band(lua_State *L){
int i;UB b=barg(L,1);for(i=lua_gettop(L);i>1;i--)b&=barg(L,i);BRET(b)}
static int bor(lua_State *L){
int i;UB b=barg(L,1);for(i=lua_gettop(L);i>1;i--)b|=barg(L,i);BRET(b)}
static int bxor(lua_State *L){
int i;UB b=barg(L,1);for(i=lua_gettop(L);i>1;i--)b^=barg(L,i);BRET(b)}
static int lshift(lua_State *L){
UB b=barg(L,1),n=barg(L,2)&31;BRET(b<<n)}
static int rshift(lua_State *L){
UB b=barg(L,1),n=barg(L,2)&31;BRET(b>>n)}
static int arshift(lua_State *L){
UB b=barg(L,1),n=barg(L,2)&31;BRET((int)b>>n)}
static int rol(lua_State *L){
UB b=barg(L,1),n=barg(L,2)&31;BRET((b<<n)|(b>>(32-n)))}
static int ror(lua_State *L){
UB b=barg(L,1),n=barg(L,2)&31;BRET((b>>n)|(b<<(32-n)))}
static int bswap(lua_State *L){
UB b=barg(L,1);b=(b>>24)|((b>>8)&0xff00)|((b&0xff00)<<8)|(b<<24);BRET(b)}
static int tohex(lua_State *L){
UB b=barg(L,1);
int n=lua_isnone(L,2)?8:(int)barg(L,2);
const char *hexdigits="0123456789abcdef";
char buf[8];
int i;
if(n<0){n=-n;hexdigits="0123456789ABCDEF";}
if(n>8)n=8;
for(i=(int)n;--i>=0;){buf[i]=hexdigits[b&15];b>>=4;}
lua_pushlstring(L,buf,(size_t)n);
return 1;
}
static const struct luaL_Reg bitlib[] = {
{"tobit",tobit},
{"bnot",bnot},
{"band",band},
{"bor",bor},
{"bxor",bxor},
{"lshift",lshift},
{"rshift",rshift},
{"arshift",arshift},
{"rol",rol},
{"ror",ror},
{"bswap",bswap},
{"tohex",tohex},
{NULL,NULL}
};
int main(int argc, char **argv){
  lua_State *L = luaL_newstate();
  int i;
  luaL_openlibs(L);
  luaL_register(L, "bit", bitlib);
  if (argc < 2) return sizeof(void *);
  lua_createtable(L, 0, 1);
  lua_pushstring(L, argv[1]);
  lua_rawseti(L, -2, 0);
  lua_setglobal(L, "arg");
  if (luaL_loadfile(L, argv[1]))
    goto err;
  for (i = 2; i < argc; i++)
    lua_pushstring(L, argv[i]);
  if (lua_pcall(L, argc - 2, 0, 0)) {
  err:
    fprintf(stderr, "Error: %s\n", lua_tostring(L, -1));
    return 1;
  }
  lua_close(L);
  return 0;
}
]]

local function read_sources()
  local t = {}
  for i, name in ipairs(LUA_FILES) do
    local fp = assert(io.open(LUA_SOURCE..name, "r"))
    t[i] = fp:read("*a")
    assert(fp:close())
  end
  t[#t+1] = CUSTOM_MAIN
  return table.concat(t)
end

local includes = {}

local function merge_includes(src)
  return gsub(src, '#include%s*"([^"]*)"%s*\n', function(name)
    if includes[name] then return "" end
    includes[name] = true
    local fp = assert(io.open(LUA_SOURCE..name, "r"))
    local src = fp:read("*a")
    assert(fp:close())
    src = gsub(src, "#ifndef%s+%w+_h\n#define%s+%w+_h\n", "")
    src = gsub(src, "#endif%s*$", "")
    return merge_includes(src)
  end)
end

local function get_license(src)
  return match(src, "/%*+\n%* Copyright %(.-%*/\n")
end

local function fold_lines(src)
  return gsub(src, "\\\n", " ")
end

local strings = {}

local function save_str(str)
  local n = #strings+1
  strings[n] = str
  return "\1"..n.."\2"
end

local function save_strings(src)
  src = gsub(src, '"[^"\n]*"', save_str)
  return gsub(src, "'[^'\n]*'", save_str)
end

local function restore_strings(src)
  return gsub(src, "\1(%d+)\2", function(numstr)
    return strings[tonumber(numstr)]
  end)
end

local function def_istrue(def)
  return def == "INT_MAX > 2147483640L" or
	 def == "LUAI_BITSINT >= 32" or
	 def == "SIZE_Bx < LUAI_BITSINT-1" or
	 def == "cast" or
	 def == "defined(LUA_CORE)" or
	 def == "MINSTRTABSIZE" or
	 def == "LUA_MINBUFFER" or
	 def == "HARDSTACKTESTS" or
	 def == "UNUSED"
end

local head, defs = {[[
#ifdef _MSC_VER
typedef unsigned __int64 U64;
#else
typedef unsigned long long U64;
#endif
]]}, {}

local function preprocess(src)
  local t = { match(src, "^(.-)#") }
  local lvl, on, oldon = 0, true, {}
  for pp, def, txt in string.gmatch(src, "#(%w+) *([^\n]*)\n([^#]*)") do
    if pp == "if" or pp == "ifdef" or pp == "ifndef" then
      lvl = lvl + 1
      oldon[lvl] = on
      on = def_istrue(def)
    elseif pp == "else" then
      if oldon[lvl] then
	if on == false then on = true else on = false end
      end
    elseif pp == "elif" then
      if oldon[lvl] then
	on = def_istrue(def)
      end
    elseif pp == "endif" then
      on = oldon[lvl]
      lvl = lvl - 1
    elseif on then
      if pp == "include" then
	if not head[def] and not REMOVE_EXTINC[def] then
	  head[def] = true
	  head[#head+1] = "#include "..def.."\n"
	end
      elseif pp == "define" then
	local k, sp, v = match(def, "([%w_]+)(%s*)(.*)")
	if k and not (sp == "" and sub(v, 1, 1) == "(") then
	  defs[k] = gsub(v, "%a[%w_]*", function(tok)
	    return defs[tok] or tok
	  end)
	else
	  t[#t+1] = "#define "..def.."\n"
	end
      elseif pp ~= "undef" then
	error("unexpected directive: "..pp.." "..def)
      end
    end
    if on then t[#t+1] = txt end
  end
  return gsub(table.concat(t), "%a[%w_]*", function(tok)
    return defs[tok] or tok
  end)
end

local function merge_header(src, license)
  local hdr = string.format([[
/* This is a heavily customized and minimized copy of Lua %s. */
/* It's only used to build LuaJIT. It does NOT have all standard functions! */
]], LUA_VERSION)
  return hdr..license..table.concat(head)..src
end

local function strip_unused1(src)
  return gsub(src, '(  {"?([%w_]+)"?,%s+%a[%w_]*},\n)', function(line, func)
    return REMOVE_LIB[func] and "" or line
  end)
end

local function strip_unused2(src)
  return gsub(src, "Symbolic Execution.-}=", "")
end

local function strip_unused3(src)
  src = gsub(src, "extern", "static")
  src = gsub(src, "\nstatic([^\n]-)%(([^)]*)%)%(", "\nstatic%1 %2(")
  src = gsub(src, "#define lua_assert[^\n]*\n", "")
  src = gsub(src, "lua_assert%b();?", "")
  src = gsub(src, "default:\n}", "default:;\n}")
  src = gsub(src, "lua_lock%b();", "")
  src = gsub(src, "lua_unlock%b();", "")
  src = gsub(src, "luai_threadyield%b();", "")
  src = gsub(src, "luai_userstateopen%b();", "{}")
  src = gsub(src, "luai_userstate%w+%b();", "")
  src = gsub(src, "%(%(c==.*luaY_parser%)", "luaY_parser")
  src = gsub(src, "trydecpoint%(ls,seminfo%)",
		  "luaX_lexerror(ls,\"malformed number\",TK_NUMBER)")
  src = gsub(src, "int c=luaZ_lookahead%b();", "")
  src = gsub(src, "luaL_register%(L,[^,]*,co_funcs%);\nreturn 2;",
		  "return 1;")
  src = gsub(src, "getfuncname%b():", "NULL:")
  src = gsub(src, "getobjname%b():", "NULL:")
  src = gsub(src, "if%([^\n]*hookmask[^\n]*%)\n[^\n]*\n", "")
  src = gsub(src, "if%([^\n]*hookmask[^\n]*%)%b{}\n", "")
  src = gsub(src, "if%([^\n]*hookmask[^\n]*&&\n[^\n]*%b{}\n", "")
  src = gsub(src, "(twoto%b()%()", "%1(size_t)")
  src = gsub(src, "i<sizenode", "i<(int)sizenode")
  return gsub(src, "\n\n+", "\n")
end

local function strip_comments(src)
  return gsub(src, "/%*.-%*/", " ")
end

local function strip_whitespace(src)
  src = gsub(src, "^%s+", "")
  src = gsub(src, "%s*\n%s*", "\n")
  src = gsub(src, "[ \t]+", " ")
  src = gsub(src, "(%W) ", "%1")
  return gsub(src, " (%W)", "%1")
end

local function rename_tokens1(src)
  src = gsub(src, "getline", "getline_")
  src = gsub(src, "struct ([%w_]+)", "ZX%1")
  return gsub(src, "union ([%w_]+)", "ZY%1")
end

local function rename_tokens2(src)
  src = gsub(src, "ZX([%w_]+)", "struct %1")
  return gsub(src, "ZY([%w_]+)", "union %1")
end

local function func_gather(src)
  local nodes, list = {}, {}
  local pos, len = 1, #src
  while pos < len do
    local d, w = match(src, "^(#define ([%w_]+)[^\n]*\n)", pos)
    if d then
      local n = #list+1
      list[n] = d
      nodes[w] = n
    else
      local s
      d, w, s = match(src, "^(([%w_]+)[^\n]*([{;])\n)", pos)
      if not d then
	d, w, s = match(src, "^(([%w_]+)[^(]*%b()([{;])\n)", pos)
	if not d then d = match(src, "^[^\n]*\n", pos) end
      end
      if s == "{" then
	d = d..sub(match(src, "^%b{}[^;\n]*;?\n", pos+#d-2), 3)
	if sub(d, -2) == "{\n" then
	  d = d..sub(match(src, "^%b{}[^;\n]*;?\n", pos+#d-2), 3)
	end
      end
      local k, v = nil, d
      if w == "typedef" then
	if match(d, "^typedef enum") then
	  head[#head+1] = d
	else
	  k = match(d, "([%w_]+);\n$")
	  if not k then k = match(d, "^.-%(.-([%w_]+)%)%(") end
	end
      elseif w == "enum" then
	head[#head+1] = v
      elseif w ~= nil then
	k = match(d, "^[^\n]-([%w_]+)[(%[=]")
	if k then
	  if w ~= "static" and k ~= "main" then v = "static "..d end
	else
	  k = w
	end
      end
      if w and k then
	local o = nodes[k]
	if o then nodes["*"..k] = o end
	local n = #list+1
	list[n] = v
	nodes[k] = n
      end
    end
    pos = pos + #d
  end
  return nodes, list
end

local function func_visit(nodes, list, used, n)
  local i = nodes[n]
  for m in string.gmatch(list[i], "[%w_]+") do
    if nodes[m] then
      local j = used[m]
      if not j then
	used[m] = i
	func_visit(nodes, list, used, m)
      elseif i < j then
	used[m] = i
      end
    end
  end
end

local function func_collect(src)
  local nodes, list = func_gather(src)
  local used = {}
  func_visit(nodes, list, used, "main")
  for n,i in pairs(nodes) do
    local j = used[n]
    if j and j < i then used["*"..n] = j end
  end
  for n,i in pairs(nodes) do
    if not used[n] then list[i] = "" end
  end
  return table.concat(list)
end

find_sources()
local src = read_sources()
src = merge_includes(src)
local license = get_license(src)
src = fold_lines(src)
src = strip_unused1(src)
src = save_strings(src)
src = strip_unused2(src)
src = strip_comments(src)
src = preprocess(src)
src = strip_whitespace(src)
src = strip_unused3(src)
src = rename_tokens1(src)
src = func_collect(src)
src = rename_tokens2(src)
src = restore_strings(src)
src = merge_header(src, license)
io.write(src)
