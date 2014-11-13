// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var EXPECTED_OUTPUT = 'sizes: 100000,25906\nok.\n';
var Module = {
  arguments: [1],
  print: function(x) {Module.printBuffer += x + '\n';},
  preRun: [function() {Module.printBuffer = ''}],
  postRun: [function() {
    assertEquals(EXPECTED_OUTPUT, Module.printBuffer);
  }],
};
// The Module object: Our interface to the outside world. We import
// and export values on it, and do the work to get that through
// closure compiler if necessary. There are various ways Module can be used:
// 1. Not defined. We create it here
// 2. A function parameter, function(Module) { ..generated code.. }
// 3. pre-run appended it, var Module = {}; ..generated code..
// 4. External script tag defines var Module.
// We need to do an eval in order to handle the closure compiler
// case, where this code here is minified but Module was defined
// elsewhere (e.g. case 4 above). We also need to check if Module
// already exists (e.g. case 3 above).
// Note that if you want to run closure, and also to use Module
// after the generated code, you will need to define   var Module = {};
// before the code. Then that object will be used in the code, and you
// can continue to use Module afterwards as well.
var Module;
if (!Module) Module = (typeof Module !== 'undefined' ? Module : null) || {};

// Sometimes an existing Module object exists with properties
// meant to overwrite the default module functionality. Here
// we collect those properties and reapply _after_ we configure
// the current environment's defaults to avoid having to be so
// defensive during initialization.
var moduleOverrides = {};
for (var key in Module) {
  if (Module.hasOwnProperty(key)) {
    moduleOverrides[key] = Module[key];
  }
}

// The environment setup code below is customized to use Module.
// *** Environment setup code ***
var ENVIRONMENT_IS_NODE = typeof process === 'object' && typeof require === 'function';
var ENVIRONMENT_IS_WEB = typeof window === 'object';
var ENVIRONMENT_IS_WORKER = typeof importScripts === 'function';
var ENVIRONMENT_IS_SHELL = !ENVIRONMENT_IS_WEB && !ENVIRONMENT_IS_NODE && !ENVIRONMENT_IS_WORKER;

if (ENVIRONMENT_IS_NODE) {
  // Expose functionality in the same simple way that the shells work
  // Note that we pollute the global namespace here, otherwise we break in node
  if (!Module['print']) Module['print'] = function print(x) {
    process['stdout'].write(x + '\n');
  };
  if (!Module['printErr']) Module['printErr'] = function printErr(x) {
    process['stderr'].write(x + '\n');
  };

  var nodeFS = require('fs');
  var nodePath = require('path');

  Module['read'] = function read(filename, binary) {
    filename = nodePath['normalize'](filename);
    var ret = nodeFS['readFileSync'](filename);
    // The path is absolute if the normalized version is the same as the resolved.
    if (!ret && filename != nodePath['resolve'](filename)) {
      filename = path.join(__dirname, '..', 'src', filename);
      ret = nodeFS['readFileSync'](filename);
    }
    if (ret && !binary) ret = ret.toString();
    return ret;
  };

  Module['readBinary'] = function readBinary(filename) { return Module['read'](filename, true) };

  Module['load'] = function load(f) {
    globalEval(read(f));
  };

  Module['arguments'] = process['argv'].slice(2);

  module['exports'] = Module;
}
else if (ENVIRONMENT_IS_SHELL) {
  if (!Module['print']) Module['print'] = print;
  if (typeof printErr != 'undefined') Module['printErr'] = printErr; // not present in v8 or older sm

  if (typeof read != 'undefined') {
    Module['read'] = read;
  } else {
    Module['read'] = function read() { throw 'no read() available (jsc?)' };
  }

  Module['readBinary'] = function readBinary(f) {
    return read(f, 'binary');
  };

  if (typeof scriptArgs != 'undefined') {
    Module['arguments'] = scriptArgs;
  } else if (typeof arguments != 'undefined') {
    Module['arguments'] = arguments;
  }

  this['Module'] = Module;

  eval("if (typeof gc === 'function' && gc.toString().indexOf('[native code]') > 0) var gc = undefined"); // wipe out the SpiderMonkey shell 'gc' function, which can confuse closure (uses it as a minified name, and it is then initted to a non-falsey value unexpectedly)
}
else if (ENVIRONMENT_IS_WEB || ENVIRONMENT_IS_WORKER) {
  Module['read'] = function read(url) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, false);
    xhr.send(null);
    return xhr.responseText;
  };

  if (typeof arguments != 'undefined') {
    Module['arguments'] = arguments;
  }

  if (typeof console !== 'undefined') {
    if (!Module['print']) Module['print'] = function print(x) {
      console.log(x);
    };
    if (!Module['printErr']) Module['printErr'] = function printErr(x) {
      console.log(x);
    };
  } else {
    // Probably a worker, and without console.log. We can do very little here...
    var TRY_USE_DUMP = false;
    if (!Module['print']) Module['print'] = (TRY_USE_DUMP && (typeof(dump) !== "undefined") ? (function(x) {
      dump(x);
    }) : (function(x) {
      // self.postMessage(x); // enable this if you want stdout to be sent as messages
    }));
  }

  if (ENVIRONMENT_IS_WEB) {
    window['Module'] = Module;
  } else {
    Module['load'] = importScripts;
  }
}
else {
  // Unreachable because SHELL is dependant on the others
  throw 'Unknown runtime environment. Where are we?';
}

function globalEval(x) {
  eval.call(null, x);
}
if (!Module['load'] == 'undefined' && Module['read']) {
  Module['load'] = function load(f) {
    globalEval(Module['read'](f));
  };
}
if (!Module['print']) {
  Module['print'] = function(){};
}
if (!Module['printErr']) {
  Module['printErr'] = Module['print'];
}
if (!Module['arguments']) {
  Module['arguments'] = [];
}
// *** Environment setup code ***

// Closure helpers
Module.print = Module['print'];
Module.printErr = Module['printErr'];

// Callbacks
Module['preRun'] = [];
Module['postRun'] = [];

// Merge back in the overrides
for (var key in moduleOverrides) {
  if (moduleOverrides.hasOwnProperty(key)) {
    Module[key] = moduleOverrides[key];
  }
}



// === Auto-generated preamble library stuff ===

//========================================
// Runtime code shared with compiler
//========================================

var Runtime = {
  stackSave: function () {
    return STACKTOP;
  },
  stackRestore: function (stackTop) {
    STACKTOP = stackTop;
  },
  forceAlign: function (target, quantum) {
    quantum = quantum || 4;
    if (quantum == 1) return target;
    if (isNumber(target) && isNumber(quantum)) {
      return Math.ceil(target/quantum)*quantum;
    } else if (isNumber(quantum) && isPowerOfTwo(quantum)) {
      return '(((' +target + ')+' + (quantum-1) + ')&' + -quantum + ')';
    }
    return 'Math.ceil((' + target + ')/' + quantum + ')*' + quantum;
  },
  isNumberType: function (type) {
    return type in Runtime.INT_TYPES || type in Runtime.FLOAT_TYPES;
  },
  isPointerType: function isPointerType(type) {
  return type[type.length-1] == '*';
},
  isStructType: function isStructType(type) {
  if (isPointerType(type)) return false;
  if (isArrayType(type)) return true;
  if (/<?\{ ?[^}]* ?\}>?/.test(type)) return true; // { i32, i8 } etc. - anonymous struct types
  // See comment in isStructPointerType()
  return type[0] == '%';
},
  INT_TYPES: {"i1":0,"i8":0,"i16":0,"i32":0,"i64":0},
  FLOAT_TYPES: {"float":0,"double":0},
  or64: function (x, y) {
    var l = (x | 0) | (y | 0);
    var h = (Math.round(x / 4294967296) | Math.round(y / 4294967296)) * 4294967296;
    return l + h;
  },
  and64: function (x, y) {
    var l = (x | 0) & (y | 0);
    var h = (Math.round(x / 4294967296) & Math.round(y / 4294967296)) * 4294967296;
    return l + h;
  },
  xor64: function (x, y) {
    var l = (x | 0) ^ (y | 0);
    var h = (Math.round(x / 4294967296) ^ Math.round(y / 4294967296)) * 4294967296;
    return l + h;
  },
  getNativeTypeSize: function (type) {
    switch (type) {
      case 'i1': case 'i8': return 1;
      case 'i16': return 2;
      case 'i32': return 4;
      case 'i64': return 8;
      case 'float': return 4;
      case 'double': return 8;
      default: {
        if (type[type.length-1] === '*') {
          return Runtime.QUANTUM_SIZE; // A pointer
        } else if (type[0] === 'i') {
          var bits = parseInt(type.substr(1));
          assert(bits % 8 === 0);
          return bits/8;
        } else {
          return 0;
        }
      }
    }
  },
  getNativeFieldSize: function (type) {
    return Math.max(Runtime.getNativeTypeSize(type), Runtime.QUANTUM_SIZE);
  },
  dedup: function dedup(items, ident) {
  var seen = {};
  if (ident) {
    return items.filter(function(item) {
      if (seen[item[ident]]) return false;
      seen[item[ident]] = true;
      return true;
    });
  } else {
    return items.filter(function(item) {
      if (seen[item]) return false;
      seen[item] = true;
      return true;
    });
  }
},
  set: function set() {
  var args = typeof arguments[0] === 'object' ? arguments[0] : arguments;
  var ret = {};
  for (var i = 0; i < args.length; i++) {
    ret[args[i]] = 0;
  }
  return ret;
},
  STACK_ALIGN: 8,
  getAlignSize: function (type, size, vararg) {
    // we align i64s and doubles on 64-bit boundaries, unlike x86
    if (!vararg && (type == 'i64' || type == 'double')) return 8;
    if (!type) return Math.min(size, 8); // align structures internally to 64 bits
    return Math.min(size || (type ? Runtime.getNativeFieldSize(type) : 0), Runtime.QUANTUM_SIZE);
  },
  calculateStructAlignment: function calculateStructAlignment(type) {
    type.flatSize = 0;
    type.alignSize = 0;
    var diffs = [];
    var prev = -1;
    var index = 0;
    type.flatIndexes = type.fields.map(function(field) {
      index++;
      var size, alignSize;
      if (Runtime.isNumberType(field) || Runtime.isPointerType(field)) {
        size = Runtime.getNativeTypeSize(field); // pack char; char; in structs, also char[X]s.
        alignSize = Runtime.getAlignSize(field, size);
      } else if (Runtime.isStructType(field)) {
        if (field[1] === '0') {
          // this is [0 x something]. When inside another structure like here, it must be at the end,
          // and it adds no size
          // XXX this happens in java-nbody for example... assert(index === type.fields.length, 'zero-length in the middle!');
          size = 0;
          if (Types.types[field]) {
            alignSize = Runtime.getAlignSize(null, Types.types[field].alignSize);
          } else {
            alignSize = type.alignSize || QUANTUM_SIZE;
          }
        } else {
          size = Types.types[field].flatSize;
          alignSize = Runtime.getAlignSize(null, Types.types[field].alignSize);
        }
      } else if (field[0] == 'b') {
        // bN, large number field, like a [N x i8]
        size = field.substr(1)|0;
        alignSize = 1;
      } else if (field[0] === '<') {
        // vector type
        size = alignSize = Types.types[field].flatSize; // fully aligned
      } else if (field[0] === 'i') {
        // illegal integer field, that could not be legalized because it is an internal structure field
        // it is ok to have such fields, if we just use them as markers of field size and nothing more complex
        size = alignSize = parseInt(field.substr(1))/8;
        assert(size % 1 === 0, 'cannot handle non-byte-size field ' + field);
      } else {
        assert(false, 'invalid type for calculateStructAlignment');
      }
      if (type.packed) alignSize = 1;
      type.alignSize = Math.max(type.alignSize, alignSize);
      var curr = Runtime.alignMemory(type.flatSize, alignSize); // if necessary, place this on aligned memory
      type.flatSize = curr + size;
      if (prev >= 0) {
        diffs.push(curr-prev);
      }
      prev = curr;
      return curr;
    });
    if (type.name_ && type.name_[0] === '[') {
      // arrays have 2 elements, so we get the proper difference. then we scale here. that way we avoid
      // allocating a potentially huge array for [999999 x i8] etc.
      type.flatSize = parseInt(type.name_.substr(1))*type.flatSize/2;
    }
    type.flatSize = Runtime.alignMemory(type.flatSize, type.alignSize);
    if (diffs.length == 0) {
      type.flatFactor = type.flatSize;
    } else if (Runtime.dedup(diffs).length == 1) {
      type.flatFactor = diffs[0];
    }
    type.needsFlattening = (type.flatFactor != 1);
    return type.flatIndexes;
  },
  generateStructInfo: function (struct, typeName, offset) {
    var type, alignment;
    if (typeName) {
      offset = offset || 0;
      type = (typeof Types === 'undefined' ? Runtime.typeInfo : Types.types)[typeName];
      if (!type) return null;
      if (type.fields.length != struct.length) {
        printErr('Number of named fields must match the type for ' + typeName + ': possibly duplicate struct names. Cannot return structInfo');
        return null;
      }
      alignment = type.flatIndexes;
    } else {
      var type = { fields: struct.map(function(item) { return item[0] }) };
      alignment = Runtime.calculateStructAlignment(type);
    }
    var ret = {
      __size__: type.flatSize
    };
    if (typeName) {
      struct.forEach(function(item, i) {
        if (typeof item === 'string') {
          ret[item] = alignment[i] + offset;
        } else {
          // embedded struct
          var key;
          for (var k in item) key = k;
          ret[key] = Runtime.generateStructInfo(item[key], type.fields[i], alignment[i]);
        }
      });
    } else {
      struct.forEach(function(item, i) {
        ret[item[1]] = alignment[i];
      });
    }
    return ret;
  },
  dynCall: function (sig, ptr, args) {
    if (args && args.length) {
      if (!args.splice) args = Array.prototype.slice.call(args);
      args.splice(0, 0, ptr);
      return Module['dynCall_' + sig].apply(null, args);
    } else {
      return Module['dynCall_' + sig].call(null, ptr);
    }
  },
  functionPointers: [],
  addFunction: function (func) {
    for (var i = 0; i < Runtime.functionPointers.length; i++) {
      if (!Runtime.functionPointers[i]) {
        Runtime.functionPointers[i] = func;
        return 2*(1 + i);
      }
    }
    throw 'Finished up all reserved function pointers. Use a higher value for RESERVED_FUNCTION_POINTERS.';
  },
  removeFunction: function (index) {
    Runtime.functionPointers[(index-2)/2] = null;
  },
  getAsmConst: function (code, numArgs) {
    // code is a constant string on the heap, so we can cache these
    if (!Runtime.asmConstCache) Runtime.asmConstCache = {};
    var func = Runtime.asmConstCache[code];
    if (func) return func;
    var args = [];
    for (var i = 0; i < numArgs; i++) {
      args.push(String.fromCharCode(36) + i); // $0, $1 etc
    }
    var source = Pointer_stringify(code);
    if (source[0] === '"') {
      // tolerate EM_ASM("..code..") even though EM_ASM(..code..) is correct
      if (source.indexOf('"', 1) === source.length-1) {
        source = source.substr(1, source.length-2);
      } else {
        // something invalid happened, e.g. EM_ASM("..code($0)..", input)
        abort('invalid EM_ASM input |' + source + '|. Please use EM_ASM(..code..) (no quotes) or EM_ASM({ ..code($0).. }, input) (to input values)');
      }
    }
    try {
      var evalled = eval('(function(' + args.join(',') + '){ ' + source + ' })'); // new Function does not allow upvars in node
    } catch(e) {
      Module.printErr('error in executing inline EM_ASM code: ' + e + ' on: \n\n' + source + '\n\nwith args |' + args + '| (make sure to use the right one out of EM_ASM, EM_ASM_ARGS, etc.)');
      throw e;
    }
    return Runtime.asmConstCache[code] = evalled;
  },
  warnOnce: function (text) {
    if (!Runtime.warnOnce.shown) Runtime.warnOnce.shown = {};
    if (!Runtime.warnOnce.shown[text]) {
      Runtime.warnOnce.shown[text] = 1;
      Module.printErr(text);
    }
  },
  funcWrappers: {},
  getFuncWrapper: function (func, sig) {
    assert(sig);
    if (!Runtime.funcWrappers[func]) {
      Runtime.funcWrappers[func] = function dynCall_wrapper() {
        return Runtime.dynCall(sig, func, arguments);
      };
    }
    return Runtime.funcWrappers[func];
  },
  UTF8Processor: function () {
    var buffer = [];
    var needed = 0;
    this.processCChar = function (code) {
      code = code & 0xFF;

      if (buffer.length == 0) {
        if ((code & 0x80) == 0x00) {        // 0xxxxxxx
          return String.fromCharCode(code);
        }
        buffer.push(code);
        if ((code & 0xE0) == 0xC0) {        // 110xxxxx
          needed = 1;
        } else if ((code & 0xF0) == 0xE0) { // 1110xxxx
          needed = 2;
        } else {                            // 11110xxx
          needed = 3;
        }
        return '';
      }

      if (needed) {
        buffer.push(code);
        needed--;
        if (needed > 0) return '';
      }

      var c1 = buffer[0];
      var c2 = buffer[1];
      var c3 = buffer[2];
      var c4 = buffer[3];
      var ret;
      if (buffer.length == 2) {
        ret = String.fromCharCode(((c1 & 0x1F) << 6)  | (c2 & 0x3F));
      } else if (buffer.length == 3) {
        ret = String.fromCharCode(((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6)  | (c3 & 0x3F));
      } else {
        // http://mathiasbynens.be/notes/javascript-encoding#surrogate-formulae
        var codePoint = ((c1 & 0x07) << 18) | ((c2 & 0x3F) << 12) |
                        ((c3 & 0x3F) << 6)  | (c4 & 0x3F);
        ret = String.fromCharCode(
          Math.floor((codePoint - 0x10000) / 0x400) + 0xD800,
          (codePoint - 0x10000) % 0x400 + 0xDC00);
      }
      buffer.length = 0;
      return ret;
    }
    this.processJSString = function processJSString(string) {
      /* TODO: use TextEncoder when present,
        var encoder = new TextEncoder();
        encoder['encoding'] = "utf-8";
        var utf8Array = encoder['encode'](aMsg.data);
      */
      string = unescape(encodeURIComponent(string));
      var ret = [];
      for (var i = 0; i < string.length; i++) {
        ret.push(string.charCodeAt(i));
      }
      return ret;
    }
  },
  getCompilerSetting: function (name) {
    throw 'You must build with -s RETAIN_COMPILER_SETTINGS=1 for Runtime.getCompilerSetting or emscripten_get_compiler_setting to work';
  },
  stackAlloc: function (size) { var ret = STACKTOP;STACKTOP = (STACKTOP + size)|0;STACKTOP = (((STACKTOP)+7)&-8); return ret; },
  staticAlloc: function (size) { var ret = STATICTOP;STATICTOP = (STATICTOP + size)|0;STATICTOP = (((STATICTOP)+7)&-8); return ret; },
  dynamicAlloc: function (size) { var ret = DYNAMICTOP;DYNAMICTOP = (DYNAMICTOP + size)|0;DYNAMICTOP = (((DYNAMICTOP)+7)&-8); if (DYNAMICTOP >= TOTAL_MEMORY) enlargeMemory();; return ret; },
  alignMemory: function (size,quantum) { var ret = size = Math.ceil((size)/(quantum ? quantum : 8))*(quantum ? quantum : 8); return ret; },
  makeBigInt: function (low,high,unsigned) { var ret = (unsigned ? ((+((low>>>0)))+((+((high>>>0)))*(+4294967296))) : ((+((low>>>0)))+((+((high|0)))*(+4294967296)))); return ret; },
  GLOBAL_BASE: 8,
  QUANTUM_SIZE: 4,
  __dummy__: 0
}


Module['Runtime'] = Runtime;









//========================================
// Runtime essentials
//========================================

var __THREW__ = 0; // Used in checking for thrown exceptions.

var ABORT = false; // whether we are quitting the application. no code should run after this. set in exit() and abort()
var EXITSTATUS = 0;

var undef = 0;
// tempInt is used for 32-bit signed values or smaller. tempBigInt is used
// for 32-bit unsigned values or more than 32 bits. TODO: audit all uses of tempInt
var tempValue, tempInt, tempBigInt, tempInt2, tempBigInt2, tempPair, tempBigIntI, tempBigIntR, tempBigIntS, tempBigIntP, tempBigIntD, tempDouble, tempFloat;
var tempI64, tempI64b;
var tempRet0, tempRet1, tempRet2, tempRet3, tempRet4, tempRet5, tempRet6, tempRet7, tempRet8, tempRet9;

function assert(condition, text) {
  if (!condition) {
    abort('Assertion failed: ' + text);
  }
}

var globalScope = this;

// C calling interface. A convenient way to call C functions (in C files, or
// defined with extern "C").
//
// Note: LLVM optimizations can inline and remove functions, after which you will not be
//       able to call them. Closure can also do so. To avoid that, add your function to
//       the exports using something like
//
//         -s EXPORTED_FUNCTIONS='["_main", "_myfunc"]'
//
// @param ident      The name of the C function (note that C++ functions will be name-mangled - use extern "C")
// @param returnType The return type of the function, one of the JS types 'number', 'string' or 'array' (use 'number' for any C pointer, and
//                   'array' for JavaScript arrays and typed arrays; note that arrays are 8-bit).
// @param argTypes   An array of the types of arguments for the function (if there are no arguments, this can be ommitted). Types are as in returnType,
//                   except that 'array' is not possible (there is no way for us to know the length of the array)
// @param args       An array of the arguments to the function, as native JS values (as in returnType)
//                   Note that string arguments will be stored on the stack (the JS string will become a C string on the stack).
// @return           The return value, as a native JS value (as in returnType)
function ccall(ident, returnType, argTypes, args) {
  return ccallFunc(getCFunc(ident), returnType, argTypes, args);
}
Module["ccall"] = ccall;

// Returns the C function with a specified identifier (for C++, you need to do manual name mangling)
function getCFunc(ident) {
  try {
    var func = Module['_' + ident]; // closure exported function
    if (!func) func = eval('_' + ident); // explicit lookup
  } catch(e) {
  }
  assert(func, 'Cannot call unknown function ' + ident + ' (perhaps LLVM optimizations or closure removed it?)');
  return func;
}

// Internal function that does a C call using a function, not an identifier
function ccallFunc(func, returnType, argTypes, args) {
  var stack = 0;
  function toC(value, type) {
    if (type == 'string') {
      if (value === null || value === undefined || value === 0) return 0; // null string
      value = intArrayFromString(value);
      type = 'array';
    }
    if (type == 'array') {
      if (!stack) stack = Runtime.stackSave();
      var ret = Runtime.stackAlloc(value.length);
      writeArrayToMemory(value, ret);
      return ret;
    }
    return value;
  }
  function fromC(value, type) {
    if (type == 'string') {
      return Pointer_stringify(value);
    }
    assert(type != 'array');
    return value;
  }
  var i = 0;
  var cArgs = args ? args.map(function(arg) {
    return toC(arg, argTypes[i++]);
  }) : [];
  var ret = fromC(func.apply(null, cArgs), returnType);
  if (stack) Runtime.stackRestore(stack);
  return ret;
}

// Returns a native JS wrapper for a C function. This is similar to ccall, but
// returns a function you can call repeatedly in a normal way. For example:
//
//   var my_function = cwrap('my_c_function', 'number', ['number', 'number']);
//   alert(my_function(5, 22));
//   alert(my_function(99, 12));
//
function cwrap(ident, returnType, argTypes) {
  var func = getCFunc(ident);
  return function() {
    return ccallFunc(func, returnType, argTypes, Array.prototype.slice.call(arguments));
  }
}
Module["cwrap"] = cwrap;

// Sets a value in memory in a dynamic way at run-time. Uses the
// type data. This is the same as makeSetValue, except that
// makeSetValue is done at compile-time and generates the needed
// code then, whereas this function picks the right code at
// run-time.
// Note that setValue and getValue only do *aligned* writes and reads!
// Note that ccall uses JS types as for defining types, while setValue and
// getValue need LLVM types ('i8', 'i32') - this is a lower-level operation
function setValue(ptr, value, type, noSafe) {
  type = type || 'i8';
  if (type.charAt(type.length-1) === '*') type = 'i32'; // pointers are 32-bit
    switch(type) {
      case 'i1': HEAP8[(ptr)]=value; break;
      case 'i8': HEAP8[(ptr)]=value; break;
      case 'i16': HEAP16[((ptr)>>1)]=value; break;
      case 'i32': HEAP32[((ptr)>>2)]=value; break;
      case 'i64': (tempI64 = [value>>>0,(tempDouble=value,(+(Math_abs(tempDouble))) >= (+1) ? (tempDouble > (+0) ? ((Math_min((+(Math_floor((tempDouble)/(+4294967296)))), (+4294967295)))|0)>>>0 : (~~((+(Math_ceil((tempDouble - +(((~~(tempDouble)))>>>0))/(+4294967296))))))>>>0) : 0)],HEAP32[((ptr)>>2)]=tempI64[0],HEAP32[(((ptr)+(4))>>2)]=tempI64[1]); break;
      case 'float': HEAPF32[((ptr)>>2)]=value; break;
      case 'double': HEAPF64[((ptr)>>3)]=value; break;
      default: abort('invalid type for setValue: ' + type);
    }
}
Module['setValue'] = setValue;

// Parallel to setValue.
function getValue(ptr, type, noSafe) {
  type = type || 'i8';
  if (type.charAt(type.length-1) === '*') type = 'i32'; // pointers are 32-bit
    switch(type) {
      case 'i1': return HEAP8[(ptr)];
      case 'i8': return HEAP8[(ptr)];
      case 'i16': return HEAP16[((ptr)>>1)];
      case 'i32': return HEAP32[((ptr)>>2)];
      case 'i64': return HEAP32[((ptr)>>2)];
      case 'float': return HEAPF32[((ptr)>>2)];
      case 'double': return HEAPF64[((ptr)>>3)];
      default: abort('invalid type for setValue: ' + type);
    }
  return null;
}
Module['getValue'] = getValue;

var ALLOC_NORMAL = 0; // Tries to use _malloc()
var ALLOC_STACK = 1; // Lives for the duration of the current function call
var ALLOC_STATIC = 2; // Cannot be freed
var ALLOC_DYNAMIC = 3; // Cannot be freed except through sbrk
var ALLOC_NONE = 4; // Do not allocate
Module['ALLOC_NORMAL'] = ALLOC_NORMAL;
Module['ALLOC_STACK'] = ALLOC_STACK;
Module['ALLOC_STATIC'] = ALLOC_STATIC;
Module['ALLOC_DYNAMIC'] = ALLOC_DYNAMIC;
Module['ALLOC_NONE'] = ALLOC_NONE;

// allocate(): This is for internal use. You can use it yourself as well, but the interface
//             is a little tricky (see docs right below). The reason is that it is optimized
//             for multiple syntaxes to save space in generated code. So you should
//             normally not use allocate(), and instead allocate memory using _malloc(),
//             initialize it with setValue(), and so forth.
// @slab: An array of data, or a number. If a number, then the size of the block to allocate,
//        in *bytes* (note that this is sometimes confusing: the next parameter does not
//        affect this!)
// @types: Either an array of types, one for each byte (or 0 if no type at that position),
//         or a single type which is used for the entire block. This only matters if there
//         is initial data - if @slab is a number, then this does not matter at all and is
//         ignored.
// @allocator: How to allocate memory, see ALLOC_*
function allocate(slab, types, allocator, ptr) {
  var zeroinit, size;
  if (typeof slab === 'number') {
    zeroinit = true;
    size = slab;
  } else {
    zeroinit = false;
    size = slab.length;
  }

  var singleType = typeof types === 'string' ? types : null;

  var ret;
  if (allocator == ALLOC_NONE) {
    ret = ptr;
  } else {
    ret = [_malloc, Runtime.stackAlloc, Runtime.staticAlloc, Runtime.dynamicAlloc][allocator === undefined ? ALLOC_STATIC : allocator](Math.max(size, singleType ? 1 : types.length));
  }

  if (zeroinit) {
    var ptr = ret, stop;
    assert((ret & 3) == 0);
    stop = ret + (size & ~3);
    for (; ptr < stop; ptr += 4) {
      HEAP32[((ptr)>>2)]=0;
    }
    stop = ret + size;
    while (ptr < stop) {
      HEAP8[((ptr++)|0)]=0;
    }
    return ret;
  }

  if (singleType === 'i8') {
    if (slab.subarray || slab.slice) {
      HEAPU8.set(slab, ret);
    } else {
      HEAPU8.set(new Uint8Array(slab), ret);
    }
    return ret;
  }

  var i = 0, type, typeSize, previousType;
  while (i < size) {
    var curr = slab[i];

    if (typeof curr === 'function') {
      curr = Runtime.getFunctionIndex(curr);
    }

    type = singleType || types[i];
    if (type === 0) {
      i++;
      continue;
    }

    if (type == 'i64') type = 'i32'; // special case: we have one i32 here, and one i32 later

    setValue(ret+i, curr, type);

    // no need to look up size unless type changes, so cache it
    if (previousType !== type) {
      typeSize = Runtime.getNativeTypeSize(type);
      previousType = type;
    }
    i += typeSize;
  }

  return ret;
}
Module['allocate'] = allocate;

function Pointer_stringify(ptr, /* optional */ length) {
  // TODO: use TextDecoder
  // Find the length, and check for UTF while doing so
  var hasUtf = false;
  var t;
  var i = 0;
  while (1) {
    t = HEAPU8[(((ptr)+(i))|0)];
    if (t >= 128) hasUtf = true;
    else if (t == 0 && !length) break;
    i++;
    if (length && i == length) break;
  }
  if (!length) length = i;

  var ret = '';

  if (!hasUtf) {
    var MAX_CHUNK = 1024; // split up into chunks, because .apply on a huge string can overflow the stack
    var curr;
    while (length > 0) {
      curr = String.fromCharCode.apply(String, HEAPU8.subarray(ptr, ptr + Math.min(length, MAX_CHUNK)));
      ret = ret ? ret + curr : curr;
      ptr += MAX_CHUNK;
      length -= MAX_CHUNK;
    }
    return ret;
  }

  var utf8 = new Runtime.UTF8Processor();
  for (i = 0; i < length; i++) {
    t = HEAPU8[(((ptr)+(i))|0)];
    ret += utf8.processCChar(t);
  }
  return ret;
}
Module['Pointer_stringify'] = Pointer_stringify;

// Given a pointer 'ptr' to a null-terminated UTF16LE-encoded string in the emscripten HEAP, returns
// a copy of that string as a Javascript String object.
function UTF16ToString(ptr) {
  var i = 0;

  var str = '';
  while (1) {
    var codeUnit = HEAP16[(((ptr)+(i*2))>>1)];
    if (codeUnit == 0)
      return str;
    ++i;
    // fromCharCode constructs a character from a UTF-16 code unit, so we can pass the UTF16 string right through.
    str += String.fromCharCode(codeUnit);
  }
}
Module['UTF16ToString'] = UTF16ToString;

// Copies the given Javascript String object 'str' to the emscripten HEAP at address 'outPtr',
// null-terminated and encoded in UTF16LE form. The copy will require at most (str.length*2+1)*2 bytes of space in the HEAP.
function stringToUTF16(str, outPtr) {
  for(var i = 0; i < str.length; ++i) {
    // charCodeAt returns a UTF-16 encoded code unit, so it can be directly written to the HEAP.
    var codeUnit = str.charCodeAt(i); // possibly a lead surrogate
    HEAP16[(((outPtr)+(i*2))>>1)]=codeUnit;
  }
  // Null-terminate the pointer to the HEAP.
  HEAP16[(((outPtr)+(str.length*2))>>1)]=0;
}
Module['stringToUTF16'] = stringToUTF16;

// Given a pointer 'ptr' to a null-terminated UTF32LE-encoded string in the emscripten HEAP, returns
// a copy of that string as a Javascript String object.
function UTF32ToString(ptr) {
  var i = 0;

  var str = '';
  while (1) {
    var utf32 = HEAP32[(((ptr)+(i*4))>>2)];
    if (utf32 == 0)
      return str;
    ++i;
    // Gotcha: fromCharCode constructs a character from a UTF-16 encoded code (pair), not from a Unicode code point! So encode the code point to UTF-16 for constructing.
    if (utf32 >= 0x10000) {
      var ch = utf32 - 0x10000;
      str += String.fromCharCode(0xD800 | (ch >> 10), 0xDC00 | (ch & 0x3FF));
    } else {
      str += String.fromCharCode(utf32);
    }
  }
}
Module['UTF32ToString'] = UTF32ToString;

// Copies the given Javascript String object 'str' to the emscripten HEAP at address 'outPtr',
// null-terminated and encoded in UTF32LE form. The copy will require at most (str.length+1)*4 bytes of space in the HEAP,
// but can use less, since str.length does not return the number of characters in the string, but the number of UTF-16 code units in the string.
function stringToUTF32(str, outPtr) {
  var iChar = 0;
  for(var iCodeUnit = 0; iCodeUnit < str.length; ++iCodeUnit) {
    // Gotcha: charCodeAt returns a 16-bit word that is a UTF-16 encoded code unit, not a Unicode code point of the character! We must decode the string to UTF-32 to the heap.
    var codeUnit = str.charCodeAt(iCodeUnit); // possibly a lead surrogate
    if (codeUnit >= 0xD800 && codeUnit <= 0xDFFF) {
      var trailSurrogate = str.charCodeAt(++iCodeUnit);
      codeUnit = 0x10000 + ((codeUnit & 0x3FF) << 10) | (trailSurrogate & 0x3FF);
    }
    HEAP32[(((outPtr)+(iChar*4))>>2)]=codeUnit;
    ++iChar;
  }
  // Null-terminate the pointer to the HEAP.
  HEAP32[(((outPtr)+(iChar*4))>>2)]=0;
}
Module['stringToUTF32'] = stringToUTF32;

function demangle(func) {
  var i = 3;
  // params, etc.
  var basicTypes = {
    'v': 'void',
    'b': 'bool',
    'c': 'char',
    's': 'short',
    'i': 'int',
    'l': 'long',
    'f': 'float',
    'd': 'double',
    'w': 'wchar_t',
    'a': 'signed char',
    'h': 'unsigned char',
    't': 'unsigned short',
    'j': 'unsigned int',
    'm': 'unsigned long',
    'x': 'long long',
    'y': 'unsigned long long',
    'z': '...'
  };
  var subs = [];
  var first = true;
  function dump(x) {
    //return;
    if (x) Module.print(x);
    Module.print(func);
    var pre = '';
    for (var a = 0; a < i; a++) pre += ' ';
    Module.print (pre + '^');
  }
  function parseNested() {
    i++;
    if (func[i] === 'K') i++; // ignore const
    var parts = [];
    while (func[i] !== 'E') {
      if (func[i] === 'S') { // substitution
        i++;
        var next = func.indexOf('_', i);
        var num = func.substring(i, next) || 0;
        parts.push(subs[num] || '?');
        i = next+1;
        continue;
      }
      if (func[i] === 'C') { // constructor
        parts.push(parts[parts.length-1]);
        i += 2;
        continue;
      }
      var size = parseInt(func.substr(i));
      var pre = size.toString().length;
      if (!size || !pre) { i--; break; } // counter i++ below us
      var curr = func.substr(i + pre, size);
      parts.push(curr);
      subs.push(curr);
      i += pre + size;
    }
    i++; // skip E
    return parts;
  }
  function parse(rawList, limit, allowVoid) { // main parser
    limit = limit || Infinity;
    var ret = '', list = [];
    function flushList() {
      return '(' + list.join(', ') + ')';
    }
    var name;
    if (func[i] === 'N') {
      // namespaced N-E
      name = parseNested().join('::');
      limit--;
      if (limit === 0) return rawList ? [name] : name;
    } else {
      // not namespaced
      if (func[i] === 'K' || (first && func[i] === 'L')) i++; // ignore const and first 'L'
      var size = parseInt(func.substr(i));
      if (size) {
        var pre = size.toString().length;
        name = func.substr(i + pre, size);
        i += pre + size;
      }
    }
    first = false;
    if (func[i] === 'I') {
      i++;
      var iList = parse(true);
      var iRet = parse(true, 1, true);
      ret += iRet[0] + ' ' + name + '<' + iList.join(', ') + '>';
    } else {
      ret = name;
    }
    paramLoop: while (i < func.length && limit-- > 0) {
      //dump('paramLoop');
      var c = func[i++];
      if (c in basicTypes) {
        list.push(basicTypes[c]);
      } else {
        switch (c) {
          case 'P': list.push(parse(true, 1, true)[0] + '*'); break; // pointer
          case 'R': list.push(parse(true, 1, true)[0] + '&'); break; // reference
          case 'L': { // literal
            i++; // skip basic type
            var end = func.indexOf('E', i);
            var size = end - i;
            list.push(func.substr(i, size));
            i += size + 2; // size + 'EE'
            break;
          }
          case 'A': { // array
            var size = parseInt(func.substr(i));
            i += size.toString().length;
            if (func[i] !== '_') throw '?';
            i++; // skip _
            list.push(parse(true, 1, true)[0] + ' [' + size + ']');
            break;
          }
          case 'E': break paramLoop;
          default: ret += '?' + c; break paramLoop;
        }
      }
    }
    if (!allowVoid && list.length === 1 && list[0] === 'void') list = []; // avoid (void)
    if (rawList) {
      if (ret) {
        list.push(ret + '?');
      }
      return list;
    } else {
      return ret + flushList();
    }
  }
  try {
    // Special-case the entry point, since its name differs from other name mangling.
    if (func == 'Object._main' || func == '_main') {
      return 'main()';
    }
    if (typeof func === 'number') func = Pointer_stringify(func);
    if (func[0] !== '_') return func;
    if (func[1] !== '_') return func; // C function
    if (func[2] !== 'Z') return func;
    switch (func[3]) {
      case 'n': return 'operator new()';
      case 'd': return 'operator delete()';
    }
    return parse();
  } catch(e) {
    return func;
  }
}

function demangleAll(text) {
  return text.replace(/__Z[\w\d_]+/g, function(x) { var y = demangle(x); return x === y ? x : (x + ' [' + y + ']') });
}

function stackTrace() {
  var stack = new Error().stack;
  return stack ? demangleAll(stack) : '(no stack trace available)'; // Stack trace is not available at least on IE10 and Safari 6.
}

// Memory management

var PAGE_SIZE = 4096;
function alignMemoryPage(x) {
  return (x+4095)&-4096;
}

var HEAP;
var HEAP8, HEAPU8, HEAP16, HEAPU16, HEAP32, HEAPU32, HEAPF32, HEAPF64;

var STATIC_BASE = 0, STATICTOP = 0, staticSealed = false; // static area
var STACK_BASE = 0, STACKTOP = 0, STACK_MAX = 0; // stack area
var DYNAMIC_BASE = 0, DYNAMICTOP = 0; // dynamic area handled by sbrk

function enlargeMemory() {
  abort('Cannot enlarge memory arrays. Either (1) compile with -s TOTAL_MEMORY=X with X higher than the current value ' + TOTAL_MEMORY + ', (2) compile with ALLOW_MEMORY_GROWTH which adjusts the size at runtime but prevents some optimizations, or (3) set Module.TOTAL_MEMORY before the program runs.');
}

var TOTAL_STACK = Module['TOTAL_STACK'] || 5242880;
var TOTAL_MEMORY = Module['TOTAL_MEMORY'] || 134217728;
var FAST_MEMORY = Module['FAST_MEMORY'] || 2097152;

var totalMemory = 4096;
while (totalMemory < TOTAL_MEMORY || totalMemory < 2*TOTAL_STACK) {
  if (totalMemory < 16*1024*1024) {
    totalMemory *= 2;
  } else {
    totalMemory += 16*1024*1024
  }
}
if (totalMemory !== TOTAL_MEMORY) {
  Module.printErr('increasing TOTAL_MEMORY to ' + totalMemory + ' to be more reasonable');
  TOTAL_MEMORY = totalMemory;
}

// Initialize the runtime's memory
// check for full engine support (use string 'subarray' to avoid closure compiler confusion)
assert(typeof Int32Array !== 'undefined' && typeof Float64Array !== 'undefined' && !!(new Int32Array(1)['subarray']) && !!(new Int32Array(1)['set']),
       'JS engine does not provide full typed array support');

var buffer = new ArrayBuffer(TOTAL_MEMORY);
HEAP8 = new Int8Array(buffer);
HEAP16 = new Int16Array(buffer);
HEAP32 = new Int32Array(buffer);
HEAPU8 = new Uint8Array(buffer);
HEAPU16 = new Uint16Array(buffer);
HEAPU32 = new Uint32Array(buffer);
HEAPF32 = new Float32Array(buffer);
HEAPF64 = new Float64Array(buffer);

// Endianness check (note: assumes compiler arch was little-endian)
HEAP32[0] = 255;
assert(HEAPU8[0] === 255 && HEAPU8[3] === 0, 'Typed arrays 2 must be run on a little-endian system');

Module['HEAP'] = HEAP;
Module['HEAP8'] = HEAP8;
Module['HEAP16'] = HEAP16;
Module['HEAP32'] = HEAP32;
Module['HEAPU8'] = HEAPU8;
Module['HEAPU16'] = HEAPU16;
Module['HEAPU32'] = HEAPU32;
Module['HEAPF32'] = HEAPF32;
Module['HEAPF64'] = HEAPF64;

function callRuntimeCallbacks(callbacks) {
  while(callbacks.length > 0) {
    var callback = callbacks.shift();
    if (typeof callback == 'function') {
      callback();
      continue;
    }
    var func = callback.func;
    if (typeof func === 'number') {
      if (callback.arg === undefined) {
        Runtime.dynCall('v', func);
      } else {
        Runtime.dynCall('vi', func, [callback.arg]);
      }
    } else {
      func(callback.arg === undefined ? null : callback.arg);
    }
  }
}

var __ATPRERUN__  = []; // functions called before the runtime is initialized
var __ATINIT__    = []; // functions called during startup
var __ATMAIN__    = []; // functions called when main() is to be run
var __ATEXIT__    = []; // functions called during shutdown
var __ATPOSTRUN__ = []; // functions called after the runtime has exited

var runtimeInitialized = false;

function preRun() {
  // compatibility - merge in anything from Module['preRun'] at this time
  if (Module['preRun']) {
    if (typeof Module['preRun'] == 'function') Module['preRun'] = [Module['preRun']];
    while (Module['preRun'].length) {
      addOnPreRun(Module['preRun'].shift());
    }
  }
  callRuntimeCallbacks(__ATPRERUN__);
}

function ensureInitRuntime() {
  if (runtimeInitialized) return;
  runtimeInitialized = true;
  callRuntimeCallbacks(__ATINIT__);
}

function preMain() {
  callRuntimeCallbacks(__ATMAIN__);
}

function exitRuntime() {
  callRuntimeCallbacks(__ATEXIT__);
}

function postRun() {
  // compatibility - merge in anything from Module['postRun'] at this time
  if (Module['postRun']) {
    if (typeof Module['postRun'] == 'function') Module['postRun'] = [Module['postRun']];
    while (Module['postRun'].length) {
      addOnPostRun(Module['postRun'].shift());
    }
  }
  callRuntimeCallbacks(__ATPOSTRUN__);
}

function addOnPreRun(cb) {
  __ATPRERUN__.unshift(cb);
}
Module['addOnPreRun'] = Module.addOnPreRun = addOnPreRun;

function addOnInit(cb) {
  __ATINIT__.unshift(cb);
}
Module['addOnInit'] = Module.addOnInit = addOnInit;

function addOnPreMain(cb) {
  __ATMAIN__.unshift(cb);
}
Module['addOnPreMain'] = Module.addOnPreMain = addOnPreMain;

function addOnExit(cb) {
  __ATEXIT__.unshift(cb);
}
Module['addOnExit'] = Module.addOnExit = addOnExit;

function addOnPostRun(cb) {
  __ATPOSTRUN__.unshift(cb);
}
Module['addOnPostRun'] = Module.addOnPostRun = addOnPostRun;

// Tools

// This processes a JS string into a C-line array of numbers, 0-terminated.
// For LLVM-originating strings, see parser.js:parseLLVMString function
function intArrayFromString(stringy, dontAddNull, length /* optional */) {
  var ret = (new Runtime.UTF8Processor()).processJSString(stringy);
  if (length) {
    ret.length = length;
  }
  if (!dontAddNull) {
    ret.push(0);
  }
  return ret;
}
Module['intArrayFromString'] = intArrayFromString;

function intArrayToString(array) {
  var ret = [];
  for (var i = 0; i < array.length; i++) {
    var chr = array[i];
    if (chr > 0xFF) {
      chr &= 0xFF;
    }
    ret.push(String.fromCharCode(chr));
  }
  return ret.join('');
}
Module['intArrayToString'] = intArrayToString;

// Write a Javascript array to somewhere in the heap
function writeStringToMemory(string, buffer, dontAddNull) {
  var array = intArrayFromString(string, dontAddNull);
  var i = 0;
  while (i < array.length) {
    var chr = array[i];
    HEAP8[(((buffer)+(i))|0)]=chr;
    i = i + 1;
  }
}
Module['writeStringToMemory'] = writeStringToMemory;

function writeArrayToMemory(array, buffer) {
  for (var i = 0; i < array.length; i++) {
    HEAP8[(((buffer)+(i))|0)]=array[i];
  }
}
Module['writeArrayToMemory'] = writeArrayToMemory;

function writeAsciiToMemory(str, buffer, dontAddNull) {
  for (var i = 0; i < str.length; i++) {
    HEAP8[(((buffer)+(i))|0)]=str.charCodeAt(i);
  }
  if (!dontAddNull) HEAP8[(((buffer)+(str.length))|0)]=0;
}
Module['writeAsciiToMemory'] = writeAsciiToMemory;

function unSign(value, bits, ignore) {
  if (value >= 0) {
    return value;
  }
  return bits <= 32 ? 2*Math.abs(1 << (bits-1)) + value // Need some trickery, since if bits == 32, we are right at the limit of the bits JS uses in bitshifts
                    : Math.pow(2, bits)         + value;
}
function reSign(value, bits, ignore) {
  if (value <= 0) {
    return value;
  }
  var half = bits <= 32 ? Math.abs(1 << (bits-1)) // abs is needed if bits == 32
                        : Math.pow(2, bits-1);
  if (value >= half && (bits <= 32 || value > half)) { // for huge values, we can hit the precision limit and always get true here. so don't do that
                                                       // but, in general there is no perfect solution here. With 64-bit ints, we get rounding and errors
                                                       // TODO: In i64 mode 1, resign the two parts separately and safely
    value = -2*half + value; // Cannot bitshift half, as it may be at the limit of the bits JS uses in bitshifts
  }
  return value;
}

// check for imul support, and also for correctness ( https://bugs.webkit.org/show_bug.cgi?id=126345 )
if (!Math['imul'] || Math['imul'](0xffffffff, 5) !== -5) Math['imul'] = function imul(a, b) {
  var ah  = a >>> 16;
  var al = a & 0xffff;
  var bh  = b >>> 16;
  var bl = b & 0xffff;
  return (al*bl + ((ah*bl + al*bh) << 16))|0;
};
Math.imul = Math['imul'];


var Math_abs = Math.abs;
var Math_cos = Math.cos;
var Math_sin = Math.sin;
var Math_tan = Math.tan;
var Math_acos = Math.acos;
var Math_asin = Math.asin;
var Math_atan = Math.atan;
var Math_atan2 = Math.atan2;
var Math_exp = Math.exp;
var Math_log = Math.log;
var Math_sqrt = Math.sqrt;
var Math_ceil = Math.ceil;
var Math_floor = Math.floor;
var Math_pow = Math.pow;
var Math_imul = Math.imul;
var Math_fround = Math.fround;
var Math_min = Math.min;

// A counter of dependencies for calling run(). If we need to
// do asynchronous work before running, increment this and
// decrement it. Incrementing must happen in a place like
// PRE_RUN_ADDITIONS (used by emcc to add file preloading).
// Note that you can add dependencies in preRun, even though
// it happens right before run - run will be postponed until
// the dependencies are met.
var runDependencies = 0;
var runDependencyWatcher = null;
var dependenciesFulfilled = null; // overridden to take different actions when all run dependencies are fulfilled

function addRunDependency(id) {
  runDependencies++;
  if (Module['monitorRunDependencies']) {
    Module['monitorRunDependencies'](runDependencies);
  }
}
Module['addRunDependency'] = addRunDependency;
function removeRunDependency(id) {
  runDependencies--;
  if (Module['monitorRunDependencies']) {
    Module['monitorRunDependencies'](runDependencies);
  }
  if (runDependencies == 0) {
    if (runDependencyWatcher !== null) {
      clearInterval(runDependencyWatcher);
      runDependencyWatcher = null;
    }
    if (dependenciesFulfilled) {
      var callback = dependenciesFulfilled;
      dependenciesFulfilled = null;
      callback(); // can add another dependenciesFulfilled
    }
  }
}
Module['removeRunDependency'] = removeRunDependency;

Module["preloadedImages"] = {}; // maps url to image data
Module["preloadedAudios"] = {}; // maps url to audio data


var memoryInitializer = null;

// === Body ===





STATIC_BASE = 8;

STATICTOP = STATIC_BASE + Runtime.alignMemory(14963);
/* global initializers */ __ATINIT__.push();


/* memory initializer */ allocate([0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,115,105,122,101,115,58,32,37,100,44,37,100,10,0,0,0,100,101,99,111,109,112,114,101,115,115,101,100,83,105,122,101,32,61,61,32,115,105,122,101,0,0,0,0,0,0,0,0,47,116,109,112,47,101,109,115,99,114,105,112,116,101,110,95,116,101,109,112,47,122,108,105,98,46,99,0,0,0,0,0,100,111,105,116,0,0,0,0,115,116,114,99,109,112,40,98,117,102,102,101,114,44,32,98,117,102,102,101,114,51,41,32,61,61,32,48,0,0,0,0,101,114,114,111,114,58,32,37,100,92,110,0,0,0,0,0,111,107,46,0,0,0,0,0,49,46,50,46,53,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,4,0,4,0,8,0,4,0,2,0,0,0,4,0,5,0,16,0,8,0,2,0,0,0,4,0,6,0,32,0,32,0,2,0,0,0,4,0,4,0,16,0,16,0,3,0,0,0,8,0,16,0,32,0,32,0,3,0,0,0,8,0,16,0,128,0,128,0,3,0,0,0,8,0,32,0,128,0,0,1,3,0,0,0,32,0,128,0,2,1,0,4,3,0,0,0,32,0,2,1,2,1,0,16,3,0,0,0,0,1,2,3,4,4,5,5,6,6,6,6,7,7,7,7,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,0,0,16,17,18,18,19,19,20,20,20,20,21,21,21,21,22,22,22,22,22,22,22,22,23,23,23,23,23,23,23,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,0,1,2,3,4,5,6,7,8,8,9,9,10,10,11,11,12,12,12,12,13,13,13,13,14,14,14,14,15,15,15,15,16,16,16,16,16,16,16,16,17,17,17,17,17,17,17,17,18,18,18,18,18,18,18,18,19,19,19,19,19,19,19,19,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,28,112,4,0,0,104,9,0,0,1,1,0,0,30,1,0,0,15,0,0,0,0,0,0,0,240,8,0,0,88,10,0,0,0,0,0,0,30,0,0,0,15,0,0,0,0,0,0,0,0,0,0,0,96,11,0,0,0,0,0,0,19,0,0,0,7,0,0,0,0,0,0,0,12,0,8,0,140,0,8,0,76,0,8,0,204,0,8,0,44,0,8,0,172,0,8,0,108,0,8,0,236,0,8,0,28,0,8,0,156,0,8,0,92,0,8,0,220,0,8,0,60,0,8,0,188,0,8,0,124,0,8,0,252,0,8,0,2,0,8,0,130,0,8,0,66,0,8,0,194,0,8,0,34,0,8,0,162,0,8,0,98,0,8,0,226,0,8,0,18,0,8,0,146,0,8,0,82,0,8,0,210,0,8,0,50,0,8,0,178,0,8,0,114,0,8,0,242,0,8,0,10,0,8,0,138,0,8,0,74,0,8,0,202,0,8,0,42,0,8,0,170,0,8,0,106,0,8,0,234,0,8,0,26,0,8,0,154,0,8,0,90,0,8,0,218,0,8,0,58,0,8,0,186,0,8,0,122,0,8,0,250,0,8,0,6,0,8,0,134,0,8,0,70,0,8,0,198,0,8,0,38,0,8,0,166,0,8,0,102,0,8,0,230,0,8,0,22,0,8,0,150,0,8,0,86,0,8,0,214,0,8,0,54,0,8,0,182,0,8,0,118,0,8,0,246,0,8,0,14,0,8,0,142,0,8,0,78,0,8,0,206,0,8,0,46,0,8,0,174,0,8,0,110,0,8,0,238,0,8,0,30,0,8,0,158,0,8,0,94,0,8,0,222,0,8,0,62,0,8,0,190,0,8,0,126,0,8,0,254,0,8,0,1,0,8,0,129,0,8,0,65,0,8,0,193,0,8,0,33,0,8,0,161,0,8,0,97,0,8,0,225,0,8,0,17,0,8,0,145,0,8,0,81,0,8,0,209,0,8,0,49,0,8,0,177,0,8,0,113,0,8,0,241,0,8,0,9,0,8,0,137,0,8,0,73,0,8,0,201,0,8,0,41,0,8,0,169,0,8,0,105,0,8,0,233,0,8,0,25,0,8,0,153,0,8,0,89,0,8,0,217,0,8,0,57,0,8,0,185,0,8,0,121,0,8,0,249,0,8,0,5,0,8,0,133,0,8,0,69,0,8,0,197,0,8,0,37,0,8,0,165,0,8,0,101,0,8,0,229,0,8,0,21,0,8,0,149,0,8,0,85,0,8,0,213,0,8,0,53,0,8,0,181,0,8,0,117,0,8,0,245,0,8,0,13,0,8,0,141,0,8,0,77,0,8,0,205,0,8,0,45,0,8,0,173,0,8,0,109,0,8,0,237,0,8,0,29,0,8,0,157,0,8,0,93,0,8,0,221,0,8,0,61,0,8,0,189,0,8,0,125,0,8,0,253,0,8,0,19,0,9,0,19,1,9,0,147,0,9,0,147,1,9,0,83,0,9,0,83,1,9,0,211,0,9,0,211,1,9,0,51,0,9,0,51,1,9,0,179,0,9,0,179,1,9,0,115,0,9,0,115,1,9,0,243,0,9,0,243,1,9,0,11,0,9,0,11,1,9,0,139,0,9,0,139,1,9,0,75,0,9,0,75,1,9,0,203,0,9,0,203,1,9,0,43,0,9,0,43,1,9,0,171,0,9,0,171,1,9,0,107,0,9,0,107,1,9,0,235,0,9,0,235,1,9,0,27,0,9,0,27,1,9,0,155,0,9,0,155,1,9,0,91,0,9,0,91,1,9,0,219,0,9,0,219,1,9,0,59,0,9,0,59,1,9,0,187,0,9,0,187,1,9,0,123,0,9,0,123,1,9,0,251,0,9,0,251,1,9,0,7,0,9,0,7,1,9,0,135,0,9,0,135,1,9,0,71,0,9,0,71,1,9,0,199,0,9,0,199,1,9,0,39,0,9,0,39,1,9,0,167,0,9,0,167,1,9,0,103,0,9,0,103,1,9,0,231,0,9,0,231,1,9,0,23,0,9,0,23,1,9,0,151,0,9,0,151,1,9,0,87,0,9,0,87,1,9,0,215,0,9,0,215,1,9,0,55,0,9,0,55,1,9,0,183,0,9,0,183,1,9,0,119,0,9,0,119,1,9,0,247,0,9,0,247,1,9,0,15,0,9,0,15,1,9,0,143,0,9,0,143,1,9,0,79,0,9,0,79,1,9,0,207,0,9,0,207,1,9,0,47,0,9,0,47,1,9,0,175,0,9,0,175,1,9,0,111,0,9,0,111,1,9,0,239,0,9,0,239,1,9,0,31,0,9,0,31,1,9,0,159,0,9,0,159,1,9,0,95,0,9,0,95,1,9,0,223,0,9,0,223,1,9,0,63,0,9,0,63,1,9,0,191,0,9,0,191,1,9,0,127,0,9,0,127,1,9,0,255,0,9,0,255,1,9,0,0,0,7,0,64,0,7,0,32,0,7,0,96,0,7,0,16,0,7,0,80,0,7,0,48,0,7,0,112,0,7,0,8,0,7,0,72,0,7,0,40,0,7,0,104,0,7,0,24,0,7,0,88,0,7,0,56,0,7,0,120,0,7,0,4,0,7,0,68,0,7,0,36,0,7,0,100,0,7,0,20,0,7,0,84,0,7,0,52,0,7,0,116,0,7,0,3,0,8,0,131,0,8,0,67,0,8,0,195,0,8,0,35,0,8,0,163,0,8,0,99,0,8,0,227,0,8,0,0,0,5,0,16,0,5,0,8,0,5,0,24,0,5,0,4,0,5,0,20,0,5,0,12,0,5,0,28,0,5,0,2,0,5,0,18,0,5,0,10,0,5,0,26,0,5,0,6,0,5,0,22,0,5,0,14,0,5,0,30,0,5,0,1,0,5,0,17,0,5,0,9,0,5,0,25,0,5,0,5,0,5,0,21,0,5,0,13,0,5,0,29,0,5,0,3,0,5,0,19,0,5,0,11,0,5,0,27,0,5,0,7,0,5,0,23,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,2,0,0,0,2,0,0,0,2,0,0,0,2,0,0,0,3,0,0,0,3,0,0,0,3,0,0,0,3,0,0,0,4,0,0,0,4,0,0,0,4,0,0,0,4,0,0,0,5,0,0,0,5,0,0,0,5,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,2,0,0,0,3,0,0,0,4,0,0,0,5,0,0,0,6,0,0,0,7,0,0,0,8,0,0,0,10,0,0,0,12,0,0,0,14,0,0,0,16,0,0,0,20,0,0,0,24,0,0,0,28,0,0,0,32,0,0,0,40,0,0,0,48,0,0,0,56,0,0,0,64,0,0,0,80,0,0,0,96,0,0,0,112,0,0,0,128,0,0,0,160,0,0,0,192,0,0,0,224,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,2,0,0,0,2,0,0,0,3,0,0,0,3,0,0,0,4,0,0,0,4,0,0,0,5,0,0,0,5,0,0,0,6,0,0,0,6,0,0,0,7,0,0,0,7,0,0,0,8,0,0,0,8,0,0,0,9,0,0,0,9,0,0,0,10,0,0,0,10,0,0,0,11,0,0,0,11,0,0,0,12,0,0,0,12,0,0,0,13,0,0,0,13,0,0,0,0,0,0,0,1,0,0,0,2,0,0,0,3,0,0,0,4,0,0,0,6,0,0,0,8,0,0,0,12,0,0,0,16,0,0,0,24,0,0,0,32,0,0,0,48,0,0,0,64,0,0,0,96,0,0,0,128,0,0,0,192,0,0,0,0,1,0,0,128,1,0,0,0,2,0,0,0,3,0,0,0,4,0,0,0,6,0,0,0,8,0,0,0,12,0,0,0,16,0,0,0,24,0,0,0,32,0,0,0,48,0,0,0,64,0,0,0,96,0,0,16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,3,0,0,0,7,0,0,0,0,0,0,0,49,46,50,46,53,0,0,0,110,101,101,100,32,100,105,99,116,105,111,110,97,114,121,0,115,116,114,101,97,109,32,101,110,100,0,0,0,0,0,0,0,0,0,0,0,0,0,0,102,105,108,101,32,101,114,114,111,114,0,0,0,0,0,0,115,116,114,101,97,109,32,101,114,114,111,114,0,0,0,0,100,97,116,97,32,101,114,114,111,114,0,0,0,0,0,0,105,110,115,117,102,102,105,99,105,101,110,116,32,109,101,109,111,114,121,0,0,0,0,0,98,117,102,102,101,114,32,101,114,114,111,114,0,0,0,0,105,110,99,111,109,112,97,116,105,98,108,101,32,118,101,114,115,105,111,110,0,0,0,0,184,11,0,0,200,11,0,0,216,11,0,0,224,11,0,0,240,11,0,0,0,12,0,0,16,12,0,0,40,12,0,0,56,12,0,0,216,11,0,0,0,0,0,0,150,48,7,119,44,97,14,238,186,81,9,153,25,196,109,7,143,244,106,112,53,165,99,233,163,149,100,158,50,136,219,14,164,184,220,121,30,233,213,224,136,217,210,151,43,76,182,9,189,124,177,126,7,45,184,231,145,29,191,144,100,16,183,29,242,32,176,106,72,113,185,243,222,65,190,132,125,212,218,26,235,228,221,109,81,181,212,244,199,133,211,131,86,152,108,19,192,168,107,100,122,249,98,253,236,201,101,138,79,92,1,20,217,108,6,99,99,61,15,250,245,13,8,141,200,32,110,59,94,16,105,76,228,65,96,213,114,113,103,162,209,228,3,60,71,212,4,75,253,133,13,210,107,181,10,165,250,168,181,53,108,152,178,66,214,201,187,219,64,249,188,172,227,108,216,50,117,92,223,69,207,13,214,220,89,61,209,171,172,48,217,38,58,0,222,81,128,81,215,200,22,97,208,191,181,244,180,33,35,196,179,86,153,149,186,207,15,165,189,184,158,184,2,40,8,136,5,95,178,217,12,198,36,233,11,177,135,124,111,47,17,76,104,88,171,29,97,193,61,45,102,182,144,65,220,118,6,113,219,1,188,32,210,152,42,16,213,239,137,133,177,113,31,181,182,6,165,228,191,159,51,212,184,232,162,201,7,120,52,249,0,15,142,168,9,150,24,152,14,225,187,13,106,127,45,61,109,8,151,108,100,145,1,92,99,230,244,81,107,107,98,97,108,28,216,48,101,133,78,0,98,242,237,149,6,108,123,165,1,27,193,244,8,130,87,196,15,245,198,217,176,101,80,233,183,18,234,184,190,139,124,136,185,252,223,29,221,98,73,45,218,21,243,124,211,140,101,76,212,251,88,97,178,77,206,81,181,58,116,0,188,163,226,48,187,212,65,165,223,74,215,149,216,61,109,196,209,164,251,244,214,211,106,233,105,67,252,217,110,52,70,136,103,173,208,184,96,218,115,45,4,68,229,29,3,51,95,76,10,170,201,124,13,221,60,113,5,80,170,65,2,39,16,16,11,190,134,32,12,201,37,181,104,87,179,133,111,32,9,212,102,185,159,228,97,206,14,249,222,94,152,201,217,41,34,152,208,176,180,168,215,199,23,61,179,89,129,13,180,46,59,92,189,183,173,108,186,192,32,131,184,237,182,179,191,154,12,226,182,3,154,210,177,116,57,71,213,234,175,119,210,157,21,38,219,4,131,22,220,115,18,11,99,227,132,59,100,148,62,106,109,13,168,90,106,122,11,207,14,228,157,255,9,147,39,174,0,10,177,158,7,125,68,147,15,240,210,163,8,135,104,242,1,30,254,194,6,105,93,87,98,247,203,103,101,128,113,54,108,25,231,6,107,110,118,27,212,254,224,43,211,137,90,122,218,16,204,74,221,103,111,223,185,249,249,239,190,142,67,190,183,23,213,142,176,96,232,163,214,214,126,147,209,161,196,194,216,56,82,242,223,79,241,103,187,209,103,87,188,166,221,6,181,63,75,54,178,72,218,43,13,216,76,27,10,175,246,74,3,54,96,122,4,65,195,239,96,223,85,223,103,168,239,142,110,49,121,190,105,70,140,179,97,203,26,131,102,188,160,210,111,37,54,226,104,82,149,119,12,204,3,71,11,187,185,22,2,34,47,38,5,85,190,59,186,197,40,11,189,178,146,90,180,43,4,106,179,92,167,255,215,194,49,207,208,181,139,158,217,44,29,174,222,91,176,194,100,155,38,242,99,236,156,163,106,117,10,147,109,2,169,6,9,156,63,54,14,235,133,103,7,114,19,87,0,5,130,74,191,149,20,122,184,226,174,43,177,123,56,27,182,12,155,142,210,146,13,190,213,229,183,239,220,124,33,223,219,11,212,210,211,134,66,226,212,241,248,179,221,104,110,131,218,31,205,22,190,129,91,38,185,246,225,119,176,111,119,71,183,24,230,90,8,136,112,106,15,255,202,59,6,102,92,11,1,17,255,158,101,143,105,174,98,248,211,255,107,97,69,207,108,22,120,226,10,160,238,210,13,215,84,131,4,78,194,179,3,57,97,38,103,167,247,22,96,208,77,71,105,73,219,119,110,62,74,106,209,174,220,90,214,217,102,11,223,64,240,59,216,55,83,174,188,169,197,158,187,222,127,207,178,71,233,255,181,48,28,242,189,189,138,194,186,202,48,147,179,83,166,163,180,36,5,54,208,186,147,6,215,205,41,87,222,84,191,103,217,35,46,122,102,179,184,74,97,196,2,27,104,93,148,43,111,42,55,190,11,180,161,142,12,195,27,223,5,90,141,239,2,45,0,0,0,0,65,49,27,25,130,98,54,50,195,83,45,43,4,197,108,100,69,244,119,125,134,167,90,86,199,150,65,79,8,138,217,200,73,187,194,209,138,232,239,250,203,217,244,227,12,79,181,172,77,126,174,181,142,45,131,158,207,28,152,135,81,18,194,74,16,35,217,83,211,112,244,120,146,65,239,97,85,215,174,46,20,230,181,55,215,181,152,28,150,132,131,5,89,152,27,130,24,169,0,155,219,250,45,176,154,203,54,169,93,93,119,230,28,108,108,255,223,63,65,212,158,14,90,205,162,36,132,149,227,21,159,140,32,70,178,167,97,119,169,190,166,225,232,241,231,208,243,232,36,131,222,195,101,178,197,218,170,174,93,93,235,159,70,68,40,204,107,111,105,253,112,118,174,107,49,57,239,90,42,32,44,9,7,11,109,56,28,18,243,54,70,223,178,7,93,198,113,84,112,237,48,101,107,244,247,243,42,187,182,194,49,162,117,145,28,137,52,160,7,144,251,188,159,23,186,141,132,14,121,222,169,37,56,239,178,60,255,121,243,115,190,72,232,106,125,27,197,65,60,42,222,88,5,79,121,240,68,126,98,233,135,45,79,194,198,28,84,219,1,138,21,148,64,187,14,141,131,232,35,166,194,217,56,191,13,197,160,56,76,244,187,33,143,167,150,10,206,150,141,19,9,0,204,92,72,49,215,69,139,98,250,110,202,83,225,119,84,93,187,186,21,108,160,163,214,63,141,136,151,14,150,145,80,152,215,222,17,169,204,199,210,250,225,236,147,203,250,245,92,215,98,114,29,230,121,107,222,181,84,64,159,132,79,89,88,18,14,22,25,35,21,15,218,112,56,36,155,65,35,61,167,107,253,101,230,90,230,124,37,9,203,87,100,56,208,78,163,174,145,1,226,159,138,24,33,204,167,51,96,253,188,42,175,225,36,173,238,208,63,180,45,131,18,159,108,178,9,134,171,36,72,201,234,21,83,208,41,70,126,251,104,119,101,226,246,121,63,47,183,72,36,54,116,27,9,29,53,42,18,4,242,188,83,75,179,141,72,82,112,222,101,121,49,239,126,96,254,243,230,231,191,194,253,254,124,145,208,213,61,160,203,204,250,54,138,131,187,7,145,154,120,84,188,177,57,101,167,168,75,152,131,59,10,169,152,34,201,250,181,9,136,203,174,16,79,93,239,95,14,108,244,70,205,63,217,109,140,14,194,116,67,18,90,243,2,35,65,234,193,112,108,193,128,65,119,216,71,215,54,151,6,230,45,142,197,181,0,165,132,132,27,188,26,138,65,113,91,187,90,104,152,232,119,67,217,217,108,90,30,79,45,21,95,126,54,12,156,45,27,39,221,28,0,62,18,0,152,185,83,49,131,160,144,98,174,139,209,83,181,146,22,197,244,221,87,244,239,196,148,167,194,239,213,150,217,246,233,188,7,174,168,141,28,183,107,222,49,156,42,239,42,133,237,121,107,202,172,72,112,211,111,27,93,248,46,42,70,225,225,54,222,102,160,7,197,127,99,84,232,84,34,101,243,77,229,243,178,2,164,194,169,27,103,145,132,48,38,160,159,41,184,174,197,228,249,159,222,253,58,204,243,214,123,253,232,207,188,107,169,128,253,90,178,153,62,9,159,178,127,56,132,171,176,36,28,44,241,21,7,53,50,70,42,30,115,119,49,7,180,225,112,72,245,208,107,81,54,131,70,122,119,178,93,99,78,215,250,203,15,230,225,210,204,181,204,249,141,132,215,224,74,18,150,175,11,35,141,182,200,112,160,157,137,65,187,132,70,93,35,3,7,108,56,26,196,63,21,49,133,14,14,40,66,152,79,103,3,169,84,126,192,250,121,85,129,203,98,76,31,197,56,129,94,244,35,152,157,167,14,179,220,150,21,170,27,0,84,229,90,49,79,252,153,98,98,215,216,83,121,206,23,79,225,73,86,126,250,80,149,45,215,123,212,28,204,98,19,138,141,45,82,187,150,52,145,232,187,31,208,217,160,6,236,243,126,94,173,194,101,71,110,145,72,108,47,160,83,117,232,54,18,58,169,7,9,35,106,84,36,8,43,101,63,17,228,121,167,150,165,72,188,143,102,27,145,164,39,42,138,189,224,188,203,242,161,141,208,235,98,222,253,192,35,239,230,217,189,225,188,20,252,208,167,13,63,131,138,38,126,178,145,63,185,36,208,112,248,21,203,105,59,70,230,66,122,119,253,91,181,107,101,220,244,90,126,197,55,9,83,238,118,56,72,247,177,174,9,184,240,159,18,161,51,204,63,138,114,253,36,147,0,0,0,0,55,106,194,1,110,212,132,3,89,190,70,2,220,168,9,7,235,194,203,6,178,124,141,4,133,22,79,5,184,81,19,14,143,59,209,15,214,133,151,13,225,239,85,12,100,249,26,9,83,147,216,8,10,45,158,10,61,71,92,11,112,163,38,28,71,201,228,29,30,119,162,31,41,29,96,30,172,11,47,27,155,97,237,26,194,223,171,24,245,181,105,25,200,242,53,18,255,152,247,19,166,38,177,17,145,76,115,16,20,90,60,21,35,48,254,20,122,142,184,22,77,228,122,23,224,70,77,56,215,44,143,57,142,146,201,59,185,248,11,58,60,238,68,63,11,132,134,62,82,58,192,60,101,80,2,61,88,23,94,54,111,125,156,55,54,195,218,53,1,169,24,52,132,191,87,49,179,213,149,48,234,107,211,50,221,1,17,51,144,229,107,36,167,143,169,37,254,49,239,39,201,91,45,38,76,77,98,35,123,39,160,34,34,153,230,32,21,243,36,33,40,180,120,42,31,222,186,43,70,96,252,41,113,10,62,40,244,28,113,45,195,118,179,44,154,200,245,46,173,162,55,47,192,141,154,112,247,231,88,113,174,89,30,115,153,51,220,114,28,37,147,119,43,79,81,118,114,241,23,116,69,155,213,117,120,220,137,126,79,182,75,127,22,8,13,125,33,98,207,124,164,116,128,121,147,30,66,120,202,160,4,122,253,202,198,123,176,46,188,108,135,68,126,109,222,250,56,111,233,144,250,110,108,134,181,107,91,236,119,106,2,82,49,104,53,56,243,105,8,127,175,98,63,21,109,99,102,171,43,97,81,193,233,96,212,215,166,101,227,189,100,100,186,3,34,102,141,105,224,103,32,203,215,72,23,161,21,73,78,31,83,75,121,117,145,74,252,99,222,79,203,9,28,78,146,183,90,76,165,221,152,77,152,154,196,70,175,240,6,71,246,78,64,69,193,36,130,68,68,50,205,65,115,88,15,64,42,230,73,66,29,140,139,67,80,104,241,84,103,2,51,85,62,188,117,87,9,214,183,86,140,192,248,83,187,170,58,82,226,20,124,80,213,126,190,81,232,57,226,90,223,83,32,91,134,237,102,89,177,135,164,88,52,145,235,93,3,251,41,92,90,69,111,94,109,47,173,95,128,27,53,225,183,113,247,224,238,207,177,226,217,165,115,227,92,179,60,230,107,217,254,231,50,103,184,229,5,13,122,228,56,74,38,239,15,32,228,238,86,158,162,236,97,244,96,237,228,226,47,232,211,136,237,233,138,54,171,235,189,92,105,234,240,184,19,253,199,210,209,252,158,108,151,254,169,6,85,255,44,16,26,250,27,122,216,251,66,196,158,249,117,174,92,248,72,233,0,243,127,131,194,242,38,61,132,240,17,87,70,241,148,65,9,244,163,43,203,245,250,149,141,247,205,255,79,246,96,93,120,217,87,55,186,216,14,137,252,218,57,227,62,219,188,245,113,222,139,159,179,223,210,33,245,221,229,75,55,220,216,12,107,215,239,102,169,214,182,216,239,212,129,178,45,213,4,164,98,208,51,206,160,209,106,112,230,211,93,26,36,210,16,254,94,197,39,148,156,196,126,42,218,198,73,64,24,199,204,86,87,194,251,60,149,195,162,130,211,193,149,232,17,192,168,175,77,203,159,197,143,202,198,123,201,200,241,17,11,201,116,7,68,204,67,109,134,205,26,211,192,207,45,185,2,206,64,150,175,145,119,252,109,144,46,66,43,146,25,40,233,147,156,62,166,150,171,84,100,151,242,234,34,149,197,128,224,148,248,199,188,159,207,173,126,158,150,19,56,156,161,121,250,157,36,111,181,152,19,5,119,153,74,187,49,155,125,209,243,154,48,53,137,141,7,95,75,140,94,225,13,142,105,139,207,143,236,157,128,138,219,247,66,139,130,73,4,137,181,35,198,136,136,100,154,131,191,14,88,130,230,176,30,128,209,218,220,129,84,204,147,132,99,166,81,133,58,24,23,135,13,114,213,134,160,208,226,169,151,186,32,168,206,4,102,170,249,110,164,171,124,120,235,174,75,18,41,175,18,172,111,173,37,198,173,172,24,129,241,167,47,235,51,166,118,85,117,164,65,63,183,165,196,41,248,160,243,67,58,161,170,253,124,163,157,151,190,162,208,115,196,181,231,25,6,180,190,167,64,182,137,205,130,183,12,219,205,178,59,177,15,179,98,15,73,177,85,101,139,176,104,34,215,187,95,72,21,186,6,246,83,184,49,156,145,185,180,138,222,188,131,224,28,189,218,94,90,191,237,52,152,190,0,0,0,0,101,103,188,184,139,200,9,170,238,175,181,18,87,151,98,143,50,240,222,55,220,95,107,37,185,56,215,157,239,40,180,197,138,79,8,125,100,224,189,111,1,135,1,215,184,191,214,74,221,216,106,242,51,119,223,224,86,16,99,88,159,87,25,80,250,48,165,232,20,159,16,250,113,248,172,66,200,192,123,223,173,167,199,103,67,8,114,117,38,111,206,205,112,127,173,149,21,24,17,45,251,183,164,63,158,208,24,135,39,232,207,26,66,143,115,162,172,32,198,176,201,71,122,8,62,175,50,160,91,200,142,24,181,103,59,10,208,0,135,178,105,56,80,47,12,95,236,151,226,240,89,133,135,151,229,61,209,135,134,101,180,224,58,221,90,79,143,207,63,40,51,119,134,16,228,234,227,119,88,82,13,216,237,64,104,191,81,248,161,248,43,240,196,159,151,72,42,48,34,90,79,87,158,226,246,111,73,127,147,8,245,199,125,167,64,213,24,192,252,109,78,208,159,53,43,183,35,141,197,24,150,159,160,127,42,39,25,71,253,186,124,32,65,2,146,143,244,16,247,232,72,168,61,88,20,155,88,63,168,35,182,144,29,49,211,247,161,137,106,207,118,20,15,168,202,172,225,7,127,190,132,96,195,6,210,112,160,94,183,23,28,230,89,184,169,244,60,223,21,76,133,231,194,209,224,128,126,105,14,47,203,123,107,72,119,195,162,15,13,203,199,104,177,115,41,199,4,97,76,160,184,217,245,152,111,68,144,255,211,252,126,80,102,238,27,55,218,86,77,39,185,14,40,64,5,182,198,239,176,164,163,136,12,28,26,176,219,129,127,215,103,57,145,120,210,43,244,31,110,147,3,247,38,59,102,144,154,131,136,63,47,145,237,88,147,41,84,96,68,180,49,7,248,12,223,168,77,30,186,207,241,166,236,223,146,254,137,184,46,70,103,23,155,84,2,112,39,236,187,72,240,113,222,47,76,201,48,128,249,219,85,231,69,99,156,160,63,107,249,199,131,211,23,104,54,193,114,15,138,121,203,55,93,228,174,80,225,92,64,255,84,78,37,152,232,246,115,136,139,174,22,239,55,22,248,64,130,4,157,39,62,188,36,31,233,33,65,120,85,153,175,215,224,139,202,176,92,51,59,182,89,237,94,209,229,85,176,126,80,71,213,25,236,255,108,33,59,98,9,70,135,218,231,233,50,200,130,142,142,112,212,158,237,40,177,249,81,144,95,86,228,130,58,49,88,58,131,9,143,167,230,110,51,31,8,193,134,13,109,166,58,181,164,225,64,189,193,134,252,5,47,41,73,23,74,78,245,175,243,118,34,50,150,17,158,138,120,190,43,152,29,217,151,32,75,201,244,120,46,174,72,192,192,1,253,210,165,102,65,106,28,94,150,247,121,57,42,79,151,150,159,93,242,241,35,229,5,25,107,77,96,126,215,245,142,209,98,231,235,182,222,95,82,142,9,194,55,233,181,122,217,70,0,104,188,33,188,208,234,49,223,136,143,86,99,48,97,249,214,34,4,158,106,154,189,166,189,7,216,193,1,191,54,110,180,173,83,9,8,21,154,78,114,29,255,41,206,165,17,134,123,183,116,225,199,15,205,217,16,146,168,190,172,42,70,17,25,56,35,118,165,128,117,102,198,216,16,1,122,96,254,174,207,114,155,201,115,202,34,241,164,87,71,150,24,239,169,57,173,253,204,94,17,69,6,238,77,118,99,137,241,206,141,38,68,220,232,65,248,100,81,121,47,249,52,30,147,65,218,177,38,83,191,214,154,235,233,198,249,179,140,161,69,11,98,14,240,25,7,105,76,161,190,81,155,60,219,54,39,132,53,153,146,150,80,254,46,46,153,185,84,38,252,222,232,158,18,113,93,140,119,22,225,52,206,46,54,169,171,73,138,17,69,230,63,3,32,129,131,187,118,145,224,227,19,246,92,91,253,89,233,73,152,62,85,241,33,6,130,108,68,97,62,212,170,206,139,198,207,169,55,126,56,65,127,214,93,38,195,110,179,137,118,124,214,238,202,196,111,214,29,89,10,177,161,225,228,30,20,243,129,121,168,75,215,105,203,19,178,14,119,171,92,161,194,185,57,198,126,1,128,254,169,156,229,153,21,36,11,54,160,54,110,81,28,142,167,22,102,134,194,113,218,62,44,222,111,44,73,185,211,148,240,129,4,9,149,230,184,177,123,73,13,163,30,46,177,27,72,62,210,67,45,89,110,251,195,246,219,233,166,145,103,81,31,169,176,204,122,206,12,116,148,97,185,102,241,6,5,222,0,0,0,0,119,7,48,150,238,14,97,44,153,9,81,186,7,109,196,25,112,106,244,143,233,99,165,53,158,100,149,163,14,219,136,50,121,220,184,164,224,213,233,30,151,210,217,136,9,182,76,43,126,177,124,189,231,184,45,7,144,191,29,145,29,183,16,100,106,176,32,242,243,185,113,72,132,190,65,222,26,218,212,125,109,221,228,235,244,212,181,81,131,211,133,199,19,108,152,86,100,107,168,192,253,98,249,122,138,101,201,236,20,1,92,79,99,6,108,217,250,15,61,99,141,8,13,245,59,110,32,200,76,105,16,94,213,96,65,228,162,103,113,114,60,3,228,209,75,4,212,71,210,13,133,253,165,10,181,107,53,181,168,250,66,178,152,108,219,187,201,214,172,188,249,64,50,216,108,227,69,223,92,117,220,214,13,207,171,209,61,89,38,217,48,172,81,222,0,58,200,215,81,128,191,208,97,22,33,180,244,181,86,179,196,35,207,186,149,153,184,189,165,15,40,2,184,158,95,5,136,8,198,12,217,178,177,11,233,36,47,111,124,135,88,104,76,17,193,97,29,171,182,102,45,61,118,220,65,144,1,219,113,6,152,210,32,188,239,213,16,42,113,177,133,137,6,182,181,31,159,191,228,165,232,184,212,51,120,7,201,162,15,0,249,52,150,9,168,142,225,14,152,24,127,106,13,187,8,109,61,45,145,100,108,151,230,99,92,1,107,107,81,244,28,108,97,98,133,101,48,216,242,98,0,78,108,6,149,237,27,1,165,123,130,8,244,193,245,15,196,87,101,176,217,198,18,183,233,80,139,190,184,234,252,185,136,124,98,221,29,223,21,218,45,73,140,211,124,243,251,212,76,101,77,178,97,88,58,181,81,206,163,188,0,116,212,187,48,226,74,223,165,65,61,216,149,215,164,209,196,109,211,214,244,251,67,105,233,106,52,110,217,252,173,103,136,70,218,96,184,208,68,4,45,115,51,3,29,229,170,10,76,95,221,13,124,201,80,5,113,60,39,2,65,170,190,11,16,16,201,12,32,134,87,104,181,37,32,111,133,179,185,102,212,9,206,97,228,159,94,222,249,14,41,217,201,152,176,208,152,34,199,215,168,180,89,179,61,23,46,180,13,129,183,189,92,59,192,186,108,173,237,184,131,32,154,191,179,182,3,182,226,12,116,177,210,154,234,213,71,57,157,210,119,175,4,219,38,21,115,220,22,131,227,99,11,18,148,100,59,132,13,109,106,62,122,106,90,168,228,14,207,11,147,9,255,157,10,0,174,39,125,7,158,177,240,15,147,68,135,8,163,210,30,1,242,104,105,6,194,254,247,98,87,93,128,101,103,203,25,108,54,113,110,107,6,231,254,212,27,118,137,211,43,224,16,218,122,90,103,221,74,204,249,185,223,111,142,190,239,249,23,183,190,67,96,176,142,213,214,214,163,232,161,209,147,126,56,216,194,196,79,223,242,82,209,187,103,241,166,188,87,103,63,181,6,221,72,178,54,75,216,13,43,218,175,10,27,76,54,3,74,246,65,4,122,96,223,96,239,195,168,103,223,85,49,110,142,239,70,105,190,121,203,97,179,140,188,102,131,26,37,111,210,160,82,104,226,54,204,12,119,149,187,11,71,3,34,2,22,185,85,5,38,47,197,186,59,190,178,189,11,40,43,180,90,146,92,179,106,4,194,215,255,167,181,208,207,49,44,217,158,139,91,222,174,29,155,100,194,176,236,99,242,38,117,106,163,156,2,109,147,10,156,9,6,169,235,14,54,63,114,7,103,133,5,0,87,19,149,191,74,130,226,184,122,20,123,177,43,174,12,182,27,56,146,210,142,155,229,213,190,13,124,220,239,183,11,219,223,33,134,211,210,212,241,212,226,66,104,221,179,248,31,218,131,110,129,190,22,205,246,185,38,91,111,176,119,225,24,183,71,119,136,8,90,230,255,15,106,112,102,6,59,202,17,1,11,92,143,101,158,255,248,98,174,105,97,107,255,211,22,108,207,69,160,10,226,120,215,13,210,238,78,4,131,84,57,3,179,194,167,103,38,97,208,96,22,247,73,105,71,77,62,110,119,219,174,209,106,74,217,214,90,220,64,223,11,102,55,216,59,240,169,188,174,83,222,187,158,197,71,178,207,127,48,181,255,233,189,189,242,28,202,186,194,138,83,179,147,48,36,180,163,166,186,208,54,5,205,215,6,147,84,222,87,41,35,217,103,191,179,102,122,46,196,97,74,184,93,104,27,2,42,111,43,148,180,11,190,55,195,12,142,161,90,5,223,27,45,2,239,141,0,0,0,0,25,27,49,65,50,54,98,130,43,45,83,195,100,108,197,4,125,119,244,69,86,90,167,134,79,65,150,199,200,217,138,8,209,194,187,73,250,239,232,138,227,244,217,203,172,181,79,12,181,174,126,77,158,131,45,142,135,152,28,207,74,194,18,81,83,217,35,16,120,244,112,211,97,239,65,146,46,174,215,85,55,181,230,20,28,152,181,215,5,131,132,150,130,27,152,89,155,0,169,24,176,45,250,219,169,54,203,154,230,119,93,93,255,108,108,28,212,65,63,223,205,90,14,158,149,132,36,162,140,159,21,227,167,178,70,32,190,169,119,97,241,232,225,166,232,243,208,231,195,222,131,36,218,197,178,101,93,93,174,170,68,70,159,235,111,107,204,40,118,112,253,105,57,49,107,174,32,42,90,239,11,7,9,44,18,28,56,109,223,70,54,243,198,93,7,178,237,112,84,113,244,107,101,48,187,42,243,247,162,49,194,182,137,28,145,117,144,7,160,52,23,159,188,251,14,132,141,186,37,169,222,121,60,178,239,56,115,243,121,255,106,232,72,190,65,197,27,125,88,222,42,60,240,121,79,5,233,98,126,68,194,79,45,135,219,84,28,198,148,21,138,1,141,14,187,64,166,35,232,131,191,56,217,194,56,160,197,13,33,187,244,76,10,150,167,143,19,141,150,206,92,204,0,9,69,215,49,72,110,250,98,139,119,225,83,202,186,187,93,84,163,160,108,21,136,141,63,214,145,150,14,151,222,215,152,80,199,204,169,17,236,225,250,210,245,250,203,147,114,98,215,92,107,121,230,29,64,84,181,222,89,79,132,159,22,14,18,88,15,21,35,25,36,56,112,218,61,35,65,155,101,253,107,167,124,230,90,230,87,203,9,37,78,208,56,100,1,145,174,163,24,138,159,226,51,167,204,33,42,188,253,96,173,36,225,175,180,63,208,238,159,18,131,45,134,9,178,108,201,72,36,171,208,83,21,234,251,126,70,41,226,101,119,104,47,63,121,246,54,36,72,183,29,9,27,116,4,18,42,53,75,83,188,242,82,72,141,179,121,101,222,112,96,126,239,49,231,230,243,254,254,253,194,191,213,208,145,124,204,203,160,61,131,138,54,250,154,145,7,187,177,188,84,120,168,167,101,57,59,131,152,75,34,152,169,10,9,181,250,201,16,174,203,136,95,239,93,79,70,244,108,14,109,217,63,205,116,194,14,140,243,90,18,67,234,65,35,2,193,108,112,193,216,119,65,128,151,54,215,71,142,45,230,6,165,0,181,197,188,27,132,132,113,65,138,26,104,90,187,91,67,119,232,152,90,108,217,217,21,45,79,30,12,54,126,95,39,27,45,156,62,0,28,221,185,152,0,18,160,131,49,83,139,174,98,144,146,181,83,209,221,244,197,22,196,239,244,87,239,194,167,148,246,217,150,213,174,7,188,233,183,28,141,168,156,49,222,107,133,42,239,42,202,107,121,237,211,112,72,172,248,93,27,111,225,70,42,46,102,222,54,225,127,197,7,160,84,232,84,99,77,243,101,34,2,178,243,229,27,169,194,164,48,132,145,103,41,159,160,38,228,197,174,184,253,222,159,249,214,243,204,58,207,232,253,123,128,169,107,188,153,178,90,253,178,159,9,62,171,132,56,127,44,28,36,176,53,7,21,241,30,42,70,50,7,49,119,115,72,112,225,180,81,107,208,245,122,70,131,54,99,93,178,119,203,250,215,78,210,225,230,15,249,204,181,204,224,215,132,141,175,150,18,74,182,141,35,11,157,160,112,200,132,187,65,137,3,35,93,70,26,56,108,7,49,21,63,196,40,14,14,133,103,79,152,66,126,84,169,3,85,121,250,192,76,98,203,129,129,56,197,31,152,35,244,94,179,14,167,157,170,21,150,220,229,84,0,27,252,79,49,90,215,98,98,153,206,121,83,216,73,225,79,23,80,250,126,86,123,215,45,149,98,204,28,212,45,141,138,19,52,150,187,82,31,187,232,145,6,160,217,208,94,126,243,236,71,101,194,173,108,72,145,110,117,83,160,47,58,18,54,232,35,9,7,169,8,36,84,106,17,63,101,43,150,167,121,228,143,188,72,165,164,145,27,102,189,138,42,39,242,203,188,224,235,208,141,161,192,253,222,98,217,230,239,35,20,188,225,189,13,167,208,252,38,138,131,63,63,145,178,126,112,208,36,185,105,203,21,248,66,230,70,59,91,253,119,122,220,101,107,181,197,126,90,244,238,83,9,55,247,72,56,118,184,9,174,177,161,18,159,240,138,63,204,51,147,36,253,114,0,0,0,0,1,194,106,55,3,132,212,110,2,70,190,89,7,9,168,220,6,203,194,235,4,141,124,178,5,79,22,133,14,19,81,184,15,209,59,143,13,151,133,214,12,85,239,225,9,26,249,100,8,216,147,83,10,158,45,10,11,92,71,61,28,38,163,112,29,228,201,71,31,162,119,30,30,96,29,41,27,47,11,172,26,237,97,155,24,171,223,194,25,105,181,245,18,53,242,200,19,247,152,255,17,177,38,166,16,115,76,145,21,60,90,20,20,254,48,35,22,184,142,122,23,122,228,77,56,77,70,224,57,143,44,215,59,201,146,142,58,11,248,185,63,68,238,60,62,134,132,11,60,192,58,82,61,2,80,101,54,94,23,88,55,156,125,111,53,218,195,54,52,24,169,1,49,87,191,132,48,149,213,179,50,211,107,234,51,17,1,221,36,107,229,144,37,169,143,167,39,239,49,254,38,45,91,201,35,98,77,76,34,160,39,123,32,230,153,34,33,36,243,21,42,120,180,40,43,186,222,31,41,252,96,70,40,62,10,113,45,113,28,244,44,179,118,195,46,245,200,154,47,55,162,173,112,154,141,192,113,88,231,247,115,30,89,174,114,220,51,153,119,147,37,28,118,81,79,43,116,23,241,114,117,213,155,69,126,137,220,120,127,75,182,79,125,13,8,22,124,207,98,33,121,128,116,164,120,66,30,147,122,4,160,202,123,198,202,253,108,188,46,176,109,126,68,135,111,56,250,222,110,250,144,233,107,181,134,108,106,119,236,91,104,49,82,2,105,243,56,53,98,175,127,8,99,109,21,63,97,43,171,102,96,233,193,81,101,166,215,212,100,100,189,227,102,34,3,186,103,224,105,141,72,215,203,32,73,21,161,23,75,83,31,78,74,145,117,121,79,222,99,252,78,28,9,203,76,90,183,146,77,152,221,165,70,196,154,152,71,6,240,175,69,64,78,246,68,130,36,193,65,205,50,68,64,15,88,115,66,73,230,42,67,139,140,29,84,241,104,80,85,51,2,103,87,117,188,62,86,183,214,9,83,248,192,140,82,58,170,187,80,124,20,226,81,190,126,213,90,226,57,232,91,32,83,223,89,102,237,134,88,164,135,177,93,235,145,52,92,41,251,3,94,111,69,90,95,173,47,109,225,53,27,128,224,247,113,183,226,177,207,238,227,115,165,217,230,60,179,92,231,254,217,107,229,184,103,50,228,122,13,5,239,38,74,56,238,228,32,15,236,162,158,86,237,96,244,97,232,47,226,228,233,237,136,211,235,171,54,138,234,105,92,189,253,19,184,240,252,209,210,199,254,151,108,158,255,85,6,169,250,26,16,44,251,216,122,27,249,158,196,66,248,92,174,117,243,0,233,72,242,194,131,127,240,132,61,38,241,70,87,17,244,9,65,148,245,203,43,163,247,141,149,250,246,79,255,205,217,120,93,96,216,186,55,87,218,252,137,14,219,62,227,57,222,113,245,188,223,179,159,139,221,245,33,210,220,55,75,229,215,107,12,216,214,169,102,239,212,239,216,182,213,45,178,129,208,98,164,4,209,160,206,51,211,230,112,106,210,36,26,93,197,94,254,16,196,156,148,39,198,218,42,126,199,24,64,73,194,87,86,204,195,149,60,251,193,211,130,162,192,17,232,149,203,77,175,168,202,143,197,159,200,201,123,198,201,11,17,241,204,68,7,116,205,134,109,67,207,192,211,26,206,2,185,45,145,175,150,64,144,109,252,119,146,43,66,46,147,233,40,25,150,166,62,156,151,100,84,171,149,34,234,242,148,224,128,197,159,188,199,248,158,126,173,207,156,56,19,150,157,250,121,161,152,181,111,36,153,119,5,19,155,49,187,74,154,243,209,125,141,137,53,48,140,75,95,7,142,13,225,94,143,207,139,105,138,128,157,236,139,66,247,219,137,4,73,130,136,198,35,181,131,154,100,136,130,88,14,191,128,30,176,230,129,220,218,209,132,147,204,84,133,81,166,99,135,23,24,58,134,213,114,13,169,226,208,160,168,32,186,151,170,102,4,206,171,164,110,249], "i8", ALLOC_NONE, Runtime.GLOBAL_BASE);
/* memory initializer */ allocate([174,235,120,124,175,41,18,75,173,111,172,18,172,173,198,37,167,241,129,24,166,51,235,47,164,117,85,118,165,183,63,65,160,248,41,196,161,58,67,243,163,124,253,170,162,190,151,157,181,196,115,208,180,6,25,231,182,64,167,190,183,130,205,137,178,205,219,12,179,15,177,59,177,73,15,98,176,139,101,85,187,215,34,104,186,21,72,95,184,83,246,6,185,145,156,49,188,222,138,180,189,28,224,131,191,90,94,218,190,152,52,237,0,0,0,0,184,188,103,101,170,9,200,139,18,181,175,238,143,98,151,87,55,222,240,50,37,107,95,220,157,215,56,185,197,180,40,239,125,8,79,138,111,189,224,100,215,1,135,1,74,214,191,184,242,106,216,221,224,223,119,51,88,99,16,86,80,25,87,159,232,165,48,250,250,16,159,20,66,172,248,113,223,123,192,200,103,199,167,173,117,114,8,67,205,206,111,38,149,173,127,112,45,17,24,21,63,164,183,251,135,24,208,158,26,207,232,39,162,115,143,66,176,198,32,172,8,122,71,201,160,50,175,62,24,142,200,91,10,59,103,181,178,135,0,208,47,80,56,105,151,236,95,12,133,89,240,226,61,229,151,135,101,134,135,209,221,58,224,180,207,143,79,90,119,51,40,63,234,228,16,134,82,88,119,227,64,237,216,13,248,81,191,104,240,43,248,161,72,151,159,196,90,34,48,42,226,158,87,79,127,73,111,246,199,245,8,147,213,64,167,125,109,252,192,24,53,159,208,78,141,35,183,43,159,150,24,197,39,42,127,160,186,253,71,25,2,65,32,124,16,244,143,146,168,72,232,247,155,20,88,61,35,168,63,88,49,29,144,182,137,161,247,211,20,118,207,106,172,202,168,15,190,127,7,225,6,195,96,132,94,160,112,210,230,28,23,183,244,169,184,89,76,21,223,60,209,194,231,133,105,126,128,224,123,203,47,14,195,119,72,107,203,13,15,162,115,177,104,199,97,4,199,41,217,184,160,76,68,111,152,245,252,211,255,144,238,102,80,126,86,218,55,27,14,185,39,77,182,5,64,40,164,176,239,198,28,12,136,163,129,219,176,26,57,103,215,127,43,210,120,145,147,110,31,244,59,38,247,3,131,154,144,102,145,47,63,136,41,147,88,237,180,68,96,84,12,248,7,49,30,77,168,223,166,241,207,186,254,146,223,236,70,46,184,137,84,155,23,103,236,39,112,2,113,240,72,187,201,76,47,222,219,249,128,48,99,69,231,85,107,63,160,156,211,131,199,249,193,54,104,23,121,138,15,114,228,93,55,203,92,225,80,174,78,84,255,64,246,232,152,37,174,139,136,115,22,55,239,22,4,130,64,248,188,62,39,157,33,233,31,36,153,85,120,65,139,224,215,175,51,92,176,202,237,89,182,59,85,229,209,94,71,80,126,176,255,236,25,213,98,59,33,108,218,135,70,9,200,50,233,231,112,142,142,130,40,237,158,212,144,81,249,177,130,228,86,95,58,88,49,58,167,143,9,131,31,51,110,230,13,134,193,8,181,58,166,109,189,64,225,164,5,252,134,193,23,73,41,47,175,245,78,74,50,34,118,243,138,158,17,150,152,43,190,120,32,151,217,29,120,244,201,75,192,72,174,46,210,253,1,192,106,65,102,165,247,150,94,28,79,42,57,121,93,159,150,151,229,35,241,242,77,107,25,5,245,215,126,96,231,98,209,142,95,222,182,235,194,9,142,82,122,181,233,55,104,0,70,217,208,188,33,188,136,223,49,234,48,99,86,143,34,214,249,97,154,106,158,4,7,189,166,189,191,1,193,216,173,180,110,54,21,8,9,83,29,114,78,154,165,206,41,255,183,123,134,17,15,199,225,116,146,16,217,205,42,172,190,168,56,25,17,70,128,165,118,35,216,198,102,117,96,122,1,16,114,207,174,254,202,115,201,155,87,164,241,34,239,24,150,71,253,173,57,169,69,17,94,204,118,77,238,6,206,241,137,99,220,68,38,141,100,248,65,232,249,47,121,81,65,147,30,52,83,38,177,218,235,154,214,191,179,249,198,233,11,69,161,140,25,240,14,98,161,76,105,7,60,155,81,190,132,39,54,219,150,146,153,53,46,46,254,80,38,84,185,153,158,232,222,252,140,93,113,18,52,225,22,119,169,54,46,206,17,138,73,171,3,63,230,69,187,131,129,32,227,224,145,118,91,92,246,19,73,233,89,253,241,85,62,152,108,130,6,33,212,62,97,68,198,139,206,170,126,55,169,207,214,127,65,56,110,195,38,93,124,118,137,179,196,202,238,214,89,29,214,111,225,161,177,10,243,20,30,228,75,168,121,129,19,203,105,215,171,119,14,178,185,194,161,92,1,126,198,57,156,169,254,128,36,21,153,229,54,160,54,11,142,28,81,110,134,102,22,167,62,218,113,194,44,111,222,44,148,211,185,73,9,4,129,240,177,184,230,149,163,13,73,123,27,177,46,30,67,210,62,72,251,110,89,45,233,219,246,195,81,103,145,166,204,176,169,31,116,12,206,122,102,185,97,148,222,5,6,241,16,0,17,0,18,0,0,0,8,0,7,0,9,0,6,0,10,0,5,0,11,0,4,0,12,0,3,0,13,0,2,0,14,0,1,0,15,0,0,0,105,110,99,111,114,114,101,99,116,32,104,101,97,100,101,114,32,99,104,101,99,107,0,0,117,110,107,110,111,119,110,32,99,111,109,112,114,101,115,115,105,111,110,32,109,101,116,104,111,100,0,0,0,0,0,0,105,110,118,97,108,105,100,32,119,105,110,100,111,119,32,115,105,122,101,0,0,0,0,0,117,110,107,110,111,119,110,32,104,101,97,100,101,114,32,102,108,97,103,115,32,115,101,116,0,0,0,0,0,0,0,0,104,101,97,100,101,114,32,99,114,99,32,109,105,115,109,97,116,99,104,0,0,0,0,0,105,110,118,97,108,105,100,32,98,108,111,99,107,32,116,121,112,101,0,0,0,0,0,0,105,110,118,97,108,105,100,32,115,116,111,114,101,100,32,98,108,111,99,107,32,108,101,110,103,116,104,115,0,0,0,0,116,111,111,32,109,97,110,121,32,108,101,110,103,116,104,32,111,114,32,100,105,115,116,97,110,99,101,32,115,121,109,98,111,108,115,0,0,0,0,0,105,110,118,97,108,105,100,32,99,111,100,101,32,108,101,110,103,116,104,115,32,115,101,116,0,0,0,0,0,0,0,0,105,110,118,97,108,105,100,32,98,105,116,32,108,101,110,103,116,104,32,114,101,112,101,97,116,0,0,0,0,0,0,0,105,110,118,97,108,105,100,32,99,111,100,101,32,45,45,32,109,105,115,115,105,110,103,32,101,110,100,45,111,102,45,98,108,111,99,107,0,0,0,0,105,110,118,97,108,105,100,32,108,105,116,101,114,97,108,47,108,101,110,103,116,104,115,32,115,101,116,0,0,0,0,0,105,110,118,97,108,105,100,32,100,105,115,116,97,110,99,101,115,32,115,101,116,0,0,0,105,110,118,97,108,105,100,32,108,105,116,101,114,97,108,47,108,101,110,103,116,104,32,99,111,100,101,0,0,0,0,0,105,110,118,97,108,105,100,32,100,105,115,116,97,110,99,101,32,99,111,100,101,0,0,0,105,110,118,97,108,105,100,32,100,105,115,116,97,110,99,101,32,116,111,111,32,102,97,114,32,98,97,99,107,0,0,0,105,110,99,111,114,114,101,99,116,32,100,97,116,97,32,99,104,101,99,107,0,0,0,0,105,110,99,111,114,114,101,99,116,32,108,101,110,103,116,104,32,99,104,101,99,107,0,0,96,7,0,0,0,8,80,0,0,8,16,0,20,8,115,0,18,7,31,0,0,8,112,0,0,8,48,0,0,9,192,0,16,7,10,0,0,8,96,0,0,8,32,0,0,9,160,0,0,8,0,0,0,8,128,0,0,8,64,0,0,9,224,0,16,7,6,0,0,8,88,0,0,8,24,0,0,9,144,0,19,7,59,0,0,8,120,0,0,8,56,0,0,9,208,0,17,7,17,0,0,8,104,0,0,8,40,0,0,9,176,0,0,8,8,0,0,8,136,0,0,8,72,0,0,9,240,0,16,7,4,0,0,8,84,0,0,8,20,0,21,8,227,0,19,7,43,0,0,8,116,0,0,8,52,0,0,9,200,0,17,7,13,0,0,8,100,0,0,8,36,0,0,9,168,0,0,8,4,0,0,8,132,0,0,8,68,0,0,9,232,0,16,7,8,0,0,8,92,0,0,8,28,0,0,9,152,0,20,7,83,0,0,8,124,0,0,8,60,0,0,9,216,0,18,7,23,0,0,8,108,0,0,8,44,0,0,9,184,0,0,8,12,0,0,8,140,0,0,8,76,0,0,9,248,0,16,7,3,0,0,8,82,0,0,8,18,0,21,8,163,0,19,7,35,0,0,8,114,0,0,8,50,0,0,9,196,0,17,7,11,0,0,8,98,0,0,8,34,0,0,9,164,0,0,8,2,0,0,8,130,0,0,8,66,0,0,9,228,0,16,7,7,0,0,8,90,0,0,8,26,0,0,9,148,0,20,7,67,0,0,8,122,0,0,8,58,0,0,9,212,0,18,7,19,0,0,8,106,0,0,8,42,0,0,9,180,0,0,8,10,0,0,8,138,0,0,8,74,0,0,9,244,0,16,7,5,0,0,8,86,0,0,8,22,0,64,8,0,0,19,7,51,0,0,8,118,0,0,8,54,0,0,9,204,0,17,7,15,0,0,8,102,0,0,8,38,0,0,9,172,0,0,8,6,0,0,8,134,0,0,8,70,0,0,9,236,0,16,7,9,0,0,8,94,0,0,8,30,0,0,9,156,0,20,7,99,0,0,8,126,0,0,8,62,0,0,9,220,0,18,7,27,0,0,8,110,0,0,8,46,0,0,9,188,0,0,8,14,0,0,8,142,0,0,8,78,0,0,9,252,0,96,7,0,0,0,8,81,0,0,8,17,0,21,8,131,0,18,7,31,0,0,8,113,0,0,8,49,0,0,9,194,0,16,7,10,0,0,8,97,0,0,8,33,0,0,9,162,0,0,8,1,0,0,8,129,0,0,8,65,0,0,9,226,0,16,7,6,0,0,8,89,0,0,8,25,0,0,9,146,0,19,7,59,0,0,8,121,0,0,8,57,0,0,9,210,0,17,7,17,0,0,8,105,0,0,8,41,0,0,9,178,0,0,8,9,0,0,8,137,0,0,8,73,0,0,9,242,0,16,7,4,0,0,8,85,0,0,8,21,0,16,8,2,1,19,7,43,0,0,8,117,0,0,8,53,0,0,9,202,0,17,7,13,0,0,8,101,0,0,8,37,0,0,9,170,0,0,8,5,0,0,8,133,0,0,8,69,0,0,9,234,0,16,7,8,0,0,8,93,0,0,8,29,0,0,9,154,0,20,7,83,0,0,8,125,0,0,8,61,0,0,9,218,0,18,7,23,0,0,8,109,0,0,8,45,0,0,9,186,0,0,8,13,0,0,8,141,0,0,8,77,0,0,9,250,0,16,7,3,0,0,8,83,0,0,8,19,0,21,8,195,0,19,7,35,0,0,8,115,0,0,8,51,0,0,9,198,0,17,7,11,0,0,8,99,0,0,8,35,0,0,9,166,0,0,8,3,0,0,8,131,0,0,8,67,0,0,9,230,0,16,7,7,0,0,8,91,0,0,8,27,0,0,9,150,0,20,7,67,0,0,8,123,0,0,8,59,0,0,9,214,0,18,7,19,0,0,8,107,0,0,8,43,0,0,9,182,0,0,8,11,0,0,8,139,0,0,8,75,0,0,9,246,0,16,7,5,0,0,8,87,0,0,8,23,0,64,8,0,0,19,7,51,0,0,8,119,0,0,8,55,0,0,9,206,0,17,7,15,0,0,8,103,0,0,8,39,0,0,9,174,0,0,8,7,0,0,8,135,0,0,8,71,0,0,9,238,0,16,7,9,0,0,8,95,0,0,8,31,0,0,9,158,0,20,7,99,0,0,8,127,0,0,8,63,0,0,9,222,0,18,7,27,0,0,8,111,0,0,8,47,0,0,9,190,0,0,8,15,0,0,8,143,0,0,8,79,0,0,9,254,0,96,7,0,0,0,8,80,0,0,8,16,0,20,8,115,0,18,7,31,0,0,8,112,0,0,8,48,0,0,9,193,0,16,7,10,0,0,8,96,0,0,8,32,0,0,9,161,0,0,8,0,0,0,8,128,0,0,8,64,0,0,9,225,0,16,7,6,0,0,8,88,0,0,8,24,0,0,9,145,0,19,7,59,0,0,8,120,0,0,8,56,0,0,9,209,0,17,7,17,0,0,8,104,0,0,8,40,0,0,9,177,0,0,8,8,0,0,8,136,0,0,8,72,0,0,9,241,0,16,7,4,0,0,8,84,0,0,8,20,0,21,8,227,0,19,7,43,0,0,8,116,0,0,8,52,0,0,9,201,0,17,7,13,0,0,8,100,0,0,8,36,0,0,9,169,0,0,8,4,0,0,8,132,0,0,8,68,0,0,9,233,0,16,7,8,0,0,8,92,0,0,8,28,0,0,9,153,0,20,7,83,0,0,8,124,0,0,8,60,0,0,9,217,0,18,7,23,0,0,8,108,0,0,8,44,0,0,9,185,0,0,8,12,0,0,8,140,0,0,8,76,0,0,9,249,0,16,7,3,0,0,8,82,0,0,8,18,0,21,8,163,0,19,7,35,0,0,8,114,0,0,8,50,0,0,9,197,0,17,7,11,0,0,8,98,0,0,8,34,0,0,9,165,0,0,8,2,0,0,8,130,0,0,8,66,0,0,9,229,0,16,7,7,0,0,8,90,0,0,8,26,0,0,9,149,0,20,7,67,0,0,8,122,0,0,8,58,0,0,9,213,0,18,7,19,0,0,8,106,0,0,8,42,0,0,9,181,0,0,8,10,0,0,8,138,0,0,8,74,0,0,9,245,0,16,7,5,0,0,8,86,0,0,8,22,0,64,8,0,0,19,7,51,0,0,8,118,0,0,8,54,0,0,9,205,0,17,7,15,0,0,8,102,0,0,8,38,0,0,9,173,0,0,8,6,0,0,8,134,0,0,8,70,0,0,9,237,0,16,7,9,0,0,8,94,0,0,8,30,0,0,9,157,0,20,7,99,0,0,8,126,0,0,8,62,0,0,9,221,0,18,7,27,0,0,8,110,0,0,8,46,0,0,9,189,0,0,8,14,0,0,8,142,0,0,8,78,0,0,9,253,0,96,7,0,0,0,8,81,0,0,8,17,0,21,8,131,0,18,7,31,0,0,8,113,0,0,8,49,0,0,9,195,0,16,7,10,0,0,8,97,0,0,8,33,0,0,9,163,0,0,8,1,0,0,8,129,0,0,8,65,0,0,9,227,0,16,7,6,0,0,8,89,0,0,8,25,0,0,9,147,0,19,7,59,0,0,8,121,0,0,8,57,0,0,9,211,0,17,7,17,0,0,8,105,0,0,8,41,0,0,9,179,0,0,8,9,0,0,8,137,0,0,8,73,0,0,9,243,0,16,7,4,0,0,8,85,0,0,8,21,0,16,8,2,1,19,7,43,0,0,8,117,0,0,8,53,0,0,9,203,0,17,7,13,0,0,8,101,0,0,8,37,0,0,9,171,0,0,8,5,0,0,8,133,0,0,8,69,0,0,9,235,0,16,7,8,0,0,8,93,0,0,8,29,0,0,9,155,0,20,7,83,0,0,8,125,0,0,8,61,0,0,9,219,0,18,7,23,0,0,8,109,0,0,8,45,0,0,9,187,0,0,8,13,0,0,8,141,0,0,8,77,0,0,9,251,0,16,7,3,0,0,8,83,0,0,8,19,0,21,8,195,0,19,7,35,0,0,8,115,0,0,8,51,0,0,9,199,0,17,7,11,0,0,8,99,0,0,8,35,0,0,9,167,0,0,8,3,0,0,8,131,0,0,8,67,0,0,9,231,0,16,7,7,0,0,8,91,0,0,8,27,0,0,9,151,0,20,7,67,0,0,8,123,0,0,8,59,0,0,9,215,0,18,7,19,0,0,8,107,0,0,8,43,0,0,9,183,0,0,8,11,0,0,8,139,0,0,8,75,0,0,9,247,0,16,7,5,0,0,8,87,0,0,8,23,0,64,8,0,0,19,7,51,0,0,8,119,0,0,8,55,0,0,9,207,0,17,7,15,0,0,8,103,0,0,8,39,0,0,9,175,0,0,8,7,0,0,8,135,0,0,8,71,0,0,9,239,0,16,7,9,0,0,8,95,0,0,8,31,0,0,9,159,0,20,7,99,0,0,8,127,0,0,8,63,0,0,9,223,0,18,7,27,0,0,8,111,0,0,8,47,0,0,9,191,0,0,8,15,0,0,8,143,0,0,8,79,0,0,9,255,0,16,5,1,0,23,5,1,1,19,5,17,0,27,5,1,16,17,5,5,0,25,5,1,4,21,5,65,0,29,5,1,64,16,5,3,0,24,5,1,2,20,5,33,0,28,5,1,32,18,5,9,0,26,5,1,8,22,5,129,0,64,5,0,0,16,5,2,0,23,5,129,1,19,5,25,0,27,5,1,24,17,5,7,0,25,5,1,6,21,5,97,0,29,5,1,96,16,5,4,0,24,5,1,3,20,5,49,0,28,5,1,48,18,5,13,0,26,5,1,12,22,5,193,0,64,5,0,0,3,0,4,0,5,0,6,0,7,0,8,0,9,0,10,0,11,0,13,0,15,0,17,0,19,0,23,0,27,0,31,0,35,0,43,0,51,0,59,0,67,0,83,0,99,0,115,0,131,0,163,0,195,0,227,0,2,1,0,0,0,0,0,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,17,0,17,0,17,0,17,0,18,0,18,0,18,0,18,0,19,0,19,0,19,0,19,0,20,0,20,0,20,0,20,0,21,0,21,0,21,0,21,0,16,0,73,0,195,0,0,0,1,0,2,0,3,0,4,0,5,0,7,0,9,0,13,0,17,0,25,0,33,0,49,0,65,0,97,0,129,0,193,0,1,1,129,1,1,2,1,3,1,4,1,6,1,8,1,12,1,16,1,24,1,32,1,48,1,64,1,96,0,0,0,0,16,0,16,0,16,0,16,0,17,0,17,0,18,0,18,0,19,0,19,0,20,0,20,0,21,0,21,0,22,0,22,0,23,0,23,0,24,0,24,0,25,0,25,0,26,0,26,0,27,0,27,0,28,0,28,0,29,0,29,0,64,0,64,0,105,110,118,97,108,105,100,32,100,105,115,116,97,110,99,101,32,116,111,111,32,102,97,114,32,98,97,99,107,0,0,0,105,110,118,97,108,105,100,32,100,105,115,116,97,110,99,101,32,99,111,100,101,0,0,0,105,110,118,97,108,105,100,32,108,105,116,101,114,97,108,47,108,101,110,103,116,104,32,99,111,100,101,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], "i8", ALLOC_NONE, Runtime.GLOBAL_BASE+10240);




var tempDoublePtr = Runtime.alignMemory(allocate(12, "i8", ALLOC_STATIC), 8);

assert(tempDoublePtr % 8 == 0);

function copyTempFloat(ptr) { // functions, because inlining this code increases code size too much

  HEAP8[tempDoublePtr] = HEAP8[ptr];

  HEAP8[tempDoublePtr+1] = HEAP8[ptr+1];

  HEAP8[tempDoublePtr+2] = HEAP8[ptr+2];

  HEAP8[tempDoublePtr+3] = HEAP8[ptr+3];

}

function copyTempDouble(ptr) {

  HEAP8[tempDoublePtr] = HEAP8[ptr];

  HEAP8[tempDoublePtr+1] = HEAP8[ptr+1];

  HEAP8[tempDoublePtr+2] = HEAP8[ptr+2];

  HEAP8[tempDoublePtr+3] = HEAP8[ptr+3];

  HEAP8[tempDoublePtr+4] = HEAP8[ptr+4];

  HEAP8[tempDoublePtr+5] = HEAP8[ptr+5];

  HEAP8[tempDoublePtr+6] = HEAP8[ptr+6];

  HEAP8[tempDoublePtr+7] = HEAP8[ptr+7];

}






  var ERRNO_CODES={EPERM:1,ENOENT:2,ESRCH:3,EINTR:4,EIO:5,ENXIO:6,E2BIG:7,ENOEXEC:8,EBADF:9,ECHILD:10,EAGAIN:11,EWOULDBLOCK:11,ENOMEM:12,EACCES:13,EFAULT:14,ENOTBLK:15,EBUSY:16,EEXIST:17,EXDEV:18,ENODEV:19,ENOTDIR:20,EISDIR:21,EINVAL:22,ENFILE:23,EMFILE:24,ENOTTY:25,ETXTBSY:26,EFBIG:27,ENOSPC:28,ESPIPE:29,EROFS:30,EMLINK:31,EPIPE:32,EDOM:33,ERANGE:34,ENOMSG:42,EIDRM:43,ECHRNG:44,EL2NSYNC:45,EL3HLT:46,EL3RST:47,ELNRNG:48,EUNATCH:49,ENOCSI:50,EL2HLT:51,EDEADLK:35,ENOLCK:37,EBADE:52,EBADR:53,EXFULL:54,ENOANO:55,EBADRQC:56,EBADSLT:57,EDEADLOCK:35,EBFONT:59,ENOSTR:60,ENODATA:61,ETIME:62,ENOSR:63,ENONET:64,ENOPKG:65,EREMOTE:66,ENOLINK:67,EADV:68,ESRMNT:69,ECOMM:70,EPROTO:71,EMULTIHOP:72,EDOTDOT:73,EBADMSG:74,ENOTUNIQ:76,EBADFD:77,EREMCHG:78,ELIBACC:79,ELIBBAD:80,ELIBSCN:81,ELIBMAX:82,ELIBEXEC:83,ENOSYS:38,ENOTEMPTY:39,ENAMETOOLONG:36,ELOOP:40,EOPNOTSUPP:95,EPFNOSUPPORT:96,ECONNRESET:104,ENOBUFS:105,EAFNOSUPPORT:97,EPROTOTYPE:91,ENOTSOCK:88,ENOPROTOOPT:92,ESHUTDOWN:108,ECONNREFUSED:111,EADDRINUSE:98,ECONNABORTED:103,ENETUNREACH:101,ENETDOWN:100,ETIMEDOUT:110,EHOSTDOWN:112,EHOSTUNREACH:113,EINPROGRESS:115,EALREADY:114,EDESTADDRREQ:89,EMSGSIZE:90,EPROTONOSUPPORT:93,ESOCKTNOSUPPORT:94,EADDRNOTAVAIL:99,ENETRESET:102,EISCONN:106,ENOTCONN:107,ETOOMANYREFS:109,EUSERS:87,EDQUOT:122,ESTALE:116,ENOTSUP:95,ENOMEDIUM:123,EILSEQ:84,EOVERFLOW:75,ECANCELED:125,ENOTRECOVERABLE:131,EOWNERDEAD:130,ESTRPIPE:86};

  var ERRNO_MESSAGES={0:"Success",1:"Not super-user",2:"No such file or directory",3:"No such process",4:"Interrupted system call",5:"I/O error",6:"No such device or address",7:"Arg list too long",8:"Exec format error",9:"Bad file number",10:"No children",11:"No more processes",12:"Not enough core",13:"Permission denied",14:"Bad address",15:"Block device required",16:"Mount device busy",17:"File exists",18:"Cross-device link",19:"No such device",20:"Not a directory",21:"Is a directory",22:"Invalid argument",23:"Too many open files in system",24:"Too many open files",25:"Not a typewriter",26:"Text file busy",27:"File too large",28:"No space left on device",29:"Illegal seek",30:"Read only file system",31:"Too many links",32:"Broken pipe",33:"Math arg out of domain of func",34:"Math result not representable",35:"File locking deadlock error",36:"File or path name too long",37:"No record locks available",38:"Function not implemented",39:"Directory not empty",40:"Too many symbolic links",42:"No message of desired type",43:"Identifier removed",44:"Channel number out of range",45:"Level 2 not synchronized",46:"Level 3 halted",47:"Level 3 reset",48:"Link number out of range",49:"Protocol driver not attached",50:"No CSI structure available",51:"Level 2 halted",52:"Invalid exchange",53:"Invalid request descriptor",54:"Exchange full",55:"No anode",56:"Invalid request code",57:"Invalid slot",59:"Bad font file fmt",60:"Device not a stream",61:"No data (for no delay io)",62:"Timer expired",63:"Out of streams resources",64:"Machine is not on the network",65:"Package not installed",66:"The object is remote",67:"The link has been severed",68:"Advertise error",69:"Srmount error",70:"Communication error on send",71:"Protocol error",72:"Multihop attempted",73:"Cross mount point (not really error)",74:"Trying to read unreadable message",75:"Value too large for defined data type",76:"Given log. name not unique",77:"f.d. invalid for this operation",78:"Remote address changed",79:"Can   access a needed shared lib",80:"Accessing a corrupted shared lib",81:".lib section in a.out corrupted",82:"Attempting to link in too many libs",83:"Attempting to exec a shared library",84:"Illegal byte sequence",86:"Streams pipe error",87:"Too many users",88:"Socket operation on non-socket",89:"Destination address required",90:"Message too long",91:"Protocol wrong type for socket",92:"Protocol not available",93:"Unknown protocol",94:"Socket type not supported",95:"Not supported",96:"Protocol family not supported",97:"Address family not supported by protocol family",98:"Address already in use",99:"Address not available",100:"Network interface is not configured",101:"Network is unreachable",102:"Connection reset by network",103:"Connection aborted",104:"Connection reset by peer",105:"No buffer space available",106:"Socket is already connected",107:"Socket is not connected",108:"Can't send after socket shutdown",109:"Too many references",110:"Connection timed out",111:"Connection refused",112:"Host is down",113:"Host is unreachable",114:"Socket already connected",115:"Connection already in progress",116:"Stale file handle",122:"Quota exceeded",123:"No medium (in tape drive)",125:"Operation canceled",130:"Previous owner died",131:"State not recoverable"};


  var ___errno_state=0;function ___setErrNo(value) {
      // For convenient setting and returning of errno.
      HEAP32[((___errno_state)>>2)]=value;
      return value;
    }

  var PATH={splitPath:function (filename) {
        var splitPathRe = /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
        return splitPathRe.exec(filename).slice(1);
      },normalizeArray:function (parts, allowAboveRoot) {
        // if the path tries to go above the root, `up` ends up > 0
        var up = 0;
        for (var i = parts.length - 1; i >= 0; i--) {
          var last = parts[i];
          if (last === '.') {
            parts.splice(i, 1);
          } else if (last === '..') {
            parts.splice(i, 1);
            up++;
          } else if (up) {
            parts.splice(i, 1);
            up--;
          }
        }
        // if the path is allowed to go above the root, restore leading ..s
        if (allowAboveRoot) {
          for (; up--; up) {
            parts.unshift('..');
          }
        }
        return parts;
      },normalize:function (path) {
        var isAbsolute = path.charAt(0) === '/',
            trailingSlash = path.substr(-1) === '/';
        // Normalize the path
        path = PATH.normalizeArray(path.split('/').filter(function(p) {
          return !!p;
        }), !isAbsolute).join('/');
        if (!path && !isAbsolute) {
          path = '.';
        }
        if (path && trailingSlash) {
          path += '/';
        }
        return (isAbsolute ? '/' : '') + path;
      },dirname:function (path) {
        var result = PATH.splitPath(path),
            root = result[0],
            dir = result[1];
        if (!root && !dir) {
          // No dirname whatsoever
          return '.';
        }
        if (dir) {
          // It has a dirname, strip trailing slash
          dir = dir.substr(0, dir.length - 1);
        }
        return root + dir;
      },basename:function (path) {
        // EMSCRIPTEN return '/'' for '/', not an empty string
        if (path === '/') return '/';
        var lastSlash = path.lastIndexOf('/');
        if (lastSlash === -1) return path;
        return path.substr(lastSlash+1);
      },extname:function (path) {
        return PATH.splitPath(path)[3];
      },join:function () {
        var paths = Array.prototype.slice.call(arguments, 0);
        return PATH.normalize(paths.join('/'));
      },join2:function (l, r) {
        return PATH.normalize(l + '/' + r);
      },resolve:function () {
        var resolvedPath = '',
          resolvedAbsolute = false;
        for (var i = arguments.length - 1; i >= -1 && !resolvedAbsolute; i--) {
          var path = (i >= 0) ? arguments[i] : FS.cwd();
          // Skip empty and invalid entries
          if (typeof path !== 'string') {
            throw new TypeError('Arguments to path.resolve must be strings');
          } else if (!path) {
            continue;
          }
          resolvedPath = path + '/' + resolvedPath;
          resolvedAbsolute = path.charAt(0) === '/';
        }
        // At this point the path should be resolved to a full absolute path, but
        // handle relative paths to be safe (might happen when process.cwd() fails)
        resolvedPath = PATH.normalizeArray(resolvedPath.split('/').filter(function(p) {
          return !!p;
        }), !resolvedAbsolute).join('/');
        return ((resolvedAbsolute ? '/' : '') + resolvedPath) || '.';
      },relative:function (from, to) {
        from = PATH.resolve(from).substr(1);
        to = PATH.resolve(to).substr(1);
        function trim(arr) {
          var start = 0;
          for (; start < arr.length; start++) {
            if (arr[start] !== '') break;
          }
          var end = arr.length - 1;
          for (; end >= 0; end--) {
            if (arr[end] !== '') break;
          }
          if (start > end) return [];
          return arr.slice(start, end - start + 1);
        }
        var fromParts = trim(from.split('/'));
        var toParts = trim(to.split('/'));
        var length = Math.min(fromParts.length, toParts.length);
        var samePartsLength = length;
        for (var i = 0; i < length; i++) {
          if (fromParts[i] !== toParts[i]) {
            samePartsLength = i;
            break;
          }
        }
        var outputParts = [];
        for (var i = samePartsLength; i < fromParts.length; i++) {
          outputParts.push('..');
        }
        outputParts = outputParts.concat(toParts.slice(samePartsLength));
        return outputParts.join('/');
      }};

  var TTY={ttys:[],init:function () {
        // https://github.com/kripken/emscripten/pull/1555
        // if (ENVIRONMENT_IS_NODE) {
        //   // currently, FS.init does not distinguish if process.stdin is a file or TTY
        //   // device, it always assumes it's a TTY device. because of this, we're forcing
        //   // process.stdin to UTF8 encoding to at least make stdin reading compatible
        //   // with text files until FS.init can be refactored.
        //   process['stdin']['setEncoding']('utf8');
        // }
      },shutdown:function () {
        // https://github.com/kripken/emscripten/pull/1555
        // if (ENVIRONMENT_IS_NODE) {
        //   // inolen: any idea as to why node -e 'process.stdin.read()' wouldn't exit immediately (with process.stdin being a tty)?
        //   // isaacs: because now it's reading from the stream, you've expressed interest in it, so that read() kicks off a _read() which creates a ReadReq operation
        //   // inolen: I thought read() in that case was a synchronous operation that just grabbed some amount of buffered data if it exists?
        //   // isaacs: it is. but it also triggers a _read() call, which calls readStart() on the handle
        //   // isaacs: do process.stdin.pause() and i'd think it'd probably close the pending call
        //   process['stdin']['pause']();
        // }
      },register:function (dev, ops) {
        TTY.ttys[dev] = { input: [], output: [], ops: ops };
        FS.registerDevice(dev, TTY.stream_ops);
      },stream_ops:{open:function (stream) {
          var tty = TTY.ttys[stream.node.rdev];
          if (!tty) {
            throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
          }
          stream.tty = tty;
          stream.seekable = false;
        },close:function (stream) {
          // flush any pending line data
          if (stream.tty.output.length) {
            stream.tty.ops.put_char(stream.tty, 10);
          }
        },read:function (stream, buffer, offset, length, pos /* ignored */) {
          if (!stream.tty || !stream.tty.ops.get_char) {
            throw new FS.ErrnoError(ERRNO_CODES.ENXIO);
          }
          var bytesRead = 0;
          for (var i = 0; i < length; i++) {
            var result;
            try {
              result = stream.tty.ops.get_char(stream.tty);
            } catch (e) {
              throw new FS.ErrnoError(ERRNO_CODES.EIO);
            }
            if (result === undefined && bytesRead === 0) {
              throw new FS.ErrnoError(ERRNO_CODES.EAGAIN);
            }
            if (result === null || result === undefined) break;
            bytesRead++;
            buffer[offset+i] = result;
          }
          if (bytesRead) {
            stream.node.timestamp = Date.now();
          }
          return bytesRead;
        },write:function (stream, buffer, offset, length, pos) {
          if (!stream.tty || !stream.tty.ops.put_char) {
            throw new FS.ErrnoError(ERRNO_CODES.ENXIO);
          }
          for (var i = 0; i < length; i++) {
            try {
              stream.tty.ops.put_char(stream.tty, buffer[offset+i]);
            } catch (e) {
              throw new FS.ErrnoError(ERRNO_CODES.EIO);
            }
          }
          if (length) {
            stream.node.timestamp = Date.now();
          }
          return i;
        }},default_tty_ops:{get_char:function (tty) {
          if (!tty.input.length) {
            var result = null;
            if (ENVIRONMENT_IS_NODE) {
              result = process['stdin']['read']();
              if (!result) {
                if (process['stdin']['_readableState'] && process['stdin']['_readableState']['ended']) {
                  return null;  // EOF
                }
                return undefined;  // no data available
              }
            } else if (typeof window != 'undefined' &&
              typeof window.prompt == 'function') {
              // Browser.
              result = window.prompt('Input: ');  // returns null on cancel
              if (result !== null) {
                result += '\n';
              }
            } else if (typeof readline == 'function') {
              // Command line.
              result = readline();
              if (result !== null) {
                result += '\n';
              }
            }
            if (!result) {
              return null;
            }
            tty.input = intArrayFromString(result, true);
          }
          return tty.input.shift();
        },put_char:function (tty, val) {
          if (val === null || val === 10) {
            Module['print'](tty.output.join(''));
            tty.output = [];
          } else {
            tty.output.push(TTY.utf8.processCChar(val));
          }
        }},default_tty1_ops:{put_char:function (tty, val) {
          if (val === null || val === 10) {
            Module['printErr'](tty.output.join(''));
            tty.output = [];
          } else {
            tty.output.push(TTY.utf8.processCChar(val));
          }
        }}};

  var MEMFS={ops_table:null,CONTENT_OWNING:1,CONTENT_FLEXIBLE:2,CONTENT_FIXED:3,mount:function (mount) {
        return MEMFS.createNode(null, '/', 16384 | 511 /* 0777 */, 0);
      },createNode:function (parent, name, mode, dev) {
        if (FS.isBlkdev(mode) || FS.isFIFO(mode)) {
          // no supported
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        if (!MEMFS.ops_table) {
          MEMFS.ops_table = {
            dir: {
              node: {
                getattr: MEMFS.node_ops.getattr,
                setattr: MEMFS.node_ops.setattr,
                lookup: MEMFS.node_ops.lookup,
                mknod: MEMFS.node_ops.mknod,
                rename: MEMFS.node_ops.rename,
                unlink: MEMFS.node_ops.unlink,
                rmdir: MEMFS.node_ops.rmdir,
                readdir: MEMFS.node_ops.readdir,
                symlink: MEMFS.node_ops.symlink
              },
              stream: {
                llseek: MEMFS.stream_ops.llseek
              }
            },
            file: {
              node: {
                getattr: MEMFS.node_ops.getattr,
                setattr: MEMFS.node_ops.setattr
              },
              stream: {
                llseek: MEMFS.stream_ops.llseek,
                read: MEMFS.stream_ops.read,
                write: MEMFS.stream_ops.write,
                allocate: MEMFS.stream_ops.allocate,
                mmap: MEMFS.stream_ops.mmap
              }
            },
            link: {
              node: {
                getattr: MEMFS.node_ops.getattr,
                setattr: MEMFS.node_ops.setattr,
                readlink: MEMFS.node_ops.readlink
              },
              stream: {}
            },
            chrdev: {
              node: {
                getattr: MEMFS.node_ops.getattr,
                setattr: MEMFS.node_ops.setattr
              },
              stream: FS.chrdev_stream_ops
            },
          };
        }
        var node = FS.createNode(parent, name, mode, dev);
        if (FS.isDir(node.mode)) {
          node.node_ops = MEMFS.ops_table.dir.node;
          node.stream_ops = MEMFS.ops_table.dir.stream;
          node.contents = {};
        } else if (FS.isFile(node.mode)) {
          node.node_ops = MEMFS.ops_table.file.node;
          node.stream_ops = MEMFS.ops_table.file.stream;
          node.contents = [];
          node.contentMode = MEMFS.CONTENT_FLEXIBLE;
        } else if (FS.isLink(node.mode)) {
          node.node_ops = MEMFS.ops_table.link.node;
          node.stream_ops = MEMFS.ops_table.link.stream;
        } else if (FS.isChrdev(node.mode)) {
          node.node_ops = MEMFS.ops_table.chrdev.node;
          node.stream_ops = MEMFS.ops_table.chrdev.stream;
        }
        node.timestamp = Date.now();
        // add the new node to the parent
        if (parent) {
          parent.contents[name] = node;
        }
        return node;
      },ensureFlexible:function (node) {
        if (node.contentMode !== MEMFS.CONTENT_FLEXIBLE) {
          var contents = node.contents;
          node.contents = Array.prototype.slice.call(contents);
          node.contentMode = MEMFS.CONTENT_FLEXIBLE;
        }
      },node_ops:{getattr:function (node) {
          var attr = {};
          // device numbers reuse inode numbers.
          attr.dev = FS.isChrdev(node.mode) ? node.id : 1;
          attr.ino = node.id;
          attr.mode = node.mode;
          attr.nlink = 1;
          attr.uid = 0;
          attr.gid = 0;
          attr.rdev = node.rdev;
          if (FS.isDir(node.mode)) {
            attr.size = 4096;
          } else if (FS.isFile(node.mode)) {
            attr.size = node.contents.length;
          } else if (FS.isLink(node.mode)) {
            attr.size = node.link.length;
          } else {
            attr.size = 0;
          }
          attr.atime = new Date(node.timestamp);
          attr.mtime = new Date(node.timestamp);
          attr.ctime = new Date(node.timestamp);
          // NOTE: In our implementation, st_blocks = Math.ceil(st_size/st_blksize),
          //       but this is not required by the standard.
          attr.blksize = 4096;
          attr.blocks = Math.ceil(attr.size / attr.blksize);
          return attr;
        },setattr:function (node, attr) {
          if (attr.mode !== undefined) {
            node.mode = attr.mode;
          }
          if (attr.timestamp !== undefined) {
            node.timestamp = attr.timestamp;
          }
          if (attr.size !== undefined) {
            MEMFS.ensureFlexible(node);
            var contents = node.contents;
            if (attr.size < contents.length) contents.length = attr.size;
            else while (attr.size > contents.length) contents.push(0);
          }
        },lookup:function (parent, name) {
          throw FS.genericErrors[ERRNO_CODES.ENOENT];
        },mknod:function (parent, name, mode, dev) {
          return MEMFS.createNode(parent, name, mode, dev);
        },rename:function (old_node, new_dir, new_name) {
          // if we're overwriting a directory at new_name, make sure it's empty.
          if (FS.isDir(old_node.mode)) {
            var new_node;
            try {
              new_node = FS.lookupNode(new_dir, new_name);
            } catch (e) {
            }
            if (new_node) {
              for (var i in new_node.contents) {
                throw new FS.ErrnoError(ERRNO_CODES.ENOTEMPTY);
              }
            }
          }
          // do the internal rewiring
          delete old_node.parent.contents[old_node.name];
          old_node.name = new_name;
          new_dir.contents[new_name] = old_node;
          old_node.parent = new_dir;
        },unlink:function (parent, name) {
          delete parent.contents[name];
        },rmdir:function (parent, name) {
          var node = FS.lookupNode(parent, name);
          for (var i in node.contents) {
            throw new FS.ErrnoError(ERRNO_CODES.ENOTEMPTY);
          }
          delete parent.contents[name];
        },readdir:function (node) {
          var entries = ['.', '..']
          for (var key in node.contents) {
            if (!node.contents.hasOwnProperty(key)) {
              continue;
            }
            entries.push(key);
          }
          return entries;
        },symlink:function (parent, newname, oldpath) {
          var node = MEMFS.createNode(parent, newname, 511 /* 0777 */ | 40960, 0);
          node.link = oldpath;
          return node;
        },readlink:function (node) {
          if (!FS.isLink(node.mode)) {
            throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
          }
          return node.link;
        }},stream_ops:{read:function (stream, buffer, offset, length, position) {
          var contents = stream.node.contents;
          if (position >= contents.length)
            return 0;
          var size = Math.min(contents.length - position, length);
          assert(size >= 0);
          if (size > 8 && contents.subarray) { // non-trivial, and typed array
            buffer.set(contents.subarray(position, position + size), offset);
          } else
          {
            for (var i = 0; i < size; i++) {
              buffer[offset + i] = contents[position + i];
            }
          }
          return size;
        },write:function (stream, buffer, offset, length, position, canOwn) {
          var node = stream.node;
          node.timestamp = Date.now();
          var contents = node.contents;
          if (length && contents.length === 0 && position === 0 && buffer.subarray) {
            // just replace it with the new data
            if (canOwn && offset === 0) {
              node.contents = buffer; // this could be a subarray of Emscripten HEAP, or allocated from some other source.
              node.contentMode = (buffer.buffer === HEAP8.buffer) ? MEMFS.CONTENT_OWNING : MEMFS.CONTENT_FIXED;
            } else {
              node.contents = new Uint8Array(buffer.subarray(offset, offset+length));
              node.contentMode = MEMFS.CONTENT_FIXED;
            }
            return length;
          }
          MEMFS.ensureFlexible(node);
          var contents = node.contents;
          while (contents.length < position) contents.push(0);
          for (var i = 0; i < length; i++) {
            contents[position + i] = buffer[offset + i];
          }
          return length;
        },llseek:function (stream, offset, whence) {
          var position = offset;
          if (whence === 1) {  // SEEK_CUR.
            position += stream.position;
          } else if (whence === 2) {  // SEEK_END.
            if (FS.isFile(stream.node.mode)) {
              position += stream.node.contents.length;
            }
          }
          if (position < 0) {
            throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
          }
          stream.ungotten = [];
          stream.position = position;
          return position;
        },allocate:function (stream, offset, length) {
          MEMFS.ensureFlexible(stream.node);
          var contents = stream.node.contents;
          var limit = offset + length;
          while (limit > contents.length) contents.push(0);
        },mmap:function (stream, buffer, offset, length, position, prot, flags) {
          if (!FS.isFile(stream.node.mode)) {
            throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
          }
          var ptr;
          var allocated;
          var contents = stream.node.contents;
          // Only make a new copy when MAP_PRIVATE is specified.
          if ( !(flags & 2) &&
                (contents.buffer === buffer || contents.buffer === buffer.buffer) ) {
            // We can't emulate MAP_SHARED when the file is not backed by the buffer
            // we're mapping to (e.g. the HEAP buffer).
            allocated = false;
            ptr = contents.byteOffset;
          } else {
            // Try to avoid unnecessary slices.
            if (position > 0 || position + length < contents.length) {
              if (contents.subarray) {
                contents = contents.subarray(position, position + length);
              } else {
                contents = Array.prototype.slice.call(contents, position, position + length);
              }
            }
            allocated = true;
            ptr = _malloc(length);
            if (!ptr) {
              throw new FS.ErrnoError(ERRNO_CODES.ENOMEM);
            }
            buffer.set(contents, ptr);
          }
          return { ptr: ptr, allocated: allocated };
        }}};

  var IDBFS={dbs:{},indexedDB:function () {
        return window.indexedDB || window.mozIndexedDB || window.webkitIndexedDB || window.msIndexedDB;
      },DB_VERSION:21,DB_STORE_NAME:"FILE_DATA",mount:function (mount) {
        // reuse all of the core MEMFS functionality
        return MEMFS.mount.apply(null, arguments);
      },syncfs:function (mount, populate, callback) {
        IDBFS.getLocalSet(mount, function(err, local) {
          if (err) return callback(err);

          IDBFS.getRemoteSet(mount, function(err, remote) {
            if (err) return callback(err);

            var src = populate ? remote : local;
            var dst = populate ? local : remote;

            IDBFS.reconcile(src, dst, callback);
          });
        });
      },getDB:function (name, callback) {
        // check the cache first
        var db = IDBFS.dbs[name];
        if (db) {
          return callback(null, db);
        }

        var req;
        try {
          req = IDBFS.indexedDB().open(name, IDBFS.DB_VERSION);
        } catch (e) {
          return callback(e);
        }
        req.onupgradeneeded = function(e) {
          var db = e.target.result;
          var transaction = e.target.transaction;

          var fileStore;

          if (db.objectStoreNames.contains(IDBFS.DB_STORE_NAME)) {
            fileStore = transaction.objectStore(IDBFS.DB_STORE_NAME);
          } else {
            fileStore = db.createObjectStore(IDBFS.DB_STORE_NAME);
          }

          fileStore.createIndex('timestamp', 'timestamp', { unique: false });
        };
        req.onsuccess = function() {
          db = req.result;

          // add to the cache
          IDBFS.dbs[name] = db;
          callback(null, db);
        };
        req.onerror = function() {
          callback(this.error);
        };
      },getLocalSet:function (mount, callback) {
        var entries = {};

        function isRealDir(p) {
          return p !== '.' && p !== '..';
        };
        function toAbsolute(root) {
          return function(p) {
            return PATH.join2(root, p);
          }
        };

        var check = FS.readdir(mount.mountpoint).filter(isRealDir).map(toAbsolute(mount.mountpoint));

        while (check.length) {
          var path = check.pop();
          var stat;

          try {
            stat = FS.stat(path);
          } catch (e) {
            return callback(e);
          }

          if (FS.isDir(stat.mode)) {
            check.push.apply(check, FS.readdir(path).filter(isRealDir).map(toAbsolute(path)));
          }

          entries[path] = { timestamp: stat.mtime };
        }

        return callback(null, { type: 'local', entries: entries });
      },getRemoteSet:function (mount, callback) {
        var entries = {};

        IDBFS.getDB(mount.mountpoint, function(err, db) {
          if (err) return callback(err);

          var transaction = db.transaction([IDBFS.DB_STORE_NAME], 'readonly');
          transaction.onerror = function() { callback(this.error); };

          var store = transaction.objectStore(IDBFS.DB_STORE_NAME);
          var index = store.index('timestamp');

          index.openKeyCursor().onsuccess = function(event) {
            var cursor = event.target.result;

            if (!cursor) {
              return callback(null, { type: 'remote', db: db, entries: entries });
            }

            entries[cursor.primaryKey] = { timestamp: cursor.key };

            cursor.continue();
          };
        });
      },loadLocalEntry:function (path, callback) {
        var stat, node;

        try {
          var lookup = FS.lookupPath(path);
          node = lookup.node;
          stat = FS.stat(path);
        } catch (e) {
          return callback(e);
        }

        if (FS.isDir(stat.mode)) {
          return callback(null, { timestamp: stat.mtime, mode: stat.mode });
        } else if (FS.isFile(stat.mode)) {
          return callback(null, { timestamp: stat.mtime, mode: stat.mode, contents: node.contents });
        } else {
          return callback(new Error('node type not supported'));
        }
      },storeLocalEntry:function (path, entry, callback) {
        try {
          if (FS.isDir(entry.mode)) {
            FS.mkdir(path, entry.mode);
          } else if (FS.isFile(entry.mode)) {
            FS.writeFile(path, entry.contents, { encoding: 'binary', canOwn: true });
          } else {
            return callback(new Error('node type not supported'));
          }

          FS.utime(path, entry.timestamp, entry.timestamp);
        } catch (e) {
          return callback(e);
        }

        callback(null);
      },removeLocalEntry:function (path, callback) {
        try {
          var lookup = FS.lookupPath(path);
          var stat = FS.stat(path);

          if (FS.isDir(stat.mode)) {
            FS.rmdir(path);
          } else if (FS.isFile(stat.mode)) {
            FS.unlink(path);
          }
        } catch (e) {
          return callback(e);
        }

        callback(null);
      },loadRemoteEntry:function (store, path, callback) {
        var req = store.get(path);
        req.onsuccess = function(event) { callback(null, event.target.result); };
        req.onerror = function() { callback(this.error); };
      },storeRemoteEntry:function (store, path, entry, callback) {
        var req = store.put(entry, path);
        req.onsuccess = function() { callback(null); };
        req.onerror = function() { callback(this.error); };
      },removeRemoteEntry:function (store, path, callback) {
        var req = store.delete(path);
        req.onsuccess = function() { callback(null); };
        req.onerror = function() { callback(this.error); };
      },reconcile:function (src, dst, callback) {
        var total = 0;

        var create = [];
        Object.keys(src.entries).forEach(function (key) {
          var e = src.entries[key];
          var e2 = dst.entries[key];
          if (!e2 || e.timestamp > e2.timestamp) {
            create.push(key);
            total++;
          }
        });

        var remove = [];
        Object.keys(dst.entries).forEach(function (key) {
          var e = dst.entries[key];
          var e2 = src.entries[key];
          if (!e2) {
            remove.push(key);
            total++;
          }
        });

        if (!total) {
          return callback(null);
        }

        var errored = false;
        var completed = 0;
        var db = src.type === 'remote' ? src.db : dst.db;
        var transaction = db.transaction([IDBFS.DB_STORE_NAME], 'readwrite');
        var store = transaction.objectStore(IDBFS.DB_STORE_NAME);

        function done(err) {
          if (err) {
            if (!done.errored) {
              done.errored = true;
              return callback(err);
            }
            return;
          }
          if (++completed >= total) {
            return callback(null);
          }
        };

        transaction.onerror = function() { done(this.error); };

        // sort paths in ascending order so directory entries are created
        // before the files inside them
        create.sort().forEach(function (path) {
          if (dst.type === 'local') {
            IDBFS.loadRemoteEntry(store, path, function (err, entry) {
              if (err) return done(err);
              IDBFS.storeLocalEntry(path, entry, done);
            });
          } else {
            IDBFS.loadLocalEntry(path, function (err, entry) {
              if (err) return done(err);
              IDBFS.storeRemoteEntry(store, path, entry, done);
            });
          }
        });

        // sort paths in descending order so files are deleted before their
        // parent directories
        remove.sort().reverse().forEach(function(path) {
          if (dst.type === 'local') {
            IDBFS.removeLocalEntry(path, done);
          } else {
            IDBFS.removeRemoteEntry(store, path, done);
          }
        });
      }};

  var NODEFS={isWindows:false,staticInit:function () {
        NODEFS.isWindows = !!process.platform.match(/^win/);
      },mount:function (mount) {
        assert(ENVIRONMENT_IS_NODE);
        return NODEFS.createNode(null, '/', NODEFS.getMode(mount.opts.root), 0);
      },createNode:function (parent, name, mode, dev) {
        if (!FS.isDir(mode) && !FS.isFile(mode) && !FS.isLink(mode)) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        var node = FS.createNode(parent, name, mode);
        node.node_ops = NODEFS.node_ops;
        node.stream_ops = NODEFS.stream_ops;
        return node;
      },getMode:function (path) {
        var stat;
        try {
          stat = fs.lstatSync(path);
          if (NODEFS.isWindows) {
            // On Windows, directories return permission bits 'rw-rw-rw-', even though they have 'rwxrwxrwx', so
            // propagate write bits to execute bits.
            stat.mode = stat.mode | ((stat.mode & 146) >> 1);
          }
        } catch (e) {
          if (!e.code) throw e;
          throw new FS.ErrnoError(ERRNO_CODES[e.code]);
        }
        return stat.mode;
      },realPath:function (node) {
        var parts = [];
        while (node.parent !== node) {
          parts.push(node.name);
          node = node.parent;
        }
        parts.push(node.mount.opts.root);
        parts.reverse();
        return PATH.join.apply(null, parts);
      },flagsToPermissionStringMap:{0:"r",1:"r+",2:"r+",64:"r",65:"r+",66:"r+",129:"rx+",193:"rx+",514:"w+",577:"w",578:"w+",705:"wx",706:"wx+",1024:"a",1025:"a",1026:"a+",1089:"a",1090:"a+",1153:"ax",1154:"ax+",1217:"ax",1218:"ax+",4096:"rs",4098:"rs+"},flagsToPermissionString:function (flags) {
        if (flags in NODEFS.flagsToPermissionStringMap) {
          return NODEFS.flagsToPermissionStringMap[flags];
        } else {
          return flags;
        }
      },node_ops:{getattr:function (node) {
          var path = NODEFS.realPath(node);
          var stat;
          try {
            stat = fs.lstatSync(path);
          } catch (e) {
            if (!e.code) throw e;
            throw new FS.ErrnoError(ERRNO_CODES[e.code]);
          }
          // node.js v0.10.20 doesn't report blksize and blocks on Windows. Fake them with default blksize of 4096.
          // See http://support.microsoft.com/kb/140365
          if (NODEFS.isWindows && !stat.blksize) {
            stat.blksize = 4096;
          }
          if (NODEFS.isWindows && !stat.blocks) {
            stat.blocks = (stat.size+stat.blksize-1)/stat.blksize|0;
          }
          return {
            dev: stat.dev,
            ino: stat.ino,
            mode: stat.mode,
            nlink: stat.nlink,
            uid: stat.uid,
            gid: stat.gid,
            rdev: stat.rdev,
            size: stat.size,
            atime: stat.atime,
            mtime: stat.mtime,
            ctime: stat.ctime,
            blksize: stat.blksize,
            blocks: stat.blocks
          };
        },setattr:function (node, attr) {
          var path = NODEFS.realPath(node);
          try {
            if (attr.mode !== undefined) {
              fs.chmodSync(path, attr.mode);
              // update the common node structure mode as well
              node.mode = attr.mode;
            }
            if (attr.timestamp !== undefined) {
              var date = new Date(attr.timestamp);
              fs.utimesSync(path, date, date);
            }
            if (attr.size !== undefined) {
              fs.truncateSync(path, attr.size);
            }
          } catch (e) {
            if (!e.code) throw e;
            throw new FS.ErrnoError(ERRNO_CODES[e.code]);
          }
        },lookup:function (parent, name) {
          var path = PATH.join2(NODEFS.realPath(parent), name);
          var mode = NODEFS.getMode(path);
          return NODEFS.createNode(parent, name, mode);
        },mknod:function (parent, name, mode, dev) {
          var node = NODEFS.createNode(parent, name, mode, dev);
          // create the backing node for this in the fs root as well
          var path = NODEFS.realPath(node);
          try {
            if (FS.isDir(node.mode)) {
              fs.mkdirSync(path, node.mode);
            } else {
              fs.writeFileSync(path, '', { mode: node.mode });
            }
          } catch (e) {
            if (!e.code) throw e;
            throw new FS.ErrnoError(ERRNO_CODES[e.code]);
          }
          return node;
        },rename:function (oldNode, newDir, newName) {
          var oldPath = NODEFS.realPath(oldNode);
          var newPath = PATH.join2(NODEFS.realPath(newDir), newName);
          try {
            fs.renameSync(oldPath, newPath);
          } catch (e) {
            if (!e.code) throw e;
            throw new FS.ErrnoError(ERRNO_CODES[e.code]);
          }
        },unlink:function (parent, name) {
          var path = PATH.join2(NODEFS.realPath(parent), name);
          try {
            fs.unlinkSync(path);
          } catch (e) {
            if (!e.code) throw e;
            throw new FS.ErrnoError(ERRNO_CODES[e.code]);
          }
        },rmdir:function (parent, name) {
          var path = PATH.join2(NODEFS.realPath(parent), name);
          try {
            fs.rmdirSync(path);
          } catch (e) {
            if (!e.code) throw e;
            throw new FS.ErrnoError(ERRNO_CODES[e.code]);
          }
        },readdir:function (node) {
          var path = NODEFS.realPath(node);
          try {
            return fs.readdirSync(path);
          } catch (e) {
            if (!e.code) throw e;
            throw new FS.ErrnoError(ERRNO_CODES[e.code]);
          }
        },symlink:function (parent, newName, oldPath) {
          var newPath = PATH.join2(NODEFS.realPath(parent), newName);
          try {
            fs.symlinkSync(oldPath, newPath);
          } catch (e) {
            if (!e.code) throw e;
            throw new FS.ErrnoError(ERRNO_CODES[e.code]);
          }
        },readlink:function (node) {
          var path = NODEFS.realPath(node);
          try {
            return fs.readlinkSync(path);
          } catch (e) {
            if (!e.code) throw e;
            throw new FS.ErrnoError(ERRNO_CODES[e.code]);
          }
        }},stream_ops:{open:function (stream) {
          var path = NODEFS.realPath(stream.node);
          try {
            if (FS.isFile(stream.node.mode)) {
              stream.nfd = fs.openSync(path, NODEFS.flagsToPermissionString(stream.flags));
            }
          } catch (e) {
            if (!e.code) throw e;
            throw new FS.ErrnoError(ERRNO_CODES[e.code]);
          }
        },close:function (stream) {
          try {
            if (FS.isFile(stream.node.mode) && stream.nfd) {
              fs.closeSync(stream.nfd);
            }
          } catch (e) {
            if (!e.code) throw e;
            throw new FS.ErrnoError(ERRNO_CODES[e.code]);
          }
        },read:function (stream, buffer, offset, length, position) {
          // FIXME this is terrible.
          var nbuffer = new Buffer(length);
          var res;
          try {
            res = fs.readSync(stream.nfd, nbuffer, 0, length, position);
          } catch (e) {
            throw new FS.ErrnoError(ERRNO_CODES[e.code]);
          }
          if (res > 0) {
            for (var i = 0; i < res; i++) {
              buffer[offset + i] = nbuffer[i];
            }
          }
          return res;
        },write:function (stream, buffer, offset, length, position) {
          // FIXME this is terrible.
          var nbuffer = new Buffer(buffer.subarray(offset, offset + length));
          var res;
          try {
            res = fs.writeSync(stream.nfd, nbuffer, 0, length, position);
          } catch (e) {
            throw new FS.ErrnoError(ERRNO_CODES[e.code]);
          }
          return res;
        },llseek:function (stream, offset, whence) {
          var position = offset;
          if (whence === 1) {  // SEEK_CUR.
            position += stream.position;
          } else if (whence === 2) {  // SEEK_END.
            if (FS.isFile(stream.node.mode)) {
              try {
                var stat = fs.fstatSync(stream.nfd);
                position += stat.size;
              } catch (e) {
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
              }
            }
          }

          if (position < 0) {
            throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
          }

          stream.position = position;
          return position;
        }}};

  var _stdin=allocate(1, "i32*", ALLOC_STATIC);

  var _stdout=allocate(1, "i32*", ALLOC_STATIC);

  var _stderr=allocate(1, "i32*", ALLOC_STATIC);

  function _fflush(stream) {
      // int fflush(FILE *stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fflush.html
      // we don't currently perform any user-space buffering of data
    }var FS={root:null,mounts:[],devices:[null],streams:[],nextInode:1,nameTable:null,currentPath:"/",initialized:false,ignorePermissions:true,ErrnoError:null,genericErrors:{},handleFSError:function (e) {
        if (!(e instanceof FS.ErrnoError)) throw e + ' : ' + stackTrace();
        return ___setErrNo(e.errno);
      },lookupPath:function (path, opts) {
        path = PATH.resolve(FS.cwd(), path);
        opts = opts || {};

        var defaults = {
          follow_mount: true,
          recurse_count: 0
        };
        for (var key in defaults) {
          if (opts[key] === undefined) {
            opts[key] = defaults[key];
          }
        }

        if (opts.recurse_count > 8) {  // max recursive lookup of 8
          throw new FS.ErrnoError(ERRNO_CODES.ELOOP);
        }

        // split the path
        var parts = PATH.normalizeArray(path.split('/').filter(function(p) {
          return !!p;
        }), false);

        // start at the root
        var current = FS.root;
        var current_path = '/';

        for (var i = 0; i < parts.length; i++) {
          var islast = (i === parts.length-1);
          if (islast && opts.parent) {
            // stop resolving
            break;
          }

          current = FS.lookupNode(current, parts[i]);
          current_path = PATH.join2(current_path, parts[i]);

          // jump to the mount's root node if this is a mountpoint
          if (FS.isMountpoint(current)) {
            if (!islast || (islast && opts.follow_mount)) {
              current = current.mounted.root;
            }
          }

          // by default, lookupPath will not follow a symlink if it is the final path component.
          // setting opts.follow = true will override this behavior.
          if (!islast || opts.follow) {
            var count = 0;
            while (FS.isLink(current.mode)) {
              var link = FS.readlink(current_path);
              current_path = PATH.resolve(PATH.dirname(current_path), link);

              var lookup = FS.lookupPath(current_path, { recurse_count: opts.recurse_count });
              current = lookup.node;

              if (count++ > 40) {  // limit max consecutive symlinks to 40 (SYMLOOP_MAX).
                throw new FS.ErrnoError(ERRNO_CODES.ELOOP);
              }
            }
          }
        }

        return { path: current_path, node: current };
      },getPath:function (node) {
        var path;
        while (true) {
          if (FS.isRoot(node)) {
            var mount = node.mount.mountpoint;
            if (!path) return mount;
            return mount[mount.length-1] !== '/' ? mount + '/' + path : mount + path;
          }
          path = path ? node.name + '/' + path : node.name;
          node = node.parent;
        }
      },hashName:function (parentid, name) {
        var hash = 0;


        for (var i = 0; i < name.length; i++) {
          hash = ((hash << 5) - hash + name.charCodeAt(i)) | 0;
        }
        return ((parentid + hash) >>> 0) % FS.nameTable.length;
      },hashAddNode:function (node) {
        var hash = FS.hashName(node.parent.id, node.name);
        node.name_next = FS.nameTable[hash];
        FS.nameTable[hash] = node;
      },hashRemoveNode:function (node) {
        var hash = FS.hashName(node.parent.id, node.name);
        if (FS.nameTable[hash] === node) {
          FS.nameTable[hash] = node.name_next;
        } else {
          var current = FS.nameTable[hash];
          while (current) {
            if (current.name_next === node) {
              current.name_next = node.name_next;
              break;
            }
            current = current.name_next;
          }
        }
      },lookupNode:function (parent, name) {
        var err = FS.mayLookup(parent);
        if (err) {
          throw new FS.ErrnoError(err);
        }
        var hash = FS.hashName(parent.id, name);
        for (var node = FS.nameTable[hash]; node; node = node.name_next) {
          var nodeName = node.name;
          if (node.parent.id === parent.id && nodeName === name) {
            return node;
          }
        }
        // if we failed to find it in the cache, call into the VFS
        return FS.lookup(parent, name);
      },createNode:function (parent, name, mode, rdev) {
        if (!FS.FSNode) {
          FS.FSNode = function(parent, name, mode, rdev) {
            if (!parent) {
              parent = this;  // root node sets parent to itself
            }
            this.parent = parent;
            this.mount = parent.mount;
            this.mounted = null;
            this.id = FS.nextInode++;
            this.name = name;
            this.mode = mode;
            this.node_ops = {};
            this.stream_ops = {};
            this.rdev = rdev;
          };

          FS.FSNode.prototype = {};

          // compatibility
          var readMode = 292 | 73;
          var writeMode = 146;

          // NOTE we must use Object.defineProperties instead of individual calls to
          // Object.defineProperty in order to make closure compiler happy
          Object.defineProperties(FS.FSNode.prototype, {
            read: {
              get: function() { return (this.mode & readMode) === readMode; },
              set: function(val) { val ? this.mode |= readMode : this.mode &= ~readMode; }
            },
            write: {
              get: function() { return (this.mode & writeMode) === writeMode; },
              set: function(val) { val ? this.mode |= writeMode : this.mode &= ~writeMode; }
            },
            isFolder: {
              get: function() { return FS.isDir(this.mode); },
            },
            isDevice: {
              get: function() { return FS.isChrdev(this.mode); },
            },
          });
        }

        var node = new FS.FSNode(parent, name, mode, rdev);

        FS.hashAddNode(node);

        return node;
      },destroyNode:function (node) {
        FS.hashRemoveNode(node);
      },isRoot:function (node) {
        return node === node.parent;
      },isMountpoint:function (node) {
        return !!node.mounted;
      },isFile:function (mode) {
        return (mode & 61440) === 32768;
      },isDir:function (mode) {
        return (mode & 61440) === 16384;
      },isLink:function (mode) {
        return (mode & 61440) === 40960;
      },isChrdev:function (mode) {
        return (mode & 61440) === 8192;
      },isBlkdev:function (mode) {
        return (mode & 61440) === 24576;
      },isFIFO:function (mode) {
        return (mode & 61440) === 4096;
      },isSocket:function (mode) {
        return (mode & 49152) === 49152;
      },flagModes:{"r":0,"rs":1052672,"r+":2,"w":577,"wx":705,"xw":705,"w+":578,"wx+":706,"xw+":706,"a":1089,"ax":1217,"xa":1217,"a+":1090,"ax+":1218,"xa+":1218},modeStringToFlags:function (str) {
        var flags = FS.flagModes[str];
        if (typeof flags === 'undefined') {
          throw new Error('Unknown file open mode: ' + str);
        }
        return flags;
      },flagsToPermissionString:function (flag) {
        var accmode = flag & 2097155;
        var perms = ['r', 'w', 'rw'][accmode];
        if ((flag & 512)) {
          perms += 'w';
        }
        return perms;
      },nodePermissions:function (node, perms) {
        if (FS.ignorePermissions) {
          return 0;
        }
        // return 0 if any user, group or owner bits are set.
        if (perms.indexOf('r') !== -1 && !(node.mode & 292)) {
          return ERRNO_CODES.EACCES;
        } else if (perms.indexOf('w') !== -1 && !(node.mode & 146)) {
          return ERRNO_CODES.EACCES;
        } else if (perms.indexOf('x') !== -1 && !(node.mode & 73)) {
          return ERRNO_CODES.EACCES;
        }
        return 0;
      },mayLookup:function (dir) {
        return FS.nodePermissions(dir, 'x');
      },mayCreate:function (dir, name) {
        try {
          var node = FS.lookupNode(dir, name);
          return ERRNO_CODES.EEXIST;
        } catch (e) {
        }
        return FS.nodePermissions(dir, 'wx');
      },mayDelete:function (dir, name, isdir) {
        var node;
        try {
          node = FS.lookupNode(dir, name);
        } catch (e) {
          return e.errno;
        }
        var err = FS.nodePermissions(dir, 'wx');
        if (err) {
          return err;
        }
        if (isdir) {
          if (!FS.isDir(node.mode)) {
            return ERRNO_CODES.ENOTDIR;
          }
          if (FS.isRoot(node) || FS.getPath(node) === FS.cwd()) {
            return ERRNO_CODES.EBUSY;
          }
        } else {
          if (FS.isDir(node.mode)) {
            return ERRNO_CODES.EISDIR;
          }
        }
        return 0;
      },mayOpen:function (node, flags) {
        if (!node) {
          return ERRNO_CODES.ENOENT;
        }
        if (FS.isLink(node.mode)) {
          return ERRNO_CODES.ELOOP;
        } else if (FS.isDir(node.mode)) {
          if ((flags & 2097155) !== 0 ||  // opening for write
              (flags & 512)) {
            return ERRNO_CODES.EISDIR;
          }
        }
        return FS.nodePermissions(node, FS.flagsToPermissionString(flags));
      },MAX_OPEN_FDS:4096,nextfd:function (fd_start, fd_end) {
        fd_start = fd_start || 0;
        fd_end = fd_end || FS.MAX_OPEN_FDS;
        for (var fd = fd_start; fd <= fd_end; fd++) {
          if (!FS.streams[fd]) {
            return fd;
          }
        }
        throw new FS.ErrnoError(ERRNO_CODES.EMFILE);
      },getStream:function (fd) {
        return FS.streams[fd];
      },createStream:function (stream, fd_start, fd_end) {
        if (!FS.FSStream) {
          FS.FSStream = function(){};
          FS.FSStream.prototype = {};
          // compatibility
          Object.defineProperties(FS.FSStream.prototype, {
            object: {
              get: function() { return this.node; },
              set: function(val) { this.node = val; }
            },
            isRead: {
              get: function() { return (this.flags & 2097155) !== 1; }
            },
            isWrite: {
              get: function() { return (this.flags & 2097155) !== 0; }
            },
            isAppend: {
              get: function() { return (this.flags & 1024); }
            }
          });
        }
        if (0) {
          // reuse the object
          stream.__proto__ = FS.FSStream.prototype;
        } else {
          var newStream = new FS.FSStream();
          for (var p in stream) {
            newStream[p] = stream[p];
          }
          stream = newStream;
        }
        var fd = FS.nextfd(fd_start, fd_end);
        stream.fd = fd;
        FS.streams[fd] = stream;
        return stream;
      },closeStream:function (fd) {
        FS.streams[fd] = null;
      },getStreamFromPtr:function (ptr) {
        return FS.streams[ptr - 1];
      },getPtrForStream:function (stream) {
        return stream ? stream.fd + 1 : 0;
      },chrdev_stream_ops:{open:function (stream) {
          var device = FS.getDevice(stream.node.rdev);
          // override node's stream ops with the device's
          stream.stream_ops = device.stream_ops;
          // forward the open call
          if (stream.stream_ops.open) {
            stream.stream_ops.open(stream);
          }
        },llseek:function () {
          throw new FS.ErrnoError(ERRNO_CODES.ESPIPE);
        }},major:function (dev) {
        return ((dev) >> 8);
      },minor:function (dev) {
        return ((dev) & 0xff);
      },makedev:function (ma, mi) {
        return ((ma) << 8 | (mi));
      },registerDevice:function (dev, ops) {
        FS.devices[dev] = { stream_ops: ops };
      },getDevice:function (dev) {
        return FS.devices[dev];
      },getMounts:function (mount) {
        var mounts = [];
        var check = [mount];

        while (check.length) {
          var m = check.pop();

          mounts.push(m);

          check.push.apply(check, m.mounts);
        }

        return mounts;
      },syncfs:function (populate, callback) {
        if (typeof(populate) === 'function') {
          callback = populate;
          populate = false;
        }

        var mounts = FS.getMounts(FS.root.mount);
        var completed = 0;

        function done(err) {
          if (err) {
            if (!done.errored) {
              done.errored = true;
              return callback(err);
            }
            return;
          }
          if (++completed >= mounts.length) {
            callback(null);
          }
        };

        // sync all mounts
        mounts.forEach(function (mount) {
          if (!mount.type.syncfs) {
            return done(null);
          }
          mount.type.syncfs(mount, populate, done);
        });
      },mount:function (type, opts, mountpoint) {
        var root = mountpoint === '/';
        var pseudo = !mountpoint;
        var node;

        if (root && FS.root) {
          throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
        } else if (!root && !pseudo) {
          var lookup = FS.lookupPath(mountpoint, { follow_mount: false });

          mountpoint = lookup.path;  // use the absolute path
          node = lookup.node;

          if (FS.isMountpoint(node)) {
            throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
          }

          if (!FS.isDir(node.mode)) {
            throw new FS.ErrnoError(ERRNO_CODES.ENOTDIR);
          }
        }

        var mount = {
          type: type,
          opts: opts,
          mountpoint: mountpoint,
          mounts: []
        };

        // create a root node for the fs
        var mountRoot = type.mount(mount);
        mountRoot.mount = mount;
        mount.root = mountRoot;

        if (root) {
          FS.root = mountRoot;
        } else if (node) {
          // set as a mountpoint
          node.mounted = mount;

          // add the new mount to the current mount's children
          if (node.mount) {
            node.mount.mounts.push(mount);
          }
        }

        return mountRoot;
      },unmount:function (mountpoint) {
        var lookup = FS.lookupPath(mountpoint, { follow_mount: false });

        if (!FS.isMountpoint(lookup.node)) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }

        // destroy the nodes for this mount, and all its child mounts
        var node = lookup.node;
        var mount = node.mounted;
        var mounts = FS.getMounts(mount);

        Object.keys(FS.nameTable).forEach(function (hash) {
          var current = FS.nameTable[hash];

          while (current) {
            var next = current.name_next;

            if (mounts.indexOf(current.mount) !== -1) {
              FS.destroyNode(current);
            }

            current = next;
          }
        });

        // no longer a mountpoint
        node.mounted = null;

        // remove this mount from the child mounts
        var idx = node.mount.mounts.indexOf(mount);
        assert(idx !== -1);
        node.mount.mounts.splice(idx, 1);
      },lookup:function (parent, name) {
        return parent.node_ops.lookup(parent, name);
      },mknod:function (path, mode, dev) {
        var lookup = FS.lookupPath(path, { parent: true });
        var parent = lookup.node;
        var name = PATH.basename(path);
        var err = FS.mayCreate(parent, name);
        if (err) {
          throw new FS.ErrnoError(err);
        }
        if (!parent.node_ops.mknod) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        return parent.node_ops.mknod(parent, name, mode, dev);
      },create:function (path, mode) {
        mode = mode !== undefined ? mode : 438 /* 0666 */;
        mode &= 4095;
        mode |= 32768;
        return FS.mknod(path, mode, 0);
      },mkdir:function (path, mode) {
        mode = mode !== undefined ? mode : 511 /* 0777 */;
        mode &= 511 | 512;
        mode |= 16384;
        return FS.mknod(path, mode, 0);
      },mkdev:function (path, mode, dev) {
        if (typeof(dev) === 'undefined') {
          dev = mode;
          mode = 438 /* 0666 */;
        }
        mode |= 8192;
        return FS.mknod(path, mode, dev);
      },symlink:function (oldpath, newpath) {
        var lookup = FS.lookupPath(newpath, { parent: true });
        var parent = lookup.node;
        var newname = PATH.basename(newpath);
        var err = FS.mayCreate(parent, newname);
        if (err) {
          throw new FS.ErrnoError(err);
        }
        if (!parent.node_ops.symlink) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        return parent.node_ops.symlink(parent, newname, oldpath);
      },rename:function (old_path, new_path) {
        var old_dirname = PATH.dirname(old_path);
        var new_dirname = PATH.dirname(new_path);
        var old_name = PATH.basename(old_path);
        var new_name = PATH.basename(new_path);
        // parents must exist
        var lookup, old_dir, new_dir;
        try {
          lookup = FS.lookupPath(old_path, { parent: true });
          old_dir = lookup.node;
          lookup = FS.lookupPath(new_path, { parent: true });
          new_dir = lookup.node;
        } catch (e) {
          throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
        }
        // need to be part of the same mount
        if (old_dir.mount !== new_dir.mount) {
          throw new FS.ErrnoError(ERRNO_CODES.EXDEV);
        }
        // source must exist
        var old_node = FS.lookupNode(old_dir, old_name);
        // old path should not be an ancestor of the new path
        var relative = PATH.relative(old_path, new_dirname);
        if (relative.charAt(0) !== '.') {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        // new path should not be an ancestor of the old path
        relative = PATH.relative(new_path, old_dirname);
        if (relative.charAt(0) !== '.') {
          throw new FS.ErrnoError(ERRNO_CODES.ENOTEMPTY);
        }
        // see if the new path already exists
        var new_node;
        try {
          new_node = FS.lookupNode(new_dir, new_name);
        } catch (e) {
          // not fatal
        }
        // early out if nothing needs to change
        if (old_node === new_node) {
          return;
        }
        // we'll need to delete the old entry
        var isdir = FS.isDir(old_node.mode);
        var err = FS.mayDelete(old_dir, old_name, isdir);
        if (err) {
          throw new FS.ErrnoError(err);
        }
        // need delete permissions if we'll be overwriting.
        // need create permissions if new doesn't already exist.
        err = new_node ?
          FS.mayDelete(new_dir, new_name, isdir) :
          FS.mayCreate(new_dir, new_name);
        if (err) {
          throw new FS.ErrnoError(err);
        }
        if (!old_dir.node_ops.rename) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        if (FS.isMountpoint(old_node) || (new_node && FS.isMountpoint(new_node))) {
          throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
        }
        // if we are going to change the parent, check write permissions
        if (new_dir !== old_dir) {
          err = FS.nodePermissions(old_dir, 'w');
          if (err) {
            throw new FS.ErrnoError(err);
          }
        }
        // remove the node from the lookup hash
        FS.hashRemoveNode(old_node);
        // do the underlying fs rename
        try {
          old_dir.node_ops.rename(old_node, new_dir, new_name);
        } catch (e) {
          throw e;
        } finally {
          // add the node back to the hash (in case node_ops.rename
          // changed its name)
          FS.hashAddNode(old_node);
        }
      },rmdir:function (path) {
        var lookup = FS.lookupPath(path, { parent: true });
        var parent = lookup.node;
        var name = PATH.basename(path);
        var node = FS.lookupNode(parent, name);
        var err = FS.mayDelete(parent, name, true);
        if (err) {
          throw new FS.ErrnoError(err);
        }
        if (!parent.node_ops.rmdir) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        if (FS.isMountpoint(node)) {
          throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
        }
        parent.node_ops.rmdir(parent, name);
        FS.destroyNode(node);
      },readdir:function (path) {
        var lookup = FS.lookupPath(path, { follow: true });
        var node = lookup.node;
        if (!node.node_ops.readdir) {
          throw new FS.ErrnoError(ERRNO_CODES.ENOTDIR);
        }
        return node.node_ops.readdir(node);
      },unlink:function (path) {
        var lookup = FS.lookupPath(path, { parent: true });
        var parent = lookup.node;
        var name = PATH.basename(path);
        var node = FS.lookupNode(parent, name);
        var err = FS.mayDelete(parent, name, false);
        if (err) {
          // POSIX says unlink should set EPERM, not EISDIR
          if (err === ERRNO_CODES.EISDIR) err = ERRNO_CODES.EPERM;
          throw new FS.ErrnoError(err);
        }
        if (!parent.node_ops.unlink) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        if (FS.isMountpoint(node)) {
          throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
        }
        parent.node_ops.unlink(parent, name);
        FS.destroyNode(node);
      },readlink:function (path) {
        var lookup = FS.lookupPath(path);
        var link = lookup.node;
        if (!link.node_ops.readlink) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        return link.node_ops.readlink(link);
      },stat:function (path, dontFollow) {
        var lookup = FS.lookupPath(path, { follow: !dontFollow });
        var node = lookup.node;
        if (!node.node_ops.getattr) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        return node.node_ops.getattr(node);
      },lstat:function (path) {
        return FS.stat(path, true);
      },chmod:function (path, mode, dontFollow) {
        var node;
        if (typeof path === 'string') {
          var lookup = FS.lookupPath(path, { follow: !dontFollow });
          node = lookup.node;
        } else {
          node = path;
        }
        if (!node.node_ops.setattr) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        node.node_ops.setattr(node, {
          mode: (mode & 4095) | (node.mode & ~4095),
          timestamp: Date.now()
        });
      },lchmod:function (path, mode) {
        FS.chmod(path, mode, true);
      },fchmod:function (fd, mode) {
        var stream = FS.getStream(fd);
        if (!stream) {
          throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        FS.chmod(stream.node, mode);
      },chown:function (path, uid, gid, dontFollow) {
        var node;
        if (typeof path === 'string') {
          var lookup = FS.lookupPath(path, { follow: !dontFollow });
          node = lookup.node;
        } else {
          node = path;
        }
        if (!node.node_ops.setattr) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        node.node_ops.setattr(node, {
          timestamp: Date.now()
          // we ignore the uid / gid for now
        });
      },lchown:function (path, uid, gid) {
        FS.chown(path, uid, gid, true);
      },fchown:function (fd, uid, gid) {
        var stream = FS.getStream(fd);
        if (!stream) {
          throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        FS.chown(stream.node, uid, gid);
      },truncate:function (path, len) {
        if (len < 0) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        var node;
        if (typeof path === 'string') {
          var lookup = FS.lookupPath(path, { follow: true });
          node = lookup.node;
        } else {
          node = path;
        }
        if (!node.node_ops.setattr) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        if (FS.isDir(node.mode)) {
          throw new FS.ErrnoError(ERRNO_CODES.EISDIR);
        }
        if (!FS.isFile(node.mode)) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        var err = FS.nodePermissions(node, 'w');
        if (err) {
          throw new FS.ErrnoError(err);
        }
        node.node_ops.setattr(node, {
          size: len,
          timestamp: Date.now()
        });
      },ftruncate:function (fd, len) {
        var stream = FS.getStream(fd);
        if (!stream) {
          throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        if ((stream.flags & 2097155) === 0) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        FS.truncate(stream.node, len);
      },utime:function (path, atime, mtime) {
        var lookup = FS.lookupPath(path, { follow: true });
        var node = lookup.node;
        node.node_ops.setattr(node, {
          timestamp: Math.max(atime, mtime)
        });
      },open:function (path, flags, mode, fd_start, fd_end) {
        flags = typeof flags === 'string' ? FS.modeStringToFlags(flags) : flags;
        mode = typeof mode === 'undefined' ? 438 /* 0666 */ : mode;
        if ((flags & 64)) {
          mode = (mode & 4095) | 32768;
        } else {
          mode = 0;
        }
        var node;
        if (typeof path === 'object') {
          node = path;
        } else {
          path = PATH.normalize(path);
          try {
            var lookup = FS.lookupPath(path, {
              follow: !(flags & 131072)
            });
            node = lookup.node;
          } catch (e) {
            // ignore
          }
        }
        // perhaps we need to create the node
        if ((flags & 64)) {
          if (node) {
            // if O_CREAT and O_EXCL are set, error out if the node already exists
            if ((flags & 128)) {
              throw new FS.ErrnoError(ERRNO_CODES.EEXIST);
            }
          } else {
            // node doesn't exist, try to create it
            node = FS.mknod(path, mode, 0);
          }
        }
        if (!node) {
          throw new FS.ErrnoError(ERRNO_CODES.ENOENT);
        }
        // can't truncate a device
        if (FS.isChrdev(node.mode)) {
          flags &= ~512;
        }
        // check permissions
        var err = FS.mayOpen(node, flags);
        if (err) {
          throw new FS.ErrnoError(err);
        }
        // do truncation if necessary
        if ((flags & 512)) {
          FS.truncate(node, 0);
        }
        // we've already handled these, don't pass down to the underlying vfs
        flags &= ~(128 | 512);

        // register the stream with the filesystem
        var stream = FS.createStream({
          node: node,
          path: FS.getPath(node),  // we want the absolute path to the node
          flags: flags,
          seekable: true,
          position: 0,
          stream_ops: node.stream_ops,
          // used by the file family libc calls (fopen, fwrite, ferror, etc.)
          ungotten: [],
          error: false
        }, fd_start, fd_end);
        // call the new stream's open function
        if (stream.stream_ops.open) {
          stream.stream_ops.open(stream);
        }
        if (Module['logReadFiles'] && !(flags & 1)) {
          if (!FS.readFiles) FS.readFiles = {};
          if (!(path in FS.readFiles)) {
            FS.readFiles[path] = 1;
            Module['printErr']('read file: ' + path);
          }
        }
        return stream;
      },close:function (stream) {
        try {
          if (stream.stream_ops.close) {
            stream.stream_ops.close(stream);
          }
        } catch (e) {
          throw e;
        } finally {
          FS.closeStream(stream.fd);
        }
      },llseek:function (stream, offset, whence) {
        if (!stream.seekable || !stream.stream_ops.llseek) {
          throw new FS.ErrnoError(ERRNO_CODES.ESPIPE);
        }
        return stream.stream_ops.llseek(stream, offset, whence);
      },read:function (stream, buffer, offset, length, position) {
        if (length < 0 || position < 0) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        if ((stream.flags & 2097155) === 1) {
          throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        if (FS.isDir(stream.node.mode)) {
          throw new FS.ErrnoError(ERRNO_CODES.EISDIR);
        }
        if (!stream.stream_ops.read) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        var seeking = true;
        if (typeof position === 'undefined') {
          position = stream.position;
          seeking = false;
        } else if (!stream.seekable) {
          throw new FS.ErrnoError(ERRNO_CODES.ESPIPE);
        }
        var bytesRead = stream.stream_ops.read(stream, buffer, offset, length, position);
        if (!seeking) stream.position += bytesRead;
        return bytesRead;
      },write:function (stream, buffer, offset, length, position, canOwn) {
        if (length < 0 || position < 0) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        if ((stream.flags & 2097155) === 0) {
          throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        if (FS.isDir(stream.node.mode)) {
          throw new FS.ErrnoError(ERRNO_CODES.EISDIR);
        }
        if (!stream.stream_ops.write) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        var seeking = true;
        if (typeof position === 'undefined') {
          position = stream.position;
          seeking = false;
        } else if (!stream.seekable) {
          throw new FS.ErrnoError(ERRNO_CODES.ESPIPE);
        }
        if (stream.flags & 1024) {
          // seek to the end before writing in append mode
          FS.llseek(stream, 0, 2);
        }
        var bytesWritten = stream.stream_ops.write(stream, buffer, offset, length, position, canOwn);
        if (!seeking) stream.position += bytesWritten;
        return bytesWritten;
      },allocate:function (stream, offset, length) {
        if (offset < 0 || length <= 0) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        if ((stream.flags & 2097155) === 0) {
          throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        if (!FS.isFile(stream.node.mode) && !FS.isDir(node.mode)) {
          throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
        }
        if (!stream.stream_ops.allocate) {
          throw new FS.ErrnoError(ERRNO_CODES.EOPNOTSUPP);
        }
        stream.stream_ops.allocate(stream, offset, length);
      },mmap:function (stream, buffer, offset, length, position, prot, flags) {
        // TODO if PROT is PROT_WRITE, make sure we have write access
        if ((stream.flags & 2097155) === 1) {
          throw new FS.ErrnoError(ERRNO_CODES.EACCES);
        }
        if (!stream.stream_ops.mmap) {
          throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
        }
        return stream.stream_ops.mmap(stream, buffer, offset, length, position, prot, flags);
      },ioctl:function (stream, cmd, arg) {
        if (!stream.stream_ops.ioctl) {
          throw new FS.ErrnoError(ERRNO_CODES.ENOTTY);
        }
        return stream.stream_ops.ioctl(stream, cmd, arg);
      },readFile:function (path, opts) {
        opts = opts || {};
        opts.flags = opts.flags || 'r';
        opts.encoding = opts.encoding || 'binary';
        if (opts.encoding !== 'utf8' && opts.encoding !== 'binary') {
          throw new Error('Invalid encoding type "' + opts.encoding + '"');
        }
        var ret;
        var stream = FS.open(path, opts.flags);
        var stat = FS.stat(path);
        var length = stat.size;
        var buf = new Uint8Array(length);
        FS.read(stream, buf, 0, length, 0);
        if (opts.encoding === 'utf8') {
          ret = '';
          var utf8 = new Runtime.UTF8Processor();
          for (var i = 0; i < length; i++) {
            ret += utf8.processCChar(buf[i]);
          }
        } else if (opts.encoding === 'binary') {
          ret = buf;
        }
        FS.close(stream);
        return ret;
      },writeFile:function (path, data, opts) {
        opts = opts || {};
        opts.flags = opts.flags || 'w';
        opts.encoding = opts.encoding || 'utf8';
        if (opts.encoding !== 'utf8' && opts.encoding !== 'binary') {
          throw new Error('Invalid encoding type "' + opts.encoding + '"');
        }
        var stream = FS.open(path, opts.flags, opts.mode);
        if (opts.encoding === 'utf8') {
          var utf8 = new Runtime.UTF8Processor();
          var buf = new Uint8Array(utf8.processJSString(data));
          FS.write(stream, buf, 0, buf.length, 0, opts.canOwn);
        } else if (opts.encoding === 'binary') {
          FS.write(stream, data, 0, data.length, 0, opts.canOwn);
        }
        FS.close(stream);
      },cwd:function () {
        return FS.currentPath;
      },chdir:function (path) {
        var lookup = FS.lookupPath(path, { follow: true });
        if (!FS.isDir(lookup.node.mode)) {
          throw new FS.ErrnoError(ERRNO_CODES.ENOTDIR);
        }
        var err = FS.nodePermissions(lookup.node, 'x');
        if (err) {
          throw new FS.ErrnoError(err);
        }
        FS.currentPath = lookup.path;
      },createDefaultDirectories:function () {
        FS.mkdir('/tmp');
      },createDefaultDevices:function () {
        // create /dev
        FS.mkdir('/dev');
        // setup /dev/null
        FS.registerDevice(FS.makedev(1, 3), {
          read: function() { return 0; },
          write: function() { return 0; }
        });
        FS.mkdev('/dev/null', FS.makedev(1, 3));
        // setup /dev/tty and /dev/tty1
        // stderr needs to print output using Module['printErr']
        // so we register a second tty just for it.
        TTY.register(FS.makedev(5, 0), TTY.default_tty_ops);
        TTY.register(FS.makedev(6, 0), TTY.default_tty1_ops);
        FS.mkdev('/dev/tty', FS.makedev(5, 0));
        FS.mkdev('/dev/tty1', FS.makedev(6, 0));
        // we're not going to emulate the actual shm device,
        // just create the tmp dirs that reside in it commonly
        FS.mkdir('/dev/shm');
        FS.mkdir('/dev/shm/tmp');
      },createStandardStreams:function () {
        // TODO deprecate the old functionality of a single
        // input / output callback and that utilizes FS.createDevice
        // and instead require a unique set of stream ops

        // by default, we symlink the standard streams to the
        // default tty devices. however, if the standard streams
        // have been overwritten we create a unique device for
        // them instead.
        if (Module['stdin']) {
          FS.createDevice('/dev', 'stdin', Module['stdin']);
        } else {
          FS.symlink('/dev/tty', '/dev/stdin');
        }
        if (Module['stdout']) {
          FS.createDevice('/dev', 'stdout', null, Module['stdout']);
        } else {
          FS.symlink('/dev/tty', '/dev/stdout');
        }
        if (Module['stderr']) {
          FS.createDevice('/dev', 'stderr', null, Module['stderr']);
        } else {
          FS.symlink('/dev/tty1', '/dev/stderr');
        }

        // open default streams for the stdin, stdout and stderr devices
        var stdin = FS.open('/dev/stdin', 'r');
        HEAP32[((_stdin)>>2)]=FS.getPtrForStream(stdin);
        assert(stdin.fd === 0, 'invalid handle for stdin (' + stdin.fd + ')');

        var stdout = FS.open('/dev/stdout', 'w');
        HEAP32[((_stdout)>>2)]=FS.getPtrForStream(stdout);
        assert(stdout.fd === 1, 'invalid handle for stdout (' + stdout.fd + ')');

        var stderr = FS.open('/dev/stderr', 'w');
        HEAP32[((_stderr)>>2)]=FS.getPtrForStream(stderr);
        assert(stderr.fd === 2, 'invalid handle for stderr (' + stderr.fd + ')');
      },ensureErrnoError:function () {
        if (FS.ErrnoError) return;
        FS.ErrnoError = function ErrnoError(errno) {
          this.errno = errno;
          for (var key in ERRNO_CODES) {
            if (ERRNO_CODES[key] === errno) {
              this.code = key;
              break;
            }
          }
          this.message = ERRNO_MESSAGES[errno];
        };
        FS.ErrnoError.prototype = new Error();
        FS.ErrnoError.prototype.constructor = FS.ErrnoError;
        // Some errors may happen quite a bit, to avoid overhead we reuse them (and suffer a lack of stack info)
        [ERRNO_CODES.ENOENT].forEach(function(code) {
          FS.genericErrors[code] = new FS.ErrnoError(code);
          FS.genericErrors[code].stack = '<generic error, no stack>';
        });
      },staticInit:function () {
        FS.ensureErrnoError();

        FS.nameTable = new Array(4096);

        FS.mount(MEMFS, {}, '/');

        FS.createDefaultDirectories();
        FS.createDefaultDevices();
      },init:function (input, output, error) {
        assert(!FS.init.initialized, 'FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)');
        FS.init.initialized = true;

        FS.ensureErrnoError();

        // Allow Module.stdin etc. to provide defaults, if none explicitly passed to us here
        Module['stdin'] = input || Module['stdin'];
        Module['stdout'] = output || Module['stdout'];
        Module['stderr'] = error || Module['stderr'];

        FS.createStandardStreams();
      },quit:function () {
        FS.init.initialized = false;
        for (var i = 0; i < FS.streams.length; i++) {
          var stream = FS.streams[i];
          if (!stream) {
            continue;
          }
          FS.close(stream);
        }
      },getMode:function (canRead, canWrite) {
        var mode = 0;
        if (canRead) mode |= 292 | 73;
        if (canWrite) mode |= 146;
        return mode;
      },joinPath:function (parts, forceRelative) {
        var path = PATH.join.apply(null, parts);
        if (forceRelative && path[0] == '/') path = path.substr(1);
        return path;
      },absolutePath:function (relative, base) {
        return PATH.resolve(base, relative);
      },standardizePath:function (path) {
        return PATH.normalize(path);
      },findObject:function (path, dontResolveLastLink) {
        var ret = FS.analyzePath(path, dontResolveLastLink);
        if (ret.exists) {
          return ret.object;
        } else {
          ___setErrNo(ret.error);
          return null;
        }
      },analyzePath:function (path, dontResolveLastLink) {
        // operate from within the context of the symlink's target
        try {
          var lookup = FS.lookupPath(path, { follow: !dontResolveLastLink });
          path = lookup.path;
        } catch (e) {
        }
        var ret = {
          isRoot: false, exists: false, error: 0, name: null, path: null, object: null,
          parentExists: false, parentPath: null, parentObject: null
        };
        try {
          var lookup = FS.lookupPath(path, { parent: true });
          ret.parentExists = true;
          ret.parentPath = lookup.path;
          ret.parentObject = lookup.node;
          ret.name = PATH.basename(path);
          lookup = FS.lookupPath(path, { follow: !dontResolveLastLink });
          ret.exists = true;
          ret.path = lookup.path;
          ret.object = lookup.node;
          ret.name = lookup.node.name;
          ret.isRoot = lookup.path === '/';
        } catch (e) {
          ret.error = e.errno;
        };
        return ret;
      },createFolder:function (parent, name, canRead, canWrite) {
        var path = PATH.join2(typeof parent === 'string' ? parent : FS.getPath(parent), name);
        var mode = FS.getMode(canRead, canWrite);
        return FS.mkdir(path, mode);
      },createPath:function (parent, path, canRead, canWrite) {
        parent = typeof parent === 'string' ? parent : FS.getPath(parent);
        var parts = path.split('/').reverse();
        while (parts.length) {
          var part = parts.pop();
          if (!part) continue;
          var current = PATH.join2(parent, part);
          try {
            FS.mkdir(current);
          } catch (e) {
            // ignore EEXIST
          }
          parent = current;
        }
        return current;
      },createFile:function (parent, name, properties, canRead, canWrite) {
        var path = PATH.join2(typeof parent === 'string' ? parent : FS.getPath(parent), name);
        var mode = FS.getMode(canRead, canWrite);
        return FS.create(path, mode);
      },createDataFile:function (parent, name, data, canRead, canWrite, canOwn) {
        var path = name ? PATH.join2(typeof parent === 'string' ? parent : FS.getPath(parent), name) : parent;
        var mode = FS.getMode(canRead, canWrite);
        var node = FS.create(path, mode);
        if (data) {
          if (typeof data === 'string') {
            var arr = new Array(data.length);
            for (var i = 0, len = data.length; i < len; ++i) arr[i] = data.charCodeAt(i);
            data = arr;
          }
          // make sure we can write to the file
          FS.chmod(node, mode | 146);
          var stream = FS.open(node, 'w');
          FS.write(stream, data, 0, data.length, 0, canOwn);
          FS.close(stream);
          FS.chmod(node, mode);
        }
        return node;
      },createDevice:function (parent, name, input, output) {
        var path = PATH.join2(typeof parent === 'string' ? parent : FS.getPath(parent), name);
        var mode = FS.getMode(!!input, !!output);
        if (!FS.createDevice.major) FS.createDevice.major = 64;
        var dev = FS.makedev(FS.createDevice.major++, 0);
        // Create a fake device that a set of stream ops to emulate
        // the old behavior.
        FS.registerDevice(dev, {
          open: function(stream) {
            stream.seekable = false;
          },
          close: function(stream) {
            // flush any pending line data
            if (output && output.buffer && output.buffer.length) {
              output(10);
            }
          },
          read: function(stream, buffer, offset, length, pos /* ignored */) {
            var bytesRead = 0;
            for (var i = 0; i < length; i++) {
              var result;
              try {
                result = input();
              } catch (e) {
                throw new FS.ErrnoError(ERRNO_CODES.EIO);
              }
              if (result === undefined && bytesRead === 0) {
                throw new FS.ErrnoError(ERRNO_CODES.EAGAIN);
              }
              if (result === null || result === undefined) break;
              bytesRead++;
              buffer[offset+i] = result;
            }
            if (bytesRead) {
              stream.node.timestamp = Date.now();
            }
            return bytesRead;
          },
          write: function(stream, buffer, offset, length, pos) {
            for (var i = 0; i < length; i++) {
              try {
                output(buffer[offset+i]);
              } catch (e) {
                throw new FS.ErrnoError(ERRNO_CODES.EIO);
              }
            }
            if (length) {
              stream.node.timestamp = Date.now();
            }
            return i;
          }
        });
        return FS.mkdev(path, mode, dev);
      },createLink:function (parent, name, target, canRead, canWrite) {
        var path = PATH.join2(typeof parent === 'string' ? parent : FS.getPath(parent), name);
        return FS.symlink(target, path);
      },forceLoadFile:function (obj) {
        if (obj.isDevice || obj.isFolder || obj.link || obj.contents) return true;
        var success = true;
        if (typeof XMLHttpRequest !== 'undefined') {
          throw new Error("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
        } else if (Module['read']) {
          // Command-line.
          try {
            // WARNING: Can't read binary files in V8's d8 or tracemonkey's js, as
            //          read() will try to parse UTF8.
            obj.contents = intArrayFromString(Module['read'](obj.url), true);
          } catch (e) {
            success = false;
          }
        } else {
          throw new Error('Cannot load without read() or XMLHttpRequest.');
        }
        if (!success) ___setErrNo(ERRNO_CODES.EIO);
        return success;
      },createLazyFile:function (parent, name, url, canRead, canWrite) {
        // Lazy chunked Uint8Array (implements get and length from Uint8Array). Actual getting is abstracted away for eventual reuse.
        function LazyUint8Array() {
          this.lengthKnown = false;
          this.chunks = []; // Loaded chunks. Index is the chunk number
        }
        LazyUint8Array.prototype.get = function LazyUint8Array_get(idx) {
          if (idx > this.length-1 || idx < 0) {
            return undefined;
          }
          var chunkOffset = idx % this.chunkSize;
          var chunkNum = Math.floor(idx / this.chunkSize);
          return this.getter(chunkNum)[chunkOffset];
        }
        LazyUint8Array.prototype.setDataGetter = function LazyUint8Array_setDataGetter(getter) {
          this.getter = getter;
        }
        LazyUint8Array.prototype.cacheLength = function LazyUint8Array_cacheLength() {
            // Find length
            var xhr = new XMLHttpRequest();
            xhr.open('HEAD', url, false);
            xhr.send(null);
            if (!(xhr.status >= 200 && xhr.status < 300 || xhr.status === 304)) throw new Error("Couldn't load " + url + ". Status: " + xhr.status);
            var datalength = Number(xhr.getResponseHeader("Content-length"));
            var header;
            var hasByteServing = (header = xhr.getResponseHeader("Accept-Ranges")) && header === "bytes";
            var chunkSize = 1024*1024; // Chunk size in bytes

            if (!hasByteServing) chunkSize = datalength;

            // Function to get a range from the remote URL.
            var doXHR = (function(from, to) {
              if (from > to) throw new Error("invalid range (" + from + ", " + to + ") or no bytes requested!");
              if (to > datalength-1) throw new Error("only " + datalength + " bytes available! programmer error!");

              // TODO: Use mozResponseArrayBuffer, responseStream, etc. if available.
              var xhr = new XMLHttpRequest();
              xhr.open('GET', url, false);
              if (datalength !== chunkSize) xhr.setRequestHeader("Range", "bytes=" + from + "-" + to);

              // Some hints to the browser that we want binary data.
              if (typeof Uint8Array != 'undefined') xhr.responseType = 'arraybuffer';
              if (xhr.overrideMimeType) {
                xhr.overrideMimeType('text/plain; charset=x-user-defined');
              }

              xhr.send(null);
              if (!(xhr.status >= 200 && xhr.status < 300 || xhr.status === 304)) throw new Error("Couldn't load " + url + ". Status: " + xhr.status);
              if (xhr.response !== undefined) {
                return new Uint8Array(xhr.response || []);
              } else {
                return intArrayFromString(xhr.responseText || '', true);
              }
            });
            var lazyArray = this;
            lazyArray.setDataGetter(function(chunkNum) {
              var start = chunkNum * chunkSize;
              var end = (chunkNum+1) * chunkSize - 1; // including this byte
              end = Math.min(end, datalength-1); // if datalength-1 is selected, this is the last block
              if (typeof(lazyArray.chunks[chunkNum]) === "undefined") {
                lazyArray.chunks[chunkNum] = doXHR(start, end);
              }
              if (typeof(lazyArray.chunks[chunkNum]) === "undefined") throw new Error("doXHR failed!");
              return lazyArray.chunks[chunkNum];
            });

            this._length = datalength;
            this._chunkSize = chunkSize;
            this.lengthKnown = true;
        }
        if (typeof XMLHttpRequest !== 'undefined') {
          if (!ENVIRONMENT_IS_WORKER) throw 'Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc';
          var lazyArray = new LazyUint8Array();
          Object.defineProperty(lazyArray, "length", {
              get: function() {
                  if(!this.lengthKnown) {
                      this.cacheLength();
                  }
                  return this._length;
              }
          });
          Object.defineProperty(lazyArray, "chunkSize", {
              get: function() {
                  if(!this.lengthKnown) {
                      this.cacheLength();
                  }
                  return this._chunkSize;
              }
          });

          var properties = { isDevice: false, contents: lazyArray };
        } else {
          var properties = { isDevice: false, url: url };
        }

        var node = FS.createFile(parent, name, properties, canRead, canWrite);
        // This is a total hack, but I want to get this lazy file code out of the
        // core of MEMFS. If we want to keep this lazy file concept I feel it should
        // be its own thin LAZYFS proxying calls to MEMFS.
        if (properties.contents) {
          node.contents = properties.contents;
        } else if (properties.url) {
          node.contents = null;
          node.url = properties.url;
        }
        // override each stream op with one that tries to force load the lazy file first
        var stream_ops = {};
        var keys = Object.keys(node.stream_ops);
        keys.forEach(function(key) {
          var fn = node.stream_ops[key];
          stream_ops[key] = function forceLoadLazyFile() {
            if (!FS.forceLoadFile(node)) {
              throw new FS.ErrnoError(ERRNO_CODES.EIO);
            }
            return fn.apply(null, arguments);
          };
        });
        // use a custom read function
        stream_ops.read = function stream_ops_read(stream, buffer, offset, length, position) {
          if (!FS.forceLoadFile(node)) {
            throw new FS.ErrnoError(ERRNO_CODES.EIO);
          }
          var contents = stream.node.contents;
          if (position >= contents.length)
            return 0;
          var size = Math.min(contents.length - position, length);
          assert(size >= 0);
          if (contents.slice) { // normal array
            for (var i = 0; i < size; i++) {
              buffer[offset + i] = contents[position + i];
            }
          } else {
            for (var i = 0; i < size; i++) { // LazyUint8Array from sync binary XHR
              buffer[offset + i] = contents.get(position + i);
            }
          }
          return size;
        };
        node.stream_ops = stream_ops;
        return node;
      },createPreloadedFile:function (parent, name, url, canRead, canWrite, onload, onerror, dontCreateFile, canOwn) {
        Browser.init();
        // TODO we should allow people to just pass in a complete filename instead
        // of parent and name being that we just join them anyways
        var fullname = name ? PATH.resolve(PATH.join2(parent, name)) : parent;
        function processData(byteArray) {
          function finish(byteArray) {
            if (!dontCreateFile) {
              FS.createDataFile(parent, name, byteArray, canRead, canWrite, canOwn);
            }
            if (onload) onload();
            removeRunDependency('cp ' + fullname);
          }
          var handled = false;
          Module['preloadPlugins'].forEach(function(plugin) {
            if (handled) return;
            if (plugin['canHandle'](fullname)) {
              plugin['handle'](byteArray, fullname, finish, function() {
                if (onerror) onerror();
                removeRunDependency('cp ' + fullname);
              });
              handled = true;
            }
          });
          if (!handled) finish(byteArray);
        }
        addRunDependency('cp ' + fullname);
        if (typeof url == 'string') {
          Browser.asyncLoad(url, function(byteArray) {
            processData(byteArray);
          }, onerror);
        } else {
          processData(url);
        }
      },indexedDB:function () {
        return window.indexedDB || window.mozIndexedDB || window.webkitIndexedDB || window.msIndexedDB;
      },DB_NAME:function () {
        return 'EM_FS_' + window.location.pathname;
      },DB_VERSION:20,DB_STORE_NAME:"FILE_DATA",saveFilesToDB:function (paths, onload, onerror) {
        onload = onload || function(){};
        onerror = onerror || function(){};
        var indexedDB = FS.indexedDB();
        try {
          var openRequest = indexedDB.open(FS.DB_NAME(), FS.DB_VERSION);
        } catch (e) {
          return onerror(e);
        }
        openRequest.onupgradeneeded = function openRequest_onupgradeneeded() {
          console.log('creating db');
          var db = openRequest.result;
          db.createObjectStore(FS.DB_STORE_NAME);
        };
        openRequest.onsuccess = function openRequest_onsuccess() {
          var db = openRequest.result;
          var transaction = db.transaction([FS.DB_STORE_NAME], 'readwrite');
          var files = transaction.objectStore(FS.DB_STORE_NAME);
          var ok = 0, fail = 0, total = paths.length;
          function finish() {
            if (fail == 0) onload(); else onerror();
          }
          paths.forEach(function(path) {
            var putRequest = files.put(FS.analyzePath(path).object.contents, path);
            putRequest.onsuccess = function putRequest_onsuccess() { ok++; if (ok + fail == total) finish() };
            putRequest.onerror = function putRequest_onerror() { fail++; if (ok + fail == total) finish() };
          });
          transaction.onerror = onerror;
        };
        openRequest.onerror = onerror;
      },loadFilesFromDB:function (paths, onload, onerror) {
        onload = onload || function(){};
        onerror = onerror || function(){};
        var indexedDB = FS.indexedDB();
        try {
          var openRequest = indexedDB.open(FS.DB_NAME(), FS.DB_VERSION);
        } catch (e) {
          return onerror(e);
        }
        openRequest.onupgradeneeded = onerror; // no database to load from
        openRequest.onsuccess = function openRequest_onsuccess() {
          var db = openRequest.result;
          try {
            var transaction = db.transaction([FS.DB_STORE_NAME], 'readonly');
          } catch(e) {
            onerror(e);
            return;
          }
          var files = transaction.objectStore(FS.DB_STORE_NAME);
          var ok = 0, fail = 0, total = paths.length;
          function finish() {
            if (fail == 0) onload(); else onerror();
          }
          paths.forEach(function(path) {
            var getRequest = files.get(path);
            getRequest.onsuccess = function getRequest_onsuccess() {
              if (FS.analyzePath(path).exists) {
                FS.unlink(path);
              }
              FS.createDataFile(PATH.dirname(path), PATH.basename(path), getRequest.result, true, true, true);
              ok++;
              if (ok + fail == total) finish();
            };
            getRequest.onerror = function getRequest_onerror() { fail++; if (ok + fail == total) finish() };
          });
          transaction.onerror = onerror;
        };
        openRequest.onerror = onerror;
      }};




  function _mkport() { throw 'TODO' }var SOCKFS={mount:function (mount) {
        return FS.createNode(null, '/', 16384 | 511 /* 0777 */, 0);
      },createSocket:function (family, type, protocol) {
        var streaming = type == 1;
        if (protocol) {
          assert(streaming == (protocol == 6)); // if SOCK_STREAM, must be tcp
        }

        // create our internal socket structure
        var sock = {
          family: family,
          type: type,
          protocol: protocol,
          server: null,
          peers: {},
          pending: [],
          recv_queue: [],
          sock_ops: SOCKFS.websocket_sock_ops
        };

        // create the filesystem node to store the socket structure
        var name = SOCKFS.nextname();
        var node = FS.createNode(SOCKFS.root, name, 49152, 0);
        node.sock = sock;

        // and the wrapping stream that enables library functions such
        // as read and write to indirectly interact with the socket
        var stream = FS.createStream({
          path: name,
          node: node,
          flags: FS.modeStringToFlags('r+'),
          seekable: false,
          stream_ops: SOCKFS.stream_ops
        });

        // map the new stream to the socket structure (sockets have a 1:1
        // relationship with a stream)
        sock.stream = stream;

        return sock;
      },getSocket:function (fd) {
        var stream = FS.getStream(fd);
        if (!stream || !FS.isSocket(stream.node.mode)) {
          return null;
        }
        return stream.node.sock;
      },stream_ops:{poll:function (stream) {
          var sock = stream.node.sock;
          return sock.sock_ops.poll(sock);
        },ioctl:function (stream, request, varargs) {
          var sock = stream.node.sock;
          return sock.sock_ops.ioctl(sock, request, varargs);
        },read:function (stream, buffer, offset, length, position /* ignored */) {
          var sock = stream.node.sock;
          var msg = sock.sock_ops.recvmsg(sock, length);
          if (!msg) {
            // socket is closed
            return 0;
          }
          buffer.set(msg.buffer, offset);
          return msg.buffer.length;
        },write:function (stream, buffer, offset, length, position /* ignored */) {
          var sock = stream.node.sock;
          return sock.sock_ops.sendmsg(sock, buffer, offset, length);
        },close:function (stream) {
          var sock = stream.node.sock;
          sock.sock_ops.close(sock);
        }},nextname:function () {
        if (!SOCKFS.nextname.current) {
          SOCKFS.nextname.current = 0;
        }
        return 'socket[' + (SOCKFS.nextname.current++) + ']';
      },websocket_sock_ops:{createPeer:function (sock, addr, port) {
          var ws;

          if (typeof addr === 'object') {
            ws = addr;
            addr = null;
            port = null;
          }

          if (ws) {
            // for sockets that've already connected (e.g. we're the server)
            // we can inspect the _socket property for the address
            if (ws._socket) {
              addr = ws._socket.remoteAddress;
              port = ws._socket.remotePort;
            }
            // if we're just now initializing a connection to the remote,
            // inspect the url property
            else {
              var result = /ws[s]?:\/\/([^:]+):(\d+)/.exec(ws.url);
              if (!result) {
                throw new Error('WebSocket URL must be in the format ws(s)://address:port');
              }
              addr = result[1];
              port = parseInt(result[2], 10);
            }
          } else {
            // create the actual websocket object and connect
            try {
              // runtimeConfig gets set to true if WebSocket runtime configuration is available.
              var runtimeConfig = (Module['websocket'] && ('object' === typeof Module['websocket']));

              // The default value is 'ws://' the replace is needed because the compiler replaces "//" comments with '#'
              // comments without checking context, so we'd end up with ws:#, the replace swaps the "#" for "//" again.
              var url = 'ws:#'.replace('#', '//');

              if (runtimeConfig) {
                if ('string' === typeof Module['websocket']['url']) {
                  url = Module['websocket']['url']; // Fetch runtime WebSocket URL config.
                }
              }

              if (url === 'ws://' || url === 'wss://') { // Is the supplied URL config just a prefix, if so complete it.
                url = url + addr + ':' + port;
              }

              // Make the WebSocket subprotocol (Sec-WebSocket-Protocol) default to binary if no configuration is set.
              var subProtocols = 'binary'; // The default value is 'binary'

              if (runtimeConfig) {
                if ('string' === typeof Module['websocket']['subprotocol']) {
                  subProtocols = Module['websocket']['subprotocol']; // Fetch runtime WebSocket subprotocol config.
                }
              }

              // The regex trims the string (removes spaces at the beginning and end, then splits the string by
              // <any space>,<any space> into an Array. Whitespace removal is important for Websockify and ws.
              subProtocols = subProtocols.replace(/^ +| +$/g,"").split(/ *, */);

              // The node ws library API for specifying optional subprotocol is slightly different than the browser's.
              var opts = ENVIRONMENT_IS_NODE ? {'protocol': subProtocols.toString()} : subProtocols;

              // If node we use the ws library.
              var WebSocket = ENVIRONMENT_IS_NODE ? require('ws') : window['WebSocket'];
              ws = new WebSocket(url, opts);
              ws.binaryType = 'arraybuffer';
            } catch (e) {
              throw new FS.ErrnoError(ERRNO_CODES.EHOSTUNREACH);
            }
          }


          var peer = {
            addr: addr,
            port: port,
            socket: ws,
            dgram_send_queue: []
          };

          SOCKFS.websocket_sock_ops.addPeer(sock, peer);
          SOCKFS.websocket_sock_ops.handlePeerEvents(sock, peer);

          // if this is a bound dgram socket, send the port number first to allow
          // us to override the ephemeral port reported to us by remotePort on the
          // remote end.
          if (sock.type === 2 && typeof sock.sport !== 'undefined') {
            peer.dgram_send_queue.push(new Uint8Array([
                255, 255, 255, 255,
                'p'.charCodeAt(0), 'o'.charCodeAt(0), 'r'.charCodeAt(0), 't'.charCodeAt(0),
                ((sock.sport & 0xff00) >> 8) , (sock.sport & 0xff)
            ]));
          }

          return peer;
        },getPeer:function (sock, addr, port) {
          return sock.peers[addr + ':' + port];
        },addPeer:function (sock, peer) {
          sock.peers[peer.addr + ':' + peer.port] = peer;
        },removePeer:function (sock, peer) {
          delete sock.peers[peer.addr + ':' + peer.port];
        },handlePeerEvents:function (sock, peer) {
          var first = true;

          var handleOpen = function () {
            try {
              var queued = peer.dgram_send_queue.shift();
              while (queued) {
                peer.socket.send(queued);
                queued = peer.dgram_send_queue.shift();
              }
            } catch (e) {
              // not much we can do here in the way of proper error handling as we've already
              // lied and said this data was sent. shut it down.
              peer.socket.close();
            }
          };

          function handleMessage(data) {
            assert(typeof data !== 'string' && data.byteLength !== undefined);  // must receive an ArrayBuffer
            data = new Uint8Array(data);  // make a typed array view on the array buffer


            // if this is the port message, override the peer's port with it
            var wasfirst = first;
            first = false;
            if (wasfirst &&
                data.length === 10 &&
                data[0] === 255 && data[1] === 255 && data[2] === 255 && data[3] === 255 &&
                data[4] === 'p'.charCodeAt(0) && data[5] === 'o'.charCodeAt(0) && data[6] === 'r'.charCodeAt(0) && data[7] === 't'.charCodeAt(0)) {
              // update the peer's port and it's key in the peer map
              var newport = ((data[8] << 8) | data[9]);
              SOCKFS.websocket_sock_ops.removePeer(sock, peer);
              peer.port = newport;
              SOCKFS.websocket_sock_ops.addPeer(sock, peer);
              return;
            }

            sock.recv_queue.push({ addr: peer.addr, port: peer.port, data: data });
          };

          if (ENVIRONMENT_IS_NODE) {
            peer.socket.on('open', handleOpen);
            peer.socket.on('message', function(data, flags) {
              if (!flags.binary) {
                return;
              }
              handleMessage((new Uint8Array(data)).buffer);  // copy from node Buffer -> ArrayBuffer
            });
            peer.socket.on('error', function() {
              // don't throw
            });
          } else {
            peer.socket.onopen = handleOpen;
            peer.socket.onmessage = function peer_socket_onmessage(event) {
              handleMessage(event.data);
            };
          }
        },poll:function (sock) {
          if (sock.type === 1 && sock.server) {
            // listen sockets should only say they're available for reading
            // if there are pending clients.
            return sock.pending.length ? (64 | 1) : 0;
          }

          var mask = 0;
          var dest = sock.type === 1 ?  // we only care about the socket state for connection-based sockets
            SOCKFS.websocket_sock_ops.getPeer(sock, sock.daddr, sock.dport) :
            null;

          if (sock.recv_queue.length ||
              !dest ||  // connection-less sockets are always ready to read
              (dest && dest.socket.readyState === dest.socket.CLOSING) ||
              (dest && dest.socket.readyState === dest.socket.CLOSED)) {  // let recv return 0 once closed
            mask |= (64 | 1);
          }

          if (!dest ||  // connection-less sockets are always ready to write
              (dest && dest.socket.readyState === dest.socket.OPEN)) {
            mask |= 4;
          }

          if ((dest && dest.socket.readyState === dest.socket.CLOSING) ||
              (dest && dest.socket.readyState === dest.socket.CLOSED)) {
            mask |= 16;
          }

          return mask;
        },ioctl:function (sock, request, arg) {
          switch (request) {
            case 21531:
              var bytes = 0;
              if (sock.recv_queue.length) {
                bytes = sock.recv_queue[0].data.length;
              }
              HEAP32[((arg)>>2)]=bytes;
              return 0;
            default:
              return ERRNO_CODES.EINVAL;
          }
        },close:function (sock) {
          // if we've spawned a listen server, close it
          if (sock.server) {
            try {
              sock.server.close();
            } catch (e) {
            }
            sock.server = null;
          }
          // close any peer connections
          var peers = Object.keys(sock.peers);
          for (var i = 0; i < peers.length; i++) {
            var peer = sock.peers[peers[i]];
            try {
              peer.socket.close();
            } catch (e) {
            }
            SOCKFS.websocket_sock_ops.removePeer(sock, peer);
          }
          return 0;
        },bind:function (sock, addr, port) {
          if (typeof sock.saddr !== 'undefined' || typeof sock.sport !== 'undefined') {
            throw new FS.ErrnoError(ERRNO_CODES.EINVAL);  // already bound
          }
          sock.saddr = addr;
          sock.sport = port || _mkport();
          // in order to emulate dgram sockets, we need to launch a listen server when
          // binding on a connection-less socket
          // note: this is only required on the server side
          if (sock.type === 2) {
            // close the existing server if it exists
            if (sock.server) {
              sock.server.close();
              sock.server = null;
            }
            // swallow error operation not supported error that occurs when binding in the
            // browser where this isn't supported
            try {
              sock.sock_ops.listen(sock, 0);
            } catch (e) {
              if (!(e instanceof FS.ErrnoError)) throw e;
              if (e.errno !== ERRNO_CODES.EOPNOTSUPP) throw e;
            }
          }
        },connect:function (sock, addr, port) {
          if (sock.server) {
            throw new FS.ErrnoError(ERRNO_CODS.EOPNOTSUPP);
          }

          // TODO autobind
          // if (!sock.addr && sock.type == 2) {
          // }

          // early out if we're already connected / in the middle of connecting
          if (typeof sock.daddr !== 'undefined' && typeof sock.dport !== 'undefined') {
            var dest = SOCKFS.websocket_sock_ops.getPeer(sock, sock.daddr, sock.dport);
            if (dest) {
              if (dest.socket.readyState === dest.socket.CONNECTING) {
                throw new FS.ErrnoError(ERRNO_CODES.EALREADY);
              } else {
                throw new FS.ErrnoError(ERRNO_CODES.EISCONN);
              }
            }
          }

          // add the socket to our peer list and set our
          // destination address / port to match
          var peer = SOCKFS.websocket_sock_ops.createPeer(sock, addr, port);
          sock.daddr = peer.addr;
          sock.dport = peer.port;

          // always "fail" in non-blocking mode
          throw new FS.ErrnoError(ERRNO_CODES.EINPROGRESS);
        },listen:function (sock, backlog) {
          if (!ENVIRONMENT_IS_NODE) {
            throw new FS.ErrnoError(ERRNO_CODES.EOPNOTSUPP);
          }
          if (sock.server) {
             throw new FS.ErrnoError(ERRNO_CODES.EINVAL);  // already listening
          }
          var WebSocketServer = require('ws').Server;
          var host = sock.saddr;
          sock.server = new WebSocketServer({
            host: host,
            port: sock.sport
            // TODO support backlog
          });

          sock.server.on('connection', function(ws) {
            if (sock.type === 1) {
              var newsock = SOCKFS.createSocket(sock.family, sock.type, sock.protocol);

              // create a peer on the new socket
              var peer = SOCKFS.websocket_sock_ops.createPeer(newsock, ws);
              newsock.daddr = peer.addr;
              newsock.dport = peer.port;

              // push to queue for accept to pick up
              sock.pending.push(newsock);
            } else {
              // create a peer on the listen socket so calling sendto
              // with the listen socket and an address will resolve
              // to the correct client
              SOCKFS.websocket_sock_ops.createPeer(sock, ws);
            }
          });
          sock.server.on('closed', function() {
            sock.server = null;
          });
          sock.server.on('error', function() {
            // don't throw
          });
        },accept:function (listensock) {
          if (!listensock.server) {
            throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
          }
          var newsock = listensock.pending.shift();
          newsock.stream.flags = listensock.stream.flags;
          return newsock;
        },getname:function (sock, peer) {
          var addr, port;
          if (peer) {
            if (sock.daddr === undefined || sock.dport === undefined) {
              throw new FS.ErrnoError(ERRNO_CODES.ENOTCONN);
            }
            addr = sock.daddr;
            port = sock.dport;
          } else {
            // TODO saddr and sport will be set for bind()'d UDP sockets, but what
            // should we be returning for TCP sockets that've been connect()'d?
            addr = sock.saddr || 0;
            port = sock.sport || 0;
          }
          return { addr: addr, port: port };
        },sendmsg:function (sock, buffer, offset, length, addr, port) {
          if (sock.type === 2) {
            // connection-less sockets will honor the message address,
            // and otherwise fall back to the bound destination address
            if (addr === undefined || port === undefined) {
              addr = sock.daddr;
              port = sock.dport;
            }
            // if there was no address to fall back to, error out
            if (addr === undefined || port === undefined) {
              throw new FS.ErrnoError(ERRNO_CODES.EDESTADDRREQ);
            }
          } else {
            // connection-based sockets will only use the bound
            addr = sock.daddr;
            port = sock.dport;
          }

          // find the peer for the destination address
          var dest = SOCKFS.websocket_sock_ops.getPeer(sock, addr, port);

          // early out if not connected with a connection-based socket
          if (sock.type === 1) {
            if (!dest || dest.socket.readyState === dest.socket.CLOSING || dest.socket.readyState === dest.socket.CLOSED) {
              throw new FS.ErrnoError(ERRNO_CODES.ENOTCONN);
            } else if (dest.socket.readyState === dest.socket.CONNECTING) {
              throw new FS.ErrnoError(ERRNO_CODES.EAGAIN);
            }
          }

          // create a copy of the incoming data to send, as the WebSocket API
          // doesn't work entirely with an ArrayBufferView, it'll just send
          // the entire underlying buffer
          var data;
          if (buffer instanceof Array || buffer instanceof ArrayBuffer) {
            data = buffer.slice(offset, offset + length);
          } else {  // ArrayBufferView
            data = buffer.buffer.slice(buffer.byteOffset + offset, buffer.byteOffset + offset + length);
          }

          // if we're emulating a connection-less dgram socket and don't have
          // a cached connection, queue the buffer to send upon connect and
          // lie, saying the data was sent now.
          if (sock.type === 2) {
            if (!dest || dest.socket.readyState !== dest.socket.OPEN) {
              // if we're not connected, open a new connection
              if (!dest || dest.socket.readyState === dest.socket.CLOSING || dest.socket.readyState === dest.socket.CLOSED) {
                dest = SOCKFS.websocket_sock_ops.createPeer(sock, addr, port);
              }
              dest.dgram_send_queue.push(data);
              return length;
            }
          }

          try {
            // send the actual data
            dest.socket.send(data);
            return length;
          } catch (e) {
            throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
          }
        },recvmsg:function (sock, length) {
          // http://pubs.opengroup.org/onlinepubs/7908799/xns/recvmsg.html
          if (sock.type === 1 && sock.server) {
            // tcp servers should not be recv()'ing on the listen socket
            throw new FS.ErrnoError(ERRNO_CODES.ENOTCONN);
          }

          var queued = sock.recv_queue.shift();
          if (!queued) {
            if (sock.type === 1) {
              var dest = SOCKFS.websocket_sock_ops.getPeer(sock, sock.daddr, sock.dport);

              if (!dest) {
                // if we have a destination address but are not connected, error out
                throw new FS.ErrnoError(ERRNO_CODES.ENOTCONN);
              }
              else if (dest.socket.readyState === dest.socket.CLOSING || dest.socket.readyState === dest.socket.CLOSED) {
                // return null if the socket has closed
                return null;
              }
              else {
                // else, our socket is in a valid state but truly has nothing available
                throw new FS.ErrnoError(ERRNO_CODES.EAGAIN);
              }
            } else {
              throw new FS.ErrnoError(ERRNO_CODES.EAGAIN);
            }
          }

          // queued.data will be an ArrayBuffer if it's unadulterated, but if it's
          // requeued TCP data it'll be an ArrayBufferView
          var queuedLength = queued.data.byteLength || queued.data.length;
          var queuedOffset = queued.data.byteOffset || 0;
          var queuedBuffer = queued.data.buffer || queued.data;
          var bytesRead = Math.min(length, queuedLength);
          var res = {
            buffer: new Uint8Array(queuedBuffer, queuedOffset, bytesRead),
            addr: queued.addr,
            port: queued.port
          };


          // push back any unread data for TCP connections
          if (sock.type === 1 && bytesRead < queuedLength) {
            var bytesRemaining = queuedLength - bytesRead;
            queued.data = new Uint8Array(queuedBuffer, queuedOffset + bytesRead, bytesRemaining);
            sock.recv_queue.unshift(queued);
          }

          return res;
        }}};function _send(fd, buf, len, flags) {
      var sock = SOCKFS.getSocket(fd);
      if (!sock) {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      }
      // TODO honor flags
      return _write(fd, buf, len);
    }

  function _pwrite(fildes, buf, nbyte, offset) {
      // ssize_t pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/write.html
      var stream = FS.getStream(fildes);
      if (!stream) {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      }
      try {
        var slab = HEAP8;
        return FS.write(stream, slab, buf, nbyte, offset);
      } catch (e) {
        FS.handleFSError(e);
        return -1;
      }
    }function _write(fildes, buf, nbyte) {
      // ssize_t write(int fildes, const void *buf, size_t nbyte);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/write.html
      var stream = FS.getStream(fildes);
      if (!stream) {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      }


      try {
        var slab = HEAP8;
        return FS.write(stream, slab, buf, nbyte);
      } catch (e) {
        FS.handleFSError(e);
        return -1;
      }
    }

  function _fileno(stream) {
      // int fileno(FILE *stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fileno.html
      stream = FS.getStreamFromPtr(stream);
      if (!stream) return -1;
      return stream.fd;
    }function _fwrite(ptr, size, nitems, stream) {
      // size_t fwrite(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fwrite.html
      var bytesToWrite = nitems * size;
      if (bytesToWrite == 0) return 0;
      var fd = _fileno(stream);
      var bytesWritten = _write(fd, ptr, bytesToWrite);
      if (bytesWritten == -1) {
        var streamObj = FS.getStreamFromPtr(stream);
        if (streamObj) streamObj.error = true;
        return 0;
      } else {
        return Math.floor(bytesWritten / size);
      }
    }



  Module["_strlen"] = _strlen;

  function __reallyNegative(x) {
      return x < 0 || (x === 0 && (1/x) === -Infinity);
    }function __formatString(format, varargs) {
      var textIndex = format;
      var argIndex = 0;
      function getNextArg(type) {
        // NOTE: Explicitly ignoring type safety. Otherwise this fails:
        //       int x = 4; printf("%c\n", (char)x);
        var ret;
        if (type === 'double') {
          ret = HEAPF64[(((varargs)+(argIndex))>>3)];
        } else if (type == 'i64') {
          ret = [HEAP32[(((varargs)+(argIndex))>>2)],
                 HEAP32[(((varargs)+(argIndex+4))>>2)]];

        } else {
          type = 'i32'; // varargs are always i32, i64, or double
          ret = HEAP32[(((varargs)+(argIndex))>>2)];
        }
        argIndex += Runtime.getNativeFieldSize(type);
        return ret;
      }

      var ret = [];
      var curr, next, currArg;
      while(1) {
        var startTextIndex = textIndex;
        curr = HEAP8[(textIndex)];
        if (curr === 0) break;
        next = HEAP8[((textIndex+1)|0)];
        if (curr == 37) {
          // Handle flags.
          var flagAlwaysSigned = false;
          var flagLeftAlign = false;
          var flagAlternative = false;
          var flagZeroPad = false;
          var flagPadSign = false;
          flagsLoop: while (1) {
            switch (next) {
              case 43:
                flagAlwaysSigned = true;
                break;
              case 45:
                flagLeftAlign = true;
                break;
              case 35:
                flagAlternative = true;
                break;
              case 48:
                if (flagZeroPad) {
                  break flagsLoop;
                } else {
                  flagZeroPad = true;
                  break;
                }
              case 32:
                flagPadSign = true;
                break;
              default:
                break flagsLoop;
            }
            textIndex++;
            next = HEAP8[((textIndex+1)|0)];
          }

          // Handle width.
          var width = 0;
          if (next == 42) {
            width = getNextArg('i32');
            textIndex++;
            next = HEAP8[((textIndex+1)|0)];
          } else {
            while (next >= 48 && next <= 57) {
              width = width * 10 + (next - 48);
              textIndex++;
              next = HEAP8[((textIndex+1)|0)];
            }
          }

          // Handle precision.
          var precisionSet = false, precision = -1;
          if (next == 46) {
            precision = 0;
            precisionSet = true;
            textIndex++;
            next = HEAP8[((textIndex+1)|0)];
            if (next == 42) {
              precision = getNextArg('i32');
              textIndex++;
            } else {
              while(1) {
                var precisionChr = HEAP8[((textIndex+1)|0)];
                if (precisionChr < 48 ||
                    precisionChr > 57) break;
                precision = precision * 10 + (precisionChr - 48);
                textIndex++;
              }
            }
            next = HEAP8[((textIndex+1)|0)];
          }
          if (precision < 0) {
            precision = 6; // Standard default.
            precisionSet = false;
          }

          // Handle integer sizes. WARNING: These assume a 32-bit architecture!
          var argSize;
          switch (String.fromCharCode(next)) {
            case 'h':
              var nextNext = HEAP8[((textIndex+2)|0)];
              if (nextNext == 104) {
                textIndex++;
                argSize = 1; // char (actually i32 in varargs)
              } else {
                argSize = 2; // short (actually i32 in varargs)
              }
              break;
            case 'l':
              var nextNext = HEAP8[((textIndex+2)|0)];
              if (nextNext == 108) {
                textIndex++;
                argSize = 8; // long long
              } else {
                argSize = 4; // long
              }
              break;
            case 'L': // long long
            case 'q': // int64_t
            case 'j': // intmax_t
              argSize = 8;
              break;
            case 'z': // size_t
            case 't': // ptrdiff_t
            case 'I': // signed ptrdiff_t or unsigned size_t
              argSize = 4;
              break;
            default:
              argSize = null;
          }
          if (argSize) textIndex++;
          next = HEAP8[((textIndex+1)|0)];

          // Handle type specifier.
          switch (String.fromCharCode(next)) {
            case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': case 'p': {
              // Integer.
              var signed = next == 100 || next == 105;
              argSize = argSize || 4;
              var currArg = getNextArg('i' + (argSize * 8));
              var argText;
              // Flatten i64-1 [low, high] into a (slightly rounded) double
              if (argSize == 8) {
                currArg = Runtime.makeBigInt(currArg[0], currArg[1], next == 117);
              }
              // Truncate to requested size.
              if (argSize <= 4) {
                var limit = Math.pow(256, argSize) - 1;
                currArg = (signed ? reSign : unSign)(currArg & limit, argSize * 8);
              }
              // Format the number.
              var currAbsArg = Math.abs(currArg);
              var prefix = '';
              if (next == 100 || next == 105) {
                argText = reSign(currArg, 8 * argSize, 1).toString(10);
              } else if (next == 117) {
                argText = unSign(currArg, 8 * argSize, 1).toString(10);
                currArg = Math.abs(currArg);
              } else if (next == 111) {
                argText = (flagAlternative ? '0' : '') + currAbsArg.toString(8);
              } else if (next == 120 || next == 88) {
                prefix = (flagAlternative && currArg != 0) ? '0x' : '';
                if (currArg < 0) {
                  // Represent negative numbers in hex as 2's complement.
                  currArg = -currArg;
                  argText = (currAbsArg - 1).toString(16);
                  var buffer = [];
                  for (var i = 0; i < argText.length; i++) {
                    buffer.push((0xF - parseInt(argText[i], 16)).toString(16));
                  }
                  argText = buffer.join('');
                  while (argText.length < argSize * 2) argText = 'f' + argText;
                } else {
                  argText = currAbsArg.toString(16);
                }
                if (next == 88) {
                  prefix = prefix.toUpperCase();
                  argText = argText.toUpperCase();
                }
              } else if (next == 112) {
                if (currAbsArg === 0) {
                  argText = '(nil)';
                } else {
                  prefix = '0x';
                  argText = currAbsArg.toString(16);
                }
              }
              if (precisionSet) {
                while (argText.length < precision) {
                  argText = '0' + argText;
                }
              }

              // Add sign if needed
              if (currArg >= 0) {
                if (flagAlwaysSigned) {
                  prefix = '+' + prefix;
                } else if (flagPadSign) {
                  prefix = ' ' + prefix;
                }
              }

              // Move sign to prefix so we zero-pad after the sign
              if (argText.charAt(0) == '-') {
                prefix = '-' + prefix;
                argText = argText.substr(1);
              }

              // Add padding.
              while (prefix.length + argText.length < width) {
                if (flagLeftAlign) {
                  argText += ' ';
                } else {
                  if (flagZeroPad) {
                    argText = '0' + argText;
                  } else {
                    prefix = ' ' + prefix;
                  }
                }
              }

              // Insert the result into the buffer.
              argText = prefix + argText;
              argText.split('').forEach(function(chr) {
                ret.push(chr.charCodeAt(0));
              });
              break;
            }
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
              // Float.
              var currArg = getNextArg('double');
              var argText;
              if (isNaN(currArg)) {
                argText = 'nan';
                flagZeroPad = false;
              } else if (!isFinite(currArg)) {
                argText = (currArg < 0 ? '-' : '') + 'inf';
                flagZeroPad = false;
              } else {
                var isGeneral = false;
                var effectivePrecision = Math.min(precision, 20);

                // Convert g/G to f/F or e/E, as per:
                // http://pubs.opengroup.org/onlinepubs/9699919799/functions/printf.html
                if (next == 103 || next == 71) {
                  isGeneral = true;
                  precision = precision || 1;
                  var exponent = parseInt(currArg.toExponential(effectivePrecision).split('e')[1], 10);
                  if (precision > exponent && exponent >= -4) {
                    next = ((next == 103) ? 'f' : 'F').charCodeAt(0);
                    precision -= exponent + 1;
                  } else {
                    next = ((next == 103) ? 'e' : 'E').charCodeAt(0);
                    precision--;
                  }
                  effectivePrecision = Math.min(precision, 20);
                }

                if (next == 101 || next == 69) {
                  argText = currArg.toExponential(effectivePrecision);
                  // Make sure the exponent has at least 2 digits.
                  if (/[eE][-+]\d$/.test(argText)) {
                    argText = argText.slice(0, -1) + '0' + argText.slice(-1);
                  }
                } else if (next == 102 || next == 70) {
                  argText = currArg.toFixed(effectivePrecision);
                  if (currArg === 0 && __reallyNegative(currArg)) {
                    argText = '-' + argText;
                  }
                }

                var parts = argText.split('e');
                if (isGeneral && !flagAlternative) {
                  // Discard trailing zeros and periods.
                  while (parts[0].length > 1 && parts[0].indexOf('.') != -1 &&
                         (parts[0].slice(-1) == '0' || parts[0].slice(-1) == '.')) {
                    parts[0] = parts[0].slice(0, -1);
                  }
                } else {
                  // Make sure we have a period in alternative mode.
                  if (flagAlternative && argText.indexOf('.') == -1) parts[0] += '.';
                  // Zero pad until required precision.
                  while (precision > effectivePrecision++) parts[0] += '0';
                }
                argText = parts[0] + (parts.length > 1 ? 'e' + parts[1] : '');

                // Capitalize 'E' if needed.
                if (next == 69) argText = argText.toUpperCase();

                // Add sign.
                if (currArg >= 0) {
                  if (flagAlwaysSigned) {
                    argText = '+' + argText;
                  } else if (flagPadSign) {
                    argText = ' ' + argText;
                  }
                }
              }

              // Add padding.
              while (argText.length < width) {
                if (flagLeftAlign) {
                  argText += ' ';
                } else {
                  if (flagZeroPad && (argText[0] == '-' || argText[0] == '+')) {
                    argText = argText[0] + '0' + argText.slice(1);
                  } else {
                    argText = (flagZeroPad ? '0' : ' ') + argText;
                  }
                }
              }

              // Adjust case.
              if (next < 97) argText = argText.toUpperCase();

              // Insert the result into the buffer.
              argText.split('').forEach(function(chr) {
                ret.push(chr.charCodeAt(0));
              });
              break;
            }
            case 's': {
              // String.
              var arg = getNextArg('i8*');
              var argLength = arg ? _strlen(arg) : '(null)'.length;
              if (precisionSet) argLength = Math.min(argLength, precision);
              if (!flagLeftAlign) {
                while (argLength < width--) {
                  ret.push(32);
                }
              }
              if (arg) {
                for (var i = 0; i < argLength; i++) {
                  ret.push(HEAPU8[((arg++)|0)]);
                }
              } else {
                ret = ret.concat(intArrayFromString('(null)'.substr(0, argLength), true));
              }
              if (flagLeftAlign) {
                while (argLength < width--) {
                  ret.push(32);
                }
              }
              break;
            }
            case 'c': {
              // Character.
              if (flagLeftAlign) ret.push(getNextArg('i8'));
              while (--width > 0) {
                ret.push(32);
              }
              if (!flagLeftAlign) ret.push(getNextArg('i8'));
              break;
            }
            case 'n': {
              // Write the length written so far to the next parameter.
              var ptr = getNextArg('i32*');
              HEAP32[((ptr)>>2)]=ret.length;
              break;
            }
            case '%': {
              // Literal percent sign.
              ret.push(curr);
              break;
            }
            default: {
              // Unknown specifiers remain untouched.
              for (var i = startTextIndex; i < textIndex + 2; i++) {
                ret.push(HEAP8[(i)]);
              }
            }
          }
          textIndex += 2;
          // TODO: Support a/A (hex float) and m (last error) specifiers.
          // TODO: Support %1${specifier} for arg selection.
        } else {
          ret.push(curr);
          textIndex += 1;
        }
      }
      return ret;
    }function _fprintf(stream, format, varargs) {
      // int fprintf(FILE *restrict stream, const char *restrict format, ...);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/printf.html
      var result = __formatString(format, varargs);
      var stack = Runtime.stackSave();
      var ret = _fwrite(allocate(result, 'i8', ALLOC_STACK), 1, result.length, stream);
      Runtime.stackRestore(stack);
      return ret;
    }function _printf(format, varargs) {
      // int printf(const char *restrict format, ...);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/printf.html
      var stdout = HEAP32[((_stdout)>>2)];
      return _fprintf(stdout, format, varargs);
    }


  function _fputs(s, stream) {
      // int fputs(const char *restrict s, FILE *restrict stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fputs.html
      var fd = _fileno(stream);
      return _write(fd, s, _strlen(s));
    }

  function _fputc(c, stream) {
      // int fputc(int c, FILE *stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fputc.html
      var chr = unSign(c & 0xFF);
      HEAP8[((_fputc.ret)|0)]=chr;
      var fd = _fileno(stream);
      var ret = _write(fd, _fputc.ret, 1);
      if (ret == -1) {
        var streamObj = FS.getStreamFromPtr(stream);
        if (streamObj) streamObj.error = true;
        return -1;
      } else {
        return chr;
      }
    }function _puts(s) {
      // int puts(const char *s);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/puts.html
      // NOTE: puts() always writes an extra newline.
      var stdout = HEAP32[((_stdout)>>2)];
      var ret = _fputs(s, stdout);
      if (ret < 0) {
        return ret;
      } else {
        var newlineRet = _fputc(10, stdout);
        return (newlineRet < 0) ? -1 : ret + 1;
      }
    }

  function _sysconf(name) {
      // long sysconf(int name);
      // http://pubs.opengroup.org/onlinepubs/009695399/functions/sysconf.html
      switch(name) {
        case 30: return PAGE_SIZE;
        case 132:
        case 133:
        case 12:
        case 137:
        case 138:
        case 15:
        case 235:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 149:
        case 13:
        case 10:
        case 236:
        case 153:
        case 9:
        case 21:
        case 22:
        case 159:
        case 154:
        case 14:
        case 77:
        case 78:
        case 139:
        case 80:
        case 81:
        case 79:
        case 82:
        case 68:
        case 67:
        case 164:
        case 11:
        case 29:
        case 47:
        case 48:
        case 95:
        case 52:
        case 51:
        case 46:
          return 200809;
        case 27:
        case 246:
        case 127:
        case 128:
        case 23:
        case 24:
        case 160:
        case 161:
        case 181:
        case 182:
        case 242:
        case 183:
        case 184:
        case 243:
        case 244:
        case 245:
        case 165:
        case 178:
        case 179:
        case 49:
        case 50:
        case 168:
        case 169:
        case 175:
        case 170:
        case 171:
        case 172:
        case 97:
        case 76:
        case 32:
        case 173:
        case 35:
          return -1;
        case 176:
        case 177:
        case 7:
        case 155:
        case 8:
        case 157:
        case 125:
        case 126:
        case 92:
        case 93:
        case 129:
        case 130:
        case 131:
        case 94:
        case 91:
          return 1;
        case 74:
        case 60:
        case 69:
        case 70:
        case 4:
          return 1024;
        case 31:
        case 42:
        case 72:
          return 32;
        case 87:
        case 26:
        case 33:
          return 2147483647;
        case 34:
        case 1:
          return 47839;
        case 38:
        case 36:
          return 99;
        case 43:
        case 37:
          return 2048;
        case 0: return 2097152;
        case 3: return 65536;
        case 28: return 32768;
        case 44: return 32767;
        case 75: return 16384;
        case 39: return 1000;
        case 89: return 700;
        case 71: return 256;
        case 40: return 255;
        case 2: return 100;
        case 180: return 64;
        case 25: return 20;
        case 5: return 16;
        case 6: return 6;
        case 73: return 4;
        case 84: return 1;
      }
      ___setErrNo(ERRNO_CODES.EINVAL);
      return -1;
    }


  Module["_memset"] = _memset;

  function ___errno_location() {
      return ___errno_state;
    }

  function _abort() {
      Module['abort']();
    }

  var Browser={mainLoop:{scheduler:null,method:"",shouldPause:false,paused:false,queue:[],pause:function () {
          Browser.mainLoop.shouldPause = true;
        },resume:function () {
          if (Browser.mainLoop.paused) {
            Browser.mainLoop.paused = false;
            Browser.mainLoop.scheduler();
          }
          Browser.mainLoop.shouldPause = false;
        },updateStatus:function () {
          if (Module['setStatus']) {
            var message = Module['statusMessage'] || 'Please wait...';
            var remaining = Browser.mainLoop.remainingBlockers;
            var expected = Browser.mainLoop.expectedBlockers;
            if (remaining) {
              if (remaining < expected) {
                Module['setStatus'](message + ' (' + (expected - remaining) + '/' + expected + ')');
              } else {
                Module['setStatus'](message);
              }
            } else {
              Module['setStatus']('');
            }
          }
        }},isFullScreen:false,pointerLock:false,moduleContextCreatedCallbacks:[],workers:[],init:function () {
        if (!Module["preloadPlugins"]) Module["preloadPlugins"] = []; // needs to exist even in workers

        if (Browser.initted || ENVIRONMENT_IS_WORKER) return;
        Browser.initted = true;

        try {
          new Blob();
          Browser.hasBlobConstructor = true;
        } catch(e) {
          Browser.hasBlobConstructor = false;
          console.log("warning: no blob constructor, cannot create blobs with mimetypes");
        }
        Browser.BlobBuilder = typeof MozBlobBuilder != "undefined" ? MozBlobBuilder : (typeof WebKitBlobBuilder != "undefined" ? WebKitBlobBuilder : (!Browser.hasBlobConstructor ? console.log("warning: no BlobBuilder") : null));
        Browser.URLObject = typeof window != "undefined" ? (window.URL ? window.URL : window.webkitURL) : undefined;
        if (!Module.noImageDecoding && typeof Browser.URLObject === 'undefined') {
          console.log("warning: Browser does not support creating object URLs. Built-in browser image decoding will not be available.");
          Module.noImageDecoding = true;
        }

        // Support for plugins that can process preloaded files. You can add more of these to
        // your app by creating and appending to Module.preloadPlugins.
        //
        // Each plugin is asked if it can handle a file based on the file's name. If it can,
        // it is given the file's raw data. When it is done, it calls a callback with the file's
        // (possibly modified) data. For example, a plugin might decompress a file, or it
        // might create some side data structure for use later (like an Image element, etc.).

        var imagePlugin = {};
        imagePlugin['canHandle'] = function imagePlugin_canHandle(name) {
          return !Module.noImageDecoding && /\.(jpg|jpeg|png|bmp)$/i.test(name);
        };
        imagePlugin['handle'] = function imagePlugin_handle(byteArray, name, onload, onerror) {
          var b = null;
          if (Browser.hasBlobConstructor) {
            try {
              b = new Blob([byteArray], { type: Browser.getMimetype(name) });
              if (b.size !== byteArray.length) { // Safari bug #118630
                // Safari's Blob can only take an ArrayBuffer
                b = new Blob([(new Uint8Array(byteArray)).buffer], { type: Browser.getMimetype(name) });
              }
            } catch(e) {
              Runtime.warnOnce('Blob constructor present but fails: ' + e + '; falling back to blob builder');
            }
          }
          if (!b) {
            var bb = new Browser.BlobBuilder();
            bb.append((new Uint8Array(byteArray)).buffer); // we need to pass a buffer, and must copy the array to get the right data range
            b = bb.getBlob();
          }
          var url = Browser.URLObject.createObjectURL(b);
          var img = new Image();
          img.onload = function img_onload() {
            assert(img.complete, 'Image ' + name + ' could not be decoded');
            var canvas = document.createElement('canvas');
            canvas.width = img.width;
            canvas.height = img.height;
            var ctx = canvas.getContext('2d');
            ctx.drawImage(img, 0, 0);
            Module["preloadedImages"][name] = canvas;
            Browser.URLObject.revokeObjectURL(url);
            if (onload) onload(byteArray);
          };
          img.onerror = function img_onerror(event) {
            console.log('Image ' + url + ' could not be decoded');
            if (onerror) onerror();
          };
          img.src = url;
        };
        Module['preloadPlugins'].push(imagePlugin);

        var audioPlugin = {};
        audioPlugin['canHandle'] = function audioPlugin_canHandle(name) {
          return !Module.noAudioDecoding && name.substr(-4) in { '.ogg': 1, '.wav': 1, '.mp3': 1 };
        };
        audioPlugin['handle'] = function audioPlugin_handle(byteArray, name, onload, onerror) {
          var done = false;
          function finish(audio) {
            if (done) return;
            done = true;
            Module["preloadedAudios"][name] = audio;
            if (onload) onload(byteArray);
          }
          function fail() {
            if (done) return;
            done = true;
            Module["preloadedAudios"][name] = new Audio(); // empty shim
            if (onerror) onerror();
          }
          if (Browser.hasBlobConstructor) {
            try {
              var b = new Blob([byteArray], { type: Browser.getMimetype(name) });
            } catch(e) {
              return fail();
            }
            var url = Browser.URLObject.createObjectURL(b); // XXX we never revoke this!
            var audio = new Audio();
            audio.addEventListener('canplaythrough', function() { finish(audio) }, false); // use addEventListener due to chromium bug 124926
            audio.onerror = function audio_onerror(event) {
              if (done) return;
              console.log('warning: browser could not fully decode audio ' + name + ', trying slower base64 approach');
              function encode64(data) {
                var BASE = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
                var PAD = '=';
                var ret = '';
                var leftchar = 0;
                var leftbits = 0;
                for (var i = 0; i < data.length; i++) {
                  leftchar = (leftchar << 8) | data[i];
                  leftbits += 8;
                  while (leftbits >= 6) {
                    var curr = (leftchar >> (leftbits-6)) & 0x3f;
                    leftbits -= 6;
                    ret += BASE[curr];
                  }
                }
                if (leftbits == 2) {
                  ret += BASE[(leftchar&3) << 4];
                  ret += PAD + PAD;
                } else if (leftbits == 4) {
                  ret += BASE[(leftchar&0xf) << 2];
                  ret += PAD;
                }
                return ret;
              }
              audio.src = 'data:audio/x-' + name.substr(-3) + ';base64,' + encode64(byteArray);
              finish(audio); // we don't wait for confirmation this worked - but it's worth trying
            };
            audio.src = url;
            // workaround for chrome bug 124926 - we do not always get oncanplaythrough or onerror
            Browser.safeSetTimeout(function() {
              finish(audio); // try to use it even though it is not necessarily ready to play
            }, 10000);
          } else {
            return fail();
          }
        };
        Module['preloadPlugins'].push(audioPlugin);

        // Canvas event setup

        var canvas = Module['canvas'];

        // forced aspect ratio can be enabled by defining 'forcedAspectRatio' on Module
        // Module['forcedAspectRatio'] = 4 / 3;

        canvas.requestPointerLock = canvas['requestPointerLock'] ||
                                    canvas['mozRequestPointerLock'] ||
                                    canvas['webkitRequestPointerLock'] ||
                                    canvas['msRequestPointerLock'] ||
                                    function(){};
        canvas.exitPointerLock = document['exitPointerLock'] ||
                                 document['mozExitPointerLock'] ||
                                 document['webkitExitPointerLock'] ||
                                 document['msExitPointerLock'] ||
                                 function(){}; // no-op if function does not exist
        canvas.exitPointerLock = canvas.exitPointerLock.bind(document);

        function pointerLockChange() {
          Browser.pointerLock = document['pointerLockElement'] === canvas ||
                                document['mozPointerLockElement'] === canvas ||
                                document['webkitPointerLockElement'] === canvas ||
                                document['msPointerLockElement'] === canvas;
        }

        document.addEventListener('pointerlockchange', pointerLockChange, false);
        document.addEventListener('mozpointerlockchange', pointerLockChange, false);
        document.addEventListener('webkitpointerlockchange', pointerLockChange, false);
        document.addEventListener('mspointerlockchange', pointerLockChange, false);

        if (Module['elementPointerLock']) {
          canvas.addEventListener("click", function(ev) {
            if (!Browser.pointerLock && canvas.requestPointerLock) {
              canvas.requestPointerLock();
              ev.preventDefault();
            }
          }, false);
        }
      },createContext:function (canvas, useWebGL, setInModule, webGLContextAttributes) {
        var ctx;
        var errorInfo = '?';
        function onContextCreationError(event) {
          errorInfo = event.statusMessage || errorInfo;
        }
        try {
          if (useWebGL) {
            var contextAttributes = {
              antialias: false,
              alpha: false
            };

            if (webGLContextAttributes) {
              for (var attribute in webGLContextAttributes) {
                contextAttributes[attribute] = webGLContextAttributes[attribute];
              }
            }


            canvas.addEventListener('webglcontextcreationerror', onContextCreationError, false);
            try {
              ['experimental-webgl', 'webgl'].some(function(webglId) {
                return ctx = canvas.getContext(webglId, contextAttributes);
              });
            } finally {
              canvas.removeEventListener('webglcontextcreationerror', onContextCreationError, false);
            }
          } else {
            ctx = canvas.getContext('2d');
          }
          if (!ctx) throw ':(';
        } catch (e) {
          Module.print('Could not create canvas: ' + [errorInfo, e]);
          return null;
        }
        if (useWebGL) {
          // Set the background of the WebGL canvas to black
          canvas.style.backgroundColor = "black";

          // Warn on context loss
          canvas.addEventListener('webglcontextlost', function(event) {
            alert('WebGL context lost. You will need to reload the page.');
          }, false);
        }
        if (setInModule) {
          GLctx = Module.ctx = ctx;
          Module.useWebGL = useWebGL;
          Browser.moduleContextCreatedCallbacks.forEach(function(callback) { callback() });
          Browser.init();
        }
        return ctx;
      },destroyContext:function (canvas, useWebGL, setInModule) {},fullScreenHandlersInstalled:false,lockPointer:undefined,resizeCanvas:undefined,requestFullScreen:function (lockPointer, resizeCanvas) {
        Browser.lockPointer = lockPointer;
        Browser.resizeCanvas = resizeCanvas;
        if (typeof Browser.lockPointer === 'undefined') Browser.lockPointer = true;
        if (typeof Browser.resizeCanvas === 'undefined') Browser.resizeCanvas = false;

        var canvas = Module['canvas'];
        function fullScreenChange() {
          Browser.isFullScreen = false;
          var canvasContainer = canvas.parentNode;
          if ((document['webkitFullScreenElement'] || document['webkitFullscreenElement'] ||
               document['mozFullScreenElement'] || document['mozFullscreenElement'] ||
               document['fullScreenElement'] || document['fullscreenElement'] ||
               document['msFullScreenElement'] || document['msFullscreenElement'] ||
               document['webkitCurrentFullScreenElement']) === canvasContainer) {
            canvas.cancelFullScreen = document['cancelFullScreen'] ||
                                      document['mozCancelFullScreen'] ||
                                      document['webkitCancelFullScreen'] ||
                                      document['msExitFullscreen'] ||
                                      document['exitFullscreen'] ||
                                      function() {};
            canvas.cancelFullScreen = canvas.cancelFullScreen.bind(document);
            if (Browser.lockPointer) canvas.requestPointerLock();
            Browser.isFullScreen = true;
            if (Browser.resizeCanvas) Browser.setFullScreenCanvasSize();
          } else {

            // remove the full screen specific parent of the canvas again to restore the HTML structure from before going full screen
            canvasContainer.parentNode.insertBefore(canvas, canvasContainer);
            canvasContainer.parentNode.removeChild(canvasContainer);

            if (Browser.resizeCanvas) Browser.setWindowedCanvasSize();
          }
          if (Module['onFullScreen']) Module['onFullScreen'](Browser.isFullScreen);
          Browser.updateCanvasDimensions(canvas);
        }

        if (!Browser.fullScreenHandlersInstalled) {
          Browser.fullScreenHandlersInstalled = true;
          document.addEventListener('fullscreenchange', fullScreenChange, false);
          document.addEventListener('mozfullscreenchange', fullScreenChange, false);
          document.addEventListener('webkitfullscreenchange', fullScreenChange, false);
          document.addEventListener('MSFullscreenChange', fullScreenChange, false);
        }

        // create a new parent to ensure the canvas has no siblings. this allows browsers to optimize full screen performance when its parent is the full screen root
        var canvasContainer = document.createElement("div");
        canvas.parentNode.insertBefore(canvasContainer, canvas);
        canvasContainer.appendChild(canvas);

        // use parent of canvas as full screen root to allow aspect ratio correction (Firefox stretches the root to screen size)
        canvasContainer.requestFullScreen = canvasContainer['requestFullScreen'] ||
                                            canvasContainer['mozRequestFullScreen'] ||
                                            canvasContainer['msRequestFullscreen'] ||
                                           (canvasContainer['webkitRequestFullScreen'] ? function() { canvasContainer['webkitRequestFullScreen'](Element['ALLOW_KEYBOARD_INPUT']) } : null);
        canvasContainer.requestFullScreen();
      },requestAnimationFrame:function requestAnimationFrame(func) {
        if (typeof window === 'undefined') { // Provide fallback to setTimeout if window is undefined (e.g. in Node.js)
          setTimeout(func, 1000/60);
        } else {
          if (!window.requestAnimationFrame) {
            window.requestAnimationFrame = window['requestAnimationFrame'] ||
                                           window['mozRequestAnimationFrame'] ||
                                           window['webkitRequestAnimationFrame'] ||
                                           window['msRequestAnimationFrame'] ||
                                           window['oRequestAnimationFrame'] ||
                                           window['setTimeout'];
          }
          window.requestAnimationFrame(func);
        }
      },safeCallback:function (func) {
        return function() {
          if (!ABORT) return func.apply(null, arguments);
        };
      },safeRequestAnimationFrame:function (func) {
        return Browser.requestAnimationFrame(function() {
          if (!ABORT) func();
        });
      },safeSetTimeout:function (func, timeout) {
        return setTimeout(function() {
          if (!ABORT) func();
        }, timeout);
      },safeSetInterval:function (func, timeout) {
        return setInterval(function() {
          if (!ABORT) func();
        }, timeout);
      },getMimetype:function (name) {
        return {
          'jpg': 'image/jpeg',
          'jpeg': 'image/jpeg',
          'png': 'image/png',
          'bmp': 'image/bmp',
          'ogg': 'audio/ogg',
          'wav': 'audio/wav',
          'mp3': 'audio/mpeg'
        }[name.substr(name.lastIndexOf('.')+1)];
      },getUserMedia:function (func) {
        if(!window.getUserMedia) {
          window.getUserMedia = navigator['getUserMedia'] ||
                                navigator['mozGetUserMedia'];
        }
        window.getUserMedia(func);
      },getMovementX:function (event) {
        return event['movementX'] ||
               event['mozMovementX'] ||
               event['webkitMovementX'] ||
               0;
      },getMovementY:function (event) {
        return event['movementY'] ||
               event['mozMovementY'] ||
               event['webkitMovementY'] ||
               0;
      },getMouseWheelDelta:function (event) {
        return Math.max(-1, Math.min(1, event.type === 'DOMMouseScroll' ? event.detail : -event.wheelDelta));
      },mouseX:0,mouseY:0,mouseMovementX:0,mouseMovementY:0,calculateMouseEvent:function (event) { // event should be mousemove, mousedown or mouseup
        if (Browser.pointerLock) {
          // When the pointer is locked, calculate the coordinates
          // based on the movement of the mouse.
          // Workaround for Firefox bug 764498
          if (event.type != 'mousemove' &&
              ('mozMovementX' in event)) {
            Browser.mouseMovementX = Browser.mouseMovementY = 0;
          } else {
            Browser.mouseMovementX = Browser.getMovementX(event);
            Browser.mouseMovementY = Browser.getMovementY(event);
          }

          // check if SDL is available
          if (typeof SDL != "undefined") {
    Browser.mouseX = SDL.mouseX + Browser.mouseMovementX;
    Browser.mouseY = SDL.mouseY + Browser.mouseMovementY;
          } else {
    // just add the mouse delta to the current absolut mouse position
    // FIXME: ideally this should be clamped against the canvas size and zero
    Browser.mouseX += Browser.mouseMovementX;
    Browser.mouseY += Browser.mouseMovementY;
          }
        } else {
          // Otherwise, calculate the movement based on the changes
          // in the coordinates.
          var rect = Module["canvas"].getBoundingClientRect();
          var x, y;

          // Neither .scrollX or .pageXOffset are defined in a spec, but
          // we prefer .scrollX because it is currently in a spec draft.
          // (see: http://www.w3.org/TR/2013/WD-cssom-view-20131217/)
          var scrollX = ((typeof window.scrollX !== 'undefined') ? window.scrollX : window.pageXOffset);
          var scrollY = ((typeof window.scrollY !== 'undefined') ? window.scrollY : window.pageYOffset);
          if (event.type == 'touchstart' ||
              event.type == 'touchend' ||
              event.type == 'touchmove') {
            var t = event.touches.item(0);
            if (t) {
              x = t.pageX - (scrollX + rect.left);
              y = t.pageY - (scrollY + rect.top);
            } else {
              return;
            }
          } else {
            x = event.pageX - (scrollX + rect.left);
            y = event.pageY - (scrollY + rect.top);
          }

          // the canvas might be CSS-scaled compared to its backbuffer;
          // SDL-using content will want mouse coordinates in terms
          // of backbuffer units.
          var cw = Module["canvas"].width;
          var ch = Module["canvas"].height;
          x = x * (cw / rect.width);
          y = y * (ch / rect.height);

          Browser.mouseMovementX = x - Browser.mouseX;
          Browser.mouseMovementY = y - Browser.mouseY;
          Browser.mouseX = x;
          Browser.mouseY = y;
        }
      },xhrLoad:function (url, onload, onerror) {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', url, true);
        xhr.responseType = 'arraybuffer';
        xhr.onload = function xhr_onload() {
          if (xhr.status == 200 || (xhr.status == 0 && xhr.response)) { // file URLs can return 0
            onload(xhr.response);
          } else {
            onerror();
          }
        };
        xhr.onerror = onerror;
        xhr.send(null);
      },asyncLoad:function (url, onload, onerror, noRunDep) {
        Browser.xhrLoad(url, function(arrayBuffer) {
          assert(arrayBuffer, 'Loading data file "' + url + '" failed (no arrayBuffer).');
          onload(new Uint8Array(arrayBuffer));
          if (!noRunDep) removeRunDependency('al ' + url);
        }, function(event) {
          if (onerror) {
            onerror();
          } else {
            throw 'Loading data file "' + url + '" failed.';
          }
        });
        if (!noRunDep) addRunDependency('al ' + url);
      },resizeListeners:[],updateResizeListeners:function () {
        var canvas = Module['canvas'];
        Browser.resizeListeners.forEach(function(listener) {
          listener(canvas.width, canvas.height);
        });
      },setCanvasSize:function (width, height, noUpdates) {
        var canvas = Module['canvas'];
        Browser.updateCanvasDimensions(canvas, width, height);
        if (!noUpdates) Browser.updateResizeListeners();
      },windowedWidth:0,windowedHeight:0,setFullScreenCanvasSize:function () {
        // check if SDL is available
        if (typeof SDL != "undefined") {
    var flags = HEAPU32[((SDL.screen+Runtime.QUANTUM_SIZE*0)>>2)];
    flags = flags | 0x00800000; // set SDL_FULLSCREEN flag
    HEAP32[((SDL.screen+Runtime.QUANTUM_SIZE*0)>>2)]=flags
        }
        Browser.updateResizeListeners();
      },setWindowedCanvasSize:function () {
        // check if SDL is available
        if (typeof SDL != "undefined") {
    var flags = HEAPU32[((SDL.screen+Runtime.QUANTUM_SIZE*0)>>2)];
    flags = flags & ~0x00800000; // clear SDL_FULLSCREEN flag
    HEAP32[((SDL.screen+Runtime.QUANTUM_SIZE*0)>>2)]=flags
        }
        Browser.updateResizeListeners();
      },updateCanvasDimensions:function (canvas, wNative, hNative) {
        if (wNative && hNative) {
          canvas.widthNative = wNative;
          canvas.heightNative = hNative;
        } else {
          wNative = canvas.widthNative;
          hNative = canvas.heightNative;
        }
        var w = wNative;
        var h = hNative;
        if (Module['forcedAspectRatio'] && Module['forcedAspectRatio'] > 0) {
          if (w/h < Module['forcedAspectRatio']) {
            w = Math.round(h * Module['forcedAspectRatio']);
          } else {
            h = Math.round(w / Module['forcedAspectRatio']);
          }
        }
        if (((document['webkitFullScreenElement'] || document['webkitFullscreenElement'] ||
             document['mozFullScreenElement'] || document['mozFullscreenElement'] ||
             document['fullScreenElement'] || document['fullscreenElement'] ||
             document['msFullScreenElement'] || document['msFullscreenElement'] ||
             document['webkitCurrentFullScreenElement']) === canvas.parentNode) && (typeof screen != 'undefined')) {
           var factor = Math.min(screen.width / w, screen.height / h);
           w = Math.round(w * factor);
           h = Math.round(h * factor);
        }
        if (Browser.resizeCanvas) {
          if (canvas.width  != w) canvas.width  = w;
          if (canvas.height != h) canvas.height = h;
          if (typeof canvas.style != 'undefined') {
            canvas.style.removeProperty( "width");
            canvas.style.removeProperty("height");
          }
        } else {
          if (canvas.width  != wNative) canvas.width  = wNative;
          if (canvas.height != hNative) canvas.height = hNative;
          if (typeof canvas.style != 'undefined') {
            if (w != wNative || h != hNative) {
              canvas.style.setProperty( "width", w + "px", "important");
              canvas.style.setProperty("height", h + "px", "important");
            } else {
              canvas.style.removeProperty( "width");
              canvas.style.removeProperty("height");
            }
          }
        }
      }};

  function _sbrk(bytes) {
      // Implement a Linux-like 'memory area' for our 'process'.
      // Changes the size of the memory area by |bytes|; returns the
      // address of the previous top ('break') of the memory area
      // We control the "dynamic" memory - DYNAMIC_BASE to DYNAMICTOP
      var self = _sbrk;
      if (!self.called) {
        DYNAMICTOP = alignMemoryPage(DYNAMICTOP); // make sure we start out aligned
        self.called = true;
        assert(Runtime.dynamicAlloc);
        self.alloc = Runtime.dynamicAlloc;
        Runtime.dynamicAlloc = function() { abort('cannot dynamically allocate, sbrk now has control') };
      }
      var ret = DYNAMICTOP;
      if (bytes != 0) self.alloc(bytes);
      return ret;  // Previous break location.
    }

  function ___assert_fail(condition, filename, line, func) {
      ABORT = true;
      throw 'Assertion failed: ' + Pointer_stringify(condition) + ', at: ' + [filename ? Pointer_stringify(filename) : 'unknown filename', line, func ? Pointer_stringify(func) : 'unknown function'] + ' at ' + stackTrace();
    }

  function _time(ptr) {
      var ret = Math.floor(Date.now()/1000);
      if (ptr) {
        HEAP32[((ptr)>>2)]=ret;
      }
      return ret;
    }

  function _llvm_bswap_i32(x) {
      return ((x&0xff)<<24) | (((x>>8)&0xff)<<16) | (((x>>16)&0xff)<<8) | (x>>>24);
    }



  function _emscripten_memcpy_big(dest, src, num) {
      HEAPU8.set(HEAPU8.subarray(src, src+num), dest);
      return dest;
    }
  Module["_memcpy"] = _memcpy;
FS.staticInit();__ATINIT__.unshift({ func: function() { if (!Module["noFSInit"] && !FS.init.initialized) FS.init() } });__ATMAIN__.push({ func: function() { FS.ignorePermissions = false } });__ATEXIT__.push({ func: function() { FS.quit() } });Module["FS_createFolder"] = FS.createFolder;Module["FS_createPath"] = FS.createPath;Module["FS_createDataFile"] = FS.createDataFile;Module["FS_createPreloadedFile"] = FS.createPreloadedFile;Module["FS_createLazyFile"] = FS.createLazyFile;Module["FS_createLink"] = FS.createLink;Module["FS_createDevice"] = FS.createDevice;
___errno_state = Runtime.staticAlloc(4); HEAP32[((___errno_state)>>2)]=0;
__ATINIT__.unshift({ func: function() { TTY.init() } });__ATEXIT__.push({ func: function() { TTY.shutdown() } });TTY.utf8 = new Runtime.UTF8Processor();
if (ENVIRONMENT_IS_NODE) { var fs = require("fs"); NODEFS.staticInit(); }
__ATINIT__.push({ func: function() { SOCKFS.root = FS.mount(SOCKFS, {}, null); } });
_fputc.ret = allocate([0], "i8", ALLOC_STATIC);
Module["requestFullScreen"] = function Module_requestFullScreen(lockPointer, resizeCanvas) { Browser.requestFullScreen(lockPointer, resizeCanvas) };
  Module["requestAnimationFrame"] = function Module_requestAnimationFrame(func) { Browser.requestAnimationFrame(func) };
  Module["setCanvasSize"] = function Module_setCanvasSize(width, height, noUpdates) { Browser.setCanvasSize(width, height, noUpdates) };
  Module["pauseMainLoop"] = function Module_pauseMainLoop() { Browser.mainLoop.pause() };
  Module["resumeMainLoop"] = function Module_resumeMainLoop() { Browser.mainLoop.resume() };
  Module["getUserMedia"] = function Module_getUserMedia() { Browser.getUserMedia() }
STACK_BASE = STACKTOP = Runtime.alignMemory(STATICTOP);

staticSealed = true; // seal the static portion of memory

STACK_MAX = STACK_BASE + 5242880;

DYNAMIC_BASE = DYNAMICTOP = Runtime.alignMemory(STACK_MAX);

assert(DYNAMIC_BASE < TOTAL_MEMORY, "TOTAL_MEMORY not big enough for stack");


var Math_min = Math.min;
function invoke_iiii(index,a1,a2,a3) {
  try {
    return Module["dynCall_iiii"](index,a1,a2,a3);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}

function invoke_vii(index,a1,a2) {
  try {
    Module["dynCall_vii"](index,a1,a2);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}

function invoke_iii(index,a1,a2) {
  try {
    return Module["dynCall_iii"](index,a1,a2);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}

function asmPrintInt(x, y) {
  Module.print('int ' + x + ',' + y);// + ' ' + new Error().stack);
}
function asmPrintFloat(x, y) {
  Module.print('float ' + x + ',' + y);// + ' ' + new Error().stack);
}
// EMSCRIPTEN_START_ASM
var asm = (function(global, env, buffer) {
  'use asm';
  var HEAP8 = new global.Int8Array(buffer);
  var HEAP16 = new global.Int16Array(buffer);
  var HEAP32 = new global.Int32Array(buffer);
  var HEAPU8 = new global.Uint8Array(buffer);
  var HEAPU16 = new global.Uint16Array(buffer);
  var HEAPU32 = new global.Uint32Array(buffer);
  var HEAPF32 = new global.Float32Array(buffer);
  var HEAPF64 = new global.Float64Array(buffer);

  var STACKTOP=env.STACKTOP|0;
  var STACK_MAX=env.STACK_MAX|0;
  var tempDoublePtr=env.tempDoublePtr|0;
  var ABORT=env.ABORT|0;

  var __THREW__ = 0;
  var threwValue = 0;
  var setjmpId = 0;
  var undef = 0;
  var nan = +env.NaN, inf = +env.Infinity;
  var tempInt = 0, tempBigInt = 0, tempBigIntP = 0, tempBigIntS = 0, tempBigIntR = 0.0, tempBigIntI = 0, tempBigIntD = 0, tempValue = 0, tempDouble = 0.0;

  var tempRet0 = 0;
  var tempRet1 = 0;
  var tempRet2 = 0;
  var tempRet3 = 0;
  var tempRet4 = 0;
  var tempRet5 = 0;
  var tempRet6 = 0;
  var tempRet7 = 0;
  var tempRet8 = 0;
  var tempRet9 = 0;
  var Math_floor=global.Math.floor;
  var Math_abs=global.Math.abs;
  var Math_sqrt=global.Math.sqrt;
  var Math_pow=global.Math.pow;
  var Math_cos=global.Math.cos;
  var Math_sin=global.Math.sin;
  var Math_tan=global.Math.tan;
  var Math_acos=global.Math.acos;
  var Math_asin=global.Math.asin;
  var Math_atan=global.Math.atan;
  var Math_atan2=global.Math.atan2;
  var Math_exp=global.Math.exp;
  var Math_log=global.Math.log;
  var Math_ceil=global.Math.ceil;
  var Math_imul=global.Math.imul;
  var abort=env.abort;
  var assert=env.assert;
  var asmPrintInt=env.asmPrintInt;
  var asmPrintFloat=env.asmPrintFloat;
  var Math_min=env.min;
  var invoke_iiii=env.invoke_iiii;
  var invoke_vii=env.invoke_vii;
  var invoke_iii=env.invoke_iii;
  var _send=env._send;
  var ___setErrNo=env.___setErrNo;
  var ___assert_fail=env.___assert_fail;
  var _fflush=env._fflush;
  var _pwrite=env._pwrite;
  var __reallyNegative=env.__reallyNegative;
  var _sbrk=env._sbrk;
  var ___errno_location=env.___errno_location;
  var _emscripten_memcpy_big=env._emscripten_memcpy_big;
  var _fileno=env._fileno;
  var _sysconf=env._sysconf;
  var _puts=env._puts;
  var _mkport=env._mkport;
  var _write=env._write;
  var _llvm_bswap_i32=env._llvm_bswap_i32;
  var _fputc=env._fputc;
  var _abort=env._abort;
  var _fwrite=env._fwrite;
  var _time=env._time;
  var _fprintf=env._fprintf;
  var __formatString=env.__formatString;
  var _fputs=env._fputs;
  var _printf=env._printf;
  var tempFloat = 0.0;

// EMSCRIPTEN_START_FUNCS
function _inflate(i2, i3) {
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i1 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, i38 = 0, i39 = 0, i40 = 0, i41 = 0, i42 = 0, i43 = 0, i44 = 0, i45 = 0, i46 = 0, i47 = 0, i48 = 0, i49 = 0, i50 = 0, i51 = 0, i52 = 0, i53 = 0, i54 = 0, i55 = 0, i56 = 0, i57 = 0, i58 = 0, i59 = 0, i60 = 0, i61 = 0, i62 = 0, i63 = 0, i64 = 0, i65 = 0, i66 = 0, i67 = 0, i68 = 0, i69 = 0, i70 = 0, i71 = 0, i72 = 0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 i25 = i1;
 if ((i2 | 0) == 0) {
  i72 = -2;
  STACKTOP = i1;
  return i72 | 0;
 }
 i4 = HEAP32[i2 + 28 >> 2] | 0;
 if ((i4 | 0) == 0) {
  i72 = -2;
  STACKTOP = i1;
  return i72 | 0;
 }
 i8 = i2 + 12 | 0;
 i19 = HEAP32[i8 >> 2] | 0;
 if ((i19 | 0) == 0) {
  i72 = -2;
  STACKTOP = i1;
  return i72 | 0;
 }
 i62 = HEAP32[i2 >> 2] | 0;
 if ((i62 | 0) == 0 ? (HEAP32[i2 + 4 >> 2] | 0) != 0 : 0) {
  i72 = -2;
  STACKTOP = i1;
  return i72 | 0;
 }
 i68 = HEAP32[i4 >> 2] | 0;
 if ((i68 | 0) == 11) {
  HEAP32[i4 >> 2] = 12;
  i68 = 12;
  i62 = HEAP32[i2 >> 2] | 0;
  i19 = HEAP32[i8 >> 2] | 0;
 }
 i15 = i2 + 16 | 0;
 i59 = HEAP32[i15 >> 2] | 0;
 i16 = i2 + 4 | 0;
 i5 = HEAP32[i16 >> 2] | 0;
 i17 = i4 + 56 | 0;
 i6 = i4 + 60 | 0;
 i12 = i4 + 8 | 0;
 i10 = i4 + 24 | 0;
 i39 = i25 + 1 | 0;
 i11 = i4 + 16 | 0;
 i38 = i4 + 32 | 0;
 i35 = i2 + 24 | 0;
 i40 = i4 + 36 | 0;
 i41 = i4 + 20 | 0;
 i9 = i2 + 48 | 0;
 i42 = i4 + 64 | 0;
 i46 = i4 + 12 | 0;
 i47 = (i3 + -5 | 0) >>> 0 < 2;
 i7 = i4 + 4 | 0;
 i48 = i4 + 76 | 0;
 i49 = i4 + 84 | 0;
 i50 = i4 + 80 | 0;
 i51 = i4 + 88 | 0;
 i43 = (i3 | 0) == 6;
 i57 = i4 + 7108 | 0;
 i37 = i4 + 72 | 0;
 i58 = i4 + 7112 | 0;
 i54 = i4 + 68 | 0;
 i28 = i4 + 44 | 0;
 i29 = i4 + 7104 | 0;
 i30 = i4 + 48 | 0;
 i31 = i4 + 52 | 0;
 i18 = i4 + 40 | 0;
 i13 = i2 + 20 | 0;
 i14 = i4 + 28 | 0;
 i32 = i4 + 96 | 0;
 i33 = i4 + 100 | 0;
 i34 = i4 + 92 | 0;
 i36 = i4 + 104 | 0;
 i52 = i4 + 1328 | 0;
 i53 = i4 + 108 | 0;
 i27 = i4 + 112 | 0;
 i55 = i4 + 752 | 0;
 i56 = i4 + 624 | 0;
 i44 = i25 + 2 | 0;
 i45 = i25 + 3 | 0;
 i67 = HEAP32[i6 >> 2] | 0;
 i65 = i5;
 i64 = HEAP32[i17 >> 2] | 0;
 i26 = i59;
 i61 = 0;
 L17 : while (1) {
  L19 : do {
   switch (i68 | 0) {
   case 16:
    {
     if (i67 >>> 0 < 14) {
      i63 = i67;
      while (1) {
       if ((i65 | 0) == 0) {
        i65 = 0;
        break L17;
       }
       i65 = i65 + -1 | 0;
       i66 = i62 + 1 | 0;
       i64 = (HEAPU8[i62] << i63) + i64 | 0;
       i63 = i63 + 8 | 0;
       if (i63 >>> 0 < 14) {
        i62 = i66;
       } else {
        i62 = i66;
        break;
       }
      }
     } else {
      i63 = i67;
     }
     i71 = (i64 & 31) + 257 | 0;
     HEAP32[i32 >> 2] = i71;
     i72 = (i64 >>> 5 & 31) + 1 | 0;
     HEAP32[i33 >> 2] = i72;
     HEAP32[i34 >> 2] = (i64 >>> 10 & 15) + 4;
     i64 = i64 >>> 14;
     i63 = i63 + -14 | 0;
     if (i71 >>> 0 > 286 | i72 >>> 0 > 30) {
      HEAP32[i35 >> 2] = 11616;
      HEAP32[i4 >> 2] = 29;
      i66 = i26;
      break L19;
     } else {
      HEAP32[i36 >> 2] = 0;
      HEAP32[i4 >> 2] = 17;
      i66 = 0;
      i60 = 154;
      break L19;
     }
    }
   case 2:
    {
     if (i67 >>> 0 < 32) {
      i63 = i67;
      i60 = 47;
     } else {
      i60 = 49;
     }
     break;
    }
   case 23:
    {
     i66 = HEAP32[i37 >> 2] | 0;
     i63 = i67;
     i60 = 240;
     break;
    }
   case 18:
    {
     i63 = HEAP32[i36 >> 2] | 0;
     i69 = i65;
     i60 = 164;
     break;
    }
   case 1:
    {
     if (i67 >>> 0 < 16) {
      i63 = i67;
      while (1) {
       if ((i65 | 0) == 0) {
        i65 = 0;
        break L17;
       }
       i65 = i65 + -1 | 0;
       i66 = i62 + 1 | 0;
       i64 = (HEAPU8[i62] << i63) + i64 | 0;
       i63 = i63 + 8 | 0;
       if (i63 >>> 0 < 16) {
        i62 = i66;
       } else {
        i62 = i66;
        break;
       }
      }
     } else {
      i63 = i67;
     }
     HEAP32[i11 >> 2] = i64;
     if ((i64 & 255 | 0) != 8) {
      HEAP32[i35 >> 2] = 11448;
      HEAP32[i4 >> 2] = 29;
      i66 = i26;
      break L19;
     }
     if ((i64 & 57344 | 0) != 0) {
      HEAP32[i35 >> 2] = 11504;
      HEAP32[i4 >> 2] = 29;
      i66 = i26;
      break L19;
     }
     i60 = HEAP32[i38 >> 2] | 0;
     if ((i60 | 0) == 0) {
      i60 = i64;
     } else {
      HEAP32[i60 >> 2] = i64 >>> 8 & 1;
      i60 = HEAP32[i11 >> 2] | 0;
     }
     if ((i60 & 512 | 0) != 0) {
      HEAP8[i25] = i64;
      HEAP8[i39] = i64 >>> 8;
      HEAP32[i10 >> 2] = _crc32(HEAP32[i10 >> 2] | 0, i25, 2) | 0;
     }
     HEAP32[i4 >> 2] = 2;
     i63 = 0;
     i64 = 0;
     i60 = 47;
     break;
    }
   case 8:
    {
     i63 = i67;
     i60 = 109;
     break;
    }
   case 22:
    {
     i63 = i67;
     i60 = 228;
     break;
    }
   case 24:
    {
     i63 = i67;
     i60 = 246;
     break;
    }
   case 19:
    {
     i63 = i67;
     i60 = 201;
     break;
    }
   case 20:
    {
     i63 = i67;
     i60 = 202;
     break;
    }
   case 21:
    {
     i66 = HEAP32[i37 >> 2] | 0;
     i63 = i67;
     i60 = 221;
     break;
    }
   case 10:
    {
     i63 = i67;
     i60 = 121;
     break;
    }
   case 11:
    {
     i63 = i67;
     i60 = 124;
     break;
    }
   case 12:
    {
     i63 = i67;
     i60 = 125;
     break;
    }
   case 5:
    {
     i63 = i67;
     i60 = 73;
     break;
    }
   case 4:
    {
     i63 = i67;
     i60 = 62;
     break;
    }
   case 0:
    {
     i66 = HEAP32[i12 >> 2] | 0;
     if ((i66 | 0) == 0) {
      HEAP32[i4 >> 2] = 12;
      i63 = i67;
      i66 = i26;
      break L19;
     }
     if (i67 >>> 0 < 16) {
      i63 = i67;
      while (1) {
       if ((i65 | 0) == 0) {
        i65 = 0;
        break L17;
       }
       i65 = i65 + -1 | 0;
       i67 = i62 + 1 | 0;
       i64 = (HEAPU8[i62] << i63) + i64 | 0;
       i63 = i63 + 8 | 0;
       if (i63 >>> 0 < 16) {
        i62 = i67;
       } else {
        i62 = i67;
        break;
       }
      }
     } else {
      i63 = i67;
     }
     if ((i66 & 2 | 0) != 0 & (i64 | 0) == 35615) {
      HEAP32[i10 >> 2] = _crc32(0, 0, 0) | 0;
      HEAP8[i25] = 31;
      HEAP8[i39] = -117;
      HEAP32[i10 >> 2] = _crc32(HEAP32[i10 >> 2] | 0, i25, 2) | 0;
      HEAP32[i4 >> 2] = 1;
      i63 = 0;
      i64 = 0;
      i66 = i26;
      break L19;
     }
     HEAP32[i11 >> 2] = 0;
     i67 = HEAP32[i38 >> 2] | 0;
     if ((i67 | 0) != 0) {
      HEAP32[i67 + 48 >> 2] = -1;
      i66 = HEAP32[i12 >> 2] | 0;
     }
     if ((i66 & 1 | 0) != 0 ? ((((i64 << 8 & 65280) + (i64 >>> 8) | 0) >>> 0) % 31 | 0 | 0) == 0 : 0) {
      if ((i64 & 15 | 0) != 8) {
       HEAP32[i35 >> 2] = 11448;
       HEAP32[i4 >> 2] = 29;
       i66 = i26;
       break L19;
      }
      i66 = i64 >>> 4;
      i63 = i63 + -4 | 0;
      i68 = (i66 & 15) + 8 | 0;
      i67 = HEAP32[i40 >> 2] | 0;
      if ((i67 | 0) != 0) {
       if (i68 >>> 0 > i67 >>> 0) {
        HEAP32[i35 >> 2] = 11480;
        HEAP32[i4 >> 2] = 29;
        i64 = i66;
        i66 = i26;
        break L19;
       }
      } else {
       HEAP32[i40 >> 2] = i68;
      }
      HEAP32[i41 >> 2] = 1 << i68;
      i63 = _adler32(0, 0, 0) | 0;
      HEAP32[i10 >> 2] = i63;
      HEAP32[i9 >> 2] = i63;
      HEAP32[i4 >> 2] = i64 >>> 12 & 2 ^ 11;
      i63 = 0;
      i64 = 0;
      i66 = i26;
      break L19;
     }
     HEAP32[i35 >> 2] = 11424;
     HEAP32[i4 >> 2] = 29;
     i66 = i26;
     break;
    }
   case 26:
    {
     if ((HEAP32[i12 >> 2] | 0) != 0) {
      if (i67 >>> 0 < 32) {
       i63 = i67;
       while (1) {
        if ((i65 | 0) == 0) {
         i65 = 0;
         break L17;
        }
        i65 = i65 + -1 | 0;
        i66 = i62 + 1 | 0;
        i64 = (HEAPU8[i62] << i63) + i64 | 0;
        i63 = i63 + 8 | 0;
        if (i63 >>> 0 < 32) {
         i62 = i66;
        } else {
         i62 = i66;
         break;
        }
       }
      } else {
       i63 = i67;
      }
      i66 = i59 - i26 | 0;
      HEAP32[i13 >> 2] = (HEAP32[i13 >> 2] | 0) + i66;
      HEAP32[i14 >> 2] = (HEAP32[i14 >> 2] | 0) + i66;
      if ((i59 | 0) != (i26 | 0)) {
       i59 = HEAP32[i10 >> 2] | 0;
       i67 = i19 + (0 - i66) | 0;
       if ((HEAP32[i11 >> 2] | 0) == 0) {
        i59 = _adler32(i59, i67, i66) | 0;
       } else {
        i59 = _crc32(i59, i67, i66) | 0;
       }
       HEAP32[i10 >> 2] = i59;
       HEAP32[i9 >> 2] = i59;
      }
      if ((HEAP32[i11 >> 2] | 0) == 0) {
       i59 = _llvm_bswap_i32(i64 | 0) | 0;
      } else {
       i59 = i64;
      }
      if ((i59 | 0) == (HEAP32[i10 >> 2] | 0)) {
       i63 = 0;
       i64 = 0;
       i59 = i26;
      } else {
       HEAP32[i35 >> 2] = 11904;
       HEAP32[i4 >> 2] = 29;
       i66 = i26;
       i59 = i26;
       break L19;
      }
     } else {
      i63 = i67;
     }
     HEAP32[i4 >> 2] = 27;
     i60 = 277;
     break;
    }
   case 27:
    {
     i63 = i67;
     i60 = 277;
     break;
    }
   case 28:
    {
     i63 = i67;
     i61 = 1;
     i60 = 285;
     break L17;
    }
   case 29:
    {
     i63 = i67;
     i61 = -3;
     break L17;
    }
   case 25:
    {
     if ((i26 | 0) == 0) {
      i63 = i67;
      i26 = 0;
      i60 = 285;
      break L17;
     }
     HEAP8[i19] = HEAP32[i42 >> 2];
     HEAP32[i4 >> 2] = 20;
     i63 = i67;
     i66 = i26 + -1 | 0;
     i19 = i19 + 1 | 0;
     break;
    }
   case 17:
    {
     i66 = HEAP32[i36 >> 2] | 0;
     if (i66 >>> 0 < (HEAP32[i34 >> 2] | 0) >>> 0) {
      i63 = i67;
      i60 = 154;
     } else {
      i60 = 158;
     }
     break;
    }
   case 13:
    {
     i63 = i67 & 7;
     i64 = i64 >>> i63;
     i63 = i67 - i63 | 0;
     if (i63 >>> 0 < 32) {
      while (1) {
       if ((i65 | 0) == 0) {
        i65 = 0;
        break L17;
       }
       i65 = i65 + -1 | 0;
       i66 = i62 + 1 | 0;
       i64 = (HEAPU8[i62] << i63) + i64 | 0;
       i63 = i63 + 8 | 0;
       if (i63 >>> 0 < 32) {
        i62 = i66;
       } else {
        i62 = i66;
        break;
       }
      }
     }
     i66 = i64 & 65535;
     if ((i66 | 0) == (i64 >>> 16 ^ 65535 | 0)) {
      HEAP32[i42 >> 2] = i66;
      HEAP32[i4 >> 2] = 14;
      if (i43) {
       i63 = 0;
       i64 = 0;
       i60 = 285;
       break L17;
      } else {
       i63 = 0;
       i64 = 0;
       i60 = 143;
       break L19;
      }
     } else {
      HEAP32[i35 >> 2] = 11584;
      HEAP32[i4 >> 2] = 29;
      i66 = i26;
      break L19;
     }
    }
   case 7:
    {
     i63 = i67;
     i60 = 96;
     break;
    }
   case 14:
    {
     i63 = i67;
     i60 = 143;
     break;
    }
   case 15:
    {
     i63 = i67;
     i60 = 144;
     break;
    }
   case 9:
    {
     if (i67 >>> 0 < 32) {
      i63 = i67;
      while (1) {
       if ((i65 | 0) == 0) {
        i65 = 0;
        break L17;
       }
       i65 = i65 + -1 | 0;
       i66 = i62 + 1 | 0;
       i64 = (HEAPU8[i62] << i63) + i64 | 0;
       i63 = i63 + 8 | 0;
       if (i63 >>> 0 < 32) {
        i62 = i66;
       } else {
        i62 = i66;
        break;
       }
      }
     }
     i63 = _llvm_bswap_i32(i64 | 0) | 0;
     HEAP32[i10 >> 2] = i63;
     HEAP32[i9 >> 2] = i63;
     HEAP32[i4 >> 2] = 10;
     i63 = 0;
     i64 = 0;
     i60 = 121;
     break;
    }
   case 30:
    {
     i60 = 299;
     break L17;
    }
   case 6:
    {
     i63 = i67;
     i60 = 83;
     break;
    }
   case 3:
    {
     if (i67 >>> 0 < 16) {
      i63 = i67;
      i66 = i62;
      i60 = 55;
     } else {
      i60 = 57;
     }
     break;
    }
   default:
    {
     i2 = -2;
     i60 = 300;
     break L17;
    }
   }
  } while (0);
  if ((i60 | 0) == 47) {
   while (1) {
    i60 = 0;
    if ((i65 | 0) == 0) {
     i65 = 0;
     break L17;
    }
    i65 = i65 + -1 | 0;
    i60 = i62 + 1 | 0;
    i64 = (HEAPU8[i62] << i63) + i64 | 0;
    i63 = i63 + 8 | 0;
    if (i63 >>> 0 < 32) {
     i62 = i60;
     i60 = 47;
    } else {
     i62 = i60;
     i60 = 49;
     break;
    }
   }
  } else if ((i60 | 0) == 121) {
   if ((HEAP32[i46 >> 2] | 0) == 0) {
    i60 = 122;
    break;
   }
   i60 = _adler32(0, 0, 0) | 0;
   HEAP32[i10 >> 2] = i60;
   HEAP32[i9 >> 2] = i60;
   HEAP32[i4 >> 2] = 11;
   i60 = 124;
  } else if ((i60 | 0) == 143) {
   HEAP32[i4 >> 2] = 15;
   i60 = 144;
  } else if ((i60 | 0) == 154) {
   while (1) {
    i60 = 0;
    if (i63 >>> 0 < 3) {
     while (1) {
      if ((i65 | 0) == 0) {
       i65 = 0;
       break L17;
      }
      i65 = i65 + -1 | 0;
      i67 = i62 + 1 | 0;
      i64 = (HEAPU8[i62] << i63) + i64 | 0;
      i63 = i63 + 8 | 0;
      if (i63 >>> 0 < 3) {
       i62 = i67;
      } else {
       i62 = i67;
       break;
      }
     }
    }
    HEAP32[i36 >> 2] = i66 + 1;
    HEAP16[i4 + (HEAPU16[11384 + (i66 << 1) >> 1] << 1) + 112 >> 1] = i64 & 7;
    i64 = i64 >>> 3;
    i63 = i63 + -3 | 0;
    i66 = HEAP32[i36 >> 2] | 0;
    if (i66 >>> 0 < (HEAP32[i34 >> 2] | 0) >>> 0) {
     i60 = 154;
    } else {
     i67 = i63;
     i60 = 158;
     break;
    }
   }
  } else if ((i60 | 0) == 277) {
   i60 = 0;
   if ((HEAP32[i12 >> 2] | 0) == 0) {
    i60 = 284;
    break;
   }
   if ((HEAP32[i11 >> 2] | 0) == 0) {
    i60 = 284;
    break;
   }
   if (i63 >>> 0 < 32) {
    i66 = i62;
    while (1) {
     if ((i65 | 0) == 0) {
      i65 = 0;
      i62 = i66;
      break L17;
     }
     i65 = i65 + -1 | 0;
     i62 = i66 + 1 | 0;
     i64 = (HEAPU8[i66] << i63) + i64 | 0;
     i63 = i63 + 8 | 0;
     if (i63 >>> 0 < 32) {
      i66 = i62;
     } else {
      break;
     }
    }
   }
   if ((i64 | 0) == (HEAP32[i14 >> 2] | 0)) {
    i63 = 0;
    i64 = 0;
    i60 = 284;
    break;
   }
   HEAP32[i35 >> 2] = 11928;
   HEAP32[i4 >> 2] = 29;
   i66 = i26;
  }
  do {
   if ((i60 | 0) == 49) {
    i60 = HEAP32[i38 >> 2] | 0;
    if ((i60 | 0) != 0) {
     HEAP32[i60 + 4 >> 2] = i64;
    }
    if ((HEAP32[i11 >> 2] & 512 | 0) != 0) {
     HEAP8[i25] = i64;
     HEAP8[i39] = i64 >>> 8;
     HEAP8[i44] = i64 >>> 16;
     HEAP8[i45] = i64 >>> 24;
     HEAP32[i10 >> 2] = _crc32(HEAP32[i10 >> 2] | 0, i25, 4) | 0;
    }
    HEAP32[i4 >> 2] = 3;
    i63 = 0;
    i64 = 0;
    i66 = i62;
    i60 = 55;
   } else if ((i60 | 0) == 124) {
    if (i47) {
     i60 = 285;
     break L17;
    } else {
     i60 = 125;
    }
   } else if ((i60 | 0) == 144) {
    i60 = 0;
    i66 = HEAP32[i42 >> 2] | 0;
    if ((i66 | 0) == 0) {
     HEAP32[i4 >> 2] = 11;
     i66 = i26;
     break;
    }
    i66 = i66 >>> 0 > i65 >>> 0 ? i65 : i66;
    i67 = i66 >>> 0 > i26 >>> 0 ? i26 : i66;
    if ((i67 | 0) == 0) {
     i60 = 285;
     break L17;
    }
    _memcpy(i19 | 0, i62 | 0, i67 | 0) | 0;
    HEAP32[i42 >> 2] = (HEAP32[i42 >> 2] | 0) - i67;
    i65 = i65 - i67 | 0;
    i66 = i26 - i67 | 0;
    i62 = i62 + i67 | 0;
    i19 = i19 + i67 | 0;
   } else if ((i60 | 0) == 158) {
    i60 = 0;
    if (i66 >>> 0 < 19) {
     while (1) {
      i61 = i66 + 1 | 0;
      HEAP16[i4 + (HEAPU16[11384 + (i66 << 1) >> 1] << 1) + 112 >> 1] = 0;
      if ((i61 | 0) == 19) {
       break;
      } else {
       i66 = i61;
      }
     }
     HEAP32[i36 >> 2] = 19;
    }
    HEAP32[i53 >> 2] = i52;
    HEAP32[i48 >> 2] = i52;
    HEAP32[i49 >> 2] = 7;
    i61 = _inflate_table(0, i27, 19, i53, i49, i55) | 0;
    if ((i61 | 0) == 0) {
     HEAP32[i36 >> 2] = 0;
     HEAP32[i4 >> 2] = 18;
     i63 = 0;
     i69 = i65;
     i61 = 0;
     i60 = 164;
     break;
    } else {
     HEAP32[i35 >> 2] = 11656;
     HEAP32[i4 >> 2] = 29;
     i63 = i67;
     i66 = i26;
     break;
    }
   }
  } while (0);
  L163 : do {
   if ((i60 | 0) == 55) {
    while (1) {
     i60 = 0;
     if ((i65 | 0) == 0) {
      i65 = 0;
      i62 = i66;
      break L17;
     }
     i65 = i65 + -1 | 0;
     i62 = i66 + 1 | 0;
     i64 = (HEAPU8[i66] << i63) + i64 | 0;
     i63 = i63 + 8 | 0;
     if (i63 >>> 0 < 16) {
      i66 = i62;
      i60 = 55;
     } else {
      i60 = 57;
      break;
     }
    }
   } else if ((i60 | 0) == 125) {
    i60 = 0;
    if ((HEAP32[i7 >> 2] | 0) != 0) {
     i66 = i63 & 7;
     HEAP32[i4 >> 2] = 26;
     i63 = i63 - i66 | 0;
     i64 = i64 >>> i66;
     i66 = i26;
     break;
    }
    if (i63 >>> 0 < 3) {
     while (1) {
      if ((i65 | 0) == 0) {
       i65 = 0;
       break L17;
      }
      i65 = i65 + -1 | 0;
      i66 = i62 + 1 | 0;
      i64 = (HEAPU8[i62] << i63) + i64 | 0;
      i63 = i63 + 8 | 0;
      if (i63 >>> 0 < 3) {
       i62 = i66;
      } else {
       i62 = i66;
       break;
      }
     }
    }
    HEAP32[i7 >> 2] = i64 & 1;
    i66 = i64 >>> 1 & 3;
    if ((i66 | 0) == 0) {
     HEAP32[i4 >> 2] = 13;
    } else if ((i66 | 0) == 1) {
     HEAP32[i48 >> 2] = 11952;
     HEAP32[i49 >> 2] = 9;
     HEAP32[i50 >> 2] = 14e3;
     HEAP32[i51 >> 2] = 5;
     HEAP32[i4 >> 2] = 19;
     if (i43) {
      i60 = 133;
      break L17;
     }
    } else if ((i66 | 0) == 2) {
     HEAP32[i4 >> 2] = 16;
    } else if ((i66 | 0) == 3) {
     HEAP32[i35 >> 2] = 11560;
     HEAP32[i4 >> 2] = 29;
    }
    i63 = i63 + -3 | 0;
    i64 = i64 >>> 3;
    i66 = i26;
   } else if ((i60 | 0) == 164) {
    i60 = 0;
    i65 = HEAP32[i32 >> 2] | 0;
    i66 = HEAP32[i33 >> 2] | 0;
    do {
     if (i63 >>> 0 < (i66 + i65 | 0) >>> 0) {
      i71 = i67;
      L181 : while (1) {
       i70 = (1 << HEAP32[i49 >> 2]) + -1 | 0;
       i72 = i70 & i64;
       i68 = HEAP32[i48 >> 2] | 0;
       i67 = HEAPU8[i68 + (i72 << 2) + 1 | 0] | 0;
       if (i67 >>> 0 > i71 >>> 0) {
        i67 = i71;
        while (1) {
         if ((i69 | 0) == 0) {
          i63 = i67;
          i65 = 0;
          break L17;
         }
         i69 = i69 + -1 | 0;
         i71 = i62 + 1 | 0;
         i64 = (HEAPU8[i62] << i67) + i64 | 0;
         i62 = i67 + 8 | 0;
         i72 = i70 & i64;
         i67 = HEAPU8[i68 + (i72 << 2) + 1 | 0] | 0;
         if (i67 >>> 0 > i62 >>> 0) {
          i67 = i62;
          i62 = i71;
         } else {
          i70 = i62;
          i62 = i71;
          break;
         }
        }
       } else {
        i70 = i71;
       }
       i68 = HEAP16[i68 + (i72 << 2) + 2 >> 1] | 0;
       L188 : do {
        if ((i68 & 65535) < 16) {
         if (i70 >>> 0 < i67 >>> 0) {
          while (1) {
           if ((i69 | 0) == 0) {
            i63 = i70;
            i65 = 0;
            break L17;
           }
           i69 = i69 + -1 | 0;
           i65 = i62 + 1 | 0;
           i64 = (HEAPU8[i62] << i70) + i64 | 0;
           i70 = i70 + 8 | 0;
           if (i70 >>> 0 < i67 >>> 0) {
            i62 = i65;
           } else {
            i62 = i65;
            break;
           }
          }
         }
         HEAP32[i36 >> 2] = i63 + 1;
         HEAP16[i4 + (i63 << 1) + 112 >> 1] = i68;
         i71 = i70 - i67 | 0;
         i64 = i64 >>> i67;
        } else {
         if (i68 << 16 >> 16 == 16) {
          i68 = i67 + 2 | 0;
          if (i70 >>> 0 < i68 >>> 0) {
           i71 = i62;
           while (1) {
            if ((i69 | 0) == 0) {
             i63 = i70;
             i65 = 0;
             i62 = i71;
             break L17;
            }
            i69 = i69 + -1 | 0;
            i62 = i71 + 1 | 0;
            i64 = (HEAPU8[i71] << i70) + i64 | 0;
            i70 = i70 + 8 | 0;
            if (i70 >>> 0 < i68 >>> 0) {
             i71 = i62;
            } else {
             break;
            }
           }
          }
          i64 = i64 >>> i67;
          i67 = i70 - i67 | 0;
          if ((i63 | 0) == 0) {
           i60 = 181;
           break L181;
          }
          i67 = i67 + -2 | 0;
          i68 = (i64 & 3) + 3 | 0;
          i64 = i64 >>> 2;
          i70 = HEAP16[i4 + (i63 + -1 << 1) + 112 >> 1] | 0;
         } else if (i68 << 16 >> 16 == 17) {
          i68 = i67 + 3 | 0;
          if (i70 >>> 0 < i68 >>> 0) {
           i71 = i62;
           while (1) {
            if ((i69 | 0) == 0) {
             i63 = i70;
             i65 = 0;
             i62 = i71;
             break L17;
            }
            i69 = i69 + -1 | 0;
            i62 = i71 + 1 | 0;
            i64 = (HEAPU8[i71] << i70) + i64 | 0;
            i70 = i70 + 8 | 0;
            if (i70 >>> 0 < i68 >>> 0) {
             i71 = i62;
            } else {
             break;
            }
           }
          }
          i64 = i64 >>> i67;
          i67 = -3 - i67 + i70 | 0;
          i68 = (i64 & 7) + 3 | 0;
          i64 = i64 >>> 3;
          i70 = 0;
         } else {
          i68 = i67 + 7 | 0;
          if (i70 >>> 0 < i68 >>> 0) {
           i71 = i62;
           while (1) {
            if ((i69 | 0) == 0) {
             i63 = i70;
             i65 = 0;
             i62 = i71;
             break L17;
            }
            i69 = i69 + -1 | 0;
            i62 = i71 + 1 | 0;
            i64 = (HEAPU8[i71] << i70) + i64 | 0;
            i70 = i70 + 8 | 0;
            if (i70 >>> 0 < i68 >>> 0) {
             i71 = i62;
            } else {
             break;
            }
           }
          }
          i64 = i64 >>> i67;
          i67 = -7 - i67 + i70 | 0;
          i68 = (i64 & 127) + 11 | 0;
          i64 = i64 >>> 7;
          i70 = 0;
         }
         if ((i63 + i68 | 0) >>> 0 > (i66 + i65 | 0) >>> 0) {
          i60 = 190;
          break L181;
         }
         while (1) {
          i68 = i68 + -1 | 0;
          HEAP32[i36 >> 2] = i63 + 1;
          HEAP16[i4 + (i63 << 1) + 112 >> 1] = i70;
          if ((i68 | 0) == 0) {
           i71 = i67;
           break L188;
          }
          i63 = HEAP32[i36 >> 2] | 0;
         }
        }
       } while (0);
       i63 = HEAP32[i36 >> 2] | 0;
       i65 = HEAP32[i32 >> 2] | 0;
       i66 = HEAP32[i33 >> 2] | 0;
       if (!(i63 >>> 0 < (i66 + i65 | 0) >>> 0)) {
        i60 = 193;
        break;
       }
      }
      if ((i60 | 0) == 181) {
       i60 = 0;
       HEAP32[i35 >> 2] = 11688;
       HEAP32[i4 >> 2] = 29;
       i63 = i67;
       i65 = i69;
       i66 = i26;
       break L163;
      } else if ((i60 | 0) == 190) {
       i60 = 0;
       HEAP32[i35 >> 2] = 11688;
       HEAP32[i4 >> 2] = 29;
       i63 = i67;
       i65 = i69;
       i66 = i26;
       break L163;
      } else if ((i60 | 0) == 193) {
       i60 = 0;
       if ((HEAP32[i4 >> 2] | 0) == 29) {
        i63 = i71;
        i65 = i69;
        i66 = i26;
        break L163;
       } else {
        i63 = i71;
        break;
       }
      }
     } else {
      i63 = i67;
     }
    } while (0);
    if ((HEAP16[i56 >> 1] | 0) == 0) {
     HEAP32[i35 >> 2] = 11720;
     HEAP32[i4 >> 2] = 29;
     i65 = i69;
     i66 = i26;
     break;
    }
    HEAP32[i53 >> 2] = i52;
    HEAP32[i48 >> 2] = i52;
    HEAP32[i49 >> 2] = 9;
    i61 = _inflate_table(1, i27, i65, i53, i49, i55) | 0;
    if ((i61 | 0) != 0) {
     HEAP32[i35 >> 2] = 11760;
     HEAP32[i4 >> 2] = 29;
     i65 = i69;
     i66 = i26;
     break;
    }
    HEAP32[i50 >> 2] = HEAP32[i53 >> 2];
    HEAP32[i51 >> 2] = 6;
    i61 = _inflate_table(2, i4 + (HEAP32[i32 >> 2] << 1) + 112 | 0, HEAP32[i33 >> 2] | 0, i53, i51, i55) | 0;
    if ((i61 | 0) == 0) {
     HEAP32[i4 >> 2] = 19;
     if (i43) {
      i65 = i69;
      i61 = 0;
      i60 = 285;
      break L17;
     } else {
      i65 = i69;
      i61 = 0;
      i60 = 201;
      break;
     }
    } else {
     HEAP32[i35 >> 2] = 11792;
     HEAP32[i4 >> 2] = 29;
     i65 = i69;
     i66 = i26;
     break;
    }
   }
  } while (0);
  if ((i60 | 0) == 57) {
   i60 = HEAP32[i38 >> 2] | 0;
   if ((i60 | 0) != 0) {
    HEAP32[i60 + 8 >> 2] = i64 & 255;
    HEAP32[i60 + 12 >> 2] = i64 >>> 8;
   }
   if ((HEAP32[i11 >> 2] & 512 | 0) != 0) {
    HEAP8[i25] = i64;
    HEAP8[i39] = i64 >>> 8;
    HEAP32[i10 >> 2] = _crc32(HEAP32[i10 >> 2] | 0, i25, 2) | 0;
   }
   HEAP32[i4 >> 2] = 4;
   i63 = 0;
   i64 = 0;
   i60 = 62;
  } else if ((i60 | 0) == 201) {
   HEAP32[i4 >> 2] = 20;
   i60 = 202;
  }
  do {
   if ((i60 | 0) == 62) {
    i60 = 0;
    i66 = HEAP32[i11 >> 2] | 0;
    if ((i66 & 1024 | 0) == 0) {
     i60 = HEAP32[i38 >> 2] | 0;
     if ((i60 | 0) != 0) {
      HEAP32[i60 + 16 >> 2] = 0;
     }
    } else {
     if (i63 >>> 0 < 16) {
      while (1) {
       if ((i65 | 0) == 0) {
        i65 = 0;
        break L17;
       }
       i65 = i65 + -1 | 0;
       i67 = i62 + 1 | 0;
       i64 = (HEAPU8[i62] << i63) + i64 | 0;
       i63 = i63 + 8 | 0;
       if (i63 >>> 0 < 16) {
        i62 = i67;
       } else {
        i62 = i67;
        break;
       }
      }
     }
     HEAP32[i42 >> 2] = i64;
     i60 = HEAP32[i38 >> 2] | 0;
     if ((i60 | 0) != 0) {
      HEAP32[i60 + 20 >> 2] = i64;
      i66 = HEAP32[i11 >> 2] | 0;
     }
     if ((i66 & 512 | 0) == 0) {
      i63 = 0;
      i64 = 0;
     } else {
      HEAP8[i25] = i64;
      HEAP8[i39] = i64 >>> 8;
      HEAP32[i10 >> 2] = _crc32(HEAP32[i10 >> 2] | 0, i25, 2) | 0;
      i63 = 0;
      i64 = 0;
     }
    }
    HEAP32[i4 >> 2] = 5;
    i60 = 73;
   } else if ((i60 | 0) == 202) {
    i60 = 0;
    if (i65 >>> 0 > 5 & i26 >>> 0 > 257) {
     HEAP32[i8 >> 2] = i19;
     HEAP32[i15 >> 2] = i26;
     HEAP32[i2 >> 2] = i62;
     HEAP32[i16 >> 2] = i65;
     HEAP32[i17 >> 2] = i64;
     HEAP32[i6 >> 2] = i63;
     _inflate_fast(i2, i59);
     i19 = HEAP32[i8 >> 2] | 0;
     i66 = HEAP32[i15 >> 2] | 0;
     i62 = HEAP32[i2 >> 2] | 0;
     i65 = HEAP32[i16 >> 2] | 0;
     i64 = HEAP32[i17 >> 2] | 0;
     i63 = HEAP32[i6 >> 2] | 0;
     if ((HEAP32[i4 >> 2] | 0) != 11) {
      break;
     }
     HEAP32[i57 >> 2] = -1;
     break;
    }
    HEAP32[i57 >> 2] = 0;
    i69 = (1 << HEAP32[i49 >> 2]) + -1 | 0;
    i71 = i69 & i64;
    i66 = HEAP32[i48 >> 2] | 0;
    i68 = HEAP8[i66 + (i71 << 2) + 1 | 0] | 0;
    i67 = i68 & 255;
    if (i67 >>> 0 > i63 >>> 0) {
     while (1) {
      if ((i65 | 0) == 0) {
       i65 = 0;
       break L17;
      }
      i65 = i65 + -1 | 0;
      i70 = i62 + 1 | 0;
      i64 = (HEAPU8[i62] << i63) + i64 | 0;
      i63 = i63 + 8 | 0;
      i71 = i69 & i64;
      i68 = HEAP8[i66 + (i71 << 2) + 1 | 0] | 0;
      i67 = i68 & 255;
      if (i67 >>> 0 > i63 >>> 0) {
       i62 = i70;
      } else {
       i62 = i70;
       break;
      }
     }
    }
    i69 = HEAP8[i66 + (i71 << 2) | 0] | 0;
    i70 = HEAP16[i66 + (i71 << 2) + 2 >> 1] | 0;
    i71 = i69 & 255;
    if (!(i69 << 24 >> 24 == 0)) {
     if ((i71 & 240 | 0) == 0) {
      i69 = i70 & 65535;
      i70 = (1 << i67 + i71) + -1 | 0;
      i71 = ((i64 & i70) >>> i67) + i69 | 0;
      i68 = HEAP8[i66 + (i71 << 2) + 1 | 0] | 0;
      if (((i68 & 255) + i67 | 0) >>> 0 > i63 >>> 0) {
       while (1) {
        if ((i65 | 0) == 0) {
         i65 = 0;
         break L17;
        }
        i65 = i65 + -1 | 0;
        i71 = i62 + 1 | 0;
        i64 = (HEAPU8[i62] << i63) + i64 | 0;
        i63 = i63 + 8 | 0;
        i62 = ((i64 & i70) >>> i67) + i69 | 0;
        i68 = HEAP8[i66 + (i62 << 2) + 1 | 0] | 0;
        if (((i68 & 255) + i67 | 0) >>> 0 > i63 >>> 0) {
         i62 = i71;
        } else {
         i69 = i62;
         i62 = i71;
         break;
        }
       }
      } else {
       i69 = i71;
      }
      i70 = HEAP16[i66 + (i69 << 2) + 2 >> 1] | 0;
      i69 = HEAP8[i66 + (i69 << 2) | 0] | 0;
      HEAP32[i57 >> 2] = i67;
      i66 = i67;
      i63 = i63 - i67 | 0;
      i64 = i64 >>> i67;
     } else {
      i66 = 0;
     }
    } else {
     i66 = 0;
     i69 = 0;
    }
    i72 = i68 & 255;
    i64 = i64 >>> i72;
    i63 = i63 - i72 | 0;
    HEAP32[i57 >> 2] = i66 + i72;
    HEAP32[i42 >> 2] = i70 & 65535;
    i66 = i69 & 255;
    if (i69 << 24 >> 24 == 0) {
     HEAP32[i4 >> 2] = 25;
     i66 = i26;
     break;
    }
    if ((i66 & 32 | 0) != 0) {
     HEAP32[i57 >> 2] = -1;
     HEAP32[i4 >> 2] = 11;
     i66 = i26;
     break;
    }
    if ((i66 & 64 | 0) == 0) {
     i66 = i66 & 15;
     HEAP32[i37 >> 2] = i66;
     HEAP32[i4 >> 2] = 21;
     i60 = 221;
     break;
    } else {
     HEAP32[i35 >> 2] = 11816;
     HEAP32[i4 >> 2] = 29;
     i66 = i26;
     break;
    }
   }
  } while (0);
  if ((i60 | 0) == 73) {
   i68 = HEAP32[i11 >> 2] | 0;
   if ((i68 & 1024 | 0) != 0) {
    i67 = HEAP32[i42 >> 2] | 0;
    i60 = i67 >>> 0 > i65 >>> 0 ? i65 : i67;
    if ((i60 | 0) != 0) {
     i66 = HEAP32[i38 >> 2] | 0;
     if ((i66 | 0) != 0 ? (i20 = HEAP32[i66 + 16 >> 2] | 0, (i20 | 0) != 0) : 0) {
      i67 = (HEAP32[i66 + 20 >> 2] | 0) - i67 | 0;
      i66 = HEAP32[i66 + 24 >> 2] | 0;
      _memcpy(i20 + i67 | 0, i62 | 0, ((i67 + i60 | 0) >>> 0 > i66 >>> 0 ? i66 - i67 | 0 : i60) | 0) | 0;
      i68 = HEAP32[i11 >> 2] | 0;
     }
     if ((i68 & 512 | 0) != 0) {
      HEAP32[i10 >> 2] = _crc32(HEAP32[i10 >> 2] | 0, i62, i60) | 0;
     }
     i67 = (HEAP32[i42 >> 2] | 0) - i60 | 0;
     HEAP32[i42 >> 2] = i67;
     i65 = i65 - i60 | 0;
     i62 = i62 + i60 | 0;
    }
    if ((i67 | 0) != 0) {
     i60 = 285;
     break;
    }
   }
   HEAP32[i42 >> 2] = 0;
   HEAP32[i4 >> 2] = 6;
   i60 = 83;
  } else if ((i60 | 0) == 221) {
   i60 = 0;
   if ((i66 | 0) == 0) {
    i60 = HEAP32[i42 >> 2] | 0;
   } else {
    if (i63 >>> 0 < i66 >>> 0) {
     while (1) {
      if ((i65 | 0) == 0) {
       i65 = 0;
       break L17;
      }
      i65 = i65 + -1 | 0;
      i67 = i62 + 1 | 0;
      i64 = (HEAPU8[i62] << i63) + i64 | 0;
      i63 = i63 + 8 | 0;
      if (i63 >>> 0 < i66 >>> 0) {
       i62 = i67;
      } else {
       i62 = i67;
       break;
      }
     }
    }
    i60 = (HEAP32[i42 >> 2] | 0) + ((1 << i66) + -1 & i64) | 0;
    HEAP32[i42 >> 2] = i60;
    HEAP32[i57 >> 2] = (HEAP32[i57 >> 2] | 0) + i66;
    i63 = i63 - i66 | 0;
    i64 = i64 >>> i66;
   }
   HEAP32[i58 >> 2] = i60;
   HEAP32[i4 >> 2] = 22;
   i60 = 228;
  }
  do {
   if ((i60 | 0) == 83) {
    if ((HEAP32[i11 >> 2] & 2048 | 0) == 0) {
     i60 = HEAP32[i38 >> 2] | 0;
     if ((i60 | 0) != 0) {
      HEAP32[i60 + 28 >> 2] = 0;
     }
    } else {
     if ((i65 | 0) == 0) {
      i65 = 0;
      i60 = 285;
      break L17;
     } else {
      i66 = 0;
     }
     while (1) {
      i60 = i66 + 1 | 0;
      i67 = HEAP8[i62 + i66 | 0] | 0;
      i66 = HEAP32[i38 >> 2] | 0;
      if (((i66 | 0) != 0 ? (i23 = HEAP32[i66 + 28 >> 2] | 0, (i23 | 0) != 0) : 0) ? (i21 = HEAP32[i42 >> 2] | 0, i21 >>> 0 < (HEAP32[i66 + 32 >> 2] | 0) >>> 0) : 0) {
       HEAP32[i42 >> 2] = i21 + 1;
       HEAP8[i23 + i21 | 0] = i67;
      }
      i66 = i67 << 24 >> 24 != 0;
      if (i66 & i60 >>> 0 < i65 >>> 0) {
       i66 = i60;
      } else {
       break;
      }
     }
     if ((HEAP32[i11 >> 2] & 512 | 0) != 0) {
      HEAP32[i10 >> 2] = _crc32(HEAP32[i10 >> 2] | 0, i62, i60) | 0;
     }
     i65 = i65 - i60 | 0;
     i62 = i62 + i60 | 0;
     if (i66) {
      i60 = 285;
      break L17;
     }
    }
    HEAP32[i42 >> 2] = 0;
    HEAP32[i4 >> 2] = 7;
    i60 = 96;
   } else if ((i60 | 0) == 228) {
    i60 = 0;
    i69 = (1 << HEAP32[i51 >> 2]) + -1 | 0;
    i71 = i69 & i64;
    i66 = HEAP32[i50 >> 2] | 0;
    i68 = HEAP8[i66 + (i71 << 2) + 1 | 0] | 0;
    i67 = i68 & 255;
    if (i67 >>> 0 > i63 >>> 0) {
     while (1) {
      if ((i65 | 0) == 0) {
       i65 = 0;
       break L17;
      }
      i65 = i65 + -1 | 0;
      i70 = i62 + 1 | 0;
      i64 = (HEAPU8[i62] << i63) + i64 | 0;
      i63 = i63 + 8 | 0;
      i71 = i69 & i64;
      i68 = HEAP8[i66 + (i71 << 2) + 1 | 0] | 0;
      i67 = i68 & 255;
      if (i67 >>> 0 > i63 >>> 0) {
       i62 = i70;
      } else {
       i62 = i70;
       break;
      }
     }
    }
    i69 = HEAP8[i66 + (i71 << 2) | 0] | 0;
    i70 = HEAP16[i66 + (i71 << 2) + 2 >> 1] | 0;
    i71 = i69 & 255;
    if ((i71 & 240 | 0) == 0) {
     i69 = i70 & 65535;
     i70 = (1 << i67 + i71) + -1 | 0;
     i71 = ((i64 & i70) >>> i67) + i69 | 0;
     i68 = HEAP8[i66 + (i71 << 2) + 1 | 0] | 0;
     if (((i68 & 255) + i67 | 0) >>> 0 > i63 >>> 0) {
      while (1) {
       if ((i65 | 0) == 0) {
        i65 = 0;
        break L17;
       }
       i65 = i65 + -1 | 0;
       i71 = i62 + 1 | 0;
       i64 = (HEAPU8[i62] << i63) + i64 | 0;
       i63 = i63 + 8 | 0;
       i62 = ((i64 & i70) >>> i67) + i69 | 0;
       i68 = HEAP8[i66 + (i62 << 2) + 1 | 0] | 0;
       if (((i68 & 255) + i67 | 0) >>> 0 > i63 >>> 0) {
        i62 = i71;
       } else {
        i69 = i62;
        i62 = i71;
        break;
       }
      }
     } else {
      i69 = i71;
     }
     i70 = HEAP16[i66 + (i69 << 2) + 2 >> 1] | 0;
     i69 = HEAP8[i66 + (i69 << 2) | 0] | 0;
     i66 = (HEAP32[i57 >> 2] | 0) + i67 | 0;
     HEAP32[i57 >> 2] = i66;
     i63 = i63 - i67 | 0;
     i64 = i64 >>> i67;
    } else {
     i66 = HEAP32[i57 >> 2] | 0;
    }
    i72 = i68 & 255;
    i64 = i64 >>> i72;
    i63 = i63 - i72 | 0;
    HEAP32[i57 >> 2] = i66 + i72;
    i66 = i69 & 255;
    if ((i66 & 64 | 0) == 0) {
     HEAP32[i54 >> 2] = i70 & 65535;
     i66 = i66 & 15;
     HEAP32[i37 >> 2] = i66;
     HEAP32[i4 >> 2] = 23;
     i60 = 240;
     break;
    } else {
     HEAP32[i35 >> 2] = 11848;
     HEAP32[i4 >> 2] = 29;
     i66 = i26;
     break;
    }
   }
  } while (0);
  if ((i60 | 0) == 96) {
   if ((HEAP32[i11 >> 2] & 4096 | 0) == 0) {
    i60 = HEAP32[i38 >> 2] | 0;
    if ((i60 | 0) != 0) {
     HEAP32[i60 + 36 >> 2] = 0;
    }
   } else {
    if ((i65 | 0) == 0) {
     i65 = 0;
     i60 = 285;
     break;
    } else {
     i66 = 0;
    }
    while (1) {
     i60 = i66 + 1 | 0;
     i66 = HEAP8[i62 + i66 | 0] | 0;
     i67 = HEAP32[i38 >> 2] | 0;
     if (((i67 | 0) != 0 ? (i24 = HEAP32[i67 + 36 >> 2] | 0, (i24 | 0) != 0) : 0) ? (i22 = HEAP32[i42 >> 2] | 0, i22 >>> 0 < (HEAP32[i67 + 40 >> 2] | 0) >>> 0) : 0) {
      HEAP32[i42 >> 2] = i22 + 1;
      HEAP8[i24 + i22 | 0] = i66;
     }
     i66 = i66 << 24 >> 24 != 0;
     if (i66 & i60 >>> 0 < i65 >>> 0) {
      i66 = i60;
     } else {
      break;
     }
    }
    if ((HEAP32[i11 >> 2] & 512 | 0) != 0) {
     HEAP32[i10 >> 2] = _crc32(HEAP32[i10 >> 2] | 0, i62, i60) | 0;
    }
    i65 = i65 - i60 | 0;
    i62 = i62 + i60 | 0;
    if (i66) {
     i60 = 285;
     break;
    }
   }
   HEAP32[i4 >> 2] = 8;
   i60 = 109;
  } else if ((i60 | 0) == 240) {
   i60 = 0;
   if ((i66 | 0) != 0) {
    if (i63 >>> 0 < i66 >>> 0) {
     i67 = i62;
     while (1) {
      if ((i65 | 0) == 0) {
       i65 = 0;
       i62 = i67;
       break L17;
      }
      i65 = i65 + -1 | 0;
      i62 = i67 + 1 | 0;
      i64 = (HEAPU8[i67] << i63) + i64 | 0;
      i63 = i63 + 8 | 0;
      if (i63 >>> 0 < i66 >>> 0) {
       i67 = i62;
      } else {
       break;
      }
     }
    }
    HEAP32[i54 >> 2] = (HEAP32[i54 >> 2] | 0) + ((1 << i66) + -1 & i64);
    HEAP32[i57 >> 2] = (HEAP32[i57 >> 2] | 0) + i66;
    i63 = i63 - i66 | 0;
    i64 = i64 >>> i66;
   }
   HEAP32[i4 >> 2] = 24;
   i60 = 246;
  }
  do {
   if ((i60 | 0) == 109) {
    i60 = 0;
    i66 = HEAP32[i11 >> 2] | 0;
    if ((i66 & 512 | 0) != 0) {
     if (i63 >>> 0 < 16) {
      i67 = i62;
      while (1) {
       if ((i65 | 0) == 0) {
        i65 = 0;
        i62 = i67;
        break L17;
       }
       i65 = i65 + -1 | 0;
       i62 = i67 + 1 | 0;
       i64 = (HEAPU8[i67] << i63) + i64 | 0;
       i63 = i63 + 8 | 0;
       if (i63 >>> 0 < 16) {
        i67 = i62;
       } else {
        break;
       }
      }
     }
     if ((i64 | 0) == (HEAP32[i10 >> 2] & 65535 | 0)) {
      i63 = 0;
      i64 = 0;
     } else {
      HEAP32[i35 >> 2] = 11536;
      HEAP32[i4 >> 2] = 29;
      i66 = i26;
      break;
     }
    }
    i67 = HEAP32[i38 >> 2] | 0;
    if ((i67 | 0) != 0) {
     HEAP32[i67 + 44 >> 2] = i66 >>> 9 & 1;
     HEAP32[i67 + 48 >> 2] = 1;
    }
    i66 = _crc32(0, 0, 0) | 0;
    HEAP32[i10 >> 2] = i66;
    HEAP32[i9 >> 2] = i66;
    HEAP32[i4 >> 2] = 11;
    i66 = i26;
   } else if ((i60 | 0) == 246) {
    i60 = 0;
    if ((i26 | 0) == 0) {
     i26 = 0;
     i60 = 285;
     break L17;
    }
    i67 = i59 - i26 | 0;
    i66 = HEAP32[i54 >> 2] | 0;
    if (i66 >>> 0 > i67 >>> 0) {
     i67 = i66 - i67 | 0;
     if (i67 >>> 0 > (HEAP32[i28 >> 2] | 0) >>> 0 ? (HEAP32[i29 >> 2] | 0) != 0 : 0) {
      HEAP32[i35 >> 2] = 11872;
      HEAP32[i4 >> 2] = 29;
      i66 = i26;
      break;
     }
     i68 = HEAP32[i30 >> 2] | 0;
     if (i67 >>> 0 > i68 >>> 0) {
      i68 = i67 - i68 | 0;
      i66 = i68;
      i68 = (HEAP32[i31 >> 2] | 0) + ((HEAP32[i18 >> 2] | 0) - i68) | 0;
     } else {
      i66 = i67;
      i68 = (HEAP32[i31 >> 2] | 0) + (i68 - i67) | 0;
     }
     i69 = HEAP32[i42 >> 2] | 0;
     i67 = i69;
     i69 = i66 >>> 0 > i69 >>> 0 ? i69 : i66;
    } else {
     i69 = HEAP32[i42 >> 2] | 0;
     i67 = i69;
     i68 = i19 + (0 - i66) | 0;
    }
    i66 = i69 >>> 0 > i26 >>> 0 ? i26 : i69;
    HEAP32[i42 >> 2] = i67 - i66;
    i67 = ~i26;
    i69 = ~i69;
    i67 = i67 >>> 0 > i69 >>> 0 ? i67 : i69;
    i69 = i66;
    i70 = i19;
    while (1) {
     HEAP8[i70] = HEAP8[i68] | 0;
     i69 = i69 + -1 | 0;
     if ((i69 | 0) == 0) {
      break;
     } else {
      i68 = i68 + 1 | 0;
      i70 = i70 + 1 | 0;
     }
    }
    i66 = i26 - i66 | 0;
    i19 = i19 + ~i67 | 0;
    if ((HEAP32[i42 >> 2] | 0) == 0) {
     HEAP32[i4 >> 2] = 20;
    }
   }
  } while (0);
  i68 = HEAP32[i4 >> 2] | 0;
  i67 = i63;
  i26 = i66;
 }
 if ((i60 | 0) == 122) {
  HEAP32[i8 >> 2] = i19;
  HEAP32[i15 >> 2] = i26;
  HEAP32[i2 >> 2] = i62;
  HEAP32[i16 >> 2] = i65;
  HEAP32[i17 >> 2] = i64;
  HEAP32[i6 >> 2] = i63;
  i72 = 2;
  STACKTOP = i1;
  return i72 | 0;
 } else if ((i60 | 0) == 133) {
  i63 = i63 + -3 | 0;
  i64 = i64 >>> 3;
 } else if ((i60 | 0) == 284) {
  HEAP32[i4 >> 2] = 28;
  i61 = 1;
 } else if ((i60 | 0) != 285) if ((i60 | 0) == 299) {
  i72 = -4;
  STACKTOP = i1;
  return i72 | 0;
 } else if ((i60 | 0) == 300) {
  STACKTOP = i1;
  return i2 | 0;
 }
 HEAP32[i8 >> 2] = i19;
 HEAP32[i15 >> 2] = i26;
 HEAP32[i2 >> 2] = i62;
 HEAP32[i16 >> 2] = i65;
 HEAP32[i17 >> 2] = i64;
 HEAP32[i6 >> 2] = i63;
 if ((HEAP32[i18 >> 2] | 0) == 0) {
  if ((HEAP32[i4 >> 2] | 0) >>> 0 < 26 ? (i59 | 0) != (HEAP32[i15 >> 2] | 0) : 0) {
   i60 = 289;
  }
 } else {
  i60 = 289;
 }
 if ((i60 | 0) == 289 ? (_updatewindow(i2, i59) | 0) != 0 : 0) {
  HEAP32[i4 >> 2] = 30;
  i72 = -4;
  STACKTOP = i1;
  return i72 | 0;
 }
 i16 = HEAP32[i16 >> 2] | 0;
 i72 = HEAP32[i15 >> 2] | 0;
 i15 = i59 - i72 | 0;
 i71 = i2 + 8 | 0;
 HEAP32[i71 >> 2] = i5 - i16 + (HEAP32[i71 >> 2] | 0);
 HEAP32[i13 >> 2] = (HEAP32[i13 >> 2] | 0) + i15;
 HEAP32[i14 >> 2] = (HEAP32[i14 >> 2] | 0) + i15;
 i13 = (i59 | 0) == (i72 | 0);
 if (!((HEAP32[i12 >> 2] | 0) == 0 | i13)) {
  i12 = HEAP32[i10 >> 2] | 0;
  i8 = (HEAP32[i8 >> 2] | 0) + (0 - i15) | 0;
  if ((HEAP32[i11 >> 2] | 0) == 0) {
   i8 = _adler32(i12, i8, i15) | 0;
  } else {
   i8 = _crc32(i12, i8, i15) | 0;
  }
  HEAP32[i10 >> 2] = i8;
  HEAP32[i9 >> 2] = i8;
 }
 i4 = HEAP32[i4 >> 2] | 0;
 if ((i4 | 0) == 19) {
  i8 = 256;
 } else {
  i8 = (i4 | 0) == 14 ? 256 : 0;
 }
 HEAP32[i2 + 44 >> 2] = ((HEAP32[i7 >> 2] | 0) != 0 ? 64 : 0) + (HEAP32[i6 >> 2] | 0) + ((i4 | 0) == 11 ? 128 : 0) + i8;
 i72 = ((i5 | 0) == (i16 | 0) & i13 | (i3 | 0) == 4) & (i61 | 0) == 0 ? -5 : i61;
 STACKTOP = i1;
 return i72 | 0;
}
function _malloc(i12) {
 i12 = i12 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0;
 i1 = STACKTOP;
 do {
  if (i12 >>> 0 < 245) {
   if (i12 >>> 0 < 11) {
    i12 = 16;
   } else {
    i12 = i12 + 11 & -8;
   }
   i20 = i12 >>> 3;
   i18 = HEAP32[3618] | 0;
   i21 = i18 >>> i20;
   if ((i21 & 3 | 0) != 0) {
    i6 = (i21 & 1 ^ 1) + i20 | 0;
    i5 = i6 << 1;
    i3 = 14512 + (i5 << 2) | 0;
    i5 = 14512 + (i5 + 2 << 2) | 0;
    i7 = HEAP32[i5 >> 2] | 0;
    i2 = i7 + 8 | 0;
    i4 = HEAP32[i2 >> 2] | 0;
    do {
     if ((i3 | 0) != (i4 | 0)) {
      if (i4 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
       _abort();
      }
      i8 = i4 + 12 | 0;
      if ((HEAP32[i8 >> 2] | 0) == (i7 | 0)) {
       HEAP32[i8 >> 2] = i3;
       HEAP32[i5 >> 2] = i4;
       break;
      } else {
       _abort();
      }
     } else {
      HEAP32[3618] = i18 & ~(1 << i6);
     }
    } while (0);
    i32 = i6 << 3;
    HEAP32[i7 + 4 >> 2] = i32 | 3;
    i32 = i7 + (i32 | 4) | 0;
    HEAP32[i32 >> 2] = HEAP32[i32 >> 2] | 1;
    i32 = i2;
    STACKTOP = i1;
    return i32 | 0;
   }
   if (i12 >>> 0 > (HEAP32[14480 >> 2] | 0) >>> 0) {
    if ((i21 | 0) != 0) {
     i7 = 2 << i20;
     i7 = i21 << i20 & (i7 | 0 - i7);
     i7 = (i7 & 0 - i7) + -1 | 0;
     i2 = i7 >>> 12 & 16;
     i7 = i7 >>> i2;
     i6 = i7 >>> 5 & 8;
     i7 = i7 >>> i6;
     i5 = i7 >>> 2 & 4;
     i7 = i7 >>> i5;
     i4 = i7 >>> 1 & 2;
     i7 = i7 >>> i4;
     i3 = i7 >>> 1 & 1;
     i3 = (i6 | i2 | i5 | i4 | i3) + (i7 >>> i3) | 0;
     i7 = i3 << 1;
     i4 = 14512 + (i7 << 2) | 0;
     i7 = 14512 + (i7 + 2 << 2) | 0;
     i5 = HEAP32[i7 >> 2] | 0;
     i2 = i5 + 8 | 0;
     i6 = HEAP32[i2 >> 2] | 0;
     do {
      if ((i4 | 0) != (i6 | 0)) {
       if (i6 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
        _abort();
       }
       i8 = i6 + 12 | 0;
       if ((HEAP32[i8 >> 2] | 0) == (i5 | 0)) {
        HEAP32[i8 >> 2] = i4;
        HEAP32[i7 >> 2] = i6;
        break;
       } else {
        _abort();
       }
      } else {
       HEAP32[3618] = i18 & ~(1 << i3);
      }
     } while (0);
     i6 = i3 << 3;
     i4 = i6 - i12 | 0;
     HEAP32[i5 + 4 >> 2] = i12 | 3;
     i3 = i5 + i12 | 0;
     HEAP32[i5 + (i12 | 4) >> 2] = i4 | 1;
     HEAP32[i5 + i6 >> 2] = i4;
     i6 = HEAP32[14480 >> 2] | 0;
     if ((i6 | 0) != 0) {
      i5 = HEAP32[14492 >> 2] | 0;
      i8 = i6 >>> 3;
      i9 = i8 << 1;
      i6 = 14512 + (i9 << 2) | 0;
      i7 = HEAP32[3618] | 0;
      i8 = 1 << i8;
      if ((i7 & i8 | 0) != 0) {
       i7 = 14512 + (i9 + 2 << 2) | 0;
       i8 = HEAP32[i7 >> 2] | 0;
       if (i8 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
        _abort();
       } else {
        i28 = i7;
        i27 = i8;
       }
      } else {
       HEAP32[3618] = i7 | i8;
       i28 = 14512 + (i9 + 2 << 2) | 0;
       i27 = i6;
      }
      HEAP32[i28 >> 2] = i5;
      HEAP32[i27 + 12 >> 2] = i5;
      HEAP32[i5 + 8 >> 2] = i27;
      HEAP32[i5 + 12 >> 2] = i6;
     }
     HEAP32[14480 >> 2] = i4;
     HEAP32[14492 >> 2] = i3;
     i32 = i2;
     STACKTOP = i1;
     return i32 | 0;
    }
    i18 = HEAP32[14476 >> 2] | 0;
    if ((i18 | 0) != 0) {
     i2 = (i18 & 0 - i18) + -1 | 0;
     i31 = i2 >>> 12 & 16;
     i2 = i2 >>> i31;
     i30 = i2 >>> 5 & 8;
     i2 = i2 >>> i30;
     i32 = i2 >>> 2 & 4;
     i2 = i2 >>> i32;
     i6 = i2 >>> 1 & 2;
     i2 = i2 >>> i6;
     i3 = i2 >>> 1 & 1;
     i3 = HEAP32[14776 + ((i30 | i31 | i32 | i6 | i3) + (i2 >>> i3) << 2) >> 2] | 0;
     i2 = (HEAP32[i3 + 4 >> 2] & -8) - i12 | 0;
     i6 = i3;
     while (1) {
      i5 = HEAP32[i6 + 16 >> 2] | 0;
      if ((i5 | 0) == 0) {
       i5 = HEAP32[i6 + 20 >> 2] | 0;
       if ((i5 | 0) == 0) {
        break;
       }
      }
      i6 = (HEAP32[i5 + 4 >> 2] & -8) - i12 | 0;
      i4 = i6 >>> 0 < i2 >>> 0;
      i2 = i4 ? i6 : i2;
      i6 = i5;
      i3 = i4 ? i5 : i3;
     }
     i6 = HEAP32[14488 >> 2] | 0;
     if (i3 >>> 0 < i6 >>> 0) {
      _abort();
     }
     i4 = i3 + i12 | 0;
     if (!(i3 >>> 0 < i4 >>> 0)) {
      _abort();
     }
     i5 = HEAP32[i3 + 24 >> 2] | 0;
     i7 = HEAP32[i3 + 12 >> 2] | 0;
     do {
      if ((i7 | 0) == (i3 | 0)) {
       i8 = i3 + 20 | 0;
       i7 = HEAP32[i8 >> 2] | 0;
       if ((i7 | 0) == 0) {
        i8 = i3 + 16 | 0;
        i7 = HEAP32[i8 >> 2] | 0;
        if ((i7 | 0) == 0) {
         i26 = 0;
         break;
        }
       }
       while (1) {
        i10 = i7 + 20 | 0;
        i9 = HEAP32[i10 >> 2] | 0;
        if ((i9 | 0) != 0) {
         i7 = i9;
         i8 = i10;
         continue;
        }
        i10 = i7 + 16 | 0;
        i9 = HEAP32[i10 >> 2] | 0;
        if ((i9 | 0) == 0) {
         break;
        } else {
         i7 = i9;
         i8 = i10;
        }
       }
       if (i8 >>> 0 < i6 >>> 0) {
        _abort();
       } else {
        HEAP32[i8 >> 2] = 0;
        i26 = i7;
        break;
       }
      } else {
       i8 = HEAP32[i3 + 8 >> 2] | 0;
       if (i8 >>> 0 < i6 >>> 0) {
        _abort();
       }
       i6 = i8 + 12 | 0;
       if ((HEAP32[i6 >> 2] | 0) != (i3 | 0)) {
        _abort();
       }
       i9 = i7 + 8 | 0;
       if ((HEAP32[i9 >> 2] | 0) == (i3 | 0)) {
        HEAP32[i6 >> 2] = i7;
        HEAP32[i9 >> 2] = i8;
        i26 = i7;
        break;
       } else {
        _abort();
       }
      }
     } while (0);
     do {
      if ((i5 | 0) != 0) {
       i7 = HEAP32[i3 + 28 >> 2] | 0;
       i6 = 14776 + (i7 << 2) | 0;
       if ((i3 | 0) == (HEAP32[i6 >> 2] | 0)) {
        HEAP32[i6 >> 2] = i26;
        if ((i26 | 0) == 0) {
         HEAP32[14476 >> 2] = HEAP32[14476 >> 2] & ~(1 << i7);
         break;
        }
       } else {
        if (i5 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
         _abort();
        }
        i6 = i5 + 16 | 0;
        if ((HEAP32[i6 >> 2] | 0) == (i3 | 0)) {
         HEAP32[i6 >> 2] = i26;
        } else {
         HEAP32[i5 + 20 >> 2] = i26;
        }
        if ((i26 | 0) == 0) {
         break;
        }
       }
       if (i26 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
        _abort();
       }
       HEAP32[i26 + 24 >> 2] = i5;
       i5 = HEAP32[i3 + 16 >> 2] | 0;
       do {
        if ((i5 | 0) != 0) {
         if (i5 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
          _abort();
         } else {
          HEAP32[i26 + 16 >> 2] = i5;
          HEAP32[i5 + 24 >> 2] = i26;
          break;
         }
        }
       } while (0);
       i5 = HEAP32[i3 + 20 >> 2] | 0;
       if ((i5 | 0) != 0) {
        if (i5 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
         _abort();
        } else {
         HEAP32[i26 + 20 >> 2] = i5;
         HEAP32[i5 + 24 >> 2] = i26;
         break;
        }
       }
      }
     } while (0);
     if (i2 >>> 0 < 16) {
      i32 = i2 + i12 | 0;
      HEAP32[i3 + 4 >> 2] = i32 | 3;
      i32 = i3 + (i32 + 4) | 0;
      HEAP32[i32 >> 2] = HEAP32[i32 >> 2] | 1;
     } else {
      HEAP32[i3 + 4 >> 2] = i12 | 3;
      HEAP32[i3 + (i12 | 4) >> 2] = i2 | 1;
      HEAP32[i3 + (i2 + i12) >> 2] = i2;
      i6 = HEAP32[14480 >> 2] | 0;
      if ((i6 | 0) != 0) {
       i5 = HEAP32[14492 >> 2] | 0;
       i8 = i6 >>> 3;
       i9 = i8 << 1;
       i6 = 14512 + (i9 << 2) | 0;
       i7 = HEAP32[3618] | 0;
       i8 = 1 << i8;
       if ((i7 & i8 | 0) != 0) {
        i7 = 14512 + (i9 + 2 << 2) | 0;
        i8 = HEAP32[i7 >> 2] | 0;
        if (i8 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
         _abort();
        } else {
         i25 = i7;
         i24 = i8;
        }
       } else {
        HEAP32[3618] = i7 | i8;
        i25 = 14512 + (i9 + 2 << 2) | 0;
        i24 = i6;
       }
       HEAP32[i25 >> 2] = i5;
       HEAP32[i24 + 12 >> 2] = i5;
       HEAP32[i5 + 8 >> 2] = i24;
       HEAP32[i5 + 12 >> 2] = i6;
      }
      HEAP32[14480 >> 2] = i2;
      HEAP32[14492 >> 2] = i4;
     }
     i32 = i3 + 8 | 0;
     STACKTOP = i1;
     return i32 | 0;
    }
   }
  } else {
   if (!(i12 >>> 0 > 4294967231)) {
    i24 = i12 + 11 | 0;
    i12 = i24 & -8;
    i26 = HEAP32[14476 >> 2] | 0;
    if ((i26 | 0) != 0) {
     i25 = 0 - i12 | 0;
     i24 = i24 >>> 8;
     if ((i24 | 0) != 0) {
      if (i12 >>> 0 > 16777215) {
       i27 = 31;
      } else {
       i31 = (i24 + 1048320 | 0) >>> 16 & 8;
       i32 = i24 << i31;
       i30 = (i32 + 520192 | 0) >>> 16 & 4;
       i32 = i32 << i30;
       i27 = (i32 + 245760 | 0) >>> 16 & 2;
       i27 = 14 - (i30 | i31 | i27) + (i32 << i27 >>> 15) | 0;
       i27 = i12 >>> (i27 + 7 | 0) & 1 | i27 << 1;
      }
     } else {
      i27 = 0;
     }
     i30 = HEAP32[14776 + (i27 << 2) >> 2] | 0;
     L126 : do {
      if ((i30 | 0) == 0) {
       i29 = 0;
       i24 = 0;
      } else {
       if ((i27 | 0) == 31) {
        i24 = 0;
       } else {
        i24 = 25 - (i27 >>> 1) | 0;
       }
       i29 = 0;
       i28 = i12 << i24;
       i24 = 0;
       while (1) {
        i32 = HEAP32[i30 + 4 >> 2] & -8;
        i31 = i32 - i12 | 0;
        if (i31 >>> 0 < i25 >>> 0) {
         if ((i32 | 0) == (i12 | 0)) {
          i25 = i31;
          i29 = i30;
          i24 = i30;
          break L126;
         } else {
          i25 = i31;
          i24 = i30;
         }
        }
        i31 = HEAP32[i30 + 20 >> 2] | 0;
        i30 = HEAP32[i30 + (i28 >>> 31 << 2) + 16 >> 2] | 0;
        i29 = (i31 | 0) == 0 | (i31 | 0) == (i30 | 0) ? i29 : i31;
        if ((i30 | 0) == 0) {
         break;
        } else {
         i28 = i28 << 1;
        }
       }
      }
     } while (0);
     if ((i29 | 0) == 0 & (i24 | 0) == 0) {
      i32 = 2 << i27;
      i26 = i26 & (i32 | 0 - i32);
      if ((i26 | 0) == 0) {
       break;
      }
      i32 = (i26 & 0 - i26) + -1 | 0;
      i28 = i32 >>> 12 & 16;
      i32 = i32 >>> i28;
      i27 = i32 >>> 5 & 8;
      i32 = i32 >>> i27;
      i30 = i32 >>> 2 & 4;
      i32 = i32 >>> i30;
      i31 = i32 >>> 1 & 2;
      i32 = i32 >>> i31;
      i29 = i32 >>> 1 & 1;
      i29 = HEAP32[14776 + ((i27 | i28 | i30 | i31 | i29) + (i32 >>> i29) << 2) >> 2] | 0;
     }
     if ((i29 | 0) != 0) {
      while (1) {
       i27 = (HEAP32[i29 + 4 >> 2] & -8) - i12 | 0;
       i26 = i27 >>> 0 < i25 >>> 0;
       i25 = i26 ? i27 : i25;
       i24 = i26 ? i29 : i24;
       i26 = HEAP32[i29 + 16 >> 2] | 0;
       if ((i26 | 0) != 0) {
        i29 = i26;
        continue;
       }
       i29 = HEAP32[i29 + 20 >> 2] | 0;
       if ((i29 | 0) == 0) {
        break;
       }
      }
     }
     if ((i24 | 0) != 0 ? i25 >>> 0 < ((HEAP32[14480 >> 2] | 0) - i12 | 0) >>> 0 : 0) {
      i4 = HEAP32[14488 >> 2] | 0;
      if (i24 >>> 0 < i4 >>> 0) {
       _abort();
      }
      i2 = i24 + i12 | 0;
      if (!(i24 >>> 0 < i2 >>> 0)) {
       _abort();
      }
      i3 = HEAP32[i24 + 24 >> 2] | 0;
      i6 = HEAP32[i24 + 12 >> 2] | 0;
      do {
       if ((i6 | 0) == (i24 | 0)) {
        i6 = i24 + 20 | 0;
        i5 = HEAP32[i6 >> 2] | 0;
        if ((i5 | 0) == 0) {
         i6 = i24 + 16 | 0;
         i5 = HEAP32[i6 >> 2] | 0;
         if ((i5 | 0) == 0) {
          i22 = 0;
          break;
         }
        }
        while (1) {
         i8 = i5 + 20 | 0;
         i7 = HEAP32[i8 >> 2] | 0;
         if ((i7 | 0) != 0) {
          i5 = i7;
          i6 = i8;
          continue;
         }
         i7 = i5 + 16 | 0;
         i8 = HEAP32[i7 >> 2] | 0;
         if ((i8 | 0) == 0) {
          break;
         } else {
          i5 = i8;
          i6 = i7;
         }
        }
        if (i6 >>> 0 < i4 >>> 0) {
         _abort();
        } else {
         HEAP32[i6 >> 2] = 0;
         i22 = i5;
         break;
        }
       } else {
        i5 = HEAP32[i24 + 8 >> 2] | 0;
        if (i5 >>> 0 < i4 >>> 0) {
         _abort();
        }
        i7 = i5 + 12 | 0;
        if ((HEAP32[i7 >> 2] | 0) != (i24 | 0)) {
         _abort();
        }
        i4 = i6 + 8 | 0;
        if ((HEAP32[i4 >> 2] | 0) == (i24 | 0)) {
         HEAP32[i7 >> 2] = i6;
         HEAP32[i4 >> 2] = i5;
         i22 = i6;
         break;
        } else {
         _abort();
        }
       }
      } while (0);
      do {
       if ((i3 | 0) != 0) {
        i4 = HEAP32[i24 + 28 >> 2] | 0;
        i5 = 14776 + (i4 << 2) | 0;
        if ((i24 | 0) == (HEAP32[i5 >> 2] | 0)) {
         HEAP32[i5 >> 2] = i22;
         if ((i22 | 0) == 0) {
          HEAP32[14476 >> 2] = HEAP32[14476 >> 2] & ~(1 << i4);
          break;
         }
        } else {
         if (i3 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
          _abort();
         }
         i4 = i3 + 16 | 0;
         if ((HEAP32[i4 >> 2] | 0) == (i24 | 0)) {
          HEAP32[i4 >> 2] = i22;
         } else {
          HEAP32[i3 + 20 >> 2] = i22;
         }
         if ((i22 | 0) == 0) {
          break;
         }
        }
        if (i22 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
         _abort();
        }
        HEAP32[i22 + 24 >> 2] = i3;
        i3 = HEAP32[i24 + 16 >> 2] | 0;
        do {
         if ((i3 | 0) != 0) {
          if (i3 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
           _abort();
          } else {
           HEAP32[i22 + 16 >> 2] = i3;
           HEAP32[i3 + 24 >> 2] = i22;
           break;
          }
         }
        } while (0);
        i3 = HEAP32[i24 + 20 >> 2] | 0;
        if ((i3 | 0) != 0) {
         if (i3 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
          _abort();
         } else {
          HEAP32[i22 + 20 >> 2] = i3;
          HEAP32[i3 + 24 >> 2] = i22;
          break;
         }
        }
       }
      } while (0);
      L204 : do {
       if (!(i25 >>> 0 < 16)) {
        HEAP32[i24 + 4 >> 2] = i12 | 3;
        HEAP32[i24 + (i12 | 4) >> 2] = i25 | 1;
        HEAP32[i24 + (i25 + i12) >> 2] = i25;
        i4 = i25 >>> 3;
        if (i25 >>> 0 < 256) {
         i6 = i4 << 1;
         i3 = 14512 + (i6 << 2) | 0;
         i5 = HEAP32[3618] | 0;
         i4 = 1 << i4;
         if ((i5 & i4 | 0) != 0) {
          i5 = 14512 + (i6 + 2 << 2) | 0;
          i4 = HEAP32[i5 >> 2] | 0;
          if (i4 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
           _abort();
          } else {
           i21 = i5;
           i20 = i4;
          }
         } else {
          HEAP32[3618] = i5 | i4;
          i21 = 14512 + (i6 + 2 << 2) | 0;
          i20 = i3;
         }
         HEAP32[i21 >> 2] = i2;
         HEAP32[i20 + 12 >> 2] = i2;
         HEAP32[i24 + (i12 + 8) >> 2] = i20;
         HEAP32[i24 + (i12 + 12) >> 2] = i3;
         break;
        }
        i3 = i25 >>> 8;
        if ((i3 | 0) != 0) {
         if (i25 >>> 0 > 16777215) {
          i3 = 31;
         } else {
          i31 = (i3 + 1048320 | 0) >>> 16 & 8;
          i32 = i3 << i31;
          i30 = (i32 + 520192 | 0) >>> 16 & 4;
          i32 = i32 << i30;
          i3 = (i32 + 245760 | 0) >>> 16 & 2;
          i3 = 14 - (i30 | i31 | i3) + (i32 << i3 >>> 15) | 0;
          i3 = i25 >>> (i3 + 7 | 0) & 1 | i3 << 1;
         }
        } else {
         i3 = 0;
        }
        i6 = 14776 + (i3 << 2) | 0;
        HEAP32[i24 + (i12 + 28) >> 2] = i3;
        HEAP32[i24 + (i12 + 20) >> 2] = 0;
        HEAP32[i24 + (i12 + 16) >> 2] = 0;
        i4 = HEAP32[14476 >> 2] | 0;
        i5 = 1 << i3;
        if ((i4 & i5 | 0) == 0) {
         HEAP32[14476 >> 2] = i4 | i5;
         HEAP32[i6 >> 2] = i2;
         HEAP32[i24 + (i12 + 24) >> 2] = i6;
         HEAP32[i24 + (i12 + 12) >> 2] = i2;
         HEAP32[i24 + (i12 + 8) >> 2] = i2;
         break;
        }
        i4 = HEAP32[i6 >> 2] | 0;
        if ((i3 | 0) == 31) {
         i3 = 0;
        } else {
         i3 = 25 - (i3 >>> 1) | 0;
        }
        L225 : do {
         if ((HEAP32[i4 + 4 >> 2] & -8 | 0) != (i25 | 0)) {
          i3 = i25 << i3;
          while (1) {
           i6 = i4 + (i3 >>> 31 << 2) + 16 | 0;
           i5 = HEAP32[i6 >> 2] | 0;
           if ((i5 | 0) == 0) {
            break;
           }
           if ((HEAP32[i5 + 4 >> 2] & -8 | 0) == (i25 | 0)) {
            i18 = i5;
            break L225;
           } else {
            i3 = i3 << 1;
            i4 = i5;
           }
          }
          if (i6 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
           _abort();
          } else {
           HEAP32[i6 >> 2] = i2;
           HEAP32[i24 + (i12 + 24) >> 2] = i4;
           HEAP32[i24 + (i12 + 12) >> 2] = i2;
           HEAP32[i24 + (i12 + 8) >> 2] = i2;
           break L204;
          }
         } else {
          i18 = i4;
         }
        } while (0);
        i4 = i18 + 8 | 0;
        i3 = HEAP32[i4 >> 2] | 0;
        i5 = HEAP32[14488 >> 2] | 0;
        if (i18 >>> 0 < i5 >>> 0) {
         _abort();
        }
        if (i3 >>> 0 < i5 >>> 0) {
         _abort();
        } else {
         HEAP32[i3 + 12 >> 2] = i2;
         HEAP32[i4 >> 2] = i2;
         HEAP32[i24 + (i12 + 8) >> 2] = i3;
         HEAP32[i24 + (i12 + 12) >> 2] = i18;
         HEAP32[i24 + (i12 + 24) >> 2] = 0;
         break;
        }
       } else {
        i32 = i25 + i12 | 0;
        HEAP32[i24 + 4 >> 2] = i32 | 3;
        i32 = i24 + (i32 + 4) | 0;
        HEAP32[i32 >> 2] = HEAP32[i32 >> 2] | 1;
       }
      } while (0);
      i32 = i24 + 8 | 0;
      STACKTOP = i1;
      return i32 | 0;
     }
    }
   } else {
    i12 = -1;
   }
  }
 } while (0);
 i18 = HEAP32[14480 >> 2] | 0;
 if (!(i12 >>> 0 > i18 >>> 0)) {
  i3 = i18 - i12 | 0;
  i2 = HEAP32[14492 >> 2] | 0;
  if (i3 >>> 0 > 15) {
   HEAP32[14492 >> 2] = i2 + i12;
   HEAP32[14480 >> 2] = i3;
   HEAP32[i2 + (i12 + 4) >> 2] = i3 | 1;
   HEAP32[i2 + i18 >> 2] = i3;
   HEAP32[i2 + 4 >> 2] = i12 | 3;
  } else {
   HEAP32[14480 >> 2] = 0;
   HEAP32[14492 >> 2] = 0;
   HEAP32[i2 + 4 >> 2] = i18 | 3;
   i32 = i2 + (i18 + 4) | 0;
   HEAP32[i32 >> 2] = HEAP32[i32 >> 2] | 1;
  }
  i32 = i2 + 8 | 0;
  STACKTOP = i1;
  return i32 | 0;
 }
 i18 = HEAP32[14484 >> 2] | 0;
 if (i12 >>> 0 < i18 >>> 0) {
  i31 = i18 - i12 | 0;
  HEAP32[14484 >> 2] = i31;
  i32 = HEAP32[14496 >> 2] | 0;
  HEAP32[14496 >> 2] = i32 + i12;
  HEAP32[i32 + (i12 + 4) >> 2] = i31 | 1;
  HEAP32[i32 + 4 >> 2] = i12 | 3;
  i32 = i32 + 8 | 0;
  STACKTOP = i1;
  return i32 | 0;
 }
 do {
  if ((HEAP32[3736] | 0) == 0) {
   i18 = _sysconf(30) | 0;
   if ((i18 + -1 & i18 | 0) == 0) {
    HEAP32[14952 >> 2] = i18;
    HEAP32[14948 >> 2] = i18;
    HEAP32[14956 >> 2] = -1;
    HEAP32[14960 >> 2] = -1;
    HEAP32[14964 >> 2] = 0;
    HEAP32[14916 >> 2] = 0;
    HEAP32[3736] = (_time(0) | 0) & -16 ^ 1431655768;
    break;
   } else {
    _abort();
   }
  }
 } while (0);
 i20 = i12 + 48 | 0;
 i25 = HEAP32[14952 >> 2] | 0;
 i21 = i12 + 47 | 0;
 i22 = i25 + i21 | 0;
 i25 = 0 - i25 | 0;
 i18 = i22 & i25;
 if (!(i18 >>> 0 > i12 >>> 0)) {
  i32 = 0;
  STACKTOP = i1;
  return i32 | 0;
 }
 i24 = HEAP32[14912 >> 2] | 0;
 if ((i24 | 0) != 0 ? (i31 = HEAP32[14904 >> 2] | 0, i32 = i31 + i18 | 0, i32 >>> 0 <= i31 >>> 0 | i32 >>> 0 > i24 >>> 0) : 0) {
  i32 = 0;
  STACKTOP = i1;
  return i32 | 0;
 }
 L269 : do {
  if ((HEAP32[14916 >> 2] & 4 | 0) == 0) {
   i26 = HEAP32[14496 >> 2] | 0;
   L271 : do {
    if ((i26 | 0) != 0) {
     i24 = 14920 | 0;
     while (1) {
      i27 = HEAP32[i24 >> 2] | 0;
      if (!(i27 >>> 0 > i26 >>> 0) ? (i23 = i24 + 4 | 0, (i27 + (HEAP32[i23 >> 2] | 0) | 0) >>> 0 > i26 >>> 0) : 0) {
       break;
      }
      i24 = HEAP32[i24 + 8 >> 2] | 0;
      if ((i24 | 0) == 0) {
       i13 = 182;
       break L271;
      }
     }
     if ((i24 | 0) != 0) {
      i25 = i22 - (HEAP32[14484 >> 2] | 0) & i25;
      if (i25 >>> 0 < 2147483647) {
       i13 = _sbrk(i25 | 0) | 0;
       i26 = (i13 | 0) == ((HEAP32[i24 >> 2] | 0) + (HEAP32[i23 >> 2] | 0) | 0);
       i22 = i13;
       i24 = i25;
       i23 = i26 ? i13 : -1;
       i25 = i26 ? i25 : 0;
       i13 = 191;
      } else {
       i25 = 0;
      }
     } else {
      i13 = 182;
     }
    } else {
     i13 = 182;
    }
   } while (0);
   do {
    if ((i13 | 0) == 182) {
     i23 = _sbrk(0) | 0;
     if ((i23 | 0) != (-1 | 0)) {
      i24 = i23;
      i22 = HEAP32[14948 >> 2] | 0;
      i25 = i22 + -1 | 0;
      if ((i25 & i24 | 0) == 0) {
       i25 = i18;
      } else {
       i25 = i18 - i24 + (i25 + i24 & 0 - i22) | 0;
      }
      i24 = HEAP32[14904 >> 2] | 0;
      i26 = i24 + i25 | 0;
      if (i25 >>> 0 > i12 >>> 0 & i25 >>> 0 < 2147483647) {
       i22 = HEAP32[14912 >> 2] | 0;
       if ((i22 | 0) != 0 ? i26 >>> 0 <= i24 >>> 0 | i26 >>> 0 > i22 >>> 0 : 0) {
        i25 = 0;
        break;
       }
       i22 = _sbrk(i25 | 0) | 0;
       i13 = (i22 | 0) == (i23 | 0);
       i24 = i25;
       i23 = i13 ? i23 : -1;
       i25 = i13 ? i25 : 0;
       i13 = 191;
      } else {
       i25 = 0;
      }
     } else {
      i25 = 0;
     }
    }
   } while (0);
   L291 : do {
    if ((i13 | 0) == 191) {
     i13 = 0 - i24 | 0;
     if ((i23 | 0) != (-1 | 0)) {
      i17 = i23;
      i14 = i25;
      i13 = 202;
      break L269;
     }
     do {
      if ((i22 | 0) != (-1 | 0) & i24 >>> 0 < 2147483647 & i24 >>> 0 < i20 >>> 0 ? (i19 = HEAP32[14952 >> 2] | 0, i19 = i21 - i24 + i19 & 0 - i19, i19 >>> 0 < 2147483647) : 0) {
       if ((_sbrk(i19 | 0) | 0) == (-1 | 0)) {
        _sbrk(i13 | 0) | 0;
        break L291;
       } else {
        i24 = i19 + i24 | 0;
        break;
       }
      }
     } while (0);
     if ((i22 | 0) != (-1 | 0)) {
      i17 = i22;
      i14 = i24;
      i13 = 202;
      break L269;
     }
    }
   } while (0);
   HEAP32[14916 >> 2] = HEAP32[14916 >> 2] | 4;
   i13 = 199;
  } else {
   i25 = 0;
   i13 = 199;
  }
 } while (0);
 if ((((i13 | 0) == 199 ? i18 >>> 0 < 2147483647 : 0) ? (i17 = _sbrk(i18 | 0) | 0, i16 = _sbrk(0) | 0, (i16 | 0) != (-1 | 0) & (i17 | 0) != (-1 | 0) & i17 >>> 0 < i16 >>> 0) : 0) ? (i15 = i16 - i17 | 0, i14 = i15 >>> 0 > (i12 + 40 | 0) >>> 0, i14) : 0) {
  i14 = i14 ? i15 : i25;
  i13 = 202;
 }
 if ((i13 | 0) == 202) {
  i15 = (HEAP32[14904 >> 2] | 0) + i14 | 0;
  HEAP32[14904 >> 2] = i15;
  if (i15 >>> 0 > (HEAP32[14908 >> 2] | 0) >>> 0) {
   HEAP32[14908 >> 2] = i15;
  }
  i15 = HEAP32[14496 >> 2] | 0;
  L311 : do {
   if ((i15 | 0) != 0) {
    i21 = 14920 | 0;
    while (1) {
     i16 = HEAP32[i21 >> 2] | 0;
     i19 = i21 + 4 | 0;
     i20 = HEAP32[i19 >> 2] | 0;
     if ((i17 | 0) == (i16 + i20 | 0)) {
      i13 = 214;
      break;
     }
     i18 = HEAP32[i21 + 8 >> 2] | 0;
     if ((i18 | 0) == 0) {
      break;
     } else {
      i21 = i18;
     }
    }
    if (((i13 | 0) == 214 ? (HEAP32[i21 + 12 >> 2] & 8 | 0) == 0 : 0) ? i15 >>> 0 >= i16 >>> 0 & i15 >>> 0 < i17 >>> 0 : 0) {
     HEAP32[i19 >> 2] = i20 + i14;
     i2 = (HEAP32[14484 >> 2] | 0) + i14 | 0;
     i3 = i15 + 8 | 0;
     if ((i3 & 7 | 0) == 0) {
      i3 = 0;
     } else {
      i3 = 0 - i3 & 7;
     }
     i32 = i2 - i3 | 0;
     HEAP32[14496 >> 2] = i15 + i3;
     HEAP32[14484 >> 2] = i32;
     HEAP32[i15 + (i3 + 4) >> 2] = i32 | 1;
     HEAP32[i15 + (i2 + 4) >> 2] = 40;
     HEAP32[14500 >> 2] = HEAP32[14960 >> 2];
     break;
    }
    if (i17 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
     HEAP32[14488 >> 2] = i17;
    }
    i19 = i17 + i14 | 0;
    i16 = 14920 | 0;
    while (1) {
     if ((HEAP32[i16 >> 2] | 0) == (i19 | 0)) {
      i13 = 224;
      break;
     }
     i18 = HEAP32[i16 + 8 >> 2] | 0;
     if ((i18 | 0) == 0) {
      break;
     } else {
      i16 = i18;
     }
    }
    if ((i13 | 0) == 224 ? (HEAP32[i16 + 12 >> 2] & 8 | 0) == 0 : 0) {
     HEAP32[i16 >> 2] = i17;
     i6 = i16 + 4 | 0;
     HEAP32[i6 >> 2] = (HEAP32[i6 >> 2] | 0) + i14;
     i6 = i17 + 8 | 0;
     if ((i6 & 7 | 0) == 0) {
      i6 = 0;
     } else {
      i6 = 0 - i6 & 7;
     }
     i7 = i17 + (i14 + 8) | 0;
     if ((i7 & 7 | 0) == 0) {
      i13 = 0;
     } else {
      i13 = 0 - i7 & 7;
     }
     i15 = i17 + (i13 + i14) | 0;
     i8 = i6 + i12 | 0;
     i7 = i17 + i8 | 0;
     i10 = i15 - (i17 + i6) - i12 | 0;
     HEAP32[i17 + (i6 + 4) >> 2] = i12 | 3;
     L348 : do {
      if ((i15 | 0) != (HEAP32[14496 >> 2] | 0)) {
       if ((i15 | 0) == (HEAP32[14492 >> 2] | 0)) {
        i32 = (HEAP32[14480 >> 2] | 0) + i10 | 0;
        HEAP32[14480 >> 2] = i32;
        HEAP32[14492 >> 2] = i7;
        HEAP32[i17 + (i8 + 4) >> 2] = i32 | 1;
        HEAP32[i17 + (i32 + i8) >> 2] = i32;
        break;
       }
       i12 = i14 + 4 | 0;
       i18 = HEAP32[i17 + (i12 + i13) >> 2] | 0;
       if ((i18 & 3 | 0) == 1) {
        i11 = i18 & -8;
        i16 = i18 >>> 3;
        do {
         if (!(i18 >>> 0 < 256)) {
          i9 = HEAP32[i17 + ((i13 | 24) + i14) >> 2] | 0;
          i19 = HEAP32[i17 + (i14 + 12 + i13) >> 2] | 0;
          do {
           if ((i19 | 0) == (i15 | 0)) {
            i19 = i13 | 16;
            i18 = i17 + (i12 + i19) | 0;
            i16 = HEAP32[i18 >> 2] | 0;
            if ((i16 | 0) == 0) {
             i18 = i17 + (i19 + i14) | 0;
             i16 = HEAP32[i18 >> 2] | 0;
             if ((i16 | 0) == 0) {
              i5 = 0;
              break;
             }
            }
            while (1) {
             i20 = i16 + 20 | 0;
             i19 = HEAP32[i20 >> 2] | 0;
             if ((i19 | 0) != 0) {
              i16 = i19;
              i18 = i20;
              continue;
             }
             i19 = i16 + 16 | 0;
             i20 = HEAP32[i19 >> 2] | 0;
             if ((i20 | 0) == 0) {
              break;
             } else {
              i16 = i20;
              i18 = i19;
             }
            }
            if (i18 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
             _abort();
            } else {
             HEAP32[i18 >> 2] = 0;
             i5 = i16;
             break;
            }
           } else {
            i18 = HEAP32[i17 + ((i13 | 8) + i14) >> 2] | 0;
            if (i18 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
             _abort();
            }
            i16 = i18 + 12 | 0;
            if ((HEAP32[i16 >> 2] | 0) != (i15 | 0)) {
             _abort();
            }
            i20 = i19 + 8 | 0;
            if ((HEAP32[i20 >> 2] | 0) == (i15 | 0)) {
             HEAP32[i16 >> 2] = i19;
             HEAP32[i20 >> 2] = i18;
             i5 = i19;
             break;
            } else {
             _abort();
            }
           }
          } while (0);
          if ((i9 | 0) != 0) {
           i16 = HEAP32[i17 + (i14 + 28 + i13) >> 2] | 0;
           i18 = 14776 + (i16 << 2) | 0;
           if ((i15 | 0) == (HEAP32[i18 >> 2] | 0)) {
            HEAP32[i18 >> 2] = i5;
            if ((i5 | 0) == 0) {
             HEAP32[14476 >> 2] = HEAP32[14476 >> 2] & ~(1 << i16);
             break;
            }
           } else {
            if (i9 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
             _abort();
            }
            i16 = i9 + 16 | 0;
            if ((HEAP32[i16 >> 2] | 0) == (i15 | 0)) {
             HEAP32[i16 >> 2] = i5;
            } else {
             HEAP32[i9 + 20 >> 2] = i5;
            }
            if ((i5 | 0) == 0) {
             break;
            }
           }
           if (i5 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
            _abort();
           }
           HEAP32[i5 + 24 >> 2] = i9;
           i15 = i13 | 16;
           i9 = HEAP32[i17 + (i15 + i14) >> 2] | 0;
           do {
            if ((i9 | 0) != 0) {
             if (i9 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
              _abort();
             } else {
              HEAP32[i5 + 16 >> 2] = i9;
              HEAP32[i9 + 24 >> 2] = i5;
              break;
             }
            }
           } while (0);
           i9 = HEAP32[i17 + (i12 + i15) >> 2] | 0;
           if ((i9 | 0) != 0) {
            if (i9 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
             _abort();
            } else {
             HEAP32[i5 + 20 >> 2] = i9;
             HEAP32[i9 + 24 >> 2] = i5;
             break;
            }
           }
          }
         } else {
          i5 = HEAP32[i17 + ((i13 | 8) + i14) >> 2] | 0;
          i12 = HEAP32[i17 + (i14 + 12 + i13) >> 2] | 0;
          i18 = 14512 + (i16 << 1 << 2) | 0;
          if ((i5 | 0) != (i18 | 0)) {
           if (i5 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
            _abort();
           }
           if ((HEAP32[i5 + 12 >> 2] | 0) != (i15 | 0)) {
            _abort();
           }
          }
          if ((i12 | 0) == (i5 | 0)) {
           HEAP32[3618] = HEAP32[3618] & ~(1 << i16);
           break;
          }
          if ((i12 | 0) != (i18 | 0)) {
           if (i12 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
            _abort();
           }
           i16 = i12 + 8 | 0;
           if ((HEAP32[i16 >> 2] | 0) == (i15 | 0)) {
            i9 = i16;
           } else {
            _abort();
           }
          } else {
           i9 = i12 + 8 | 0;
          }
          HEAP32[i5 + 12 >> 2] = i12;
          HEAP32[i9 >> 2] = i5;
         }
        } while (0);
        i15 = i17 + ((i11 | i13) + i14) | 0;
        i10 = i11 + i10 | 0;
       }
       i5 = i15 + 4 | 0;
       HEAP32[i5 >> 2] = HEAP32[i5 >> 2] & -2;
       HEAP32[i17 + (i8 + 4) >> 2] = i10 | 1;
       HEAP32[i17 + (i10 + i8) >> 2] = i10;
       i5 = i10 >>> 3;
       if (i10 >>> 0 < 256) {
        i10 = i5 << 1;
        i2 = 14512 + (i10 << 2) | 0;
        i9 = HEAP32[3618] | 0;
        i5 = 1 << i5;
        if ((i9 & i5 | 0) != 0) {
         i9 = 14512 + (i10 + 2 << 2) | 0;
         i5 = HEAP32[i9 >> 2] | 0;
         if (i5 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
          _abort();
         } else {
          i3 = i9;
          i4 = i5;
         }
        } else {
         HEAP32[3618] = i9 | i5;
         i3 = 14512 + (i10 + 2 << 2) | 0;
         i4 = i2;
        }
        HEAP32[i3 >> 2] = i7;
        HEAP32[i4 + 12 >> 2] = i7;
        HEAP32[i17 + (i8 + 8) >> 2] = i4;
        HEAP32[i17 + (i8 + 12) >> 2] = i2;
        break;
       }
       i3 = i10 >>> 8;
       if ((i3 | 0) != 0) {
        if (i10 >>> 0 > 16777215) {
         i3 = 31;
        } else {
         i31 = (i3 + 1048320 | 0) >>> 16 & 8;
         i32 = i3 << i31;
         i30 = (i32 + 520192 | 0) >>> 16 & 4;
         i32 = i32 << i30;
         i3 = (i32 + 245760 | 0) >>> 16 & 2;
         i3 = 14 - (i30 | i31 | i3) + (i32 << i3 >>> 15) | 0;
         i3 = i10 >>> (i3 + 7 | 0) & 1 | i3 << 1;
        }
       } else {
        i3 = 0;
       }
       i4 = 14776 + (i3 << 2) | 0;
       HEAP32[i17 + (i8 + 28) >> 2] = i3;
       HEAP32[i17 + (i8 + 20) >> 2] = 0;
       HEAP32[i17 + (i8 + 16) >> 2] = 0;
       i9 = HEAP32[14476 >> 2] | 0;
       i5 = 1 << i3;
       if ((i9 & i5 | 0) == 0) {
        HEAP32[14476 >> 2] = i9 | i5;
        HEAP32[i4 >> 2] = i7;
        HEAP32[i17 + (i8 + 24) >> 2] = i4;
        HEAP32[i17 + (i8 + 12) >> 2] = i7;
        HEAP32[i17 + (i8 + 8) >> 2] = i7;
        break;
       }
       i4 = HEAP32[i4 >> 2] | 0;
       if ((i3 | 0) == 31) {
        i3 = 0;
       } else {
        i3 = 25 - (i3 >>> 1) | 0;
       }
       L444 : do {
        if ((HEAP32[i4 + 4 >> 2] & -8 | 0) != (i10 | 0)) {
         i3 = i10 << i3;
         while (1) {
          i5 = i4 + (i3 >>> 31 << 2) + 16 | 0;
          i9 = HEAP32[i5 >> 2] | 0;
          if ((i9 | 0) == 0) {
           break;
          }
          if ((HEAP32[i9 + 4 >> 2] & -8 | 0) == (i10 | 0)) {
           i2 = i9;
           break L444;
          } else {
           i3 = i3 << 1;
           i4 = i9;
          }
         }
         if (i5 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
          _abort();
         } else {
          HEAP32[i5 >> 2] = i7;
          HEAP32[i17 + (i8 + 24) >> 2] = i4;
          HEAP32[i17 + (i8 + 12) >> 2] = i7;
          HEAP32[i17 + (i8 + 8) >> 2] = i7;
          break L348;
         }
        } else {
         i2 = i4;
        }
       } while (0);
       i4 = i2 + 8 | 0;
       i3 = HEAP32[i4 >> 2] | 0;
       i5 = HEAP32[14488 >> 2] | 0;
       if (i2 >>> 0 < i5 >>> 0) {
        _abort();
       }
       if (i3 >>> 0 < i5 >>> 0) {
        _abort();
       } else {
        HEAP32[i3 + 12 >> 2] = i7;
        HEAP32[i4 >> 2] = i7;
        HEAP32[i17 + (i8 + 8) >> 2] = i3;
        HEAP32[i17 + (i8 + 12) >> 2] = i2;
        HEAP32[i17 + (i8 + 24) >> 2] = 0;
        break;
       }
      } else {
       i32 = (HEAP32[14484 >> 2] | 0) + i10 | 0;
       HEAP32[14484 >> 2] = i32;
       HEAP32[14496 >> 2] = i7;
       HEAP32[i17 + (i8 + 4) >> 2] = i32 | 1;
      }
     } while (0);
     i32 = i17 + (i6 | 8) | 0;
     STACKTOP = i1;
     return i32 | 0;
    }
    i3 = 14920 | 0;
    while (1) {
     i2 = HEAP32[i3 >> 2] | 0;
     if (!(i2 >>> 0 > i15 >>> 0) ? (i11 = HEAP32[i3 + 4 >> 2] | 0, i10 = i2 + i11 | 0, i10 >>> 0 > i15 >>> 0) : 0) {
      break;
     }
     i3 = HEAP32[i3 + 8 >> 2] | 0;
    }
    i3 = i2 + (i11 + -39) | 0;
    if ((i3 & 7 | 0) == 0) {
     i3 = 0;
    } else {
     i3 = 0 - i3 & 7;
    }
    i2 = i2 + (i11 + -47 + i3) | 0;
    i2 = i2 >>> 0 < (i15 + 16 | 0) >>> 0 ? i15 : i2;
    i3 = i2 + 8 | 0;
    i4 = i17 + 8 | 0;
    if ((i4 & 7 | 0) == 0) {
     i4 = 0;
    } else {
     i4 = 0 - i4 & 7;
    }
    i32 = i14 + -40 - i4 | 0;
    HEAP32[14496 >> 2] = i17 + i4;
    HEAP32[14484 >> 2] = i32;
    HEAP32[i17 + (i4 + 4) >> 2] = i32 | 1;
    HEAP32[i17 + (i14 + -36) >> 2] = 40;
    HEAP32[14500 >> 2] = HEAP32[14960 >> 2];
    HEAP32[i2 + 4 >> 2] = 27;
    HEAP32[i3 + 0 >> 2] = HEAP32[14920 >> 2];
    HEAP32[i3 + 4 >> 2] = HEAP32[14924 >> 2];
    HEAP32[i3 + 8 >> 2] = HEAP32[14928 >> 2];
    HEAP32[i3 + 12 >> 2] = HEAP32[14932 >> 2];
    HEAP32[14920 >> 2] = i17;
    HEAP32[14924 >> 2] = i14;
    HEAP32[14932 >> 2] = 0;
    HEAP32[14928 >> 2] = i3;
    i4 = i2 + 28 | 0;
    HEAP32[i4 >> 2] = 7;
    if ((i2 + 32 | 0) >>> 0 < i10 >>> 0) {
     while (1) {
      i3 = i4 + 4 | 0;
      HEAP32[i3 >> 2] = 7;
      if ((i4 + 8 | 0) >>> 0 < i10 >>> 0) {
       i4 = i3;
      } else {
       break;
      }
     }
    }
    if ((i2 | 0) != (i15 | 0)) {
     i2 = i2 - i15 | 0;
     i3 = i15 + (i2 + 4) | 0;
     HEAP32[i3 >> 2] = HEAP32[i3 >> 2] & -2;
     HEAP32[i15 + 4 >> 2] = i2 | 1;
     HEAP32[i15 + i2 >> 2] = i2;
     i3 = i2 >>> 3;
     if (i2 >>> 0 < 256) {
      i4 = i3 << 1;
      i2 = 14512 + (i4 << 2) | 0;
      i5 = HEAP32[3618] | 0;
      i3 = 1 << i3;
      if ((i5 & i3 | 0) != 0) {
       i4 = 14512 + (i4 + 2 << 2) | 0;
       i3 = HEAP32[i4 >> 2] | 0;
       if (i3 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
        _abort();
       } else {
        i7 = i4;
        i8 = i3;
       }
      } else {
       HEAP32[3618] = i5 | i3;
       i7 = 14512 + (i4 + 2 << 2) | 0;
       i8 = i2;
      }
      HEAP32[i7 >> 2] = i15;
      HEAP32[i8 + 12 >> 2] = i15;
      HEAP32[i15 + 8 >> 2] = i8;
      HEAP32[i15 + 12 >> 2] = i2;
      break;
     }
     i3 = i2 >>> 8;
     if ((i3 | 0) != 0) {
      if (i2 >>> 0 > 16777215) {
       i3 = 31;
      } else {
       i31 = (i3 + 1048320 | 0) >>> 16 & 8;
       i32 = i3 << i31;
       i30 = (i32 + 520192 | 0) >>> 16 & 4;
       i32 = i32 << i30;
       i3 = (i32 + 245760 | 0) >>> 16 & 2;
       i3 = 14 - (i30 | i31 | i3) + (i32 << i3 >>> 15) | 0;
       i3 = i2 >>> (i3 + 7 | 0) & 1 | i3 << 1;
      }
     } else {
      i3 = 0;
     }
     i7 = 14776 + (i3 << 2) | 0;
     HEAP32[i15 + 28 >> 2] = i3;
     HEAP32[i15 + 20 >> 2] = 0;
     HEAP32[i15 + 16 >> 2] = 0;
     i4 = HEAP32[14476 >> 2] | 0;
     i5 = 1 << i3;
     if ((i4 & i5 | 0) == 0) {
      HEAP32[14476 >> 2] = i4 | i5;
      HEAP32[i7 >> 2] = i15;
      HEAP32[i15 + 24 >> 2] = i7;
      HEAP32[i15 + 12 >> 2] = i15;
      HEAP32[i15 + 8 >> 2] = i15;
      break;
     }
     i4 = HEAP32[i7 >> 2] | 0;
     if ((i3 | 0) == 31) {
      i3 = 0;
     } else {
      i3 = 25 - (i3 >>> 1) | 0;
     }
     L499 : do {
      if ((HEAP32[i4 + 4 >> 2] & -8 | 0) != (i2 | 0)) {
       i3 = i2 << i3;
       while (1) {
        i7 = i4 + (i3 >>> 31 << 2) + 16 | 0;
        i5 = HEAP32[i7 >> 2] | 0;
        if ((i5 | 0) == 0) {
         break;
        }
        if ((HEAP32[i5 + 4 >> 2] & -8 | 0) == (i2 | 0)) {
         i6 = i5;
         break L499;
        } else {
         i3 = i3 << 1;
         i4 = i5;
        }
       }
       if (i7 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
        _abort();
       } else {
        HEAP32[i7 >> 2] = i15;
        HEAP32[i15 + 24 >> 2] = i4;
        HEAP32[i15 + 12 >> 2] = i15;
        HEAP32[i15 + 8 >> 2] = i15;
        break L311;
       }
      } else {
       i6 = i4;
      }
     } while (0);
     i4 = i6 + 8 | 0;
     i3 = HEAP32[i4 >> 2] | 0;
     i2 = HEAP32[14488 >> 2] | 0;
     if (i6 >>> 0 < i2 >>> 0) {
      _abort();
     }
     if (i3 >>> 0 < i2 >>> 0) {
      _abort();
     } else {
      HEAP32[i3 + 12 >> 2] = i15;
      HEAP32[i4 >> 2] = i15;
      HEAP32[i15 + 8 >> 2] = i3;
      HEAP32[i15 + 12 >> 2] = i6;
      HEAP32[i15 + 24 >> 2] = 0;
      break;
     }
    }
   } else {
    i32 = HEAP32[14488 >> 2] | 0;
    if ((i32 | 0) == 0 | i17 >>> 0 < i32 >>> 0) {
     HEAP32[14488 >> 2] = i17;
    }
    HEAP32[14920 >> 2] = i17;
    HEAP32[14924 >> 2] = i14;
    HEAP32[14932 >> 2] = 0;
    HEAP32[14508 >> 2] = HEAP32[3736];
    HEAP32[14504 >> 2] = -1;
    i2 = 0;
    do {
     i32 = i2 << 1;
     i31 = 14512 + (i32 << 2) | 0;
     HEAP32[14512 + (i32 + 3 << 2) >> 2] = i31;
     HEAP32[14512 + (i32 + 2 << 2) >> 2] = i31;
     i2 = i2 + 1 | 0;
    } while ((i2 | 0) != 32);
    i2 = i17 + 8 | 0;
    if ((i2 & 7 | 0) == 0) {
     i2 = 0;
    } else {
     i2 = 0 - i2 & 7;
    }
    i32 = i14 + -40 - i2 | 0;
    HEAP32[14496 >> 2] = i17 + i2;
    HEAP32[14484 >> 2] = i32;
    HEAP32[i17 + (i2 + 4) >> 2] = i32 | 1;
    HEAP32[i17 + (i14 + -36) >> 2] = 40;
    HEAP32[14500 >> 2] = HEAP32[14960 >> 2];
   }
  } while (0);
  i2 = HEAP32[14484 >> 2] | 0;
  if (i2 >>> 0 > i12 >>> 0) {
   i31 = i2 - i12 | 0;
   HEAP32[14484 >> 2] = i31;
   i32 = HEAP32[14496 >> 2] | 0;
   HEAP32[14496 >> 2] = i32 + i12;
   HEAP32[i32 + (i12 + 4) >> 2] = i31 | 1;
   HEAP32[i32 + 4 >> 2] = i12 | 3;
   i32 = i32 + 8 | 0;
   STACKTOP = i1;
   return i32 | 0;
  }
 }
 HEAP32[(___errno_location() | 0) >> 2] = 12;
 i32 = 0;
 STACKTOP = i1;
 return i32 | 0;
}
function _deflate(i2, i10) {
 i2 = i2 | 0;
 i10 = i10 | 0;
 var i1 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0;
 i1 = STACKTOP;
 if ((i2 | 0) == 0) {
  i37 = -2;
  STACKTOP = i1;
  return i37 | 0;
 }
 i5 = i2 + 28 | 0;
 i7 = HEAP32[i5 >> 2] | 0;
 if ((i7 | 0) == 0 | i10 >>> 0 > 5) {
  i37 = -2;
  STACKTOP = i1;
  return i37 | 0;
 }
 i4 = i2 + 12 | 0;
 do {
  if ((HEAP32[i4 >> 2] | 0) != 0) {
   if ((HEAP32[i2 >> 2] | 0) == 0 ? (HEAP32[i2 + 4 >> 2] | 0) != 0 : 0) {
    break;
   }
   i11 = i7 + 4 | 0;
   i29 = HEAP32[i11 >> 2] | 0;
   i9 = (i10 | 0) == 4;
   if ((i29 | 0) != 666 | i9) {
    i3 = i2 + 16 | 0;
    if ((HEAP32[i3 >> 2] | 0) == 0) {
     HEAP32[i2 + 24 >> 2] = HEAP32[3180 >> 2];
     i37 = -5;
     STACKTOP = i1;
     return i37 | 0;
    }
    HEAP32[i7 >> 2] = i2;
    i8 = i7 + 40 | 0;
    i18 = HEAP32[i8 >> 2] | 0;
    HEAP32[i8 >> 2] = i10;
    do {
     if ((i29 | 0) == 42) {
      if ((HEAP32[i7 + 24 >> 2] | 0) != 2) {
       i17 = (HEAP32[i7 + 48 >> 2] << 12) + -30720 | 0;
       if ((HEAP32[i7 + 136 >> 2] | 0) <= 1 ? (i28 = HEAP32[i7 + 132 >> 2] | 0, (i28 | 0) >= 2) : 0) {
        if ((i28 | 0) < 6) {
         i28 = 64;
        } else {
         i28 = (i28 | 0) == 6 ? 128 : 192;
        }
       } else {
        i28 = 0;
       }
       i28 = i28 | i17;
       i17 = i7 + 108 | 0;
       i37 = (HEAP32[i17 >> 2] | 0) == 0 ? i28 : i28 | 32;
       HEAP32[i11 >> 2] = 113;
       i29 = i7 + 20 | 0;
       i30 = HEAP32[i29 >> 2] | 0;
       HEAP32[i29 >> 2] = i30 + 1;
       i28 = i7 + 8 | 0;
       HEAP8[(HEAP32[i28 >> 2] | 0) + i30 | 0] = i37 >>> 8;
       i30 = HEAP32[i29 >> 2] | 0;
       HEAP32[i29 >> 2] = i30 + 1;
       HEAP8[(HEAP32[i28 >> 2] | 0) + i30 | 0] = (i37 | ((i37 >>> 0) % 31 | 0)) ^ 31;
       i30 = i2 + 48 | 0;
       if ((HEAP32[i17 >> 2] | 0) != 0) {
        i37 = HEAP32[i30 >> 2] | 0;
        i36 = HEAP32[i29 >> 2] | 0;
        HEAP32[i29 >> 2] = i36 + 1;
        HEAP8[(HEAP32[i28 >> 2] | 0) + i36 | 0] = i37 >>> 24;
        i36 = HEAP32[i29 >> 2] | 0;
        HEAP32[i29 >> 2] = i36 + 1;
        HEAP8[(HEAP32[i28 >> 2] | 0) + i36 | 0] = i37 >>> 16;
        i36 = HEAP32[i30 >> 2] | 0;
        i37 = HEAP32[i29 >> 2] | 0;
        HEAP32[i29 >> 2] = i37 + 1;
        HEAP8[(HEAP32[i28 >> 2] | 0) + i37 | 0] = i36 >>> 8;
        i37 = HEAP32[i29 >> 2] | 0;
        HEAP32[i29 >> 2] = i37 + 1;
        HEAP8[(HEAP32[i28 >> 2] | 0) + i37 | 0] = i36;
       }
       HEAP32[i30 >> 2] = _adler32(0, 0, 0) | 0;
       i31 = HEAP32[i11 >> 2] | 0;
       i17 = 32;
       break;
      }
      i32 = i2 + 48 | 0;
      HEAP32[i32 >> 2] = _crc32(0, 0, 0) | 0;
      i30 = i7 + 20 | 0;
      i28 = HEAP32[i30 >> 2] | 0;
      HEAP32[i30 >> 2] = i28 + 1;
      i29 = i7 + 8 | 0;
      HEAP8[(HEAP32[i29 >> 2] | 0) + i28 | 0] = 31;
      i28 = HEAP32[i30 >> 2] | 0;
      HEAP32[i30 >> 2] = i28 + 1;
      HEAP8[(HEAP32[i29 >> 2] | 0) + i28 | 0] = -117;
      i28 = HEAP32[i30 >> 2] | 0;
      HEAP32[i30 >> 2] = i28 + 1;
      HEAP8[(HEAP32[i29 >> 2] | 0) + i28 | 0] = 8;
      i28 = i7 + 28 | 0;
      i33 = HEAP32[i28 >> 2] | 0;
      if ((i33 | 0) == 0) {
       i22 = HEAP32[i30 >> 2] | 0;
       HEAP32[i30 >> 2] = i22 + 1;
       HEAP8[(HEAP32[i29 >> 2] | 0) + i22 | 0] = 0;
       i22 = HEAP32[i30 >> 2] | 0;
       HEAP32[i30 >> 2] = i22 + 1;
       HEAP8[(HEAP32[i29 >> 2] | 0) + i22 | 0] = 0;
       i22 = HEAP32[i30 >> 2] | 0;
       HEAP32[i30 >> 2] = i22 + 1;
       HEAP8[(HEAP32[i29 >> 2] | 0) + i22 | 0] = 0;
       i22 = HEAP32[i30 >> 2] | 0;
       HEAP32[i30 >> 2] = i22 + 1;
       HEAP8[(HEAP32[i29 >> 2] | 0) + i22 | 0] = 0;
       i22 = HEAP32[i30 >> 2] | 0;
       HEAP32[i30 >> 2] = i22 + 1;
       HEAP8[(HEAP32[i29 >> 2] | 0) + i22 | 0] = 0;
       i22 = HEAP32[i7 + 132 >> 2] | 0;
       if ((i22 | 0) != 9) {
        if ((HEAP32[i7 + 136 >> 2] | 0) > 1) {
         i22 = 4;
        } else {
         i22 = (i22 | 0) < 2 ? 4 : 0;
        }
       } else {
        i22 = 2;
       }
       i37 = HEAP32[i30 >> 2] | 0;
       HEAP32[i30 >> 2] = i37 + 1;
       HEAP8[(HEAP32[i29 >> 2] | 0) + i37 | 0] = i22;
       i37 = HEAP32[i30 >> 2] | 0;
       HEAP32[i30 >> 2] = i37 + 1;
       HEAP8[(HEAP32[i29 >> 2] | 0) + i37 | 0] = 3;
       HEAP32[i11 >> 2] = 113;
       break;
      }
      i37 = (((HEAP32[i33 + 44 >> 2] | 0) != 0 ? 2 : 0) | (HEAP32[i33 >> 2] | 0) != 0 | ((HEAP32[i33 + 16 >> 2] | 0) == 0 ? 0 : 4) | ((HEAP32[i33 + 28 >> 2] | 0) == 0 ? 0 : 8) | ((HEAP32[i33 + 36 >> 2] | 0) == 0 ? 0 : 16)) & 255;
      i17 = HEAP32[i30 >> 2] | 0;
      HEAP32[i30 >> 2] = i17 + 1;
      HEAP8[(HEAP32[i29 >> 2] | 0) + i17 | 0] = i37;
      i17 = HEAP32[(HEAP32[i28 >> 2] | 0) + 4 >> 2] & 255;
      i37 = HEAP32[i30 >> 2] | 0;
      HEAP32[i30 >> 2] = i37 + 1;
      HEAP8[(HEAP32[i29 >> 2] | 0) + i37 | 0] = i17;
      i37 = (HEAP32[(HEAP32[i28 >> 2] | 0) + 4 >> 2] | 0) >>> 8 & 255;
      i17 = HEAP32[i30 >> 2] | 0;
      HEAP32[i30 >> 2] = i17 + 1;
      HEAP8[(HEAP32[i29 >> 2] | 0) + i17 | 0] = i37;
      i17 = (HEAP32[(HEAP32[i28 >> 2] | 0) + 4 >> 2] | 0) >>> 16 & 255;
      i37 = HEAP32[i30 >> 2] | 0;
      HEAP32[i30 >> 2] = i37 + 1;
      HEAP8[(HEAP32[i29 >> 2] | 0) + i37 | 0] = i17;
      i37 = (HEAP32[(HEAP32[i28 >> 2] | 0) + 4 >> 2] | 0) >>> 24 & 255;
      i17 = HEAP32[i30 >> 2] | 0;
      HEAP32[i30 >> 2] = i17 + 1;
      HEAP8[(HEAP32[i29 >> 2] | 0) + i17 | 0] = i37;
      i17 = HEAP32[i7 + 132 >> 2] | 0;
      if ((i17 | 0) != 9) {
       if ((HEAP32[i7 + 136 >> 2] | 0) > 1) {
        i17 = 4;
       } else {
        i17 = (i17 | 0) < 2 ? 4 : 0;
       }
      } else {
       i17 = 2;
      }
      i37 = HEAP32[i30 >> 2] | 0;
      HEAP32[i30 >> 2] = i37 + 1;
      HEAP8[(HEAP32[i29 >> 2] | 0) + i37 | 0] = i17;
      i37 = HEAP32[(HEAP32[i28 >> 2] | 0) + 12 >> 2] & 255;
      i17 = HEAP32[i30 >> 2] | 0;
      HEAP32[i30 >> 2] = i17 + 1;
      HEAP8[(HEAP32[i29 >> 2] | 0) + i17 | 0] = i37;
      i17 = HEAP32[i28 >> 2] | 0;
      if ((HEAP32[i17 + 16 >> 2] | 0) != 0) {
       i17 = HEAP32[i17 + 20 >> 2] & 255;
       i37 = HEAP32[i30 >> 2] | 0;
       HEAP32[i30 >> 2] = i37 + 1;
       HEAP8[(HEAP32[i29 >> 2] | 0) + i37 | 0] = i17;
       i37 = (HEAP32[(HEAP32[i28 >> 2] | 0) + 20 >> 2] | 0) >>> 8 & 255;
       i17 = HEAP32[i30 >> 2] | 0;
       HEAP32[i30 >> 2] = i17 + 1;
       HEAP8[(HEAP32[i29 >> 2] | 0) + i17 | 0] = i37;
       i17 = HEAP32[i28 >> 2] | 0;
      }
      if ((HEAP32[i17 + 44 >> 2] | 0) != 0) {
       HEAP32[i32 >> 2] = _crc32(HEAP32[i32 >> 2] | 0, HEAP32[i29 >> 2] | 0, HEAP32[i30 >> 2] | 0) | 0;
      }
      HEAP32[i7 + 32 >> 2] = 0;
      HEAP32[i11 >> 2] = 69;
      i17 = 34;
     } else {
      i31 = i29;
      i17 = 32;
     }
    } while (0);
    if ((i17 | 0) == 32) {
     if ((i31 | 0) == 69) {
      i28 = i7 + 28 | 0;
      i17 = 34;
     } else {
      i17 = 55;
     }
    }
    do {
     if ((i17 | 0) == 34) {
      i37 = HEAP32[i28 >> 2] | 0;
      if ((HEAP32[i37 + 16 >> 2] | 0) == 0) {
       HEAP32[i11 >> 2] = 73;
       i17 = 57;
       break;
      }
      i29 = i7 + 20 | 0;
      i34 = HEAP32[i29 >> 2] | 0;
      i17 = i7 + 32 | 0;
      i36 = HEAP32[i17 >> 2] | 0;
      L55 : do {
       if (i36 >>> 0 < (HEAP32[i37 + 20 >> 2] & 65535) >>> 0) {
        i30 = i7 + 12 | 0;
        i32 = i2 + 48 | 0;
        i31 = i7 + 8 | 0;
        i33 = i2 + 20 | 0;
        i35 = i34;
        while (1) {
         if ((i35 | 0) == (HEAP32[i30 >> 2] | 0)) {
          if ((HEAP32[i37 + 44 >> 2] | 0) != 0 & i35 >>> 0 > i34 >>> 0) {
           HEAP32[i32 >> 2] = _crc32(HEAP32[i32 >> 2] | 0, (HEAP32[i31 >> 2] | 0) + i34 | 0, i35 - i34 | 0) | 0;
          }
          i34 = HEAP32[i5 >> 2] | 0;
          i35 = HEAP32[i34 + 20 >> 2] | 0;
          i36 = HEAP32[i3 >> 2] | 0;
          i35 = i35 >>> 0 > i36 >>> 0 ? i36 : i35;
          if ((i35 | 0) != 0 ? (_memcpy(HEAP32[i4 >> 2] | 0, HEAP32[i34 + 16 >> 2] | 0, i35 | 0) | 0, HEAP32[i4 >> 2] = (HEAP32[i4 >> 2] | 0) + i35, i27 = (HEAP32[i5 >> 2] | 0) + 16 | 0, HEAP32[i27 >> 2] = (HEAP32[i27 >> 2] | 0) + i35, HEAP32[i33 >> 2] = (HEAP32[i33 >> 2] | 0) + i35, HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) - i35, i27 = HEAP32[i5 >> 2] | 0, i36 = i27 + 20 | 0, i37 = HEAP32[i36 >> 2] | 0, HEAP32[i36 >> 2] = i37 - i35, (i37 | 0) == (i35 | 0)) : 0) {
           HEAP32[i27 + 16 >> 2] = HEAP32[i27 + 8 >> 2];
          }
          i34 = HEAP32[i29 >> 2] | 0;
          if ((i34 | 0) == (HEAP32[i30 >> 2] | 0)) {
           break;
          }
          i37 = HEAP32[i28 >> 2] | 0;
          i36 = HEAP32[i17 >> 2] | 0;
          i35 = i34;
         }
         i36 = HEAP8[(HEAP32[i37 + 16 >> 2] | 0) + i36 | 0] | 0;
         HEAP32[i29 >> 2] = i35 + 1;
         HEAP8[(HEAP32[i31 >> 2] | 0) + i35 | 0] = i36;
         i36 = (HEAP32[i17 >> 2] | 0) + 1 | 0;
         HEAP32[i17 >> 2] = i36;
         i37 = HEAP32[i28 >> 2] | 0;
         if (!(i36 >>> 0 < (HEAP32[i37 + 20 >> 2] & 65535) >>> 0)) {
          break L55;
         }
         i35 = HEAP32[i29 >> 2] | 0;
        }
        i37 = HEAP32[i28 >> 2] | 0;
       }
      } while (0);
      if ((HEAP32[i37 + 44 >> 2] | 0) != 0 ? (i26 = HEAP32[i29 >> 2] | 0, i26 >>> 0 > i34 >>> 0) : 0) {
       i37 = i2 + 48 | 0;
       HEAP32[i37 >> 2] = _crc32(HEAP32[i37 >> 2] | 0, (HEAP32[i7 + 8 >> 2] | 0) + i34 | 0, i26 - i34 | 0) | 0;
       i37 = HEAP32[i28 >> 2] | 0;
      }
      if ((HEAP32[i17 >> 2] | 0) == (HEAP32[i37 + 20 >> 2] | 0)) {
       HEAP32[i17 >> 2] = 0;
       HEAP32[i11 >> 2] = 73;
       i17 = 57;
       break;
      } else {
       i31 = HEAP32[i11 >> 2] | 0;
       i17 = 55;
       break;
      }
     }
    } while (0);
    if ((i17 | 0) == 55) {
     if ((i31 | 0) == 73) {
      i37 = HEAP32[i7 + 28 >> 2] | 0;
      i17 = 57;
     } else {
      i17 = 76;
     }
    }
    do {
     if ((i17 | 0) == 57) {
      i26 = i7 + 28 | 0;
      if ((HEAP32[i37 + 28 >> 2] | 0) == 0) {
       HEAP32[i11 >> 2] = 91;
       i17 = 78;
       break;
      }
      i27 = i7 + 20 | 0;
      i35 = HEAP32[i27 >> 2] | 0;
      i32 = i7 + 12 | 0;
      i29 = i2 + 48 | 0;
      i28 = i7 + 8 | 0;
      i31 = i2 + 20 | 0;
      i30 = i7 + 32 | 0;
      i33 = i35;
      while (1) {
       if ((i33 | 0) == (HEAP32[i32 >> 2] | 0)) {
        if ((HEAP32[(HEAP32[i26 >> 2] | 0) + 44 >> 2] | 0) != 0 & i33 >>> 0 > i35 >>> 0) {
         HEAP32[i29 >> 2] = _crc32(HEAP32[i29 >> 2] | 0, (HEAP32[i28 >> 2] | 0) + i35 | 0, i33 - i35 | 0) | 0;
        }
        i33 = HEAP32[i5 >> 2] | 0;
        i34 = HEAP32[i33 + 20 >> 2] | 0;
        i35 = HEAP32[i3 >> 2] | 0;
        i34 = i34 >>> 0 > i35 >>> 0 ? i35 : i34;
        if ((i34 | 0) != 0 ? (_memcpy(HEAP32[i4 >> 2] | 0, HEAP32[i33 + 16 >> 2] | 0, i34 | 0) | 0, HEAP32[i4 >> 2] = (HEAP32[i4 >> 2] | 0) + i34, i25 = (HEAP32[i5 >> 2] | 0) + 16 | 0, HEAP32[i25 >> 2] = (HEAP32[i25 >> 2] | 0) + i34, HEAP32[i31 >> 2] = (HEAP32[i31 >> 2] | 0) + i34, HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) - i34, i25 = HEAP32[i5 >> 2] | 0, i36 = i25 + 20 | 0, i37 = HEAP32[i36 >> 2] | 0, HEAP32[i36 >> 2] = i37 - i34, (i37 | 0) == (i34 | 0)) : 0) {
         HEAP32[i25 + 16 >> 2] = HEAP32[i25 + 8 >> 2];
        }
        i35 = HEAP32[i27 >> 2] | 0;
        if ((i35 | 0) == (HEAP32[i32 >> 2] | 0)) {
         i25 = 1;
         break;
        } else {
         i33 = i35;
        }
       }
       i34 = HEAP32[i30 >> 2] | 0;
       HEAP32[i30 >> 2] = i34 + 1;
       i34 = HEAP8[(HEAP32[(HEAP32[i26 >> 2] | 0) + 28 >> 2] | 0) + i34 | 0] | 0;
       HEAP32[i27 >> 2] = i33 + 1;
       HEAP8[(HEAP32[i28 >> 2] | 0) + i33 | 0] = i34;
       if (i34 << 24 >> 24 == 0) {
        i17 = 68;
        break;
       }
       i33 = HEAP32[i27 >> 2] | 0;
      }
      if ((i17 | 0) == 68) {
       i25 = i34 & 255;
      }
      if ((HEAP32[(HEAP32[i26 >> 2] | 0) + 44 >> 2] | 0) != 0 ? (i24 = HEAP32[i27 >> 2] | 0, i24 >>> 0 > i35 >>> 0) : 0) {
       HEAP32[i29 >> 2] = _crc32(HEAP32[i29 >> 2] | 0, (HEAP32[i28 >> 2] | 0) + i35 | 0, i24 - i35 | 0) | 0;
      }
      if ((i25 | 0) == 0) {
       HEAP32[i30 >> 2] = 0;
       HEAP32[i11 >> 2] = 91;
       i17 = 78;
       break;
      } else {
       i31 = HEAP32[i11 >> 2] | 0;
       i17 = 76;
       break;
      }
     }
    } while (0);
    if ((i17 | 0) == 76) {
     if ((i31 | 0) == 91) {
      i26 = i7 + 28 | 0;
      i17 = 78;
     } else {
      i17 = 97;
     }
    }
    do {
     if ((i17 | 0) == 78) {
      if ((HEAP32[(HEAP32[i26 >> 2] | 0) + 36 >> 2] | 0) == 0) {
       HEAP32[i11 >> 2] = 103;
       i17 = 99;
       break;
      }
      i24 = i7 + 20 | 0;
      i32 = HEAP32[i24 >> 2] | 0;
      i29 = i7 + 12 | 0;
      i27 = i2 + 48 | 0;
      i25 = i7 + 8 | 0;
      i28 = i2 + 20 | 0;
      i30 = i7 + 32 | 0;
      i31 = i32;
      while (1) {
       if ((i31 | 0) == (HEAP32[i29 >> 2] | 0)) {
        if ((HEAP32[(HEAP32[i26 >> 2] | 0) + 44 >> 2] | 0) != 0 & i31 >>> 0 > i32 >>> 0) {
         HEAP32[i27 >> 2] = _crc32(HEAP32[i27 >> 2] | 0, (HEAP32[i25 >> 2] | 0) + i32 | 0, i31 - i32 | 0) | 0;
        }
        i31 = HEAP32[i5 >> 2] | 0;
        i33 = HEAP32[i31 + 20 >> 2] | 0;
        i32 = HEAP32[i3 >> 2] | 0;
        i32 = i33 >>> 0 > i32 >>> 0 ? i32 : i33;
        if ((i32 | 0) != 0 ? (_memcpy(HEAP32[i4 >> 2] | 0, HEAP32[i31 + 16 >> 2] | 0, i32 | 0) | 0, HEAP32[i4 >> 2] = (HEAP32[i4 >> 2] | 0) + i32, i23 = (HEAP32[i5 >> 2] | 0) + 16 | 0, HEAP32[i23 >> 2] = (HEAP32[i23 >> 2] | 0) + i32, HEAP32[i28 >> 2] = (HEAP32[i28 >> 2] | 0) + i32, HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) - i32, i23 = HEAP32[i5 >> 2] | 0, i36 = i23 + 20 | 0, i37 = HEAP32[i36 >> 2] | 0, HEAP32[i36 >> 2] = i37 - i32, (i37 | 0) == (i32 | 0)) : 0) {
         HEAP32[i23 + 16 >> 2] = HEAP32[i23 + 8 >> 2];
        }
        i32 = HEAP32[i24 >> 2] | 0;
        if ((i32 | 0) == (HEAP32[i29 >> 2] | 0)) {
         i23 = 1;
         break;
        } else {
         i31 = i32;
        }
       }
       i33 = HEAP32[i30 >> 2] | 0;
       HEAP32[i30 >> 2] = i33 + 1;
       i33 = HEAP8[(HEAP32[(HEAP32[i26 >> 2] | 0) + 36 >> 2] | 0) + i33 | 0] | 0;
       HEAP32[i24 >> 2] = i31 + 1;
       HEAP8[(HEAP32[i25 >> 2] | 0) + i31 | 0] = i33;
       if (i33 << 24 >> 24 == 0) {
        i17 = 89;
        break;
       }
       i31 = HEAP32[i24 >> 2] | 0;
      }
      if ((i17 | 0) == 89) {
       i23 = i33 & 255;
      }
      if ((HEAP32[(HEAP32[i26 >> 2] | 0) + 44 >> 2] | 0) != 0 ? (i22 = HEAP32[i24 >> 2] | 0, i22 >>> 0 > i32 >>> 0) : 0) {
       HEAP32[i27 >> 2] = _crc32(HEAP32[i27 >> 2] | 0, (HEAP32[i25 >> 2] | 0) + i32 | 0, i22 - i32 | 0) | 0;
      }
      if ((i23 | 0) == 0) {
       HEAP32[i11 >> 2] = 103;
       i17 = 99;
       break;
      } else {
       i31 = HEAP32[i11 >> 2] | 0;
       i17 = 97;
       break;
      }
     }
    } while (0);
    if ((i17 | 0) == 97 ? (i31 | 0) == 103 : 0) {
     i26 = i7 + 28 | 0;
     i17 = 99;
    }
    do {
     if ((i17 | 0) == 99) {
      if ((HEAP32[(HEAP32[i26 >> 2] | 0) + 44 >> 2] | 0) == 0) {
       HEAP32[i11 >> 2] = 113;
       break;
      }
      i17 = i7 + 20 | 0;
      i22 = i7 + 12 | 0;
      if ((((HEAP32[i17 >> 2] | 0) + 2 | 0) >>> 0 > (HEAP32[i22 >> 2] | 0) >>> 0 ? (i20 = HEAP32[i5 >> 2] | 0, i21 = HEAP32[i20 + 20 >> 2] | 0, i23 = HEAP32[i3 >> 2] | 0, i21 = i21 >>> 0 > i23 >>> 0 ? i23 : i21, (i21 | 0) != 0) : 0) ? (_memcpy(HEAP32[i4 >> 2] | 0, HEAP32[i20 + 16 >> 2] | 0, i21 | 0) | 0, HEAP32[i4 >> 2] = (HEAP32[i4 >> 2] | 0) + i21, i19 = (HEAP32[i5 >> 2] | 0) + 16 | 0, HEAP32[i19 >> 2] = (HEAP32[i19 >> 2] | 0) + i21, i19 = i2 + 20 | 0, HEAP32[i19 >> 2] = (HEAP32[i19 >> 2] | 0) + i21, HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) - i21, i19 = HEAP32[i5 >> 2] | 0, i36 = i19 + 20 | 0, i37 = HEAP32[i36 >> 2] | 0, HEAP32[i36 >> 2] = i37 - i21, (i37 | 0) == (i21 | 0)) : 0) {
       HEAP32[i19 + 16 >> 2] = HEAP32[i19 + 8 >> 2];
      }
      i19 = HEAP32[i17 >> 2] | 0;
      if (!((i19 + 2 | 0) >>> 0 > (HEAP32[i22 >> 2] | 0) >>> 0)) {
       i37 = i2 + 48 | 0;
       i34 = HEAP32[i37 >> 2] & 255;
       HEAP32[i17 >> 2] = i19 + 1;
       i35 = i7 + 8 | 0;
       HEAP8[(HEAP32[i35 >> 2] | 0) + i19 | 0] = i34;
       i34 = (HEAP32[i37 >> 2] | 0) >>> 8 & 255;
       i36 = HEAP32[i17 >> 2] | 0;
       HEAP32[i17 >> 2] = i36 + 1;
       HEAP8[(HEAP32[i35 >> 2] | 0) + i36 | 0] = i34;
       HEAP32[i37 >> 2] = _crc32(0, 0, 0) | 0;
       HEAP32[i11 >> 2] = 113;
      }
     }
    } while (0);
    i19 = i7 + 20 | 0;
    if ((HEAP32[i19 >> 2] | 0) == 0) {
     if ((HEAP32[i2 + 4 >> 2] | 0) == 0 ? (i18 | 0) >= (i10 | 0) & (i10 | 0) != 4 : 0) {
      HEAP32[i2 + 24 >> 2] = HEAP32[3180 >> 2];
      i37 = -5;
      STACKTOP = i1;
      return i37 | 0;
     }
    } else {
     i17 = HEAP32[i5 >> 2] | 0;
     i20 = HEAP32[i17 + 20 >> 2] | 0;
     i18 = HEAP32[i3 >> 2] | 0;
     i20 = i20 >>> 0 > i18 >>> 0 ? i18 : i20;
     if ((i20 | 0) != 0) {
      _memcpy(HEAP32[i4 >> 2] | 0, HEAP32[i17 + 16 >> 2] | 0, i20 | 0) | 0;
      HEAP32[i4 >> 2] = (HEAP32[i4 >> 2] | 0) + i20;
      i17 = (HEAP32[i5 >> 2] | 0) + 16 | 0;
      HEAP32[i17 >> 2] = (HEAP32[i17 >> 2] | 0) + i20;
      i17 = i2 + 20 | 0;
      HEAP32[i17 >> 2] = (HEAP32[i17 >> 2] | 0) + i20;
      HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) - i20;
      i17 = HEAP32[i5 >> 2] | 0;
      i36 = i17 + 20 | 0;
      i37 = HEAP32[i36 >> 2] | 0;
      HEAP32[i36 >> 2] = i37 - i20;
      if ((i37 | 0) == (i20 | 0)) {
       HEAP32[i17 + 16 >> 2] = HEAP32[i17 + 8 >> 2];
      }
      i18 = HEAP32[i3 >> 2] | 0;
     }
     if ((i18 | 0) == 0) {
      HEAP32[i8 >> 2] = -1;
      i37 = 0;
      STACKTOP = i1;
      return i37 | 0;
     }
    }
    i18 = (HEAP32[i11 >> 2] | 0) == 666;
    i17 = (HEAP32[i2 + 4 >> 2] | 0) == 0;
    if (i18) {
     if (i17) {
      i17 = 121;
     } else {
      HEAP32[i2 + 24 >> 2] = HEAP32[3180 >> 2];
      i37 = -5;
      STACKTOP = i1;
      return i37 | 0;
     }
    } else {
     if (i17) {
      i17 = 121;
     } else {
      i17 = 124;
     }
    }
    do {
     if ((i17 | 0) == 121) {
      if ((HEAP32[i7 + 116 >> 2] | 0) == 0) {
       if ((i10 | 0) != 0) {
        if (i18) {
         break;
        } else {
         i17 = 124;
         break;
        }
       } else {
        i37 = 0;
        STACKTOP = i1;
        return i37 | 0;
       }
      } else {
       i17 = 124;
      }
     }
    } while (0);
    do {
     if ((i17 | 0) == 124) {
      i18 = HEAP32[i7 + 136 >> 2] | 0;
      L185 : do {
       if ((i18 | 0) == 2) {
        i22 = i7 + 116 | 0;
        i18 = i7 + 96 | 0;
        i13 = i7 + 108 | 0;
        i14 = i7 + 56 | 0;
        i21 = i7 + 5792 | 0;
        i20 = i7 + 5796 | 0;
        i24 = i7 + 5784 | 0;
        i23 = i7 + 5788 | 0;
        i12 = i7 + 92 | 0;
        while (1) {
         if ((HEAP32[i22 >> 2] | 0) == 0 ? (_fill_window(i7), (HEAP32[i22 >> 2] | 0) == 0) : 0) {
          break;
         }
         HEAP32[i18 >> 2] = 0;
         i37 = HEAP8[(HEAP32[i14 >> 2] | 0) + (HEAP32[i13 >> 2] | 0) | 0] | 0;
         i26 = HEAP32[i21 >> 2] | 0;
         HEAP16[(HEAP32[i20 >> 2] | 0) + (i26 << 1) >> 1] = 0;
         HEAP32[i21 >> 2] = i26 + 1;
         HEAP8[(HEAP32[i24 >> 2] | 0) + i26 | 0] = i37;
         i37 = i7 + ((i37 & 255) << 2) + 148 | 0;
         HEAP16[i37 >> 1] = (HEAP16[i37 >> 1] | 0) + 1 << 16 >> 16;
         i37 = (HEAP32[i21 >> 2] | 0) == ((HEAP32[i23 >> 2] | 0) + -1 | 0);
         HEAP32[i22 >> 2] = (HEAP32[i22 >> 2] | 0) + -1;
         i26 = (HEAP32[i13 >> 2] | 0) + 1 | 0;
         HEAP32[i13 >> 2] = i26;
         if (!i37) {
          continue;
         }
         i25 = HEAP32[i12 >> 2] | 0;
         if ((i25 | 0) > -1) {
          i27 = (HEAP32[i14 >> 2] | 0) + i25 | 0;
         } else {
          i27 = 0;
         }
         __tr_flush_block(i7, i27, i26 - i25 | 0, 0);
         HEAP32[i12 >> 2] = HEAP32[i13 >> 2];
         i26 = HEAP32[i7 >> 2] | 0;
         i25 = i26 + 28 | 0;
         i27 = HEAP32[i25 >> 2] | 0;
         i30 = HEAP32[i27 + 20 >> 2] | 0;
         i28 = i26 + 16 | 0;
         i29 = HEAP32[i28 >> 2] | 0;
         i29 = i30 >>> 0 > i29 >>> 0 ? i29 : i30;
         if ((i29 | 0) != 0 ? (i16 = i26 + 12 | 0, _memcpy(HEAP32[i16 >> 2] | 0, HEAP32[i27 + 16 >> 2] | 0, i29 | 0) | 0, HEAP32[i16 >> 2] = (HEAP32[i16 >> 2] | 0) + i29, i16 = (HEAP32[i25 >> 2] | 0) + 16 | 0, HEAP32[i16 >> 2] = (HEAP32[i16 >> 2] | 0) + i29, i16 = i26 + 20 | 0, HEAP32[i16 >> 2] = (HEAP32[i16 >> 2] | 0) + i29, HEAP32[i28 >> 2] = (HEAP32[i28 >> 2] | 0) - i29, i16 = HEAP32[i25 >> 2] | 0, i36 = i16 + 20 | 0, i37 = HEAP32[i36 >> 2] | 0, HEAP32[i36 >> 2] = i37 - i29, (i37 | 0) == (i29 | 0)) : 0) {
          HEAP32[i16 + 16 >> 2] = HEAP32[i16 + 8 >> 2];
         }
         if ((HEAP32[(HEAP32[i7 >> 2] | 0) + 16 >> 2] | 0) == 0) {
          break L185;
         }
        }
        if ((i10 | 0) != 0) {
         i16 = HEAP32[i12 >> 2] | 0;
         if ((i16 | 0) > -1) {
          i14 = (HEAP32[i14 >> 2] | 0) + i16 | 0;
         } else {
          i14 = 0;
         }
         __tr_flush_block(i7, i14, (HEAP32[i13 >> 2] | 0) - i16 | 0, i9 & 1);
         HEAP32[i12 >> 2] = HEAP32[i13 >> 2];
         i14 = HEAP32[i7 >> 2] | 0;
         i13 = i14 + 28 | 0;
         i12 = HEAP32[i13 >> 2] | 0;
         i17 = HEAP32[i12 + 20 >> 2] | 0;
         i16 = i14 + 16 | 0;
         i18 = HEAP32[i16 >> 2] | 0;
         i17 = i17 >>> 0 > i18 >>> 0 ? i18 : i17;
         if ((i17 | 0) != 0 ? (i15 = i14 + 12 | 0, _memcpy(HEAP32[i15 >> 2] | 0, HEAP32[i12 + 16 >> 2] | 0, i17 | 0) | 0, HEAP32[i15 >> 2] = (HEAP32[i15 >> 2] | 0) + i17, i15 = (HEAP32[i13 >> 2] | 0) + 16 | 0, HEAP32[i15 >> 2] = (HEAP32[i15 >> 2] | 0) + i17, i15 = i14 + 20 | 0, HEAP32[i15 >> 2] = (HEAP32[i15 >> 2] | 0) + i17, HEAP32[i16 >> 2] = (HEAP32[i16 >> 2] | 0) - i17, i15 = HEAP32[i13 >> 2] | 0, i36 = i15 + 20 | 0, i37 = HEAP32[i36 >> 2] | 0, HEAP32[i36 >> 2] = i37 - i17, (i37 | 0) == (i17 | 0)) : 0) {
          HEAP32[i15 + 16 >> 2] = HEAP32[i15 + 8 >> 2];
         }
         if ((HEAP32[(HEAP32[i7 >> 2] | 0) + 16 >> 2] | 0) == 0) {
          i12 = i9 ? 2 : 0;
          i17 = 183;
          break;
         } else {
          i12 = i9 ? 3 : 1;
          i17 = 183;
          break;
         }
        }
       } else if ((i18 | 0) == 3) {
        i27 = i7 + 116 | 0;
        i26 = (i10 | 0) == 0;
        i22 = i7 + 96 | 0;
        i15 = i7 + 108 | 0;
        i20 = i7 + 5792 | 0;
        i24 = i7 + 5796 | 0;
        i23 = i7 + 5784 | 0;
        i21 = i7 + (HEAPU8[296] << 2) + 2440 | 0;
        i25 = i7 + 5788 | 0;
        i18 = i7 + 56 | 0;
        i16 = i7 + 92 | 0;
        while (1) {
         i29 = HEAP32[i27 >> 2] | 0;
         if (i29 >>> 0 < 258) {
          _fill_window(i7);
          i29 = HEAP32[i27 >> 2] | 0;
          if (i29 >>> 0 < 258 & i26) {
           break L185;
          }
          if ((i29 | 0) == 0) {
           break;
          }
          HEAP32[i22 >> 2] = 0;
          if (i29 >>> 0 > 2) {
           i17 = 151;
          } else {
           i28 = HEAP32[i15 >> 2] | 0;
           i17 = 166;
          }
         } else {
          HEAP32[i22 >> 2] = 0;
          i17 = 151;
         }
         if ((i17 | 0) == 151) {
          i17 = 0;
          i28 = HEAP32[i15 >> 2] | 0;
          if ((i28 | 0) != 0) {
           i31 = HEAP32[i18 >> 2] | 0;
           i30 = HEAP8[i31 + (i28 + -1) | 0] | 0;
           if ((i30 << 24 >> 24 == (HEAP8[i31 + i28 | 0] | 0) ? i30 << 24 >> 24 == (HEAP8[i31 + (i28 + 1) | 0] | 0) : 0) ? (i14 = i31 + (i28 + 2) | 0, i30 << 24 >> 24 == (HEAP8[i14] | 0)) : 0) {
            i31 = i31 + (i28 + 258) | 0;
            i32 = i14;
            do {
             i33 = i32 + 1 | 0;
             if (!(i30 << 24 >> 24 == (HEAP8[i33] | 0))) {
              i32 = i33;
              break;
             }
             i33 = i32 + 2 | 0;
             if (!(i30 << 24 >> 24 == (HEAP8[i33] | 0))) {
              i32 = i33;
              break;
             }
             i33 = i32 + 3 | 0;
             if (!(i30 << 24 >> 24 == (HEAP8[i33] | 0))) {
              i32 = i33;
              break;
             }
             i33 = i32 + 4 | 0;
             if (!(i30 << 24 >> 24 == (HEAP8[i33] | 0))) {
              i32 = i33;
              break;
             }
             i33 = i32 + 5 | 0;
             if (!(i30 << 24 >> 24 == (HEAP8[i33] | 0))) {
              i32 = i33;
              break;
             }
             i33 = i32 + 6 | 0;
             if (!(i30 << 24 >> 24 == (HEAP8[i33] | 0))) {
              i32 = i33;
              break;
             }
             i33 = i32 + 7 | 0;
             if (!(i30 << 24 >> 24 == (HEAP8[i33] | 0))) {
              i32 = i33;
              break;
             }
             i32 = i32 + 8 | 0;
            } while (i30 << 24 >> 24 == (HEAP8[i32] | 0) & i32 >>> 0 < i31 >>> 0);
            i30 = i32 - i31 + 258 | 0;
            i29 = i30 >>> 0 > i29 >>> 0 ? i29 : i30;
            HEAP32[i22 >> 2] = i29;
            if (i29 >>> 0 > 2) {
             i29 = i29 + 253 | 0;
             i28 = HEAP32[i20 >> 2] | 0;
             HEAP16[(HEAP32[i24 >> 2] | 0) + (i28 << 1) >> 1] = 1;
             HEAP32[i20 >> 2] = i28 + 1;
             HEAP8[(HEAP32[i23 >> 2] | 0) + i28 | 0] = i29;
             i29 = i7 + ((HEAPU8[808 + (i29 & 255) | 0] | 256) + 1 << 2) + 148 | 0;
             HEAP16[i29 >> 1] = (HEAP16[i29 >> 1] | 0) + 1 << 16 >> 16;
             HEAP16[i21 >> 1] = (HEAP16[i21 >> 1] | 0) + 1 << 16 >> 16;
             i29 = (HEAP32[i20 >> 2] | 0) == ((HEAP32[i25 >> 2] | 0) + -1 | 0) | 0;
             i28 = HEAP32[i22 >> 2] | 0;
             HEAP32[i27 >> 2] = (HEAP32[i27 >> 2] | 0) - i28;
             i28 = (HEAP32[i15 >> 2] | 0) + i28 | 0;
             HEAP32[i15 >> 2] = i28;
             HEAP32[i22 >> 2] = 0;
            } else {
             i17 = 166;
            }
           } else {
            i17 = 166;
           }
          } else {
           i28 = 0;
           i17 = 166;
          }
         }
         if ((i17 | 0) == 166) {
          i17 = 0;
          i29 = HEAP8[(HEAP32[i18 >> 2] | 0) + i28 | 0] | 0;
          i28 = HEAP32[i20 >> 2] | 0;
          HEAP16[(HEAP32[i24 >> 2] | 0) + (i28 << 1) >> 1] = 0;
          HEAP32[i20 >> 2] = i28 + 1;
          HEAP8[(HEAP32[i23 >> 2] | 0) + i28 | 0] = i29;
          i29 = i7 + ((i29 & 255) << 2) + 148 | 0;
          HEAP16[i29 >> 1] = (HEAP16[i29 >> 1] | 0) + 1 << 16 >> 16;
          i29 = (HEAP32[i20 >> 2] | 0) == ((HEAP32[i25 >> 2] | 0) + -1 | 0) | 0;
          HEAP32[i27 >> 2] = (HEAP32[i27 >> 2] | 0) + -1;
          i28 = (HEAP32[i15 >> 2] | 0) + 1 | 0;
          HEAP32[i15 >> 2] = i28;
         }
         if ((i29 | 0) == 0) {
          continue;
         }
         i29 = HEAP32[i16 >> 2] | 0;
         if ((i29 | 0) > -1) {
          i30 = (HEAP32[i18 >> 2] | 0) + i29 | 0;
         } else {
          i30 = 0;
         }
         __tr_flush_block(i7, i30, i28 - i29 | 0, 0);
         HEAP32[i16 >> 2] = HEAP32[i15 >> 2];
         i30 = HEAP32[i7 >> 2] | 0;
         i28 = i30 + 28 | 0;
         i29 = HEAP32[i28 >> 2] | 0;
         i33 = HEAP32[i29 + 20 >> 2] | 0;
         i31 = i30 + 16 | 0;
         i32 = HEAP32[i31 >> 2] | 0;
         i32 = i33 >>> 0 > i32 >>> 0 ? i32 : i33;
         if ((i32 | 0) != 0 ? (i13 = i30 + 12 | 0, _memcpy(HEAP32[i13 >> 2] | 0, HEAP32[i29 + 16 >> 2] | 0, i32 | 0) | 0, HEAP32[i13 >> 2] = (HEAP32[i13 >> 2] | 0) + i32, i13 = (HEAP32[i28 >> 2] | 0) + 16 | 0, HEAP32[i13 >> 2] = (HEAP32[i13 >> 2] | 0) + i32, i13 = i30 + 20 | 0, HEAP32[i13 >> 2] = (HEAP32[i13 >> 2] | 0) + i32, HEAP32[i31 >> 2] = (HEAP32[i31 >> 2] | 0) - i32, i13 = HEAP32[i28 >> 2] | 0, i36 = i13 + 20 | 0, i37 = HEAP32[i36 >> 2] | 0, HEAP32[i36 >> 2] = i37 - i32, (i37 | 0) == (i32 | 0)) : 0) {
          HEAP32[i13 + 16 >> 2] = HEAP32[i13 + 8 >> 2];
         }
         if ((HEAP32[(HEAP32[i7 >> 2] | 0) + 16 >> 2] | 0) == 0) {
          break L185;
         }
        }
        i13 = HEAP32[i16 >> 2] | 0;
        if ((i13 | 0) > -1) {
         i14 = (HEAP32[i18 >> 2] | 0) + i13 | 0;
        } else {
         i14 = 0;
        }
        __tr_flush_block(i7, i14, (HEAP32[i15 >> 2] | 0) - i13 | 0, i9 & 1);
        HEAP32[i16 >> 2] = HEAP32[i15 >> 2];
        i14 = HEAP32[i7 >> 2] | 0;
        i16 = i14 + 28 | 0;
        i15 = HEAP32[i16 >> 2] | 0;
        i18 = HEAP32[i15 + 20 >> 2] | 0;
        i13 = i14 + 16 | 0;
        i17 = HEAP32[i13 >> 2] | 0;
        i17 = i18 >>> 0 > i17 >>> 0 ? i17 : i18;
        if ((i17 | 0) != 0 ? (i12 = i14 + 12 | 0, _memcpy(HEAP32[i12 >> 2] | 0, HEAP32[i15 + 16 >> 2] | 0, i17 | 0) | 0, HEAP32[i12 >> 2] = (HEAP32[i12 >> 2] | 0) + i17, i12 = (HEAP32[i16 >> 2] | 0) + 16 | 0, HEAP32[i12 >> 2] = (HEAP32[i12 >> 2] | 0) + i17, i12 = i14 + 20 | 0, HEAP32[i12 >> 2] = (HEAP32[i12 >> 2] | 0) + i17, HEAP32[i13 >> 2] = (HEAP32[i13 >> 2] | 0) - i17, i12 = HEAP32[i16 >> 2] | 0, i36 = i12 + 20 | 0, i37 = HEAP32[i36 >> 2] | 0, HEAP32[i36 >> 2] = i37 - i17, (i37 | 0) == (i17 | 0)) : 0) {
         HEAP32[i12 + 16 >> 2] = HEAP32[i12 + 8 >> 2];
        }
        if ((HEAP32[(HEAP32[i7 >> 2] | 0) + 16 >> 2] | 0) == 0) {
         i12 = i9 ? 2 : 0;
         i17 = 183;
         break;
        } else {
         i12 = i9 ? 3 : 1;
         i17 = 183;
         break;
        }
       } else {
        i12 = FUNCTION_TABLE_iii[HEAP32[184 + ((HEAP32[i7 + 132 >> 2] | 0) * 12 | 0) >> 2] & 3](i7, i10) | 0;
        i17 = 183;
       }
      } while (0);
      if ((i17 | 0) == 183) {
       if ((i12 & -2 | 0) == 2) {
        HEAP32[i11 >> 2] = 666;
       }
       if ((i12 & -3 | 0) != 0) {
        if ((i12 | 0) != 1) {
         break;
        }
        if ((i10 | 0) == 1) {
         __tr_align(i7);
        } else if (((i10 | 0) != 5 ? (__tr_stored_block(i7, 0, 0, 0), (i10 | 0) == 3) : 0) ? (i37 = HEAP32[i7 + 76 >> 2] | 0, i36 = HEAP32[i7 + 68 >> 2] | 0, HEAP16[i36 + (i37 + -1 << 1) >> 1] = 0, _memset(i36 | 0, 0, (i37 << 1) + -2 | 0) | 0, (HEAP32[i7 + 116 >> 2] | 0) == 0) : 0) {
         HEAP32[i7 + 108 >> 2] = 0;
         HEAP32[i7 + 92 >> 2] = 0;
        }
        i11 = HEAP32[i5 >> 2] | 0;
        i12 = HEAP32[i11 + 20 >> 2] | 0;
        i10 = HEAP32[i3 >> 2] | 0;
        i12 = i12 >>> 0 > i10 >>> 0 ? i10 : i12;
        if ((i12 | 0) != 0) {
         _memcpy(HEAP32[i4 >> 2] | 0, HEAP32[i11 + 16 >> 2] | 0, i12 | 0) | 0;
         HEAP32[i4 >> 2] = (HEAP32[i4 >> 2] | 0) + i12;
         i10 = (HEAP32[i5 >> 2] | 0) + 16 | 0;
         HEAP32[i10 >> 2] = (HEAP32[i10 >> 2] | 0) + i12;
         i10 = i2 + 20 | 0;
         HEAP32[i10 >> 2] = (HEAP32[i10 >> 2] | 0) + i12;
         HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) - i12;
         i10 = HEAP32[i5 >> 2] | 0;
         i36 = i10 + 20 | 0;
         i37 = HEAP32[i36 >> 2] | 0;
         HEAP32[i36 >> 2] = i37 - i12;
         if ((i37 | 0) == (i12 | 0)) {
          HEAP32[i10 + 16 >> 2] = HEAP32[i10 + 8 >> 2];
         }
         i10 = HEAP32[i3 >> 2] | 0;
        }
        if ((i10 | 0) != 0) {
         break;
        }
        HEAP32[i8 >> 2] = -1;
        i37 = 0;
        STACKTOP = i1;
        return i37 | 0;
       }
      }
      if ((HEAP32[i3 >> 2] | 0) != 0) {
       i37 = 0;
       STACKTOP = i1;
       return i37 | 0;
      }
      HEAP32[i8 >> 2] = -1;
      i37 = 0;
      STACKTOP = i1;
      return i37 | 0;
     }
    } while (0);
    if (!i9) {
     i37 = 0;
     STACKTOP = i1;
     return i37 | 0;
    }
    i8 = i7 + 24 | 0;
    i10 = HEAP32[i8 >> 2] | 0;
    if ((i10 | 0) < 1) {
     i37 = 1;
     STACKTOP = i1;
     return i37 | 0;
    }
    i11 = i2 + 48 | 0;
    i9 = HEAP32[i11 >> 2] | 0;
    if ((i10 | 0) == 2) {
     i34 = HEAP32[i19 >> 2] | 0;
     HEAP32[i19 >> 2] = i34 + 1;
     i36 = i7 + 8 | 0;
     HEAP8[(HEAP32[i36 >> 2] | 0) + i34 | 0] = i9;
     i34 = (HEAP32[i11 >> 2] | 0) >>> 8 & 255;
     i35 = HEAP32[i19 >> 2] | 0;
     HEAP32[i19 >> 2] = i35 + 1;
     HEAP8[(HEAP32[i36 >> 2] | 0) + i35 | 0] = i34;
     i35 = (HEAP32[i11 >> 2] | 0) >>> 16 & 255;
     i34 = HEAP32[i19 >> 2] | 0;
     HEAP32[i19 >> 2] = i34 + 1;
     HEAP8[(HEAP32[i36 >> 2] | 0) + i34 | 0] = i35;
     i34 = (HEAP32[i11 >> 2] | 0) >>> 24 & 255;
     i35 = HEAP32[i19 >> 2] | 0;
     HEAP32[i19 >> 2] = i35 + 1;
     HEAP8[(HEAP32[i36 >> 2] | 0) + i35 | 0] = i34;
     i35 = i2 + 8 | 0;
     i34 = HEAP32[i35 >> 2] & 255;
     i37 = HEAP32[i19 >> 2] | 0;
     HEAP32[i19 >> 2] = i37 + 1;
     HEAP8[(HEAP32[i36 >> 2] | 0) + i37 | 0] = i34;
     i37 = (HEAP32[i35 >> 2] | 0) >>> 8 & 255;
     i34 = HEAP32[i19 >> 2] | 0;
     HEAP32[i19 >> 2] = i34 + 1;
     HEAP8[(HEAP32[i36 >> 2] | 0) + i34 | 0] = i37;
     i34 = (HEAP32[i35 >> 2] | 0) >>> 16 & 255;
     i37 = HEAP32[i19 >> 2] | 0;
     HEAP32[i19 >> 2] = i37 + 1;
     HEAP8[(HEAP32[i36 >> 2] | 0) + i37 | 0] = i34;
     i35 = (HEAP32[i35 >> 2] | 0) >>> 24 & 255;
     i37 = HEAP32[i19 >> 2] | 0;
     HEAP32[i19 >> 2] = i37 + 1;
     HEAP8[(HEAP32[i36 >> 2] | 0) + i37 | 0] = i35;
    } else {
     i35 = HEAP32[i19 >> 2] | 0;
     HEAP32[i19 >> 2] = i35 + 1;
     i36 = i7 + 8 | 0;
     HEAP8[(HEAP32[i36 >> 2] | 0) + i35 | 0] = i9 >>> 24;
     i35 = HEAP32[i19 >> 2] | 0;
     HEAP32[i19 >> 2] = i35 + 1;
     HEAP8[(HEAP32[i36 >> 2] | 0) + i35 | 0] = i9 >>> 16;
     i35 = HEAP32[i11 >> 2] | 0;
     i37 = HEAP32[i19 >> 2] | 0;
     HEAP32[i19 >> 2] = i37 + 1;
     HEAP8[(HEAP32[i36 >> 2] | 0) + i37 | 0] = i35 >>> 8;
     i37 = HEAP32[i19 >> 2] | 0;
     HEAP32[i19 >> 2] = i37 + 1;
     HEAP8[(HEAP32[i36 >> 2] | 0) + i37 | 0] = i35;
    }
    i7 = HEAP32[i5 >> 2] | 0;
    i10 = HEAP32[i7 + 20 >> 2] | 0;
    i9 = HEAP32[i3 >> 2] | 0;
    i9 = i10 >>> 0 > i9 >>> 0 ? i9 : i10;
    if ((i9 | 0) != 0 ? (_memcpy(HEAP32[i4 >> 2] | 0, HEAP32[i7 + 16 >> 2] | 0, i9 | 0) | 0, HEAP32[i4 >> 2] = (HEAP32[i4 >> 2] | 0) + i9, i6 = (HEAP32[i5 >> 2] | 0) + 16 | 0, HEAP32[i6 >> 2] = (HEAP32[i6 >> 2] | 0) + i9, i6 = i2 + 20 | 0, HEAP32[i6 >> 2] = (HEAP32[i6 >> 2] | 0) + i9, HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) - i9, i6 = HEAP32[i5 >> 2] | 0, i36 = i6 + 20 | 0, i37 = HEAP32[i36 >> 2] | 0, HEAP32[i36 >> 2] = i37 - i9, (i37 | 0) == (i9 | 0)) : 0) {
     HEAP32[i6 + 16 >> 2] = HEAP32[i6 + 8 >> 2];
    }
    i2 = HEAP32[i8 >> 2] | 0;
    if ((i2 | 0) > 0) {
     HEAP32[i8 >> 2] = 0 - i2;
    }
    i37 = (HEAP32[i19 >> 2] | 0) == 0 | 0;
    STACKTOP = i1;
    return i37 | 0;
   }
  }
 } while (0);
 HEAP32[i2 + 24 >> 2] = HEAP32[3168 >> 2];
 i37 = -2;
 STACKTOP = i1;
 return i37 | 0;
}
function _free(i7) {
 i7 = i7 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0;
 i1 = STACKTOP;
 if ((i7 | 0) == 0) {
  STACKTOP = i1;
  return;
 }
 i15 = i7 + -8 | 0;
 i16 = HEAP32[14488 >> 2] | 0;
 if (i15 >>> 0 < i16 >>> 0) {
  _abort();
 }
 i13 = HEAP32[i7 + -4 >> 2] | 0;
 i12 = i13 & 3;
 if ((i12 | 0) == 1) {
  _abort();
 }
 i8 = i13 & -8;
 i6 = i7 + (i8 + -8) | 0;
 do {
  if ((i13 & 1 | 0) == 0) {
   i19 = HEAP32[i15 >> 2] | 0;
   if ((i12 | 0) == 0) {
    STACKTOP = i1;
    return;
   }
   i15 = -8 - i19 | 0;
   i13 = i7 + i15 | 0;
   i12 = i19 + i8 | 0;
   if (i13 >>> 0 < i16 >>> 0) {
    _abort();
   }
   if ((i13 | 0) == (HEAP32[14492 >> 2] | 0)) {
    i2 = i7 + (i8 + -4) | 0;
    if ((HEAP32[i2 >> 2] & 3 | 0) != 3) {
     i2 = i13;
     i11 = i12;
     break;
    }
    HEAP32[14480 >> 2] = i12;
    HEAP32[i2 >> 2] = HEAP32[i2 >> 2] & -2;
    HEAP32[i7 + (i15 + 4) >> 2] = i12 | 1;
    HEAP32[i6 >> 2] = i12;
    STACKTOP = i1;
    return;
   }
   i18 = i19 >>> 3;
   if (i19 >>> 0 < 256) {
    i2 = HEAP32[i7 + (i15 + 8) >> 2] | 0;
    i11 = HEAP32[i7 + (i15 + 12) >> 2] | 0;
    i14 = 14512 + (i18 << 1 << 2) | 0;
    if ((i2 | 0) != (i14 | 0)) {
     if (i2 >>> 0 < i16 >>> 0) {
      _abort();
     }
     if ((HEAP32[i2 + 12 >> 2] | 0) != (i13 | 0)) {
      _abort();
     }
    }
    if ((i11 | 0) == (i2 | 0)) {
     HEAP32[3618] = HEAP32[3618] & ~(1 << i18);
     i2 = i13;
     i11 = i12;
     break;
    }
    if ((i11 | 0) != (i14 | 0)) {
     if (i11 >>> 0 < i16 >>> 0) {
      _abort();
     }
     i14 = i11 + 8 | 0;
     if ((HEAP32[i14 >> 2] | 0) == (i13 | 0)) {
      i17 = i14;
     } else {
      _abort();
     }
    } else {
     i17 = i11 + 8 | 0;
    }
    HEAP32[i2 + 12 >> 2] = i11;
    HEAP32[i17 >> 2] = i2;
    i2 = i13;
    i11 = i12;
    break;
   }
   i17 = HEAP32[i7 + (i15 + 24) >> 2] | 0;
   i18 = HEAP32[i7 + (i15 + 12) >> 2] | 0;
   do {
    if ((i18 | 0) == (i13 | 0)) {
     i19 = i7 + (i15 + 20) | 0;
     i18 = HEAP32[i19 >> 2] | 0;
     if ((i18 | 0) == 0) {
      i19 = i7 + (i15 + 16) | 0;
      i18 = HEAP32[i19 >> 2] | 0;
      if ((i18 | 0) == 0) {
       i14 = 0;
       break;
      }
     }
     while (1) {
      i21 = i18 + 20 | 0;
      i20 = HEAP32[i21 >> 2] | 0;
      if ((i20 | 0) != 0) {
       i18 = i20;
       i19 = i21;
       continue;
      }
      i20 = i18 + 16 | 0;
      i21 = HEAP32[i20 >> 2] | 0;
      if ((i21 | 0) == 0) {
       break;
      } else {
       i18 = i21;
       i19 = i20;
      }
     }
     if (i19 >>> 0 < i16 >>> 0) {
      _abort();
     } else {
      HEAP32[i19 >> 2] = 0;
      i14 = i18;
      break;
     }
    } else {
     i19 = HEAP32[i7 + (i15 + 8) >> 2] | 0;
     if (i19 >>> 0 < i16 >>> 0) {
      _abort();
     }
     i16 = i19 + 12 | 0;
     if ((HEAP32[i16 >> 2] | 0) != (i13 | 0)) {
      _abort();
     }
     i20 = i18 + 8 | 0;
     if ((HEAP32[i20 >> 2] | 0) == (i13 | 0)) {
      HEAP32[i16 >> 2] = i18;
      HEAP32[i20 >> 2] = i19;
      i14 = i18;
      break;
     } else {
      _abort();
     }
    }
   } while (0);
   if ((i17 | 0) != 0) {
    i18 = HEAP32[i7 + (i15 + 28) >> 2] | 0;
    i16 = 14776 + (i18 << 2) | 0;
    if ((i13 | 0) == (HEAP32[i16 >> 2] | 0)) {
     HEAP32[i16 >> 2] = i14;
     if ((i14 | 0) == 0) {
      HEAP32[14476 >> 2] = HEAP32[14476 >> 2] & ~(1 << i18);
      i2 = i13;
      i11 = i12;
      break;
     }
    } else {
     if (i17 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
      _abort();
     }
     i16 = i17 + 16 | 0;
     if ((HEAP32[i16 >> 2] | 0) == (i13 | 0)) {
      HEAP32[i16 >> 2] = i14;
     } else {
      HEAP32[i17 + 20 >> 2] = i14;
     }
     if ((i14 | 0) == 0) {
      i2 = i13;
      i11 = i12;
      break;
     }
    }
    if (i14 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
     _abort();
    }
    HEAP32[i14 + 24 >> 2] = i17;
    i16 = HEAP32[i7 + (i15 + 16) >> 2] | 0;
    do {
     if ((i16 | 0) != 0) {
      if (i16 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
       _abort();
      } else {
       HEAP32[i14 + 16 >> 2] = i16;
       HEAP32[i16 + 24 >> 2] = i14;
       break;
      }
     }
    } while (0);
    i15 = HEAP32[i7 + (i15 + 20) >> 2] | 0;
    if ((i15 | 0) != 0) {
     if (i15 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
      _abort();
     } else {
      HEAP32[i14 + 20 >> 2] = i15;
      HEAP32[i15 + 24 >> 2] = i14;
      i2 = i13;
      i11 = i12;
      break;
     }
    } else {
     i2 = i13;
     i11 = i12;
    }
   } else {
    i2 = i13;
    i11 = i12;
   }
  } else {
   i2 = i15;
   i11 = i8;
  }
 } while (0);
 if (!(i2 >>> 0 < i6 >>> 0)) {
  _abort();
 }
 i12 = i7 + (i8 + -4) | 0;
 i13 = HEAP32[i12 >> 2] | 0;
 if ((i13 & 1 | 0) == 0) {
  _abort();
 }
 if ((i13 & 2 | 0) == 0) {
  if ((i6 | 0) == (HEAP32[14496 >> 2] | 0)) {
   i21 = (HEAP32[14484 >> 2] | 0) + i11 | 0;
   HEAP32[14484 >> 2] = i21;
   HEAP32[14496 >> 2] = i2;
   HEAP32[i2 + 4 >> 2] = i21 | 1;
   if ((i2 | 0) != (HEAP32[14492 >> 2] | 0)) {
    STACKTOP = i1;
    return;
   }
   HEAP32[14492 >> 2] = 0;
   HEAP32[14480 >> 2] = 0;
   STACKTOP = i1;
   return;
  }
  if ((i6 | 0) == (HEAP32[14492 >> 2] | 0)) {
   i21 = (HEAP32[14480 >> 2] | 0) + i11 | 0;
   HEAP32[14480 >> 2] = i21;
   HEAP32[14492 >> 2] = i2;
   HEAP32[i2 + 4 >> 2] = i21 | 1;
   HEAP32[i2 + i21 >> 2] = i21;
   STACKTOP = i1;
   return;
  }
  i11 = (i13 & -8) + i11 | 0;
  i12 = i13 >>> 3;
  do {
   if (!(i13 >>> 0 < 256)) {
    i10 = HEAP32[i7 + (i8 + 16) >> 2] | 0;
    i15 = HEAP32[i7 + (i8 | 4) >> 2] | 0;
    do {
     if ((i15 | 0) == (i6 | 0)) {
      i13 = i7 + (i8 + 12) | 0;
      i12 = HEAP32[i13 >> 2] | 0;
      if ((i12 | 0) == 0) {
       i13 = i7 + (i8 + 8) | 0;
       i12 = HEAP32[i13 >> 2] | 0;
       if ((i12 | 0) == 0) {
        i9 = 0;
        break;
       }
      }
      while (1) {
       i14 = i12 + 20 | 0;
       i15 = HEAP32[i14 >> 2] | 0;
       if ((i15 | 0) != 0) {
        i12 = i15;
        i13 = i14;
        continue;
       }
       i14 = i12 + 16 | 0;
       i15 = HEAP32[i14 >> 2] | 0;
       if ((i15 | 0) == 0) {
        break;
       } else {
        i12 = i15;
        i13 = i14;
       }
      }
      if (i13 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
       _abort();
      } else {
       HEAP32[i13 >> 2] = 0;
       i9 = i12;
       break;
      }
     } else {
      i13 = HEAP32[i7 + i8 >> 2] | 0;
      if (i13 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
       _abort();
      }
      i14 = i13 + 12 | 0;
      if ((HEAP32[i14 >> 2] | 0) != (i6 | 0)) {
       _abort();
      }
      i12 = i15 + 8 | 0;
      if ((HEAP32[i12 >> 2] | 0) == (i6 | 0)) {
       HEAP32[i14 >> 2] = i15;
       HEAP32[i12 >> 2] = i13;
       i9 = i15;
       break;
      } else {
       _abort();
      }
     }
    } while (0);
    if ((i10 | 0) != 0) {
     i12 = HEAP32[i7 + (i8 + 20) >> 2] | 0;
     i13 = 14776 + (i12 << 2) | 0;
     if ((i6 | 0) == (HEAP32[i13 >> 2] | 0)) {
      HEAP32[i13 >> 2] = i9;
      if ((i9 | 0) == 0) {
       HEAP32[14476 >> 2] = HEAP32[14476 >> 2] & ~(1 << i12);
       break;
      }
     } else {
      if (i10 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
       _abort();
      }
      i12 = i10 + 16 | 0;
      if ((HEAP32[i12 >> 2] | 0) == (i6 | 0)) {
       HEAP32[i12 >> 2] = i9;
      } else {
       HEAP32[i10 + 20 >> 2] = i9;
      }
      if ((i9 | 0) == 0) {
       break;
      }
     }
     if (i9 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
      _abort();
     }
     HEAP32[i9 + 24 >> 2] = i10;
     i6 = HEAP32[i7 + (i8 + 8) >> 2] | 0;
     do {
      if ((i6 | 0) != 0) {
       if (i6 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
        _abort();
       } else {
        HEAP32[i9 + 16 >> 2] = i6;
        HEAP32[i6 + 24 >> 2] = i9;
        break;
       }
      }
     } while (0);
     i6 = HEAP32[i7 + (i8 + 12) >> 2] | 0;
     if ((i6 | 0) != 0) {
      if (i6 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
       _abort();
      } else {
       HEAP32[i9 + 20 >> 2] = i6;
       HEAP32[i6 + 24 >> 2] = i9;
       break;
      }
     }
    }
   } else {
    i9 = HEAP32[i7 + i8 >> 2] | 0;
    i7 = HEAP32[i7 + (i8 | 4) >> 2] | 0;
    i8 = 14512 + (i12 << 1 << 2) | 0;
    if ((i9 | 0) != (i8 | 0)) {
     if (i9 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
      _abort();
     }
     if ((HEAP32[i9 + 12 >> 2] | 0) != (i6 | 0)) {
      _abort();
     }
    }
    if ((i7 | 0) == (i9 | 0)) {
     HEAP32[3618] = HEAP32[3618] & ~(1 << i12);
     break;
    }
    if ((i7 | 0) != (i8 | 0)) {
     if (i7 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
      _abort();
     }
     i8 = i7 + 8 | 0;
     if ((HEAP32[i8 >> 2] | 0) == (i6 | 0)) {
      i10 = i8;
     } else {
      _abort();
     }
    } else {
     i10 = i7 + 8 | 0;
    }
    HEAP32[i9 + 12 >> 2] = i7;
    HEAP32[i10 >> 2] = i9;
   }
  } while (0);
  HEAP32[i2 + 4 >> 2] = i11 | 1;
  HEAP32[i2 + i11 >> 2] = i11;
  if ((i2 | 0) == (HEAP32[14492 >> 2] | 0)) {
   HEAP32[14480 >> 2] = i11;
   STACKTOP = i1;
   return;
  }
 } else {
  HEAP32[i12 >> 2] = i13 & -2;
  HEAP32[i2 + 4 >> 2] = i11 | 1;
  HEAP32[i2 + i11 >> 2] = i11;
 }
 i6 = i11 >>> 3;
 if (i11 >>> 0 < 256) {
  i7 = i6 << 1;
  i3 = 14512 + (i7 << 2) | 0;
  i8 = HEAP32[3618] | 0;
  i6 = 1 << i6;
  if ((i8 & i6 | 0) != 0) {
   i6 = 14512 + (i7 + 2 << 2) | 0;
   i7 = HEAP32[i6 >> 2] | 0;
   if (i7 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
    _abort();
   } else {
    i4 = i6;
    i5 = i7;
   }
  } else {
   HEAP32[3618] = i8 | i6;
   i4 = 14512 + (i7 + 2 << 2) | 0;
   i5 = i3;
  }
  HEAP32[i4 >> 2] = i2;
  HEAP32[i5 + 12 >> 2] = i2;
  HEAP32[i2 + 8 >> 2] = i5;
  HEAP32[i2 + 12 >> 2] = i3;
  STACKTOP = i1;
  return;
 }
 i4 = i11 >>> 8;
 if ((i4 | 0) != 0) {
  if (i11 >>> 0 > 16777215) {
   i4 = 31;
  } else {
   i20 = (i4 + 1048320 | 0) >>> 16 & 8;
   i21 = i4 << i20;
   i19 = (i21 + 520192 | 0) >>> 16 & 4;
   i21 = i21 << i19;
   i4 = (i21 + 245760 | 0) >>> 16 & 2;
   i4 = 14 - (i19 | i20 | i4) + (i21 << i4 >>> 15) | 0;
   i4 = i11 >>> (i4 + 7 | 0) & 1 | i4 << 1;
  }
 } else {
  i4 = 0;
 }
 i5 = 14776 + (i4 << 2) | 0;
 HEAP32[i2 + 28 >> 2] = i4;
 HEAP32[i2 + 20 >> 2] = 0;
 HEAP32[i2 + 16 >> 2] = 0;
 i7 = HEAP32[14476 >> 2] | 0;
 i6 = 1 << i4;
 L199 : do {
  if ((i7 & i6 | 0) != 0) {
   i5 = HEAP32[i5 >> 2] | 0;
   if ((i4 | 0) == 31) {
    i4 = 0;
   } else {
    i4 = 25 - (i4 >>> 1) | 0;
   }
   L204 : do {
    if ((HEAP32[i5 + 4 >> 2] & -8 | 0) != (i11 | 0)) {
     i4 = i11 << i4;
     i7 = i5;
     while (1) {
      i6 = i7 + (i4 >>> 31 << 2) + 16 | 0;
      i5 = HEAP32[i6 >> 2] | 0;
      if ((i5 | 0) == 0) {
       break;
      }
      if ((HEAP32[i5 + 4 >> 2] & -8 | 0) == (i11 | 0)) {
       i3 = i5;
       break L204;
      } else {
       i4 = i4 << 1;
       i7 = i5;
      }
     }
     if (i6 >>> 0 < (HEAP32[14488 >> 2] | 0) >>> 0) {
      _abort();
     } else {
      HEAP32[i6 >> 2] = i2;
      HEAP32[i2 + 24 >> 2] = i7;
      HEAP32[i2 + 12 >> 2] = i2;
      HEAP32[i2 + 8 >> 2] = i2;
      break L199;
     }
    } else {
     i3 = i5;
    }
   } while (0);
   i5 = i3 + 8 | 0;
   i4 = HEAP32[i5 >> 2] | 0;
   i6 = HEAP32[14488 >> 2] | 0;
   if (i3 >>> 0 < i6 >>> 0) {
    _abort();
   }
   if (i4 >>> 0 < i6 >>> 0) {
    _abort();
   } else {
    HEAP32[i4 + 12 >> 2] = i2;
    HEAP32[i5 >> 2] = i2;
    HEAP32[i2 + 8 >> 2] = i4;
    HEAP32[i2 + 12 >> 2] = i3;
    HEAP32[i2 + 24 >> 2] = 0;
    break;
   }
  } else {
   HEAP32[14476 >> 2] = i7 | i6;
   HEAP32[i5 >> 2] = i2;
   HEAP32[i2 + 24 >> 2] = i5;
   HEAP32[i2 + 12 >> 2] = i2;
   HEAP32[i2 + 8 >> 2] = i2;
  }
 } while (0);
 i21 = (HEAP32[14504 >> 2] | 0) + -1 | 0;
 HEAP32[14504 >> 2] = i21;
 if ((i21 | 0) == 0) {
  i2 = 14928 | 0;
 } else {
  STACKTOP = i1;
  return;
 }
 while (1) {
  i2 = HEAP32[i2 >> 2] | 0;
  if ((i2 | 0) == 0) {
   break;
  } else {
   i2 = i2 + 8 | 0;
  }
 }
 HEAP32[14504 >> 2] = -1;
 STACKTOP = i1;
 return;
}
function _build_tree(i4, i9) {
 i4 = i4 | 0;
 i9 = i9 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0;
 i2 = STACKTOP;
 STACKTOP = STACKTOP + 32 | 0;
 i1 = i2;
 i3 = HEAP32[i9 >> 2] | 0;
 i7 = i9 + 8 | 0;
 i11 = HEAP32[i7 >> 2] | 0;
 i12 = HEAP32[i11 >> 2] | 0;
 i11 = HEAP32[i11 + 12 >> 2] | 0;
 i8 = i4 + 5200 | 0;
 HEAP32[i8 >> 2] = 0;
 i6 = i4 + 5204 | 0;
 HEAP32[i6 >> 2] = 573;
 if ((i11 | 0) > 0) {
  i5 = -1;
  i13 = 0;
  do {
   if ((HEAP16[i3 + (i13 << 2) >> 1] | 0) == 0) {
    HEAP16[i3 + (i13 << 2) + 2 >> 1] = 0;
   } else {
    i5 = (HEAP32[i8 >> 2] | 0) + 1 | 0;
    HEAP32[i8 >> 2] = i5;
    HEAP32[i4 + (i5 << 2) + 2908 >> 2] = i13;
    HEAP8[i4 + i13 + 5208 | 0] = 0;
    i5 = i13;
   }
   i13 = i13 + 1 | 0;
  } while ((i13 | 0) != (i11 | 0));
  i14 = HEAP32[i8 >> 2] | 0;
  if ((i14 | 0) < 2) {
   i10 = 3;
  }
 } else {
  i14 = 0;
  i5 = -1;
  i10 = 3;
 }
 if ((i10 | 0) == 3) {
  i10 = i4 + 5800 | 0;
  i13 = i4 + 5804 | 0;
  if ((i12 | 0) == 0) {
   do {
    i12 = (i5 | 0) < 2;
    i13 = i5 + 1 | 0;
    i5 = i12 ? i13 : i5;
    i23 = i12 ? i13 : 0;
    i14 = i14 + 1 | 0;
    HEAP32[i8 >> 2] = i14;
    HEAP32[i4 + (i14 << 2) + 2908 >> 2] = i23;
    HEAP16[i3 + (i23 << 2) >> 1] = 1;
    HEAP8[i4 + i23 + 5208 | 0] = 0;
    HEAP32[i10 >> 2] = (HEAP32[i10 >> 2] | 0) + -1;
    i14 = HEAP32[i8 >> 2] | 0;
   } while ((i14 | 0) < 2);
  } else {
   do {
    i15 = (i5 | 0) < 2;
    i16 = i5 + 1 | 0;
    i5 = i15 ? i16 : i5;
    i23 = i15 ? i16 : 0;
    i14 = i14 + 1 | 0;
    HEAP32[i8 >> 2] = i14;
    HEAP32[i4 + (i14 << 2) + 2908 >> 2] = i23;
    HEAP16[i3 + (i23 << 2) >> 1] = 1;
    HEAP8[i4 + i23 + 5208 | 0] = 0;
    HEAP32[i10 >> 2] = (HEAP32[i10 >> 2] | 0) + -1;
    HEAP32[i13 >> 2] = (HEAP32[i13 >> 2] | 0) - (HEAPU16[i12 + (i23 << 2) + 2 >> 1] | 0);
    i14 = HEAP32[i8 >> 2] | 0;
   } while ((i14 | 0) < 2);
  }
 }
 i10 = i9 + 4 | 0;
 HEAP32[i10 >> 2] = i5;
 i12 = HEAP32[i8 >> 2] | 0;
 if ((i12 | 0) > 1) {
  i18 = i12;
  i13 = (i12 | 0) / 2 | 0;
  do {
   i12 = HEAP32[i4 + (i13 << 2) + 2908 >> 2] | 0;
   i14 = i4 + i12 + 5208 | 0;
   i17 = i13 << 1;
   L21 : do {
    if ((i17 | 0) > (i18 | 0)) {
     i15 = i13;
    } else {
     i16 = i3 + (i12 << 2) | 0;
     i15 = i13;
     while (1) {
      do {
       if ((i17 | 0) < (i18 | 0)) {
        i18 = i17 | 1;
        i19 = HEAP32[i4 + (i18 << 2) + 2908 >> 2] | 0;
        i22 = HEAP16[i3 + (i19 << 2) >> 1] | 0;
        i20 = HEAP32[i4 + (i17 << 2) + 2908 >> 2] | 0;
        i21 = HEAP16[i3 + (i20 << 2) >> 1] | 0;
        if (!((i22 & 65535) < (i21 & 65535))) {
         if (!(i22 << 16 >> 16 == i21 << 16 >> 16)) {
          break;
         }
         if ((HEAPU8[i4 + i19 + 5208 | 0] | 0) > (HEAPU8[i4 + i20 + 5208 | 0] | 0)) {
          break;
         }
        }
        i17 = i18;
       }
      } while (0);
      i19 = HEAP16[i16 >> 1] | 0;
      i18 = HEAP32[i4 + (i17 << 2) + 2908 >> 2] | 0;
      i20 = HEAP16[i3 + (i18 << 2) >> 1] | 0;
      if ((i19 & 65535) < (i20 & 65535)) {
       break L21;
      }
      if (i19 << 16 >> 16 == i20 << 16 >> 16 ? (HEAPU8[i14] | 0) <= (HEAPU8[i4 + i18 + 5208 | 0] | 0) : 0) {
       break L21;
      }
      HEAP32[i4 + (i15 << 2) + 2908 >> 2] = i18;
      i19 = i17 << 1;
      i18 = HEAP32[i8 >> 2] | 0;
      if ((i19 | 0) > (i18 | 0)) {
       i15 = i17;
       break;
      } else {
       i15 = i17;
       i17 = i19;
      }
     }
    }
   } while (0);
   HEAP32[i4 + (i15 << 2) + 2908 >> 2] = i12;
   i13 = i13 + -1 | 0;
   i18 = HEAP32[i8 >> 2] | 0;
  } while ((i13 | 0) > 0);
 } else {
  i18 = i12;
 }
 i12 = i4 + 2912 | 0;
 while (1) {
  i13 = HEAP32[i12 >> 2] | 0;
  i20 = i18 + -1 | 0;
  HEAP32[i8 >> 2] = i20;
  i14 = HEAP32[i4 + (i18 << 2) + 2908 >> 2] | 0;
  HEAP32[i12 >> 2] = i14;
  i15 = i4 + i14 + 5208 | 0;
  L40 : do {
   if ((i18 | 0) < 3) {
    i17 = 1;
   } else {
    i16 = i3 + (i14 << 2) | 0;
    i17 = 1;
    i18 = 2;
    while (1) {
     do {
      if ((i18 | 0) < (i20 | 0)) {
       i22 = i18 | 1;
       i21 = HEAP32[i4 + (i22 << 2) + 2908 >> 2] | 0;
       i23 = HEAP16[i3 + (i21 << 2) >> 1] | 0;
       i20 = HEAP32[i4 + (i18 << 2) + 2908 >> 2] | 0;
       i19 = HEAP16[i3 + (i20 << 2) >> 1] | 0;
       if (!((i23 & 65535) < (i19 & 65535))) {
        if (!(i23 << 16 >> 16 == i19 << 16 >> 16)) {
         break;
        }
        if ((HEAPU8[i4 + i21 + 5208 | 0] | 0) > (HEAPU8[i4 + i20 + 5208 | 0] | 0)) {
         break;
        }
       }
       i18 = i22;
      }
     } while (0);
     i21 = HEAP16[i16 >> 1] | 0;
     i20 = HEAP32[i4 + (i18 << 2) + 2908 >> 2] | 0;
     i19 = HEAP16[i3 + (i20 << 2) >> 1] | 0;
     if ((i21 & 65535) < (i19 & 65535)) {
      break L40;
     }
     if (i21 << 16 >> 16 == i19 << 16 >> 16 ? (HEAPU8[i15] | 0) <= (HEAPU8[i4 + i20 + 5208 | 0] | 0) : 0) {
      break L40;
     }
     HEAP32[i4 + (i17 << 2) + 2908 >> 2] = i20;
     i19 = i18 << 1;
     i20 = HEAP32[i8 >> 2] | 0;
     if ((i19 | 0) > (i20 | 0)) {
      i17 = i18;
      break;
     } else {
      i17 = i18;
      i18 = i19;
     }
    }
   }
  } while (0);
  HEAP32[i4 + (i17 << 2) + 2908 >> 2] = i14;
  i17 = HEAP32[i12 >> 2] | 0;
  i14 = (HEAP32[i6 >> 2] | 0) + -1 | 0;
  HEAP32[i6 >> 2] = i14;
  HEAP32[i4 + (i14 << 2) + 2908 >> 2] = i13;
  i14 = (HEAP32[i6 >> 2] | 0) + -1 | 0;
  HEAP32[i6 >> 2] = i14;
  HEAP32[i4 + (i14 << 2) + 2908 >> 2] = i17;
  i14 = i3 + (i11 << 2) | 0;
  HEAP16[i14 >> 1] = (HEAPU16[i3 + (i17 << 2) >> 1] | 0) + (HEAPU16[i3 + (i13 << 2) >> 1] | 0);
  i18 = HEAP8[i4 + i13 + 5208 | 0] | 0;
  i16 = HEAP8[i4 + i17 + 5208 | 0] | 0;
  i15 = i4 + i11 + 5208 | 0;
  HEAP8[i15] = (((i18 & 255) < (i16 & 255) ? i16 : i18) & 255) + 1;
  i19 = i11 & 65535;
  HEAP16[i3 + (i17 << 2) + 2 >> 1] = i19;
  HEAP16[i3 + (i13 << 2) + 2 >> 1] = i19;
  i13 = i11 + 1 | 0;
  HEAP32[i12 >> 2] = i11;
  i19 = HEAP32[i8 >> 2] | 0;
  L56 : do {
   if ((i19 | 0) < 2) {
    i16 = 1;
   } else {
    i16 = 1;
    i17 = 2;
    while (1) {
     do {
      if ((i17 | 0) < (i19 | 0)) {
       i21 = i17 | 1;
       i22 = HEAP32[i4 + (i21 << 2) + 2908 >> 2] | 0;
       i19 = HEAP16[i3 + (i22 << 2) >> 1] | 0;
       i18 = HEAP32[i4 + (i17 << 2) + 2908 >> 2] | 0;
       i20 = HEAP16[i3 + (i18 << 2) >> 1] | 0;
       if (!((i19 & 65535) < (i20 & 65535))) {
        if (!(i19 << 16 >> 16 == i20 << 16 >> 16)) {
         break;
        }
        if ((HEAPU8[i4 + i22 + 5208 | 0] | 0) > (HEAPU8[i4 + i18 + 5208 | 0] | 0)) {
         break;
        }
       }
       i17 = i21;
      }
     } while (0);
     i19 = HEAP16[i14 >> 1] | 0;
     i20 = HEAP32[i4 + (i17 << 2) + 2908 >> 2] | 0;
     i18 = HEAP16[i3 + (i20 << 2) >> 1] | 0;
     if ((i19 & 65535) < (i18 & 65535)) {
      break L56;
     }
     if (i19 << 16 >> 16 == i18 << 16 >> 16 ? (HEAPU8[i15] | 0) <= (HEAPU8[i4 + i20 + 5208 | 0] | 0) : 0) {
      break L56;
     }
     HEAP32[i4 + (i16 << 2) + 2908 >> 2] = i20;
     i18 = i17 << 1;
     i19 = HEAP32[i8 >> 2] | 0;
     if ((i18 | 0) > (i19 | 0)) {
      i16 = i17;
      break;
     } else {
      i16 = i17;
      i17 = i18;
     }
    }
   }
  } while (0);
  HEAP32[i4 + (i16 << 2) + 2908 >> 2] = i11;
  i18 = HEAP32[i8 >> 2] | 0;
  if ((i18 | 0) > 1) {
   i11 = i13;
  } else {
   break;
  }
 }
 i12 = HEAP32[i12 >> 2] | 0;
 i8 = (HEAP32[i6 >> 2] | 0) + -1 | 0;
 HEAP32[i6 >> 2] = i8;
 HEAP32[i4 + (i8 << 2) + 2908 >> 2] = i12;
 i8 = HEAP32[i9 >> 2] | 0;
 i9 = HEAP32[i10 >> 2] | 0;
 i7 = HEAP32[i7 >> 2] | 0;
 i12 = HEAP32[i7 >> 2] | 0;
 i11 = HEAP32[i7 + 4 >> 2] | 0;
 i10 = HEAP32[i7 + 8 >> 2] | 0;
 i7 = HEAP32[i7 + 16 >> 2] | 0;
 i13 = i4 + 2876 | 0;
 i14 = i13 + 32 | 0;
 do {
  HEAP16[i13 >> 1] = 0;
  i13 = i13 + 2 | 0;
 } while ((i13 | 0) < (i14 | 0));
 i14 = HEAP32[i6 >> 2] | 0;
 HEAP16[i8 + (HEAP32[i4 + (i14 << 2) + 2908 >> 2] << 2) + 2 >> 1] = 0;
 i14 = i14 + 1 | 0;
 L72 : do {
  if ((i14 | 0) < 573) {
   i6 = i4 + 5800 | 0;
   i13 = i4 + 5804 | 0;
   if ((i12 | 0) == 0) {
    i18 = 0;
    do {
     i12 = HEAP32[i4 + (i14 << 2) + 2908 >> 2] | 0;
     i13 = i8 + (i12 << 2) + 2 | 0;
     i15 = HEAPU16[i8 + (HEAPU16[i13 >> 1] << 2) + 2 >> 1] | 0;
     i16 = (i15 | 0) < (i7 | 0);
     i15 = i16 ? i15 + 1 | 0 : i7;
     i18 = (i16 & 1 ^ 1) + i18 | 0;
     HEAP16[i13 >> 1] = i15;
     if ((i12 | 0) <= (i9 | 0)) {
      i23 = i4 + (i15 << 1) + 2876 | 0;
      HEAP16[i23 >> 1] = (HEAP16[i23 >> 1] | 0) + 1 << 16 >> 16;
      if ((i12 | 0) < (i10 | 0)) {
       i13 = 0;
      } else {
       i13 = HEAP32[i11 + (i12 - i10 << 2) >> 2] | 0;
      }
      i23 = Math_imul(HEAPU16[i8 + (i12 << 2) >> 1] | 0, i13 + i15 | 0) | 0;
      HEAP32[i6 >> 2] = i23 + (HEAP32[i6 >> 2] | 0);
     }
     i14 = i14 + 1 | 0;
    } while ((i14 | 0) != 573);
   } else {
    i18 = 0;
    do {
     i15 = HEAP32[i4 + (i14 << 2) + 2908 >> 2] | 0;
     i16 = i8 + (i15 << 2) + 2 | 0;
     i17 = HEAPU16[i8 + (HEAPU16[i16 >> 1] << 2) + 2 >> 1] | 0;
     i19 = (i17 | 0) < (i7 | 0);
     i17 = i19 ? i17 + 1 | 0 : i7;
     i18 = (i19 & 1 ^ 1) + i18 | 0;
     HEAP16[i16 >> 1] = i17;
     if ((i15 | 0) <= (i9 | 0)) {
      i23 = i4 + (i17 << 1) + 2876 | 0;
      HEAP16[i23 >> 1] = (HEAP16[i23 >> 1] | 0) + 1 << 16 >> 16;
      if ((i15 | 0) < (i10 | 0)) {
       i16 = 0;
      } else {
       i16 = HEAP32[i11 + (i15 - i10 << 2) >> 2] | 0;
      }
      i23 = HEAPU16[i8 + (i15 << 2) >> 1] | 0;
      i22 = Math_imul(i23, i16 + i17 | 0) | 0;
      HEAP32[i6 >> 2] = i22 + (HEAP32[i6 >> 2] | 0);
      i23 = Math_imul((HEAPU16[i12 + (i15 << 2) + 2 >> 1] | 0) + i16 | 0, i23) | 0;
      HEAP32[i13 >> 2] = i23 + (HEAP32[i13 >> 2] | 0);
     }
     i14 = i14 + 1 | 0;
    } while ((i14 | 0) != 573);
   }
   if ((i18 | 0) != 0) {
    i10 = i4 + (i7 << 1) + 2876 | 0;
    do {
     i12 = i7;
     while (1) {
      i11 = i12 + -1 | 0;
      i13 = i4 + (i11 << 1) + 2876 | 0;
      i14 = HEAP16[i13 >> 1] | 0;
      if (i14 << 16 >> 16 == 0) {
       i12 = i11;
      } else {
       break;
      }
     }
     HEAP16[i13 >> 1] = i14 + -1 << 16 >> 16;
     i11 = i4 + (i12 << 1) + 2876 | 0;
     HEAP16[i11 >> 1] = (HEAPU16[i11 >> 1] | 0) + 2;
     i11 = (HEAP16[i10 >> 1] | 0) + -1 << 16 >> 16;
     HEAP16[i10 >> 1] = i11;
     i18 = i18 + -2 | 0;
    } while ((i18 | 0) > 0);
    if ((i7 | 0) != 0) {
     i12 = 573;
     while (1) {
      i10 = i7 & 65535;
      if (!(i11 << 16 >> 16 == 0)) {
       i11 = i11 & 65535;
       do {
        do {
         i12 = i12 + -1 | 0;
         i15 = HEAP32[i4 + (i12 << 2) + 2908 >> 2] | 0;
        } while ((i15 | 0) > (i9 | 0));
        i13 = i8 + (i15 << 2) + 2 | 0;
        i14 = HEAPU16[i13 >> 1] | 0;
        if ((i14 | 0) != (i7 | 0)) {
         i23 = Math_imul(HEAPU16[i8 + (i15 << 2) >> 1] | 0, i7 - i14 | 0) | 0;
         HEAP32[i6 >> 2] = i23 + (HEAP32[i6 >> 2] | 0);
         HEAP16[i13 >> 1] = i10;
        }
        i11 = i11 + -1 | 0;
       } while ((i11 | 0) != 0);
      }
      i7 = i7 + -1 | 0;
      if ((i7 | 0) == 0) {
       break L72;
      }
      i11 = HEAP16[i4 + (i7 << 1) + 2876 >> 1] | 0;
     }
    }
   }
  }
 } while (0);
 i7 = 1;
 i6 = 0;
 do {
  i6 = (HEAPU16[i4 + (i7 + -1 << 1) + 2876 >> 1] | 0) + (i6 & 65534) << 1;
  HEAP16[i1 + (i7 << 1) >> 1] = i6;
  i7 = i7 + 1 | 0;
 } while ((i7 | 0) != 16);
 if ((i5 | 0) < 0) {
  STACKTOP = i2;
  return;
 } else {
  i4 = 0;
 }
 while (1) {
  i23 = HEAP16[i3 + (i4 << 2) + 2 >> 1] | 0;
  i7 = i23 & 65535;
  if (!(i23 << 16 >> 16 == 0)) {
   i8 = i1 + (i7 << 1) | 0;
   i6 = HEAP16[i8 >> 1] | 0;
   HEAP16[i8 >> 1] = i6 + 1 << 16 >> 16;
   i6 = i6 & 65535;
   i8 = 0;
   while (1) {
    i8 = i8 | i6 & 1;
    i7 = i7 + -1 | 0;
    if ((i7 | 0) <= 0) {
     break;
    } else {
     i6 = i6 >>> 1;
     i8 = i8 << 1;
    }
   }
   HEAP16[i3 + (i4 << 2) >> 1] = i8;
  }
  if ((i4 | 0) == (i5 | 0)) {
   break;
  } else {
   i4 = i4 + 1 | 0;
  }
 }
 STACKTOP = i2;
 return;
}
function _deflate_slow(i2, i6) {
 i2 = i2 | 0;
 i6 = i6 | 0;
 var i1 = 0, i3 = 0, i4 = 0, i5 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0;
 i1 = STACKTOP;
 i15 = i2 + 116 | 0;
 i16 = (i6 | 0) == 0;
 i17 = i2 + 72 | 0;
 i18 = i2 + 88 | 0;
 i5 = i2 + 108 | 0;
 i7 = i2 + 56 | 0;
 i19 = i2 + 84 | 0;
 i20 = i2 + 68 | 0;
 i22 = i2 + 52 | 0;
 i21 = i2 + 64 | 0;
 i9 = i2 + 96 | 0;
 i10 = i2 + 120 | 0;
 i11 = i2 + 112 | 0;
 i12 = i2 + 100 | 0;
 i26 = i2 + 5792 | 0;
 i27 = i2 + 5796 | 0;
 i29 = i2 + 5784 | 0;
 i23 = i2 + 5788 | 0;
 i8 = i2 + 104 | 0;
 i4 = i2 + 92 | 0;
 i24 = i2 + 128 | 0;
 i14 = i2 + 44 | 0;
 i13 = i2 + 136 | 0;
 L1 : while (1) {
  i30 = HEAP32[i15 >> 2] | 0;
  while (1) {
   if (i30 >>> 0 < 262) {
    _fill_window(i2);
    i30 = HEAP32[i15 >> 2] | 0;
    if (i30 >>> 0 < 262 & i16) {
     i2 = 0;
     i30 = 50;
     break L1;
    }
    if ((i30 | 0) == 0) {
     i30 = 40;
     break L1;
    }
    if (!(i30 >>> 0 > 2)) {
     HEAP32[i10 >> 2] = HEAP32[i9 >> 2];
     HEAP32[i12 >> 2] = HEAP32[i11 >> 2];
     HEAP32[i9 >> 2] = 2;
     i32 = 2;
     i30 = 16;
    } else {
     i30 = 8;
    }
   } else {
    i30 = 8;
   }
   do {
    if ((i30 | 0) == 8) {
     i30 = 0;
     i34 = HEAP32[i5 >> 2] | 0;
     i31 = ((HEAPU8[(HEAP32[i7 >> 2] | 0) + (i34 + 2) | 0] | 0) ^ HEAP32[i17 >> 2] << HEAP32[i18 >> 2]) & HEAP32[i19 >> 2];
     HEAP32[i17 >> 2] = i31;
     i31 = (HEAP32[i20 >> 2] | 0) + (i31 << 1) | 0;
     i35 = HEAP16[i31 >> 1] | 0;
     HEAP16[(HEAP32[i21 >> 2] | 0) + ((HEAP32[i22 >> 2] & i34) << 1) >> 1] = i35;
     i32 = i35 & 65535;
     HEAP16[i31 >> 1] = i34;
     i31 = HEAP32[i9 >> 2] | 0;
     HEAP32[i10 >> 2] = i31;
     HEAP32[i12 >> 2] = HEAP32[i11 >> 2];
     HEAP32[i9 >> 2] = 2;
     if (!(i35 << 16 >> 16 == 0)) {
      if (i31 >>> 0 < (HEAP32[i24 >> 2] | 0) >>> 0) {
       if (!(((HEAP32[i5 >> 2] | 0) - i32 | 0) >>> 0 > ((HEAP32[i14 >> 2] | 0) + -262 | 0) >>> 0)) {
        i32 = _longest_match(i2, i32) | 0;
        HEAP32[i9 >> 2] = i32;
        if (i32 >>> 0 < 6) {
         if ((HEAP32[i13 >> 2] | 0) != 1) {
          if ((i32 | 0) != 3) {
           i30 = 16;
           break;
          }
          if (!(((HEAP32[i5 >> 2] | 0) - (HEAP32[i11 >> 2] | 0) | 0) >>> 0 > 4096)) {
           i32 = 3;
           i30 = 16;
           break;
          }
         }
         HEAP32[i9 >> 2] = 2;
         i32 = 2;
         i30 = 16;
        } else {
         i30 = 16;
        }
       } else {
        i32 = 2;
        i30 = 16;
       }
      } else {
       i32 = 2;
      }
     } else {
      i32 = 2;
      i30 = 16;
     }
    }
   } while (0);
   if ((i30 | 0) == 16) {
    i31 = HEAP32[i10 >> 2] | 0;
   }
   if (!(i31 >>> 0 < 3 | i32 >>> 0 > i31 >>> 0)) {
    break;
   }
   if ((HEAP32[i8 >> 2] | 0) == 0) {
    HEAP32[i8 >> 2] = 1;
    HEAP32[i5 >> 2] = (HEAP32[i5 >> 2] | 0) + 1;
    i30 = (HEAP32[i15 >> 2] | 0) + -1 | 0;
    HEAP32[i15 >> 2] = i30;
    continue;
   }
   i35 = HEAP8[(HEAP32[i7 >> 2] | 0) + ((HEAP32[i5 >> 2] | 0) + -1) | 0] | 0;
   i34 = HEAP32[i26 >> 2] | 0;
   HEAP16[(HEAP32[i27 >> 2] | 0) + (i34 << 1) >> 1] = 0;
   HEAP32[i26 >> 2] = i34 + 1;
   HEAP8[(HEAP32[i29 >> 2] | 0) + i34 | 0] = i35;
   i35 = i2 + ((i35 & 255) << 2) + 148 | 0;
   HEAP16[i35 >> 1] = (HEAP16[i35 >> 1] | 0) + 1 << 16 >> 16;
   if ((HEAP32[i26 >> 2] | 0) == ((HEAP32[i23 >> 2] | 0) + -1 | 0)) {
    i30 = HEAP32[i4 >> 2] | 0;
    if ((i30 | 0) > -1) {
     i31 = (HEAP32[i7 >> 2] | 0) + i30 | 0;
    } else {
     i31 = 0;
    }
    __tr_flush_block(i2, i31, (HEAP32[i5 >> 2] | 0) - i30 | 0, 0);
    HEAP32[i4 >> 2] = HEAP32[i5 >> 2];
    i33 = HEAP32[i2 >> 2] | 0;
    i32 = i33 + 28 | 0;
    i30 = HEAP32[i32 >> 2] | 0;
    i35 = HEAP32[i30 + 20 >> 2] | 0;
    i31 = i33 + 16 | 0;
    i34 = HEAP32[i31 >> 2] | 0;
    i34 = i35 >>> 0 > i34 >>> 0 ? i34 : i35;
    if ((i34 | 0) != 0 ? (i28 = i33 + 12 | 0, _memcpy(HEAP32[i28 >> 2] | 0, HEAP32[i30 + 16 >> 2] | 0, i34 | 0) | 0, HEAP32[i28 >> 2] = (HEAP32[i28 >> 2] | 0) + i34, i28 = (HEAP32[i32 >> 2] | 0) + 16 | 0, HEAP32[i28 >> 2] = (HEAP32[i28 >> 2] | 0) + i34, i28 = i33 + 20 | 0, HEAP32[i28 >> 2] = (HEAP32[i28 >> 2] | 0) + i34, HEAP32[i31 >> 2] = (HEAP32[i31 >> 2] | 0) - i34, i28 = HEAP32[i32 >> 2] | 0, i33 = i28 + 20 | 0, i35 = HEAP32[i33 >> 2] | 0, HEAP32[i33 >> 2] = i35 - i34, (i35 | 0) == (i34 | 0)) : 0) {
     HEAP32[i28 + 16 >> 2] = HEAP32[i28 + 8 >> 2];
    }
   }
   HEAP32[i5 >> 2] = (HEAP32[i5 >> 2] | 0) + 1;
   i30 = (HEAP32[i15 >> 2] | 0) + -1 | 0;
   HEAP32[i15 >> 2] = i30;
   if ((HEAP32[(HEAP32[i2 >> 2] | 0) + 16 >> 2] | 0) == 0) {
    i2 = 0;
    i30 = 50;
    break L1;
   }
  }
  i34 = HEAP32[i5 >> 2] | 0;
  i30 = i34 + -3 + (HEAP32[i15 >> 2] | 0) | 0;
  i35 = i31 + 253 | 0;
  i31 = i34 + 65535 - (HEAP32[i12 >> 2] | 0) | 0;
  i34 = HEAP32[i26 >> 2] | 0;
  HEAP16[(HEAP32[i27 >> 2] | 0) + (i34 << 1) >> 1] = i31;
  HEAP32[i26 >> 2] = i34 + 1;
  HEAP8[(HEAP32[i29 >> 2] | 0) + i34 | 0] = i35;
  i35 = i2 + ((HEAPU8[808 + (i35 & 255) | 0] | 0 | 256) + 1 << 2) + 148 | 0;
  HEAP16[i35 >> 1] = (HEAP16[i35 >> 1] | 0) + 1 << 16 >> 16;
  i31 = i31 + 65535 & 65535;
  if (!(i31 >>> 0 < 256)) {
   i31 = (i31 >>> 7) + 256 | 0;
  }
  i32 = i2 + ((HEAPU8[296 + i31 | 0] | 0) << 2) + 2440 | 0;
  HEAP16[i32 >> 1] = (HEAP16[i32 >> 1] | 0) + 1 << 16 >> 16;
  i32 = HEAP32[i26 >> 2] | 0;
  i31 = (HEAP32[i23 >> 2] | 0) + -1 | 0;
  i34 = HEAP32[i10 >> 2] | 0;
  HEAP32[i15 >> 2] = 1 - i34 + (HEAP32[i15 >> 2] | 0);
  i34 = i34 + -2 | 0;
  HEAP32[i10 >> 2] = i34;
  i33 = HEAP32[i5 >> 2] | 0;
  while (1) {
   i35 = i33 + 1 | 0;
   HEAP32[i5 >> 2] = i35;
   if (!(i35 >>> 0 > i30 >>> 0)) {
    i36 = ((HEAPU8[(HEAP32[i7 >> 2] | 0) + (i33 + 3) | 0] | 0) ^ HEAP32[i17 >> 2] << HEAP32[i18 >> 2]) & HEAP32[i19 >> 2];
    HEAP32[i17 >> 2] = i36;
    i36 = (HEAP32[i20 >> 2] | 0) + (i36 << 1) | 0;
    HEAP16[(HEAP32[i21 >> 2] | 0) + ((HEAP32[i22 >> 2] & i35) << 1) >> 1] = HEAP16[i36 >> 1] | 0;
    HEAP16[i36 >> 1] = i35;
   }
   i34 = i34 + -1 | 0;
   HEAP32[i10 >> 2] = i34;
   if ((i34 | 0) == 0) {
    break;
   } else {
    i33 = i35;
   }
  }
  HEAP32[i8 >> 2] = 0;
  HEAP32[i9 >> 2] = 2;
  i30 = i33 + 2 | 0;
  HEAP32[i5 >> 2] = i30;
  if ((i32 | 0) != (i31 | 0)) {
   continue;
  }
  i32 = HEAP32[i4 >> 2] | 0;
  if ((i32 | 0) > -1) {
   i31 = (HEAP32[i7 >> 2] | 0) + i32 | 0;
  } else {
   i31 = 0;
  }
  __tr_flush_block(i2, i31, i30 - i32 | 0, 0);
  HEAP32[i4 >> 2] = HEAP32[i5 >> 2];
  i33 = HEAP32[i2 >> 2] | 0;
  i31 = i33 + 28 | 0;
  i32 = HEAP32[i31 >> 2] | 0;
  i35 = HEAP32[i32 + 20 >> 2] | 0;
  i30 = i33 + 16 | 0;
  i34 = HEAP32[i30 >> 2] | 0;
  i34 = i35 >>> 0 > i34 >>> 0 ? i34 : i35;
  if ((i34 | 0) != 0 ? (i25 = i33 + 12 | 0, _memcpy(HEAP32[i25 >> 2] | 0, HEAP32[i32 + 16 >> 2] | 0, i34 | 0) | 0, HEAP32[i25 >> 2] = (HEAP32[i25 >> 2] | 0) + i34, i25 = (HEAP32[i31 >> 2] | 0) + 16 | 0, HEAP32[i25 >> 2] = (HEAP32[i25 >> 2] | 0) + i34, i25 = i33 + 20 | 0, HEAP32[i25 >> 2] = (HEAP32[i25 >> 2] | 0) + i34, HEAP32[i30 >> 2] = (HEAP32[i30 >> 2] | 0) - i34, i25 = HEAP32[i31 >> 2] | 0, i35 = i25 + 20 | 0, i36 = HEAP32[i35 >> 2] | 0, HEAP32[i35 >> 2] = i36 - i34, (i36 | 0) == (i34 | 0)) : 0) {
   HEAP32[i25 + 16 >> 2] = HEAP32[i25 + 8 >> 2];
  }
  if ((HEAP32[(HEAP32[i2 >> 2] | 0) + 16 >> 2] | 0) == 0) {
   i2 = 0;
   i30 = 50;
   break;
  }
 }
 if ((i30 | 0) == 40) {
  if ((HEAP32[i8 >> 2] | 0) != 0) {
   i36 = HEAP8[(HEAP32[i7 >> 2] | 0) + ((HEAP32[i5 >> 2] | 0) + -1) | 0] | 0;
   i35 = HEAP32[i26 >> 2] | 0;
   HEAP16[(HEAP32[i27 >> 2] | 0) + (i35 << 1) >> 1] = 0;
   HEAP32[i26 >> 2] = i35 + 1;
   HEAP8[(HEAP32[i29 >> 2] | 0) + i35 | 0] = i36;
   i36 = i2 + ((i36 & 255) << 2) + 148 | 0;
   HEAP16[i36 >> 1] = (HEAP16[i36 >> 1] | 0) + 1 << 16 >> 16;
   HEAP32[i8 >> 2] = 0;
  }
  i8 = HEAP32[i4 >> 2] | 0;
  if ((i8 | 0) > -1) {
   i7 = (HEAP32[i7 >> 2] | 0) + i8 | 0;
  } else {
   i7 = 0;
  }
  i6 = (i6 | 0) == 4;
  __tr_flush_block(i2, i7, (HEAP32[i5 >> 2] | 0) - i8 | 0, i6 & 1);
  HEAP32[i4 >> 2] = HEAP32[i5 >> 2];
  i4 = HEAP32[i2 >> 2] | 0;
  i7 = i4 + 28 | 0;
  i5 = HEAP32[i7 >> 2] | 0;
  i10 = HEAP32[i5 + 20 >> 2] | 0;
  i8 = i4 + 16 | 0;
  i9 = HEAP32[i8 >> 2] | 0;
  i9 = i10 >>> 0 > i9 >>> 0 ? i9 : i10;
  if ((i9 | 0) != 0 ? (i3 = i4 + 12 | 0, _memcpy(HEAP32[i3 >> 2] | 0, HEAP32[i5 + 16 >> 2] | 0, i9 | 0) | 0, HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) + i9, i3 = (HEAP32[i7 >> 2] | 0) + 16 | 0, HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) + i9, i3 = i4 + 20 | 0, HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) + i9, HEAP32[i8 >> 2] = (HEAP32[i8 >> 2] | 0) - i9, i3 = HEAP32[i7 >> 2] | 0, i35 = i3 + 20 | 0, i36 = HEAP32[i35 >> 2] | 0, HEAP32[i35 >> 2] = i36 - i9, (i36 | 0) == (i9 | 0)) : 0) {
   HEAP32[i3 + 16 >> 2] = HEAP32[i3 + 8 >> 2];
  }
  if ((HEAP32[(HEAP32[i2 >> 2] | 0) + 16 >> 2] | 0) == 0) {
   i36 = i6 ? 2 : 0;
   STACKTOP = i1;
   return i36 | 0;
  } else {
   i36 = i6 ? 3 : 1;
   STACKTOP = i1;
   return i36 | 0;
  }
 } else if ((i30 | 0) == 50) {
  STACKTOP = i1;
  return i2 | 0;
 }
 return 0;
}
function _inflate_fast(i7, i19) {
 i7 = i7 | 0;
 i19 = i19 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0;
 i1 = STACKTOP;
 i11 = HEAP32[i7 + 28 >> 2] | 0;
 i29 = HEAP32[i7 >> 2] | 0;
 i5 = i7 + 4 | 0;
 i8 = i29 + ((HEAP32[i5 >> 2] | 0) + -6) | 0;
 i9 = i7 + 12 | 0;
 i28 = HEAP32[i9 >> 2] | 0;
 i4 = i7 + 16 | 0;
 i25 = HEAP32[i4 >> 2] | 0;
 i6 = i28 + (i25 + -258) | 0;
 i17 = HEAP32[i11 + 44 >> 2] | 0;
 i12 = HEAP32[i11 + 48 >> 2] | 0;
 i18 = HEAP32[i11 + 52 >> 2] | 0;
 i3 = i11 + 56 | 0;
 i2 = i11 + 60 | 0;
 i16 = HEAP32[i11 + 76 >> 2] | 0;
 i13 = HEAP32[i11 + 80 >> 2] | 0;
 i14 = (1 << HEAP32[i11 + 84 >> 2]) + -1 | 0;
 i15 = (1 << HEAP32[i11 + 88 >> 2]) + -1 | 0;
 i19 = i28 + (i25 + ~i19) | 0;
 i25 = i11 + 7104 | 0;
 i20 = i18 + -1 | 0;
 i27 = (i12 | 0) == 0;
 i24 = (HEAP32[i11 + 40 >> 2] | 0) + -1 | 0;
 i21 = i24 + i12 | 0;
 i22 = i12 + -1 | 0;
 i23 = i19 + -1 | 0;
 i26 = i19 - i12 | 0;
 i31 = HEAP32[i2 >> 2] | 0;
 i30 = HEAP32[i3 >> 2] | 0;
 i29 = i29 + -1 | 0;
 i28 = i28 + -1 | 0;
 L1 : do {
  if (i31 >>> 0 < 15) {
   i37 = i29 + 2 | 0;
   i33 = i31 + 16 | 0;
   i30 = ((HEAPU8[i29 + 1 | 0] | 0) << i31) + i30 + ((HEAPU8[i37] | 0) << i31 + 8) | 0;
   i29 = i37;
  } else {
   i33 = i31;
  }
  i31 = i30 & i14;
  i34 = HEAP8[i16 + (i31 << 2) | 0] | 0;
  i32 = HEAP16[i16 + (i31 << 2) + 2 >> 1] | 0;
  i31 = HEAPU8[i16 + (i31 << 2) + 1 | 0] | 0;
  i30 = i30 >>> i31;
  i31 = i33 - i31 | 0;
  do {
   if (!(i34 << 24 >> 24 == 0)) {
    i33 = i34 & 255;
    while (1) {
     if ((i33 & 16 | 0) != 0) {
      break;
     }
     if ((i33 & 64 | 0) != 0) {
      i10 = 55;
      break L1;
     }
     i37 = (i30 & (1 << i33) + -1) + (i32 & 65535) | 0;
     i33 = HEAP8[i16 + (i37 << 2) | 0] | 0;
     i32 = HEAP16[i16 + (i37 << 2) + 2 >> 1] | 0;
     i37 = HEAPU8[i16 + (i37 << 2) + 1 | 0] | 0;
     i30 = i30 >>> i37;
     i31 = i31 - i37 | 0;
     if (i33 << 24 >> 24 == 0) {
      i10 = 6;
      break;
     } else {
      i33 = i33 & 255;
     }
    }
    if ((i10 | 0) == 6) {
     i32 = i32 & 255;
     i10 = 7;
     break;
    }
    i32 = i32 & 65535;
    i33 = i33 & 15;
    if ((i33 | 0) != 0) {
     if (i31 >>> 0 < i33 >>> 0) {
      i29 = i29 + 1 | 0;
      i35 = i31 + 8 | 0;
      i34 = ((HEAPU8[i29] | 0) << i31) + i30 | 0;
     } else {
      i35 = i31;
      i34 = i30;
     }
     i31 = i35 - i33 | 0;
     i30 = i34 >>> i33;
     i32 = (i34 & (1 << i33) + -1) + i32 | 0;
    }
    if (i31 >>> 0 < 15) {
     i37 = i29 + 2 | 0;
     i34 = i31 + 16 | 0;
     i30 = ((HEAPU8[i29 + 1 | 0] | 0) << i31) + i30 + ((HEAPU8[i37] | 0) << i31 + 8) | 0;
     i29 = i37;
    } else {
     i34 = i31;
    }
    i37 = i30 & i15;
    i33 = HEAP16[i13 + (i37 << 2) + 2 >> 1] | 0;
    i31 = HEAPU8[i13 + (i37 << 2) + 1 | 0] | 0;
    i30 = i30 >>> i31;
    i31 = i34 - i31 | 0;
    i34 = HEAPU8[i13 + (i37 << 2) | 0] | 0;
    if ((i34 & 16 | 0) == 0) {
     do {
      if ((i34 & 64 | 0) != 0) {
       i10 = 52;
       break L1;
      }
      i34 = (i30 & (1 << i34) + -1) + (i33 & 65535) | 0;
      i33 = HEAP16[i13 + (i34 << 2) + 2 >> 1] | 0;
      i37 = HEAPU8[i13 + (i34 << 2) + 1 | 0] | 0;
      i30 = i30 >>> i37;
      i31 = i31 - i37 | 0;
      i34 = HEAPU8[i13 + (i34 << 2) | 0] | 0;
     } while ((i34 & 16 | 0) == 0);
    }
    i33 = i33 & 65535;
    i34 = i34 & 15;
    if (i31 >>> 0 < i34 >>> 0) {
     i35 = i29 + 1 | 0;
     i30 = ((HEAPU8[i35] | 0) << i31) + i30 | 0;
     i36 = i31 + 8 | 0;
     if (i36 >>> 0 < i34 >>> 0) {
      i29 = i29 + 2 | 0;
      i31 = i31 + 16 | 0;
      i30 = ((HEAPU8[i29] | 0) << i36) + i30 | 0;
     } else {
      i31 = i36;
      i29 = i35;
     }
    }
    i33 = (i30 & (1 << i34) + -1) + i33 | 0;
    i30 = i30 >>> i34;
    i31 = i31 - i34 | 0;
    i35 = i28;
    i34 = i35 - i19 | 0;
    if (!(i33 >>> 0 > i34 >>> 0)) {
     i34 = i28 + (0 - i33) | 0;
     while (1) {
      HEAP8[i28 + 1 | 0] = HEAP8[i34 + 1 | 0] | 0;
      HEAP8[i28 + 2 | 0] = HEAP8[i34 + 2 | 0] | 0;
      i35 = i34 + 3 | 0;
      i33 = i28 + 3 | 0;
      HEAP8[i33] = HEAP8[i35] | 0;
      i32 = i32 + -3 | 0;
      if (!(i32 >>> 0 > 2)) {
       break;
      } else {
       i34 = i35;
       i28 = i33;
      }
     }
     if ((i32 | 0) == 0) {
      i28 = i33;
      break;
     }
     i33 = i28 + 4 | 0;
     HEAP8[i33] = HEAP8[i34 + 4 | 0] | 0;
     if (!(i32 >>> 0 > 1)) {
      i28 = i33;
      break;
     }
     i28 = i28 + 5 | 0;
     HEAP8[i28] = HEAP8[i34 + 5 | 0] | 0;
     break;
    }
    i34 = i33 - i34 | 0;
    if (i34 >>> 0 > i17 >>> 0 ? (HEAP32[i25 >> 2] | 0) != 0 : 0) {
     i10 = 22;
     break L1;
    }
    do {
     if (i27) {
      i36 = i18 + (i24 - i34) | 0;
      if (i34 >>> 0 < i32 >>> 0) {
       i32 = i32 - i34 | 0;
       i35 = i33 - i35 | 0;
       i37 = i28;
       do {
        i36 = i36 + 1 | 0;
        i37 = i37 + 1 | 0;
        HEAP8[i37] = HEAP8[i36] | 0;
        i34 = i34 + -1 | 0;
       } while ((i34 | 0) != 0);
       i33 = i28 + (i23 + i35 + (1 - i33)) | 0;
       i28 = i28 + (i19 + i35) | 0;
      } else {
       i33 = i36;
      }
     } else {
      if (!(i12 >>> 0 < i34 >>> 0)) {
       i36 = i18 + (i22 - i34) | 0;
       if (!(i34 >>> 0 < i32 >>> 0)) {
        i33 = i36;
        break;
       }
       i32 = i32 - i34 | 0;
       i35 = i33 - i35 | 0;
       i37 = i28;
       do {
        i36 = i36 + 1 | 0;
        i37 = i37 + 1 | 0;
        HEAP8[i37] = HEAP8[i36] | 0;
        i34 = i34 + -1 | 0;
       } while ((i34 | 0) != 0);
       i33 = i28 + (i23 + i35 + (1 - i33)) | 0;
       i28 = i28 + (i19 + i35) | 0;
       break;
      }
      i37 = i18 + (i21 - i34) | 0;
      i36 = i34 - i12 | 0;
      if (i36 >>> 0 < i32 >>> 0) {
       i32 = i32 - i36 | 0;
       i34 = i33 - i35 | 0;
       i35 = i28;
       do {
        i37 = i37 + 1 | 0;
        i35 = i35 + 1 | 0;
        HEAP8[i35] = HEAP8[i37] | 0;
        i36 = i36 + -1 | 0;
       } while ((i36 | 0) != 0);
       i35 = i28 + (i26 + i34) | 0;
       if (i12 >>> 0 < i32 >>> 0) {
        i32 = i32 - i12 | 0;
        i37 = i20;
        i36 = i12;
        do {
         i37 = i37 + 1 | 0;
         i35 = i35 + 1 | 0;
         HEAP8[i35] = HEAP8[i37] | 0;
         i36 = i36 + -1 | 0;
        } while ((i36 | 0) != 0);
        i33 = i28 + (i23 + i34 + (1 - i33)) | 0;
        i28 = i28 + (i19 + i34) | 0;
       } else {
        i33 = i20;
        i28 = i35;
       }
      } else {
       i33 = i37;
      }
     }
    } while (0);
    if (i32 >>> 0 > 2) {
     do {
      HEAP8[i28 + 1 | 0] = HEAP8[i33 + 1 | 0] | 0;
      HEAP8[i28 + 2 | 0] = HEAP8[i33 + 2 | 0] | 0;
      i33 = i33 + 3 | 0;
      i28 = i28 + 3 | 0;
      HEAP8[i28] = HEAP8[i33] | 0;
      i32 = i32 + -3 | 0;
     } while (i32 >>> 0 > 2);
    }
    if ((i32 | 0) != 0) {
     i34 = i28 + 1 | 0;
     HEAP8[i34] = HEAP8[i33 + 1 | 0] | 0;
     if (i32 >>> 0 > 1) {
      i28 = i28 + 2 | 0;
      HEAP8[i28] = HEAP8[i33 + 2 | 0] | 0;
     } else {
      i28 = i34;
     }
    }
   } else {
    i32 = i32 & 255;
    i10 = 7;
   }
  } while (0);
  if ((i10 | 0) == 7) {
   i10 = 0;
   i28 = i28 + 1 | 0;
   HEAP8[i28] = i32;
  }
 } while (i29 >>> 0 < i8 >>> 0 & i28 >>> 0 < i6 >>> 0);
 do {
  if ((i10 | 0) == 22) {
   HEAP32[i7 + 24 >> 2] = 14384;
   HEAP32[i11 >> 2] = 29;
  } else if ((i10 | 0) == 52) {
   HEAP32[i7 + 24 >> 2] = 14416;
   HEAP32[i11 >> 2] = 29;
  } else if ((i10 | 0) == 55) {
   if ((i33 & 32 | 0) == 0) {
    HEAP32[i7 + 24 >> 2] = 14440;
    HEAP32[i11 >> 2] = 29;
    break;
   } else {
    HEAP32[i11 >> 2] = 11;
    break;
   }
  }
 } while (0);
 i37 = i31 >>> 3;
 i11 = i29 + (0 - i37) | 0;
 i10 = i31 - (i37 << 3) | 0;
 i12 = (1 << i10) + -1 & i30;
 HEAP32[i7 >> 2] = i29 + (1 - i37);
 HEAP32[i9 >> 2] = i28 + 1;
 if (i11 >>> 0 < i8 >>> 0) {
  i7 = i8 - i11 | 0;
 } else {
  i7 = i8 - i11 | 0;
 }
 HEAP32[i5 >> 2] = i7 + 5;
 if (i28 >>> 0 < i6 >>> 0) {
  i37 = i6 - i28 | 0;
  i37 = i37 + 257 | 0;
  HEAP32[i4 >> 2] = i37;
  HEAP32[i3 >> 2] = i12;
  HEAP32[i2 >> 2] = i10;
  STACKTOP = i1;
  return;
 } else {
  i37 = i6 - i28 | 0;
  i37 = i37 + 257 | 0;
  HEAP32[i4 >> 2] = i37;
  HEAP32[i3 >> 2] = i12;
  HEAP32[i2 >> 2] = i10;
  STACKTOP = i1;
  return;
 }
}
function _send_tree(i2, i13, i12) {
 i2 = i2 | 0;
 i13 = i13 | 0;
 i12 = i12 | 0;
 var i1 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0;
 i11 = STACKTOP;
 i15 = HEAP16[i13 + 2 >> 1] | 0;
 i16 = i15 << 16 >> 16 == 0;
 i7 = i2 + 2754 | 0;
 i4 = i2 + 5820 | 0;
 i8 = i2 + 2752 | 0;
 i3 = i2 + 5816 | 0;
 i14 = i2 + 20 | 0;
 i10 = i2 + 8 | 0;
 i9 = i2 + 2758 | 0;
 i1 = i2 + 2756 | 0;
 i5 = i2 + 2750 | 0;
 i6 = i2 + 2748 | 0;
 i21 = i16 ? 138 : 7;
 i23 = i16 ? 3 : 4;
 i18 = 0;
 i15 = i15 & 65535;
 i24 = -1;
 L1 : while (1) {
  i20 = 0;
  while (1) {
   if ((i18 | 0) > (i12 | 0)) {
    break L1;
   }
   i18 = i18 + 1 | 0;
   i19 = HEAP16[i13 + (i18 << 2) + 2 >> 1] | 0;
   i16 = i19 & 65535;
   i22 = i20 + 1 | 0;
   i17 = (i15 | 0) == (i16 | 0);
   if (!((i22 | 0) < (i21 | 0) & i17)) {
    break;
   } else {
    i20 = i22;
   }
  }
  do {
   if ((i22 | 0) >= (i23 | 0)) {
    if ((i15 | 0) != 0) {
     if ((i15 | 0) == (i24 | 0)) {
      i23 = HEAP16[i3 >> 1] | 0;
      i21 = HEAP32[i4 >> 2] | 0;
      i20 = i22;
     } else {
      i22 = HEAPU16[i2 + (i15 << 2) + 2686 >> 1] | 0;
      i21 = HEAP32[i4 >> 2] | 0;
      i24 = HEAPU16[i2 + (i15 << 2) + 2684 >> 1] | 0;
      i25 = HEAPU16[i3 >> 1] | 0 | i24 << i21;
      i23 = i25 & 65535;
      HEAP16[i3 >> 1] = i23;
      if ((i21 | 0) > (16 - i22 | 0)) {
       i23 = HEAP32[i14 >> 2] | 0;
       HEAP32[i14 >> 2] = i23 + 1;
       HEAP8[(HEAP32[i10 >> 2] | 0) + i23 | 0] = i25;
       i23 = (HEAPU16[i3 >> 1] | 0) >>> 8 & 255;
       i21 = HEAP32[i14 >> 2] | 0;
       HEAP32[i14 >> 2] = i21 + 1;
       HEAP8[(HEAP32[i10 >> 2] | 0) + i21 | 0] = i23;
       i21 = HEAP32[i4 >> 2] | 0;
       i23 = i24 >>> (16 - i21 | 0) & 65535;
       HEAP16[i3 >> 1] = i23;
       i21 = i22 + -16 + i21 | 0;
      } else {
       i21 = i21 + i22 | 0;
      }
      HEAP32[i4 >> 2] = i21;
     }
     i22 = HEAPU16[i5 >> 1] | 0;
     i24 = HEAPU16[i6 >> 1] | 0;
     i23 = i23 & 65535 | i24 << i21;
     HEAP16[i3 >> 1] = i23;
     if ((i21 | 0) > (16 - i22 | 0)) {
      i21 = HEAP32[i14 >> 2] | 0;
      HEAP32[i14 >> 2] = i21 + 1;
      HEAP8[(HEAP32[i10 >> 2] | 0) + i21 | 0] = i23;
      i23 = (HEAPU16[i3 >> 1] | 0) >>> 8 & 255;
      i21 = HEAP32[i14 >> 2] | 0;
      HEAP32[i14 >> 2] = i21 + 1;
      HEAP8[(HEAP32[i10 >> 2] | 0) + i21 | 0] = i23;
      i21 = HEAP32[i4 >> 2] | 0;
      i23 = i24 >>> (16 - i21 | 0);
      HEAP16[i3 >> 1] = i23;
      i21 = i22 + -16 + i21 | 0;
     } else {
      i21 = i21 + i22 | 0;
     }
     HEAP32[i4 >> 2] = i21;
     i20 = i20 + 65533 & 65535;
     i22 = i23 & 65535 | i20 << i21;
     HEAP16[i3 >> 1] = i22;
     if ((i21 | 0) > 14) {
      i26 = HEAP32[i14 >> 2] | 0;
      HEAP32[i14 >> 2] = i26 + 1;
      HEAP8[(HEAP32[i10 >> 2] | 0) + i26 | 0] = i22;
      i26 = (HEAPU16[i3 >> 1] | 0) >>> 8 & 255;
      i27 = HEAP32[i14 >> 2] | 0;
      HEAP32[i14 >> 2] = i27 + 1;
      HEAP8[(HEAP32[i10 >> 2] | 0) + i27 | 0] = i26;
      i27 = HEAP32[i4 >> 2] | 0;
      HEAP16[i3 >> 1] = i20 >>> (16 - i27 | 0);
      HEAP32[i4 >> 2] = i27 + -14;
      break;
     } else {
      HEAP32[i4 >> 2] = i21 + 2;
      break;
     }
    }
    if ((i22 | 0) < 11) {
     i24 = HEAPU16[i7 >> 1] | 0;
     i23 = HEAP32[i4 >> 2] | 0;
     i21 = HEAPU16[i8 >> 1] | 0;
     i22 = HEAPU16[i3 >> 1] | 0 | i21 << i23;
     HEAP16[i3 >> 1] = i22;
     if ((i23 | 0) > (16 - i24 | 0)) {
      i27 = HEAP32[i14 >> 2] | 0;
      HEAP32[i14 >> 2] = i27 + 1;
      HEAP8[(HEAP32[i10 >> 2] | 0) + i27 | 0] = i22;
      i22 = (HEAPU16[i3 >> 1] | 0) >>> 8 & 255;
      i27 = HEAP32[i14 >> 2] | 0;
      HEAP32[i14 >> 2] = i27 + 1;
      HEAP8[(HEAP32[i10 >> 2] | 0) + i27 | 0] = i22;
      i27 = HEAP32[i4 >> 2] | 0;
      i22 = i21 >>> (16 - i27 | 0);
      HEAP16[i3 >> 1] = i22;
      i21 = i24 + -16 + i27 | 0;
     } else {
      i21 = i23 + i24 | 0;
     }
     HEAP32[i4 >> 2] = i21;
     i20 = i20 + 65534 & 65535;
     i22 = i22 & 65535 | i20 << i21;
     HEAP16[i3 >> 1] = i22;
     if ((i21 | 0) > 13) {
      i26 = HEAP32[i14 >> 2] | 0;
      HEAP32[i14 >> 2] = i26 + 1;
      HEAP8[(HEAP32[i10 >> 2] | 0) + i26 | 0] = i22;
      i26 = (HEAPU16[i3 >> 1] | 0) >>> 8 & 255;
      i27 = HEAP32[i14 >> 2] | 0;
      HEAP32[i14 >> 2] = i27 + 1;
      HEAP8[(HEAP32[i10 >> 2] | 0) + i27 | 0] = i26;
      i27 = HEAP32[i4 >> 2] | 0;
      HEAP16[i3 >> 1] = i20 >>> (16 - i27 | 0);
      HEAP32[i4 >> 2] = i27 + -13;
      break;
     } else {
      HEAP32[i4 >> 2] = i21 + 3;
      break;
     }
    } else {
     i21 = HEAPU16[i9 >> 1] | 0;
     i24 = HEAP32[i4 >> 2] | 0;
     i23 = HEAPU16[i1 >> 1] | 0;
     i22 = HEAPU16[i3 >> 1] | 0 | i23 << i24;
     HEAP16[i3 >> 1] = i22;
     if ((i24 | 0) > (16 - i21 | 0)) {
      i27 = HEAP32[i14 >> 2] | 0;
      HEAP32[i14 >> 2] = i27 + 1;
      HEAP8[(HEAP32[i10 >> 2] | 0) + i27 | 0] = i22;
      i22 = (HEAPU16[i3 >> 1] | 0) >>> 8 & 255;
      i27 = HEAP32[i14 >> 2] | 0;
      HEAP32[i14 >> 2] = i27 + 1;
      HEAP8[(HEAP32[i10 >> 2] | 0) + i27 | 0] = i22;
      i27 = HEAP32[i4 >> 2] | 0;
      i22 = i23 >>> (16 - i27 | 0);
      HEAP16[i3 >> 1] = i22;
      i21 = i21 + -16 + i27 | 0;
     } else {
      i21 = i24 + i21 | 0;
     }
     HEAP32[i4 >> 2] = i21;
     i20 = i20 + 65526 & 65535;
     i22 = i22 & 65535 | i20 << i21;
     HEAP16[i3 >> 1] = i22;
     if ((i21 | 0) > 9) {
      i26 = HEAP32[i14 >> 2] | 0;
      HEAP32[i14 >> 2] = i26 + 1;
      HEAP8[(HEAP32[i10 >> 2] | 0) + i26 | 0] = i22;
      i26 = (HEAPU16[i3 >> 1] | 0) >>> 8 & 255;
      i27 = HEAP32[i14 >> 2] | 0;
      HEAP32[i14 >> 2] = i27 + 1;
      HEAP8[(HEAP32[i10 >> 2] | 0) + i27 | 0] = i26;
      i27 = HEAP32[i4 >> 2] | 0;
      HEAP16[i3 >> 1] = i20 >>> (16 - i27 | 0);
      HEAP32[i4 >> 2] = i27 + -9;
      break;
     } else {
      HEAP32[i4 >> 2] = i21 + 7;
      break;
     }
    }
   } else {
    i20 = i2 + (i15 << 2) + 2686 | 0;
    i21 = i2 + (i15 << 2) + 2684 | 0;
    i23 = HEAP32[i4 >> 2] | 0;
    i26 = HEAP16[i3 >> 1] | 0;
    do {
     i24 = HEAPU16[i20 >> 1] | 0;
     i25 = HEAPU16[i21 >> 1] | 0;
     i27 = i26 & 65535 | i25 << i23;
     i26 = i27 & 65535;
     HEAP16[i3 >> 1] = i26;
     if ((i23 | 0) > (16 - i24 | 0)) {
      i26 = HEAP32[i14 >> 2] | 0;
      HEAP32[i14 >> 2] = i26 + 1;
      HEAP8[(HEAP32[i10 >> 2] | 0) + i26 | 0] = i27;
      i26 = (HEAPU16[i3 >> 1] | 0) >>> 8 & 255;
      i23 = HEAP32[i14 >> 2] | 0;
      HEAP32[i14 >> 2] = i23 + 1;
      HEAP8[(HEAP32[i10 >> 2] | 0) + i23 | 0] = i26;
      i23 = HEAP32[i4 >> 2] | 0;
      i26 = i25 >>> (16 - i23 | 0) & 65535;
      HEAP16[i3 >> 1] = i26;
      i23 = i24 + -16 + i23 | 0;
     } else {
      i23 = i23 + i24 | 0;
     }
     HEAP32[i4 >> 2] = i23;
     i22 = i22 + -1 | 0;
    } while ((i22 | 0) != 0);
   }
  } while (0);
  if (i19 << 16 >> 16 == 0) {
   i24 = i15;
   i21 = 138;
   i23 = 3;
   i15 = i16;
   continue;
  }
  i24 = i15;
  i21 = i17 ? 6 : 7;
  i23 = i17 ? 3 : 4;
  i15 = i16;
 }
 STACKTOP = i11;
 return;
}
function __tr_flush_block(i2, i4, i6, i3) {
 i2 = i2 | 0;
 i4 = i4 | 0;
 i6 = i6 | 0;
 i3 = i3 | 0;
 var i1 = 0, i5 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0;
 i1 = STACKTOP;
 if ((HEAP32[i2 + 132 >> 2] | 0) > 0) {
  i5 = (HEAP32[i2 >> 2] | 0) + 44 | 0;
  if ((HEAP32[i5 >> 2] | 0) == 2) {
   i8 = -201342849;
   i9 = 0;
   while (1) {
    if ((i8 & 1 | 0) != 0 ? (HEAP16[i2 + (i9 << 2) + 148 >> 1] | 0) != 0 : 0) {
     i8 = 0;
     break;
    }
    i9 = i9 + 1 | 0;
    if ((i9 | 0) < 32) {
     i8 = i8 >>> 1;
    } else {
     i7 = 6;
     break;
    }
   }
   L9 : do {
    if ((i7 | 0) == 6) {
     if (((HEAP16[i2 + 184 >> 1] | 0) == 0 ? (HEAP16[i2 + 188 >> 1] | 0) == 0 : 0) ? (HEAP16[i2 + 200 >> 1] | 0) == 0 : 0) {
      i8 = 32;
      while (1) {
       i7 = i8 + 1 | 0;
       if ((HEAP16[i2 + (i8 << 2) + 148 >> 1] | 0) != 0) {
        i8 = 1;
        break L9;
       }
       if ((i7 | 0) < 256) {
        i8 = i7;
       } else {
        i8 = 0;
        break;
       }
      }
     } else {
      i8 = 1;
     }
    }
   } while (0);
   HEAP32[i5 >> 2] = i8;
  }
  _build_tree(i2, i2 + 2840 | 0);
  _build_tree(i2, i2 + 2852 | 0);
  _scan_tree(i2, i2 + 148 | 0, HEAP32[i2 + 2844 >> 2] | 0);
  _scan_tree(i2, i2 + 2440 | 0, HEAP32[i2 + 2856 >> 2] | 0);
  _build_tree(i2, i2 + 2864 | 0);
  i5 = 18;
  while (1) {
   i7 = i5 + -1 | 0;
   if ((HEAP16[i2 + (HEAPU8[2888 + i5 | 0] << 2) + 2686 >> 1] | 0) != 0) {
    break;
   }
   if ((i7 | 0) > 2) {
    i5 = i7;
   } else {
    i5 = i7;
    break;
   }
  }
  i10 = i2 + 5800 | 0;
  i7 = (i5 * 3 | 0) + 17 + (HEAP32[i10 >> 2] | 0) | 0;
  HEAP32[i10 >> 2] = i7;
  i7 = (i7 + 10 | 0) >>> 3;
  i10 = ((HEAP32[i2 + 5804 >> 2] | 0) + 10 | 0) >>> 3;
  i9 = i10 >>> 0 > i7 >>> 0 ? i7 : i10;
 } else {
  i10 = i6 + 5 | 0;
  i5 = 0;
  i9 = i10;
 }
 do {
  if ((i6 + 4 | 0) >>> 0 > i9 >>> 0 | (i4 | 0) == 0) {
   i4 = i2 + 5820 | 0;
   i7 = HEAP32[i4 >> 2] | 0;
   i8 = (i7 | 0) > 13;
   if ((HEAP32[i2 + 136 >> 2] | 0) == 4 | (i10 | 0) == (i9 | 0)) {
    i9 = i3 + 2 & 65535;
    i6 = i2 + 5816 | 0;
    i5 = HEAPU16[i6 >> 1] | i9 << i7;
    HEAP16[i6 >> 1] = i5;
    if (i8) {
     i12 = i2 + 20 | 0;
     i13 = HEAP32[i12 >> 2] | 0;
     HEAP32[i12 >> 2] = i13 + 1;
     i14 = i2 + 8 | 0;
     HEAP8[(HEAP32[i14 >> 2] | 0) + i13 | 0] = i5;
     i13 = (HEAPU16[i6 >> 1] | 0) >>> 8 & 255;
     i5 = HEAP32[i12 >> 2] | 0;
     HEAP32[i12 >> 2] = i5 + 1;
     HEAP8[(HEAP32[i14 >> 2] | 0) + i5 | 0] = i13;
     i5 = HEAP32[i4 >> 2] | 0;
     HEAP16[i6 >> 1] = i9 >>> (16 - i5 | 0);
     i5 = i5 + -13 | 0;
    } else {
     i5 = i7 + 3 | 0;
    }
    HEAP32[i4 >> 2] = i5;
    _compress_block(i2, 1136, 2288);
    break;
   }
   i10 = i3 + 4 & 65535;
   i6 = i2 + 5816 | 0;
   i9 = HEAPU16[i6 >> 1] | i10 << i7;
   HEAP16[i6 >> 1] = i9;
   if (i8) {
    i13 = i2 + 20 | 0;
    i12 = HEAP32[i13 >> 2] | 0;
    HEAP32[i13 >> 2] = i12 + 1;
    i14 = i2 + 8 | 0;
    HEAP8[(HEAP32[i14 >> 2] | 0) + i12 | 0] = i9;
    i9 = (HEAPU16[i6 >> 1] | 0) >>> 8 & 255;
    i12 = HEAP32[i13 >> 2] | 0;
    HEAP32[i13 >> 2] = i12 + 1;
    HEAP8[(HEAP32[i14 >> 2] | 0) + i12 | 0] = i9;
    i12 = HEAP32[i4 >> 2] | 0;
    i9 = i10 >>> (16 - i12 | 0);
    HEAP16[i6 >> 1] = i9;
    i12 = i12 + -13 | 0;
   } else {
    i12 = i7 + 3 | 0;
   }
   HEAP32[i4 >> 2] = i12;
   i7 = HEAP32[i2 + 2844 >> 2] | 0;
   i8 = HEAP32[i2 + 2856 >> 2] | 0;
   i10 = i7 + 65280 & 65535;
   i11 = i9 & 65535 | i10 << i12;
   HEAP16[i6 >> 1] = i11;
   if ((i12 | 0) > 11) {
    i13 = i2 + 20 | 0;
    i9 = HEAP32[i13 >> 2] | 0;
    HEAP32[i13 >> 2] = i9 + 1;
    i14 = i2 + 8 | 0;
    HEAP8[(HEAP32[i14 >> 2] | 0) + i9 | 0] = i11;
    i11 = (HEAPU16[i6 >> 1] | 0) >>> 8 & 255;
    i9 = HEAP32[i13 >> 2] | 0;
    HEAP32[i13 >> 2] = i9 + 1;
    HEAP8[(HEAP32[i14 >> 2] | 0) + i9 | 0] = i11;
    i9 = HEAP32[i4 >> 2] | 0;
    i11 = i10 >>> (16 - i9 | 0);
    HEAP16[i6 >> 1] = i11;
    i9 = i9 + -11 | 0;
   } else {
    i9 = i12 + 5 | 0;
   }
   HEAP32[i4 >> 2] = i9;
   i10 = i8 & 65535;
   i11 = i10 << i9 | i11 & 65535;
   HEAP16[i6 >> 1] = i11;
   if ((i9 | 0) > 11) {
    i13 = i2 + 20 | 0;
    i9 = HEAP32[i13 >> 2] | 0;
    HEAP32[i13 >> 2] = i9 + 1;
    i14 = i2 + 8 | 0;
    HEAP8[(HEAP32[i14 >> 2] | 0) + i9 | 0] = i11;
    i11 = (HEAPU16[i6 >> 1] | 0) >>> 8 & 255;
    i9 = HEAP32[i13 >> 2] | 0;
    HEAP32[i13 >> 2] = i9 + 1;
    HEAP8[(HEAP32[i14 >> 2] | 0) + i9 | 0] = i11;
    i9 = HEAP32[i4 >> 2] | 0;
    i11 = i10 >>> (16 - i9 | 0);
    HEAP16[i6 >> 1] = i11;
    i9 = i9 + -11 | 0;
   } else {
    i9 = i9 + 5 | 0;
   }
   HEAP32[i4 >> 2] = i9;
   i10 = i5 + 65533 & 65535;
   i14 = i10 << i9 | i11 & 65535;
   HEAP16[i6 >> 1] = i14;
   if ((i9 | 0) > 12) {
    i12 = i2 + 20 | 0;
    i11 = HEAP32[i12 >> 2] | 0;
    HEAP32[i12 >> 2] = i11 + 1;
    i13 = i2 + 8 | 0;
    HEAP8[(HEAP32[i13 >> 2] | 0) + i11 | 0] = i14;
    i14 = (HEAPU16[i6 >> 1] | 0) >>> 8 & 255;
    i11 = HEAP32[i12 >> 2] | 0;
    HEAP32[i12 >> 2] = i11 + 1;
    HEAP8[(HEAP32[i13 >> 2] | 0) + i11 | 0] = i14;
    i11 = HEAP32[i4 >> 2] | 0;
    i14 = i10 >>> (16 - i11 | 0);
    HEAP16[i6 >> 1] = i14;
    i11 = i11 + -12 | 0;
   } else {
    i11 = i9 + 4 | 0;
   }
   HEAP32[i4 >> 2] = i11;
   if ((i5 | 0) > -1) {
    i10 = i2 + 20 | 0;
    i9 = i2 + 8 | 0;
    i12 = 0;
    while (1) {
     i13 = HEAPU16[i2 + (HEAPU8[2888 + i12 | 0] << 2) + 2686 >> 1] | 0;
     i14 = i13 << i11 | i14 & 65535;
     HEAP16[i6 >> 1] = i14;
     if ((i11 | 0) > 13) {
      i11 = HEAP32[i10 >> 2] | 0;
      HEAP32[i10 >> 2] = i11 + 1;
      HEAP8[(HEAP32[i9 >> 2] | 0) + i11 | 0] = i14;
      i14 = (HEAPU16[i6 >> 1] | 0) >>> 8 & 255;
      i11 = HEAP32[i10 >> 2] | 0;
      HEAP32[i10 >> 2] = i11 + 1;
      HEAP8[(HEAP32[i9 >> 2] | 0) + i11 | 0] = i14;
      i11 = HEAP32[i4 >> 2] | 0;
      i14 = i13 >>> (16 - i11 | 0);
      HEAP16[i6 >> 1] = i14;
      i11 = i11 + -13 | 0;
     } else {
      i11 = i11 + 3 | 0;
     }
     HEAP32[i4 >> 2] = i11;
     if ((i12 | 0) == (i5 | 0)) {
      break;
     } else {
      i12 = i12 + 1 | 0;
     }
    }
   }
   i13 = i2 + 148 | 0;
   _send_tree(i2, i13, i7);
   i14 = i2 + 2440 | 0;
   _send_tree(i2, i14, i8);
   _compress_block(i2, i13, i14);
  } else {
   __tr_stored_block(i2, i4, i6, i3);
  }
 } while (0);
 _init_block(i2);
 if ((i3 | 0) == 0) {
  STACKTOP = i1;
  return;
 }
 i3 = i2 + 5820 | 0;
 i4 = HEAP32[i3 >> 2] | 0;
 if ((i4 | 0) <= 8) {
  i5 = i2 + 5816 | 0;
  if ((i4 | 0) > 0) {
   i13 = HEAP16[i5 >> 1] & 255;
   i12 = i2 + 20 | 0;
   i14 = HEAP32[i12 >> 2] | 0;
   HEAP32[i12 >> 2] = i14 + 1;
   HEAP8[(HEAP32[i2 + 8 >> 2] | 0) + i14 | 0] = i13;
  }
 } else {
  i5 = i2 + 5816 | 0;
  i14 = HEAP16[i5 >> 1] & 255;
  i11 = i2 + 20 | 0;
  i12 = HEAP32[i11 >> 2] | 0;
  HEAP32[i11 >> 2] = i12 + 1;
  i13 = i2 + 8 | 0;
  HEAP8[(HEAP32[i13 >> 2] | 0) + i12 | 0] = i14;
  i12 = (HEAPU16[i5 >> 1] | 0) >>> 8 & 255;
  i14 = HEAP32[i11 >> 2] | 0;
  HEAP32[i11 >> 2] = i14 + 1;
  HEAP8[(HEAP32[i13 >> 2] | 0) + i14 | 0] = i12;
 }
 HEAP16[i5 >> 1] = 0;
 HEAP32[i3 >> 2] = 0;
 STACKTOP = i1;
 return;
}
function _deflate_fast(i3, i6) {
 i3 = i3 | 0;
 i6 = i6 | 0;
 var i1 = 0, i2 = 0, i4 = 0, i5 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0;
 i1 = STACKTOP;
 i20 = i3 + 116 | 0;
 i22 = (i6 | 0) == 0;
 i23 = i3 + 72 | 0;
 i24 = i3 + 88 | 0;
 i5 = i3 + 108 | 0;
 i7 = i3 + 56 | 0;
 i9 = i3 + 84 | 0;
 i10 = i3 + 68 | 0;
 i11 = i3 + 52 | 0;
 i12 = i3 + 64 | 0;
 i19 = i3 + 44 | 0;
 i21 = i3 + 96 | 0;
 i16 = i3 + 112 | 0;
 i13 = i3 + 5792 | 0;
 i17 = i3 + 5796 | 0;
 i18 = i3 + 5784 | 0;
 i14 = i3 + 5788 | 0;
 i15 = i3 + 128 | 0;
 i4 = i3 + 92 | 0;
 while (1) {
  if ((HEAP32[i20 >> 2] | 0) >>> 0 < 262) {
   _fill_window(i3);
   i25 = HEAP32[i20 >> 2] | 0;
   if (i25 >>> 0 < 262 & i22) {
    i2 = 0;
    i25 = 34;
    break;
   }
   if ((i25 | 0) == 0) {
    i25 = 26;
    break;
   }
   if (!(i25 >>> 0 > 2)) {
    i25 = 9;
   } else {
    i25 = 6;
   }
  } else {
   i25 = 6;
  }
  if ((i25 | 0) == 6) {
   i25 = 0;
   i26 = HEAP32[i5 >> 2] | 0;
   i34 = ((HEAPU8[(HEAP32[i7 >> 2] | 0) + (i26 + 2) | 0] | 0) ^ HEAP32[i23 >> 2] << HEAP32[i24 >> 2]) & HEAP32[i9 >> 2];
   HEAP32[i23 >> 2] = i34;
   i34 = (HEAP32[i10 >> 2] | 0) + (i34 << 1) | 0;
   i35 = HEAP16[i34 >> 1] | 0;
   HEAP16[(HEAP32[i12 >> 2] | 0) + ((HEAP32[i11 >> 2] & i26) << 1) >> 1] = i35;
   i27 = i35 & 65535;
   HEAP16[i34 >> 1] = i26;
   if (!(i35 << 16 >> 16 == 0) ? !((i26 - i27 | 0) >>> 0 > ((HEAP32[i19 >> 2] | 0) + -262 | 0) >>> 0) : 0) {
    i26 = _longest_match(i3, i27) | 0;
    HEAP32[i21 >> 2] = i26;
   } else {
    i25 = 9;
   }
  }
  if ((i25 | 0) == 9) {
   i26 = HEAP32[i21 >> 2] | 0;
  }
  do {
   if (i26 >>> 0 > 2) {
    i35 = i26 + 253 | 0;
    i25 = (HEAP32[i5 >> 2] | 0) - (HEAP32[i16 >> 2] | 0) | 0;
    i34 = HEAP32[i13 >> 2] | 0;
    HEAP16[(HEAP32[i17 >> 2] | 0) + (i34 << 1) >> 1] = i25;
    HEAP32[i13 >> 2] = i34 + 1;
    HEAP8[(HEAP32[i18 >> 2] | 0) + i34 | 0] = i35;
    i35 = i3 + ((HEAPU8[808 + (i35 & 255) | 0] | 0 | 256) + 1 << 2) + 148 | 0;
    HEAP16[i35 >> 1] = (HEAP16[i35 >> 1] | 0) + 1 << 16 >> 16;
    i25 = i25 + 65535 & 65535;
    if (!(i25 >>> 0 < 256)) {
     i25 = (i25 >>> 7) + 256 | 0;
    }
    i25 = i3 + ((HEAPU8[296 + i25 | 0] | 0) << 2) + 2440 | 0;
    HEAP16[i25 >> 1] = (HEAP16[i25 >> 1] | 0) + 1 << 16 >> 16;
    i25 = (HEAP32[i13 >> 2] | 0) == ((HEAP32[i14 >> 2] | 0) + -1 | 0) | 0;
    i26 = HEAP32[i21 >> 2] | 0;
    i35 = (HEAP32[i20 >> 2] | 0) - i26 | 0;
    HEAP32[i20 >> 2] = i35;
    if (!(i26 >>> 0 <= (HEAP32[i15 >> 2] | 0) >>> 0 & i35 >>> 0 > 2)) {
     i26 = (HEAP32[i5 >> 2] | 0) + i26 | 0;
     HEAP32[i5 >> 2] = i26;
     HEAP32[i21 >> 2] = 0;
     i34 = HEAP32[i7 >> 2] | 0;
     i35 = HEAPU8[i34 + i26 | 0] | 0;
     HEAP32[i23 >> 2] = i35;
     HEAP32[i23 >> 2] = ((HEAPU8[i34 + (i26 + 1) | 0] | 0) ^ i35 << HEAP32[i24 >> 2]) & HEAP32[i9 >> 2];
     break;
    }
    i30 = i26 + -1 | 0;
    HEAP32[i21 >> 2] = i30;
    i34 = HEAP32[i24 >> 2] | 0;
    i33 = HEAP32[i7 >> 2] | 0;
    i35 = HEAP32[i9 >> 2] | 0;
    i32 = HEAP32[i10 >> 2] | 0;
    i27 = HEAP32[i11 >> 2] | 0;
    i29 = HEAP32[i12 >> 2] | 0;
    i26 = HEAP32[i5 >> 2] | 0;
    i31 = HEAP32[i23 >> 2] | 0;
    while (1) {
     i28 = i26 + 1 | 0;
     HEAP32[i5 >> 2] = i28;
     i31 = ((HEAPU8[i33 + (i26 + 3) | 0] | 0) ^ i31 << i34) & i35;
     HEAP32[i23 >> 2] = i31;
     i36 = i32 + (i31 << 1) | 0;
     HEAP16[i29 + ((i27 & i28) << 1) >> 1] = HEAP16[i36 >> 1] | 0;
     HEAP16[i36 >> 1] = i28;
     i30 = i30 + -1 | 0;
     HEAP32[i21 >> 2] = i30;
     if ((i30 | 0) == 0) {
      break;
     } else {
      i26 = i28;
     }
    }
    i26 = i26 + 2 | 0;
    HEAP32[i5 >> 2] = i26;
   } else {
    i25 = HEAP8[(HEAP32[i7 >> 2] | 0) + (HEAP32[i5 >> 2] | 0) | 0] | 0;
    i26 = HEAP32[i13 >> 2] | 0;
    HEAP16[(HEAP32[i17 >> 2] | 0) + (i26 << 1) >> 1] = 0;
    HEAP32[i13 >> 2] = i26 + 1;
    HEAP8[(HEAP32[i18 >> 2] | 0) + i26 | 0] = i25;
    i25 = i3 + ((i25 & 255) << 2) + 148 | 0;
    HEAP16[i25 >> 1] = (HEAP16[i25 >> 1] | 0) + 1 << 16 >> 16;
    i25 = (HEAP32[i13 >> 2] | 0) == ((HEAP32[i14 >> 2] | 0) + -1 | 0) | 0;
    HEAP32[i20 >> 2] = (HEAP32[i20 >> 2] | 0) + -1;
    i26 = (HEAP32[i5 >> 2] | 0) + 1 | 0;
    HEAP32[i5 >> 2] = i26;
   }
  } while (0);
  if ((i25 | 0) == 0) {
   continue;
  }
  i25 = HEAP32[i4 >> 2] | 0;
  if ((i25 | 0) > -1) {
   i27 = (HEAP32[i7 >> 2] | 0) + i25 | 0;
  } else {
   i27 = 0;
  }
  __tr_flush_block(i3, i27, i26 - i25 | 0, 0);
  HEAP32[i4 >> 2] = HEAP32[i5 >> 2];
  i27 = HEAP32[i3 >> 2] | 0;
  i28 = i27 + 28 | 0;
  i25 = HEAP32[i28 >> 2] | 0;
  i30 = HEAP32[i25 + 20 >> 2] | 0;
  i26 = i27 + 16 | 0;
  i29 = HEAP32[i26 >> 2] | 0;
  i29 = i30 >>> 0 > i29 >>> 0 ? i29 : i30;
  if ((i29 | 0) != 0 ? (i8 = i27 + 12 | 0, _memcpy(HEAP32[i8 >> 2] | 0, HEAP32[i25 + 16 >> 2] | 0, i29 | 0) | 0, HEAP32[i8 >> 2] = (HEAP32[i8 >> 2] | 0) + i29, i8 = (HEAP32[i28 >> 2] | 0) + 16 | 0, HEAP32[i8 >> 2] = (HEAP32[i8 >> 2] | 0) + i29, i8 = i27 + 20 | 0, HEAP32[i8 >> 2] = (HEAP32[i8 >> 2] | 0) + i29, HEAP32[i26 >> 2] = (HEAP32[i26 >> 2] | 0) - i29, i8 = HEAP32[i28 >> 2] | 0, i35 = i8 + 20 | 0, i36 = HEAP32[i35 >> 2] | 0, HEAP32[i35 >> 2] = i36 - i29, (i36 | 0) == (i29 | 0)) : 0) {
   HEAP32[i8 + 16 >> 2] = HEAP32[i8 + 8 >> 2];
  }
  if ((HEAP32[(HEAP32[i3 >> 2] | 0) + 16 >> 2] | 0) == 0) {
   i2 = 0;
   i25 = 34;
   break;
  }
 }
 if ((i25 | 0) == 26) {
  i8 = HEAP32[i4 >> 2] | 0;
  if ((i8 | 0) > -1) {
   i7 = (HEAP32[i7 >> 2] | 0) + i8 | 0;
  } else {
   i7 = 0;
  }
  i6 = (i6 | 0) == 4;
  __tr_flush_block(i3, i7, (HEAP32[i5 >> 2] | 0) - i8 | 0, i6 & 1);
  HEAP32[i4 >> 2] = HEAP32[i5 >> 2];
  i5 = HEAP32[i3 >> 2] | 0;
  i7 = i5 + 28 | 0;
  i4 = HEAP32[i7 >> 2] | 0;
  i10 = HEAP32[i4 + 20 >> 2] | 0;
  i8 = i5 + 16 | 0;
  i9 = HEAP32[i8 >> 2] | 0;
  i9 = i10 >>> 0 > i9 >>> 0 ? i9 : i10;
  if ((i9 | 0) != 0 ? (i2 = i5 + 12 | 0, _memcpy(HEAP32[i2 >> 2] | 0, HEAP32[i4 + 16 >> 2] | 0, i9 | 0) | 0, HEAP32[i2 >> 2] = (HEAP32[i2 >> 2] | 0) + i9, i2 = (HEAP32[i7 >> 2] | 0) + 16 | 0, HEAP32[i2 >> 2] = (HEAP32[i2 >> 2] | 0) + i9, i2 = i5 + 20 | 0, HEAP32[i2 >> 2] = (HEAP32[i2 >> 2] | 0) + i9, HEAP32[i8 >> 2] = (HEAP32[i8 >> 2] | 0) - i9, i2 = HEAP32[i7 >> 2] | 0, i35 = i2 + 20 | 0, i36 = HEAP32[i35 >> 2] | 0, HEAP32[i35 >> 2] = i36 - i9, (i36 | 0) == (i9 | 0)) : 0) {
   HEAP32[i2 + 16 >> 2] = HEAP32[i2 + 8 >> 2];
  }
  if ((HEAP32[(HEAP32[i3 >> 2] | 0) + 16 >> 2] | 0) == 0) {
   i36 = i6 ? 2 : 0;
   STACKTOP = i1;
   return i36 | 0;
  } else {
   i36 = i6 ? 3 : 1;
   STACKTOP = i1;
   return i36 | 0;
  }
 } else if ((i25 | 0) == 34) {
  STACKTOP = i1;
  return i2 | 0;
 }
 return 0;
}
function _inflate_table(i11, i5, i13, i2, i1, i10) {
 i11 = i11 | 0;
 i5 = i5 | 0;
 i13 = i13 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 i10 = i10 | 0;
 var i3 = 0, i4 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i12 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0;
 i3 = STACKTOP;
 STACKTOP = STACKTOP + 64 | 0;
 i7 = i3 + 32 | 0;
 i12 = i3;
 i4 = i7 + 0 | 0;
 i9 = i4 + 32 | 0;
 do {
  HEAP16[i4 >> 1] = 0;
  i4 = i4 + 2 | 0;
 } while ((i4 | 0) < (i9 | 0));
 i14 = (i13 | 0) == 0;
 if (!i14) {
  i4 = 0;
  do {
   i32 = i7 + (HEAPU16[i5 + (i4 << 1) >> 1] << 1) | 0;
   HEAP16[i32 >> 1] = (HEAP16[i32 >> 1] | 0) + 1 << 16 >> 16;
   i4 = i4 + 1 | 0;
  } while ((i4 | 0) != (i13 | 0));
 }
 i4 = HEAP32[i1 >> 2] | 0;
 i9 = 15;
 while (1) {
  i15 = i9 + -1 | 0;
  if ((HEAP16[i7 + (i9 << 1) >> 1] | 0) != 0) {
   break;
  }
  if ((i15 | 0) == 0) {
   i6 = 7;
   break;
  } else {
   i9 = i15;
  }
 }
 if ((i6 | 0) == 7) {
  i32 = HEAP32[i2 >> 2] | 0;
  HEAP32[i2 >> 2] = i32 + 4;
  HEAP8[i32] = 64;
  HEAP8[i32 + 1 | 0] = 1;
  HEAP16[i32 + 2 >> 1] = 0;
  i32 = HEAP32[i2 >> 2] | 0;
  HEAP32[i2 >> 2] = i32 + 4;
  HEAP8[i32] = 64;
  HEAP8[i32 + 1 | 0] = 1;
  HEAP16[i32 + 2 >> 1] = 0;
  HEAP32[i1 >> 2] = 1;
  i32 = 0;
  STACKTOP = i3;
  return i32 | 0;
 }
 i4 = i4 >>> 0 > i9 >>> 0 ? i9 : i4;
 L12 : do {
  if (i9 >>> 0 > 1) {
   i27 = 1;
   while (1) {
    i15 = i27 + 1 | 0;
    if ((HEAP16[i7 + (i27 << 1) >> 1] | 0) != 0) {
     break L12;
    }
    if (i15 >>> 0 < i9 >>> 0) {
     i27 = i15;
    } else {
     i27 = i15;
     break;
    }
   }
  } else {
   i27 = 1;
  }
 } while (0);
 i4 = i4 >>> 0 < i27 >>> 0 ? i27 : i4;
 i16 = 1;
 i15 = 1;
 do {
  i16 = (i16 << 1) - (HEAPU16[i7 + (i15 << 1) >> 1] | 0) | 0;
  i15 = i15 + 1 | 0;
  if ((i16 | 0) < 0) {
   i8 = -1;
   i6 = 56;
   break;
  }
 } while (i15 >>> 0 < 16);
 if ((i6 | 0) == 56) {
  STACKTOP = i3;
  return i8 | 0;
 }
 if ((i16 | 0) > 0 ? !((i11 | 0) != 0 & (i9 | 0) == 1) : 0) {
  i32 = -1;
  STACKTOP = i3;
  return i32 | 0;
 }
 HEAP16[i12 + 2 >> 1] = 0;
 i16 = 0;
 i15 = 1;
 do {
  i16 = (HEAPU16[i7 + (i15 << 1) >> 1] | 0) + (i16 & 65535) | 0;
  i15 = i15 + 1 | 0;
  HEAP16[i12 + (i15 << 1) >> 1] = i16;
 } while ((i15 | 0) != 15);
 if (!i14) {
  i15 = 0;
  do {
   i14 = HEAP16[i5 + (i15 << 1) >> 1] | 0;
   if (!(i14 << 16 >> 16 == 0)) {
    i31 = i12 + ((i14 & 65535) << 1) | 0;
    i32 = HEAP16[i31 >> 1] | 0;
    HEAP16[i31 >> 1] = i32 + 1 << 16 >> 16;
    HEAP16[i10 + ((i32 & 65535) << 1) >> 1] = i15;
   }
   i15 = i15 + 1 | 0;
  } while ((i15 | 0) != (i13 | 0));
 }
 if ((i11 | 0) == 1) {
  i14 = 1 << i4;
  if (i14 >>> 0 > 851) {
   i32 = 1;
   STACKTOP = i3;
   return i32 | 0;
  } else {
   i16 = 0;
   i20 = 1;
   i17 = 14128 + -514 | 0;
   i19 = 256;
   i18 = 14192 + -514 | 0;
  }
 } else if ((i11 | 0) != 0) {
  i14 = 1 << i4;
  i16 = (i11 | 0) == 2;
  if (i16 & i14 >>> 0 > 591) {
   i32 = 1;
   STACKTOP = i3;
   return i32 | 0;
  } else {
   i20 = 0;
   i17 = 14256;
   i19 = -1;
   i18 = 14320;
  }
 } else {
  i16 = 0;
  i14 = 1 << i4;
  i20 = 0;
  i17 = i10;
  i19 = 19;
  i18 = i10;
 }
 i11 = i14 + -1 | 0;
 i12 = i4 & 255;
 i22 = i4;
 i21 = 0;
 i25 = 0;
 i13 = -1;
 i15 = HEAP32[i2 >> 2] | 0;
 i24 = 0;
 L44 : while (1) {
  i23 = 1 << i22;
  while (1) {
   i29 = i27 - i21 | 0;
   i22 = i29 & 255;
   i28 = HEAP16[i10 + (i24 << 1) >> 1] | 0;
   i30 = i28 & 65535;
   if ((i30 | 0) >= (i19 | 0)) {
    if ((i30 | 0) > (i19 | 0)) {
     i26 = HEAP16[i18 + (i30 << 1) >> 1] & 255;
     i28 = HEAP16[i17 + (i30 << 1) >> 1] | 0;
    } else {
     i26 = 96;
     i28 = 0;
    }
   } else {
    i26 = 0;
   }
   i31 = 1 << i29;
   i30 = i25 >>> i21;
   i32 = i23;
   while (1) {
    i29 = i32 - i31 | 0;
    i33 = i29 + i30 | 0;
    HEAP8[i15 + (i33 << 2) | 0] = i26;
    HEAP8[i15 + (i33 << 2) + 1 | 0] = i22;
    HEAP16[i15 + (i33 << 2) + 2 >> 1] = i28;
    if ((i32 | 0) == (i31 | 0)) {
     break;
    } else {
     i32 = i29;
    }
   }
   i26 = 1 << i27 + -1;
   while (1) {
    if ((i26 & i25 | 0) == 0) {
     break;
    } else {
     i26 = i26 >>> 1;
    }
   }
   if ((i26 | 0) == 0) {
    i25 = 0;
   } else {
    i25 = (i26 + -1 & i25) + i26 | 0;
   }
   i24 = i24 + 1 | 0;
   i32 = i7 + (i27 << 1) | 0;
   i33 = (HEAP16[i32 >> 1] | 0) + -1 << 16 >> 16;
   HEAP16[i32 >> 1] = i33;
   if (i33 << 16 >> 16 == 0) {
    if ((i27 | 0) == (i9 | 0)) {
     break L44;
    }
    i27 = HEAPU16[i5 + (HEAPU16[i10 + (i24 << 1) >> 1] << 1) >> 1] | 0;
   }
   if (!(i27 >>> 0 > i4 >>> 0)) {
    continue;
   }
   i26 = i25 & i11;
   if ((i26 | 0) != (i13 | 0)) {
    break;
   }
  }
  i28 = (i21 | 0) == 0 ? i4 : i21;
  i23 = i15 + (i23 << 2) | 0;
  i31 = i27 - i28 | 0;
  L67 : do {
   if (i27 >>> 0 < i9 >>> 0) {
    i29 = i27;
    i30 = i31;
    i31 = 1 << i31;
    while (1) {
     i31 = i31 - (HEAPU16[i7 + (i29 << 1) >> 1] | 0) | 0;
     if ((i31 | 0) < 1) {
      break L67;
     }
     i30 = i30 + 1 | 0;
     i29 = i30 + i28 | 0;
     if (i29 >>> 0 < i9 >>> 0) {
      i31 = i31 << 1;
     } else {
      break;
     }
    }
   } else {
    i30 = i31;
   }
  } while (0);
  i29 = (1 << i30) + i14 | 0;
  if (i20 & i29 >>> 0 > 851 | i16 & i29 >>> 0 > 591) {
   i8 = 1;
   i6 = 56;
   break;
  }
  HEAP8[(HEAP32[i2 >> 2] | 0) + (i26 << 2) | 0] = i30;
  HEAP8[(HEAP32[i2 >> 2] | 0) + (i26 << 2) + 1 | 0] = i12;
  i22 = HEAP32[i2 >> 2] | 0;
  HEAP16[i22 + (i26 << 2) + 2 >> 1] = (i23 - i22 | 0) >>> 2;
  i22 = i30;
  i21 = i28;
  i13 = i26;
  i15 = i23;
  i14 = i29;
 }
 if ((i6 | 0) == 56) {
  STACKTOP = i3;
  return i8 | 0;
 }
 L77 : do {
  if ((i25 | 0) != 0) {
   do {
    if ((i21 | 0) != 0) {
     if ((i25 & i11 | 0) != (i13 | 0)) {
      i21 = 0;
      i22 = i12;
      i9 = i4;
      i15 = HEAP32[i2 >> 2] | 0;
     }
    } else {
     i21 = 0;
    }
    i5 = i25 >>> i21;
    HEAP8[i15 + (i5 << 2) | 0] = 64;
    HEAP8[i15 + (i5 << 2) + 1 | 0] = i22;
    HEAP16[i15 + (i5 << 2) + 2 >> 1] = 0;
    i5 = 1 << i9 + -1;
    while (1) {
     if ((i5 & i25 | 0) == 0) {
      break;
     } else {
      i5 = i5 >>> 1;
     }
    }
    if ((i5 | 0) == 0) {
     break L77;
    }
    i25 = (i5 + -1 & i25) + i5 | 0;
   } while ((i25 | 0) != 0);
  }
 } while (0);
 HEAP32[i2 >> 2] = (HEAP32[i2 >> 2] | 0) + (i14 << 2);
 HEAP32[i1 >> 2] = i4;
 i33 = 0;
 STACKTOP = i3;
 return i33 | 0;
}
function _compress_block(i1, i3, i7) {
 i1 = i1 | 0;
 i3 = i3 | 0;
 i7 = i7 | 0;
 var i2 = 0, i4 = 0, i5 = 0, i6 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0;
 i2 = STACKTOP;
 i11 = i1 + 5792 | 0;
 if ((HEAP32[i11 >> 2] | 0) == 0) {
  i14 = HEAP32[i1 + 5820 >> 2] | 0;
  i17 = HEAP16[i1 + 5816 >> 1] | 0;
 } else {
  i9 = i1 + 5796 | 0;
  i10 = i1 + 5784 | 0;
  i8 = i1 + 5820 | 0;
  i12 = i1 + 5816 | 0;
  i5 = i1 + 20 | 0;
  i6 = i1 + 8 | 0;
  i14 = 0;
  while (1) {
   i20 = HEAP16[(HEAP32[i9 >> 2] | 0) + (i14 << 1) >> 1] | 0;
   i13 = i20 & 65535;
   i4 = i14 + 1 | 0;
   i14 = HEAPU8[(HEAP32[i10 >> 2] | 0) + i14 | 0] | 0;
   do {
    if (i20 << 16 >> 16 == 0) {
     i15 = HEAPU16[i3 + (i14 << 2) + 2 >> 1] | 0;
     i13 = HEAP32[i8 >> 2] | 0;
     i14 = HEAPU16[i3 + (i14 << 2) >> 1] | 0;
     i16 = HEAPU16[i12 >> 1] | 0 | i14 << i13;
     i17 = i16 & 65535;
     HEAP16[i12 >> 1] = i17;
     if ((i13 | 0) > (16 - i15 | 0)) {
      i17 = HEAP32[i5 >> 2] | 0;
      HEAP32[i5 >> 2] = i17 + 1;
      HEAP8[(HEAP32[i6 >> 2] | 0) + i17 | 0] = i16;
      i17 = (HEAPU16[i12 >> 1] | 0) >>> 8 & 255;
      i20 = HEAP32[i5 >> 2] | 0;
      HEAP32[i5 >> 2] = i20 + 1;
      HEAP8[(HEAP32[i6 >> 2] | 0) + i20 | 0] = i17;
      i20 = HEAP32[i8 >> 2] | 0;
      i17 = i14 >>> (16 - i20 | 0) & 65535;
      HEAP16[i12 >> 1] = i17;
      i14 = i15 + -16 + i20 | 0;
      HEAP32[i8 >> 2] = i14;
      break;
     } else {
      i14 = i13 + i15 | 0;
      HEAP32[i8 >> 2] = i14;
      break;
     }
    } else {
     i15 = HEAPU8[808 + i14 | 0] | 0;
     i19 = (i15 | 256) + 1 | 0;
     i18 = HEAPU16[i3 + (i19 << 2) + 2 >> 1] | 0;
     i17 = HEAP32[i8 >> 2] | 0;
     i19 = HEAPU16[i3 + (i19 << 2) >> 1] | 0;
     i20 = HEAPU16[i12 >> 1] | 0 | i19 << i17;
     i16 = i20 & 65535;
     HEAP16[i12 >> 1] = i16;
     if ((i17 | 0) > (16 - i18 | 0)) {
      i16 = HEAP32[i5 >> 2] | 0;
      HEAP32[i5 >> 2] = i16 + 1;
      HEAP8[(HEAP32[i6 >> 2] | 0) + i16 | 0] = i20;
      i16 = (HEAPU16[i12 >> 1] | 0) >>> 8 & 255;
      i20 = HEAP32[i5 >> 2] | 0;
      HEAP32[i5 >> 2] = i20 + 1;
      HEAP8[(HEAP32[i6 >> 2] | 0) + i20 | 0] = i16;
      i20 = HEAP32[i8 >> 2] | 0;
      i16 = i19 >>> (16 - i20 | 0) & 65535;
      HEAP16[i12 >> 1] = i16;
      i18 = i18 + -16 + i20 | 0;
     } else {
      i18 = i17 + i18 | 0;
     }
     HEAP32[i8 >> 2] = i18;
     i17 = HEAP32[2408 + (i15 << 2) >> 2] | 0;
     do {
      if ((i15 + -8 | 0) >>> 0 < 20) {
       i14 = i14 - (HEAP32[2528 + (i15 << 2) >> 2] | 0) & 65535;
       i15 = i14 << i18 | i16 & 65535;
       i16 = i15 & 65535;
       HEAP16[i12 >> 1] = i16;
       if ((i18 | 0) > (16 - i17 | 0)) {
        i16 = HEAP32[i5 >> 2] | 0;
        HEAP32[i5 >> 2] = i16 + 1;
        HEAP8[(HEAP32[i6 >> 2] | 0) + i16 | 0] = i15;
        i16 = (HEAPU16[i12 >> 1] | 0) >>> 8 & 255;
        i20 = HEAP32[i5 >> 2] | 0;
        HEAP32[i5 >> 2] = i20 + 1;
        HEAP8[(HEAP32[i6 >> 2] | 0) + i20 | 0] = i16;
        i20 = HEAP32[i8 >> 2] | 0;
        i16 = i14 >>> (16 - i20 | 0) & 65535;
        HEAP16[i12 >> 1] = i16;
        i14 = i17 + -16 + i20 | 0;
        HEAP32[i8 >> 2] = i14;
        break;
       } else {
        i14 = i18 + i17 | 0;
        HEAP32[i8 >> 2] = i14;
        break;
       }
      } else {
       i14 = i18;
      }
     } while (0);
     i13 = i13 + -1 | 0;
     if (i13 >>> 0 < 256) {
      i15 = i13;
     } else {
      i15 = (i13 >>> 7) + 256 | 0;
     }
     i15 = HEAPU8[296 + i15 | 0] | 0;
     i17 = HEAPU16[i7 + (i15 << 2) + 2 >> 1] | 0;
     i18 = HEAPU16[i7 + (i15 << 2) >> 1] | 0;
     i19 = i16 & 65535 | i18 << i14;
     i16 = i19 & 65535;
     HEAP16[i12 >> 1] = i16;
     if ((i14 | 0) > (16 - i17 | 0)) {
      i20 = HEAP32[i5 >> 2] | 0;
      HEAP32[i5 >> 2] = i20 + 1;
      HEAP8[(HEAP32[i6 >> 2] | 0) + i20 | 0] = i19;
      i20 = (HEAPU16[i12 >> 1] | 0) >>> 8 & 255;
      i14 = HEAP32[i5 >> 2] | 0;
      HEAP32[i5 >> 2] = i14 + 1;
      HEAP8[(HEAP32[i6 >> 2] | 0) + i14 | 0] = i20;
      i14 = HEAP32[i8 >> 2] | 0;
      i20 = i18 >>> (16 - i14 | 0) & 65535;
      HEAP16[i12 >> 1] = i20;
      i14 = i17 + -16 + i14 | 0;
      i17 = i20;
     } else {
      i14 = i14 + i17 | 0;
      i17 = i16;
     }
     HEAP32[i8 >> 2] = i14;
     i16 = HEAP32[2648 + (i15 << 2) >> 2] | 0;
     if ((i15 + -4 | 0) >>> 0 < 26) {
      i13 = i13 - (HEAP32[2768 + (i15 << 2) >> 2] | 0) & 65535;
      i15 = i13 << i14 | i17 & 65535;
      i17 = i15 & 65535;
      HEAP16[i12 >> 1] = i17;
      if ((i14 | 0) > (16 - i16 | 0)) {
       i17 = HEAP32[i5 >> 2] | 0;
       HEAP32[i5 >> 2] = i17 + 1;
       HEAP8[(HEAP32[i6 >> 2] | 0) + i17 | 0] = i15;
       i17 = (HEAPU16[i12 >> 1] | 0) >>> 8 & 255;
       i14 = HEAP32[i5 >> 2] | 0;
       HEAP32[i5 >> 2] = i14 + 1;
       HEAP8[(HEAP32[i6 >> 2] | 0) + i14 | 0] = i17;
       i14 = HEAP32[i8 >> 2] | 0;
       i17 = i13 >>> (16 - i14 | 0) & 65535;
       HEAP16[i12 >> 1] = i17;
       i14 = i16 + -16 + i14 | 0;
       HEAP32[i8 >> 2] = i14;
       break;
      } else {
       i14 = i14 + i16 | 0;
       HEAP32[i8 >> 2] = i14;
       break;
      }
     }
    }
   } while (0);
   if (i4 >>> 0 < (HEAP32[i11 >> 2] | 0) >>> 0) {
    i14 = i4;
   } else {
    break;
   }
  }
 }
 i5 = i3 + 1026 | 0;
 i6 = HEAPU16[i5 >> 1] | 0;
 i4 = i1 + 5820 | 0;
 i3 = HEAPU16[i3 + 1024 >> 1] | 0;
 i7 = i1 + 5816 | 0;
 i8 = i17 & 65535 | i3 << i14;
 HEAP16[i7 >> 1] = i8;
 if ((i14 | 0) > (16 - i6 | 0)) {
  i17 = i1 + 20 | 0;
  i18 = HEAP32[i17 >> 2] | 0;
  HEAP32[i17 >> 2] = i18 + 1;
  i20 = i1 + 8 | 0;
  HEAP8[(HEAP32[i20 >> 2] | 0) + i18 | 0] = i8;
  i18 = (HEAPU16[i7 >> 1] | 0) >>> 8 & 255;
  i19 = HEAP32[i17 >> 2] | 0;
  HEAP32[i17 >> 2] = i19 + 1;
  HEAP8[(HEAP32[i20 >> 2] | 0) + i19 | 0] = i18;
  i19 = HEAP32[i4 >> 2] | 0;
  HEAP16[i7 >> 1] = i3 >>> (16 - i19 | 0);
  i19 = i6 + -16 + i19 | 0;
  HEAP32[i4 >> 2] = i19;
  i19 = HEAP16[i5 >> 1] | 0;
  i19 = i19 & 65535;
  i20 = i1 + 5812 | 0;
  HEAP32[i20 >> 2] = i19;
  STACKTOP = i2;
  return;
 } else {
  i19 = i14 + i6 | 0;
  HEAP32[i4 >> 2] = i19;
  i19 = HEAP16[i5 >> 1] | 0;
  i19 = i19 & 65535;
  i20 = i1 + 5812 | 0;
  HEAP32[i20 >> 2] = i19;
  STACKTOP = i2;
  return;
 }
}
function _deflate_stored(i2, i5) {
 i2 = i2 | 0;
 i5 = i5 | 0;
 var i1 = 0, i3 = 0, i4 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0;
 i1 = STACKTOP;
 i4 = (HEAP32[i2 + 12 >> 2] | 0) + -5 | 0;
 i11 = i4 >>> 0 < 65535 ? i4 : 65535;
 i12 = i2 + 116 | 0;
 i4 = i2 + 108 | 0;
 i6 = i2 + 92 | 0;
 i10 = i2 + 44 | 0;
 i7 = i2 + 56 | 0;
 while (1) {
  i13 = HEAP32[i12 >> 2] | 0;
  if (i13 >>> 0 < 2) {
   _fill_window(i2);
   i13 = HEAP32[i12 >> 2] | 0;
   if ((i13 | i5 | 0) == 0) {
    i2 = 0;
    i8 = 28;
    break;
   }
   if ((i13 | 0) == 0) {
    i8 = 20;
    break;
   }
  }
  i13 = (HEAP32[i4 >> 2] | 0) + i13 | 0;
  HEAP32[i4 >> 2] = i13;
  HEAP32[i12 >> 2] = 0;
  i14 = HEAP32[i6 >> 2] | 0;
  i15 = i14 + i11 | 0;
  if (!((i13 | 0) != 0 & i13 >>> 0 < i15 >>> 0)) {
   HEAP32[i12 >> 2] = i13 - i15;
   HEAP32[i4 >> 2] = i15;
   if ((i14 | 0) > -1) {
    i13 = (HEAP32[i7 >> 2] | 0) + i14 | 0;
   } else {
    i13 = 0;
   }
   __tr_flush_block(i2, i13, i11, 0);
   HEAP32[i6 >> 2] = HEAP32[i4 >> 2];
   i16 = HEAP32[i2 >> 2] | 0;
   i14 = i16 + 28 | 0;
   i15 = HEAP32[i14 >> 2] | 0;
   i17 = HEAP32[i15 + 20 >> 2] | 0;
   i13 = i16 + 16 | 0;
   i18 = HEAP32[i13 >> 2] | 0;
   i17 = i17 >>> 0 > i18 >>> 0 ? i18 : i17;
   if ((i17 | 0) != 0 ? (i8 = i16 + 12 | 0, _memcpy(HEAP32[i8 >> 2] | 0, HEAP32[i15 + 16 >> 2] | 0, i17 | 0) | 0, HEAP32[i8 >> 2] = (HEAP32[i8 >> 2] | 0) + i17, i8 = (HEAP32[i14 >> 2] | 0) + 16 | 0, HEAP32[i8 >> 2] = (HEAP32[i8 >> 2] | 0) + i17, i8 = i16 + 20 | 0, HEAP32[i8 >> 2] = (HEAP32[i8 >> 2] | 0) + i17, HEAP32[i13 >> 2] = (HEAP32[i13 >> 2] | 0) - i17, i8 = HEAP32[i14 >> 2] | 0, i16 = i8 + 20 | 0, i18 = HEAP32[i16 >> 2] | 0, HEAP32[i16 >> 2] = i18 - i17, (i18 | 0) == (i17 | 0)) : 0) {
    HEAP32[i8 + 16 >> 2] = HEAP32[i8 + 8 >> 2];
   }
   if ((HEAP32[(HEAP32[i2 >> 2] | 0) + 16 >> 2] | 0) == 0) {
    i2 = 0;
    i8 = 28;
    break;
   }
   i14 = HEAP32[i6 >> 2] | 0;
   i13 = HEAP32[i4 >> 2] | 0;
  }
  i13 = i13 - i14 | 0;
  if (i13 >>> 0 < ((HEAP32[i10 >> 2] | 0) + -262 | 0) >>> 0) {
   continue;
  }
  if ((i14 | 0) > -1) {
   i14 = (HEAP32[i7 >> 2] | 0) + i14 | 0;
  } else {
   i14 = 0;
  }
  __tr_flush_block(i2, i14, i13, 0);
  HEAP32[i6 >> 2] = HEAP32[i4 >> 2];
  i16 = HEAP32[i2 >> 2] | 0;
  i14 = i16 + 28 | 0;
  i15 = HEAP32[i14 >> 2] | 0;
  i17 = HEAP32[i15 + 20 >> 2] | 0;
  i13 = i16 + 16 | 0;
  i18 = HEAP32[i13 >> 2] | 0;
  i17 = i17 >>> 0 > i18 >>> 0 ? i18 : i17;
  if ((i17 | 0) != 0 ? (i9 = i16 + 12 | 0, _memcpy(HEAP32[i9 >> 2] | 0, HEAP32[i15 + 16 >> 2] | 0, i17 | 0) | 0, HEAP32[i9 >> 2] = (HEAP32[i9 >> 2] | 0) + i17, i9 = (HEAP32[i14 >> 2] | 0) + 16 | 0, HEAP32[i9 >> 2] = (HEAP32[i9 >> 2] | 0) + i17, i9 = i16 + 20 | 0, HEAP32[i9 >> 2] = (HEAP32[i9 >> 2] | 0) + i17, HEAP32[i13 >> 2] = (HEAP32[i13 >> 2] | 0) - i17, i9 = HEAP32[i14 >> 2] | 0, i16 = i9 + 20 | 0, i18 = HEAP32[i16 >> 2] | 0, HEAP32[i16 >> 2] = i18 - i17, (i18 | 0) == (i17 | 0)) : 0) {
   HEAP32[i9 + 16 >> 2] = HEAP32[i9 + 8 >> 2];
  }
  if ((HEAP32[(HEAP32[i2 >> 2] | 0) + 16 >> 2] | 0) == 0) {
   i2 = 0;
   i8 = 28;
   break;
  }
 }
 if ((i8 | 0) == 20) {
  i8 = HEAP32[i6 >> 2] | 0;
  if ((i8 | 0) > -1) {
   i7 = (HEAP32[i7 >> 2] | 0) + i8 | 0;
  } else {
   i7 = 0;
  }
  i5 = (i5 | 0) == 4;
  __tr_flush_block(i2, i7, (HEAP32[i4 >> 2] | 0) - i8 | 0, i5 & 1);
  HEAP32[i6 >> 2] = HEAP32[i4 >> 2];
  i4 = HEAP32[i2 >> 2] | 0;
  i7 = i4 + 28 | 0;
  i6 = HEAP32[i7 >> 2] | 0;
  i9 = HEAP32[i6 + 20 >> 2] | 0;
  i8 = i4 + 16 | 0;
  i10 = HEAP32[i8 >> 2] | 0;
  i9 = i9 >>> 0 > i10 >>> 0 ? i10 : i9;
  if ((i9 | 0) != 0 ? (i3 = i4 + 12 | 0, _memcpy(HEAP32[i3 >> 2] | 0, HEAP32[i6 + 16 >> 2] | 0, i9 | 0) | 0, HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) + i9, i3 = (HEAP32[i7 >> 2] | 0) + 16 | 0, HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) + i9, i3 = i4 + 20 | 0, HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) + i9, HEAP32[i8 >> 2] = (HEAP32[i8 >> 2] | 0) - i9, i3 = HEAP32[i7 >> 2] | 0, i17 = i3 + 20 | 0, i18 = HEAP32[i17 >> 2] | 0, HEAP32[i17 >> 2] = i18 - i9, (i18 | 0) == (i9 | 0)) : 0) {
   HEAP32[i3 + 16 >> 2] = HEAP32[i3 + 8 >> 2];
  }
  if ((HEAP32[(HEAP32[i2 >> 2] | 0) + 16 >> 2] | 0) == 0) {
   i18 = i5 ? 2 : 0;
   STACKTOP = i1;
   return i18 | 0;
  } else {
   i18 = i5 ? 3 : 1;
   STACKTOP = i1;
   return i18 | 0;
  }
 } else if ((i8 | 0) == 28) {
  STACKTOP = i1;
  return i2 | 0;
 }
 return 0;
}
function _fill_window(i15) {
 i15 = i15 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0;
 i2 = STACKTOP;
 i16 = i15 + 44 | 0;
 i9 = HEAP32[i16 >> 2] | 0;
 i4 = i15 + 60 | 0;
 i8 = i15 + 116 | 0;
 i3 = i15 + 108 | 0;
 i5 = i9 + -262 | 0;
 i1 = i15 + 56 | 0;
 i17 = i15 + 72 | 0;
 i6 = i15 + 88 | 0;
 i7 = i15 + 84 | 0;
 i11 = i15 + 112 | 0;
 i12 = i15 + 92 | 0;
 i13 = i15 + 76 | 0;
 i14 = i15 + 68 | 0;
 i10 = i15 + 64 | 0;
 i19 = HEAP32[i8 >> 2] | 0;
 i21 = i9;
 while (1) {
  i20 = HEAP32[i3 >> 2] | 0;
  i19 = (HEAP32[i4 >> 2] | 0) - i19 - i20 | 0;
  if (!(i20 >>> 0 < (i5 + i21 | 0) >>> 0)) {
   i20 = HEAP32[i1 >> 2] | 0;
   _memcpy(i20 | 0, i20 + i9 | 0, i9 | 0) | 0;
   HEAP32[i11 >> 2] = (HEAP32[i11 >> 2] | 0) - i9;
   i20 = (HEAP32[i3 >> 2] | 0) - i9 | 0;
   HEAP32[i3 >> 2] = i20;
   HEAP32[i12 >> 2] = (HEAP32[i12 >> 2] | 0) - i9;
   i22 = HEAP32[i13 >> 2] | 0;
   i21 = i22;
   i22 = (HEAP32[i14 >> 2] | 0) + (i22 << 1) | 0;
   do {
    i22 = i22 + -2 | 0;
    i23 = HEAPU16[i22 >> 1] | 0;
    if (i23 >>> 0 < i9 >>> 0) {
     i23 = 0;
    } else {
     i23 = i23 - i9 & 65535;
    }
    HEAP16[i22 >> 1] = i23;
    i21 = i21 + -1 | 0;
   } while ((i21 | 0) != 0);
   i22 = i9;
   i21 = (HEAP32[i10 >> 2] | 0) + (i9 << 1) | 0;
   do {
    i21 = i21 + -2 | 0;
    i23 = HEAPU16[i21 >> 1] | 0;
    if (i23 >>> 0 < i9 >>> 0) {
     i23 = 0;
    } else {
     i23 = i23 - i9 & 65535;
    }
    HEAP16[i21 >> 1] = i23;
    i22 = i22 + -1 | 0;
   } while ((i22 | 0) != 0);
   i19 = i19 + i9 | 0;
  }
  i21 = HEAP32[i15 >> 2] | 0;
  i24 = i21 + 4 | 0;
  i23 = HEAP32[i24 >> 2] | 0;
  if ((i23 | 0) == 0) {
   i18 = 28;
   break;
  }
  i22 = HEAP32[i8 >> 2] | 0;
  i20 = (HEAP32[i1 >> 2] | 0) + (i22 + i20) | 0;
  i19 = i23 >>> 0 > i19 >>> 0 ? i19 : i23;
  if ((i19 | 0) == 0) {
   i19 = 0;
  } else {
   HEAP32[i24 >> 2] = i23 - i19;
   i22 = HEAP32[(HEAP32[i21 + 28 >> 2] | 0) + 24 >> 2] | 0;
   if ((i22 | 0) == 1) {
    i22 = i21 + 48 | 0;
    HEAP32[i22 >> 2] = _adler32(HEAP32[i22 >> 2] | 0, HEAP32[i21 >> 2] | 0, i19) | 0;
    i22 = i21;
   } else if ((i22 | 0) == 2) {
    i22 = i21 + 48 | 0;
    HEAP32[i22 >> 2] = _crc32(HEAP32[i22 >> 2] | 0, HEAP32[i21 >> 2] | 0, i19) | 0;
    i22 = i21;
   } else {
    i22 = i21;
   }
   _memcpy(i20 | 0, HEAP32[i22 >> 2] | 0, i19 | 0) | 0;
   HEAP32[i22 >> 2] = (HEAP32[i22 >> 2] | 0) + i19;
   i22 = i21 + 8 | 0;
   HEAP32[i22 >> 2] = (HEAP32[i22 >> 2] | 0) + i19;
   i22 = HEAP32[i8 >> 2] | 0;
  }
  i19 = i22 + i19 | 0;
  HEAP32[i8 >> 2] = i19;
  if (i19 >>> 0 > 2 ? (i23 = HEAP32[i3 >> 2] | 0, i22 = HEAP32[i1 >> 2] | 0, i24 = HEAPU8[i22 + i23 | 0] | 0, HEAP32[i17 >> 2] = i24, HEAP32[i17 >> 2] = ((HEAPU8[i22 + (i23 + 1) | 0] | 0) ^ i24 << HEAP32[i6 >> 2]) & HEAP32[i7 >> 2], !(i19 >>> 0 < 262)) : 0) {
   break;
  }
  if ((HEAP32[(HEAP32[i15 >> 2] | 0) + 4 >> 2] | 0) == 0) {
   break;
  }
  i21 = HEAP32[i16 >> 2] | 0;
 }
 if ((i18 | 0) == 28) {
  STACKTOP = i2;
  return;
 }
 i5 = i15 + 5824 | 0;
 i6 = HEAP32[i5 >> 2] | 0;
 i4 = HEAP32[i4 >> 2] | 0;
 if (!(i6 >>> 0 < i4 >>> 0)) {
  STACKTOP = i2;
  return;
 }
 i3 = i19 + (HEAP32[i3 >> 2] | 0) | 0;
 if (i6 >>> 0 < i3 >>> 0) {
  i4 = i4 - i3 | 0;
  i24 = i4 >>> 0 > 258 ? 258 : i4;
  _memset((HEAP32[i1 >> 2] | 0) + i3 | 0, 0, i24 | 0) | 0;
  HEAP32[i5 >> 2] = i24 + i3;
  STACKTOP = i2;
  return;
 }
 i3 = i3 + 258 | 0;
 if (!(i6 >>> 0 < i3 >>> 0)) {
  STACKTOP = i2;
  return;
 }
 i3 = i3 - i6 | 0;
 i4 = i4 - i6 | 0;
 i24 = i3 >>> 0 > i4 >>> 0 ? i4 : i3;
 _memset((HEAP32[i1 >> 2] | 0) + i6 | 0, 0, i24 | 0) | 0;
 HEAP32[i5 >> 2] = (HEAP32[i5 >> 2] | 0) + i24;
 STACKTOP = i2;
 return;
}
function __tr_align(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0;
 i2 = STACKTOP;
 i3 = i1 + 5820 | 0;
 i6 = HEAP32[i3 >> 2] | 0;
 i4 = i1 + 5816 | 0;
 i7 = HEAPU16[i4 >> 1] | 0 | 2 << i6;
 i5 = i7 & 65535;
 HEAP16[i4 >> 1] = i5;
 if ((i6 | 0) > 13) {
  i8 = i1 + 20 | 0;
  i6 = HEAP32[i8 >> 2] | 0;
  HEAP32[i8 >> 2] = i6 + 1;
  i5 = i1 + 8 | 0;
  HEAP8[(HEAP32[i5 >> 2] | 0) + i6 | 0] = i7;
  i7 = (HEAPU16[i4 >> 1] | 0) >>> 8 & 255;
  i6 = HEAP32[i8 >> 2] | 0;
  HEAP32[i8 >> 2] = i6 + 1;
  HEAP8[(HEAP32[i5 >> 2] | 0) + i6 | 0] = i7;
  i6 = HEAP32[i3 >> 2] | 0;
  i5 = 2 >>> (16 - i6 | 0) & 65535;
  HEAP16[i4 >> 1] = i5;
  i6 = i6 + -13 | 0;
 } else {
  i6 = i6 + 3 | 0;
 }
 HEAP32[i3 >> 2] = i6;
 if ((i6 | 0) > 9) {
  i7 = i1 + 20 | 0;
  i6 = HEAP32[i7 >> 2] | 0;
  HEAP32[i7 >> 2] = i6 + 1;
  i8 = i1 + 8 | 0;
  HEAP8[(HEAP32[i8 >> 2] | 0) + i6 | 0] = i5;
  i5 = (HEAPU16[i4 >> 1] | 0) >>> 8 & 255;
  i6 = HEAP32[i7 >> 2] | 0;
  HEAP32[i7 >> 2] = i6 + 1;
  HEAP8[(HEAP32[i8 >> 2] | 0) + i6 | 0] = i5;
  HEAP16[i4 >> 1] = 0;
  i6 = (HEAP32[i3 >> 2] | 0) + -9 | 0;
  i5 = 0;
 } else {
  i6 = i6 + 7 | 0;
 }
 HEAP32[i3 >> 2] = i6;
 if ((i6 | 0) != 16) {
  if ((i6 | 0) > 7) {
   i6 = i1 + 20 | 0;
   i7 = HEAP32[i6 >> 2] | 0;
   HEAP32[i6 >> 2] = i7 + 1;
   HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i7 | 0] = i5;
   i7 = (HEAPU16[i4 >> 1] | 0) >>> 8;
   HEAP16[i4 >> 1] = i7;
   i6 = (HEAP32[i3 >> 2] | 0) + -8 | 0;
   HEAP32[i3 >> 2] = i6;
  } else {
   i7 = i5;
  }
 } else {
  i9 = i1 + 20 | 0;
  i8 = HEAP32[i9 >> 2] | 0;
  HEAP32[i9 >> 2] = i8 + 1;
  i7 = i1 + 8 | 0;
  HEAP8[(HEAP32[i7 >> 2] | 0) + i8 | 0] = i5;
  i8 = (HEAPU16[i4 >> 1] | 0) >>> 8 & 255;
  i6 = HEAP32[i9 >> 2] | 0;
  HEAP32[i9 >> 2] = i6 + 1;
  HEAP8[(HEAP32[i7 >> 2] | 0) + i6 | 0] = i8;
  HEAP16[i4 >> 1] = 0;
  HEAP32[i3 >> 2] = 0;
  i6 = 0;
  i7 = 0;
 }
 i5 = i1 + 5812 | 0;
 if ((11 - i6 + (HEAP32[i5 >> 2] | 0) | 0) >= 9) {
  HEAP32[i5 >> 2] = 7;
  STACKTOP = i2;
  return;
 }
 i7 = i7 & 65535 | 2 << i6;
 HEAP16[i4 >> 1] = i7;
 if ((i6 | 0) > 13) {
  i8 = i1 + 20 | 0;
  i6 = HEAP32[i8 >> 2] | 0;
  HEAP32[i8 >> 2] = i6 + 1;
  i9 = i1 + 8 | 0;
  HEAP8[(HEAP32[i9 >> 2] | 0) + i6 | 0] = i7;
  i7 = (HEAPU16[i4 >> 1] | 0) >>> 8 & 255;
  i6 = HEAP32[i8 >> 2] | 0;
  HEAP32[i8 >> 2] = i6 + 1;
  HEAP8[(HEAP32[i9 >> 2] | 0) + i6 | 0] = i7;
  i6 = HEAP32[i3 >> 2] | 0;
  i7 = 2 >>> (16 - i6 | 0);
  HEAP16[i4 >> 1] = i7;
  i6 = i6 + -13 | 0;
 } else {
  i6 = i6 + 3 | 0;
 }
 i7 = i7 & 255;
 HEAP32[i3 >> 2] = i6;
 if ((i6 | 0) > 9) {
  i8 = i1 + 20 | 0;
  i9 = HEAP32[i8 >> 2] | 0;
  HEAP32[i8 >> 2] = i9 + 1;
  i6 = i1 + 8 | 0;
  HEAP8[(HEAP32[i6 >> 2] | 0) + i9 | 0] = i7;
  i9 = (HEAPU16[i4 >> 1] | 0) >>> 8 & 255;
  i7 = HEAP32[i8 >> 2] | 0;
  HEAP32[i8 >> 2] = i7 + 1;
  HEAP8[(HEAP32[i6 >> 2] | 0) + i7 | 0] = i9;
  HEAP16[i4 >> 1] = 0;
  i7 = 0;
  i6 = (HEAP32[i3 >> 2] | 0) + -9 | 0;
 } else {
  i6 = i6 + 7 | 0;
 }
 HEAP32[i3 >> 2] = i6;
 if ((i6 | 0) == 16) {
  i6 = i1 + 20 | 0;
  i9 = HEAP32[i6 >> 2] | 0;
  HEAP32[i6 >> 2] = i9 + 1;
  i8 = i1 + 8 | 0;
  HEAP8[(HEAP32[i8 >> 2] | 0) + i9 | 0] = i7;
  i7 = (HEAPU16[i4 >> 1] | 0) >>> 8 & 255;
  i9 = HEAP32[i6 >> 2] | 0;
  HEAP32[i6 >> 2] = i9 + 1;
  HEAP8[(HEAP32[i8 >> 2] | 0) + i9 | 0] = i7;
  HEAP16[i4 >> 1] = 0;
  HEAP32[i3 >> 2] = 0;
  HEAP32[i5 >> 2] = 7;
  STACKTOP = i2;
  return;
 }
 if ((i6 | 0) <= 7) {
  HEAP32[i5 >> 2] = 7;
  STACKTOP = i2;
  return;
 }
 i8 = i1 + 20 | 0;
 i9 = HEAP32[i8 >> 2] | 0;
 HEAP32[i8 >> 2] = i9 + 1;
 HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i9 | 0] = i7;
 HEAP16[i4 >> 1] = (HEAPU16[i4 >> 1] | 0) >>> 8;
 HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) + -8;
 HEAP32[i5 >> 2] = 7;
 STACKTOP = i2;
 return;
}
function _adler32(i6, i4, i5) {
 i6 = i6 | 0;
 i4 = i4 | 0;
 i5 = i5 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0;
 i1 = STACKTOP;
 i3 = i6 >>> 16;
 i6 = i6 & 65535;
 if ((i5 | 0) == 1) {
  i2 = (HEAPU8[i4] | 0) + i6 | 0;
  i2 = i2 >>> 0 > 65520 ? i2 + -65521 | 0 : i2;
  i3 = i2 + i3 | 0;
  i8 = (i3 >>> 0 > 65520 ? i3 + 15 | 0 : i3) << 16 | i2;
  STACKTOP = i1;
  return i8 | 0;
 }
 if ((i4 | 0) == 0) {
  i8 = 1;
  STACKTOP = i1;
  return i8 | 0;
 }
 if (i5 >>> 0 < 16) {
  if ((i5 | 0) != 0) {
   while (1) {
    i5 = i5 + -1 | 0;
    i6 = (HEAPU8[i4] | 0) + i6 | 0;
    i3 = i6 + i3 | 0;
    if ((i5 | 0) == 0) {
     break;
    } else {
     i4 = i4 + 1 | 0;
    }
   }
  }
  i8 = ((i3 >>> 0) % 65521 | 0) << 16 | (i6 >>> 0 > 65520 ? i6 + -65521 | 0 : i6);
  STACKTOP = i1;
  return i8 | 0;
 }
 if (i5 >>> 0 > 5551) {
  do {
   i5 = i5 + -5552 | 0;
   i7 = i4;
   i8 = 347;
   while (1) {
    i23 = (HEAPU8[i7] | 0) + i6 | 0;
    i22 = i23 + (HEAPU8[i7 + 1 | 0] | 0) | 0;
    i21 = i22 + (HEAPU8[i7 + 2 | 0] | 0) | 0;
    i20 = i21 + (HEAPU8[i7 + 3 | 0] | 0) | 0;
    i19 = i20 + (HEAPU8[i7 + 4 | 0] | 0) | 0;
    i18 = i19 + (HEAPU8[i7 + 5 | 0] | 0) | 0;
    i17 = i18 + (HEAPU8[i7 + 6 | 0] | 0) | 0;
    i16 = i17 + (HEAPU8[i7 + 7 | 0] | 0) | 0;
    i15 = i16 + (HEAPU8[i7 + 8 | 0] | 0) | 0;
    i14 = i15 + (HEAPU8[i7 + 9 | 0] | 0) | 0;
    i13 = i14 + (HEAPU8[i7 + 10 | 0] | 0) | 0;
    i12 = i13 + (HEAPU8[i7 + 11 | 0] | 0) | 0;
    i11 = i12 + (HEAPU8[i7 + 12 | 0] | 0) | 0;
    i10 = i11 + (HEAPU8[i7 + 13 | 0] | 0) | 0;
    i9 = i10 + (HEAPU8[i7 + 14 | 0] | 0) | 0;
    i6 = i9 + (HEAPU8[i7 + 15 | 0] | 0) | 0;
    i3 = i23 + i3 + i22 + i21 + i20 + i19 + i18 + i17 + i16 + i15 + i14 + i13 + i12 + i11 + i10 + i9 + i6 | 0;
    i8 = i8 + -1 | 0;
    if ((i8 | 0) == 0) {
     break;
    } else {
     i7 = i7 + 16 | 0;
    }
   }
   i4 = i4 + 5552 | 0;
   i6 = (i6 >>> 0) % 65521 | 0;
   i3 = (i3 >>> 0) % 65521 | 0;
  } while (i5 >>> 0 > 5551);
  if ((i5 | 0) != 0) {
   if (i5 >>> 0 > 15) {
    i2 = 15;
   } else {
    i2 = 16;
   }
  }
 } else {
  i2 = 15;
 }
 if ((i2 | 0) == 15) {
  while (1) {
   i5 = i5 + -16 | 0;
   i9 = (HEAPU8[i4] | 0) + i6 | 0;
   i10 = i9 + (HEAPU8[i4 + 1 | 0] | 0) | 0;
   i11 = i10 + (HEAPU8[i4 + 2 | 0] | 0) | 0;
   i12 = i11 + (HEAPU8[i4 + 3 | 0] | 0) | 0;
   i13 = i12 + (HEAPU8[i4 + 4 | 0] | 0) | 0;
   i14 = i13 + (HEAPU8[i4 + 5 | 0] | 0) | 0;
   i15 = i14 + (HEAPU8[i4 + 6 | 0] | 0) | 0;
   i16 = i15 + (HEAPU8[i4 + 7 | 0] | 0) | 0;
   i17 = i16 + (HEAPU8[i4 + 8 | 0] | 0) | 0;
   i18 = i17 + (HEAPU8[i4 + 9 | 0] | 0) | 0;
   i19 = i18 + (HEAPU8[i4 + 10 | 0] | 0) | 0;
   i20 = i19 + (HEAPU8[i4 + 11 | 0] | 0) | 0;
   i21 = i20 + (HEAPU8[i4 + 12 | 0] | 0) | 0;
   i22 = i21 + (HEAPU8[i4 + 13 | 0] | 0) | 0;
   i23 = i22 + (HEAPU8[i4 + 14 | 0] | 0) | 0;
   i6 = i23 + (HEAPU8[i4 + 15 | 0] | 0) | 0;
   i3 = i9 + i3 + i10 + i11 + i12 + i13 + i14 + i15 + i16 + i17 + i18 + i19 + i20 + i21 + i22 + i23 + i6 | 0;
   i4 = i4 + 16 | 0;
   if (!(i5 >>> 0 > 15)) {
    break;
   } else {
    i2 = 15;
   }
  }
  if ((i5 | 0) == 0) {
   i2 = 17;
  } else {
   i2 = 16;
  }
 }
 if ((i2 | 0) == 16) {
  while (1) {
   i5 = i5 + -1 | 0;
   i6 = (HEAPU8[i4] | 0) + i6 | 0;
   i3 = i6 + i3 | 0;
   if ((i5 | 0) == 0) {
    i2 = 17;
    break;
   } else {
    i4 = i4 + 1 | 0;
    i2 = 16;
   }
  }
 }
 if ((i2 | 0) == 17) {
  i6 = (i6 >>> 0) % 65521 | 0;
  i3 = (i3 >>> 0) % 65521 | 0;
 }
 i23 = i3 << 16 | i6;
 STACKTOP = i1;
 return i23 | 0;
}
function _crc32(i4, i2, i3) {
 i4 = i4 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i1 = 0, i5 = 0;
 i1 = STACKTOP;
 if ((i2 | 0) == 0) {
  i5 = 0;
  STACKTOP = i1;
  return i5 | 0;
 }
 i4 = ~i4;
 L4 : do {
  if ((i3 | 0) != 0) {
   while (1) {
    if ((i2 & 3 | 0) == 0) {
     break;
    }
    i4 = HEAP32[3192 + (((HEAPU8[i2] | 0) ^ i4 & 255) << 2) >> 2] ^ i4 >>> 8;
    i3 = i3 + -1 | 0;
    if ((i3 | 0) == 0) {
     break L4;
    } else {
     i2 = i2 + 1 | 0;
    }
   }
   if (i3 >>> 0 > 31) {
    while (1) {
     i4 = HEAP32[i2 >> 2] ^ i4;
     i4 = HEAP32[5240 + ((i4 >>> 8 & 255) << 2) >> 2] ^ HEAP32[6264 + ((i4 & 255) << 2) >> 2] ^ HEAP32[4216 + ((i4 >>> 16 & 255) << 2) >> 2] ^ HEAP32[3192 + (i4 >>> 24 << 2) >> 2] ^ HEAP32[i2 + 4 >> 2];
     i4 = HEAP32[5240 + ((i4 >>> 8 & 255) << 2) >> 2] ^ HEAP32[6264 + ((i4 & 255) << 2) >> 2] ^ HEAP32[4216 + ((i4 >>> 16 & 255) << 2) >> 2] ^ HEAP32[3192 + (i4 >>> 24 << 2) >> 2] ^ HEAP32[i2 + 8 >> 2];
     i4 = HEAP32[5240 + ((i4 >>> 8 & 255) << 2) >> 2] ^ HEAP32[6264 + ((i4 & 255) << 2) >> 2] ^ HEAP32[4216 + ((i4 >>> 16 & 255) << 2) >> 2] ^ HEAP32[3192 + (i4 >>> 24 << 2) >> 2] ^ HEAP32[i2 + 12 >> 2];
     i4 = HEAP32[5240 + ((i4 >>> 8 & 255) << 2) >> 2] ^ HEAP32[6264 + ((i4 & 255) << 2) >> 2] ^ HEAP32[4216 + ((i4 >>> 16 & 255) << 2) >> 2] ^ HEAP32[3192 + (i4 >>> 24 << 2) >> 2] ^ HEAP32[i2 + 16 >> 2];
     i4 = HEAP32[5240 + ((i4 >>> 8 & 255) << 2) >> 2] ^ HEAP32[6264 + ((i4 & 255) << 2) >> 2] ^ HEAP32[4216 + ((i4 >>> 16 & 255) << 2) >> 2] ^ HEAP32[3192 + (i4 >>> 24 << 2) >> 2] ^ HEAP32[i2 + 20 >> 2];
     i4 = HEAP32[5240 + ((i4 >>> 8 & 255) << 2) >> 2] ^ HEAP32[6264 + ((i4 & 255) << 2) >> 2] ^ HEAP32[4216 + ((i4 >>> 16 & 255) << 2) >> 2] ^ HEAP32[3192 + (i4 >>> 24 << 2) >> 2] ^ HEAP32[i2 + 24 >> 2];
     i5 = i2 + 32 | 0;
     i4 = HEAP32[5240 + ((i4 >>> 8 & 255) << 2) >> 2] ^ HEAP32[6264 + ((i4 & 255) << 2) >> 2] ^ HEAP32[4216 + ((i4 >>> 16 & 255) << 2) >> 2] ^ HEAP32[3192 + (i4 >>> 24 << 2) >> 2] ^ HEAP32[i2 + 28 >> 2];
     i4 = HEAP32[5240 + ((i4 >>> 8 & 255) << 2) >> 2] ^ HEAP32[6264 + ((i4 & 255) << 2) >> 2] ^ HEAP32[4216 + ((i4 >>> 16 & 255) << 2) >> 2] ^ HEAP32[3192 + (i4 >>> 24 << 2) >> 2];
     i3 = i3 + -32 | 0;
     if (i3 >>> 0 > 31) {
      i2 = i5;
     } else {
      i2 = i5;
      break;
     }
    }
   }
   if (i3 >>> 0 > 3) {
    while (1) {
     i5 = i2 + 4 | 0;
     i4 = HEAP32[i2 >> 2] ^ i4;
     i4 = HEAP32[5240 + ((i4 >>> 8 & 255) << 2) >> 2] ^ HEAP32[6264 + ((i4 & 255) << 2) >> 2] ^ HEAP32[4216 + ((i4 >>> 16 & 255) << 2) >> 2] ^ HEAP32[3192 + (i4 >>> 24 << 2) >> 2];
     i3 = i3 + -4 | 0;
     if (i3 >>> 0 > 3) {
      i2 = i5;
     } else {
      i2 = i5;
      break;
     }
    }
   }
   if ((i3 | 0) != 0) {
    while (1) {
     i4 = HEAP32[3192 + (((HEAPU8[i2] | 0) ^ i4 & 255) << 2) >> 2] ^ i4 >>> 8;
     i3 = i3 + -1 | 0;
     if ((i3 | 0) == 0) {
      break;
     } else {
      i2 = i2 + 1 | 0;
     }
    }
   }
  }
 } while (0);
 i5 = ~i4;
 STACKTOP = i1;
 return i5 | 0;
}
function _deflateInit2_(i3, i7, i8, i10, i4, i1, i5, i6) {
 i3 = i3 | 0;
 i7 = i7 | 0;
 i8 = i8 | 0;
 i10 = i10 | 0;
 i4 = i4 | 0;
 i1 = i1 | 0;
 i5 = i5 | 0;
 i6 = i6 | 0;
 var i2 = 0, i9 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0;
 i2 = STACKTOP;
 if ((i5 | 0) == 0) {
  i12 = -6;
  STACKTOP = i2;
  return i12 | 0;
 }
 if (!((HEAP8[i5] | 0) == 49 & (i6 | 0) == 56)) {
  i12 = -6;
  STACKTOP = i2;
  return i12 | 0;
 }
 if ((i3 | 0) == 0) {
  i12 = -2;
  STACKTOP = i2;
  return i12 | 0;
 }
 i5 = i3 + 24 | 0;
 HEAP32[i5 >> 2] = 0;
 i6 = i3 + 32 | 0;
 i9 = HEAP32[i6 >> 2] | 0;
 if ((i9 | 0) == 0) {
  HEAP32[i6 >> 2] = 1;
  HEAP32[i3 + 40 >> 2] = 0;
  i9 = 1;
 }
 i11 = i3 + 36 | 0;
 if ((HEAP32[i11 >> 2] | 0) == 0) {
  HEAP32[i11 >> 2] = 1;
 }
 i7 = (i7 | 0) == -1 ? 6 : i7;
 if ((i10 | 0) < 0) {
  i10 = 0 - i10 | 0;
  i11 = 0;
 } else {
  i11 = (i10 | 0) > 15;
  i10 = i11 ? i10 + -16 | 0 : i10;
  i11 = i11 ? 2 : 1;
 }
 if (!((i4 + -1 | 0) >>> 0 < 9 & (i8 | 0) == 8)) {
  i12 = -2;
  STACKTOP = i2;
  return i12 | 0;
 }
 if ((i10 + -8 | 0) >>> 0 > 7 | i7 >>> 0 > 9 | i1 >>> 0 > 4) {
  i12 = -2;
  STACKTOP = i2;
  return i12 | 0;
 }
 i12 = (i10 | 0) == 8 ? 9 : i10;
 i10 = i3 + 40 | 0;
 i8 = FUNCTION_TABLE_iiii[i9 & 1](HEAP32[i10 >> 2] | 0, 1, 5828) | 0;
 if ((i8 | 0) == 0) {
  i12 = -4;
  STACKTOP = i2;
  return i12 | 0;
 }
 HEAP32[i3 + 28 >> 2] = i8;
 HEAP32[i8 >> 2] = i3;
 HEAP32[i8 + 24 >> 2] = i11;
 HEAP32[i8 + 28 >> 2] = 0;
 HEAP32[i8 + 48 >> 2] = i12;
 i14 = 1 << i12;
 i11 = i8 + 44 | 0;
 HEAP32[i11 >> 2] = i14;
 HEAP32[i8 + 52 >> 2] = i14 + -1;
 i12 = i4 + 7 | 0;
 HEAP32[i8 + 80 >> 2] = i12;
 i12 = 1 << i12;
 i13 = i8 + 76 | 0;
 HEAP32[i13 >> 2] = i12;
 HEAP32[i8 + 84 >> 2] = i12 + -1;
 HEAP32[i8 + 88 >> 2] = ((i4 + 9 | 0) >>> 0) / 3 | 0;
 i12 = i8 + 56 | 0;
 HEAP32[i12 >> 2] = FUNCTION_TABLE_iiii[HEAP32[i6 >> 2] & 1](HEAP32[i10 >> 2] | 0, i14, 2) | 0;
 i14 = FUNCTION_TABLE_iiii[HEAP32[i6 >> 2] & 1](HEAP32[i10 >> 2] | 0, HEAP32[i11 >> 2] | 0, 2) | 0;
 i9 = i8 + 64 | 0;
 HEAP32[i9 >> 2] = i14;
 _memset(i14 | 0, 0, HEAP32[i11 >> 2] << 1 | 0) | 0;
 i11 = i8 + 68 | 0;
 HEAP32[i11 >> 2] = FUNCTION_TABLE_iiii[HEAP32[i6 >> 2] & 1](HEAP32[i10 >> 2] | 0, HEAP32[i13 >> 2] | 0, 2) | 0;
 HEAP32[i8 + 5824 >> 2] = 0;
 i4 = 1 << i4 + 6;
 i13 = i8 + 5788 | 0;
 HEAP32[i13 >> 2] = i4;
 i4 = FUNCTION_TABLE_iiii[HEAP32[i6 >> 2] & 1](HEAP32[i10 >> 2] | 0, i4, 4) | 0;
 HEAP32[i8 + 8 >> 2] = i4;
 i6 = HEAP32[i13 >> 2] | 0;
 HEAP32[i8 + 12 >> 2] = i6 << 2;
 if (((HEAP32[i12 >> 2] | 0) != 0 ? (HEAP32[i9 >> 2] | 0) != 0 : 0) ? !((HEAP32[i11 >> 2] | 0) == 0 | (i4 | 0) == 0) : 0) {
  HEAP32[i8 + 5796 >> 2] = i4 + (i6 >>> 1 << 1);
  HEAP32[i8 + 5784 >> 2] = i4 + (i6 * 3 | 0);
  HEAP32[i8 + 132 >> 2] = i7;
  HEAP32[i8 + 136 >> 2] = i1;
  HEAP8[i8 + 36 | 0] = 8;
  i14 = _deflateReset(i3) | 0;
  STACKTOP = i2;
  return i14 | 0;
 }
 HEAP32[i8 + 4 >> 2] = 666;
 HEAP32[i5 >> 2] = HEAP32[3176 >> 2];
 _deflateEnd(i3) | 0;
 i14 = -4;
 STACKTOP = i2;
 return i14 | 0;
}
function _longest_match(i19, i16) {
 i19 = i19 | 0;
 i16 = i16 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i17 = 0, i18 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0;
 i1 = STACKTOP;
 i18 = HEAP32[i19 + 124 >> 2] | 0;
 i3 = HEAP32[i19 + 56 >> 2] | 0;
 i5 = HEAP32[i19 + 108 >> 2] | 0;
 i4 = i3 + i5 | 0;
 i20 = HEAP32[i19 + 120 >> 2] | 0;
 i10 = HEAP32[i19 + 144 >> 2] | 0;
 i2 = (HEAP32[i19 + 44 >> 2] | 0) + -262 | 0;
 i8 = i5 >>> 0 > i2 >>> 0 ? i5 - i2 | 0 : 0;
 i6 = HEAP32[i19 + 64 >> 2] | 0;
 i7 = HEAP32[i19 + 52 >> 2] | 0;
 i9 = i3 + (i5 + 258) | 0;
 i2 = HEAP32[i19 + 116 >> 2] | 0;
 i12 = i10 >>> 0 > i2 >>> 0 ? i2 : i10;
 i11 = i19 + 112 | 0;
 i15 = i3 + (i5 + 1) | 0;
 i14 = i3 + (i5 + 2) | 0;
 i13 = i9;
 i10 = i5 + 257 | 0;
 i17 = i20;
 i18 = i20 >>> 0 < (HEAP32[i19 + 140 >> 2] | 0) >>> 0 ? i18 : i18 >>> 2;
 i19 = HEAP8[i3 + (i20 + i5) | 0] | 0;
 i20 = HEAP8[i3 + (i5 + -1 + i20) | 0] | 0;
 while (1) {
  i21 = i3 + i16 | 0;
  if ((((HEAP8[i3 + (i16 + i17) | 0] | 0) == i19 << 24 >> 24 ? (HEAP8[i3 + (i17 + -1 + i16) | 0] | 0) == i20 << 24 >> 24 : 0) ? (HEAP8[i21] | 0) == (HEAP8[i4] | 0) : 0) ? (HEAP8[i3 + (i16 + 1) | 0] | 0) == (HEAP8[i15] | 0) : 0) {
   i21 = i3 + (i16 + 2) | 0;
   i22 = i14;
   do {
    i23 = i22 + 1 | 0;
    if ((HEAP8[i23] | 0) != (HEAP8[i21 + 1 | 0] | 0)) {
     i22 = i23;
     break;
    }
    i23 = i22 + 2 | 0;
    if ((HEAP8[i23] | 0) != (HEAP8[i21 + 2 | 0] | 0)) {
     i22 = i23;
     break;
    }
    i23 = i22 + 3 | 0;
    if ((HEAP8[i23] | 0) != (HEAP8[i21 + 3 | 0] | 0)) {
     i22 = i23;
     break;
    }
    i23 = i22 + 4 | 0;
    if ((HEAP8[i23] | 0) != (HEAP8[i21 + 4 | 0] | 0)) {
     i22 = i23;
     break;
    }
    i23 = i22 + 5 | 0;
    if ((HEAP8[i23] | 0) != (HEAP8[i21 + 5 | 0] | 0)) {
     i22 = i23;
     break;
    }
    i23 = i22 + 6 | 0;
    if ((HEAP8[i23] | 0) != (HEAP8[i21 + 6 | 0] | 0)) {
     i22 = i23;
     break;
    }
    i23 = i22 + 7 | 0;
    if ((HEAP8[i23] | 0) != (HEAP8[i21 + 7 | 0] | 0)) {
     i22 = i23;
     break;
    }
    i22 = i22 + 8 | 0;
    i21 = i21 + 8 | 0;
   } while ((HEAP8[i22] | 0) == (HEAP8[i21] | 0) & i22 >>> 0 < i9 >>> 0);
   i21 = i22 - i13 | 0;
   i22 = i21 + 258 | 0;
   if ((i22 | 0) > (i17 | 0)) {
    HEAP32[i11 >> 2] = i16;
    if ((i22 | 0) >= (i12 | 0)) {
     i17 = i22;
     i3 = 20;
     break;
    }
    i17 = i22;
    i19 = HEAP8[i3 + (i22 + i5) | 0] | 0;
    i20 = HEAP8[i3 + (i10 + i21) | 0] | 0;
   }
  }
  i16 = HEAPU16[i6 + ((i16 & i7) << 1) >> 1] | 0;
  if (!(i16 >>> 0 > i8 >>> 0)) {
   i3 = 20;
   break;
  }
  i18 = i18 + -1 | 0;
  if ((i18 | 0) == 0) {
   i3 = 20;
   break;
  }
 }
 if ((i3 | 0) == 20) {
  STACKTOP = i1;
  return (i17 >>> 0 > i2 >>> 0 ? i2 : i17) | 0;
 }
 return 0;
}
function __tr_stored_block(i3, i2, i5, i6) {
 i3 = i3 | 0;
 i2 = i2 | 0;
 i5 = i5 | 0;
 i6 = i6 | 0;
 var i1 = 0, i4 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0;
 i1 = STACKTOP;
 i4 = i3 + 5820 | 0;
 i7 = HEAP32[i4 >> 2] | 0;
 i9 = i6 & 65535;
 i6 = i3 + 5816 | 0;
 i8 = HEAPU16[i6 >> 1] | 0 | i9 << i7;
 HEAP16[i6 >> 1] = i8;
 if ((i7 | 0) > 13) {
  i11 = i3 + 20 | 0;
  i7 = HEAP32[i11 >> 2] | 0;
  HEAP32[i11 >> 2] = i7 + 1;
  i10 = i3 + 8 | 0;
  HEAP8[(HEAP32[i10 >> 2] | 0) + i7 | 0] = i8;
  i8 = (HEAPU16[i6 >> 1] | 0) >>> 8 & 255;
  i7 = HEAP32[i11 >> 2] | 0;
  HEAP32[i11 >> 2] = i7 + 1;
  HEAP8[(HEAP32[i10 >> 2] | 0) + i7 | 0] = i8;
  i7 = HEAP32[i4 >> 2] | 0;
  i8 = i9 >>> (16 - i7 | 0);
  HEAP16[i6 >> 1] = i8;
  i7 = i7 + -13 | 0;
 } else {
  i7 = i7 + 3 | 0;
 }
 i8 = i8 & 255;
 HEAP32[i4 >> 2] = i7;
 do {
  if ((i7 | 0) <= 8) {
   i9 = i3 + 20 | 0;
   if ((i7 | 0) > 0) {
    i7 = HEAP32[i9 >> 2] | 0;
    HEAP32[i9 >> 2] = i7 + 1;
    i11 = i3 + 8 | 0;
    HEAP8[(HEAP32[i11 >> 2] | 0) + i7 | 0] = i8;
    i7 = i9;
    i8 = i11;
    break;
   } else {
    i7 = i9;
    i8 = i3 + 8 | 0;
    break;
   }
  } else {
   i7 = i3 + 20 | 0;
   i10 = HEAP32[i7 >> 2] | 0;
   HEAP32[i7 >> 2] = i10 + 1;
   i11 = i3 + 8 | 0;
   HEAP8[(HEAP32[i11 >> 2] | 0) + i10 | 0] = i8;
   i10 = (HEAPU16[i6 >> 1] | 0) >>> 8 & 255;
   i8 = HEAP32[i7 >> 2] | 0;
   HEAP32[i7 >> 2] = i8 + 1;
   HEAP8[(HEAP32[i11 >> 2] | 0) + i8 | 0] = i10;
   i8 = i11;
  }
 } while (0);
 HEAP16[i6 >> 1] = 0;
 HEAP32[i4 >> 2] = 0;
 HEAP32[i3 + 5812 >> 2] = 8;
 i10 = HEAP32[i7 >> 2] | 0;
 HEAP32[i7 >> 2] = i10 + 1;
 HEAP8[(HEAP32[i8 >> 2] | 0) + i10 | 0] = i5;
 i10 = HEAP32[i7 >> 2] | 0;
 HEAP32[i7 >> 2] = i10 + 1;
 HEAP8[(HEAP32[i8 >> 2] | 0) + i10 | 0] = i5 >>> 8;
 i10 = i5 & 65535 ^ 65535;
 i11 = HEAP32[i7 >> 2] | 0;
 HEAP32[i7 >> 2] = i11 + 1;
 HEAP8[(HEAP32[i8 >> 2] | 0) + i11 | 0] = i10;
 i11 = HEAP32[i7 >> 2] | 0;
 HEAP32[i7 >> 2] = i11 + 1;
 HEAP8[(HEAP32[i8 >> 2] | 0) + i11 | 0] = i10 >>> 8;
 if ((i5 | 0) == 0) {
  STACKTOP = i1;
  return;
 }
 while (1) {
  i5 = i5 + -1 | 0;
  i10 = HEAP8[i2] | 0;
  i11 = HEAP32[i7 >> 2] | 0;
  HEAP32[i7 >> 2] = i11 + 1;
  HEAP8[(HEAP32[i8 >> 2] | 0) + i11 | 0] = i10;
  if ((i5 | 0) == 0) {
   break;
  } else {
   i2 = i2 + 1 | 0;
  }
 }
 STACKTOP = i1;
 return;
}
function _inflateInit_(i1, i3, i4) {
 i1 = i1 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 var i2 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0;
 i2 = STACKTOP;
 if ((i3 | 0) == 0) {
  i11 = -6;
  STACKTOP = i2;
  return i11 | 0;
 }
 if (!((HEAP8[i3] | 0) == 49 & (i4 | 0) == 56)) {
  i11 = -6;
  STACKTOP = i2;
  return i11 | 0;
 }
 if ((i1 | 0) == 0) {
  i11 = -2;
  STACKTOP = i2;
  return i11 | 0;
 }
 i3 = i1 + 24 | 0;
 HEAP32[i3 >> 2] = 0;
 i4 = i1 + 32 | 0;
 i6 = HEAP32[i4 >> 2] | 0;
 if ((i6 | 0) == 0) {
  HEAP32[i4 >> 2] = 1;
  HEAP32[i1 + 40 >> 2] = 0;
  i6 = 1;
 }
 i4 = i1 + 36 | 0;
 if ((HEAP32[i4 >> 2] | 0) == 0) {
  HEAP32[i4 >> 2] = 1;
 }
 i5 = i1 + 40 | 0;
 i8 = FUNCTION_TABLE_iiii[i6 & 1](HEAP32[i5 >> 2] | 0, 1, 7116) | 0;
 if ((i8 | 0) == 0) {
  i11 = -4;
  STACKTOP = i2;
  return i11 | 0;
 }
 i6 = i1 + 28 | 0;
 HEAP32[i6 >> 2] = i8;
 HEAP32[i8 + 52 >> 2] = 0;
 i9 = HEAP32[i6 >> 2] | 0;
 do {
  if ((i9 | 0) != 0) {
   i10 = i9 + 52 | 0;
   i11 = HEAP32[i10 >> 2] | 0;
   i7 = i9 + 36 | 0;
   if ((i11 | 0) != 0) {
    if ((HEAP32[i7 >> 2] | 0) == 15) {
     i10 = i9;
    } else {
     FUNCTION_TABLE_vii[HEAP32[i4 >> 2] & 1](HEAP32[i5 >> 2] | 0, i11);
     HEAP32[i10 >> 2] = 0;
     i10 = HEAP32[i6 >> 2] | 0;
    }
    HEAP32[i9 + 8 >> 2] = 1;
    HEAP32[i7 >> 2] = 15;
    if ((i10 | 0) == 0) {
     break;
    } else {
     i9 = i10;
    }
   } else {
    HEAP32[i9 + 8 >> 2] = 1;
    HEAP32[i7 >> 2] = 15;
   }
   HEAP32[i9 + 28 >> 2] = 0;
   HEAP32[i1 + 20 >> 2] = 0;
   HEAP32[i1 + 8 >> 2] = 0;
   HEAP32[i3 >> 2] = 0;
   HEAP32[i1 + 48 >> 2] = 1;
   HEAP32[i9 >> 2] = 0;
   HEAP32[i9 + 4 >> 2] = 0;
   HEAP32[i9 + 12 >> 2] = 0;
   HEAP32[i9 + 20 >> 2] = 32768;
   HEAP32[i9 + 32 >> 2] = 0;
   HEAP32[i9 + 40 >> 2] = 0;
   HEAP32[i9 + 44 >> 2] = 0;
   HEAP32[i9 + 48 >> 2] = 0;
   HEAP32[i9 + 56 >> 2] = 0;
   HEAP32[i9 + 60 >> 2] = 0;
   i11 = i9 + 1328 | 0;
   HEAP32[i9 + 108 >> 2] = i11;
   HEAP32[i9 + 80 >> 2] = i11;
   HEAP32[i9 + 76 >> 2] = i11;
   HEAP32[i9 + 7104 >> 2] = 1;
   HEAP32[i9 + 7108 >> 2] = -1;
   i11 = 0;
   STACKTOP = i2;
   return i11 | 0;
  }
 } while (0);
 FUNCTION_TABLE_vii[HEAP32[i4 >> 2] & 1](HEAP32[i5 >> 2] | 0, i8);
 HEAP32[i6 >> 2] = 0;
 i11 = -2;
 STACKTOP = i2;
 return i11 | 0;
}
function _init_block(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0;
 i2 = STACKTOP;
 i3 = 0;
 do {
  HEAP16[i1 + (i3 << 2) + 148 >> 1] = 0;
  i3 = i3 + 1 | 0;
 } while ((i3 | 0) != 286);
 HEAP16[i1 + 2440 >> 1] = 0;
 HEAP16[i1 + 2444 >> 1] = 0;
 HEAP16[i1 + 2448 >> 1] = 0;
 HEAP16[i1 + 2452 >> 1] = 0;
 HEAP16[i1 + 2456 >> 1] = 0;
 HEAP16[i1 + 2460 >> 1] = 0;
 HEAP16[i1 + 2464 >> 1] = 0;
 HEAP16[i1 + 2468 >> 1] = 0;
 HEAP16[i1 + 2472 >> 1] = 0;
 HEAP16[i1 + 2476 >> 1] = 0;
 HEAP16[i1 + 2480 >> 1] = 0;
 HEAP16[i1 + 2484 >> 1] = 0;
 HEAP16[i1 + 2488 >> 1] = 0;
 HEAP16[i1 + 2492 >> 1] = 0;
 HEAP16[i1 + 2496 >> 1] = 0;
 HEAP16[i1 + 2500 >> 1] = 0;
 HEAP16[i1 + 2504 >> 1] = 0;
 HEAP16[i1 + 2508 >> 1] = 0;
 HEAP16[i1 + 2512 >> 1] = 0;
 HEAP16[i1 + 2516 >> 1] = 0;
 HEAP16[i1 + 2520 >> 1] = 0;
 HEAP16[i1 + 2524 >> 1] = 0;
 HEAP16[i1 + 2528 >> 1] = 0;
 HEAP16[i1 + 2532 >> 1] = 0;
 HEAP16[i1 + 2536 >> 1] = 0;
 HEAP16[i1 + 2540 >> 1] = 0;
 HEAP16[i1 + 2544 >> 1] = 0;
 HEAP16[i1 + 2548 >> 1] = 0;
 HEAP16[i1 + 2552 >> 1] = 0;
 HEAP16[i1 + 2556 >> 1] = 0;
 HEAP16[i1 + 2684 >> 1] = 0;
 HEAP16[i1 + 2688 >> 1] = 0;
 HEAP16[i1 + 2692 >> 1] = 0;
 HEAP16[i1 + 2696 >> 1] = 0;
 HEAP16[i1 + 2700 >> 1] = 0;
 HEAP16[i1 + 2704 >> 1] = 0;
 HEAP16[i1 + 2708 >> 1] = 0;
 HEAP16[i1 + 2712 >> 1] = 0;
 HEAP16[i1 + 2716 >> 1] = 0;
 HEAP16[i1 + 2720 >> 1] = 0;
 HEAP16[i1 + 2724 >> 1] = 0;
 HEAP16[i1 + 2728 >> 1] = 0;
 HEAP16[i1 + 2732 >> 1] = 0;
 HEAP16[i1 + 2736 >> 1] = 0;
 HEAP16[i1 + 2740 >> 1] = 0;
 HEAP16[i1 + 2744 >> 1] = 0;
 HEAP16[i1 + 2748 >> 1] = 0;
 HEAP16[i1 + 2752 >> 1] = 0;
 HEAP16[i1 + 2756 >> 1] = 0;
 HEAP16[i1 + 1172 >> 1] = 1;
 HEAP32[i1 + 5804 >> 2] = 0;
 HEAP32[i1 + 5800 >> 2] = 0;
 HEAP32[i1 + 5808 >> 2] = 0;
 HEAP32[i1 + 5792 >> 2] = 0;
 STACKTOP = i2;
 return;
}
function _deflateReset(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0;
 i2 = STACKTOP;
 if ((i1 | 0) == 0) {
  i5 = -2;
  STACKTOP = i2;
  return i5 | 0;
 }
 i3 = HEAP32[i1 + 28 >> 2] | 0;
 if ((i3 | 0) == 0) {
  i5 = -2;
  STACKTOP = i2;
  return i5 | 0;
 }
 if ((HEAP32[i1 + 32 >> 2] | 0) == 0) {
  i5 = -2;
  STACKTOP = i2;
  return i5 | 0;
 }
 if ((HEAP32[i1 + 36 >> 2] | 0) == 0) {
  i5 = -2;
  STACKTOP = i2;
  return i5 | 0;
 }
 HEAP32[i1 + 20 >> 2] = 0;
 HEAP32[i1 + 8 >> 2] = 0;
 HEAP32[i1 + 24 >> 2] = 0;
 HEAP32[i1 + 44 >> 2] = 2;
 HEAP32[i3 + 20 >> 2] = 0;
 HEAP32[i3 + 16 >> 2] = HEAP32[i3 + 8 >> 2];
 i4 = i3 + 24 | 0;
 i5 = HEAP32[i4 >> 2] | 0;
 if ((i5 | 0) < 0) {
  i5 = 0 - i5 | 0;
  HEAP32[i4 >> 2] = i5;
 }
 HEAP32[i3 + 4 >> 2] = (i5 | 0) != 0 ? 42 : 113;
 if ((i5 | 0) == 2) {
  i4 = _crc32(0, 0, 0) | 0;
 } else {
  i4 = _adler32(0, 0, 0) | 0;
 }
 HEAP32[i1 + 48 >> 2] = i4;
 HEAP32[i3 + 40 >> 2] = 0;
 __tr_init(i3);
 HEAP32[i3 + 60 >> 2] = HEAP32[i3 + 44 >> 2] << 1;
 i5 = HEAP32[i3 + 76 >> 2] | 0;
 i4 = HEAP32[i3 + 68 >> 2] | 0;
 HEAP16[i4 + (i5 + -1 << 1) >> 1] = 0;
 _memset(i4 | 0, 0, (i5 << 1) + -2 | 0) | 0;
 i5 = HEAP32[i3 + 132 >> 2] | 0;
 HEAP32[i3 + 128 >> 2] = HEAPU16[178 + (i5 * 12 | 0) >> 1] | 0;
 HEAP32[i3 + 140 >> 2] = HEAPU16[176 + (i5 * 12 | 0) >> 1] | 0;
 HEAP32[i3 + 144 >> 2] = HEAPU16[180 + (i5 * 12 | 0) >> 1] | 0;
 HEAP32[i3 + 124 >> 2] = HEAPU16[182 + (i5 * 12 | 0) >> 1] | 0;
 HEAP32[i3 + 108 >> 2] = 0;
 HEAP32[i3 + 92 >> 2] = 0;
 HEAP32[i3 + 116 >> 2] = 0;
 HEAP32[i3 + 120 >> 2] = 2;
 HEAP32[i3 + 96 >> 2] = 2;
 HEAP32[i3 + 112 >> 2] = 0;
 HEAP32[i3 + 104 >> 2] = 0;
 HEAP32[i3 + 72 >> 2] = 0;
 i5 = 0;
 STACKTOP = i2;
 return i5 | 0;
}
function _updatewindow(i6, i4) {
 i6 = i6 | 0;
 i4 = i4 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i5 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0;
 i1 = STACKTOP;
 i2 = HEAP32[i6 + 28 >> 2] | 0;
 i3 = i2 + 52 | 0;
 i8 = HEAP32[i3 >> 2] | 0;
 if ((i8 | 0) == 0) {
  i8 = FUNCTION_TABLE_iiii[HEAP32[i6 + 32 >> 2] & 1](HEAP32[i6 + 40 >> 2] | 0, 1 << HEAP32[i2 + 36 >> 2], 1) | 0;
  HEAP32[i3 >> 2] = i8;
  if ((i8 | 0) == 0) {
   i10 = 1;
   STACKTOP = i1;
   return i10 | 0;
  }
 }
 i5 = i2 + 40 | 0;
 i10 = HEAP32[i5 >> 2] | 0;
 if ((i10 | 0) == 0) {
  i10 = 1 << HEAP32[i2 + 36 >> 2];
  HEAP32[i5 >> 2] = i10;
  HEAP32[i2 + 48 >> 2] = 0;
  HEAP32[i2 + 44 >> 2] = 0;
 }
 i4 = i4 - (HEAP32[i6 + 16 >> 2] | 0) | 0;
 if (!(i4 >>> 0 < i10 >>> 0)) {
  _memcpy(i8 | 0, (HEAP32[i6 + 12 >> 2] | 0) + (0 - i10) | 0, i10 | 0) | 0;
  HEAP32[i2 + 48 >> 2] = 0;
  HEAP32[i2 + 44 >> 2] = HEAP32[i5 >> 2];
  i10 = 0;
  STACKTOP = i1;
  return i10 | 0;
 }
 i7 = i2 + 48 | 0;
 i9 = HEAP32[i7 >> 2] | 0;
 i10 = i10 - i9 | 0;
 i10 = i10 >>> 0 > i4 >>> 0 ? i4 : i10;
 i6 = i6 + 12 | 0;
 _memcpy(i8 + i9 | 0, (HEAP32[i6 >> 2] | 0) + (0 - i4) | 0, i10 | 0) | 0;
 i8 = i4 - i10 | 0;
 if ((i4 | 0) != (i10 | 0)) {
  _memcpy(HEAP32[i3 >> 2] | 0, (HEAP32[i6 >> 2] | 0) + (0 - i8) | 0, i8 | 0) | 0;
  HEAP32[i7 >> 2] = i8;
  HEAP32[i2 + 44 >> 2] = HEAP32[i5 >> 2];
  i10 = 0;
  STACKTOP = i1;
  return i10 | 0;
 }
 i6 = (HEAP32[i7 >> 2] | 0) + i4 | 0;
 i3 = HEAP32[i5 >> 2] | 0;
 HEAP32[i7 >> 2] = (i6 | 0) == (i3 | 0) ? 0 : i6;
 i5 = i2 + 44 | 0;
 i2 = HEAP32[i5 >> 2] | 0;
 if (!(i2 >>> 0 < i3 >>> 0)) {
  i10 = 0;
  STACKTOP = i1;
  return i10 | 0;
 }
 HEAP32[i5 >> 2] = i2 + i4;
 i10 = 0;
 STACKTOP = i1;
 return i10 | 0;
}
function _scan_tree(i1, i5, i6) {
 i1 = i1 | 0;
 i5 = i5 | 0;
 i6 = i6 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0;
 i8 = STACKTOP;
 i10 = HEAP16[i5 + 2 >> 1] | 0;
 i9 = i10 << 16 >> 16 == 0;
 HEAP16[i5 + (i6 + 1 << 2) + 2 >> 1] = -1;
 i2 = i1 + 2752 | 0;
 i3 = i1 + 2756 | 0;
 i4 = i1 + 2748 | 0;
 i7 = i9 ? 138 : 7;
 i9 = i9 ? 3 : 4;
 i13 = 0;
 i11 = i10 & 65535;
 i12 = -1;
 L1 : while (1) {
  i14 = 0;
  do {
   if ((i13 | 0) > (i6 | 0)) {
    break L1;
   }
   i13 = i13 + 1 | 0;
   i16 = HEAP16[i5 + (i13 << 2) + 2 >> 1] | 0;
   i10 = i16 & 65535;
   i14 = i14 + 1 | 0;
   i15 = (i11 | 0) == (i10 | 0);
  } while ((i14 | 0) < (i7 | 0) & i15);
  do {
   if ((i14 | 0) >= (i9 | 0)) {
    if ((i11 | 0) == 0) {
     if ((i14 | 0) < 11) {
      HEAP16[i2 >> 1] = (HEAP16[i2 >> 1] | 0) + 1 << 16 >> 16;
      break;
     } else {
      HEAP16[i3 >> 1] = (HEAP16[i3 >> 1] | 0) + 1 << 16 >> 16;
      break;
     }
    } else {
     if ((i11 | 0) != (i12 | 0)) {
      i14 = i1 + (i11 << 2) + 2684 | 0;
      HEAP16[i14 >> 1] = (HEAP16[i14 >> 1] | 0) + 1 << 16 >> 16;
     }
     HEAP16[i4 >> 1] = (HEAP16[i4 >> 1] | 0) + 1 << 16 >> 16;
     break;
    }
   } else {
    i12 = i1 + (i11 << 2) + 2684 | 0;
    HEAP16[i12 >> 1] = (HEAPU16[i12 >> 1] | 0) + i14;
   }
  } while (0);
  if (i16 << 16 >> 16 == 0) {
   i12 = i11;
   i7 = 138;
   i9 = 3;
   i11 = i10;
   continue;
  }
  i12 = i11;
  i7 = i15 ? 6 : 7;
  i9 = i15 ? 3 : 4;
  i11 = i10;
 }
 STACKTOP = i8;
 return;
}
function _deflateEnd(i4) {
 i4 = i4 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i5 = 0, i6 = 0, i7 = 0;
 i3 = STACKTOP;
 if ((i4 | 0) == 0) {
  i7 = -2;
  STACKTOP = i3;
  return i7 | 0;
 }
 i1 = i4 + 28 | 0;
 i6 = HEAP32[i1 >> 2] | 0;
 if ((i6 | 0) == 0) {
  i7 = -2;
  STACKTOP = i3;
  return i7 | 0;
 }
 i2 = HEAP32[i6 + 4 >> 2] | 0;
 switch (i2 | 0) {
 case 42:
 case 69:
 case 73:
 case 91:
 case 103:
 case 113:
 case 666:
  {
   break;
  }
 default:
  {
   i7 = -2;
   STACKTOP = i3;
   return i7 | 0;
  }
 }
 i5 = HEAP32[i6 + 8 >> 2] | 0;
 if ((i5 | 0) != 0) {
  FUNCTION_TABLE_vii[HEAP32[i4 + 36 >> 2] & 1](HEAP32[i4 + 40 >> 2] | 0, i5);
  i6 = HEAP32[i1 >> 2] | 0;
 }
 i5 = HEAP32[i6 + 68 >> 2] | 0;
 if ((i5 | 0) != 0) {
  FUNCTION_TABLE_vii[HEAP32[i4 + 36 >> 2] & 1](HEAP32[i4 + 40 >> 2] | 0, i5);
  i6 = HEAP32[i1 >> 2] | 0;
 }
 i5 = HEAP32[i6 + 64 >> 2] | 0;
 if ((i5 | 0) != 0) {
  FUNCTION_TABLE_vii[HEAP32[i4 + 36 >> 2] & 1](HEAP32[i4 + 40 >> 2] | 0, i5);
  i6 = HEAP32[i1 >> 2] | 0;
 }
 i7 = HEAP32[i6 + 56 >> 2] | 0;
 i5 = i4 + 36 | 0;
 if ((i7 | 0) == 0) {
  i4 = i4 + 40 | 0;
 } else {
  i4 = i4 + 40 | 0;
  FUNCTION_TABLE_vii[HEAP32[i5 >> 2] & 1](HEAP32[i4 >> 2] | 0, i7);
  i6 = HEAP32[i1 >> 2] | 0;
 }
 FUNCTION_TABLE_vii[HEAP32[i5 >> 2] & 1](HEAP32[i4 >> 2] | 0, i6);
 HEAP32[i1 >> 2] = 0;
 i7 = (i2 | 0) == 113 ? -3 : 0;
 STACKTOP = i3;
 return i7 | 0;
}
function _main(i4, i5) {
 i4 = i4 | 0;
 i5 = i5 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i6 = 0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 i2 = i1;
 L1 : do {
  if ((i4 | 0) > 1) {
   i4 = HEAP8[HEAP32[i5 + 4 >> 2] | 0] | 0;
   switch (i4 | 0) {
   case 50:
    {
     i2 = 250;
     break L1;
    }
   case 51:
    {
     i3 = 4;
     break L1;
    }
   case 52:
    {
     i2 = 2500;
     break L1;
    }
   case 53:
    {
     i2 = 5e3;
     break L1;
    }
   case 48:
    {
     i6 = 0;
     STACKTOP = i1;
     return i6 | 0;
    }
   case 49:
    {
     i2 = 60;
     break L1;
    }
   default:
    {
     HEAP32[i2 >> 2] = i4 + -48;
     _printf(144, i2 | 0) | 0;
     i6 = -1;
     STACKTOP = i1;
     return i6 | 0;
    }
   }
  } else {
   i3 = 4;
  }
 } while (0);
 if ((i3 | 0) == 4) {
  i2 = 500;
 }
 i3 = _malloc(1e5) | 0;
 i4 = 0;
 i6 = 0;
 i5 = 17;
 while (1) {
  do {
   if ((i6 | 0) <= 0) {
    if ((i4 & 7 | 0) == 0) {
     i6 = i4 & 31;
     i5 = 0;
     break;
    } else {
     i5 = (((Math_imul(i4, i4) | 0) >>> 0) % 6714 | 0) & 255;
     break;
    }
   } else {
    i6 = i6 + -1 | 0;
   }
  } while (0);
  HEAP8[i3 + i4 | 0] = i5;
  i4 = i4 + 1 | 0;
  if ((i4 | 0) == 1e5) {
   i4 = 0;
   break;
  }
 }
 do {
  _doit(i3, 1e5, i4);
  i4 = i4 + 1 | 0;
 } while ((i4 | 0) < (i2 | 0));
 _puts(160) | 0;
 i6 = 0;
 STACKTOP = i1;
 return i6 | 0;
}
function _doit(i6, i1, i7) {
 i6 = i6 | 0;
 i1 = i1 | 0;
 i7 = i7 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i8 = 0, i9 = 0;
 i5 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 i4 = i5;
 i3 = i5 + 12 | 0;
 i2 = i5 + 8 | 0;
 i8 = _compressBound(i1) | 0;
 i9 = HEAP32[2] | 0;
 if ((i9 | 0) == 0) {
  i9 = _malloc(i8) | 0;
  HEAP32[2] = i9;
 }
 if ((HEAP32[4] | 0) == 0) {
  HEAP32[4] = _malloc(i1) | 0;
 }
 HEAP32[i3 >> 2] = i8;
 _compress(i9, i3, i6, i1) | 0;
 i7 = (i7 | 0) == 0;
 if (i7) {
  i9 = HEAP32[i3 >> 2] | 0;
  HEAP32[i4 >> 2] = i1;
  HEAP32[i4 + 4 >> 2] = i9;
  _printf(24, i4 | 0) | 0;
 }
 HEAP32[i2 >> 2] = i1;
 _uncompress(HEAP32[4] | 0, i2, HEAP32[2] | 0, HEAP32[i3 >> 2] | 0) | 0;
 if ((HEAP32[i2 >> 2] | 0) != (i1 | 0)) {
  ___assert_fail(40, 72, 24, 104);
 }
 if (!i7) {
  STACKTOP = i5;
  return;
 }
 if ((_strcmp(i6, HEAP32[4] | 0) | 0) == 0) {
  STACKTOP = i5;
  return;
 } else {
  ___assert_fail(112, 72, 25, 104);
 }
}
function _uncompress(i6, i1, i5, i7) {
 i6 = i6 | 0;
 i1 = i1 | 0;
 i5 = i5 | 0;
 i7 = i7 | 0;
 var i2 = 0, i3 = 0, i4 = 0;
 i2 = STACKTOP;
 STACKTOP = STACKTOP + 64 | 0;
 i3 = i2;
 HEAP32[i3 >> 2] = i5;
 i5 = i3 + 4 | 0;
 HEAP32[i5 >> 2] = i7;
 HEAP32[i3 + 12 >> 2] = i6;
 HEAP32[i3 + 16 >> 2] = HEAP32[i1 >> 2];
 HEAP32[i3 + 32 >> 2] = 0;
 HEAP32[i3 + 36 >> 2] = 0;
 i6 = _inflateInit_(i3, 2992, 56) | 0;
 if ((i6 | 0) != 0) {
  i7 = i6;
  STACKTOP = i2;
  return i7 | 0;
 }
 i6 = _inflate(i3, 4) | 0;
 if ((i6 | 0) == 1) {
  HEAP32[i1 >> 2] = HEAP32[i3 + 20 >> 2];
  i7 = _inflateEnd(i3) | 0;
  STACKTOP = i2;
  return i7 | 0;
 }
 _inflateEnd(i3) | 0;
 if ((i6 | 0) == 2) {
  i7 = -3;
  STACKTOP = i2;
  return i7 | 0;
 } else if ((i6 | 0) == -5) {
  i4 = 4;
 }
 if ((i4 | 0) == 4 ? (HEAP32[i5 >> 2] | 0) == 0 : 0) {
  i7 = -3;
  STACKTOP = i2;
  return i7 | 0;
 }
 i7 = i6;
 STACKTOP = i2;
 return i7 | 0;
}
function _compress(i4, i1, i6, i5) {
 i4 = i4 | 0;
 i1 = i1 | 0;
 i6 = i6 | 0;
 i5 = i5 | 0;
 var i2 = 0, i3 = 0;
 i2 = STACKTOP;
 STACKTOP = STACKTOP + 64 | 0;
 i3 = i2;
 HEAP32[i3 >> 2] = i6;
 HEAP32[i3 + 4 >> 2] = i5;
 HEAP32[i3 + 12 >> 2] = i4;
 HEAP32[i3 + 16 >> 2] = HEAP32[i1 >> 2];
 HEAP32[i3 + 32 >> 2] = 0;
 HEAP32[i3 + 36 >> 2] = 0;
 HEAP32[i3 + 40 >> 2] = 0;
 i4 = _deflateInit_(i3, -1, 168, 56) | 0;
 if ((i4 | 0) != 0) {
  i6 = i4;
  STACKTOP = i2;
  return i6 | 0;
 }
 i4 = _deflate(i3, 4) | 0;
 if ((i4 | 0) == 1) {
  HEAP32[i1 >> 2] = HEAP32[i3 + 20 >> 2];
  i6 = _deflateEnd(i3) | 0;
  STACKTOP = i2;
  return i6 | 0;
 } else {
  _deflateEnd(i3) | 0;
  i6 = (i4 | 0) == 0 ? -5 : i4;
  STACKTOP = i2;
  return i6 | 0;
 }
 return 0;
}
function _inflateEnd(i4) {
 i4 = i4 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i5 = 0, i6 = 0, i7 = 0;
 i1 = STACKTOP;
 if ((i4 | 0) == 0) {
  i7 = -2;
  STACKTOP = i1;
  return i7 | 0;
 }
 i2 = i4 + 28 | 0;
 i3 = HEAP32[i2 >> 2] | 0;
 if ((i3 | 0) == 0) {
  i7 = -2;
  STACKTOP = i1;
  return i7 | 0;
 }
 i6 = i4 + 36 | 0;
 i5 = HEAP32[i6 >> 2] | 0;
 if ((i5 | 0) == 0) {
  i7 = -2;
  STACKTOP = i1;
  return i7 | 0;
 }
 i7 = HEAP32[i3 + 52 >> 2] | 0;
 i4 = i4 + 40 | 0;
 if ((i7 | 0) != 0) {
  FUNCTION_TABLE_vii[i5 & 1](HEAP32[i4 >> 2] | 0, i7);
  i5 = HEAP32[i6 >> 2] | 0;
  i3 = HEAP32[i2 >> 2] | 0;
 }
 FUNCTION_TABLE_vii[i5 & 1](HEAP32[i4 >> 2] | 0, i3);
 HEAP32[i2 >> 2] = 0;
 i7 = 0;
 STACKTOP = i1;
 return i7 | 0;
}
function _memcpy(i3, i2, i1) {
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 var i4 = 0;
 if ((i1 | 0) >= 4096) return _emscripten_memcpy_big(i3 | 0, i2 | 0, i1 | 0) | 0;
 i4 = i3 | 0;
 if ((i3 & 3) == (i2 & 3)) {
  while (i3 & 3) {
   if ((i1 | 0) == 0) return i4 | 0;
   HEAP8[i3] = HEAP8[i2] | 0;
   i3 = i3 + 1 | 0;
   i2 = i2 + 1 | 0;
   i1 = i1 - 1 | 0;
  }
  while ((i1 | 0) >= 4) {
   HEAP32[i3 >> 2] = HEAP32[i2 >> 2];
   i3 = i3 + 4 | 0;
   i2 = i2 + 4 | 0;
   i1 = i1 - 4 | 0;
  }
 }
 while ((i1 | 0) > 0) {
  HEAP8[i3] = HEAP8[i2] | 0;
  i3 = i3 + 1 | 0;
  i2 = i2 + 1 | 0;
  i1 = i1 - 1 | 0;
 }
 return i4 | 0;
}
function _strcmp(i4, i2) {
 i4 = i4 | 0;
 i2 = i2 | 0;
 var i1 = 0, i3 = 0, i5 = 0;
 i1 = STACKTOP;
 i5 = HEAP8[i4] | 0;
 i3 = HEAP8[i2] | 0;
 if (i5 << 24 >> 24 != i3 << 24 >> 24 | i5 << 24 >> 24 == 0 | i3 << 24 >> 24 == 0) {
  i4 = i5;
  i5 = i3;
  i4 = i4 & 255;
  i5 = i5 & 255;
  i5 = i4 - i5 | 0;
  STACKTOP = i1;
  return i5 | 0;
 }
 do {
  i4 = i4 + 1 | 0;
  i2 = i2 + 1 | 0;
  i5 = HEAP8[i4] | 0;
  i3 = HEAP8[i2] | 0;
 } while (!(i5 << 24 >> 24 != i3 << 24 >> 24 | i5 << 24 >> 24 == 0 | i3 << 24 >> 24 == 0));
 i4 = i5 & 255;
 i5 = i3 & 255;
 i5 = i4 - i5 | 0;
 STACKTOP = i1;
 return i5 | 0;
}
function _memset(i1, i4, i3) {
 i1 = i1 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 var i2 = 0, i5 = 0, i6 = 0, i7 = 0;
 i2 = i1 + i3 | 0;
 if ((i3 | 0) >= 20) {
  i4 = i4 & 255;
  i7 = i1 & 3;
  i6 = i4 | i4 << 8 | i4 << 16 | i4 << 24;
  i5 = i2 & ~3;
  if (i7) {
   i7 = i1 + 4 - i7 | 0;
   while ((i1 | 0) < (i7 | 0)) {
    HEAP8[i1] = i4;
    i1 = i1 + 1 | 0;
   }
  }
  while ((i1 | 0) < (i5 | 0)) {
   HEAP32[i1 >> 2] = i6;
   i1 = i1 + 4 | 0;
  }
 }
 while ((i1 | 0) < (i2 | 0)) {
  HEAP8[i1] = i4;
  i1 = i1 + 1 | 0;
 }
 return i1 - i3 | 0;
}
function copyTempDouble(i1) {
 i1 = i1 | 0;
 HEAP8[tempDoublePtr] = HEAP8[i1];
 HEAP8[tempDoublePtr + 1 | 0] = HEAP8[i1 + 1 | 0];
 HEAP8[tempDoublePtr + 2 | 0] = HEAP8[i1 + 2 | 0];
 HEAP8[tempDoublePtr + 3 | 0] = HEAP8[i1 + 3 | 0];
 HEAP8[tempDoublePtr + 4 | 0] = HEAP8[i1 + 4 | 0];
 HEAP8[tempDoublePtr + 5 | 0] = HEAP8[i1 + 5 | 0];
 HEAP8[tempDoublePtr + 6 | 0] = HEAP8[i1 + 6 | 0];
 HEAP8[tempDoublePtr + 7 | 0] = HEAP8[i1 + 7 | 0];
}
function __tr_init(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 HEAP32[i1 + 2840 >> 2] = i1 + 148;
 HEAP32[i1 + 2848 >> 2] = 1064;
 HEAP32[i1 + 2852 >> 2] = i1 + 2440;
 HEAP32[i1 + 2860 >> 2] = 1088;
 HEAP32[i1 + 2864 >> 2] = i1 + 2684;
 HEAP32[i1 + 2872 >> 2] = 1112;
 HEAP16[i1 + 5816 >> 1] = 0;
 HEAP32[i1 + 5820 >> 2] = 0;
 HEAP32[i1 + 5812 >> 2] = 8;
 _init_block(i1);
 STACKTOP = i2;
 return;
}
function copyTempFloat(i1) {
 i1 = i1 | 0;
 HEAP8[tempDoublePtr] = HEAP8[i1];
 HEAP8[tempDoublePtr + 1 | 0] = HEAP8[i1 + 1 | 0];
 HEAP8[tempDoublePtr + 2 | 0] = HEAP8[i1 + 2 | 0];
 HEAP8[tempDoublePtr + 3 | 0] = HEAP8[i1 + 3 | 0];
}
function _deflateInit_(i4, i3, i2, i1) {
 i4 = i4 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 var i5 = 0;
 i5 = STACKTOP;
 i4 = _deflateInit2_(i4, i3, 8, 15, 8, 0, i2, i1) | 0;
 STACKTOP = i5;
 return i4 | 0;
}
function _zcalloc(i3, i1, i2) {
 i3 = i3 | 0;
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i4 = 0;
 i4 = STACKTOP;
 i3 = _malloc(Math_imul(i2, i1) | 0) | 0;
 STACKTOP = i4;
 return i3 | 0;
}
function dynCall_iiii(i4, i3, i2, i1) {
 i4 = i4 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 return FUNCTION_TABLE_iiii[i4 & 1](i3 | 0, i2 | 0, i1 | 0) | 0;
}
function runPostSets() {}
function _strlen(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = i1;
 while (HEAP8[i2] | 0) {
  i2 = i2 + 1 | 0;
 }
 return i2 - i1 | 0;
}
function stackAlloc(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 STACKTOP = STACKTOP + i1 | 0;
 STACKTOP = STACKTOP + 7 & -8;
 return i2 | 0;
}
function dynCall_iii(i3, i2, i1) {
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 return FUNCTION_TABLE_iii[i3 & 3](i2 | 0, i1 | 0) | 0;
}
function setThrew(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 if ((__THREW__ | 0) == 0) {
  __THREW__ = i1;
  threwValue = i2;
 }
}
function dynCall_vii(i3, i2, i1) {
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 FUNCTION_TABLE_vii[i3 & 1](i2 | 0, i1 | 0);
}
function _zcfree(i2, i1) {
 i2 = i2 | 0;
 i1 = i1 | 0;
 i2 = STACKTOP;
 _free(i1);
 STACKTOP = i2;
 return;
}
function _compressBound(i1) {
 i1 = i1 | 0;
 return i1 + 13 + (i1 >>> 12) + (i1 >>> 14) + (i1 >>> 25) | 0;
}
function b0(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 abort(0);
 return 0;
}
function b2(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 abort(2);
 return 0;
}
function b1(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 abort(1);
}
function stackRestore(i1) {
 i1 = i1 | 0;
 STACKTOP = i1;
}
function setTempRet9(i1) {
 i1 = i1 | 0;
 tempRet9 = i1;
}
function setTempRet8(i1) {
 i1 = i1 | 0;
 tempRet8 = i1;
}
function setTempRet7(i1) {
 i1 = i1 | 0;
 tempRet7 = i1;
}
function setTempRet6(i1) {
 i1 = i1 | 0;
 tempRet6 = i1;
}
function setTempRet5(i1) {
 i1 = i1 | 0;
 tempRet5 = i1;
}
function setTempRet4(i1) {
 i1 = i1 | 0;
 tempRet4 = i1;
}
function setTempRet3(i1) {
 i1 = i1 | 0;
 tempRet3 = i1;
}
function setTempRet2(i1) {
 i1 = i1 | 0;
 tempRet2 = i1;
}
function setTempRet1(i1) {
 i1 = i1 | 0;
 tempRet1 = i1;
}
function setTempRet0(i1) {
 i1 = i1 | 0;
 tempRet0 = i1;
}
function stackSave() {
 return STACKTOP | 0;
}

// EMSCRIPTEN_END_FUNCS
  var FUNCTION_TABLE_iiii = [b0,_zcalloc];
  var FUNCTION_TABLE_vii = [b1,_zcfree];
  var FUNCTION_TABLE_iii = [b2,_deflate_stored,_deflate_fast,_deflate_slow];

  return { _strlen: _strlen, _free: _free, _main: _main, _memset: _memset, _malloc: _malloc, _memcpy: _memcpy, runPostSets: runPostSets, stackAlloc: stackAlloc, stackSave: stackSave, stackRestore: stackRestore, setThrew: setThrew, setTempRet0: setTempRet0, setTempRet1: setTempRet1, setTempRet2: setTempRet2, setTempRet3: setTempRet3, setTempRet4: setTempRet4, setTempRet5: setTempRet5, setTempRet6: setTempRet6, setTempRet7: setTempRet7, setTempRet8: setTempRet8, setTempRet9: setTempRet9, dynCall_iiii: dynCall_iiii, dynCall_vii: dynCall_vii, dynCall_iii: dynCall_iii };
})
// EMSCRIPTEN_END_ASM
({ "Math": Math, "Int8Array": Int8Array, "Int16Array": Int16Array, "Int32Array": Int32Array, "Uint8Array": Uint8Array, "Uint16Array": Uint16Array, "Uint32Array": Uint32Array, "Float32Array": Float32Array, "Float64Array": Float64Array }, { "abort": abort, "assert": assert, "asmPrintInt": asmPrintInt, "asmPrintFloat": asmPrintFloat, "min": Math_min, "invoke_iiii": invoke_iiii, "invoke_vii": invoke_vii, "invoke_iii": invoke_iii, "_send": _send, "___setErrNo": ___setErrNo, "___assert_fail": ___assert_fail, "_fflush": _fflush, "_pwrite": _pwrite, "__reallyNegative": __reallyNegative, "_sbrk": _sbrk, "___errno_location": ___errno_location, "_emscripten_memcpy_big": _emscripten_memcpy_big, "_fileno": _fileno, "_sysconf": _sysconf, "_puts": _puts, "_mkport": _mkport, "_write": _write, "_llvm_bswap_i32": _llvm_bswap_i32, "_fputc": _fputc, "_abort": _abort, "_fwrite": _fwrite, "_time": _time, "_fprintf": _fprintf, "__formatString": __formatString, "_fputs": _fputs, "_printf": _printf, "STACKTOP": STACKTOP, "STACK_MAX": STACK_MAX, "tempDoublePtr": tempDoublePtr, "ABORT": ABORT, "NaN": NaN, "Infinity": Infinity }, buffer);
var _strlen = Module["_strlen"] = asm["_strlen"];
var _free = Module["_free"] = asm["_free"];
var _main = Module["_main"] = asm["_main"];
var _memset = Module["_memset"] = asm["_memset"];
var _malloc = Module["_malloc"] = asm["_malloc"];
var _memcpy = Module["_memcpy"] = asm["_memcpy"];
var runPostSets = Module["runPostSets"] = asm["runPostSets"];
var dynCall_iiii = Module["dynCall_iiii"] = asm["dynCall_iiii"];
var dynCall_vii = Module["dynCall_vii"] = asm["dynCall_vii"];
var dynCall_iii = Module["dynCall_iii"] = asm["dynCall_iii"];

Runtime.stackAlloc = function(size) { return asm['stackAlloc'](size) };
Runtime.stackSave = function() { return asm['stackSave']() };
Runtime.stackRestore = function(top) { asm['stackRestore'](top) };


// Warning: printing of i64 values may be slightly rounded! No deep i64 math used, so precise i64 code not included
var i64Math = null;

// === Auto-generated postamble setup entry stuff ===

if (memoryInitializer) {
  if (ENVIRONMENT_IS_NODE || ENVIRONMENT_IS_SHELL) {
    var data = Module['readBinary'](memoryInitializer);
    HEAPU8.set(data, STATIC_BASE);
  } else {
    addRunDependency('memory initializer');
    Browser.asyncLoad(memoryInitializer, function(data) {
      HEAPU8.set(data, STATIC_BASE);
      removeRunDependency('memory initializer');
    }, function(data) {
      throw 'could not load memory initializer ' + memoryInitializer;
    });
  }
}

function ExitStatus(status) {
  this.name = "ExitStatus";
  this.message = "Program terminated with exit(" + status + ")";
  this.status = status;
};
ExitStatus.prototype = new Error();
ExitStatus.prototype.constructor = ExitStatus;

var initialStackTop;
var preloadStartTime = null;
var calledMain = false;

dependenciesFulfilled = function runCaller() {
  // If run has never been called, and we should call run (INVOKE_RUN is true, and Module.noInitialRun is not false)
  if (!Module['calledRun'] && shouldRunNow) run([].concat(Module["arguments"]));
  if (!Module['calledRun']) dependenciesFulfilled = runCaller; // try this again later, after new deps are fulfilled
}

Module['callMain'] = Module.callMain = function callMain(args) {
  assert(runDependencies == 0, 'cannot call main when async dependencies remain! (listen on __ATMAIN__)');
  assert(__ATPRERUN__.length == 0, 'cannot call main when preRun functions remain to be called');

  args = args || [];

  ensureInitRuntime();

  var argc = args.length+1;
  function pad() {
    for (var i = 0; i < 4-1; i++) {
      argv.push(0);
    }
  }
  var argv = [allocate(intArrayFromString("/bin/this.program"), 'i8', ALLOC_NORMAL) ];
  pad();
  for (var i = 0; i < argc-1; i = i + 1) {
    argv.push(allocate(intArrayFromString(args[i]), 'i8', ALLOC_NORMAL));
    pad();
  }
  argv.push(0);
  argv = allocate(argv, 'i32', ALLOC_NORMAL);

  initialStackTop = STACKTOP;

  try {

    var ret = Module['_main'](argc, argv, 0);


    // if we're not running an evented main loop, it's time to exit
    if (!Module['noExitRuntime']) {
      exit(ret);
    }
  }
  catch(e) {
    if (e instanceof ExitStatus) {
      // exit() throws this once it's done to make sure execution
      // has been stopped completely
      return;
    } else if (e == 'SimulateInfiniteLoop') {
      // running an evented main loop, don't immediately exit
      Module['noExitRuntime'] = true;
      return;
    } else {
      if (e && typeof e === 'object' && e.stack) Module.printErr('exception thrown: ' + [e, e.stack]);
      throw e;
    }
  } finally {
    calledMain = true;
  }
}




function run(args) {
  args = args || Module['arguments'];

  if (preloadStartTime === null) preloadStartTime = Date.now();

  if (runDependencies > 0) {
    Module.printErr('run() called, but dependencies remain, so not running');
    return;
  }

  preRun();

  if (runDependencies > 0) return; // a preRun added a dependency, run will be called later
  if (Module['calledRun']) return; // run may have just been called through dependencies being fulfilled just in this very frame

  function doRun() {
    if (Module['calledRun']) return; // run may have just been called while the async setStatus time below was happening
    Module['calledRun'] = true;

    ensureInitRuntime();

    preMain();

    if (ENVIRONMENT_IS_WEB && preloadStartTime !== null) {
      Module.printErr('pre-main prep time: ' + (Date.now() - preloadStartTime) + ' ms');
    }

    if (Module['_main'] && shouldRunNow) {
      Module['callMain'](args);
    }

    postRun();
  }

  if (Module['setStatus']) {
    Module['setStatus']('Running...');
    setTimeout(function() {
      setTimeout(function() {
        Module['setStatus']('');
      }, 1);
      if (!ABORT) doRun();
    }, 1);
  } else {
    doRun();
  }
}
Module['run'] = Module.run = run;

function exit(status) {
  ABORT = true;
  EXITSTATUS = status;
  STACKTOP = initialStackTop;

  // exit the runtime
  exitRuntime();

  // TODO We should handle this differently based on environment.
  // In the browser, the best we can do is throw an exception
  // to halt execution, but in node we could process.exit and
  // I'd imagine SM shell would have something equivalent.
  // This would let us set a proper exit status (which
  // would be great for checking test exit statuses).
  // https://github.com/kripken/emscripten/issues/1371

  // throw an exception to halt the current execution
  throw new ExitStatus(status);
}
Module['exit'] = Module.exit = exit;

function abort(text) {
  if (text) {
    Module.print(text);
    Module.printErr(text);
  }

  ABORT = true;
  EXITSTATUS = 1;

  var extra = '\nIf this abort() is unexpected, build with -s ASSERTIONS=1 which can give more information.';

  throw 'abort() at ' + stackTrace() + extra;
}
Module['abort'] = Module.abort = abort;

// {{PRE_RUN_ADDITIONS}}

if (Module['preInit']) {
  if (typeof Module['preInit'] == 'function') Module['preInit'] = [Module['preInit']];
  while (Module['preInit'].length > 0) {
    Module['preInit'].pop()();
  }
}

// shouldRunNow refers to calling main(), not run().
var shouldRunNow = true;
if (Module['noInitialRun']) {
  shouldRunNow = false;
}


run([].concat(Module["arguments"]));
