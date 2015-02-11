#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import js2c
import multiprocessing
import optparse
import os
import random
import re
import shutil
import signal
import string
import subprocess
import sys
import time

FILENAME = "src/runtime.cc"
HEADERFILENAME = "src/runtime.h"
FUNCTION = re.compile("^RUNTIME_FUNCTION\(Runtime_(\w+)")
ARGSLENGTH = re.compile(".*DCHECK\(.*args\.length\(\) == (\d+)\);")
FUNCTIONEND = "}\n"
MACRO = re.compile(r"^#define ([^ ]+)\(([^)]*)\) *([^\\]*)\\?\n$")
FIRST_WORD = re.compile("^\s*(.*?)[\s({\[]")

WORKSPACE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
BASEPATH = os.path.join(WORKSPACE, "test", "mjsunit", "runtime-gen")
THIS_SCRIPT = os.path.relpath(sys.argv[0])

# Expand these macros, they define further runtime functions.
EXPAND_MACROS = [
  "BUFFER_VIEW_GETTER",
  "DATA_VIEW_GETTER",
  "DATA_VIEW_SETTER",
  "RUNTIME_UNARY_MATH",
]
# TODO(jkummerow): We could also whitelist the following macros, but the
# functions they define are so trivial that it's unclear how much benefit
# that would provide:
# ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION
# FIXED_TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION
# TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION

# Counts of functions in each detection state. These are used to assert
# that the parser doesn't bit-rot. Change the values as needed when you add,
# remove or change runtime functions, but make sure we don't lose our ability
# to parse them!
EXPECTED_FUNCTION_COUNT = 428
EXPECTED_FUZZABLE_COUNT = 331
EXPECTED_CCTEST_COUNT = 7
EXPECTED_UNKNOWN_COUNT = 16
EXPECTED_BUILTINS_COUNT = 809


# Don't call these at all.
BLACKLISTED = [
  "Abort",  # Kills the process.
  "AbortJS",  # Kills the process.
  "CompileForOnStackReplacement",  # Riddled with DCHECK.
  "IS_VAR",  # Not implemented in the runtime.
  "ListNatives",  # Not available in Release mode.
  "SetAllocationTimeout",  # Too slow for fuzzing.
  "SystemBreak",  # Kills (int3) the process.

  # These are weird. They violate some invariants when called after
  # bootstrapping.
  "DisableAccessChecks",
  "EnableAccessChecks",

  # The current LiveEdit implementation relies on and messes with internals
  # in ways that makes it fundamentally unfuzzable :-(
  "DebugGetLoadedScripts",
  "DebugSetScriptSource",
  "LiveEditFindSharedFunctionInfosForScript",
  "LiveEditFunctionSourceUpdated",
  "LiveEditGatherCompileInfo",
  "LiveEditPatchFunctionPositions",
  "LiveEditReplaceFunctionCode",
  "LiveEditReplaceRefToNestedFunction",
  "LiveEditReplaceScript",
  "LiveEditRestartFrame",
  "SetScriptBreakPoint",

  # TODO(jkummerow): Fix these and un-blacklist them!
  "CreateDateTimeFormat",
  "CreateNumberFormat",

  # TODO(danno): Fix these internal function that are only callable form stubs
  # and un-blacklist them!
  "NumberToString",
  "RxegExpConstructResult",
  "RegExpExec",
  "StringAdd",
  "SubString",
  "StringCompare",
  "StringCharCodeAt",
  "GetFromCache",

  # Compilation
  "CompileUnoptimized",
  "CompileOptimized",
  "TryInstallOptimizedCode",
  "NotifyDeoptimized",
  "NotifyStubFailure",

  # Utilities
  "AllocateInNewSpace",
  "AllocateInTargetSpace",
  "AllocateHeapNumber",
  "NumberToSmi",
  "NumberToStringSkipCache",

  "NewSloppyArguments",
  "NewStrictArguments",

  # Harmony
  "CreateJSGeneratorObject",
  "SuspendJSGeneratorObject",
  "ResumeJSGeneratorObject",
  "ThrowGeneratorStateError",

  # Arrays
  "ArrayConstructor",
  "InternalArrayConstructor",
  "NormalizeElements",

  # Literals
  "MaterializeRegExpLiteral",
  "CreateObjectLiteral",
  "CreateArrayLiteral",
  "CreateArrayLiteralStubBailout",

  # Statements
  "NewClosure",
  "NewClosureFromStubFailure",
  "NewObject",
  "NewObjectWithAllocationSite",
  "FinalizeInstanceSize",
  "Throw",
  "ReThrow",
  "ThrowReferenceError",
  "ThrowNotDateError",
  "StackGuard",
  "Interrupt",
  "PromoteScheduledException",

  # Contexts
  "NewGlobalContext",
  "NewFunctionContext",
  "PushWithContext",
  "PushCatchContext",
  "PushBlockContext",
  "PushModuleContext",
  "DeleteLookupSlot",
  "LoadLookupSlot",
  "LoadLookupSlotNoReferenceError",
  "StoreLookupSlot",

  # Declarations
  "DeclareGlobals",
  "DeclareModules",
  "DeclareContextSlot",
  "InitializeConstGlobal",
  "InitializeConstContextSlot",

  # Eval
  "ResolvePossiblyDirectEval",

  # Maths
  "MathPowSlow",
  "MathPowRT"
]


# These will always throw.
THROWS = [
  "CheckExecutionState",  # Needs to hit a break point.
  "CheckIsBootstrapping",  # Needs to be bootstrapping.
  "DebugEvaluate",  # Needs to hit a break point.
  "DebugEvaluateGlobal",  # Needs to hit a break point.
  "DebugIndexedInterceptorElementValue",  # Needs an indexed interceptor.
  "DebugNamedInterceptorPropertyValue",  # Needs a named interceptor.
  "DebugSetScriptSource",  # Checks compilation state of script.
  "GetAllScopesDetails",  # Needs to hit a break point.
  "GetFrameCount",  # Needs to hit a break point.
  "GetFrameDetails",  # Needs to hit a break point.
  "GetRootNaN",  # Needs to be bootstrapping.
  "GetScopeCount",  # Needs to hit a break point.
  "GetScopeDetails",  # Needs to hit a break point.
  "GetStepInPositions",  # Needs to hit a break point.
  "GetTemplateField",  # Needs a {Function,Object}TemplateInfo.
  "GetThreadCount",  # Needs to hit a break point.
  "GetThreadDetails",  # Needs to hit a break point.
  "IsAccessAllowedForObserver",  # Needs access-check-required object.
  "UnblockConcurrentRecompilation"  # Needs --block-concurrent-recompilation.
]


