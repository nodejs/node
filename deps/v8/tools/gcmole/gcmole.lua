-- Copyright 2011 the V8 project authors. All rights reserved.
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions are
-- met:
--
--     * Redistributions of source code must retain the above copyright
--       notice, this list of conditions and the following disclaimer.
--     * Redistributions in binary form must reproduce the above
--       copyright notice, this list of conditions and the following
--       disclaimer in the documentation and/or other materials provided
--       with the distribution.
--     * Neither the name of Google Inc. nor the names of its
--       contributors may be used to endorse or promote products derived
--       from this software without specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
-- "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
-- LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
-- A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
-- OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
-- SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
-- LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
-- DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
-- THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
-- (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
-- OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-- This is main driver for gcmole tool. See README for more details.
-- Usage: CLANG_BIN=clang-bin-dir lua tools/gcmole/gcmole.lua [arm|ia32|x64]

local DIR = arg[0]:match("^(.+)/[^/]+$")

local FLAGS = {
   -- Do not build gcsuspects file and reuse previously generated one.
   reuse_gcsuspects = false;

   -- Don't use parallel python runner.
   sequential = false;

   -- Print commands to console before executing them.
   verbose = false;

   -- Perform dead variable analysis (generates many false positives).
   -- TODO add some sort of whiteliste to filter out false positives.
   dead_vars = false;

   -- When building gcsuspects whitelist certain functions as if they
   -- can be causing GC. Currently used to reduce number of false
   -- positives in dead variables analysis. See TODO for WHITELIST
   -- below.
   whitelist = true;
}
local ARGS = {}

for i = 1, #arg do
   local flag = arg[i]:match "^%-%-([%w_-]+)$"
   if flag then
      local no, real_flag = flag:match "^(no)([%w_-]+)$"
      if real_flag then flag = real_flag end

      flag = flag:gsub("%-", "_")
      if FLAGS[flag] ~= nil then
         FLAGS[flag] = (no ~= "no")
      else
         error("Unknown flag: " .. flag)
      end
   else
      table.insert(ARGS, arg[i])
   end
end

local ARCHS = ARGS[1] and { ARGS[1] } or { 'ia32', 'arm', 'x64', 'arm64' }

local io = require "io"
local os = require "os"

function log(...)
   io.stderr:write(string.format(...))
   io.stderr:write "\n"
end

-------------------------------------------------------------------------------
-- Clang invocation

local CLANG_BIN = os.getenv "CLANG_BIN"
local CLANG_PLUGINS = os.getenv "CLANG_PLUGINS"

if not CLANG_BIN or CLANG_BIN == "" then
   error "CLANG_BIN not set"
end

if not CLANG_PLUGINS or CLANG_PLUGINS == "" then
   CLANG_PLUGINS = DIR
end

local function MakeClangCommandLine(
      plugin, plugin_args, triple, arch_define, arch_options)
   if plugin_args then
     for i = 1, #plugin_args do
        plugin_args[i] = "-Xclang -plugin-arg-" .. plugin
           .. " -Xclang " .. plugin_args[i]
     end
     plugin_args = " " .. table.concat(plugin_args, " ")
   end
   return CLANG_BIN .. "/clang++ -std=c++11 -c "
      .. " -Xclang -load -Xclang " .. CLANG_PLUGINS .. "/libgcmole.so"
      .. " -Xclang -plugin -Xclang "  .. plugin
      .. (plugin_args or "")
      .. " -Xclang -triple -Xclang " .. triple
      .. " -D" .. arch_define
      .. " -DENABLE_DEBUGGER_SUPPORT"
      .. " -DV8_INTL_SUPPORT"
      .. " -I./"
      .. " -Iinclude/"
      .. " -Iout/Release/gen"
      .. " -Ithird_party/icu/source/common"
      .. " -Ithird_party/icu/source/i18n"
      .. " " .. arch_options
end

local function IterTable(t)
  return coroutine.wrap(function ()
    for i, v in ipairs(t) do
      coroutine.yield(v)
    end
  end)
end

local function SplitResults(lines, func)
   -- Splits the output of parallel.py and calls func on each result.
   -- Bails out in case of an error in one of the executions.
   local current = {}
   local filename = ""
   for line in lines do
      local new_file = line:match "^______________ (.*)$"
      local code = line:match "^______________ finish (%d+) ______________$"
      if code then
         if tonumber(code) > 0 then
            log(table.concat(current, "\n"))
            log("Failed to examine " .. filename)
            return false
         end
         log("-- %s", filename)
         func(filename, IterTable(current))
      elseif new_file then
         filename = new_file
         current = {}
      else
         table.insert(current, line)
      end
   end
   return true
end

function InvokeClangPluginForEachFile(filenames, cfg, func)
   local cmd_line = MakeClangCommandLine(cfg.plugin,
                                         cfg.plugin_args,
                                         cfg.triple,
                                         cfg.arch_define,
                                         cfg.arch_options)
   if FLAGS.sequential then
      log("** Sequential execution.")
      for _, filename in ipairs(filenames) do
         log("-- %s", filename)
         local action = cmd_line .. " " .. filename .. " 2>&1"
         if FLAGS.verbose then print('popen ', action) end
         local pipe = io.popen(action)
         func(filename, pipe:lines())
         local success = pipe:close()
         if not success then error("Failed to run: " .. action) end
      end
   else
      log("** Parallel execution.")
      local action = "python tools/gcmole/parallel.py \""
         .. cmd_line .. "\" " .. table.concat(filenames, " ")
      if FLAGS.verbose then print('popen ', action) end
      local pipe = io.popen(action)
      local success = SplitResults(pipe:lines(), func)
      local closed = pipe:close()
      if not (success and closed) then error("Failed to run: " .. action) end
   end
end

-------------------------------------------------------------------------------

local function ParseGNFile(for_test)
   local result = {}
   local gn_files
   if for_test then
      gn_files = {
         { "tools/gcmole/GCMOLE.gn",             '"([^"]-%.cc)"',      ""         }
      }
   else
      gn_files = {
         { "BUILD.gn",             '"([^"]-%.cc)"',      ""         },
         { "test/cctest/BUILD.gn", '"(test-[^"]-%.cc)"', "test/cctest/" }
      }
   end

   for i = 1, #gn_files do
      local filename = gn_files[i][1]
      local pattern = gn_files[i][2]
      local prefix = gn_files[i][3]
      local gn_file = assert(io.open(filename), "failed to open GN file")
      local gn = gn_file:read('*a')
      for condition, sources in
         gn:gmatch "### gcmole%((.-)%) ###(.-)%]" do
         if result[condition] == nil then result[condition] = {} end
         for file in sources:gmatch(pattern) do
            table.insert(result[condition], prefix .. file)
         end
      end
      gn_file:close()
   end

   return result
end

local function EvaluateCondition(cond, props)
   if cond == 'all' then return true end

   local p, v = cond:match "(%w+):(%w+)"

   assert(p and v, "failed to parse condition: " .. cond)
   assert(props[p] ~= nil, "undefined configuration property: " .. p)

   return props[p] == v
end

local function BuildFileList(sources, props)
   local list = {}
   for condition, files in pairs(sources) do
      if EvaluateCondition(condition, props) then
         for i = 1, #files do table.insert(list, files[i]) end
      end
   end
   return list
end


local gn_sources = ParseGNFile(false)
local gn_test_sources = ParseGNFile(true)

local function FilesForArch(arch)
   return BuildFileList(gn_sources, { os = 'linux',
                                      arch = arch,
                                      mode = 'debug',
                                      simulator = ''})
end

local function FilesForTest(arch)
   return BuildFileList(gn_test_sources, { os = 'linux',
                                      arch = arch,
                                      mode = 'debug',
                                      simulator = ''})
end

local mtConfig = {}

mtConfig.__index = mtConfig

local function config (t) return setmetatable(t, mtConfig) end

function mtConfig:extend(t)
   local e = {}
   for k, v in pairs(self) do e[k] = v end
   for k, v in pairs(t) do e[k] = v end
   return config(e)
end

local ARCHITECTURES = {
   ia32 = config { triple = "i586-unknown-linux",
                   arch_define = "V8_TARGET_ARCH_IA32",
                   arch_options = "-m32" },
   arm = config { triple = "i586-unknown-linux",
                  arch_define = "V8_TARGET_ARCH_ARM",
                  arch_options = "-m32" },
   x64 = config { triple = "x86_64-unknown-linux",
                  arch_define = "V8_TARGET_ARCH_X64",
                  arch_options = "" },
   arm64 = config { triple = "x86_64-unknown-linux",
                    arch_define = "V8_TARGET_ARCH_ARM64",
                    arch_options = "" },
}

-------------------------------------------------------------------------------
-- GCSuspects Generation

local gc, gc_caused, funcs

local WHITELIST = {
   -- The following functions call CEntryStub which is always present.
   "MacroAssembler.*CallRuntime",
   "CompileCallLoadPropertyWithInterceptor",
   "CallIC.*GenerateMiss",

   -- DirectCEntryStub is a special stub used on ARM.
   -- It is pinned and always present.
   "DirectCEntryStub.*GenerateCall",

   -- TODO GCMole currently is sensitive enough to understand that certain
   --      functions only cause GC and return Failure simulataneously.
   --      Callsites of such functions are safe as long as they are properly
   --      check return value and propagate the Failure to the caller.
   --      It should be possible to extend GCMole to understand this.
   "Heap.*AllocateFunctionPrototype",

   -- Ignore all StateTag methods.
   "StateTag",

   -- Ignore printing of elements transition.
   "PrintElementsTransition"
};

local function AddCause(name, cause)
   local t = gc_caused[name]
   if not t then
      t = {}
      gc_caused[name] = t
   end
   table.insert(t, cause)
end

local function resolve(name)
   local f = funcs[name]

   if not f then
      f = {}
      funcs[name] = f

      if name:match "Collect.*Garbage" then
         gc[name] = true
         AddCause(name, "<GC>")
      end

      if FLAGS.whitelist then
         for i = 1, #WHITELIST do
            if name:match(WHITELIST[i]) then
               gc[name] = false
            end
         end
      end
   end

    return f
end

local function parse (filename, lines)
   local scope

   for funcname in lines do
      if funcname:sub(1, 1) ~= '\t' then
         resolve(funcname)
         scope = funcname
      else
         local name = funcname:sub(2)
         resolve(name)[scope] = true
      end
   end
end

local function propagate ()
   log "** Propagating GC information"

   local function mark(from, callers)
      for caller, _ in pairs(callers) do
         if gc[caller] == nil then
            gc[caller] = true
            mark(caller, funcs[caller])
         end
         AddCause(caller, from)
      end
   end

   for funcname, callers in pairs(funcs) do
      if gc[funcname] then mark(funcname, callers) end
   end
end

local function GenerateGCSuspects(arch, files, cfg)
   -- Reset the global state.
   gc, gc_caused, funcs = {}, {}, {}

   log ("** Building GC Suspects for %s", arch)
   InvokeClangPluginForEachFile (files,
                                 cfg:extend { plugin = "dump-callees" },
                                 parse)

   propagate()

   local out = assert(io.open("gcsuspects", "w"))
   for name, value in pairs(gc) do if value then out:write (name, '\n') end end
   out:close()

   local out = assert(io.open("gccauses", "w"))
   out:write "GC = {"
   for name, causes in pairs(gc_caused) do
      out:write("['", name, "'] = {")
      for i = 1, #causes do out:write ("'", causes[i], "';") end
      out:write("};\n")
   end
   out:write "}"
   out:close()

   log ("** GCSuspects generated for %s", arch)
end

--------------------------------------------------------------------------------
-- Analysis

local function CheckCorrectnessForArch(arch, for_test)
   local files
   if for_test then
      files = FilesForTest(arch)
   else
      files = FilesForArch(arch)
   end
   local cfg = ARCHITECTURES[arch]

   if not FLAGS.reuse_gcsuspects then
      GenerateGCSuspects(arch, files, cfg)
   end

   local processed_files = 0
   local errors_found = false
   local output = ""
   local function SearchForErrors(filename, lines)
      processed_files = processed_files + 1
      for l in lines do
         errors_found = errors_found or
            l:match "^[^:]+:%d+:%d+:" or
            l:match "error" or
            l:match "warning"
         if for_test then
            output = output.."\n"..l
         else
            print(l)
         end
      end
   end

   log("** Searching for evaluation order problems%s for %s",
       FLAGS.dead_vars and " and dead variables" or "",
       arch)
   local plugin_args
   if FLAGS.dead_vars then plugin_args = { "--dead-vars" } end
   InvokeClangPluginForEachFile(files,
                                cfg:extend { plugin = "find-problems",
                                             plugin_args = plugin_args },
                                SearchForErrors)
   log("** Done processing %d files. %s",
       processed_files,
       errors_found and "Errors found" or "No errors found")

   return errors_found, output
end

local function SafeCheckCorrectnessForArch(arch, for_test)
   local status, errors, output = pcall(CheckCorrectnessForArch, arch, for_test)
   if not status then
      print(string.format("There was an error: %s", errors))
      errors = true
   end
   return errors, output
end

local function TestRun()
   local errors, output = SafeCheckCorrectnessForArch('x64', true)

   local filename = "tools/gcmole/test-expectations.txt"
   local exp_file = assert(io.open(filename), "failed to open test expectations file")
   local expectations = exp_file:read('*all')

   if output ~= expectations then
      log("** Output mismatch from running tests. Please run them manually.")
   else
      log("** Tests ran successfully")
   end
end

TestRun()

local errors = false

for _, arch in ipairs(ARCHS) do
   if not ARCHITECTURES[arch] then
      error ("Unknown arch: " .. arch)
   end

   errors = SafeCheckCorrectnessForArch(arch, false) or errors
end

os.exit(errors and 1 or 0)