# Definitions used in CUSTOM_KNOWN_GOOD_INPUT below.
_BREAK_ITERATOR = (
    "%GetImplFromInitializedIntlObject(new Intl.v8BreakIterator())")
_COLLATOR = "%GetImplFromInitializedIntlObject(new Intl.Collator('en-US'))"
_DATETIME_FORMAT = (
    "%GetImplFromInitializedIntlObject(new Intl.DateTimeFormat('en-US'))")
_NUMBER_FORMAT = (
    "%GetImplFromInitializedIntlObject(new Intl.NumberFormat('en-US'))")


# Custom definitions for function input that does not throw.
# Format: "FunctionName": ["arg0", "arg1", ..., argslength].
# None means "fall back to autodetected value".
CUSTOM_KNOWN_GOOD_INPUT = {
  "AddNamedProperty": [None, "\"bla\"", None, None, None],
  "AddPropertyForTemplate": [None, 10, None, None, None],
  "Apply": ["function() {}", None, None, None, None, None],
  "ArrayBufferSliceImpl": [None, None, 0, None],
  "ArrayConcat": ["[1, 'a']", None],
  "BreakIteratorAdoptText": [_BREAK_ITERATOR, None, None],
  "BreakIteratorBreakType": [_BREAK_ITERATOR, None],
  "BreakIteratorCurrent": [_BREAK_ITERATOR, None],
  "BreakIteratorFirst": [_BREAK_ITERATOR, None],
  "BreakIteratorNext": [_BREAK_ITERATOR, None],
  "CompileString": [None, "false", None],
  "CreateBreakIterator": ["'en-US'", "{type: 'string'}", None, None],
  "CreateJSFunctionProxy": [None, "function() {}", None, None, None],
  "CreatePrivateSymbol": ["\"foo\"", None],
  "CreateSymbol": ["\"foo\"", None],
  "DateParseString": [None, "new Array(8)", None],
  "DefineAccessorPropertyUnchecked": [None, None, "function() {}",
                                      "function() {}", 2, None],
  "FunctionBindArguments": [None, None, "undefined", None, None],
  "GetBreakLocations": [None, 0, None],
  "GetDefaultReceiver": ["function() {}", None],
  "GetImplFromInitializedIntlObject": ["new Intl.NumberFormat('en-US')", None],
  "InternalCompare": [_COLLATOR, None, None, None],
  "InternalDateFormat": [_DATETIME_FORMAT, None, None],
  "InternalDateParse": [_DATETIME_FORMAT, None, None],
  "InternalNumberFormat": [_NUMBER_FORMAT, None, None],
  "InternalNumberParse": [_NUMBER_FORMAT, None, None],
  "IsSloppyModeFunction": ["function() {}", None],
  "LoadMutableDouble": ["{foo: 1.2}", None, None],
  "NewObjectFromBound": ["(function() {}).bind({})", None],
  "NumberToRadixString": [None, "2", None],
  "ParseJson": ["\"{}\"", 1],
  "RegExpExecMultiple": [None, None, "['a']", "['a']", None],
  "DefineApiAccessorProperty": [None, None, "undefined", "undefined", None, None],
  "SetIteratorInitialize": [None, None, "2", None],
  "SetDebugEventListener": ["undefined", None, None],
  "SetFunctionBreakPoint": [None, 218, None, None],
  "StringBuilderConcat": ["[1, 2, 3]", 3, None, None],
  "StringBuilderJoin": ["['a', 'b']", 4, None, None],
  "StringMatch": [None, None, "['a', 'b']", None],
  "StringNormalize": [None, 2, None],
  "StringReplaceGlobalRegExpWithString": [None, None, None, "['a']", None],
  "TypedArrayInitialize": [None, 6, "new ArrayBuffer(8)", None, 4, None],
  "TypedArrayInitializeFromArrayLike": [None, 6, None, None, None],
  "TypedArraySetFastCases": [None, None, "0", None],
  "FunctionIsArrow": ["() => null", None],
}


# Types of arguments that cannot be generated in a JavaScript testcase.
NON_JS_TYPES = [
  "Code", "Context", "FixedArray", "FunctionTemplateInfo",
  "JSFunctionResultCache", "JSMessageObject", "Map", "ScopeInfo",
  "SharedFunctionInfo"]


class Generator(object):

  def RandomVariable(self, varname, vartype, simple):
    if simple:
      return self._Variable(varname, self.GENERATORS[vartype][0])
    return self.GENERATORS[vartype][1](self, varname,
                                       self.DEFAULT_RECURSION_BUDGET)

  @staticmethod
  def IsTypeSupported(typename):
    return typename in Generator.GENERATORS

  USUAL_SUSPECT_PROPERTIES = ["size", "length", "byteLength", "__proto__",
                              "prototype", "0", "1", "-1"]
  DEFAULT_RECURSION_BUDGET = 2
  PROXY_TRAPS = """{
      getOwnPropertyDescriptor: function(name) {
        return {value: function() {}, configurable: true, writable: true,
                enumerable: true};
      },
      getPropertyDescriptor: function(name) {
        return {value: function() {}, configurable: true, writable: true,
                enumerable: true};
      },
      getOwnPropertyNames: function() { return []; },
      getPropertyNames: function() { return []; },
      defineProperty: function(name, descriptor) {},
      delete: function(name) { return true; },
      fix: function() {}
    }"""

  def _Variable(self, name, value, fallback=None):
    args = { "name": name, "value": value, "fallback": fallback }
    if fallback:
      wrapper = "try { %%s } catch(e) { var %(name)s = %(fallback)s; }" % args
    else:
      wrapper = "%s"
    return [wrapper % ("var %(name)s = %(value)s;" % args)]

  def _Boolean(self, name, recursion_budget):
    return self._Variable(name, random.choice(["true", "false"]))

  def _Oddball(self, name, recursion_budget):
    return self._Variable(name,
                          random.choice(["true", "false", "undefined", "null"]))

  def _StrictMode(self, name, recursion_budget):
    return self._Variable(name, random.choice([0, 1]))

  def _Int32(self, name, recursion_budget=0):
    die = random.random()
    if die < 0.5:
      value = random.choice([-3, -1, 0, 1, 2, 10, 515, 0x3fffffff, 0x7fffffff,
                             0x40000000, -0x40000000, -0x80000000])
    elif die < 0.75:
      value = random.randint(-1000, 1000)
    else:
      value = random.randint(-0x80000000, 0x7fffffff)
    return self._Variable(name, value)

  def _Uint32(self, name, recursion_budget=0):
    die = random.random()
    if die < 0.5:
      value = random.choice([0, 1, 2, 3, 4, 8, 0x3fffffff, 0x40000000,
                             0x7fffffff, 0xffffffff])
    elif die < 0.75:
      value = random.randint(0, 1000)
    else:
      value = random.randint(0, 0xffffffff)
    return self._Variable(name, value)

  def _Smi(self, name, recursion_budget):
    die = random.random()
    if die < 0.5:
      value = random.choice([-5, -1, 0, 1, 2, 3, 0x3fffffff, -0x40000000])
    elif die < 0.75:
      value = random.randint(-1000, 1000)
    else:
      value = random.randint(-0x40000000, 0x3fffffff)
    return self._Variable(name, value)

  def _Number(self, name, recursion_budget):
    die = random.random()
    if die < 0.5:
      return self._Smi(name, recursion_budget)
    elif die < 0.6:
      value = random.choice(["Infinity", "-Infinity", "NaN", "-0",
                             "1.7976931348623157e+308",  # Max value.
                             "2.2250738585072014e-308",  # Min value.
                             "4.9406564584124654e-324"])  # Min subnormal.
    else:
      value = random.lognormvariate(0, 15)
    return self._Variable(name, value)

  def _RawRandomString(self, minlength=0, maxlength=100,
                       alphabet=string.ascii_letters):
    length = random.randint(minlength, maxlength)
    result = ""
    for i in xrange(length):
      result += random.choice(alphabet)
    return result

  def _SeqString(self, name, recursion_budget):
    s1 = self._RawRandomString(1, 5)
    s2 = self._RawRandomString(1, 5)
    # 'foo' + 'bar'
    return self._Variable(name, "\"%s\" + \"%s\"" % (s1, s2))

  def _SeqTwoByteString(self, name):
    s1 = self._RawRandomString(1, 5)
    s2 = self._RawRandomString(1, 5)
    # 'foo' + unicode + 'bar'
    return self._Variable(name, "\"%s\" + \"\\2082\" + \"%s\"" % (s1, s2))

  def _SlicedString(self, name):
    s = self._RawRandomString(20, 30)
    # 'ffoo12345678901234567890'.substr(1)
    return self._Variable(name, "\"%s\".substr(1)" % s)

  def _ConsString(self, name):
    s1 = self._RawRandomString(8, 15)
    s2 = self._RawRandomString(8, 15)
    # 'foo12345' + (function() { return 'bar12345';})()
    return self._Variable(name,
        "\"%s\" + (function() { return \"%s\";})()" % (s1, s2))

  def _InternalizedString(self, name):
    return self._Variable(name, "\"%s\"" % self._RawRandomString(0, 20))

  def _String(self, name, recursion_budget):
    die = random.random()
    if die < 0.5:
      string = random.choice(self.USUAL_SUSPECT_PROPERTIES)
      return self._Variable(name, "\"%s\"" % string)
    elif die < 0.6:
      number_name = name + "_number"
      result = self._Number(number_name, recursion_budget)
      return result + self._Variable(name, "\"\" + %s" % number_name)
    elif die < 0.7:
      return self._SeqString(name, recursion_budget)
    elif die < 0.8:
      return self._ConsString(name)
    elif die < 0.9:
      return self._InternalizedString(name)
    else:
      return self._SlicedString(name)

  def _Symbol(self, name, recursion_budget):
    raw_string_name = name + "_1"
    result = self._String(raw_string_name, recursion_budget)
    return result + self._Variable(name, "Symbol(%s)" % raw_string_name)

  def _Name(self, name, recursion_budget):
    if random.random() < 0.2:
      return self._Symbol(name, recursion_budget)
    return self._String(name, recursion_budget)

  def _JSValue(self, name, recursion_budget):
    die = random.random()
    raw_name = name + "_1"
    if die < 0.33:
      result = self._String(raw_name, recursion_budget)
      return result + self._Variable(name, "new String(%s)" % raw_name)
    elif die < 0.66:
      result = self._Boolean(raw_name, recursion_budget)
      return result + self._Variable(name, "new Boolean(%s)" % raw_name)
    else:
      result = self._Number(raw_name, recursion_budget)
      return result + self._Variable(name, "new Number(%s)" % raw_name)

  def _RawRandomPropertyName(self):
    if random.random() < 0.5:
      return random.choice(self.USUAL_SUSPECT_PROPERTIES)
    return self._RawRandomString(0, 10)

  def _AddProperties(self, name, result, recursion_budget):
    propcount = random.randint(0, 3)
    propname = None
    for i in range(propcount):
      die = random.random()
      if die < 0.5:
        propname = "%s_prop%d" % (name, i)
        result += self._Name(propname, recursion_budget - 1)
      else:
        propname = "\"%s\"" % self._RawRandomPropertyName()
      propvalue_name = "%s_val%d" % (name, i)
      result += self._Object(propvalue_name, recursion_budget - 1)
      result.append("try { %s[%s] = %s; } catch (e) {}" %
                    (name, propname, propvalue_name))
    if random.random() < 0.2 and propname:
      # Force the object to slow mode.
      result.append("delete %s[%s];" % (name, propname))

  def _RandomElementIndex(self, element_name, result):
    if random.random() < 0.5:
      return random.randint(-1000, 1000)
    result += self._Smi(element_name, 0)
    return element_name

  def _AddElements(self, name, result, recursion_budget):
    elementcount = random.randint(0, 3)
    for i in range(elementcount):
      element_name = "%s_idx%d" % (name, i)
      index = self._RandomElementIndex(element_name, result)
      value_name = "%s_elt%d" % (name, i)
      result += self._Object(value_name, recursion_budget - 1)
      result.append("try { %s[%s] = %s; } catch(e) {}" %
                    (name, index, value_name))

  def _AddAccessors(self, name, result, recursion_budget):
    accessorcount = random.randint(0, 3)
    for i in range(accessorcount):
      propname = self._RawRandomPropertyName()
      what = random.choice(["get", "set"])
      function_name = "%s_access%d" % (name, i)
      result += self._PlainFunction(function_name, recursion_budget - 1)
      result.append("try { Object.defineProperty(%s, \"%s\", {%s: %s}); } "
                    "catch (e) {}" % (name, propname, what, function_name))

  def _PlainArray(self, name, recursion_budget):
    die = random.random()
    if die < 0.5:
      literal = random.choice(["[]", "[1, 2]", "[1.5, 2.5]",
                               "['a', 'b', 1, true]"])
      return self._Variable(name, literal)
    else:
      new = random.choice(["", "new "])
      length = random.randint(0, 101000)
      return self._Variable(name, "%sArray(%d)" % (new, length))

  def _PlainObject(self, name, recursion_budget):
    die = random.random()
    if die < 0.67:
      literal_propcount = random.randint(0, 3)
      properties = []
      result = []
      for i in range(literal_propcount):
        propname = self._RawRandomPropertyName()
        propvalue_name = "%s_lit%d" % (name, i)
        result += self._Object(propvalue_name, recursion_budget - 1)
        properties.append("\"%s\": %s" % (propname, propvalue_name))
      return result + self._Variable(name, "{%s}" % ", ".join(properties))
    else:
      return self._Variable(name, "new Object()")

  def _JSArray(self, name, recursion_budget):
    result = self._PlainArray(name, recursion_budget)
    self._AddAccessors(name, result, recursion_budget)
    self._AddProperties(name, result, recursion_budget)
    self._AddElements(name, result, recursion_budget)
    return result

  def _RawRandomBufferLength(self):
    if random.random() < 0.2:
      return random.choice([0, 1, 8, 0x40000000, 0x80000000])
    return random.randint(0, 1000)

  def _JSArrayBuffer(self, name, recursion_budget):
    length = self._RawRandomBufferLength()
    return self._Variable(name, "new ArrayBuffer(%d)" % length)

  def _JSDataView(self, name, recursion_budget):
    buffer_name = name + "_buffer"
    result = self._JSArrayBuffer(buffer_name, recursion_budget)
    args = [buffer_name]
    die = random.random()
    if die < 0.67:
      offset = self._RawRandomBufferLength()
      args.append("%d" % offset)
      if die < 0.33:
        length = self._RawRandomBufferLength()
        args.append("%d" % length)
    result += self._Variable(name, "new DataView(%s)" % ", ".join(args),
                             fallback="new DataView(new ArrayBuffer(8))")
    return result

  def _JSDate(self, name, recursion_budget):
    die = random.random()
    if die < 0.25:
      return self._Variable(name, "new Date()")
    elif die < 0.5:
      ms_name = name + "_ms"
      result = self._Number(ms_name, recursion_budget)
      return result + self._Variable(name, "new Date(%s)" % ms_name)
    elif die < 0.75:
      str_name = name + "_str"
      month = random.choice(["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                             "Aug", "Sep", "Oct", "Nov", "Dec"])
      day = random.randint(1, 28)
      year = random.randint(1900, 2100)
      hour = random.randint(0, 23)
      minute = random.randint(0, 59)
      second = random.randint(0, 59)
      str_value = ("\"%s %s, %s %s:%s:%s\"" %
                   (month, day, year, hour, minute, second))
      result = self._Variable(str_name, str_value)
      return result + self._Variable(name, "new Date(%s)" % str_name)
    else:
      components = tuple(map(lambda x: "%s_%s" % (name, x),
                             ["y", "m", "d", "h", "min", "s", "ms"]))
      return ([j for i in map(self._Int32, components) for j in i] +
              self._Variable(name, "new Date(%s)" % ", ".join(components)))

  def _PlainFunction(self, name, recursion_budget):
    result_name = "result"
    body = ["function() {"]
    body += self._Object(result_name, recursion_budget - 1)
    body.append("return result;\n}")
    return self._Variable(name, "%s" % "\n".join(body))

  def _JSFunction(self, name, recursion_budget):
    result = self._PlainFunction(name, recursion_budget)
    self._AddAccessors(name, result, recursion_budget)
    self._AddProperties(name, result, recursion_budget)
    self._AddElements(name, result, recursion_budget)
    return result

  def _JSFunctionProxy(self, name, recursion_budget):
    # TODO(jkummerow): Revisit this as the Proxy implementation evolves.
    return self._Variable(name, "Proxy.createFunction(%s, function() {})" %
                                self.PROXY_TRAPS)

  def _JSGeneratorObject(self, name, recursion_budget):
    # TODO(jkummerow): Be more creative here?
    return self._Variable(name, "(function*() { yield 1; })()")

  def _JSMap(self, name, recursion_budget, weak=""):
    result = self._Variable(name, "new %sMap()" % weak)
    num_entries = random.randint(0, 3)
    for i in range(num_entries):
      key_name = "%s_k%d" % (name, i)
      value_name = "%s_v%d" % (name, i)
      if weak:
        result += self._JSObject(key_name, recursion_budget - 1)
      else:
        result += self._Object(key_name, recursion_budget - 1)
      result += self._Object(value_name, recursion_budget - 1)
      result.append("%s.set(%s, %s)" % (name, key_name, value_name))
    return result

  def _JSMapIterator(self, name, recursion_budget):
    map_name = name + "_map"
    result = self._JSMap(map_name, recursion_budget)
    iterator_type = random.choice(['keys', 'values', 'entries'])
    return (result + self._Variable(name, "%s.%s()" %
                                          (map_name, iterator_type)))

  def _JSProxy(self, name, recursion_budget):
    # TODO(jkummerow): Revisit this as the Proxy implementation evolves.
    return self._Variable(name, "Proxy.create(%s)" % self.PROXY_TRAPS)

  def _JSRegExp(self, name, recursion_budget):
    flags = random.choice(["", "g", "i", "m", "gi"])
    string = "a(b|c)*a"  # TODO(jkummerow): Be more creative here?
    ctor = random.choice(["/%s/%s", "new RegExp(\"%s\", \"%s\")"])
    return self._Variable(name, ctor % (string, flags))

  def _JSSet(self, name, recursion_budget, weak=""):
    result = self._Variable(name, "new %sSet()" % weak)
    num_entries = random.randint(0, 3)
    for i in range(num_entries):
      element_name = "%s_e%d" % (name, i)
      if weak:
        result += self._JSObject(element_name, recursion_budget - 1)
      else:
        result += self._Object(element_name, recursion_budget - 1)
      result.append("%s.add(%s)" % (name, element_name))
    return result

  def _JSSetIterator(self, name, recursion_budget):
    set_name = name + "_set"
    result = self._JSSet(set_name, recursion_budget)
    iterator_type = random.choice(['values', 'entries'])
    return (result + self._Variable(name, "%s.%s()" %
                                          (set_name, iterator_type)))

  def _JSTypedArray(self, name, recursion_budget):
    arraytype = random.choice(["Int8", "Int16", "Int32", "Uint8", "Uint16",
                               "Uint32", "Float32", "Float64", "Uint8Clamped"])
    ctor_type = random.randint(0, 3)
    if ctor_type == 0:
      length = random.randint(0, 1000)
      return self._Variable(name, "new %sArray(%d)" % (arraytype, length),
                            fallback="new %sArray(8)" % arraytype)
    elif ctor_type == 1:
      input_name = name + "_typedarray"
      result = self._JSTypedArray(input_name, recursion_budget - 1)
      return (result +
              self._Variable(name, "new %sArray(%s)" % (arraytype, input_name),
                             fallback="new %sArray(8)" % arraytype))
    elif ctor_type == 2:
      arraylike_name = name + "_arraylike"
      result = self._JSObject(arraylike_name, recursion_budget - 1)
      length = random.randint(0, 1000)
      result.append("try { %s.length = %d; } catch(e) {}" %
                    (arraylike_name, length))
      return (result +
              self._Variable(name,
                             "new %sArray(%s)" % (arraytype, arraylike_name),
                             fallback="new %sArray(8)" % arraytype))
    else:
      die = random.random()
      buffer_name = name + "_buffer"
      args = [buffer_name]
      result = self._JSArrayBuffer(buffer_name, recursion_budget)
      if die < 0.67:
        offset_name = name + "_offset"
        args.append(offset_name)
        result += self._Int32(offset_name)
      if die < 0.33:
        length_name = name + "_length"
        args.append(length_name)
        result += self._Int32(length_name)
      return (result +
              self._Variable(name,
                             "new %sArray(%s)" % (arraytype, ", ".join(args)),
                             fallback="new %sArray(8)" % arraytype))

  def _JSArrayBufferView(self, name, recursion_budget):
    if random.random() < 0.4:
      return self._JSDataView(name, recursion_budget)
    else:
      return self._JSTypedArray(name, recursion_budget)

  def _JSWeakCollection(self, name, recursion_budget):
    ctor = random.choice([self._JSMap, self._JSSet])
    return ctor(name, recursion_budget, weak="Weak")

  def _PropertyDetails(self, name, recursion_budget):
    # TODO(jkummerow): Be more clever here?
    return self._Int32(name)

  def _JSObject(self, name, recursion_budget):
    die = random.random()
    if die < 0.4:
      function = random.choice([self._PlainObject, self._PlainArray,
                                self._PlainFunction])
    elif die < 0.5:
      return self._Variable(name, "this")  # Global object.
    else:
      function = random.choice([self._JSArrayBuffer, self._JSDataView,
                                self._JSDate, self._JSFunctionProxy,
                                self._JSGeneratorObject, self._JSMap,
                                self._JSMapIterator, self._JSRegExp,
                                self._JSSet, self._JSSetIterator,
                                self._JSTypedArray, self._JSValue,
                                self._JSWeakCollection])
    result = function(name, recursion_budget)
    self._AddAccessors(name, result, recursion_budget)
    self._AddProperties(name, result, recursion_budget)
    self._AddElements(name, result, recursion_budget)
    return result

  def _JSReceiver(self, name, recursion_budget):
    if random.random() < 0.9: return self._JSObject(name, recursion_budget)
    return self._JSProxy(name, recursion_budget)

  def _HeapObject(self, name, recursion_budget):
    die = random.random()
    if die < 0.9: return self._JSReceiver(name, recursion_budget)
    elif die < 0.95: return  self._Oddball(name, recursion_budget)
    else: return self._Name(name, recursion_budget)

  def _Object(self, name, recursion_budget):
    if recursion_budget <= 0:
      function = random.choice([self._Oddball, self._Number, self._Name,
                                self._JSValue, self._JSRegExp])
      return function(name, recursion_budget)
    if random.random() < 0.2:
      return self._Smi(name, recursion_budget)
    return self._HeapObject(name, recursion_budget)

  GENERATORS = {
    "Boolean": ["true", _Boolean],
    "HeapObject": ["new Object()", _HeapObject],
    "Int32": ["32", _Int32],
    "JSArray": ["new Array()", _JSArray],
    "JSArrayBuffer": ["new ArrayBuffer(8)", _JSArrayBuffer],
    "JSArrayBufferView": ["new Int32Array(2)", _JSArrayBufferView],
    "JSDataView": ["new DataView(new ArrayBuffer(24))", _JSDataView],
    "JSDate": ["new Date()", _JSDate],
    "JSFunction": ["function() {}", _JSFunction],
    "JSFunctionProxy": ["Proxy.createFunction({}, function() {})",
                        _JSFunctionProxy],
    "JSGeneratorObject": ["(function*(){ yield 1; })()", _JSGeneratorObject],
    "JSMap": ["new Map()", _JSMap],
    "JSMapIterator": ["new Map().entries()", _JSMapIterator],
    "JSObject": ["new Object()", _JSObject],
    "JSProxy": ["Proxy.create({})", _JSProxy],
    "JSReceiver": ["new Object()", _JSReceiver],
    "JSRegExp": ["/ab/g", _JSRegExp],
    "JSSet": ["new Set()", _JSSet],
    "JSSetIterator": ["new Set().values()", _JSSetIterator],
    "JSTypedArray": ["new Int32Array(2)", _JSTypedArray],
    "JSValue": ["new String('foo')", _JSValue],
    "JSWeakCollection": ["new WeakMap()", _JSWeakCollection],
    "Name": ["\"name\"", _Name],
    "Number": ["1.5", _Number],
    "Object": ["new Object()", _Object],
    "PropertyDetails": ["513", _PropertyDetails],
    "SeqOneByteString": ["\"seq 1-byte\"", _SeqString],
    "SeqString": ["\"seqstring\"", _SeqString],
    "SeqTwoByteString": ["\"seq \\u2082-byte\"", _SeqTwoByteString],
    "Smi": ["1", _Smi],
    "StrictMode": ["1", _StrictMode],
    "String": ["\"foo\"", _String],
    "Symbol": ["Symbol(\"symbol\")", _Symbol],
    "Uint32": ["32", _Uint32],
  }


class ArgParser(object):
  def __init__(self, regex, ctor):
    self.regex = regex
    self.ArgCtor = ctor


class Arg(object):
  def __init__(self, typename, varname, index):
    self.type = typename
    self.name = "_%s" % varname
    self.index = index


class Function(object):
  def __init__(self, match):
    self.name = match.group(1)
    self.argslength = -1
    self.args = {}
    self.inline = ""

  handle_arg_parser = ArgParser(
      re.compile("^\s*CONVERT_ARG_HANDLE_CHECKED\((\w+), (\w+), (\d+)\)"),
      lambda match: Arg(match.group(1), match.group(2), int(match.group(3))))

  plain_arg_parser = ArgParser(
      re.compile("^\s*CONVERT_ARG_CHECKED\((\w+), (\w+), (\d+)\)"),
      lambda match: Arg(match.group(1), match.group(2), int(match.group(3))))

  number_handle_arg_parser = ArgParser(
      re.compile("^\s*CONVERT_NUMBER_ARG_HANDLE_CHECKED\((\w+), (\d+)\)"),
      lambda match: Arg("Number", match.group(1), int(match.group(2))))

  smi_arg_parser = ArgParser(
      re.compile("^\s*CONVERT_SMI_ARG_CHECKED\((\w+), (\d+)\)"),
      lambda match: Arg("Smi", match.group(1), int(match.group(2))))

  double_arg_parser = ArgParser(
      re.compile("^\s*CONVERT_DOUBLE_ARG_CHECKED\((\w+), (\d+)\)"),
      lambda match: Arg("Number", match.group(1), int(match.group(2))))

  number_arg_parser = ArgParser(
      re.compile(
          "^\s*CONVERT_NUMBER_CHECKED\(\w+, (\w+), (\w+), args\[(\d+)\]\)"),
      lambda match: Arg(match.group(2), match.group(1), int(match.group(3))))

  strict_mode_arg_parser = ArgParser(
      re.compile("^\s*CONVERT_STRICT_MODE_ARG_CHECKED\((\w+), (\d+)\)"),
      lambda match: Arg("StrictMode", match.group(1), int(match.group(2))))

  boolean_arg_parser = ArgParser(
      re.compile("^\s*CONVERT_BOOLEAN_ARG_CHECKED\((\w+), (\d+)\)"),
      lambda match: Arg("Boolean", match.group(1), int(match.group(2))))

  property_details_parser = ArgParser(
      re.compile("^\s*CONVERT_PROPERTY_DETAILS_CHECKED\((\w+), (\d+)\)"),
      lambda match: Arg("PropertyDetails", match.group(1), int(match.group(2))))

  arg_parsers = [handle_arg_parser, plain_arg_parser, number_handle_arg_parser,
                 smi_arg_parser,
                 double_arg_parser, number_arg_parser, strict_mode_arg_parser,
                 boolean_arg_parser, property_details_parser]

  def SetArgsLength(self, match):
    self.argslength = int(match.group(1))

  def TryParseArg(self, line):
    for parser in Function.arg_parsers:
      match = parser.regex.match(line)
      if match:
        arg = parser.ArgCtor(match)
        self.args[arg.index] = arg
        return True
    return False

  def Filename(self):
    return "%s.js" % self.name.lower()

  def __str__(self):
    s = [self.name, "("]
    argcount = self.argslength
    if argcount < 0:
      print("WARNING: unknown argslength for function %s" % self.name)
      if self.args:
        argcount = max([self.args[i].index + 1 for i in self.args])
      else:
        argcount = 0
    for i in range(argcount):
      if i > 0: s.append(", ")
      s.append(self.args[i].type if i in self.args else "<unknown>")
    s.append(")")
    return "".join(s)


class Macro(object):
  def __init__(self, match):
    self.name = match.group(1)
    self.args = [s.strip() for s in match.group(2).split(",")]
    self.lines = []
    self.indentation = 0
    self.AddLine(match.group(3))

  def AddLine(self, line):
    if not line: return
    if not self.lines:
      # This is the first line, detect indentation.
      self.indentation = len(line) - len(line.lstrip())
    line = line.rstrip("\\\n ")
    if not line: return
    assert len(line[:self.indentation].strip()) == 0, \
        ("expected whitespace: '%s', full line: '%s'" %
         (line[:self.indentation], line))
    line = line[self.indentation:]
    if not line: return
    self.lines.append(line + "\n")

  def Finalize(self):
    for arg in self.args:
      pattern = re.compile(r"(##|\b)%s(##|\b)" % arg)
      for i in range(len(self.lines)):
        self.lines[i] = re.sub(pattern, "%%(%s)s" % arg, self.lines[i])

  def FillIn(self, arg_values):
    filler = {}
    assert len(arg_values) == len(self.args)
    for i in range(len(self.args)):
      filler[self.args[i]] = arg_values[i]
    result = []
    for line in self.lines:
      result.append(line % filler)
    return result


# Parses HEADERFILENAME to find out which runtime functions are "inline".
def FindInlineRuntimeFunctions():
  inline_functions = []
  with open(HEADERFILENAME, "r") as f:
    inline_list = "#define INLINE_FUNCTION_LIST(F) \\\n"
    inline_function = re.compile(r"^\s*F\((\w+), \d+, \d+\)\s*\\?")
    mode = "SEARCHING"
    for line in f:
      if mode == "ACTIVE":
        match = inline_function.match(line)
        if match:
          inline_functions.append(match.group(1))
        if not line.endswith("\\\n"):
          mode = "SEARCHING"
      elif mode == "SEARCHING":
        if line == inline_list:
          mode = "ACTIVE"
  return inline_functions


def ReadFileAndExpandMacros(filename):
  found_macros = {}
  expanded_lines = []
  with open(filename, "r") as f:
    found_macro = None
    for line in f:
      if found_macro is not None:
        found_macro.AddLine(line)
        if not line.endswith("\\\n"):
          found_macro.Finalize()
          found_macro = None
        continue

      match = MACRO.match(line)
      if match:
        found_macro = Macro(match)
        if found_macro.name in EXPAND_MACROS:
          found_macros[found_macro.name] = found_macro
        else:
          found_macro = None
        continue

      match = FIRST_WORD.match(line)
      if match:
        first_word = match.group(1)
        if first_word in found_macros:
          MACRO_CALL = re.compile("%s\(([^)]*)\)" % first_word)
          match = MACRO_CALL.match(line)
          assert match
          args = [s.strip() for s in match.group(1).split(",")]
          expanded_lines += found_macros[first_word].FillIn(args)
          continue

      expanded_lines.append(line)
  return expanded_lines


# Detects runtime functions by parsing FILENAME.
def FindRuntimeFunctions():
  inline_functions = FindInlineRuntimeFunctions()
  functions = []
  expanded_lines = ReadFileAndExpandMacros(FILENAME)
  function = None
  partial_line = ""
  for line in expanded_lines:
    # Multi-line definition support, ignoring macros.
    if line.startswith("RUNTIME_FUNCTION") and not line.endswith("{\n"):
      if line.endswith("\\\n"): continue
      partial_line = line.rstrip()
      continue
    if partial_line:
      partial_line += " " + line.strip()
      if partial_line.endswith("{"):
        line = partial_line
        partial_line = ""
      else:
        continue

    match = FUNCTION.match(line)
    if match:
      function = Function(match)
      if function.name in inline_functions:
        function.inline = "_"
      continue
    if function is None: continue

    match = ARGSLENGTH.match(line)
    if match:
      function.SetArgsLength(match)
      continue

    if function.TryParseArg(line):
      continue

    if line == FUNCTIONEND:
      if function is not None:
        functions.append(function)
        function = None
  return functions


# Hack: This must have the same fields as class Function above, because the
# two are used polymorphically in RunFuzzer(). We could use inheritance...
class Builtin(object):
  def __init__(self, match):
    self.name = match.group(1)
    args = match.group(2)
    self.argslength = 0 if args == "" else args.count(",") + 1
    self.inline = ""
    self.args = {}
    if self.argslength > 0:
      args = args.split(",")
      for i in range(len(args)):
        # a = args[i].strip()  # TODO: filter out /* comments */ first.
        a = ""
        self.args[i] = Arg("Object", a, i)

  def __str__(self):
    return "%s(%d)" % (self.name, self.argslength)


def FindJSBuiltins():
  PATH = "src"
  fileslist = []
  for (root, dirs, files) in os.walk(PATH):
    for f in files:
      if f.endswith(".js"):
        fileslist.append(os.path.join(root, f))
  builtins = []
  regexp = re.compile("^function (\w+)\s*\((.*?)\) {")
  matches = 0
  for filename in fileslist:
    with open(filename, "r") as f:
      file_contents = f.read()
    file_contents = js2c.ExpandInlineMacros(file_contents)
    lines = file_contents.split("\n")
    partial_line = ""
    for line in lines:
      if line.startswith("function") and not '{' in line:
        partial_line += line.rstrip()
        continue
      if partial_line:
        partial_line += " " + line.strip()
        if '{' in line:
          line = partial_line
          partial_line = ""
        else:
          continue
      match = regexp.match(line)
      if match:
        builtins.append(Builtin(match))
  return builtins


# Classifies runtime functions.
def ClassifyFunctions(functions):
  # Can be fuzzed with a JavaScript testcase.
  js_fuzzable_functions = []
  # We have enough information to fuzz these, but they need inputs that
  # cannot be created or passed around in JavaScript.
  cctest_fuzzable_functions = []
  # This script does not have enough information about these.
  unknown_functions = []

  types = {}
  for f in functions:
    if f.name in BLACKLISTED:
      continue
    decision = js_fuzzable_functions
    custom = CUSTOM_KNOWN_GOOD_INPUT.get(f.name, None)
    if f.argslength < 0:
      # Unknown length -> give up unless there's a custom definition.
      if custom and custom[-1] is not None:
        f.argslength = custom[-1]
        assert len(custom) == f.argslength + 1, \
            ("%s: last custom definition must be argslength" % f.name)
      else:
        decision = unknown_functions
    else:
      if custom:
        # Any custom definitions must match the known argslength.
        assert len(custom) == f.argslength + 1, \
            ("%s should have %d custom definitions but has %d" %
            (f.name, f.argslength + 1, len(custom)))
      for i in range(f.argslength):
        if custom and custom[i] is not None:
          # All good, there's a custom definition.
          pass
        elif not i in f.args:
          # No custom definition and no parse result -> give up.
          decision = unknown_functions
        else:
          t = f.args[i].type
          if t in NON_JS_TYPES:
            decision = cctest_fuzzable_functions
          else:
            assert Generator.IsTypeSupported(t), \
                ("type generator not found for %s, function: %s" % (t, f))
    decision.append(f)
  return (js_fuzzable_functions, cctest_fuzzable_functions, unknown_functions)


def _GetKnownGoodArgs(function, generator):
  custom_input = CUSTOM_KNOWN_GOOD_INPUT.get(function.name, None)
  definitions = []
  argslist = []
  for i in range(function.argslength):
    if custom_input and custom_input[i] is not None:
      name = "arg%d" % i
      definitions.append("var %s = %s;" % (name, custom_input[i]))
    else:
      arg = function.args[i]
      name = arg.name
      definitions += generator.RandomVariable(name, arg.type, simple=True)
    argslist.append(name)
  return (definitions, argslist)


def _GenerateTestcase(function, definitions, argslist, throws):
  s = ["// Copyright 2014 the V8 project authors. All rights reserved.",
       "// AUTO-GENERATED BY tools/generate-runtime-tests.py, DO NOT MODIFY",
       "// Flags: --allow-natives-syntax --harmony --harmony-proxies"
      ] + definitions
  call = "%%%s%s(%s);" % (function.inline, function.name, ", ".join(argslist))
  if throws:
    s.append("try {")
    s.append(call);
    s.append("} catch(e) {}")
  else:
    s.append(call)
  testcase = "\n".join(s)
  return testcase


def GenerateJSTestcaseForFunction(function):
  gen = Generator()
  (definitions, argslist) = _GetKnownGoodArgs(function, gen)
  testcase = _GenerateTestcase(function, definitions, argslist,
                               function.name in THROWS)
  path = os.path.join(BASEPATH, function.Filename())
  with open(path, "w") as f:
    f.write("%s\n" % testcase)


def GenerateTestcases(functions):
  shutil.rmtree(BASEPATH)  # Re-generate everything.
  os.makedirs(BASEPATH)
  for f in functions:
    GenerateJSTestcaseForFunction(f)


def _SaveFileName(save_path, process_id, save_file_index):
  return "%s/fuzz_%d_%d.js" % (save_path, process_id, save_file_index)


def _GetFuzzableRuntimeFunctions():
  functions = FindRuntimeFunctions()
  (js_fuzzable_functions, cctest_fuzzable_functions, unknown_functions) = \
      ClassifyFunctions(functions)
  return js_fuzzable_functions


FUZZ_TARGET_LISTS = {
  "runtime": _GetFuzzableRuntimeFunctions,
  "builtins": FindJSBuiltins,
}


def RunFuzzer(process_id, options, stop_running):
  MAX_SLEEP_TIME = 0.1
  INITIAL_SLEEP_TIME = 0.001
  SLEEP_TIME_FACTOR = 1.25
  base_file_name = "/dev/shm/runtime_fuzz_%d" % process_id
  test_file_name = "%s.js" % base_file_name
  stderr_file_name = "%s.out" % base_file_name
  save_file_index = 0
  while os.path.exists(_SaveFileName(options.save_path, process_id,
                                     save_file_index)):
    save_file_index += 1

  targets = FUZZ_TARGET_LISTS[options.fuzz_target]()
  try:
    for i in range(options.num_tests):
      if stop_running.is_set(): break
      function = None
      while function is None or function.argslength == 0:
        function = random.choice(targets)
      args = []
      definitions = []
      gen = Generator()
      for i in range(function.argslength):
        arg = function.args[i]
        argname = "arg%d%s" % (i, arg.name)
        args.append(argname)
        definitions += gen.RandomVariable(argname, arg.type, simple=False)
      testcase = _GenerateTestcase(function, definitions, args, True)
      with open(test_file_name, "w") as f:
        f.write("%s\n" % testcase)
      with open("/dev/null", "w") as devnull:
        with open(stderr_file_name, "w") as stderr:
          process = subprocess.Popen(
              [options.binary, "--allow-natives-syntax", "--harmony",
               "--harmony-proxies", "--enable-slow-asserts", test_file_name],
              stdout=devnull, stderr=stderr)
          end_time = time.time() + options.timeout
          timed_out = False
          exit_code = None
          sleep_time = INITIAL_SLEEP_TIME
          while exit_code is None:
            if time.time() >= end_time:
              # Kill the process and wait for it to exit.
              os.kill(process.pid, signal.SIGTERM)
              exit_code = process.wait()
              timed_out = True
            else:
              exit_code = process.poll()
              time.sleep(sleep_time)
              sleep_time = sleep_time * SLEEP_TIME_FACTOR
              if sleep_time > MAX_SLEEP_TIME:
                sleep_time = MAX_SLEEP_TIME
      if exit_code != 0 and not timed_out:
        oom = False
        with open(stderr_file_name, "r") as stderr:
          for line in stderr:
            if line.strip() == "# Allocation failed - process out of memory":
              oom = True
              break
        if oom: continue
        save_name = _SaveFileName(options.save_path, process_id,
                                  save_file_index)
        shutil.copyfile(test_file_name, save_name)
        save_file_index += 1
  except KeyboardInterrupt:
    stop_running.set()
  finally:
    if os.path.exists(test_file_name):
      os.remove(test_file_name)
    if os.path.exists(stderr_file_name):
      os.remove(stderr_file_name)


def BuildOptionParser():
  usage = """Usage: %%prog [options] ACTION

where ACTION can be:

info      Print diagnostic info.
check     Check that runtime functions can be parsed as expected, and that
          test cases exist.
generate  Parse source code for runtime functions, and auto-generate
          test cases for them. Warning: this will nuke and re-create
          %(path)s.
fuzz      Generate fuzz tests, run them, save those that crashed (see options).
""" % {"path": os.path.relpath(BASEPATH)}

  o = optparse.OptionParser(usage=usage)
  o.add_option("--binary", default="out/x64.debug/d8",
               help="d8 binary used for running fuzz tests (default: %default)")
  o.add_option("--fuzz-target", default="runtime",
               help="Set of functions targeted by fuzzing. Allowed values: "
                    "%s (default: %%default)" % ", ".join(FUZZ_TARGET_LISTS))
  o.add_option("-n", "--num-tests", default=1000, type="int",
               help="Number of fuzz tests to generate per worker process"
                    " (default: %default)")
  o.add_option("--save-path", default="~/runtime_fuzz_output",
               help="Path to directory where failing tests will be stored"
                    " (default: %default)")
  o.add_option("--timeout", default=20, type="int",
               help="Timeout for each fuzz test (in seconds, default:"
                    "%default)")
  return o


def ProcessOptions(options, args):
  options.save_path = os.path.expanduser(options.save_path)
  if options.fuzz_target not in FUZZ_TARGET_LISTS:
    print("Invalid fuzz target: %s" % options.fuzz_target)
    return False
  if len(args) != 1 or args[0] == "help":
    return False
  return True


def Main():
  parser = BuildOptionParser()
  (options, args) = parser.parse_args()

  if not ProcessOptions(options, args):
    parser.print_help()
    return 1
  action = args[0]

  functions = FindRuntimeFunctions()
  (js_fuzzable_functions, cctest_fuzzable_functions, unknown_functions) = \
      ClassifyFunctions(functions)
  builtins = FindJSBuiltins()

  if action == "test":
    print("put your temporary debugging code here")
    return 0

  if action == "info":
    print("%d functions total; js_fuzzable_functions: %d, "
          "cctest_fuzzable_functions: %d, unknown_functions: %d"
          % (len(functions), len(js_fuzzable_functions),
             len(cctest_fuzzable_functions), len(unknown_functions)))
    print("%d JavaScript builtins" % len(builtins))
    print("unknown functions:")
    for f in unknown_functions:
      print(f)
    return 0

  if action == "check":
    errors = 0

    def CheckCount(actual, expected, description):
      if len(actual) != expected:
        print("Expected to detect %d %s, but found %d." % (
              expected, description, len(actual)))
        print("If this change is intentional, please update the expectations"
              " at the top of %s." % THIS_SCRIPT)
        return 1
      return 0

    errors += CheckCount(functions, EXPECTED_FUNCTION_COUNT,
                         "functions in total")
    errors += CheckCount(js_fuzzable_functions, EXPECTED_FUZZABLE_COUNT,
                         "JavaScript-fuzzable functions")
    errors += CheckCount(cctest_fuzzable_functions, EXPECTED_CCTEST_COUNT,
                         "cctest-fuzzable functions")
    errors += CheckCount(unknown_functions, EXPECTED_UNKNOWN_COUNT,
                         "functions with incomplete type information")
    errors += CheckCount(builtins, EXPECTED_BUILTINS_COUNT,
                         "JavaScript builtins")

    def CheckTestcasesExisting(functions):
      errors = 0
      for f in functions:
        if not os.path.isfile(os.path.join(BASEPATH, f.Filename())):
          print("Missing testcase for %s, please run '%s generate'" %
                (f.name, THIS_SCRIPT))
          errors += 1
      files = filter(lambda filename: not filename.startswith("."),
                     os.listdir(BASEPATH))
      if (len(files) != len(functions)):
        unexpected_files = set(files) - set([f.Filename() for f in functions])
        for f in unexpected_files:
          print("Unexpected testcase: %s" % os.path.join(BASEPATH, f))
          errors += 1
        print("Run '%s generate' to automatically clean these up."
              % THIS_SCRIPT)
      return errors

    errors += CheckTestcasesExisting(js_fuzzable_functions)

    def CheckNameClashes(runtime_functions, builtins):
      errors = 0
      runtime_map = {}
      for f in runtime_functions:
        runtime_map[f.name] = 1
      for b in builtins:
        if b.name in runtime_map:
          print("Builtin/Runtime_Function name clash: %s" % b.name)
          errors += 1
      return errors

    errors += CheckNameClashes(functions, builtins)

    if errors > 0:
      return 1
    print("Generated runtime tests: all good.")
    return 0

  if action == "generate":
    GenerateTestcases(js_fuzzable_functions)
    return 0

  if action == "fuzz":
    processes = []
    if not os.path.isdir(options.save_path):
      os.makedirs(options.save_path)
    stop_running = multiprocessing.Event()
    for i in range(multiprocessing.cpu_count()):
      args = (i, options, stop_running)
      p = multiprocessing.Process(target=RunFuzzer, args=args)
      p.start()
      processes.append(p)
    try:
      for i in range(len(processes)):
        processes[i].join()
    except KeyboardInterrupt:
      stop_running.set()
      for i in range(len(processes)):
        processes[i].join()
    return 0

if __name__ == "__main__":
  sys.exit(Main())
