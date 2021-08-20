// Modified embenchen to direct to asm-wasm.
// Flags: --validate-asm --allow-natives-syntax --wasm-loop-unrolling

var EXPECTED_OUTPUT =
  /frame averages: .+ \+- .+, range: .+ to .+ \n/;
var Module = {
  arguments: [1],
  print: function(x) {Module.printBuffer += x + '\n';},
  preRun: [function() {Module.printBuffer = ''}],
  postRun: [function() {
    assertTrue(EXPECTED_OUTPUT.test(Module.printBuffer));
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
var __ZTVN10__cxxabiv117__class_type_infoE = 7024;
var __ZTVN10__cxxabiv120__si_class_type_infoE = 7064;




STATIC_BASE = 8;

STATICTOP = STATIC_BASE + Runtime.alignMemory(7731);
/* global initializers */ __ATINIT__.push();


/* memory initializer */ allocate([0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,232,118,72,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,101,114,114,111,114,58,32,37,100,92,110,0,0,0,0,0,102,114,97,109,101,32,97,118,101,114,97,103,101,115,58,32,37,46,51,102,32,43,45,32,37,46,51,102,44,32,114,97,110,103,101,58,32,37,46,51,102,32,116,111,32,37,46,51,102,32,10,0,0,0,0,0,105,102,32,40,77,111,100,117,108,101,46,114,101,112,111,114,116,67,111,109,112,108,101,116,105,111,110,41,32,77,111,100,117,108,101,46,114,101,112,111,114,116,67,111,109,112,108,101,116,105,111,110,40,41,0,0,114,101,115,112,111,110,115,105,118,101,32,109,97,105,110,32,108,111,111,112,0,0,0,0,0,0,0,0,56,1,0,0,1,0,0,0,2,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,49,49,98,50,69,100,103,101,83,104,97,112,101,0,0,0,55,98,50,83,104,97,112,101,0,0,0,0,0,0,0,0,120,27,0,0,32,1,0,0,160,27,0,0,16,1,0,0,48,1,0,0,0,0,0,0,66,111,120,50,68,47,67,111,108,108,105,115,105,111,110,47,83,104,97,112,101,115,47,98,50,80,111,108,121,103,111,110,83,104,97,112,101,46,99,112,112,0,0,0,0,0,0,0,48,46,48,102,32,60,61,32,108,111,119,101,114,32,38,38,32,108,111,119,101,114,32,60,61,32,105,110,112,117,116,46,109,97,120,70,114,97,99,116,105,111,110,0,0,0,0,0,82,97,121,67,97,115,116,0,109,95,118,101,114,116,101,120,67,111,117,110,116,32,62,61,32,51,0,0,0,0,0,0,67,111,109,112,117,116,101,77,97,115,115,0,0,0,0,0,97,114,101,97,32,62,32,49,46,49,57,50,48,57,50,57,48,101,45,48,55,70,0,0,0,0,0,0,48,2,0,0,3,0,0,0,4,0,0,0,2,0,0,0,2,0,0,0,2,0,0,0,2,0,0,0,2,0,0,0,2,0,0,0,49,52,98,50,80,111,108,121,103,111,110,83,104,97,112,101,0,0,0,0,0,0,0,0,160,27,0,0,24,2,0,0,48,1,0,0,0,0,0,0,16,0,0,0,32,0,0,0,64,0,0,0,96,0,0,0,128,0,0,0,160,0,0,0,192,0,0,0,224,0,0,0,0,1,0,0,64,1,0,0,128,1,0,0,192,1,0,0,0,2,0,0,128,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,106,32,60,32,98,50,95,98,108,111,99,107,83,105,122,101,115,0,0,0,0,0,0,0,66,111,120,50,68,47,67,111,109,109,111,110,47,98,50,66,108,111,99,107,65,108,108,111,99,97,116,111,114,46,99,112,112,0,0,0,0,0,0,0,98,50,66,108,111,99,107,65,108,108,111,99,97,116,111,114,0,0,0,0,0,0,0,0,48,32,60,32,115,105,122,101,0,0,0,0,0,0,0,0,65,108,108,111,99,97,116,101,0,0,0,0,0,0,0,0,48,32,60,61,32,105,110,100,101,120,32,38,38,32,105,110,100,101,120,32,60,32,98,50,95,98,108,111,99,107,83,105,122,101,115,0,0,0,0,0,98,108,111,99,107,67,111,117,110,116,32,42,32,98,108,111,99,107,83,105,122,101,32,60,61,32,98,50,95,99,104,117,110,107,83,105,122,101,0,0,70,114,101,101,0,0,0,0,98,100,45,62,112,111,115,105,116,105,111,110,46,73,115,86,97,108,105,100,40,41,0,0,66,111,120,50,68,47,68,121,110,97,109,105,99,115,47,98,50,66,111,100,121,46,99,112,112,0,0,0,0,0,0,0,98,50,66,111,100,121,0,0,98,100,45,62,108,105,110,101,97,114,86,101,108,111,99,105,116,121,46,73,115,86,97,108,105,100,40,41,0,0,0,0,98,50,73,115,86,97,108,105,100,40,98,100,45,62,97,110,103,108,101,41,0,0,0,0,98,50,73,115,86,97,108,105,100,40,98,100,45,62,97,110,103,117,108,97,114,86,101,108,111,99,105,116,121,41,0,0,98,50,73,115,86,97,108,105,100,40,98,100,45,62,97,110,103,117,108,97,114,68,97,109,112,105,110,103,41,32,38,38,32,98,100,45,62,97,110,103,117,108,97,114,68,97,109,112,105,110,103,32,62,61,32,48,46,48,102,0,0,0,0,0,98,50,73,115,86,97,108,105,100,40,98,100,45,62,108,105,110,101,97,114,68,97,109,112,105,110,103,41,32,38,38,32,98,100,45,62,108,105,110,101,97,114,68,97,109,112,105,110,103,32,62,61,32,48,46,48,102,0,0,0,0,0,0,0,109,95,119,111,114,108,100,45,62,73,115,76,111,99,107,101,100,40,41,32,61,61,32,102,97,108,115,101,0,0,0,0,67,114,101,97,116,101,70,105,120,116,117,114,101,0,0,0,109,95,116,121,112,101,32,61,61,32,98,50,95,100,121,110,97,109,105,99,66,111,100,121,0,0,0,0,0,0,0,0,82,101,115,101,116,77,97,115,115,68,97,116,97,0,0,0,109,95,73,32,62,32,48,46,48,102,0,0,0,0,0,0,0,10,0,0,0,0,0,0,240,7,0,0,0,0,0,0,48,32,60,61,32,112,114,111,120,121,73,100,32,38,38,32,112,114,111,120,121,73,100,32,60,32,109,95,110,111,100,101,67,97,112,97,99,105,116,121,0,0,0,0,0,0,0,0,46,47,66,111,120,50,68,47,67,111,108,108,105,115,105,111,110,47,98,50,68,121,110,97,109,105,99,84,114,101,101,46,104,0,0,0,0,0,0,0,71,101,116,85,115,101,114,68,97,116,97,0,0,0,0,0,71,101,116,70,97,116,65,65,66,66,0,0,0,0,0,0,0,0,0,0,32,8,0,0,5,0,0,0,6,0,0,0,1,0,0,0,2,0,0,0,1,0,0,0,2,0,0,0,49,55,98,50,67,111,110,116,97,99,116,76,105,115,116,101,110,101,114,0,0,0,0,0,120,27,0,0,8,8,0,0,109,95,112,114,111,120,121,67,111,117,110,116,32,61,61,32,48,0,0,0,0,0,0,0,66,111,120,50,68,47,68,121,110,97,109,105,99,115,47,98,50,70,105,120,116,117,114,101,46,99,112,112,0,0,0,0,67,114,101,97,116,101,80,114,111,120,105,101,115,0,0,0,73,115,76,111,99,107,101,100,40,41,32,61,61,32,102,97,108,115,101,0,0,0,0,0,66,111,120,50,68,47,68,121,110,97,109,105,99,115,47,98,50,87,111,114,108,100,46,99,112,112,0,0,0,0,0,0,67,114,101,97,116,101,66,111,100,121,0,0,0,0,0,0,98,45,62,73,115,65,99,116,105,118,101,40,41,32,61,61,32,116,114,117,101,0,0,0,83,111,108,118,101,0,0,0,115,116,97,99,107,67,111,117,110,116,32,60,32,115,116,97,99,107,83,105,122,101,0,0,116,121,112,101,65,32,61,61,32,98,50,95,100,121,110,97,109,105,99,66,111,100,121,32,124,124,32,116,121,112,101,66,32,61,61,32,98,50,95,100,121,110,97,109,105,99,66,111,100,121,0,0,0,0,0,0,83,111,108,118,101,84,79,73,0,0,0,0,0,0,0,0,97,108,112,104,97,48,32,60,32,49,46,48,102,0,0,0,46,47,66,111,120,50,68,47,67,111,109,109,111,110,47,98,50,77,97,116,104,46,104,0,65,100,118,97,110,99,101,0,109,95,106,111,105,110,116,67,111,117,110,116,32,60,32,109,95,106,111,105,110,116,67,97,112,97,99,105,116,121,0,0,46,47,66,111,120,50,68,47,68,121,110,97,109,105,99,115,47,98,50,73,115,108,97,110,100,46,104,0,0,0,0,0,65,100,100,0,0,0,0,0,109,95,99,111,110,116,97,99,116,67,111,117,110,116,32,60,32,109,95,99,111,110,116,97,99,116,67,97,112,97,99,105,116,121,0,0,0,0,0,0,109,95,98,111,100,121,67,111,117,110,116,32,60,32,109,95,98,111,100,121,67,97,112,97,99,105,116,121,0,0,0,0,0,0,0,0,40,10,0,0,7,0,0,0,8,0,0,0,3,0,0,0,0,0,0,0,49,53,98,50,67,111,110,116,97,99,116,70,105,108,116,101,114,0,0,0,0,0,0,0,120,27,0,0,16,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,48,32,60,61,32,105,110,100,101,120,32,38,38,32,105,110,100,101,120,32,60,32,99,104,97,105,110,45,62,109,95,99,111,117,110,116,0,0,0,0,66,111,120,50,68,47,67,111,108,108,105,115,105,111,110,47,98,50,68,105,115,116,97,110,99,101,46,99,112,112,0,0,83,101,116,0,0,0,0,0,102,97,108,115,101,0,0,0,98,50,68,105,115,116,97,110,99,101,0,0,0,0,0,0,71,101,116,77,101,116,114,105,99,0,0,0,0,0,0,0,71,101,116,87,105,116,110,101,115,115,80,111,105,110,116,115,0,0,0,0,0,0,0,0,48,32,60,61,32,105,110,100,101,120,32,38,38,32,105,110,100,101,120,32,60,32,109,95,99,111,117,110,116,0,0,0,46,47,66,111,120,50,68,47,67,111,108,108,105,115,105,111,110,47,98,50,68,105,115,116,97,110,99,101,46,104,0,0,71,101,116,86,101,114,116,101,120,0,0,0,0,0,0,0,71,101,116,67,108,111,115,101,115,116,80,111,105,110,116,0,99,97,99,104,101,45,62,99,111,117,110,116,32,60,61,32,51,0,0,0,0,0,0,0,82,101,97,100,67,97,99,104,101,0,0,0,0,0,0,0,109,95,110,111,100,101,67,111,117,110,116,32,61,61,32,109,95,110,111,100,101,67,97,112,97,99,105,116,121,0,0,0,66,111,120,50,68,47,67,111,108,108,105,115,105,111,110,47,98,50,68,121,110,97,109,105,99,84,114,101,101,46,99,112,112,0,0,0,0,0,0,0,65,108,108,111,99,97,116,101,78,111,100,101,0,0,0,0,48,32,60,61,32,110,111,100,101,73,100,32,38,38,32,110,111,100,101,73,100,32,60,32,109,95,110,111,100,101,67,97,112,97,99,105,116,121,0,0,70,114,101,101,78,111,100,101,0,0,0,0,0,0,0,0,48,32,60,32,109,95,110,111,100,101,67,111,117,110,116,0,48,32,60,61,32,112,114,111,120,121,73,100,32,38,38,32,112,114,111,120,121,73,100,32,60,32,109,95,110,111,100,101,67,97,112,97,99,105,116,121,0,0,0,0,0,0,0,0,109,95,110,111,100,101,115,91,112,114,111,120,121,73,100,93,46,73,115,76,101,97,102,40,41,0,0,0,0,0,0,0,77,111,118,101,80,114,111,120,121,0,0,0,0,0,0,0,99,104,105,108,100,49,32,33,61,32,40,45,49,41,0,0,73,110,115,101,114,116,76,101,97,102,0,0,0,0,0,0,99,104,105,108,100,50,32,33,61,32,40,45,49,41,0,0,105,65,32,33,61,32,40,45,49,41,0,0,0,0,0,0,66,97,108,97,110,99,101,0,48,32,60,61,32,105,66,32,38,38,32,105,66,32,60,32,109,95,110,111,100,101,67,97,112,97,99,105,116,121,0,0,48,32,60,61,32,105,67,32,38,38,32,105,67,32,60,32,109,95,110,111,100,101,67,97,112,97,99,105,116,121,0,0,48,32,60,61,32,105,70,32,38,38,32,105,70,32,60,32,109,95,110,111,100,101,67,97,112,97,99,105,116,121,0,0,48,32,60,61,32,105,71,32,38,38,32,105,71,32,60,32,109,95,110,111,100,101,67,97,112,97,99,105,116,121,0,0,109,95,110,111,100,101,115,91,67,45,62,112,97,114,101,110,116,93,46,99,104,105,108,100,50,32,61,61,32,105,65,0,48,32,60,61,32,105,68,32,38,38,32,105,68,32,60,32,109,95,110,111,100,101,67,97,112,97,99,105,116,121,0,0,48,32,60,61,32,105,69,32,38,38,32,105,69,32,60,32,109,95,110,111,100,101,67,97,112,97,99,105,116,121,0,0,109,95,110,111,100,101,115,91,66,45,62,112,97,114,101,110,116,93,46,99,104,105,108,100,50,32,61,61,32,105,65,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,116,97,114,103,101,116,32,62,32,116,111,108,101,114,97,110,99,101,0,0,0,0,0,0,66,111,120,50,68,47,67,111,108,108,105,115,105,111,110,47,98,50,84,105,109,101,79,102,73,109,112,97,99,116,46,99,112,112,0,0,0,0,0,0,98,50,84,105,109,101,79,102,73,109,112,97,99,116,0,0,102,97,108,115,101,0,0,0,69,118,97,108,117,97,116,101,0,0,0,0,0,0,0,0,48,32,60,61,32,105,110,100,101,120,32,38,38,32,105,110,100,101,120,32,60,32,109,95,99,111,117,110,116,0,0,0,46,47,66,111,120,50,68,47,67,111,108,108,105,115,105,111,110,47,98,50,68,105,115,116,97,110,99,101,46,104,0,0,71,101,116,86,101,114,116,101,120,0,0,0,0,0,0,0,70,105,110,100,77,105,110,83,101,112,97,114,97,116,105,111,110,0,0,0,0,0,0,0,48,32,60,32,99,111,117,110,116,32,38,38,32,99,111,117,110,116,32,60,32,51,0,0,73,110,105,116,105,97,108,105,122,101,0,0,0,0,0,0,0,0,0,0,0,0,0,0,109,95,105,110,100,101,120,32,61,61,32,48,0,0,0,0,66,111,120,50,68,47,67,111,109,109,111,110,47,98,50,83,116,97,99,107,65,108,108,111,99,97,116,111,114,46,99,112,112,0,0,0,0,0,0,0,126,98,50,83,116,97,99,107,65,108,108,111,99,97,116,111,114,0,0,0,0,0,0,0,109,95,101,110,116,114,121,67,111,117,110,116,32,61,61,32,48,0,0,0,0,0,0,0,109,95,101,110,116,114,121,67,111,117,110,116,32,60,32,98,50,95,109,97,120,83,116,97,99,107,69,110,116,114,105,101,115,0,0,0,0,0,0,0,65,108,108,111,99,97,116,101,0,0,0,0,0,0,0,0,109,95,101,110,116,114,121,67,111,117,110,116,32,62,32,48,0,0,0,0,0,0,0,0,70,114,101,101,0,0,0,0,112,32,61,61,32,101,110,116,114,121,45,62,100,97,116,97,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,48,32,60,61,32,116,121,112,101,49,32,38,38,32,116,121,112,101,49,32,60,32,98,50,83,104,97,112,101,58,58,101,95,116,121,112,101,67,111,117,110,116,0,0,0,0,0,0,66,111,120,50,68,47,68,121,110,97,109,105,99,115,47,67,111,110,116,97,99,116,115,47,98,50,67,111,110,116,97,99,116,46,99,112,112,0,0,0,48,32,60,61,32,116,121,112,101,50,32,38,38,32,116,121,112,101,50,32,60,32,98,50,83,104,97,112,101,58,58,101,95,116,121,112,101,67,111,117,110,116,0,0,0,0,0,0,67,114,101,97,116,101,0,0,115,95,105,110,105,116,105,97,108,105,122,101,100,32,61,61,32,116,114,117,101,0,0,0,68,101,115,116,114,111,121,0,48,32,60,61,32,116,121,112,101,65,32,38,38,32,116,121,112,101,66,32,60,32,98,50,83,104,97,112,101,58,58,101,95,116,121,112,101,67,111,117,110,116,0,0,0,0,0,0,0,0,0,0,120,17,0,0,1,0,0,0,9,0,0,0,10,0,0,0,0,0,0,0,57,98,50,67,111,110,116,97,99,116,0,0,0,0,0,0,120,27,0,0,104,17,0,0,0,0,0,0,104,18,0,0,3,0,0,0,11,0,0,0,12,0,0,0,0,0,0,0,109,95,102,105,120,116,117,114,101,65,45,62,71,101,116,84,121,112,101,40,41,32,61,61,32,98,50,83,104,97,112,101,58,58,101,95,101,100,103,101,0,0,0,0,0,0,0,0,66,111,120,50,68,47,68,121,110,97,109,105,99,115,47,67,111,110,116,97,99,116,115,47,98,50,69,100,103,101,65,110,100,67,105,114,99,108,101,67,111,110,116,97,99,116,46,99,112,112,0,0,0,0,0,0,98,50,69,100,103,101,65,110,100,67,105,114,99,108,101,67,111,110,116,97,99,116,0,0,109,95,102,105,120,116,117,114,101,66,45,62,71,101,116,84,121,112,101,40,41,32,61,61,32,98,50,83,104,97,112,101,58,58,101,95,99,105,114,99,108,101,0,0,0,0,0,0,50,50,98,50,69,100,103,101,65,110,100,67,105,114,99,108,101,67,111,110,116,97,99,116,0,0,0,0,0,0,0,0,160,27,0,0,72,18,0,0,120,17,0,0,0,0,0,0,0,0,0,0,96,19,0,0,4,0,0,0,13,0,0,0,14,0,0,0,0,0,0,0,109,95,102,105,120,116,117,114,101,65,45,62,71,101,116,84,121,112,101,40,41,32,61,61,32,98,50,83,104,97,112,101,58,58,101,95,101,100,103,101,0,0,0,0,0,0,0,0,66,111,120,50,68,47,68,121,110,97,109,105,99,115,47,67,111,110,116,97,99,116,115,47,98,50,69,100,103,101,65,110,100,80,111,108,121,103,111,110,67,111,110,116,97,99,116,46,99,112,112,0,0,0,0,0,98,50,69,100,103,101,65,110,100,80,111,108,121,103,111,110,67,111,110,116,97,99,116,0,109,95,102,105,120,116,117,114,101,66,45,62,71,101,116,84,121,112,101,40,41,32,61,61,32,98,50,83,104,97,112,101,58,58,101,95,112,111,108,121,103,111,110,0,0,0,0,0,50,51,98,50,69,100,103,101,65,110,100,80,111,108,121,103,111,110,67,111,110,116,97,99,116,0,0,0,0,0,0,0,160,27,0,0,64,19,0,0,120,17,0,0,0,0,0,0,0,0,0,0,96,20,0,0,5,0,0,0,15,0,0,0,16,0,0,0,0,0,0,0,109,95,102,105,120,116,117,114,101,65,45,62,71,101,116,84,121,112,101,40,41,32,61,61,32,98,50,83,104,97,112,101,58,58,101,95,112,111,108,121,103,111,110,0,0,0,0,0,66,111,120,50,68,47,68,121,110,97,109,105,99,115,47,67,111,110,116,97,99,116,115,47,98,50,80,111,108,121,103,111,110,65,110,100,67,105,114,99,108,101,67,111,110,116,97,99,116,46,99,112,112,0,0,0,98,50,80,111,108,121,103,111,110,65,110,100,67,105,114,99,108,101,67,111,110,116,97,99,116,0,0,0,0,0,0,0,109,95,102,105,120,116,117,114,101,66,45,62,71,101,116,84,121,112,101,40,41,32,61,61,32,98,50,83,104,97,112,101,58,58,101,95,99,105,114,99,108,101,0,0,0,0,0,0,50,53,98,50,80,111,108,121,103,111,110,65,110,100,67,105,114,99,108,101,67,111,110,116,97,99,116,0,0,0,0,0,160,27,0,0,64,20,0,0,120,17,0,0,0,0,0,0,0,0,0,0,72,21,0,0,6,0,0,0,17,0,0,0,18,0,0,0,0,0,0,0,109,95,102,105,120,116,117,114,101,65,45,62,71,101,116,84,121,112,101,40,41,32,61,61,32,98,50,83,104,97,112,101,58,58,101,95,112,111,108,121,103,111,110,0,0,0,0,0,66,111,120,50,68,47,68,121,110,97,109,105,99,115,47,67,111,110,116,97,99,116,115,47,98,50,80,111,108,121,103,111,110,67,111,110,116,97,99,116,46,99,112,112,0,0,0,0,98,50,80,111,108,121,103,111,110,67,111,110,116,97,99,116,0,0,0,0,0,0,0,0,109,95,102,105,120,116,117,114,101,66,45,62,71,101,116,84,121,112,101,40,41,32,61,61,32,98,50,83,104,97,112,101,58,58,101,95,112,111,108,121,103,111,110,0,0,0,0,0,49,54,98,50,80,111,108,121,103,111,110,67,111,110,116,97,99,116,0,0,0,0,0,0,160,27,0,0,48,21,0,0,120,17,0,0,0,0,0,0,116,111,105,73,110,100,101,120,65,32,60,32,109,95,98,111,100,121,67,111,117,110,116,0,66,111,120,50,68,47,68,121,110,97,109,105,99,115,47,98,50,73,115,108,97,110,100,46,99,112,112,0,0,0,0,0,83,111,108,118,101,84,79,73,0,0,0,0,0,0,0,0,116,111,105,73,110,100,101,120,66,32,60,32,109,95,98,111,100,121,67,111,117,110,116,0,100,101,110,32,62,32,48,46,48,102,0,0,0,0,0,0,66,111,120,50,68,47,67,111,108,108,105,115,105,111,110,47,98,50,67,111,108,108,105,100,101,69,100,103,101,46,99,112,112,0,0,0,0,0,0,0,98,50,67,111,108,108,105,100,101,69,100,103,101,65,110,100,67,105,114,99,108,101,0,0,48,32,60,61,32,101,100,103,101,49,32,38,38,32,101,100,103,101,49,32,60,32,112,111,108,121,49,45,62,109,95,118,101,114,116,101,120,67,111,117,110,116,0,0,0,0,0,0,66,111,120,50,68,47,67,111,108,108,105,115,105,111,110,47,98,50,67,111,108,108,105,100,101,80,111,108,121,103,111,110,46,99,112,112,0,0,0,0,98,50,70,105,110,100,73,110,99,105,100,101,110,116,69,100,103,101,0,0,0,0,0,0,98,50,69,100,103,101,83,101,112,97,114,97,116,105,111,110,0,0,0,0,0,0,0,0,0,0,0,0,120,23,0,0,7,0,0,0,19,0,0,0,20,0,0,0,0,0,0,0,109,95,102,105,120,116,117,114,101,65,45,62,71,101,116,84,121,112,101,40,41,32,61,61,32,98,50,83,104,97,112,101,58,58,101,95,99,104,97,105,110,0,0,0,0,0,0,0,66,111,120,50,68,47,68,121,110,97,109,105,99,115,47,67,111,110,116,97,99,116,115,47,98,50,67,104,97,105,110,65,110,100,67,105,114,99,108,101,67,111,110,116,97,99,116,46,99,112,112,0,0,0,0,0,98,50,67,104,97,105,110,65,110,100,67,105,114,99,108,101,67,111,110,116,97,99,116,0,109,95,102,105,120,116,117,114,101,66,45,62,71,101,116,84,121,112,101,40,41,32,61,61,32,98,50,83,104,97,112,101,58,58,101,95,99,105,114,99,108,101,0,0,0,0,0,0,50,51,98,50,67,104,97,105,110,65,110,100,67,105,114,99,108,101,67,111,110,116,97,99,116,0,0,0,0,0,0,0,160,27,0,0,88,23,0,0,120,17,0,0,0,0,0,0,0,0,0,0,120,24,0,0,8,0,0,0,21,0,0,0,22,0,0,0,0,0,0,0,109,95,102,105,120,116,117,114,101,65,45,62,71,101,116,84,121,112,101,40,41,32,61,61,32,98,50,83,104,97,112,101,58,58,101,95,99,104,97,105,110,0,0,0,0,0,0,0,66,111,120,50,68,47,68,121,110,97,109,105,99,115,47,67,111,110,116,97,99,116,115,47,98,50,67,104,97,105,110,65,110,100,80,111,108,121,103,111,110,67,111,110,116,97,99,116,46,99,112,112,0,0,0,0,98,50,67,104,97,105,110,65,110,100,80,111,108,121,103,111,110,67,111,110,116,97,99,116,0,0,0,0,0,0,0,0,109,95,102,105,120,116,117,114,101,66,45,62,71,101,116,84,121,112,101,40,41,32,61,61,32,98,50,83,104,97,112,101,58,58,101,95,112,111,108,121,103,111,110,0,0,0,0,0,50,52,98,50,67,104,97,105,110,65,110,100,80,111,108,121,103,111,110,67,111,110,116,97,99,116,0,0,0,0,0,0,160,27,0,0,88,24,0,0,120,17,0,0,0,0,0,0,0,0,0,0,88,25,0,0,9,0,0,0,23,0,0,0,24,0,0,0,0,0,0,0,109,95,102,105,120,116,117,114,101,65,45,62,71,101,116,84,121,112,101,40,41,32,61,61,32,98,50,83,104,97,112,101,58,58,101,95,99,105,114,99,108,101,0,0,0,0,0,0,66,111,120,50,68,47,68,121,110,97,109,105,99,115,47,67,111,110,116,97,99,116,115,47,98,50,67,105,114,99,108,101,67,111,110,116,97,99,116,46,99,112,112,0,0,0,0,0,98,50,67,105,114,99,108,101,67,111,110,116,97,99,116,0,109,95,102,105,120,116,117,114,101,66,45,62,71,101,116,84,121,112,101,40,41,32,61,61,32,98,50,83,104,97,112,101,58,58,101,95,99,105,114,99,108,101,0,0,0,0,0,0,49,53,98,50,67,105,114,99,108,101,67,111,110,116,97,99,116,0,0,0,0,0,0,0,160,27,0,0,64,25,0,0,120,17,0,0,0,0,0,0,112,111,105,110,116,67,111,117,110,116,32,62,32,48,0,0,66,111,120,50,68,47,68,121,110,97,109,105,99,115,47,67,111,110,116,97,99,116,115,47,98,50,67,111,110,116,97,99,116,83,111,108,118,101,114,46,99,112,112,0,0,0,0,0,98,50,67,111,110,116,97,99,116,83,111,108,118,101,114,0,109,97,110,105,102,111,108,100,45,62,112,111,105,110,116,67,111,117,110,116,32,62,32,48,0,0,0,0,0,0,0,0,73,110,105,116,105,97,108,105,122,101,86,101,108,111,99,105,116,121,67,111,110,115,116,114,97,105,110,116,115,0,0,0,112,111,105,110,116,67,111,117,110,116,32,61,61,32,49,32,124,124,32,112,111,105,110,116,67,111,117,110,116,32,61,61,32,50,0,0,0,0,0,0,83,111,108,118,101,86,101,108,111,99,105,116,121,67,111,110,115,116,114,97,105,110,116,115,0,0,0,0,0,0,0,0,97,46,120,32,62,61,32,48,46,48,102,32,38,38,32,97,46,121,32,62,61,32,48,46,48,102,0,0,0,0,0,0,112,99,45,62,112,111,105,110,116,67,111,117,110,116,32,62,32,48,0,0,0,0,0,0,73,110,105,116,105,97,108,105,122,101,0,0,0,0,0,0,66,111,120,50,68,47,67,111,108,108,105,115,105,111,110,47,83,104,97,112,101,115,47,98,50,67,104,97,105,110,83,104,97,112,101,46,99,112,112,0,48,32,60,61,32,105,110,100,101,120,32,38,38,32,105,110,100,101,120,32,60,32,109,95,99,111,117,110,116,32,45,32,49,0,0,0,0,0,0,0,71,101,116,67,104,105,108,100,69,100,103,101,0,0,0,0,83,116,57,116,121,112,101,95,105,110,102,111,0,0,0,0,120,27,0,0,232,26,0,0,78,49,48,95,95,99,120,120,97,98,105,118,49,49,54,95,95,115,104,105,109,95,116,121,112,101,95,105,110,102,111,69,0,0,0,0,0,0,0,0,160,27,0,0,0,27,0,0,248,26,0,0,0,0,0,0,78,49,48,95,95,99,120,120,97,98,105,118,49,49,55,95,95,99,108,97,115,115,95,116,121,112,101,95,105,110,102,111,69,0,0,0,0,0,0,0,160,27,0,0,56,27,0,0,40,27,0,0,0,0,0,0,0,0,0,0,96,27,0,0,25,0,0,0,26,0,0,0,27,0,0,0,28,0,0,0,4,0,0,0,1,0,0,0,1,0,0,0,10,0,0,0,0,0,0,0,232,27,0,0,25,0,0,0,29,0,0,0,27,0,0,0,28,0,0,0,4,0,0,0,2,0,0,0,2,0,0,0,11,0,0,0,78,49,48,95,95,99,120,120,97,98,105,118,49,50,48,95,95,115,105,95,99,108,97,115,115,95,116,121,112,101,95,105,110,102,111,69,0,0,0,0,160,27,0,0,192,27,0,0,96,27,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,40,30,0,0,30,0,0,0,31,0,0,0,3,0,0,0,0,0,0,0,115,116,100,58,58,98,97,100,95,97,108,108,111,99,0,0,83,116,57,98,97,100,95,97,108,108,111,99,0,0,0,0,160,27,0,0,24,30,0,0,0,0,0,0,0,0,0,0], "i8", ALLOC_NONE, Runtime.GLOBAL_BASE);




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


  function _emscripten_set_main_loop(func, fps, simulateInfiniteLoop, arg) {
      Module['noExitRuntime'] = true;

      Browser.mainLoop.runner = function Browser_mainLoop_runner() {
        if (ABORT) return;
        if (Browser.mainLoop.queue.length > 0) {
          var start = Date.now();
          var blocker = Browser.mainLoop.queue.shift();
          blocker.func(blocker.arg);
          if (Browser.mainLoop.remainingBlockers) {
            var remaining = Browser.mainLoop.remainingBlockers;
            var next = remaining%1 == 0 ? remaining-1 : Math.floor(remaining);
            if (blocker.counted) {
              Browser.mainLoop.remainingBlockers = next;
            } else {
              // not counted, but move the progress along a tiny bit
              next = next + 0.5; // do not steal all the next one's progress
              Browser.mainLoop.remainingBlockers = (8*remaining + next)/9;
            }
          }
          console.log('main loop blocker "' + blocker.name + '" took ' + (Date.now() - start) + ' ms'); //, left: ' + Browser.mainLoop.remainingBlockers);
          Browser.mainLoop.updateStatus();
          setTimeout(Browser.mainLoop.runner, 0);
          return;
        }
        if (Browser.mainLoop.shouldPause) {
          // catch pauses from non-main loop sources
          Browser.mainLoop.paused = true;
          Browser.mainLoop.shouldPause = false;
          return;
        }

        // Signal GL rendering layer that processing of a new frame is about to start. This helps it optimize
        // VBO double-buffering and reduce GPU stalls.

        if (Browser.mainLoop.method === 'timeout' && Module.ctx) {
          Module.printErr('Looks like you are rendering without using requestAnimationFrame for the main loop. You should use 0 for the frame rate in emscripten_set_main_loop in order to use requestAnimationFrame, as that can greatly improve your frame rates!');
          Browser.mainLoop.method = ''; // just warn once per call to set main loop
        }

        if (Module['preMainLoop']) {
          Module['preMainLoop']();
        }

        try {
          if (typeof arg !== 'undefined') {
            Runtime.dynCall('vi', func, [arg]);
          } else {
            Runtime.dynCall('v', func);
          }
        } catch (e) {
          if (e instanceof ExitStatus) {
            return;
          } else {
            if (e && typeof e === 'object' && e.stack) Module.printErr('exception thrown: ' + [e, e.stack]);
            throw e;
          }
        }

        if (Module['postMainLoop']) {
          Module['postMainLoop']();
        }

        if (Browser.mainLoop.shouldPause) {
          // catch pauses from the main loop itself
          Browser.mainLoop.paused = true;
          Browser.mainLoop.shouldPause = false;
          return;
        }
        Browser.mainLoop.scheduler();
      }
      if (fps && fps > 0) {
        Browser.mainLoop.scheduler = function Browser_mainLoop_scheduler() {
          setTimeout(Browser.mainLoop.runner, 1000/fps); // doing this each time means that on exception, we stop
        };
        Browser.mainLoop.method = 'timeout';
      } else {
        Browser.mainLoop.scheduler = function Browser_mainLoop_scheduler() {
          Browser.requestAnimationFrame(Browser.mainLoop.runner);
        };
        Browser.mainLoop.method = 'rAF';
      }
      Browser.mainLoop.scheduler();

      if (simulateInfiniteLoop) {
        throw 'SimulateInfiniteLoop';
      }
    }

  var _cosf=Math_cos;

  function ___cxa_pure_virtual() {
      ABORT = true;
      throw 'Pure virtual function called!';
    }

  function _time(ptr) {
      var ret = Math.floor(Date.now()/1000);
      if (ptr) {
        HEAP32[((ptr)>>2)]=ret;
      }
      return ret;
    }

  function ___assert_fail(condition, filename, line, func) {
      ABORT = true;
      throw 'Assertion failed: ' + Pointer_stringify(condition) + ', at: ' + [filename ? Pointer_stringify(filename) : 'unknown filename', line, func ? Pointer_stringify(func) : 'unknown function'] + ' at ' + stackTrace();
    }


  function __ZSt18uncaught_exceptionv() { // std::uncaught_exception()
      return !!__ZSt18uncaught_exceptionv.uncaught_exception;
    }



  function ___cxa_is_number_type(type) {
      var isNumber = false;
      try { if (type == __ZTIi) isNumber = true } catch(e){}
      try { if (type == __ZTIj) isNumber = true } catch(e){}
      try { if (type == __ZTIl) isNumber = true } catch(e){}
      try { if (type == __ZTIm) isNumber = true } catch(e){}
      try { if (type == __ZTIx) isNumber = true } catch(e){}
      try { if (type == __ZTIy) isNumber = true } catch(e){}
      try { if (type == __ZTIf) isNumber = true } catch(e){}
      try { if (type == __ZTId) isNumber = true } catch(e){}
      try { if (type == __ZTIe) isNumber = true } catch(e){}
      try { if (type == __ZTIc) isNumber = true } catch(e){}
      try { if (type == __ZTIa) isNumber = true } catch(e){}
      try { if (type == __ZTIh) isNumber = true } catch(e){}
      try { if (type == __ZTIs) isNumber = true } catch(e){}
      try { if (type == __ZTIt) isNumber = true } catch(e){}
      return isNumber;
    }function ___cxa_does_inherit(definiteType, possibilityType, possibility) {
      if (possibility == 0) return false;
      if (possibilityType == 0 || possibilityType == definiteType)
        return true;
      var possibility_type_info;
      if (___cxa_is_number_type(possibilityType)) {
        possibility_type_info = possibilityType;
      } else {
        var possibility_type_infoAddr = HEAP32[((possibilityType)>>2)] - 8;
        possibility_type_info = HEAP32[((possibility_type_infoAddr)>>2)];
      }
      switch (possibility_type_info) {
      case 0: // possibility is a pointer
        // See if definite type is a pointer
        var definite_type_infoAddr = HEAP32[((definiteType)>>2)] - 8;
        var definite_type_info = HEAP32[((definite_type_infoAddr)>>2)];
        if (definite_type_info == 0) {
          // Also a pointer; compare base types of pointers
          var defPointerBaseAddr = definiteType+8;
          var defPointerBaseType = HEAP32[((defPointerBaseAddr)>>2)];
          var possPointerBaseAddr = possibilityType+8;
          var possPointerBaseType = HEAP32[((possPointerBaseAddr)>>2)];
          return ___cxa_does_inherit(defPointerBaseType, possPointerBaseType, possibility);
        } else
          return false; // one pointer and one non-pointer
      case 1: // class with no base class
        return false;
      case 2: // class with base class
        var parentTypeAddr = possibilityType + 8;
        var parentType = HEAP32[((parentTypeAddr)>>2)];
        return ___cxa_does_inherit(definiteType, parentType, possibility);
      default:
        return false; // some unencountered type
      }
    }



  var ___cxa_last_thrown_exception=0;function ___resumeException(ptr) {
      if (!___cxa_last_thrown_exception) { ___cxa_last_thrown_exception = ptr; }
      throw ptr + " - Exception catching is disabled, this exception cannot be caught. Compile with -s DISABLE_EXCEPTION_CATCHING=0 or DISABLE_EXCEPTION_CATCHING=2 to catch.";
    }

  var ___cxa_exception_header_size=8;function ___cxa_find_matching_catch(thrown, throwntype) {
      if (thrown == -1) thrown = ___cxa_last_thrown_exception;
      header = thrown - ___cxa_exception_header_size;
      if (throwntype == -1) throwntype = HEAP32[((header)>>2)];
      var typeArray = Array.prototype.slice.call(arguments, 2);

      // If throwntype is a pointer, this means a pointer has been
      // thrown. When a pointer is thrown, actually what's thrown
      // is a pointer to the pointer. We'll dereference it.
      if (throwntype != 0 && !___cxa_is_number_type(throwntype)) {
        var throwntypeInfoAddr= HEAP32[((throwntype)>>2)] - 8;
        var throwntypeInfo= HEAP32[((throwntypeInfoAddr)>>2)];
        if (throwntypeInfo == 0)
          thrown = HEAP32[((thrown)>>2)];
      }
      // The different catch blocks are denoted by different types.
      // Due to inheritance, those types may not precisely match the
      // type of the thrown object. Find one which matches, and
      // return the type of the catch block which should be called.
      for (var i = 0; i < typeArray.length; i++) {
        if (___cxa_does_inherit(typeArray[i], throwntype, thrown))
          return ((asm["setTempRet0"](typeArray[i]),thrown)|0);
      }
      // Shouldn't happen unless we have bogus data in typeArray
      // or encounter a type for which emscripten doesn't have suitable
      // typeinfo defined. Best-efforts match just in case.
      return ((asm["setTempRet0"](throwntype),thrown)|0);
    }function ___cxa_throw(ptr, type, destructor) {
      if (!___cxa_throw.initialized) {
        try {
          HEAP32[((__ZTVN10__cxxabiv119__pointer_type_infoE)>>2)]=0; // Workaround for libcxxabi integration bug
        } catch(e){}
        try {
          HEAP32[((__ZTVN10__cxxabiv117__class_type_infoE)>>2)]=1; // Workaround for libcxxabi integration bug
        } catch(e){}
        try {
          HEAP32[((__ZTVN10__cxxabiv120__si_class_type_infoE)>>2)]=2; // Workaround for libcxxabi integration bug
        } catch(e){}
        ___cxa_throw.initialized = true;
      }
      var header = ptr - ___cxa_exception_header_size;
      HEAP32[((header)>>2)]=type;
      HEAP32[(((header)+(4))>>2)]=destructor;
      ___cxa_last_thrown_exception = ptr;
      if (!("uncaught_exception" in __ZSt18uncaught_exceptionv)) {
        __ZSt18uncaught_exceptionv.uncaught_exception = 1;
      } else {
        __ZSt18uncaught_exceptionv.uncaught_exception++;
      }
      throw ptr + " - Exception catching is disabled, this exception cannot be caught. Compile with -s DISABLE_EXCEPTION_CATCHING=0 or DISABLE_EXCEPTION_CATCHING=2 to catch.";
    }


  Module["_memset"] = _memset;



  function __exit(status) {
      // void _exit(int status);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/exit.html
      Module['exit'](status);
    }function _exit(status) {
      __exit(status);
    }function __ZSt9terminatev() {
      _exit(-1234);
    }

  function _abort() {
      Module['abort']();
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

  var _sinf=Math_sin;


  var _sqrtf=Math_sqrt;

  var _floorf=Math_floor;


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

  function _clock() {
      if (_clock.start === undefined) _clock.start = Date.now();
      return Math.floor((Date.now() - _clock.start) * (1000000/1000));
    }


  var ___cxa_caught_exceptions=[];function ___cxa_begin_catch(ptr) {
      __ZSt18uncaught_exceptionv.uncaught_exception--;
      ___cxa_caught_exceptions.push(___cxa_last_thrown_exception);
      return ptr;
    }

  function ___errno_location() {
      return ___errno_state;
    }


  function _emscripten_memcpy_big(dest, src, num) {
      HEAPU8.set(HEAPU8.subarray(src, src+num), dest);
      return dest;
    }
  Module["_memcpy"] = _memcpy;

  function __ZNSt9exceptionD2Ev() {}

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

  function _emscripten_run_script(ptr) {
      eval(Pointer_stringify(ptr));
    }


  function _malloc(bytes) {
      /* Over-allocate to make sure it is byte-aligned by 8.
       * This will leak memory, but this is only the dummy
       * implementation (replaced by dlmalloc normally) so
       * not an issue.
       */
      var ptr = Runtime.dynamicAlloc(bytes + 8);
      return (ptr+8) & 0xFFFFFFF8;
    }
  Module["_malloc"] = _malloc;function ___cxa_allocate_exception(size) {
      var ptr = _malloc(size + ___cxa_exception_header_size);
      return ptr + ___cxa_exception_header_size;
    }

  function _emscripten_cancel_main_loop() {
      Browser.mainLoop.scheduler = null;
      Browser.mainLoop.shouldPause = true;
    }

  var __ZTISt9exception=allocate([allocate([1,0,0,0,0,0,0], "i8", ALLOC_STATIC)+8, 0], "i32", ALLOC_STATIC);
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

function invoke_viiiii(index,a1,a2,a3,a4,a5) {
  try {
    Module["dynCall_viiiii"](index,a1,a2,a3,a4,a5);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}

function invoke_vi(index,a1) {
  try {
    Module["dynCall_vi"](index,a1);
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

function invoke_ii(index,a1) {
  try {
    return Module["dynCall_ii"](index,a1);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}

function invoke_viii(index,a1,a2,a3) {
  try {
    Module["dynCall_viii"](index,a1,a2,a3);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}

function invoke_v(index) {
  try {
    Module["dynCall_v"](index);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}

function invoke_viid(index,a1,a2,a3) {
  try {
    Module["dynCall_viid"](index,a1,a2,a3);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}

function invoke_viiiiii(index,a1,a2,a3,a4,a5,a6) {
  try {
    Module["dynCall_viiiiii"](index,a1,a2,a3,a4,a5,a6);
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

function invoke_iiiiii(index,a1,a2,a3,a4,a5) {
  try {
    return Module["dynCall_iiiiii"](index,a1,a2,a3,a4,a5);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}

function invoke_viiii(index,a1,a2,a3,a4) {
  try {
    Module["dynCall_viiii"](index,a1,a2,a3,a4);
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
var ModuleFunc;
var asm = (ModuleFunc = function(global, env, buffer) {
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
  var __ZTISt9exception=env.__ZTISt9exception|0;

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
  var invoke_viiiii=env.invoke_viiiii;
  var invoke_vi=env.invoke_vi;
  var invoke_vii=env.invoke_vii;
  var invoke_ii=env.invoke_ii;
  var invoke_viii=env.invoke_viii;
  var invoke_v=env.invoke_v;
  var invoke_viid=env.invoke_viid;
  var invoke_viiiiii=env.invoke_viiiiii;
  var invoke_iii=env.invoke_iii;
  var invoke_iiiiii=env.invoke_iiiiii;
  var invoke_viiii=env.invoke_viiii;
  var ___cxa_throw=env.___cxa_throw;
  var _emscripten_run_script=env._emscripten_run_script;
  var _cosf=env._cosf;
  var _send=env._send;
  var __ZSt9terminatev=env.__ZSt9terminatev;
  var __reallyNegative=env.__reallyNegative;
  var ___cxa_is_number_type=env.___cxa_is_number_type;
  var ___assert_fail=env.___assert_fail;
  var ___cxa_allocate_exception=env.___cxa_allocate_exception;
  var ___cxa_find_matching_catch=env.___cxa_find_matching_catch;
  var _fflush=env._fflush;
  var _pwrite=env._pwrite;
  var ___setErrNo=env.___setErrNo;
  var _sbrk=env._sbrk;
  var ___cxa_begin_catch=env.___cxa_begin_catch;
  var _sinf=env._sinf;
  var _fileno=env._fileno;
  var ___resumeException=env.___resumeException;
  var __ZSt18uncaught_exceptionv=env.__ZSt18uncaught_exceptionv;
  var _sysconf=env._sysconf;
  var _clock=env._clock;
  var _emscripten_memcpy_big=env._emscripten_memcpy_big;
  var _puts=env._puts;
  var _mkport=env._mkport;
  var _floorf=env._floorf;
  var _sqrtf=env._sqrtf;
  var _write=env._write;
  var _emscripten_set_main_loop=env._emscripten_set_main_loop;
  var ___errno_location=env.___errno_location;
  var __ZNSt9exceptionD2Ev=env.__ZNSt9exceptionD2Ev;
  var _printf=env._printf;
  var ___cxa_does_inherit=env.___cxa_does_inherit;
  var __exit=env.__exit;
  var _fputc=env._fputc;
  var _abort=env._abort;
  var _fwrite=env._fwrite;
  var _time=env._time;
  var _fprintf=env._fprintf;
  var _emscripten_cancel_main_loop=env._emscripten_cancel_main_loop;
  var __formatString=env.__formatString;
  var _fputs=env._fputs;
  var _exit=env._exit;
  var ___cxa_pure_virtual=env.___cxa_pure_virtual;
  var tempFloat = 0.0;

// EMSCRIPTEN_START_FUNCS
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
   i18 = HEAP32[1790] | 0;
   i21 = i18 >>> i20;
   if ((i21 & 3 | 0) != 0) {
    i6 = (i21 & 1 ^ 1) + i20 | 0;
    i5 = i6 << 1;
    i3 = 7200 + (i5 << 2) | 0;
    i5 = 7200 + (i5 + 2 << 2) | 0;
    i7 = HEAP32[i5 >> 2] | 0;
    i2 = i7 + 8 | 0;
    i4 = HEAP32[i2 >> 2] | 0;
    do {
     if ((i3 | 0) != (i4 | 0)) {
      if (i4 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
      HEAP32[1790] = i18 & ~(1 << i6);
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
   if (i12 >>> 0 > (HEAP32[7168 >> 2] | 0) >>> 0) {
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
     i4 = 7200 + (i7 << 2) | 0;
     i7 = 7200 + (i7 + 2 << 2) | 0;
     i5 = HEAP32[i7 >> 2] | 0;
     i2 = i5 + 8 | 0;
     i6 = HEAP32[i2 >> 2] | 0;
     do {
      if ((i4 | 0) != (i6 | 0)) {
       if (i6 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
       HEAP32[1790] = i18 & ~(1 << i3);
      }
     } while (0);
     i6 = i3 << 3;
     i4 = i6 - i12 | 0;
     HEAP32[i5 + 4 >> 2] = i12 | 3;
     i3 = i5 + i12 | 0;
     HEAP32[i5 + (i12 | 4) >> 2] = i4 | 1;
     HEAP32[i5 + i6 >> 2] = i4;
     i6 = HEAP32[7168 >> 2] | 0;
     if ((i6 | 0) != 0) {
      i5 = HEAP32[7180 >> 2] | 0;
      i8 = i6 >>> 3;
      i9 = i8 << 1;
      i6 = 7200 + (i9 << 2) | 0;
      i7 = HEAP32[1790] | 0;
      i8 = 1 << i8;
      if ((i7 & i8 | 0) != 0) {
       i7 = 7200 + (i9 + 2 << 2) | 0;
       i8 = HEAP32[i7 >> 2] | 0;
       if (i8 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
        _abort();
       } else {
        i28 = i7;
        i27 = i8;
       }
      } else {
       HEAP32[1790] = i7 | i8;
       i28 = 7200 + (i9 + 2 << 2) | 0;
       i27 = i6;
      }
      HEAP32[i28 >> 2] = i5;
      HEAP32[i27 + 12 >> 2] = i5;
      HEAP32[i5 + 8 >> 2] = i27;
      HEAP32[i5 + 12 >> 2] = i6;
     }
     HEAP32[7168 >> 2] = i4;
     HEAP32[7180 >> 2] = i3;
     i32 = i2;
     STACKTOP = i1;
     return i32 | 0;
    }
    i18 = HEAP32[7164 >> 2] | 0;
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
     i3 = HEAP32[7464 + ((i30 | i31 | i32 | i6 | i3) + (i2 >>> i3) << 2) >> 2] | 0;
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
     i6 = HEAP32[7176 >> 2] | 0;
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
       i6 = 7464 + (i7 << 2) | 0;
       if ((i3 | 0) == (HEAP32[i6 >> 2] | 0)) {
        HEAP32[i6 >> 2] = i26;
        if ((i26 | 0) == 0) {
         HEAP32[7164 >> 2] = HEAP32[7164 >> 2] & ~(1 << i7);
         break;
        }
       } else {
        if (i5 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
       if (i26 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
        _abort();
       }
       HEAP32[i26 + 24 >> 2] = i5;
       i5 = HEAP32[i3 + 16 >> 2] | 0;
       do {
        if ((i5 | 0) != 0) {
         if (i5 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
        if (i5 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
      i6 = HEAP32[7168 >> 2] | 0;
      if ((i6 | 0) != 0) {
       i5 = HEAP32[7180 >> 2] | 0;
       i8 = i6 >>> 3;
       i9 = i8 << 1;
       i6 = 7200 + (i9 << 2) | 0;
       i7 = HEAP32[1790] | 0;
       i8 = 1 << i8;
       if ((i7 & i8 | 0) != 0) {
        i7 = 7200 + (i9 + 2 << 2) | 0;
        i8 = HEAP32[i7 >> 2] | 0;
        if (i8 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
         _abort();
        } else {
         i25 = i7;
         i24 = i8;
        }
       } else {
        HEAP32[1790] = i7 | i8;
        i25 = 7200 + (i9 + 2 << 2) | 0;
        i24 = i6;
       }
       HEAP32[i25 >> 2] = i5;
       HEAP32[i24 + 12 >> 2] = i5;
       HEAP32[i5 + 8 >> 2] = i24;
       HEAP32[i5 + 12 >> 2] = i6;
      }
      HEAP32[7168 >> 2] = i2;
      HEAP32[7180 >> 2] = i4;
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
    i26 = HEAP32[7164 >> 2] | 0;
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
     i30 = HEAP32[7464 + (i27 << 2) >> 2] | 0;
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
      i29 = HEAP32[7464 + ((i27 | i28 | i30 | i31 | i29) + (i32 >>> i29) << 2) >> 2] | 0;
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
     if ((i24 | 0) != 0 ? i25 >>> 0 < ((HEAP32[7168 >> 2] | 0) - i12 | 0) >>> 0 : 0) {
      i4 = HEAP32[7176 >> 2] | 0;
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
        i5 = 7464 + (i4 << 2) | 0;
        if ((i24 | 0) == (HEAP32[i5 >> 2] | 0)) {
         HEAP32[i5 >> 2] = i22;
         if ((i22 | 0) == 0) {
          HEAP32[7164 >> 2] = HEAP32[7164 >> 2] & ~(1 << i4);
          break;
         }
        } else {
         if (i3 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
        if (i22 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
         _abort();
        }
        HEAP32[i22 + 24 >> 2] = i3;
        i3 = HEAP32[i24 + 16 >> 2] | 0;
        do {
         if ((i3 | 0) != 0) {
          if (i3 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
         if (i3 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
         i3 = 7200 + (i6 << 2) | 0;
         i5 = HEAP32[1790] | 0;
         i4 = 1 << i4;
         if ((i5 & i4 | 0) != 0) {
          i5 = 7200 + (i6 + 2 << 2) | 0;
          i4 = HEAP32[i5 >> 2] | 0;
          if (i4 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
           _abort();
          } else {
           i21 = i5;
           i20 = i4;
          }
         } else {
          HEAP32[1790] = i5 | i4;
          i21 = 7200 + (i6 + 2 << 2) | 0;
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
        i6 = 7464 + (i3 << 2) | 0;
        HEAP32[i24 + (i12 + 28) >> 2] = i3;
        HEAP32[i24 + (i12 + 20) >> 2] = 0;
        HEAP32[i24 + (i12 + 16) >> 2] = 0;
        i4 = HEAP32[7164 >> 2] | 0;
        i5 = 1 << i3;
        if ((i4 & i5 | 0) == 0) {
         HEAP32[7164 >> 2] = i4 | i5;
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
          if (i6 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
        i5 = HEAP32[7176 >> 2] | 0;
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
 i18 = HEAP32[7168 >> 2] | 0;
 if (!(i12 >>> 0 > i18 >>> 0)) {
  i3 = i18 - i12 | 0;
  i2 = HEAP32[7180 >> 2] | 0;
  if (i3 >>> 0 > 15) {
   HEAP32[7180 >> 2] = i2 + i12;
   HEAP32[7168 >> 2] = i3;
   HEAP32[i2 + (i12 + 4) >> 2] = i3 | 1;
   HEAP32[i2 + i18 >> 2] = i3;
   HEAP32[i2 + 4 >> 2] = i12 | 3;
  } else {
   HEAP32[7168 >> 2] = 0;
   HEAP32[7180 >> 2] = 0;
   HEAP32[i2 + 4 >> 2] = i18 | 3;
   i32 = i2 + (i18 + 4) | 0;
   HEAP32[i32 >> 2] = HEAP32[i32 >> 2] | 1;
  }
  i32 = i2 + 8 | 0;
  STACKTOP = i1;
  return i32 | 0;
 }
 i18 = HEAP32[7172 >> 2] | 0;
 if (i12 >>> 0 < i18 >>> 0) {
  i31 = i18 - i12 | 0;
  HEAP32[7172 >> 2] = i31;
  i32 = HEAP32[7184 >> 2] | 0;
  HEAP32[7184 >> 2] = i32 + i12;
  HEAP32[i32 + (i12 + 4) >> 2] = i31 | 1;
  HEAP32[i32 + 4 >> 2] = i12 | 3;
  i32 = i32 + 8 | 0;
  STACKTOP = i1;
  return i32 | 0;
 }
 do {
  if ((HEAP32[1908] | 0) == 0) {
   i18 = _sysconf(30) | 0;
   if ((i18 + -1 & i18 | 0) == 0) {
    HEAP32[7640 >> 2] = i18;
    HEAP32[7636 >> 2] = i18;
    HEAP32[7644 >> 2] = -1;
    HEAP32[7648 >> 2] = -1;
    HEAP32[7652 >> 2] = 0;
    HEAP32[7604 >> 2] = 0;
    HEAP32[1908] = (_time(0) | 0) & -16 ^ 1431655768;
    break;
   } else {
    _abort();
   }
  }
 } while (0);
 i20 = i12 + 48 | 0;
 i25 = HEAP32[7640 >> 2] | 0;
 i21 = i12 + 47 | 0;
 i22 = i25 + i21 | 0;
 i25 = 0 - i25 | 0;
 i18 = i22 & i25;
 if (!(i18 >>> 0 > i12 >>> 0)) {
  i32 = 0;
  STACKTOP = i1;
  return i32 | 0;
 }
 i24 = HEAP32[7600 >> 2] | 0;
 if ((i24 | 0) != 0 ? (i31 = HEAP32[7592 >> 2] | 0, i32 = i31 + i18 | 0, i32 >>> 0 <= i31 >>> 0 | i32 >>> 0 > i24 >>> 0) : 0) {
  i32 = 0;
  STACKTOP = i1;
  return i32 | 0;
 }
 L269 : do {
  if ((HEAP32[7604 >> 2] & 4 | 0) == 0) {
   i26 = HEAP32[7184 >> 2] | 0;
   L271 : do {
    if ((i26 | 0) != 0) {
     i24 = 7608 | 0;
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
      i25 = i22 - (HEAP32[7172 >> 2] | 0) & i25;
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
      i22 = HEAP32[7636 >> 2] | 0;
      i25 = i22 + -1 | 0;
      if ((i25 & i24 | 0) == 0) {
       i25 = i18;
      } else {
       i25 = i18 - i24 + (i25 + i24 & 0 - i22) | 0;
      }
      i24 = HEAP32[7592 >> 2] | 0;
      i26 = i24 + i25 | 0;
      if (i25 >>> 0 > i12 >>> 0 & i25 >>> 0 < 2147483647) {
       i22 = HEAP32[7600 >> 2] | 0;
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
      if ((i22 | 0) != (-1 | 0) & i24 >>> 0 < 2147483647 & i24 >>> 0 < i20 >>> 0 ? (i19 = HEAP32[7640 >> 2] | 0, i19 = i21 - i24 + i19 & 0 - i19, i19 >>> 0 < 2147483647) : 0) {
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
   HEAP32[7604 >> 2] = HEAP32[7604 >> 2] | 4;
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
  i15 = (HEAP32[7592 >> 2] | 0) + i14 | 0;
  HEAP32[7592 >> 2] = i15;
  if (i15 >>> 0 > (HEAP32[7596 >> 2] | 0) >>> 0) {
   HEAP32[7596 >> 2] = i15;
  }
  i15 = HEAP32[7184 >> 2] | 0;
  L311 : do {
   if ((i15 | 0) != 0) {
    i21 = 7608 | 0;
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
     i2 = (HEAP32[7172 >> 2] | 0) + i14 | 0;
     i3 = i15 + 8 | 0;
     if ((i3 & 7 | 0) == 0) {
      i3 = 0;
     } else {
      i3 = 0 - i3 & 7;
     }
     i32 = i2 - i3 | 0;
     HEAP32[7184 >> 2] = i15 + i3;
     HEAP32[7172 >> 2] = i32;
     HEAP32[i15 + (i3 + 4) >> 2] = i32 | 1;
     HEAP32[i15 + (i2 + 4) >> 2] = 40;
     HEAP32[7188 >> 2] = HEAP32[7648 >> 2];
     break;
    }
    if (i17 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
     HEAP32[7176 >> 2] = i17;
    }
    i19 = i17 + i14 | 0;
    i16 = 7608 | 0;
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
      if ((i15 | 0) != (HEAP32[7184 >> 2] | 0)) {
       if ((i15 | 0) == (HEAP32[7180 >> 2] | 0)) {
        i32 = (HEAP32[7168 >> 2] | 0) + i10 | 0;
        HEAP32[7168 >> 2] = i32;
        HEAP32[7180 >> 2] = i7;
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
            if (i18 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
             _abort();
            } else {
             HEAP32[i18 >> 2] = 0;
             i5 = i16;
             break;
            }
           } else {
            i18 = HEAP32[i17 + ((i13 | 8) + i14) >> 2] | 0;
            if (i18 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
           i18 = 7464 + (i16 << 2) | 0;
           if ((i15 | 0) == (HEAP32[i18 >> 2] | 0)) {
            HEAP32[i18 >> 2] = i5;
            if ((i5 | 0) == 0) {
             HEAP32[7164 >> 2] = HEAP32[7164 >> 2] & ~(1 << i16);
             break;
            }
           } else {
            if (i9 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
           if (i5 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
            _abort();
           }
           HEAP32[i5 + 24 >> 2] = i9;
           i15 = i13 | 16;
           i9 = HEAP32[i17 + (i15 + i14) >> 2] | 0;
           do {
            if ((i9 | 0) != 0) {
             if (i9 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
            if (i9 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
          i18 = 7200 + (i16 << 1 << 2) | 0;
          if ((i5 | 0) != (i18 | 0)) {
           if (i5 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
            _abort();
           }
           if ((HEAP32[i5 + 12 >> 2] | 0) != (i15 | 0)) {
            _abort();
           }
          }
          if ((i12 | 0) == (i5 | 0)) {
           HEAP32[1790] = HEAP32[1790] & ~(1 << i16);
           break;
          }
          if ((i12 | 0) != (i18 | 0)) {
           if (i12 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
        i2 = 7200 + (i10 << 2) | 0;
        i9 = HEAP32[1790] | 0;
        i5 = 1 << i5;
        if ((i9 & i5 | 0) != 0) {
         i9 = 7200 + (i10 + 2 << 2) | 0;
         i5 = HEAP32[i9 >> 2] | 0;
         if (i5 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
          _abort();
         } else {
          i3 = i9;
          i4 = i5;
         }
        } else {
         HEAP32[1790] = i9 | i5;
         i3 = 7200 + (i10 + 2 << 2) | 0;
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
       i4 = 7464 + (i3 << 2) | 0;
       HEAP32[i17 + (i8 + 28) >> 2] = i3;
       HEAP32[i17 + (i8 + 20) >> 2] = 0;
       HEAP32[i17 + (i8 + 16) >> 2] = 0;
       i9 = HEAP32[7164 >> 2] | 0;
       i5 = 1 << i3;
       if ((i9 & i5 | 0) == 0) {
        HEAP32[7164 >> 2] = i9 | i5;
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
         if (i5 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
       i5 = HEAP32[7176 >> 2] | 0;
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
       i32 = (HEAP32[7172 >> 2] | 0) + i10 | 0;
       HEAP32[7172 >> 2] = i32;
       HEAP32[7184 >> 2] = i7;
       HEAP32[i17 + (i8 + 4) >> 2] = i32 | 1;
      }
     } while (0);
     i32 = i17 + (i6 | 8) | 0;
     STACKTOP = i1;
     return i32 | 0;
    }
    i3 = 7608 | 0;
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
    HEAP32[7184 >> 2] = i17 + i4;
    HEAP32[7172 >> 2] = i32;
    HEAP32[i17 + (i4 + 4) >> 2] = i32 | 1;
    HEAP32[i17 + (i14 + -36) >> 2] = 40;
    HEAP32[7188 >> 2] = HEAP32[7648 >> 2];
    HEAP32[i2 + 4 >> 2] = 27;
    HEAP32[i3 + 0 >> 2] = HEAP32[7608 >> 2];
    HEAP32[i3 + 4 >> 2] = HEAP32[7612 >> 2];
    HEAP32[i3 + 8 >> 2] = HEAP32[7616 >> 2];
    HEAP32[i3 + 12 >> 2] = HEAP32[7620 >> 2];
    HEAP32[7608 >> 2] = i17;
    HEAP32[7612 >> 2] = i14;
    HEAP32[7620 >> 2] = 0;
    HEAP32[7616 >> 2] = i3;
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
      i2 = 7200 + (i4 << 2) | 0;
      i5 = HEAP32[1790] | 0;
      i3 = 1 << i3;
      if ((i5 & i3 | 0) != 0) {
       i4 = 7200 + (i4 + 2 << 2) | 0;
       i3 = HEAP32[i4 >> 2] | 0;
       if (i3 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
        _abort();
       } else {
        i7 = i4;
        i8 = i3;
       }
      } else {
       HEAP32[1790] = i5 | i3;
       i7 = 7200 + (i4 + 2 << 2) | 0;
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
     i7 = 7464 + (i3 << 2) | 0;
     HEAP32[i15 + 28 >> 2] = i3;
     HEAP32[i15 + 20 >> 2] = 0;
     HEAP32[i15 + 16 >> 2] = 0;
     i4 = HEAP32[7164 >> 2] | 0;
     i5 = 1 << i3;
     if ((i4 & i5 | 0) == 0) {
      HEAP32[7164 >> 2] = i4 | i5;
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
       if (i7 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
     i2 = HEAP32[7176 >> 2] | 0;
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
    i32 = HEAP32[7176 >> 2] | 0;
    if ((i32 | 0) == 0 | i17 >>> 0 < i32 >>> 0) {
     HEAP32[7176 >> 2] = i17;
    }
    HEAP32[7608 >> 2] = i17;
    HEAP32[7612 >> 2] = i14;
    HEAP32[7620 >> 2] = 0;
    HEAP32[7196 >> 2] = HEAP32[1908];
    HEAP32[7192 >> 2] = -1;
    i2 = 0;
    do {
     i32 = i2 << 1;
     i31 = 7200 + (i32 << 2) | 0;
     HEAP32[7200 + (i32 + 3 << 2) >> 2] = i31;
     HEAP32[7200 + (i32 + 2 << 2) >> 2] = i31;
     i2 = i2 + 1 | 0;
    } while ((i2 | 0) != 32);
    i2 = i17 + 8 | 0;
    if ((i2 & 7 | 0) == 0) {
     i2 = 0;
    } else {
     i2 = 0 - i2 & 7;
    }
    i32 = i14 + -40 - i2 | 0;
    HEAP32[7184 >> 2] = i17 + i2;
    HEAP32[7172 >> 2] = i32;
    HEAP32[i17 + (i2 + 4) >> 2] = i32 | 1;
    HEAP32[i17 + (i14 + -36) >> 2] = 40;
    HEAP32[7188 >> 2] = HEAP32[7648 >> 2];
   }
  } while (0);
  i2 = HEAP32[7172 >> 2] | 0;
  if (i2 >>> 0 > i12 >>> 0) {
   i31 = i2 - i12 | 0;
   HEAP32[7172 >> 2] = i31;
   i32 = HEAP32[7184 >> 2] | 0;
   HEAP32[7184 >> 2] = i32 + i12;
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
function __ZN12b2EPCollider7CollideEP10b2ManifoldPK11b2EdgeShapeRK11b2TransformPK14b2PolygonShapeS7_(i12, i2, i16, i5, i8, i6) {
 i12 = i12 | 0;
 i2 = i2 | 0;
 i16 = i16 | 0;
 i5 = i5 | 0;
 i8 = i8 | 0;
 i6 = i6 | 0;
 var i1 = 0, i3 = 0, i4 = 0, i7 = 0, i9 = 0, i10 = 0, i11 = 0, i13 = 0, i14 = 0, i15 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, d22 = 0.0, d23 = 0.0, d24 = 0.0, d25 = 0.0, d26 = 0.0, d27 = 0.0, d28 = 0.0, i29 = 0, d30 = 0.0, d31 = 0.0, d32 = 0.0, d33 = 0.0, i34 = 0, i35 = 0, d36 = 0.0, d37 = 0.0, i38 = 0, d39 = 0.0, i40 = 0, i41 = 0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 144 | 0;
 i18 = i1 + 128 | 0;
 i11 = i1 + 24 | 0;
 i9 = i1 + 72 | 0;
 i10 = i1 + 48 | 0;
 i3 = i1;
 i4 = i12 + 132 | 0;
 d28 = +HEAPF32[i5 + 12 >> 2];
 d37 = +HEAPF32[i6 + 8 >> 2];
 d23 = +HEAPF32[i5 + 8 >> 2];
 d27 = +HEAPF32[i6 + 12 >> 2];
 d22 = d28 * d37 - d23 * d27;
 d27 = d37 * d23 + d28 * d27;
 d37 = +d22;
 d26 = +d27;
 d25 = +HEAPF32[i6 >> 2] - +HEAPF32[i5 >> 2];
 d36 = +HEAPF32[i6 + 4 >> 2] - +HEAPF32[i5 + 4 >> 2];
 d24 = d28 * d25 + d23 * d36;
 d25 = d28 * d36 - d23 * d25;
 d23 = +d24;
 d36 = +d25;
 i5 = i4;
 HEAPF32[i5 >> 2] = d23;
 HEAPF32[i5 + 4 >> 2] = d36;
 i5 = i12 + 140 | 0;
 HEAPF32[i5 >> 2] = d37;
 HEAPF32[i5 + 4 >> 2] = d26;
 i5 = i12 + 144 | 0;
 d26 = +HEAPF32[i8 + 12 >> 2];
 i7 = i12 + 140 | 0;
 d37 = +HEAPF32[i8 + 16 >> 2];
 d24 = d24 + (d27 * d26 - d22 * d37);
 i6 = i12 + 136 | 0;
 d25 = d26 * d22 + d27 * d37 + d25;
 d37 = +d24;
 d27 = +d25;
 i34 = i12 + 148 | 0;
 HEAPF32[i34 >> 2] = d37;
 HEAPF32[i34 + 4 >> 2] = d27;
 i34 = i16 + 28 | 0;
 i29 = HEAP32[i34 >> 2] | 0;
 i34 = HEAP32[i34 + 4 >> 2] | 0;
 i14 = i12 + 156 | 0;
 HEAP32[i14 >> 2] = i29;
 HEAP32[i14 + 4 >> 2] = i34;
 i14 = i12 + 164 | 0;
 i17 = i16 + 12 | 0;
 i40 = HEAP32[i17 >> 2] | 0;
 i17 = HEAP32[i17 + 4 >> 2] | 0;
 i13 = i14;
 HEAP32[i13 >> 2] = i40;
 HEAP32[i13 + 4 >> 2] = i17;
 i13 = i12 + 172 | 0;
 i20 = i16 + 20 | 0;
 i41 = HEAP32[i20 >> 2] | 0;
 i20 = HEAP32[i20 + 4 >> 2] | 0;
 i38 = i13;
 HEAP32[i38 >> 2] = i41;
 HEAP32[i38 + 4 >> 2] = i20;
 i38 = i16 + 36 | 0;
 i35 = HEAP32[i38 >> 2] | 0;
 i38 = HEAP32[i38 + 4 >> 2] | 0;
 i19 = i12 + 180 | 0;
 HEAP32[i19 >> 2] = i35;
 HEAP32[i19 + 4 >> 2] = i38;
 i19 = (HEAP8[i16 + 44 | 0] | 0) != 0;
 i21 = (HEAP8[i16 + 45 | 0] | 0) != 0;
 d27 = (HEAP32[tempDoublePtr >> 2] = i41, +HEAPF32[tempDoublePtr >> 2]);
 d37 = (HEAP32[tempDoublePtr >> 2] = i40, +HEAPF32[tempDoublePtr >> 2]);
 d22 = d27 - d37;
 d26 = (HEAP32[tempDoublePtr >> 2] = i20, +HEAPF32[tempDoublePtr >> 2]);
 i20 = i12 + 168 | 0;
 d36 = (HEAP32[tempDoublePtr >> 2] = i17, +HEAPF32[tempDoublePtr >> 2]);
 d23 = d26 - d36;
 d28 = +Math_sqrt(+(d22 * d22 + d23 * d23));
 d33 = (HEAP32[tempDoublePtr >> 2] = i29, +HEAPF32[tempDoublePtr >> 2]);
 d32 = (HEAP32[tempDoublePtr >> 2] = i34, +HEAPF32[tempDoublePtr >> 2]);
 d31 = (HEAP32[tempDoublePtr >> 2] = i35, +HEAPF32[tempDoublePtr >> 2]);
 d30 = (HEAP32[tempDoublePtr >> 2] = i38, +HEAPF32[tempDoublePtr >> 2]);
 if (!(d28 < 1.1920928955078125e-7)) {
  d39 = 1.0 / d28;
  d22 = d22 * d39;
  d23 = d23 * d39;
 }
 i16 = i12 + 196 | 0;
 d28 = -d22;
 HEAPF32[i16 >> 2] = d23;
 i17 = i12 + 200 | 0;
 HEAPF32[i17 >> 2] = d28;
 d28 = (d24 - d37) * d23 + (d25 - d36) * d28;
 if (i19) {
  d37 = d37 - d33;
  d36 = d36 - d32;
  d39 = +Math_sqrt(+(d37 * d37 + d36 * d36));
  if (!(d39 < 1.1920928955078125e-7)) {
   d39 = 1.0 / d39;
   d37 = d37 * d39;
   d36 = d36 * d39;
  }
  d39 = -d37;
  HEAPF32[i12 + 188 >> 2] = d36;
  HEAPF32[i12 + 192 >> 2] = d39;
  i29 = d23 * d37 - d22 * d36 >= 0.0;
  d32 = (d24 - d33) * d36 + (d25 - d32) * d39;
 } else {
  i29 = 0;
  d32 = 0.0;
 }
 L10 : do {
  if (!i21) {
   if (!i19) {
    i41 = d28 >= 0.0;
    HEAP8[i12 + 248 | 0] = i41 & 1;
    i19 = i12 + 212 | 0;
    if (i41) {
     i15 = 64;
     break;
    } else {
     i15 = 65;
     break;
    }
   }
   i19 = d32 >= 0.0;
   if (i29) {
    if (!i19) {
     i41 = d28 >= 0.0;
     HEAP8[i12 + 248 | 0] = i41 & 1;
     i19 = i12 + 212 | 0;
     if (!i41) {
      d37 = +-d23;
      d39 = +d22;
      i38 = i19;
      HEAPF32[i38 >> 2] = d37;
      HEAPF32[i38 + 4 >> 2] = d39;
      i38 = i16;
      i40 = HEAP32[i38 >> 2] | 0;
      i38 = HEAP32[i38 + 4 >> 2] | 0;
      i41 = i12 + 228 | 0;
      HEAP32[i41 >> 2] = i40;
      HEAP32[i41 + 4 >> 2] = i38;
      i41 = i12 + 236 | 0;
      HEAPF32[i41 >> 2] = -(HEAP32[tempDoublePtr >> 2] = i40, +HEAPF32[tempDoublePtr >> 2]);
      HEAPF32[i41 + 4 >> 2] = d39;
      break;
     }
    } else {
     HEAP8[i12 + 248 | 0] = 1;
     i19 = i12 + 212 | 0;
    }
    i41 = i16;
    i40 = HEAP32[i41 + 4 >> 2] | 0;
    i38 = i19;
    HEAP32[i38 >> 2] = HEAP32[i41 >> 2];
    HEAP32[i38 + 4 >> 2] = i40;
    i38 = i12 + 188 | 0;
    i40 = HEAP32[i38 + 4 >> 2] | 0;
    i41 = i12 + 228 | 0;
    HEAP32[i41 >> 2] = HEAP32[i38 >> 2];
    HEAP32[i41 + 4 >> 2] = i40;
    d37 = +-+HEAPF32[i16 >> 2];
    d39 = +-+HEAPF32[i17 >> 2];
    i41 = i12 + 236 | 0;
    HEAPF32[i41 >> 2] = d37;
    HEAPF32[i41 + 4 >> 2] = d39;
    break;
   } else {
    if (i19) {
     i41 = d28 >= 0.0;
     HEAP8[i12 + 248 | 0] = i41 & 1;
     i19 = i12 + 212 | 0;
     if (i41) {
      i38 = i16;
      i41 = HEAP32[i38 >> 2] | 0;
      i38 = HEAP32[i38 + 4 >> 2] | 0;
      i40 = i19;
      HEAP32[i40 >> 2] = i41;
      HEAP32[i40 + 4 >> 2] = i38;
      i40 = i12 + 228 | 0;
      HEAP32[i40 >> 2] = i41;
      HEAP32[i40 + 4 >> 2] = i38;
      d37 = +-(HEAP32[tempDoublePtr >> 2] = i41, +HEAPF32[tempDoublePtr >> 2]);
      d39 = +d22;
      i41 = i12 + 236 | 0;
      HEAPF32[i41 >> 2] = d37;
      HEAPF32[i41 + 4 >> 2] = d39;
      break;
     }
    } else {
     HEAP8[i12 + 248 | 0] = 0;
     i19 = i12 + 212 | 0;
    }
    d39 = +-d23;
    d37 = +d22;
    i38 = i19;
    HEAPF32[i38 >> 2] = d39;
    HEAPF32[i38 + 4 >> 2] = d37;
    i38 = i16;
    i40 = HEAP32[i38 + 4 >> 2] | 0;
    i41 = i12 + 228 | 0;
    HEAP32[i41 >> 2] = HEAP32[i38 >> 2];
    HEAP32[i41 + 4 >> 2] = i40;
    d37 = +-+HEAPF32[i12 + 188 >> 2];
    d39 = +-+HEAPF32[i12 + 192 >> 2];
    i41 = i12 + 236 | 0;
    HEAPF32[i41 >> 2] = d37;
    HEAPF32[i41 + 4 >> 2] = d39;
    break;
   }
  } else {
   d33 = d31 - d27;
   d31 = d30 - d26;
   d30 = +Math_sqrt(+(d33 * d33 + d31 * d31));
   if (d30 < 1.1920928955078125e-7) {
    d30 = d33;
   } else {
    d39 = 1.0 / d30;
    d30 = d33 * d39;
    d31 = d31 * d39;
   }
   d39 = -d30;
   i34 = i12 + 204 | 0;
   HEAPF32[i34 >> 2] = d31;
   i35 = i12 + 208 | 0;
   HEAPF32[i35 >> 2] = d39;
   i38 = d22 * d31 - d23 * d30 > 0.0;
   d24 = (d24 - d27) * d31 + (d25 - d26) * d39;
   if (!i19) {
    i19 = d28 >= 0.0;
    if (!i21) {
     HEAP8[i12 + 248 | 0] = i19 & 1;
     i15 = i12 + 212 | 0;
     if (i19) {
      i19 = i15;
      i15 = 64;
      break;
     } else {
      i19 = i15;
      i15 = 65;
      break;
     }
    }
    if (i38) {
     if (!i19) {
      i41 = d24 >= 0.0;
      HEAP8[i12 + 248 | 0] = i41 & 1;
      i19 = i12 + 212 | 0;
      if (!i41) {
       d37 = +-d23;
       d39 = +d22;
       i38 = i19;
       HEAPF32[i38 >> 2] = d37;
       HEAPF32[i38 + 4 >> 2] = d39;
       i38 = i12 + 228 | 0;
       HEAPF32[i38 >> 2] = d37;
       HEAPF32[i38 + 4 >> 2] = d39;
       i38 = i16;
       i40 = HEAP32[i38 + 4 >> 2] | 0;
       i41 = i12 + 236 | 0;
       HEAP32[i41 >> 2] = HEAP32[i38 >> 2];
       HEAP32[i41 + 4 >> 2] = i40;
       break;
      }
     } else {
      HEAP8[i12 + 248 | 0] = 1;
      i19 = i12 + 212 | 0;
     }
     i41 = i16;
     i40 = HEAP32[i41 + 4 >> 2] | 0;
     i38 = i19;
     HEAP32[i38 >> 2] = HEAP32[i41 >> 2];
     HEAP32[i38 + 4 >> 2] = i40;
     d37 = +-+HEAPF32[i16 >> 2];
     d39 = +-+HEAPF32[i17 >> 2];
     i38 = i12 + 228 | 0;
     HEAPF32[i38 >> 2] = d37;
     HEAPF32[i38 + 4 >> 2] = d39;
     i38 = i12 + 204 | 0;
     i40 = HEAP32[i38 + 4 >> 2] | 0;
     i41 = i12 + 236 | 0;
     HEAP32[i41 >> 2] = HEAP32[i38 >> 2];
     HEAP32[i41 + 4 >> 2] = i40;
     break;
    } else {
     if (i19) {
      i41 = d24 >= 0.0;
      HEAP8[i12 + 248 | 0] = i41 & 1;
      i19 = i12 + 212 | 0;
      if (i41) {
       i40 = i16;
       i38 = HEAP32[i40 >> 2] | 0;
       i40 = HEAP32[i40 + 4 >> 2] | 0;
       i41 = i19;
       HEAP32[i41 >> 2] = i38;
       HEAP32[i41 + 4 >> 2] = i40;
       d37 = +-(HEAP32[tempDoublePtr >> 2] = i38, +HEAPF32[tempDoublePtr >> 2]);
       d39 = +d22;
       i41 = i12 + 228 | 0;
       HEAPF32[i41 >> 2] = d37;
       HEAPF32[i41 + 4 >> 2] = d39;
       i41 = i12 + 236 | 0;
       HEAP32[i41 >> 2] = i38;
       HEAP32[i41 + 4 >> 2] = i40;
       break;
      }
     } else {
      HEAP8[i12 + 248 | 0] = 0;
      i19 = i12 + 212 | 0;
     }
     d39 = +-d23;
     d37 = +d22;
     i38 = i19;
     HEAPF32[i38 >> 2] = d39;
     HEAPF32[i38 + 4 >> 2] = d37;
     d37 = +-+HEAPF32[i12 + 204 >> 2];
     d39 = +-+HEAPF32[i12 + 208 >> 2];
     i38 = i12 + 228 | 0;
     HEAPF32[i38 >> 2] = d37;
     HEAPF32[i38 + 4 >> 2] = d39;
     i38 = i16;
     i40 = HEAP32[i38 + 4 >> 2] | 0;
     i41 = i12 + 236 | 0;
     HEAP32[i41 >> 2] = HEAP32[i38 >> 2];
     HEAP32[i41 + 4 >> 2] = i40;
     break;
    }
   }
   if (i29 & i38) {
    if (!(d32 >= 0.0) & !(d28 >= 0.0)) {
     i41 = d24 >= 0.0;
     HEAP8[i12 + 248 | 0] = i41 & 1;
     i19 = i12 + 212 | 0;
     if (!i41) {
      d37 = +-d23;
      d39 = +d22;
      i41 = i19;
      HEAPF32[i41 >> 2] = d37;
      HEAPF32[i41 + 4 >> 2] = d39;
      i41 = i12 + 228 | 0;
      HEAPF32[i41 >> 2] = d37;
      HEAPF32[i41 + 4 >> 2] = d39;
      i41 = i12 + 236 | 0;
      HEAPF32[i41 >> 2] = d37;
      HEAPF32[i41 + 4 >> 2] = d39;
      break;
     }
    } else {
     HEAP8[i12 + 248 | 0] = 1;
     i19 = i12 + 212 | 0;
    }
    i38 = i16;
    i40 = HEAP32[i38 + 4 >> 2] | 0;
    i41 = i19;
    HEAP32[i41 >> 2] = HEAP32[i38 >> 2];
    HEAP32[i41 + 4 >> 2] = i40;
    i41 = i12 + 188 | 0;
    i40 = HEAP32[i41 + 4 >> 2] | 0;
    i38 = i12 + 228 | 0;
    HEAP32[i38 >> 2] = HEAP32[i41 >> 2];
    HEAP32[i38 + 4 >> 2] = i40;
    i38 = i12 + 204 | 0;
    i40 = HEAP32[i38 + 4 >> 2] | 0;
    i41 = i12 + 236 | 0;
    HEAP32[i41 >> 2] = HEAP32[i38 >> 2];
    HEAP32[i41 + 4 >> 2] = i40;
    break;
   }
   if (i29) {
    do {
     if (!(d32 >= 0.0)) {
      if (d28 >= 0.0) {
       i41 = d24 >= 0.0;
       HEAP8[i12 + 248 | 0] = i41 & 1;
       i19 = i12 + 212 | 0;
       if (i41) {
        break;
       }
      } else {
       HEAP8[i12 + 248 | 0] = 0;
       i19 = i12 + 212 | 0;
      }
      d37 = +-d23;
      d39 = +d22;
      i41 = i19;
      HEAPF32[i41 >> 2] = d37;
      HEAPF32[i41 + 4 >> 2] = d39;
      d39 = +-+HEAPF32[i34 >> 2];
      d37 = +-+HEAPF32[i35 >> 2];
      i41 = i12 + 228 | 0;
      HEAPF32[i41 >> 2] = d39;
      HEAPF32[i41 + 4 >> 2] = d37;
      d37 = +-+HEAPF32[i16 >> 2];
      d39 = +-+HEAPF32[i17 >> 2];
      i41 = i12 + 236 | 0;
      HEAPF32[i41 >> 2] = d37;
      HEAPF32[i41 + 4 >> 2] = d39;
      break L10;
     } else {
      HEAP8[i12 + 248 | 0] = 1;
      i19 = i12 + 212 | 0;
     }
    } while (0);
    i38 = i16;
    i40 = HEAP32[i38 + 4 >> 2] | 0;
    i41 = i19;
    HEAP32[i41 >> 2] = HEAP32[i38 >> 2];
    HEAP32[i41 + 4 >> 2] = i40;
    i41 = i12 + 188 | 0;
    i40 = HEAP32[i41 + 4 >> 2] | 0;
    i38 = i12 + 228 | 0;
    HEAP32[i38 >> 2] = HEAP32[i41 >> 2];
    HEAP32[i38 + 4 >> 2] = i40;
    i38 = i16;
    i40 = HEAP32[i38 + 4 >> 2] | 0;
    i41 = i12 + 236 | 0;
    HEAP32[i41 >> 2] = HEAP32[i38 >> 2];
    HEAP32[i41 + 4 >> 2] = i40;
    break;
   }
   if (!i38) {
    if (!(!(d32 >= 0.0) | !(d28 >= 0.0))) {
     i41 = d24 >= 0.0;
     HEAP8[i12 + 248 | 0] = i41 & 1;
     i19 = i12 + 212 | 0;
     if (i41) {
      i40 = i16;
      i38 = HEAP32[i40 >> 2] | 0;
      i40 = HEAP32[i40 + 4 >> 2] | 0;
      i41 = i19;
      HEAP32[i41 >> 2] = i38;
      HEAP32[i41 + 4 >> 2] = i40;
      i41 = i12 + 228 | 0;
      HEAP32[i41 >> 2] = i38;
      HEAP32[i41 + 4 >> 2] = i40;
      i41 = i12 + 236 | 0;
      HEAP32[i41 >> 2] = i38;
      HEAP32[i41 + 4 >> 2] = i40;
      break;
     }
    } else {
     HEAP8[i12 + 248 | 0] = 0;
     i19 = i12 + 212 | 0;
    }
    d37 = +-d23;
    d39 = +d22;
    i41 = i19;
    HEAPF32[i41 >> 2] = d37;
    HEAPF32[i41 + 4 >> 2] = d39;
    d39 = +-+HEAPF32[i34 >> 2];
    d37 = +-+HEAPF32[i35 >> 2];
    i41 = i12 + 228 | 0;
    HEAPF32[i41 >> 2] = d39;
    HEAPF32[i41 + 4 >> 2] = d37;
    d37 = +-+HEAPF32[i12 + 188 >> 2];
    d39 = +-+HEAPF32[i12 + 192 >> 2];
    i41 = i12 + 236 | 0;
    HEAPF32[i41 >> 2] = d37;
    HEAPF32[i41 + 4 >> 2] = d39;
    break;
   }
   do {
    if (!(d24 >= 0.0)) {
     if (d32 >= 0.0) {
      i41 = d28 >= 0.0;
      HEAP8[i12 + 248 | 0] = i41 & 1;
      i19 = i12 + 212 | 0;
      if (i41) {
       break;
      }
     } else {
      HEAP8[i12 + 248 | 0] = 0;
      i19 = i12 + 212 | 0;
     }
     d37 = +-d23;
     d39 = +d22;
     i41 = i19;
     HEAPF32[i41 >> 2] = d37;
     HEAPF32[i41 + 4 >> 2] = d39;
     d39 = +-+HEAPF32[i16 >> 2];
     d37 = +-+HEAPF32[i17 >> 2];
     i41 = i12 + 228 | 0;
     HEAPF32[i41 >> 2] = d39;
     HEAPF32[i41 + 4 >> 2] = d37;
     d37 = +-+HEAPF32[i12 + 188 >> 2];
     d39 = +-+HEAPF32[i12 + 192 >> 2];
     i41 = i12 + 236 | 0;
     HEAPF32[i41 >> 2] = d37;
     HEAPF32[i41 + 4 >> 2] = d39;
     break L10;
    } else {
     HEAP8[i12 + 248 | 0] = 1;
     i19 = i12 + 212 | 0;
    }
   } while (0);
   i38 = i16;
   i40 = HEAP32[i38 + 4 >> 2] | 0;
   i41 = i19;
   HEAP32[i41 >> 2] = HEAP32[i38 >> 2];
   HEAP32[i41 + 4 >> 2] = i40;
   i41 = i16;
   i40 = HEAP32[i41 + 4 >> 2] | 0;
   i38 = i12 + 228 | 0;
   HEAP32[i38 >> 2] = HEAP32[i41 >> 2];
   HEAP32[i38 + 4 >> 2] = i40;
   i38 = i12 + 204 | 0;
   i40 = HEAP32[i38 + 4 >> 2] | 0;
   i41 = i12 + 236 | 0;
   HEAP32[i41 >> 2] = HEAP32[i38 >> 2];
   HEAP32[i41 + 4 >> 2] = i40;
  }
 } while (0);
 if ((i15 | 0) == 64) {
  i38 = i16;
  i41 = HEAP32[i38 >> 2] | 0;
  i38 = HEAP32[i38 + 4 >> 2] | 0;
  i40 = i19;
  HEAP32[i40 >> 2] = i41;
  HEAP32[i40 + 4 >> 2] = i38;
  d37 = +-(HEAP32[tempDoublePtr >> 2] = i41, +HEAPF32[tempDoublePtr >> 2]);
  d39 = +d22;
  i41 = i12 + 228 | 0;
  HEAPF32[i41 >> 2] = d37;
  HEAPF32[i41 + 4 >> 2] = d39;
  i41 = i12 + 236 | 0;
  HEAPF32[i41 >> 2] = d37;
  HEAPF32[i41 + 4 >> 2] = d39;
 } else if ((i15 | 0) == 65) {
  d37 = +-d23;
  d39 = +d22;
  i40 = i19;
  HEAPF32[i40 >> 2] = d37;
  HEAPF32[i40 + 4 >> 2] = d39;
  i40 = i16;
  i38 = HEAP32[i40 >> 2] | 0;
  i40 = HEAP32[i40 + 4 >> 2] | 0;
  i41 = i12 + 228 | 0;
  HEAP32[i41 >> 2] = i38;
  HEAP32[i41 + 4 >> 2] = i40;
  i41 = i12 + 236 | 0;
  HEAP32[i41 >> 2] = i38;
  HEAP32[i41 + 4 >> 2] = i40;
 }
 i21 = i8 + 148 | 0;
 i34 = i12 + 128 | 0;
 HEAP32[i34 >> 2] = HEAP32[i21 >> 2];
 if ((HEAP32[i21 >> 2] | 0) > 0) {
  i19 = 0;
  do {
   d33 = +HEAPF32[i5 >> 2];
   d37 = +HEAPF32[i8 + (i19 << 3) + 20 >> 2];
   d39 = +HEAPF32[i7 >> 2];
   d36 = +HEAPF32[i8 + (i19 << 3) + 24 >> 2];
   d32 = +(+HEAPF32[i4 >> 2] + (d33 * d37 - d39 * d36));
   d36 = +(d37 * d39 + d33 * d36 + +HEAPF32[i6 >> 2]);
   i41 = i12 + (i19 << 3) | 0;
   HEAPF32[i41 >> 2] = d32;
   HEAPF32[i41 + 4 >> 2] = d36;
   d36 = +HEAPF32[i5 >> 2];
   d32 = +HEAPF32[i8 + (i19 << 3) + 84 >> 2];
   d33 = +HEAPF32[i7 >> 2];
   d39 = +HEAPF32[i8 + (i19 << 3) + 88 >> 2];
   d37 = +(d36 * d32 - d33 * d39);
   d39 = +(d32 * d33 + d36 * d39);
   i41 = i12 + (i19 << 3) + 64 | 0;
   HEAPF32[i41 >> 2] = d37;
   HEAPF32[i41 + 4 >> 2] = d39;
   i19 = i19 + 1 | 0;
  } while ((i19 | 0) < (HEAP32[i21 >> 2] | 0));
 }
 i21 = i12 + 244 | 0;
 HEAPF32[i21 >> 2] = .019999999552965164;
 i19 = i2 + 60 | 0;
 HEAP32[i19 >> 2] = 0;
 i29 = i12 + 248 | 0;
 i35 = HEAP32[i34 >> 2] | 0;
 if ((i35 | 0) <= 0) {
  STACKTOP = i1;
  return;
 }
 d23 = +HEAPF32[i12 + 164 >> 2];
 d26 = +HEAPF32[i20 >> 2];
 d24 = +HEAPF32[i12 + 212 >> 2];
 d27 = +HEAPF32[i12 + 216 >> 2];
 d22 = 3.4028234663852886e+38;
 i20 = 0;
 do {
  d25 = d24 * (+HEAPF32[i12 + (i20 << 3) >> 2] - d23) + d27 * (+HEAPF32[i12 + (i20 << 3) + 4 >> 2] - d26);
  d22 = d25 < d22 ? d25 : d22;
  i20 = i20 + 1 | 0;
 } while ((i20 | 0) != (i35 | 0));
 if (d22 > .019999999552965164) {
  STACKTOP = i1;
  return;
 }
 __ZN12b2EPCollider24ComputePolygonSeparationEv(i18, i12);
 i20 = HEAP32[i18 >> 2] | 0;
 if ((i20 | 0) != 0) {
  d23 = +HEAPF32[i18 + 8 >> 2];
  if (d23 > +HEAPF32[i21 >> 2]) {
   STACKTOP = i1;
   return;
  }
  if (d23 > d22 * .9800000190734863 + .0010000000474974513) {
   i18 = HEAP32[i18 + 4 >> 2] | 0;
   i35 = i2 + 56 | 0;
   if ((i20 | 0) == 1) {
    i18 = i11;
    i15 = 77;
   } else {
    HEAP32[i35 >> 2] = 2;
    i40 = i14;
    i41 = HEAP32[i40 + 4 >> 2] | 0;
    i38 = i11;
    HEAP32[i38 >> 2] = HEAP32[i40 >> 2];
    HEAP32[i38 + 4 >> 2] = i41;
    i38 = i11 + 8 | 0;
    HEAP8[i38] = 0;
    i41 = i18 & 255;
    HEAP8[i38 + 1 | 0] = i41;
    HEAP8[i38 + 2 | 0] = 0;
    HEAP8[i38 + 3 | 0] = 1;
    i38 = i13;
    i40 = HEAP32[i38 + 4 >> 2] | 0;
    i13 = i11 + 12 | 0;
    HEAP32[i13 >> 2] = HEAP32[i38 >> 2];
    HEAP32[i13 + 4 >> 2] = i40;
    i13 = i11 + 20 | 0;
    HEAP8[i13] = 0;
    HEAP8[i13 + 1 | 0] = i41;
    HEAP8[i13 + 2 | 0] = 0;
    HEAP8[i13 + 3 | 0] = 1;
    HEAP32[i9 >> 2] = i18;
    i13 = i18 + 1 | 0;
    i16 = (i13 | 0) < (HEAP32[i34 >> 2] | 0) ? i13 : 0;
    HEAP32[i9 + 4 >> 2] = i16;
    i17 = i12 + (i18 << 3) | 0;
    i13 = HEAP32[i17 >> 2] | 0;
    i17 = HEAP32[i17 + 4 >> 2] | 0;
    i29 = i9 + 8 | 0;
    HEAP32[i29 >> 2] = i13;
    HEAP32[i29 + 4 >> 2] = i17;
    i16 = i12 + (i16 << 3) | 0;
    i29 = HEAP32[i16 >> 2] | 0;
    i16 = HEAP32[i16 + 4 >> 2] | 0;
    i20 = i9 + 16 | 0;
    HEAP32[i20 >> 2] = i29;
    HEAP32[i20 + 4 >> 2] = i16;
    i20 = i12 + (i18 << 3) + 64 | 0;
    i12 = HEAP32[i20 >> 2] | 0;
    i20 = HEAP32[i20 + 4 >> 2] | 0;
    i14 = i9 + 24 | 0;
    HEAP32[i14 >> 2] = i12;
    HEAP32[i14 + 4 >> 2] = i20;
    i14 = 0;
   }
  } else {
   i15 = 75;
  }
 } else {
  i15 = 75;
 }
 if ((i15 | 0) == 75) {
  i18 = i11;
  i35 = i2 + 56 | 0;
  i15 = 77;
 }
 do {
  if ((i15 | 0) == 77) {
   HEAP32[i35 >> 2] = 1;
   i15 = HEAP32[i34 >> 2] | 0;
   if ((i15 | 0) > 1) {
    d23 = +HEAPF32[i12 + 216 >> 2];
    d22 = +HEAPF32[i12 + 212 >> 2];
    i34 = 0;
    d24 = d22 * +HEAPF32[i12 + 64 >> 2] + d23 * +HEAPF32[i12 + 68 >> 2];
    i35 = 1;
    while (1) {
     d25 = d22 * +HEAPF32[i12 + (i35 << 3) + 64 >> 2] + d23 * +HEAPF32[i12 + (i35 << 3) + 68 >> 2];
     i20 = d25 < d24;
     i34 = i20 ? i35 : i34;
     i35 = i35 + 1 | 0;
     if ((i35 | 0) < (i15 | 0)) {
      d24 = i20 ? d25 : d24;
     } else {
      break;
     }
    }
   } else {
    i34 = 0;
   }
   i20 = i34 + 1 | 0;
   i40 = (i20 | 0) < (i15 | 0) ? i20 : 0;
   i41 = i12 + (i34 << 3) | 0;
   i38 = HEAP32[i41 + 4 >> 2] | 0;
   i35 = i11;
   HEAP32[i35 >> 2] = HEAP32[i41 >> 2];
   HEAP32[i35 + 4 >> 2] = i38;
   i35 = i11 + 8 | 0;
   HEAP8[i35] = 0;
   HEAP8[i35 + 1 | 0] = i34;
   HEAP8[i35 + 2 | 0] = 1;
   HEAP8[i35 + 3 | 0] = 0;
   i35 = i12 + (i40 << 3) | 0;
   i38 = HEAP32[i35 + 4 >> 2] | 0;
   i41 = i11 + 12 | 0;
   HEAP32[i41 >> 2] = HEAP32[i35 >> 2];
   HEAP32[i41 + 4 >> 2] = i38;
   i41 = i11 + 20 | 0;
   HEAP8[i41] = 0;
   HEAP8[i41 + 1 | 0] = i40;
   HEAP8[i41 + 2 | 0] = 1;
   HEAP8[i41 + 3 | 0] = 0;
   if ((HEAP8[i29] | 0) == 0) {
    HEAP32[i9 >> 2] = 1;
    HEAP32[i9 + 4 >> 2] = 0;
    i11 = i13;
    i13 = HEAP32[i11 >> 2] | 0;
    i11 = HEAP32[i11 + 4 >> 2] | 0;
    i29 = i9 + 8 | 0;
    HEAP32[i29 >> 2] = i13;
    HEAP32[i29 + 4 >> 2] = i11;
    i29 = HEAP32[i14 >> 2] | 0;
    i14 = HEAP32[i14 + 4 >> 2] | 0;
    i12 = i9 + 16 | 0;
    HEAP32[i12 >> 2] = i29;
    HEAP32[i12 + 4 >> 2] = i14;
    i12 = (HEAPF32[tempDoublePtr >> 2] = -+HEAPF32[i16 >> 2], HEAP32[tempDoublePtr >> 2] | 0);
    i20 = (HEAPF32[tempDoublePtr >> 2] = -+HEAPF32[i17 >> 2], HEAP32[tempDoublePtr >> 2] | 0);
    i16 = i9 + 24 | 0;
    HEAP32[i16 >> 2] = i12;
    HEAP32[i16 + 4 >> 2] = i20;
    i16 = i14;
    i17 = i11;
    i11 = i18;
    i18 = 1;
    i14 = 1;
    break;
   } else {
    HEAP32[i9 >> 2] = 0;
    HEAP32[i9 + 4 >> 2] = 1;
    i17 = i14;
    i11 = HEAP32[i17 >> 2] | 0;
    i17 = HEAP32[i17 + 4 >> 2] | 0;
    i29 = i9 + 8 | 0;
    HEAP32[i29 >> 2] = i11;
    HEAP32[i29 + 4 >> 2] = i17;
    i29 = HEAP32[i13 >> 2] | 0;
    i13 = HEAP32[i13 + 4 >> 2] | 0;
    i20 = i9 + 16 | 0;
    HEAP32[i20 >> 2] = i29;
    HEAP32[i20 + 4 >> 2] = i13;
    i20 = i16;
    i12 = HEAP32[i20 >> 2] | 0;
    i20 = HEAP32[i20 + 4 >> 2] | 0;
    i16 = i9 + 24 | 0;
    HEAP32[i16 >> 2] = i12;
    HEAP32[i16 + 4 >> 2] = i20;
    i16 = i13;
    i13 = i11;
    i11 = i18;
    i18 = 0;
    i14 = 1;
    break;
   }
  }
 } while (0);
 d30 = (HEAP32[tempDoublePtr >> 2] = i20, +HEAPF32[tempDoublePtr >> 2]);
 d39 = (HEAP32[tempDoublePtr >> 2] = i12, +HEAPF32[tempDoublePtr >> 2]);
 d31 = (HEAP32[tempDoublePtr >> 2] = i13, +HEAPF32[tempDoublePtr >> 2]);
 d32 = (HEAP32[tempDoublePtr >> 2] = i17, +HEAPF32[tempDoublePtr >> 2]);
 d33 = (HEAP32[tempDoublePtr >> 2] = i29, +HEAPF32[tempDoublePtr >> 2]);
 d37 = (HEAP32[tempDoublePtr >> 2] = i16, +HEAPF32[tempDoublePtr >> 2]);
 i41 = i9 + 32 | 0;
 i16 = i9 + 24 | 0;
 i13 = i9 + 28 | 0;
 d39 = -d39;
 HEAPF32[i41 >> 2] = d30;
 HEAPF32[i9 + 36 >> 2] = d39;
 i20 = i9 + 44 | 0;
 d36 = -d30;
 i17 = i20;
 HEAPF32[i17 >> 2] = d36;
 HEAP32[i17 + 4 >> 2] = i12;
 i17 = i9 + 8 | 0;
 i15 = i9 + 12 | 0;
 d39 = d30 * d31 + d32 * d39;
 HEAPF32[i9 + 40 >> 2] = d39;
 i29 = i9 + 52 | 0;
 HEAPF32[i29 >> 2] = d33 * d36 + (HEAP32[tempDoublePtr >> 2] = i12, +HEAPF32[tempDoublePtr >> 2]) * d37;
 if ((__Z19b2ClipSegmentToLineP12b2ClipVertexPKS_RK6b2Vec2fi(i10, i11, i41, d39, i18) | 0) < 2) {
  STACKTOP = i1;
  return;
 }
 if ((__Z19b2ClipSegmentToLineP12b2ClipVertexPKS_RK6b2Vec2fi(i3, i10, i20, +HEAPF32[i29 >> 2], HEAP32[i9 + 4 >> 2] | 0) | 0) < 2) {
  STACKTOP = i1;
  return;
 }
 i10 = i2 + 40 | 0;
 if (i14) {
  i40 = i16;
  i41 = HEAP32[i40 >> 2] | 0;
  i40 = HEAP32[i40 + 4 >> 2] | 0;
  i35 = i10;
  HEAP32[i35 >> 2] = i41;
  HEAP32[i35 + 4 >> 2] = i40;
  i35 = i17;
  i40 = HEAP32[i35 >> 2] | 0;
  i35 = HEAP32[i35 + 4 >> 2] | 0;
  i38 = i2 + 48 | 0;
  HEAP32[i38 >> 2] = i40;
  HEAP32[i38 + 4 >> 2] = i35;
  d23 = (HEAP32[tempDoublePtr >> 2] = i40, +HEAPF32[tempDoublePtr >> 2]);
  d22 = (HEAP32[tempDoublePtr >> 2] = i41, +HEAPF32[tempDoublePtr >> 2]);
  d24 = +HEAPF32[i15 >> 2];
  d25 = +HEAPF32[i13 >> 2];
  d28 = +HEAPF32[i3 >> 2];
  d27 = +HEAPF32[i3 + 4 >> 2];
  d26 = +HEAPF32[i21 >> 2];
  if (!((d28 - d23) * d22 + (d27 - d24) * d25 <= d26)) {
   d28 = d26;
   i8 = 0;
  } else {
   d37 = d28 - +HEAPF32[i4 >> 2];
   d36 = d27 - +HEAPF32[i6 >> 2];
   d33 = +HEAPF32[i5 >> 2];
   d28 = +HEAPF32[i7 >> 2];
   d39 = +(d37 * d33 + d36 * d28);
   d28 = +(d33 * d36 - d37 * d28);
   i8 = i2;
   HEAPF32[i8 >> 2] = d39;
   HEAPF32[i8 + 4 >> 2] = d28;
   HEAP32[i2 + 16 >> 2] = HEAP32[i3 + 8 >> 2];
   d28 = +HEAPF32[i21 >> 2];
   i8 = 1;
  }
  d26 = +HEAPF32[i3 + 12 >> 2];
  d27 = +HEAPF32[i3 + 16 >> 2];
  if ((d26 - d23) * d22 + (d27 - d24) * d25 <= d28) {
   d36 = d26 - +HEAPF32[i4 >> 2];
   d33 = d27 - +HEAPF32[i6 >> 2];
   d32 = +HEAPF32[i5 >> 2];
   d39 = +HEAPF32[i7 >> 2];
   d37 = +(d36 * d32 + d33 * d39);
   d39 = +(d32 * d33 - d36 * d39);
   i41 = i2 + (i8 * 20 | 0) | 0;
   HEAPF32[i41 >> 2] = d37;
   HEAPF32[i41 + 4 >> 2] = d39;
   HEAP32[i2 + (i8 * 20 | 0) + 16 >> 2] = HEAP32[i3 + 20 >> 2];
   i8 = i8 + 1 | 0;
  }
 } else {
  i38 = HEAP32[i9 >> 2] | 0;
  i35 = i8 + (i38 << 3) + 84 | 0;
  i41 = HEAP32[i35 + 4 >> 2] | 0;
  i40 = i10;
  HEAP32[i40 >> 2] = HEAP32[i35 >> 2];
  HEAP32[i40 + 4 >> 2] = i41;
  i38 = i8 + (i38 << 3) + 20 | 0;
  i40 = HEAP32[i38 + 4 >> 2] | 0;
  i41 = i2 + 48 | 0;
  HEAP32[i41 >> 2] = HEAP32[i38 >> 2];
  HEAP32[i41 + 4 >> 2] = i40;
  d22 = +HEAPF32[i17 >> 2];
  d23 = +HEAPF32[i16 >> 2];
  d24 = +HEAPF32[i15 >> 2];
  d25 = +HEAPF32[i13 >> 2];
  d26 = +HEAPF32[i21 >> 2];
  if (!((+HEAPF32[i3 >> 2] - d22) * d23 + (+HEAPF32[i3 + 4 >> 2] - d24) * d25 <= d26)) {
   i8 = 0;
  } else {
   i40 = i3;
   i8 = HEAP32[i40 + 4 >> 2] | 0;
   i41 = i2;
   HEAP32[i41 >> 2] = HEAP32[i40 >> 2];
   HEAP32[i41 + 4 >> 2] = i8;
   i41 = i3 + 8 | 0;
   i8 = i2 + 16 | 0;
   HEAP8[i8 + 2 | 0] = HEAP8[i41 + 3 | 0] | 0;
   HEAP8[i8 + 3 | 0] = HEAP8[i41 + 2 | 0] | 0;
   HEAP8[i8] = HEAP8[i41 + 1 | 0] | 0;
   HEAP8[i8 + 1 | 0] = HEAP8[i41] | 0;
   d26 = +HEAPF32[i21 >> 2];
   i8 = 1;
  }
  i4 = i3 + 12 | 0;
  if ((+HEAPF32[i4 >> 2] - d22) * d23 + (+HEAPF32[i3 + 16 >> 2] - d24) * d25 <= d26) {
   i38 = i4;
   i41 = HEAP32[i38 + 4 >> 2] | 0;
   i40 = i2 + (i8 * 20 | 0) | 0;
   HEAP32[i40 >> 2] = HEAP32[i38 >> 2];
   HEAP32[i40 + 4 >> 2] = i41;
   i40 = i3 + 20 | 0;
   i41 = i2 + (i8 * 20 | 0) + 16 | 0;
   HEAP8[i41 + 2 | 0] = HEAP8[i40 + 3 | 0] | 0;
   HEAP8[i41 + 3 | 0] = HEAP8[i40 + 2 | 0] | 0;
   HEAP8[i41] = HEAP8[i40 + 1 | 0] | 0;
   HEAP8[i41 + 1 | 0] = HEAP8[i40] | 0;
   i8 = i8 + 1 | 0;
  }
 }
 HEAP32[i19 >> 2] = i8;
 STACKTOP = i1;
 return;
}
function __ZN7b2World8SolveTOIERK10b2TimeStep(i30, i11) {
 i30 = i30 | 0;
 i11 = i11 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, i38 = 0, i39 = 0, i40 = 0, i41 = 0, d42 = 0.0, i43 = 0, i44 = 0, i45 = 0, i46 = 0, i47 = 0, i48 = 0, i49 = 0, i50 = 0, i51 = 0, i52 = 0, i53 = 0, i54 = 0, i55 = 0, i56 = 0, i57 = 0, i58 = 0, i59 = 0, i60 = 0, i61 = 0, i62 = 0, i63 = 0, i64 = 0, i65 = 0, i66 = 0, d67 = 0.0, d68 = 0.0, d69 = 0.0, d70 = 0.0, d71 = 0.0, d72 = 0.0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 336 | 0;
 i3 = i1 + 284 | 0;
 i6 = i1 + 152 | 0;
 i5 = i1 + 144 | 0;
 i4 = i1 + 108 | 0;
 i8 = i1 + 72 | 0;
 i7 = i1 + 64 | 0;
 i14 = i1 + 24 | 0;
 i9 = i1;
 i10 = i30 + 102872 | 0;
 i13 = i30 + 102944 | 0;
 __ZN8b2IslandC2EiiiP16b2StackAllocatorP17b2ContactListener(i3, 64, 32, 0, i30 + 68 | 0, HEAP32[i13 >> 2] | 0);
 i2 = i30 + 102995 | 0;
 if ((HEAP8[i2] | 0) != 0) {
  i15 = HEAP32[i30 + 102952 >> 2] | 0;
  if ((i15 | 0) != 0) {
   do {
    i66 = i15 + 4 | 0;
    HEAP16[i66 >> 1] = HEAP16[i66 >> 1] & 65534;
    HEAPF32[i15 + 60 >> 2] = 0.0;
    i15 = HEAP32[i15 + 96 >> 2] | 0;
   } while ((i15 | 0) != 0);
  }
  i15 = i30 + 102932 | 0;
  i16 = HEAP32[i15 >> 2] | 0;
  if ((i16 | 0) != 0) {
   do {
    i66 = i16 + 4 | 0;
    HEAP32[i66 >> 2] = HEAP32[i66 >> 2] & -34;
    HEAP32[i16 + 128 >> 2] = 0;
    HEAPF32[i16 + 132 >> 2] = 1.0;
    i16 = HEAP32[i16 + 12 >> 2] | 0;
   } while ((i16 | 0) != 0);
  }
 } else {
  i15 = i30 + 102932 | 0;
 }
 i25 = i3 + 28 | 0;
 i26 = i3 + 36 | 0;
 i27 = i3 + 32 | 0;
 i28 = i3 + 40 | 0;
 i29 = i3 + 8 | 0;
 i24 = i3 + 44 | 0;
 i23 = i3 + 12 | 0;
 i22 = i7 + 4 | 0;
 i21 = i9 + 4 | 0;
 i20 = i9 + 8 | 0;
 i19 = i9 + 16 | 0;
 i18 = i11 + 12 | 0;
 i17 = i9 + 12 | 0;
 i16 = i9 + 20 | 0;
 i39 = i30 + 102994 | 0;
 i37 = i6 + 16 | 0;
 i36 = i6 + 20 | 0;
 i35 = i6 + 24 | 0;
 i34 = i6 + 44 | 0;
 i33 = i6 + 48 | 0;
 i32 = i6 + 52 | 0;
 i41 = i6 + 28 | 0;
 i31 = i6 + 56 | 0;
 i40 = i6 + 92 | 0;
 i30 = i6 + 128 | 0;
 i38 = i5 + 4 | 0;
 L11 : while (1) {
  i47 = HEAP32[i15 >> 2] | 0;
  if ((i47 | 0) == 0) {
   i4 = 36;
   break;
  } else {
   d42 = 1.0;
   i44 = 0;
  }
  do {
   i48 = i47 + 4 | 0;
   i43 = HEAP32[i48 >> 2] | 0;
   do {
    if ((i43 & 4 | 0) != 0 ? (HEAP32[i47 + 128 >> 2] | 0) <= 8 : 0) {
     if ((i43 & 32 | 0) == 0) {
      i43 = HEAP32[i47 + 48 >> 2] | 0;
      i45 = HEAP32[i47 + 52 >> 2] | 0;
      if ((HEAP8[i43 + 38 | 0] | 0) != 0) {
       break;
      }
      if ((HEAP8[i45 + 38 | 0] | 0) != 0) {
       break;
      }
      i46 = HEAP32[i43 + 8 >> 2] | 0;
      i50 = HEAP32[i45 + 8 >> 2] | 0;
      i53 = HEAP32[i46 >> 2] | 0;
      i52 = HEAP32[i50 >> 2] | 0;
      if (!((i53 | 0) == 2 | (i52 | 0) == 2)) {
       i4 = 16;
       break L11;
      }
      i51 = HEAP16[i46 + 4 >> 1] | 0;
      i49 = HEAP16[i50 + 4 >> 1] | 0;
      if (!((i51 & 2) != 0 & (i53 | 0) != 0 | (i49 & 2) != 0 & (i52 | 0) != 0)) {
       break;
      }
      if (!((i51 & 8) != 0 | (i53 | 0) != 2 | ((i49 & 8) != 0 | (i52 | 0) != 2))) {
       break;
      }
      i51 = i46 + 28 | 0;
      i52 = i46 + 60 | 0;
      d68 = +HEAPF32[i52 >> 2];
      i49 = i50 + 28 | 0;
      i53 = i50 + 60 | 0;
      d67 = +HEAPF32[i53 >> 2];
      if (!(d68 < d67)) {
       if (d67 < d68) {
        if (!(d67 < 1.0)) {
         i4 = 25;
         break L11;
        }
        d67 = (d68 - d67) / (1.0 - d67);
        i66 = i50 + 36 | 0;
        d69 = 1.0 - d67;
        d71 = +(+HEAPF32[i66 >> 2] * d69 + d67 * +HEAPF32[i50 + 44 >> 2]);
        d70 = +(d69 * +HEAPF32[i50 + 40 >> 2] + d67 * +HEAPF32[i50 + 48 >> 2]);
        HEAPF32[i66 >> 2] = d71;
        HEAPF32[i66 + 4 >> 2] = d70;
        i66 = i50 + 52 | 0;
        HEAPF32[i66 >> 2] = d69 * +HEAPF32[i66 >> 2] + d67 * +HEAPF32[i50 + 56 >> 2];
        HEAPF32[i53 >> 2] = d68;
        d67 = d68;
       } else {
        d67 = d68;
       }
      } else {
       if (!(d68 < 1.0)) {
        i4 = 21;
        break L11;
       }
       d71 = (d67 - d68) / (1.0 - d68);
       i66 = i46 + 36 | 0;
       d70 = 1.0 - d71;
       d68 = +(+HEAPF32[i66 >> 2] * d70 + d71 * +HEAPF32[i46 + 44 >> 2]);
       d69 = +(d70 * +HEAPF32[i46 + 40 >> 2] + d71 * +HEAPF32[i46 + 48 >> 2]);
       HEAPF32[i66 >> 2] = d68;
       HEAPF32[i66 + 4 >> 2] = d69;
       i66 = i46 + 52 | 0;
       HEAPF32[i66 >> 2] = d70 * +HEAPF32[i66 >> 2] + d71 * +HEAPF32[i46 + 56 >> 2];
       HEAPF32[i52 >> 2] = d67;
      }
      if (!(d67 < 1.0)) {
       i4 = 28;
       break L11;
      }
      i66 = HEAP32[i47 + 56 >> 2] | 0;
      i46 = HEAP32[i47 + 60 >> 2] | 0;
      HEAP32[i37 >> 2] = 0;
      HEAP32[i36 >> 2] = 0;
      HEAPF32[i35 >> 2] = 0.0;
      HEAP32[i34 >> 2] = 0;
      HEAP32[i33 >> 2] = 0;
      HEAPF32[i32 >> 2] = 0.0;
      __ZN15b2DistanceProxy3SetEPK7b2Shapei(i6, HEAP32[i43 + 12 >> 2] | 0, i66);
      __ZN15b2DistanceProxy3SetEPK7b2Shapei(i41, HEAP32[i45 + 12 >> 2] | 0, i46);
      i43 = i31 + 0 | 0;
      i45 = i51 + 0 | 0;
      i46 = i43 + 36 | 0;
      do {
       HEAP32[i43 >> 2] = HEAP32[i45 >> 2];
       i43 = i43 + 4 | 0;
       i45 = i45 + 4 | 0;
      } while ((i43 | 0) < (i46 | 0));
      i43 = i40 + 0 | 0;
      i45 = i49 + 0 | 0;
      i46 = i43 + 36 | 0;
      do {
       HEAP32[i43 >> 2] = HEAP32[i45 >> 2];
       i43 = i43 + 4 | 0;
       i45 = i45 + 4 | 0;
      } while ((i43 | 0) < (i46 | 0));
      HEAPF32[i30 >> 2] = 1.0;
      __Z14b2TimeOfImpactP11b2TOIOutputPK10b2TOIInput(i5, i6);
      if ((HEAP32[i5 >> 2] | 0) == 3) {
       d67 = d67 + (1.0 - d67) * +HEAPF32[i38 >> 2];
       d67 = d67 < 1.0 ? d67 : 1.0;
      } else {
       d67 = 1.0;
      }
      HEAPF32[i47 + 132 >> 2] = d67;
      HEAP32[i48 >> 2] = HEAP32[i48 >> 2] | 32;
     } else {
      d67 = +HEAPF32[i47 + 132 >> 2];
     }
     if (d67 < d42) {
      d42 = d67;
      i44 = i47;
     }
    }
   } while (0);
   i47 = HEAP32[i47 + 12 >> 2] | 0;
  } while ((i47 | 0) != 0);
  if ((i44 | 0) == 0 | d42 > .9999988079071045) {
   i4 = 36;
   break;
  }
  i47 = HEAP32[(HEAP32[i44 + 48 >> 2] | 0) + 8 >> 2] | 0;
  i48 = HEAP32[(HEAP32[i44 + 52 >> 2] | 0) + 8 >> 2] | 0;
  i49 = i47 + 28 | 0;
  i43 = i4 + 0 | 0;
  i45 = i49 + 0 | 0;
  i46 = i43 + 36 | 0;
  do {
   HEAP32[i43 >> 2] = HEAP32[i45 >> 2];
   i43 = i43 + 4 | 0;
   i45 = i45 + 4 | 0;
  } while ((i43 | 0) < (i46 | 0));
  i50 = i48 + 28 | 0;
  i43 = i8 + 0 | 0;
  i45 = i50 + 0 | 0;
  i46 = i43 + 36 | 0;
  do {
   HEAP32[i43 >> 2] = HEAP32[i45 >> 2];
   i43 = i43 + 4 | 0;
   i45 = i45 + 4 | 0;
  } while ((i43 | 0) < (i46 | 0));
  i43 = i47 + 60 | 0;
  d67 = +HEAPF32[i43 >> 2];
  if (!(d67 < 1.0)) {
   i4 = 38;
   break;
  }
  d70 = (d42 - d67) / (1.0 - d67);
  i57 = i47 + 36 | 0;
  d67 = 1.0 - d70;
  i52 = i47 + 44 | 0;
  i53 = i47 + 48 | 0;
  d71 = +HEAPF32[i57 >> 2] * d67 + d70 * +HEAPF32[i52 >> 2];
  d72 = d67 * +HEAPF32[i47 + 40 >> 2] + d70 * +HEAPF32[i53 >> 2];
  d69 = +d71;
  d68 = +d72;
  HEAPF32[i57 >> 2] = d69;
  HEAPF32[i57 + 4 >> 2] = d68;
  i57 = i47 + 52 | 0;
  i51 = i47 + 56 | 0;
  d70 = d67 * +HEAPF32[i57 >> 2] + d70 * +HEAPF32[i51 >> 2];
  HEAPF32[i57 >> 2] = d70;
  HEAPF32[i43 >> 2] = d42;
  i57 = i47 + 44 | 0;
  HEAPF32[i57 >> 2] = d69;
  HEAPF32[i57 + 4 >> 2] = d68;
  HEAPF32[i51 >> 2] = d70;
  d68 = +Math_sin(+d70);
  i57 = i47 + 20 | 0;
  HEAPF32[i57 >> 2] = d68;
  d70 = +Math_cos(+d70);
  i56 = i47 + 24 | 0;
  HEAPF32[i56 >> 2] = d70;
  i58 = i47 + 12 | 0;
  i55 = i47 + 28 | 0;
  d69 = +HEAPF32[i55 >> 2];
  i54 = i47 + 32 | 0;
  d67 = +HEAPF32[i54 >> 2];
  d71 = +(d71 - (d70 * d69 - d68 * d67));
  d67 = +(d72 - (d68 * d69 + d70 * d67));
  i43 = i58;
  HEAPF32[i43 >> 2] = d71;
  HEAPF32[i43 + 4 >> 2] = d67;
  i43 = i48 + 60 | 0;
  d67 = +HEAPF32[i43 >> 2];
  if (!(d67 < 1.0)) {
   i4 = 40;
   break;
  }
  d70 = (d42 - d67) / (1.0 - d67);
  i64 = i48 + 36 | 0;
  d72 = 1.0 - d70;
  i61 = i48 + 44 | 0;
  i60 = i48 + 48 | 0;
  d71 = +HEAPF32[i64 >> 2] * d72 + d70 * +HEAPF32[i61 >> 2];
  d67 = d72 * +HEAPF32[i48 + 40 >> 2] + d70 * +HEAPF32[i60 >> 2];
  d69 = +d71;
  d68 = +d67;
  HEAPF32[i64 >> 2] = d69;
  HEAPF32[i64 + 4 >> 2] = d68;
  i64 = i48 + 52 | 0;
  i59 = i48 + 56 | 0;
  d70 = d72 * +HEAPF32[i64 >> 2] + d70 * +HEAPF32[i59 >> 2];
  HEAPF32[i64 >> 2] = d70;
  HEAPF32[i43 >> 2] = d42;
  i64 = i48 + 44 | 0;
  HEAPF32[i64 >> 2] = d69;
  HEAPF32[i64 + 4 >> 2] = d68;
  HEAPF32[i59 >> 2] = d70;
  d68 = +Math_sin(+d70);
  i64 = i48 + 20 | 0;
  HEAPF32[i64 >> 2] = d68;
  d70 = +Math_cos(+d70);
  i63 = i48 + 24 | 0;
  HEAPF32[i63 >> 2] = d70;
  i65 = i48 + 12 | 0;
  i62 = i48 + 28 | 0;
  d69 = +HEAPF32[i62 >> 2];
  i66 = i48 + 32 | 0;
  d72 = +HEAPF32[i66 >> 2];
  d71 = +(d71 - (d70 * d69 - d68 * d72));
  d72 = +(d67 - (d68 * d69 + d70 * d72));
  i43 = i65;
  HEAPF32[i43 >> 2] = d71;
  HEAPF32[i43 + 4 >> 2] = d72;
  __ZN9b2Contact6UpdateEP17b2ContactListener(i44, HEAP32[i13 >> 2] | 0);
  i43 = i44 + 4 | 0;
  i45 = HEAP32[i43 >> 2] | 0;
  HEAP32[i43 >> 2] = i45 & -33;
  i46 = i44 + 128 | 0;
  HEAP32[i46 >> 2] = (HEAP32[i46 >> 2] | 0) + 1;
  if ((i45 & 6 | 0) != 6) {
   HEAP32[i43 >> 2] = i45 & -37;
   i43 = i49 + 0 | 0;
   i45 = i4 + 0 | 0;
   i46 = i43 + 36 | 0;
   do {
    HEAP32[i43 >> 2] = HEAP32[i45 >> 2];
    i43 = i43 + 4 | 0;
    i45 = i45 + 4 | 0;
   } while ((i43 | 0) < (i46 | 0));
   i43 = i50 + 0 | 0;
   i45 = i8 + 0 | 0;
   i46 = i43 + 36 | 0;
   do {
    HEAP32[i43 >> 2] = HEAP32[i45 >> 2];
    i43 = i43 + 4 | 0;
    i45 = i45 + 4 | 0;
   } while ((i43 | 0) < (i46 | 0));
   d69 = +HEAPF32[i51 >> 2];
   d71 = +Math_sin(+d69);
   HEAPF32[i57 >> 2] = d71;
   d69 = +Math_cos(+d69);
   HEAPF32[i56 >> 2] = d69;
   d72 = +HEAPF32[i55 >> 2];
   d70 = +HEAPF32[i54 >> 2];
   d68 = +(+HEAPF32[i52 >> 2] - (d69 * d72 - d71 * d70));
   d70 = +(+HEAPF32[i53 >> 2] - (d71 * d72 + d69 * d70));
   HEAPF32[i58 >> 2] = d68;
   HEAPF32[i58 + 4 >> 2] = d70;
   d70 = +HEAPF32[i59 >> 2];
   d68 = +Math_sin(+d70);
   HEAPF32[i64 >> 2] = d68;
   d70 = +Math_cos(+d70);
   HEAPF32[i63 >> 2] = d70;
   d69 = +HEAPF32[i62 >> 2];
   d72 = +HEAPF32[i66 >> 2];
   d71 = +(+HEAPF32[i61 >> 2] - (d70 * d69 - d68 * d72));
   d72 = +(+HEAPF32[i60 >> 2] - (d68 * d69 + d70 * d72));
   i66 = i65;
   HEAPF32[i66 >> 2] = d71;
   HEAPF32[i66 + 4 >> 2] = d72;
   continue;
  }
  i45 = i47 + 4 | 0;
  i46 = HEAPU16[i45 >> 1] | 0;
  if ((i46 & 2 | 0) == 0) {
   HEAP16[i45 >> 1] = i46 | 2;
   HEAPF32[i47 + 144 >> 2] = 0.0;
  }
  i46 = i48 + 4 | 0;
  i49 = HEAPU16[i46 >> 1] | 0;
  if ((i49 & 2 | 0) == 0) {
   HEAP16[i46 >> 1] = i49 | 2;
   HEAPF32[i48 + 144 >> 2] = 0.0;
  }
  HEAP32[i25 >> 2] = 0;
  HEAP32[i26 >> 2] = 0;
  HEAP32[i27 >> 2] = 0;
  if ((HEAP32[i28 >> 2] | 0) <= 0) {
   i4 = 48;
   break;
  }
  i49 = i47 + 8 | 0;
  HEAP32[i49 >> 2] = 0;
  i51 = HEAP32[i25 >> 2] | 0;
  HEAP32[(HEAP32[i29 >> 2] | 0) + (i51 << 2) >> 2] = i47;
  i51 = i51 + 1 | 0;
  HEAP32[i25 >> 2] = i51;
  if ((i51 | 0) >= (HEAP32[i28 >> 2] | 0)) {
   i4 = 50;
   break;
  }
  i50 = i48 + 8 | 0;
  HEAP32[i50 >> 2] = i51;
  i51 = HEAP32[i25 >> 2] | 0;
  HEAP32[(HEAP32[i29 >> 2] | 0) + (i51 << 2) >> 2] = i48;
  HEAP32[i25 >> 2] = i51 + 1;
  i51 = HEAP32[i26 >> 2] | 0;
  if ((i51 | 0) >= (HEAP32[i24 >> 2] | 0)) {
   i4 = 52;
   break;
  }
  HEAP32[i26 >> 2] = i51 + 1;
  HEAP32[(HEAP32[i23 >> 2] | 0) + (i51 << 2) >> 2] = i44;
  HEAP16[i45 >> 1] = HEAPU16[i45 >> 1] | 1;
  HEAP16[i46 >> 1] = HEAPU16[i46 >> 1] | 1;
  HEAP32[i43 >> 2] = HEAP32[i43 >> 2] | 1;
  HEAP32[i7 >> 2] = i47;
  HEAP32[i22 >> 2] = i48;
  i44 = 1;
  while (1) {
   L58 : do {
    if ((HEAP32[i47 >> 2] | 0) == 2 ? (i12 = HEAP32[i47 + 112 >> 2] | 0, (i12 | 0) != 0) : 0) {
     i47 = i47 + 4 | 0;
     i51 = i12;
     do {
      if ((HEAP32[i25 >> 2] | 0) == (HEAP32[i28 >> 2] | 0)) {
       break L58;
      }
      if ((HEAP32[i26 >> 2] | 0) == (HEAP32[i24 >> 2] | 0)) {
       break L58;
      }
      i52 = HEAP32[i51 + 4 >> 2] | 0;
      i53 = i52 + 4 | 0;
      do {
       if ((HEAP32[i53 >> 2] & 1 | 0) == 0) {
        i48 = HEAP32[i51 >> 2] | 0;
        if (((HEAP32[i48 >> 2] | 0) == 2 ? (HEAP16[i47 >> 1] & 8) == 0 : 0) ? (HEAP16[i48 + 4 >> 1] & 8) == 0 : 0) {
         break;
        }
        if ((HEAP8[(HEAP32[i52 + 48 >> 2] | 0) + 38 | 0] | 0) == 0 ? (HEAP8[(HEAP32[i52 + 52 >> 2] | 0) + 38 | 0] | 0) == 0 : 0) {
         i54 = i48 + 28 | 0;
         i43 = i14 + 0 | 0;
         i45 = i54 + 0 | 0;
         i46 = i43 + 36 | 0;
         do {
          HEAP32[i43 >> 2] = HEAP32[i45 >> 2];
          i43 = i43 + 4 | 0;
          i45 = i45 + 4 | 0;
         } while ((i43 | 0) < (i46 | 0));
         i43 = i48 + 4 | 0;
         if ((HEAP16[i43 >> 1] & 1) == 0) {
          i45 = i48 + 60 | 0;
          d67 = +HEAPF32[i45 >> 2];
          if (!(d67 < 1.0)) {
           i4 = 67;
           break L11;
          }
          d70 = (d42 - d67) / (1.0 - d67);
          i65 = i48 + 36 | 0;
          d72 = 1.0 - d70;
          d71 = +HEAPF32[i65 >> 2] * d72 + d70 * +HEAPF32[i48 + 44 >> 2];
          d67 = d72 * +HEAPF32[i48 + 40 >> 2] + d70 * +HEAPF32[i48 + 48 >> 2];
          d69 = +d71;
          d68 = +d67;
          HEAPF32[i65 >> 2] = d69;
          HEAPF32[i65 + 4 >> 2] = d68;
          i65 = i48 + 52 | 0;
          i66 = i48 + 56 | 0;
          d70 = d72 * +HEAPF32[i65 >> 2] + d70 * +HEAPF32[i66 >> 2];
          HEAPF32[i65 >> 2] = d70;
          HEAPF32[i45 >> 2] = d42;
          i65 = i48 + 44 | 0;
          HEAPF32[i65 >> 2] = d69;
          HEAPF32[i65 + 4 >> 2] = d68;
          HEAPF32[i66 >> 2] = d70;
          d68 = +Math_sin(+d70);
          HEAPF32[i48 + 20 >> 2] = d68;
          d70 = +Math_cos(+d70);
          HEAPF32[i48 + 24 >> 2] = d70;
          d69 = +HEAPF32[i48 + 28 >> 2];
          d72 = +HEAPF32[i48 + 32 >> 2];
          d71 = +(d71 - (d70 * d69 - d68 * d72));
          d72 = +(d67 - (d68 * d69 + d70 * d72));
          i66 = i48 + 12 | 0;
          HEAPF32[i66 >> 2] = d71;
          HEAPF32[i66 + 4 >> 2] = d72;
         }
         __ZN9b2Contact6UpdateEP17b2ContactListener(i52, HEAP32[i13 >> 2] | 0);
         i45 = HEAP32[i53 >> 2] | 0;
         if ((i45 & 4 | 0) == 0) {
          i43 = i54 + 0 | 0;
          i45 = i14 + 0 | 0;
          i46 = i43 + 36 | 0;
          do {
           HEAP32[i43 >> 2] = HEAP32[i45 >> 2];
           i43 = i43 + 4 | 0;
           i45 = i45 + 4 | 0;
          } while ((i43 | 0) < (i46 | 0));
          d70 = +HEAPF32[i48 + 56 >> 2];
          d68 = +Math_sin(+d70);
          HEAPF32[i48 + 20 >> 2] = d68;
          d70 = +Math_cos(+d70);
          HEAPF32[i48 + 24 >> 2] = d70;
          d69 = +HEAPF32[i48 + 28 >> 2];
          d72 = +HEAPF32[i48 + 32 >> 2];
          d71 = +(+HEAPF32[i48 + 44 >> 2] - (d70 * d69 - d68 * d72));
          d72 = +(+HEAPF32[i48 + 48 >> 2] - (d68 * d69 + d70 * d72));
          i66 = i48 + 12 | 0;
          HEAPF32[i66 >> 2] = d71;
          HEAPF32[i66 + 4 >> 2] = d72;
          break;
         }
         if ((i45 & 2 | 0) == 0) {
          i43 = i54 + 0 | 0;
          i45 = i14 + 0 | 0;
          i46 = i43 + 36 | 0;
          do {
           HEAP32[i43 >> 2] = HEAP32[i45 >> 2];
           i43 = i43 + 4 | 0;
           i45 = i45 + 4 | 0;
          } while ((i43 | 0) < (i46 | 0));
          d70 = +HEAPF32[i48 + 56 >> 2];
          d68 = +Math_sin(+d70);
          HEAPF32[i48 + 20 >> 2] = d68;
          d70 = +Math_cos(+d70);
          HEAPF32[i48 + 24 >> 2] = d70;
          d69 = +HEAPF32[i48 + 28 >> 2];
          d72 = +HEAPF32[i48 + 32 >> 2];
          d71 = +(+HEAPF32[i48 + 44 >> 2] - (d70 * d69 - d68 * d72));
          d72 = +(+HEAPF32[i48 + 48 >> 2] - (d68 * d69 + d70 * d72));
          i66 = i48 + 12 | 0;
          HEAPF32[i66 >> 2] = d71;
          HEAPF32[i66 + 4 >> 2] = d72;
          break;
         }
         HEAP32[i53 >> 2] = i45 | 1;
         i45 = HEAP32[i26 >> 2] | 0;
         if ((i45 | 0) >= (HEAP32[i24 >> 2] | 0)) {
          i4 = 74;
          break L11;
         }
         HEAP32[i26 >> 2] = i45 + 1;
         HEAP32[(HEAP32[i23 >> 2] | 0) + (i45 << 2) >> 2] = i52;
         i45 = HEAPU16[i43 >> 1] | 0;
         if ((i45 & 1 | 0) == 0) {
          HEAP16[i43 >> 1] = i45 | 1;
          if ((HEAP32[i48 >> 2] | 0) != 0 ? (i45 & 2 | 0) == 0 : 0) {
           HEAP16[i43 >> 1] = i45 | 3;
           HEAPF32[i48 + 144 >> 2] = 0.0;
          }
          i43 = HEAP32[i25 >> 2] | 0;
          if ((i43 | 0) >= (HEAP32[i28 >> 2] | 0)) {
           i4 = 80;
           break L11;
          }
          HEAP32[i48 + 8 >> 2] = i43;
          i66 = HEAP32[i25 >> 2] | 0;
          HEAP32[(HEAP32[i29 >> 2] | 0) + (i66 << 2) >> 2] = i48;
          HEAP32[i25 >> 2] = i66 + 1;
         }
        }
       }
      } while (0);
      i51 = HEAP32[i51 + 12 >> 2] | 0;
     } while ((i51 | 0) != 0);
    }
   } while (0);
   if ((i44 | 0) >= 2) {
    break;
   }
   i47 = HEAP32[i7 + (i44 << 2) >> 2] | 0;
   i44 = i44 + 1 | 0;
  }
  d72 = (1.0 - d42) * +HEAPF32[i11 >> 2];
  HEAPF32[i9 >> 2] = d72;
  HEAPF32[i21 >> 2] = 1.0 / d72;
  HEAPF32[i20 >> 2] = 1.0;
  HEAP32[i19 >> 2] = 20;
  HEAP32[i17 >> 2] = HEAP32[i18 >> 2];
  HEAP8[i16] = 0;
  __ZN8b2Island8SolveTOIERK10b2TimeStepii(i3, i9, HEAP32[i49 >> 2] | 0, HEAP32[i50 >> 2] | 0);
  i44 = HEAP32[i25 >> 2] | 0;
  if ((i44 | 0) > 0) {
   i43 = 0;
   do {
    i45 = HEAP32[(HEAP32[i29 >> 2] | 0) + (i43 << 2) >> 2] | 0;
    i66 = i45 + 4 | 0;
    HEAP16[i66 >> 1] = HEAP16[i66 >> 1] & 65534;
    if ((HEAP32[i45 >> 2] | 0) == 2) {
     __ZN6b2Body19SynchronizeFixturesEv(i45);
     i44 = HEAP32[i45 + 112 >> 2] | 0;
     if ((i44 | 0) != 0) {
      do {
       i66 = (HEAP32[i44 + 4 >> 2] | 0) + 4 | 0;
       HEAP32[i66 >> 2] = HEAP32[i66 >> 2] & -34;
       i44 = HEAP32[i44 + 12 >> 2] | 0;
      } while ((i44 | 0) != 0);
     }
     i44 = HEAP32[i25 >> 2] | 0;
    }
    i43 = i43 + 1 | 0;
   } while ((i43 | 0) < (i44 | 0));
  }
  __ZN16b2ContactManager15FindNewContactsEv(i10);
  if ((HEAP8[i39] | 0) != 0) {
   i4 = 92;
   break;
  }
 }
 if ((i4 | 0) == 16) {
  ___assert_fail(2288, 2184, 641, 2344);
 } else if ((i4 | 0) == 21) {
  ___assert_fail(2360, 2376, 723, 2400);
 } else if ((i4 | 0) == 25) {
  ___assert_fail(2360, 2376, 723, 2400);
 } else if ((i4 | 0) == 28) {
  ___assert_fail(2360, 2184, 676, 2344);
 } else if ((i4 | 0) == 36) {
  HEAP8[i2] = 1;
  __ZN8b2IslandD2Ev(i3);
  STACKTOP = i1;
  return;
 } else if ((i4 | 0) == 38) {
  ___assert_fail(2360, 2376, 723, 2400);
 } else if ((i4 | 0) == 40) {
  ___assert_fail(2360, 2376, 723, 2400);
 } else if ((i4 | 0) == 48) {
  ___assert_fail(2520, 2440, 54, 2472);
 } else if ((i4 | 0) == 50) {
  ___assert_fail(2520, 2440, 54, 2472);
 } else if ((i4 | 0) == 52) {
  ___assert_fail(2480, 2440, 62, 2472);
 } else if ((i4 | 0) == 67) {
  ___assert_fail(2360, 2376, 723, 2400);
 } else if ((i4 | 0) == 74) {
  ___assert_fail(2480, 2440, 62, 2472);
 } else if ((i4 | 0) == 80) {
  ___assert_fail(2520, 2440, 54, 2472);
 } else if ((i4 | 0) == 92) {
  HEAP8[i2] = 0;
  __ZN8b2IslandD2Ev(i3);
  STACKTOP = i1;
  return;
 }
}
function __ZNSt3__16__sortIRPFbRK6b2PairS3_EPS1_EEvT0_S8_T_(i5, i8, i1) {
 i5 = i5 | 0;
 i8 = i8 | 0;
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i6 = 0, i7 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0;
 i3 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 i2 = i3;
 L1 : while (1) {
  i7 = i8;
  i4 = i8 + -12 | 0;
  L3 : while (1) {
   i9 = i5;
   i11 = i7 - i9 | 0;
   switch ((i11 | 0) / 12 | 0 | 0) {
   case 4:
    {
     i6 = 14;
     break L1;
    }
   case 2:
    {
     i6 = 4;
     break L1;
    }
   case 3:
    {
     i6 = 6;
     break L1;
    }
   case 5:
    {
     i6 = 15;
     break L1;
    }
   case 1:
   case 0:
    {
     i6 = 67;
     break L1;
    }
   default:
    {}
   }
   if ((i11 | 0) < 372) {
    i6 = 21;
    break L1;
   }
   i12 = (i11 | 0) / 24 | 0;
   i10 = i5 + (i12 * 12 | 0) | 0;
   do {
    if ((i11 | 0) > 11988) {
     i14 = (i11 | 0) / 48 | 0;
     i11 = i5 + (i14 * 12 | 0) | 0;
     i14 = i5 + ((i14 + i12 | 0) * 12 | 0) | 0;
     i12 = __ZNSt3__17__sort4IRPFbRK6b2PairS3_EPS1_EEjT0_S8_S8_S8_T_(i5, i11, i10, i14, i1) | 0;
     if (FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i4, i14) | 0) {
      HEAP32[i2 + 0 >> 2] = HEAP32[i14 + 0 >> 2];
      HEAP32[i2 + 4 >> 2] = HEAP32[i14 + 4 >> 2];
      HEAP32[i2 + 8 >> 2] = HEAP32[i14 + 8 >> 2];
      HEAP32[i14 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
      HEAP32[i14 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
      HEAP32[i14 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
      HEAP32[i4 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
      HEAP32[i4 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
      HEAP32[i4 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
      i13 = i12 + 1 | 0;
      if (FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i14, i10) | 0) {
       HEAP32[i2 + 0 >> 2] = HEAP32[i10 + 0 >> 2];
       HEAP32[i2 + 4 >> 2] = HEAP32[i10 + 4 >> 2];
       HEAP32[i2 + 8 >> 2] = HEAP32[i10 + 8 >> 2];
       HEAP32[i10 + 0 >> 2] = HEAP32[i14 + 0 >> 2];
       HEAP32[i10 + 4 >> 2] = HEAP32[i14 + 4 >> 2];
       HEAP32[i10 + 8 >> 2] = HEAP32[i14 + 8 >> 2];
       HEAP32[i14 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
       HEAP32[i14 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
       HEAP32[i14 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
       i13 = i12 + 2 | 0;
       if (FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i10, i11) | 0) {
        HEAP32[i2 + 0 >> 2] = HEAP32[i11 + 0 >> 2];
        HEAP32[i2 + 4 >> 2] = HEAP32[i11 + 4 >> 2];
        HEAP32[i2 + 8 >> 2] = HEAP32[i11 + 8 >> 2];
        HEAP32[i11 + 0 >> 2] = HEAP32[i10 + 0 >> 2];
        HEAP32[i11 + 4 >> 2] = HEAP32[i10 + 4 >> 2];
        HEAP32[i11 + 8 >> 2] = HEAP32[i10 + 8 >> 2];
        HEAP32[i10 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
        HEAP32[i10 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
        HEAP32[i10 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
        if (FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i11, i5) | 0) {
         HEAP32[i2 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
         HEAP32[i2 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
         HEAP32[i2 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
         HEAP32[i5 + 0 >> 2] = HEAP32[i11 + 0 >> 2];
         HEAP32[i5 + 4 >> 2] = HEAP32[i11 + 4 >> 2];
         HEAP32[i5 + 8 >> 2] = HEAP32[i11 + 8 >> 2];
         HEAP32[i11 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
         HEAP32[i11 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
         HEAP32[i11 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
         i12 = i12 + 4 | 0;
        } else {
         i12 = i12 + 3 | 0;
        }
       } else {
        i12 = i13;
       }
      } else {
       i12 = i13;
      }
     }
    } else {
     i15 = FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i10, i5) | 0;
     i11 = FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i4, i10) | 0;
     if (!i15) {
      if (!i11) {
       i12 = 0;
       break;
      }
      HEAP32[i2 + 0 >> 2] = HEAP32[i10 + 0 >> 2];
      HEAP32[i2 + 4 >> 2] = HEAP32[i10 + 4 >> 2];
      HEAP32[i2 + 8 >> 2] = HEAP32[i10 + 8 >> 2];
      HEAP32[i10 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
      HEAP32[i10 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
      HEAP32[i10 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
      HEAP32[i4 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
      HEAP32[i4 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
      HEAP32[i4 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
      if (!(FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i10, i5) | 0)) {
       i12 = 1;
       break;
      }
      HEAP32[i2 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
      HEAP32[i2 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
      HEAP32[i2 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
      HEAP32[i5 + 0 >> 2] = HEAP32[i10 + 0 >> 2];
      HEAP32[i5 + 4 >> 2] = HEAP32[i10 + 4 >> 2];
      HEAP32[i5 + 8 >> 2] = HEAP32[i10 + 8 >> 2];
      HEAP32[i10 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
      HEAP32[i10 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
      HEAP32[i10 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
      i12 = 2;
      break;
     }
     if (i11) {
      HEAP32[i2 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
      HEAP32[i2 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
      HEAP32[i2 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
      HEAP32[i5 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
      HEAP32[i5 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
      HEAP32[i5 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
      HEAP32[i4 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
      HEAP32[i4 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
      HEAP32[i4 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
      i12 = 1;
      break;
     }
     HEAP32[i2 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
     HEAP32[i2 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
     HEAP32[i2 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
     HEAP32[i5 + 0 >> 2] = HEAP32[i10 + 0 >> 2];
     HEAP32[i5 + 4 >> 2] = HEAP32[i10 + 4 >> 2];
     HEAP32[i5 + 8 >> 2] = HEAP32[i10 + 8 >> 2];
     HEAP32[i10 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
     HEAP32[i10 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
     HEAP32[i10 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
     if (FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i4, i10) | 0) {
      HEAP32[i2 + 0 >> 2] = HEAP32[i10 + 0 >> 2];
      HEAP32[i2 + 4 >> 2] = HEAP32[i10 + 4 >> 2];
      HEAP32[i2 + 8 >> 2] = HEAP32[i10 + 8 >> 2];
      HEAP32[i10 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
      HEAP32[i10 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
      HEAP32[i10 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
      HEAP32[i4 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
      HEAP32[i4 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
      HEAP32[i4 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
      i12 = 2;
     } else {
      i12 = 1;
     }
    }
   } while (0);
   do {
    if (FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i5, i10) | 0) {
     i13 = i4;
    } else {
     i13 = i4;
     while (1) {
      i13 = i13 + -12 | 0;
      if ((i5 | 0) == (i13 | 0)) {
       break;
      }
      if (FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i13, i10) | 0) {
       i6 = 50;
       break;
      }
     }
     if ((i6 | 0) == 50) {
      i6 = 0;
      HEAP32[i2 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
      HEAP32[i2 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
      HEAP32[i2 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
      HEAP32[i5 + 0 >> 2] = HEAP32[i13 + 0 >> 2];
      HEAP32[i5 + 4 >> 2] = HEAP32[i13 + 4 >> 2];
      HEAP32[i5 + 8 >> 2] = HEAP32[i13 + 8 >> 2];
      HEAP32[i13 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
      HEAP32[i13 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
      HEAP32[i13 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
      i12 = i12 + 1 | 0;
      break;
     }
     i10 = i5 + 12 | 0;
     if (!(FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i5, i4) | 0)) {
      if ((i10 | 0) == (i4 | 0)) {
       i6 = 67;
       break L1;
      }
      while (1) {
       i9 = i10 + 12 | 0;
       if (FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i5, i10) | 0) {
        break;
       }
       if ((i9 | 0) == (i4 | 0)) {
        i6 = 67;
        break L1;
       } else {
        i10 = i9;
       }
      }
      HEAP32[i2 + 0 >> 2] = HEAP32[i10 + 0 >> 2];
      HEAP32[i2 + 4 >> 2] = HEAP32[i10 + 4 >> 2];
      HEAP32[i2 + 8 >> 2] = HEAP32[i10 + 8 >> 2];
      HEAP32[i10 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
      HEAP32[i10 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
      HEAP32[i10 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
      HEAP32[i4 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
      HEAP32[i4 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
      HEAP32[i4 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
      i10 = i9;
     }
     if ((i10 | 0) == (i4 | 0)) {
      i6 = 67;
      break L1;
     } else {
      i9 = i4;
     }
     while (1) {
      while (1) {
       i11 = i10 + 12 | 0;
       if (FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i5, i10) | 0) {
        break;
       } else {
        i10 = i11;
       }
      }
      do {
       i9 = i9 + -12 | 0;
      } while (FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i5, i9) | 0);
      if (!(i10 >>> 0 < i9 >>> 0)) {
       i5 = i10;
       continue L3;
      }
      HEAP32[i2 + 0 >> 2] = HEAP32[i10 + 0 >> 2];
      HEAP32[i2 + 4 >> 2] = HEAP32[i10 + 4 >> 2];
      HEAP32[i2 + 8 >> 2] = HEAP32[i10 + 8 >> 2];
      HEAP32[i10 + 0 >> 2] = HEAP32[i9 + 0 >> 2];
      HEAP32[i10 + 4 >> 2] = HEAP32[i9 + 4 >> 2];
      HEAP32[i10 + 8 >> 2] = HEAP32[i9 + 8 >> 2];
      HEAP32[i9 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
      HEAP32[i9 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
      HEAP32[i9 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
      i10 = i11;
     }
    }
   } while (0);
   i11 = i5 + 12 | 0;
   L47 : do {
    if (i11 >>> 0 < i13 >>> 0) {
     while (1) {
      i15 = i11;
      while (1) {
       i11 = i15 + 12 | 0;
       if (FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i15, i10) | 0) {
        i15 = i11;
       } else {
        i14 = i13;
        break;
       }
      }
      do {
       i14 = i14 + -12 | 0;
      } while (!(FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i14, i10) | 0));
      if (i15 >>> 0 > i14 >>> 0) {
       i11 = i15;
       break L47;
      }
      HEAP32[i2 + 0 >> 2] = HEAP32[i15 + 0 >> 2];
      HEAP32[i2 + 4 >> 2] = HEAP32[i15 + 4 >> 2];
      HEAP32[i2 + 8 >> 2] = HEAP32[i15 + 8 >> 2];
      HEAP32[i15 + 0 >> 2] = HEAP32[i14 + 0 >> 2];
      HEAP32[i15 + 4 >> 2] = HEAP32[i14 + 4 >> 2];
      HEAP32[i15 + 8 >> 2] = HEAP32[i14 + 8 >> 2];
      HEAP32[i14 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
      HEAP32[i14 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
      HEAP32[i14 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
      i13 = i14;
      i10 = (i10 | 0) == (i15 | 0) ? i14 : i10;
      i12 = i12 + 1 | 0;
     }
    }
   } while (0);
   if ((i11 | 0) != (i10 | 0) ? FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i10, i11) | 0 : 0) {
    HEAP32[i2 + 0 >> 2] = HEAP32[i11 + 0 >> 2];
    HEAP32[i2 + 4 >> 2] = HEAP32[i11 + 4 >> 2];
    HEAP32[i2 + 8 >> 2] = HEAP32[i11 + 8 >> 2];
    HEAP32[i11 + 0 >> 2] = HEAP32[i10 + 0 >> 2];
    HEAP32[i11 + 4 >> 2] = HEAP32[i10 + 4 >> 2];
    HEAP32[i11 + 8 >> 2] = HEAP32[i10 + 8 >> 2];
    HEAP32[i10 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
    HEAP32[i10 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
    HEAP32[i10 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
    i12 = i12 + 1 | 0;
   }
   if ((i12 | 0) == 0) {
    i12 = __ZNSt3__127__insertion_sort_incompleteIRPFbRK6b2PairS3_EPS1_EEbT0_S8_T_(i5, i11, i1) | 0;
    i10 = i11 + 12 | 0;
    if (__ZNSt3__127__insertion_sort_incompleteIRPFbRK6b2PairS3_EPS1_EEbT0_S8_T_(i10, i8, i1) | 0) {
     i6 = 62;
     break;
    }
    if (i12) {
     i5 = i10;
     continue;
    }
   }
   i15 = i11;
   if ((i15 - i9 | 0) >= (i7 - i15 | 0)) {
    i6 = 66;
    break;
   }
   __ZNSt3__16__sortIRPFbRK6b2PairS3_EPS1_EEvT0_S8_T_(i5, i11, i1);
   i5 = i11 + 12 | 0;
  }
  if ((i6 | 0) == 62) {
   i6 = 0;
   if (i12) {
    i6 = 67;
    break;
   } else {
    i8 = i11;
    continue;
   }
  } else if ((i6 | 0) == 66) {
   i6 = 0;
   __ZNSt3__16__sortIRPFbRK6b2PairS3_EPS1_EEvT0_S8_T_(i11 + 12 | 0, i8, i1);
   i8 = i11;
   continue;
  }
 }
 if ((i6 | 0) == 4) {
  if (!(FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i4, i5) | 0)) {
   STACKTOP = i3;
   return;
  }
  HEAP32[i2 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
  HEAP32[i2 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
  HEAP32[i2 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
  HEAP32[i5 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
  HEAP32[i5 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
  HEAP32[i5 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
  HEAP32[i4 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
  HEAP32[i4 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
  HEAP32[i4 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
  STACKTOP = i3;
  return;
 } else if ((i6 | 0) == 6) {
  i6 = i5 + 12 | 0;
  i15 = FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i6, i5) | 0;
  i7 = FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i4, i6) | 0;
  if (!i15) {
   if (!i7) {
    STACKTOP = i3;
    return;
   }
   HEAP32[i2 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
   HEAP32[i2 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
   HEAP32[i2 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
   HEAP32[i6 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
   HEAP32[i6 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
   HEAP32[i6 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
   HEAP32[i4 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
   HEAP32[i4 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
   HEAP32[i4 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
   if (!(FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i6, i5) | 0)) {
    STACKTOP = i3;
    return;
   }
   HEAP32[i2 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
   HEAP32[i2 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
   HEAP32[i2 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
   HEAP32[i5 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
   HEAP32[i5 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
   HEAP32[i5 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
   HEAP32[i6 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
   HEAP32[i6 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
   HEAP32[i6 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
   STACKTOP = i3;
   return;
  }
  if (i7) {
   HEAP32[i2 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
   HEAP32[i2 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
   HEAP32[i2 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
   HEAP32[i5 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
   HEAP32[i5 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
   HEAP32[i5 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
   HEAP32[i4 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
   HEAP32[i4 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
   HEAP32[i4 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
   STACKTOP = i3;
   return;
  }
  HEAP32[i2 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
  HEAP32[i2 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
  HEAP32[i2 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
  HEAP32[i5 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
  HEAP32[i5 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
  HEAP32[i5 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
  HEAP32[i6 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
  HEAP32[i6 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
  HEAP32[i6 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
  if (!(FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i4, i6) | 0)) {
   STACKTOP = i3;
   return;
  }
  HEAP32[i2 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
  HEAP32[i2 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
  HEAP32[i2 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
  HEAP32[i6 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
  HEAP32[i6 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
  HEAP32[i6 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
  HEAP32[i4 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
  HEAP32[i4 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
  HEAP32[i4 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
  STACKTOP = i3;
  return;
 } else if ((i6 | 0) == 14) {
  __ZNSt3__17__sort4IRPFbRK6b2PairS3_EPS1_EEjT0_S8_S8_S8_T_(i5, i5 + 12 | 0, i5 + 24 | 0, i4, i1) | 0;
  STACKTOP = i3;
  return;
 } else if ((i6 | 0) == 15) {
  i6 = i5 + 12 | 0;
  i7 = i5 + 24 | 0;
  i8 = i5 + 36 | 0;
  __ZNSt3__17__sort4IRPFbRK6b2PairS3_EPS1_EEjT0_S8_S8_S8_T_(i5, i6, i7, i8, i1) | 0;
  if (!(FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i4, i8) | 0)) {
   STACKTOP = i3;
   return;
  }
  HEAP32[i2 + 0 >> 2] = HEAP32[i8 + 0 >> 2];
  HEAP32[i2 + 4 >> 2] = HEAP32[i8 + 4 >> 2];
  HEAP32[i2 + 8 >> 2] = HEAP32[i8 + 8 >> 2];
  HEAP32[i8 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
  HEAP32[i8 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
  HEAP32[i8 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
  HEAP32[i4 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
  HEAP32[i4 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
  HEAP32[i4 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
  if (!(FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i8, i7) | 0)) {
   STACKTOP = i3;
   return;
  }
  HEAP32[i2 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
  HEAP32[i2 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
  HEAP32[i2 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
  HEAP32[i7 + 0 >> 2] = HEAP32[i8 + 0 >> 2];
  HEAP32[i7 + 4 >> 2] = HEAP32[i8 + 4 >> 2];
  HEAP32[i7 + 8 >> 2] = HEAP32[i8 + 8 >> 2];
  HEAP32[i8 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
  HEAP32[i8 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
  HEAP32[i8 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
  if (!(FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i7, i6) | 0)) {
   STACKTOP = i3;
   return;
  }
  HEAP32[i2 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
  HEAP32[i2 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
  HEAP32[i2 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
  HEAP32[i6 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
  HEAP32[i6 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
  HEAP32[i6 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
  HEAP32[i7 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
  HEAP32[i7 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
  HEAP32[i7 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
  if (!(FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i6, i5) | 0)) {
   STACKTOP = i3;
   return;
  }
  HEAP32[i2 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
  HEAP32[i2 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
  HEAP32[i2 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
  HEAP32[i5 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
  HEAP32[i5 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
  HEAP32[i5 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
  HEAP32[i6 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
  HEAP32[i6 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
  HEAP32[i6 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
  STACKTOP = i3;
  return;
 } else if ((i6 | 0) == 21) {
  __ZNSt3__118__insertion_sort_3IRPFbRK6b2PairS3_EPS1_EEvT0_S8_T_(i5, i8, i1);
  STACKTOP = i3;
  return;
 } else if ((i6 | 0) == 67) {
  STACKTOP = i3;
  return;
 }
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
 i16 = HEAP32[7176 >> 2] | 0;
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
   if ((i13 | 0) == (HEAP32[7180 >> 2] | 0)) {
    i2 = i7 + (i8 + -4) | 0;
    if ((HEAP32[i2 >> 2] & 3 | 0) != 3) {
     i2 = i13;
     i11 = i12;
     break;
    }
    HEAP32[7168 >> 2] = i12;
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
    i14 = 7200 + (i18 << 1 << 2) | 0;
    if ((i2 | 0) != (i14 | 0)) {
     if (i2 >>> 0 < i16 >>> 0) {
      _abort();
     }
     if ((HEAP32[i2 + 12 >> 2] | 0) != (i13 | 0)) {
      _abort();
     }
    }
    if ((i11 | 0) == (i2 | 0)) {
     HEAP32[1790] = HEAP32[1790] & ~(1 << i18);
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
    i16 = 7464 + (i18 << 2) | 0;
    if ((i13 | 0) == (HEAP32[i16 >> 2] | 0)) {
     HEAP32[i16 >> 2] = i14;
     if ((i14 | 0) == 0) {
      HEAP32[7164 >> 2] = HEAP32[7164 >> 2] & ~(1 << i18);
      i2 = i13;
      i11 = i12;
      break;
     }
    } else {
     if (i17 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
    if (i14 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
     _abort();
    }
    HEAP32[i14 + 24 >> 2] = i17;
    i16 = HEAP32[i7 + (i15 + 16) >> 2] | 0;
    do {
     if ((i16 | 0) != 0) {
      if (i16 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
     if (i15 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
  if ((i6 | 0) == (HEAP32[7184 >> 2] | 0)) {
   i21 = (HEAP32[7172 >> 2] | 0) + i11 | 0;
   HEAP32[7172 >> 2] = i21;
   HEAP32[7184 >> 2] = i2;
   HEAP32[i2 + 4 >> 2] = i21 | 1;
   if ((i2 | 0) != (HEAP32[7180 >> 2] | 0)) {
    STACKTOP = i1;
    return;
   }
   HEAP32[7180 >> 2] = 0;
   HEAP32[7168 >> 2] = 0;
   STACKTOP = i1;
   return;
  }
  if ((i6 | 0) == (HEAP32[7180 >> 2] | 0)) {
   i21 = (HEAP32[7168 >> 2] | 0) + i11 | 0;
   HEAP32[7168 >> 2] = i21;
   HEAP32[7180 >> 2] = i2;
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
      if (i13 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
       _abort();
      } else {
       HEAP32[i13 >> 2] = 0;
       i9 = i12;
       break;
      }
     } else {
      i13 = HEAP32[i7 + i8 >> 2] | 0;
      if (i13 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
     i13 = 7464 + (i12 << 2) | 0;
     if ((i6 | 0) == (HEAP32[i13 >> 2] | 0)) {
      HEAP32[i13 >> 2] = i9;
      if ((i9 | 0) == 0) {
       HEAP32[7164 >> 2] = HEAP32[7164 >> 2] & ~(1 << i12);
       break;
      }
     } else {
      if (i10 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
     if (i9 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
      _abort();
     }
     HEAP32[i9 + 24 >> 2] = i10;
     i6 = HEAP32[i7 + (i8 + 8) >> 2] | 0;
     do {
      if ((i6 | 0) != 0) {
       if (i6 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
      if (i6 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
    i8 = 7200 + (i12 << 1 << 2) | 0;
    if ((i9 | 0) != (i8 | 0)) {
     if (i9 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
      _abort();
     }
     if ((HEAP32[i9 + 12 >> 2] | 0) != (i6 | 0)) {
      _abort();
     }
    }
    if ((i7 | 0) == (i9 | 0)) {
     HEAP32[1790] = HEAP32[1790] & ~(1 << i12);
     break;
    }
    if ((i7 | 0) != (i8 | 0)) {
     if (i7 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
  if ((i2 | 0) == (HEAP32[7180 >> 2] | 0)) {
   HEAP32[7168 >> 2] = i11;
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
  i3 = 7200 + (i7 << 2) | 0;
  i8 = HEAP32[1790] | 0;
  i6 = 1 << i6;
  if ((i8 & i6 | 0) != 0) {
   i6 = 7200 + (i7 + 2 << 2) | 0;
   i7 = HEAP32[i6 >> 2] | 0;
   if (i7 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
    _abort();
   } else {
    i4 = i6;
    i5 = i7;
   }
  } else {
   HEAP32[1790] = i8 | i6;
   i4 = 7200 + (i7 + 2 << 2) | 0;
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
 i5 = 7464 + (i4 << 2) | 0;
 HEAP32[i2 + 28 >> 2] = i4;
 HEAP32[i2 + 20 >> 2] = 0;
 HEAP32[i2 + 16 >> 2] = 0;
 i7 = HEAP32[7164 >> 2] | 0;
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
     if (i6 >>> 0 < (HEAP32[7176 >> 2] | 0) >>> 0) {
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
   i6 = HEAP32[7176 >> 2] | 0;
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
   HEAP32[7164 >> 2] = i7 | i6;
   HEAP32[i5 >> 2] = i2;
   HEAP32[i2 + 24 >> 2] = i5;
   HEAP32[i2 + 12 >> 2] = i2;
   HEAP32[i2 + 8 >> 2] = i2;
  }
 } while (0);
 i21 = (HEAP32[7192 >> 2] | 0) + -1 | 0;
 HEAP32[7192 >> 2] = i21;
 if ((i21 | 0) == 0) {
  i2 = 7616 | 0;
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
 HEAP32[7192 >> 2] = -1;
 STACKTOP = i1;
 return;
}
function __ZNSt3__127__insertion_sort_incompleteIRPFbRK6b2PairS3_EPS1_EEbT0_S8_T_(i3, i4, i2) {
 i3 = i3 | 0;
 i4 = i4 | 0;
 i2 = i2 | 0;
 var i1 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 32 | 0;
 i7 = i1 + 12 | 0;
 i6 = i1;
 switch ((i4 - i3 | 0) / 12 | 0 | 0) {
 case 5:
  {
   i6 = i3 + 12 | 0;
   i8 = i3 + 24 | 0;
   i5 = i3 + 36 | 0;
   i4 = i4 + -12 | 0;
   __ZNSt3__17__sort4IRPFbRK6b2PairS3_EPS1_EEjT0_S8_S8_S8_T_(i3, i6, i8, i5, i2) | 0;
   if (!(FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i4, i5) | 0)) {
    i10 = 1;
    STACKTOP = i1;
    return i10 | 0;
   }
   HEAP32[i7 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
   HEAP32[i7 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
   HEAP32[i7 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
   HEAP32[i5 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
   HEAP32[i5 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
   HEAP32[i5 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
   HEAP32[i4 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
   HEAP32[i4 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
   HEAP32[i4 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
   if (!(FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i5, i8) | 0)) {
    i10 = 1;
    STACKTOP = i1;
    return i10 | 0;
   }
   HEAP32[i7 + 0 >> 2] = HEAP32[i8 + 0 >> 2];
   HEAP32[i7 + 4 >> 2] = HEAP32[i8 + 4 >> 2];
   HEAP32[i7 + 8 >> 2] = HEAP32[i8 + 8 >> 2];
   HEAP32[i8 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
   HEAP32[i8 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
   HEAP32[i8 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
   HEAP32[i5 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
   HEAP32[i5 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
   HEAP32[i5 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
   if (!(FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i8, i6) | 0)) {
    i10 = 1;
    STACKTOP = i1;
    return i10 | 0;
   }
   HEAP32[i7 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
   HEAP32[i7 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
   HEAP32[i7 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
   HEAP32[i6 + 0 >> 2] = HEAP32[i8 + 0 >> 2];
   HEAP32[i6 + 4 >> 2] = HEAP32[i8 + 4 >> 2];
   HEAP32[i6 + 8 >> 2] = HEAP32[i8 + 8 >> 2];
   HEAP32[i8 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
   HEAP32[i8 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
   HEAP32[i8 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
   if (!(FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i6, i3) | 0)) {
    i10 = 1;
    STACKTOP = i1;
    return i10 | 0;
   }
   HEAP32[i7 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
   HEAP32[i7 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
   HEAP32[i7 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
   HEAP32[i3 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
   HEAP32[i3 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
   HEAP32[i3 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
   HEAP32[i6 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
   HEAP32[i6 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
   HEAP32[i6 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
   i10 = 1;
   STACKTOP = i1;
   return i10 | 0;
  }
 case 4:
  {
   __ZNSt3__17__sort4IRPFbRK6b2PairS3_EPS1_EEjT0_S8_S8_S8_T_(i3, i3 + 12 | 0, i3 + 24 | 0, i4 + -12 | 0, i2) | 0;
   i10 = 1;
   STACKTOP = i1;
   return i10 | 0;
  }
 case 3:
  {
   i5 = i3 + 12 | 0;
   i4 = i4 + -12 | 0;
   i10 = FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i5, i3) | 0;
   i6 = FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i4, i5) | 0;
   if (!i10) {
    if (!i6) {
     i10 = 1;
     STACKTOP = i1;
     return i10 | 0;
    }
    HEAP32[i7 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
    HEAP32[i7 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
    HEAP32[i7 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
    HEAP32[i5 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
    HEAP32[i5 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
    HEAP32[i5 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
    HEAP32[i4 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
    HEAP32[i4 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
    HEAP32[i4 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
    if (!(FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i5, i3) | 0)) {
     i10 = 1;
     STACKTOP = i1;
     return i10 | 0;
    }
    HEAP32[i7 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
    HEAP32[i7 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
    HEAP32[i7 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
    HEAP32[i3 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
    HEAP32[i3 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
    HEAP32[i3 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
    HEAP32[i5 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
    HEAP32[i5 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
    HEAP32[i5 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
    i10 = 1;
    STACKTOP = i1;
    return i10 | 0;
   }
   if (i6) {
    HEAP32[i7 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
    HEAP32[i7 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
    HEAP32[i7 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
    HEAP32[i3 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
    HEAP32[i3 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
    HEAP32[i3 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
    HEAP32[i4 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
    HEAP32[i4 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
    HEAP32[i4 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
    i10 = 1;
    STACKTOP = i1;
    return i10 | 0;
   }
   HEAP32[i7 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
   HEAP32[i7 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
   HEAP32[i7 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
   HEAP32[i3 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
   HEAP32[i3 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
   HEAP32[i3 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
   HEAP32[i5 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
   HEAP32[i5 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
   HEAP32[i5 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
   if (!(FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i4, i5) | 0)) {
    i10 = 1;
    STACKTOP = i1;
    return i10 | 0;
   }
   HEAP32[i7 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
   HEAP32[i7 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
   HEAP32[i7 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
   HEAP32[i5 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
   HEAP32[i5 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
   HEAP32[i5 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
   HEAP32[i4 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
   HEAP32[i4 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
   HEAP32[i4 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
   i10 = 1;
   STACKTOP = i1;
   return i10 | 0;
  }
 case 2:
  {
   i4 = i4 + -12 | 0;
   if (!(FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i4, i3) | 0)) {
    i10 = 1;
    STACKTOP = i1;
    return i10 | 0;
   }
   HEAP32[i7 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
   HEAP32[i7 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
   HEAP32[i7 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
   HEAP32[i3 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
   HEAP32[i3 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
   HEAP32[i3 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
   HEAP32[i4 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
   HEAP32[i4 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
   HEAP32[i4 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
   i10 = 1;
   STACKTOP = i1;
   return i10 | 0;
  }
 case 1:
 case 0:
  {
   i10 = 1;
   STACKTOP = i1;
   return i10 | 0;
  }
 default:
  {
   i9 = i3 + 24 | 0;
   i10 = i3 + 12 | 0;
   i11 = FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i10, i3) | 0;
   i8 = FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i9, i10) | 0;
   do {
    if (i11) {
     if (i8) {
      HEAP32[i7 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
      HEAP32[i7 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
      HEAP32[i7 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
      HEAP32[i3 + 0 >> 2] = HEAP32[i9 + 0 >> 2];
      HEAP32[i3 + 4 >> 2] = HEAP32[i9 + 4 >> 2];
      HEAP32[i3 + 8 >> 2] = HEAP32[i9 + 8 >> 2];
      HEAP32[i9 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
      HEAP32[i9 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
      HEAP32[i9 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
      break;
     }
     HEAP32[i7 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
     HEAP32[i7 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
     HEAP32[i7 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
     HEAP32[i3 + 0 >> 2] = HEAP32[i10 + 0 >> 2];
     HEAP32[i3 + 4 >> 2] = HEAP32[i10 + 4 >> 2];
     HEAP32[i3 + 8 >> 2] = HEAP32[i10 + 8 >> 2];
     HEAP32[i10 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
     HEAP32[i10 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
     HEAP32[i10 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
     if (FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i9, i10) | 0) {
      HEAP32[i7 + 0 >> 2] = HEAP32[i10 + 0 >> 2];
      HEAP32[i7 + 4 >> 2] = HEAP32[i10 + 4 >> 2];
      HEAP32[i7 + 8 >> 2] = HEAP32[i10 + 8 >> 2];
      HEAP32[i10 + 0 >> 2] = HEAP32[i9 + 0 >> 2];
      HEAP32[i10 + 4 >> 2] = HEAP32[i9 + 4 >> 2];
      HEAP32[i10 + 8 >> 2] = HEAP32[i9 + 8 >> 2];
      HEAP32[i9 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
      HEAP32[i9 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
      HEAP32[i9 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
     }
    } else {
     if (i8) {
      HEAP32[i7 + 0 >> 2] = HEAP32[i10 + 0 >> 2];
      HEAP32[i7 + 4 >> 2] = HEAP32[i10 + 4 >> 2];
      HEAP32[i7 + 8 >> 2] = HEAP32[i10 + 8 >> 2];
      HEAP32[i10 + 0 >> 2] = HEAP32[i9 + 0 >> 2];
      HEAP32[i10 + 4 >> 2] = HEAP32[i9 + 4 >> 2];
      HEAP32[i10 + 8 >> 2] = HEAP32[i9 + 8 >> 2];
      HEAP32[i9 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
      HEAP32[i9 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
      HEAP32[i9 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
      if (FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i10, i3) | 0) {
       HEAP32[i7 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
       HEAP32[i7 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
       HEAP32[i7 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
       HEAP32[i3 + 0 >> 2] = HEAP32[i10 + 0 >> 2];
       HEAP32[i3 + 4 >> 2] = HEAP32[i10 + 4 >> 2];
       HEAP32[i3 + 8 >> 2] = HEAP32[i10 + 8 >> 2];
       HEAP32[i10 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
       HEAP32[i10 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
       HEAP32[i10 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
      }
     }
    }
   } while (0);
   i7 = i3 + 36 | 0;
   if ((i7 | 0) == (i4 | 0)) {
    i11 = 1;
    STACKTOP = i1;
    return i11 | 0;
   }
   i8 = 0;
   while (1) {
    if (FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i7, i9) | 0) {
     HEAP32[i6 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
     HEAP32[i6 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
     HEAP32[i6 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
     i10 = i7;
     while (1) {
      HEAP32[i10 + 0 >> 2] = HEAP32[i9 + 0 >> 2];
      HEAP32[i10 + 4 >> 2] = HEAP32[i9 + 4 >> 2];
      HEAP32[i10 + 8 >> 2] = HEAP32[i9 + 8 >> 2];
      if ((i9 | 0) == (i3 | 0)) {
       break;
      }
      i10 = i9 + -12 | 0;
      if (FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i6, i10) | 0) {
       i11 = i9;
       i9 = i10;
       i10 = i11;
      } else {
       break;
      }
     }
     HEAP32[i9 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
     HEAP32[i9 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
     HEAP32[i9 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
     i8 = i8 + 1 | 0;
     if ((i8 | 0) == 8) {
      break;
     }
    }
    i9 = i7 + 12 | 0;
    if ((i9 | 0) == (i4 | 0)) {
     i2 = 1;
     i5 = 35;
     break;
    } else {
     i11 = i7;
     i7 = i9;
     i9 = i11;
    }
   }
   if ((i5 | 0) == 35) {
    STACKTOP = i1;
    return i2 | 0;
   }
   i11 = (i7 + 12 | 0) == (i4 | 0);
   STACKTOP = i1;
   return i11 | 0;
  }
 }
 return 0;
}
function __ZN13b2DynamicTree7BalanceEi(i11, i6) {
 i11 = i11 | 0;
 i6 = i6 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, i5 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, d19 = 0.0, i20 = 0, i21 = 0, d22 = 0.0, d23 = 0.0, d24 = 0.0, d25 = 0.0;
 i1 = STACKTOP;
 if ((i6 | 0) == -1) {
  ___assert_fail(3216, 2944, 382, 3232);
 }
 i5 = HEAP32[i11 + 4 >> 2] | 0;
 i13 = i5 + (i6 * 36 | 0) | 0;
 i18 = i5 + (i6 * 36 | 0) + 24 | 0;
 i8 = HEAP32[i18 >> 2] | 0;
 if ((i8 | 0) == -1) {
  i21 = i6;
  STACKTOP = i1;
  return i21 | 0;
 }
 i2 = i5 + (i6 * 36 | 0) + 32 | 0;
 if ((HEAP32[i2 >> 2] | 0) < 2) {
  i21 = i6;
  STACKTOP = i1;
  return i21 | 0;
 }
 i20 = i5 + (i6 * 36 | 0) + 28 | 0;
 i7 = HEAP32[i20 >> 2] | 0;
 if (!((i8 | 0) > -1)) {
  ___assert_fail(3240, 2944, 392, 3232);
 }
 i12 = HEAP32[i11 + 12 >> 2] | 0;
 if ((i8 | 0) >= (i12 | 0)) {
  ___assert_fail(3240, 2944, 392, 3232);
 }
 if (!((i7 | 0) > -1 & (i7 | 0) < (i12 | 0))) {
  ___assert_fail(3272, 2944, 393, 3232);
 }
 i9 = i5 + (i8 * 36 | 0) | 0;
 i10 = i5 + (i7 * 36 | 0) | 0;
 i3 = i5 + (i7 * 36 | 0) + 32 | 0;
 i4 = i5 + (i8 * 36 | 0) + 32 | 0;
 i14 = (HEAP32[i3 >> 2] | 0) - (HEAP32[i4 >> 2] | 0) | 0;
 if ((i14 | 0) > 1) {
  i21 = i5 + (i7 * 36 | 0) + 24 | 0;
  i14 = HEAP32[i21 >> 2] | 0;
  i18 = i5 + (i7 * 36 | 0) + 28 | 0;
  i15 = HEAP32[i18 >> 2] | 0;
  i16 = i5 + (i14 * 36 | 0) | 0;
  i17 = i5 + (i15 * 36 | 0) | 0;
  if (!((i14 | 0) > -1 & (i14 | 0) < (i12 | 0))) {
   ___assert_fail(3304, 2944, 407, 3232);
  }
  if (!((i15 | 0) > -1 & (i15 | 0) < (i12 | 0))) {
   ___assert_fail(3336, 2944, 408, 3232);
  }
  HEAP32[i21 >> 2] = i6;
  i21 = i5 + (i6 * 36 | 0) + 20 | 0;
  i12 = i5 + (i7 * 36 | 0) + 20 | 0;
  HEAP32[i12 >> 2] = HEAP32[i21 >> 2];
  HEAP32[i21 >> 2] = i7;
  i12 = HEAP32[i12 >> 2] | 0;
  do {
   if (!((i12 | 0) == -1)) {
    i11 = i5 + (i12 * 36 | 0) + 24 | 0;
    if ((HEAP32[i11 >> 2] | 0) == (i6 | 0)) {
     HEAP32[i11 >> 2] = i7;
     break;
    }
    i11 = i5 + (i12 * 36 | 0) + 28 | 0;
    if ((HEAP32[i11 >> 2] | 0) == (i6 | 0)) {
     HEAP32[i11 >> 2] = i7;
     break;
    } else {
     ___assert_fail(3368, 2944, 424, 3232);
    }
   } else {
    HEAP32[i11 >> 2] = i7;
   }
  } while (0);
  i11 = i5 + (i14 * 36 | 0) + 32 | 0;
  i12 = i5 + (i15 * 36 | 0) + 32 | 0;
  if ((HEAP32[i11 >> 2] | 0) > (HEAP32[i12 >> 2] | 0)) {
   HEAP32[i18 >> 2] = i14;
   HEAP32[i20 >> 2] = i15;
   HEAP32[i5 + (i15 * 36 | 0) + 20 >> 2] = i6;
   d19 = +HEAPF32[i9 >> 2];
   d22 = +HEAPF32[i17 >> 2];
   d19 = d19 < d22 ? d19 : d22;
   d23 = +HEAPF32[i5 + (i8 * 36 | 0) + 4 >> 2];
   d22 = +HEAPF32[i5 + (i15 * 36 | 0) + 4 >> 2];
   d24 = +d19;
   d23 = +(d23 < d22 ? d23 : d22);
   i21 = i13;
   HEAPF32[i21 >> 2] = d24;
   HEAPF32[i21 + 4 >> 2] = d23;
   d23 = +HEAPF32[i5 + (i8 * 36 | 0) + 8 >> 2];
   d24 = +HEAPF32[i5 + (i15 * 36 | 0) + 8 >> 2];
   d22 = +HEAPF32[i5 + (i8 * 36 | 0) + 12 >> 2];
   d25 = +HEAPF32[i5 + (i15 * 36 | 0) + 12 >> 2];
   d23 = +(d23 > d24 ? d23 : d24);
   d24 = +(d22 > d25 ? d22 : d25);
   i21 = i5 + (i6 * 36 | 0) + 8 | 0;
   HEAPF32[i21 >> 2] = d23;
   HEAPF32[i21 + 4 >> 2] = d24;
   d24 = +HEAPF32[i16 >> 2];
   d22 = +HEAPF32[i5 + (i6 * 36 | 0) + 4 >> 2];
   d23 = +HEAPF32[i5 + (i14 * 36 | 0) + 4 >> 2];
   d19 = +(d19 < d24 ? d19 : d24);
   d22 = +(d22 < d23 ? d22 : d23);
   i21 = i10;
   HEAPF32[i21 >> 2] = d19;
   HEAPF32[i21 + 4 >> 2] = d22;
   d22 = +HEAPF32[i5 + (i6 * 36 | 0) + 8 >> 2];
   d19 = +HEAPF32[i5 + (i14 * 36 | 0) + 8 >> 2];
   d23 = +HEAPF32[i5 + (i6 * 36 | 0) + 12 >> 2];
   d24 = +HEAPF32[i5 + (i14 * 36 | 0) + 12 >> 2];
   d19 = +(d22 > d19 ? d22 : d19);
   d25 = +(d23 > d24 ? d23 : d24);
   i5 = i5 + (i7 * 36 | 0) + 8 | 0;
   HEAPF32[i5 >> 2] = d19;
   HEAPF32[i5 + 4 >> 2] = d25;
   i4 = HEAP32[i4 >> 2] | 0;
   i5 = HEAP32[i12 >> 2] | 0;
   i4 = ((i4 | 0) > (i5 | 0) ? i4 : i5) + 1 | 0;
   HEAP32[i2 >> 2] = i4;
   i2 = HEAP32[i11 >> 2] | 0;
   i2 = (i4 | 0) > (i2 | 0) ? i4 : i2;
  } else {
   HEAP32[i18 >> 2] = i15;
   HEAP32[i20 >> 2] = i14;
   HEAP32[i5 + (i14 * 36 | 0) + 20 >> 2] = i6;
   d19 = +HEAPF32[i9 >> 2];
   d22 = +HEAPF32[i16 >> 2];
   d19 = d19 < d22 ? d19 : d22;
   d23 = +HEAPF32[i5 + (i8 * 36 | 0) + 4 >> 2];
   d24 = +HEAPF32[i5 + (i14 * 36 | 0) + 4 >> 2];
   d22 = +d19;
   d23 = +(d23 < d24 ? d23 : d24);
   i21 = i13;
   HEAPF32[i21 >> 2] = d22;
   HEAPF32[i21 + 4 >> 2] = d23;
   d23 = +HEAPF32[i5 + (i8 * 36 | 0) + 8 >> 2];
   d24 = +HEAPF32[i5 + (i14 * 36 | 0) + 8 >> 2];
   d22 = +HEAPF32[i5 + (i8 * 36 | 0) + 12 >> 2];
   d25 = +HEAPF32[i5 + (i14 * 36 | 0) + 12 >> 2];
   d23 = +(d23 > d24 ? d23 : d24);
   d24 = +(d22 > d25 ? d22 : d25);
   i21 = i5 + (i6 * 36 | 0) + 8 | 0;
   HEAPF32[i21 >> 2] = d23;
   HEAPF32[i21 + 4 >> 2] = d24;
   d24 = +HEAPF32[i17 >> 2];
   d22 = +HEAPF32[i5 + (i6 * 36 | 0) + 4 >> 2];
   d23 = +HEAPF32[i5 + (i15 * 36 | 0) + 4 >> 2];
   d19 = +(d19 < d24 ? d19 : d24);
   d23 = +(d22 < d23 ? d22 : d23);
   i21 = i10;
   HEAPF32[i21 >> 2] = d19;
   HEAPF32[i21 + 4 >> 2] = d23;
   d23 = +HEAPF32[i5 + (i6 * 36 | 0) + 8 >> 2];
   d19 = +HEAPF32[i5 + (i15 * 36 | 0) + 8 >> 2];
   d22 = +HEAPF32[i5 + (i6 * 36 | 0) + 12 >> 2];
   d24 = +HEAPF32[i5 + (i15 * 36 | 0) + 12 >> 2];
   d19 = +(d23 > d19 ? d23 : d19);
   d25 = +(d22 > d24 ? d22 : d24);
   i5 = i5 + (i7 * 36 | 0) + 8 | 0;
   HEAPF32[i5 >> 2] = d19;
   HEAPF32[i5 + 4 >> 2] = d25;
   i4 = HEAP32[i4 >> 2] | 0;
   i5 = HEAP32[i11 >> 2] | 0;
   i4 = ((i4 | 0) > (i5 | 0) ? i4 : i5) + 1 | 0;
   HEAP32[i2 >> 2] = i4;
   i2 = HEAP32[i12 >> 2] | 0;
   i2 = (i4 | 0) > (i2 | 0) ? i4 : i2;
  }
  HEAP32[i3 >> 2] = i2 + 1;
  i21 = i7;
  STACKTOP = i1;
  return i21 | 0;
 }
 if (!((i14 | 0) < -1)) {
  i21 = i6;
  STACKTOP = i1;
  return i21 | 0;
 }
 i21 = i5 + (i8 * 36 | 0) + 24 | 0;
 i14 = HEAP32[i21 >> 2] | 0;
 i20 = i5 + (i8 * 36 | 0) + 28 | 0;
 i15 = HEAP32[i20 >> 2] | 0;
 i17 = i5 + (i14 * 36 | 0) | 0;
 i16 = i5 + (i15 * 36 | 0) | 0;
 if (!((i14 | 0) > -1 & (i14 | 0) < (i12 | 0))) {
  ___assert_fail(3400, 2944, 467, 3232);
 }
 if (!((i15 | 0) > -1 & (i15 | 0) < (i12 | 0))) {
  ___assert_fail(3432, 2944, 468, 3232);
 }
 HEAP32[i21 >> 2] = i6;
 i21 = i5 + (i6 * 36 | 0) + 20 | 0;
 i12 = i5 + (i8 * 36 | 0) + 20 | 0;
 HEAP32[i12 >> 2] = HEAP32[i21 >> 2];
 HEAP32[i21 >> 2] = i8;
 i12 = HEAP32[i12 >> 2] | 0;
 do {
  if (!((i12 | 0) == -1)) {
   i11 = i5 + (i12 * 36 | 0) + 24 | 0;
   if ((HEAP32[i11 >> 2] | 0) == (i6 | 0)) {
    HEAP32[i11 >> 2] = i8;
    break;
   }
   i11 = i5 + (i12 * 36 | 0) + 28 | 0;
   if ((HEAP32[i11 >> 2] | 0) == (i6 | 0)) {
    HEAP32[i11 >> 2] = i8;
    break;
   } else {
    ___assert_fail(3464, 2944, 484, 3232);
   }
  } else {
   HEAP32[i11 >> 2] = i8;
  }
 } while (0);
 i12 = i5 + (i14 * 36 | 0) + 32 | 0;
 i11 = i5 + (i15 * 36 | 0) + 32 | 0;
 if ((HEAP32[i12 >> 2] | 0) > (HEAP32[i11 >> 2] | 0)) {
  HEAP32[i20 >> 2] = i14;
  HEAP32[i18 >> 2] = i15;
  HEAP32[i5 + (i15 * 36 | 0) + 20 >> 2] = i6;
  d19 = +HEAPF32[i10 >> 2];
  d22 = +HEAPF32[i16 >> 2];
  d19 = d19 < d22 ? d19 : d22;
  d23 = +HEAPF32[i5 + (i7 * 36 | 0) + 4 >> 2];
  d22 = +HEAPF32[i5 + (i15 * 36 | 0) + 4 >> 2];
  d24 = +d19;
  d23 = +(d23 < d22 ? d23 : d22);
  i21 = i13;
  HEAPF32[i21 >> 2] = d24;
  HEAPF32[i21 + 4 >> 2] = d23;
  d23 = +HEAPF32[i5 + (i7 * 36 | 0) + 8 >> 2];
  d22 = +HEAPF32[i5 + (i15 * 36 | 0) + 8 >> 2];
  d24 = +HEAPF32[i5 + (i7 * 36 | 0) + 12 >> 2];
  d25 = +HEAPF32[i5 + (i15 * 36 | 0) + 12 >> 2];
  d22 = +(d23 > d22 ? d23 : d22);
  d24 = +(d24 > d25 ? d24 : d25);
  i21 = i5 + (i6 * 36 | 0) + 8 | 0;
  HEAPF32[i21 >> 2] = d22;
  HEAPF32[i21 + 4 >> 2] = d24;
  d24 = +HEAPF32[i17 >> 2];
  d23 = +HEAPF32[i5 + (i6 * 36 | 0) + 4 >> 2];
  d22 = +HEAPF32[i5 + (i14 * 36 | 0) + 4 >> 2];
  d19 = +(d19 < d24 ? d19 : d24);
  d22 = +(d23 < d22 ? d23 : d22);
  i21 = i9;
  HEAPF32[i21 >> 2] = d19;
  HEAPF32[i21 + 4 >> 2] = d22;
  d22 = +HEAPF32[i5 + (i6 * 36 | 0) + 8 >> 2];
  d23 = +HEAPF32[i5 + (i14 * 36 | 0) + 8 >> 2];
  d19 = +HEAPF32[i5 + (i6 * 36 | 0) + 12 >> 2];
  d24 = +HEAPF32[i5 + (i14 * 36 | 0) + 12 >> 2];
  d22 = +(d22 > d23 ? d22 : d23);
  d25 = +(d19 > d24 ? d19 : d24);
  i5 = i5 + (i8 * 36 | 0) + 8 | 0;
  HEAPF32[i5 >> 2] = d22;
  HEAPF32[i5 + 4 >> 2] = d25;
  i3 = HEAP32[i3 >> 2] | 0;
  i5 = HEAP32[i11 >> 2] | 0;
  i3 = ((i3 | 0) > (i5 | 0) ? i3 : i5) + 1 | 0;
  HEAP32[i2 >> 2] = i3;
  i2 = HEAP32[i12 >> 2] | 0;
  i2 = (i3 | 0) > (i2 | 0) ? i3 : i2;
 } else {
  HEAP32[i20 >> 2] = i15;
  HEAP32[i18 >> 2] = i14;
  HEAP32[i5 + (i14 * 36 | 0) + 20 >> 2] = i6;
  d19 = +HEAPF32[i10 >> 2];
  d22 = +HEAPF32[i17 >> 2];
  d19 = d19 < d22 ? d19 : d22;
  d23 = +HEAPF32[i5 + (i7 * 36 | 0) + 4 >> 2];
  d24 = +HEAPF32[i5 + (i14 * 36 | 0) + 4 >> 2];
  d22 = +d19;
  d24 = +(d23 < d24 ? d23 : d24);
  i21 = i13;
  HEAPF32[i21 >> 2] = d22;
  HEAPF32[i21 + 4 >> 2] = d24;
  d24 = +HEAPF32[i5 + (i7 * 36 | 0) + 8 >> 2];
  d23 = +HEAPF32[i5 + (i14 * 36 | 0) + 8 >> 2];
  d22 = +HEAPF32[i5 + (i7 * 36 | 0) + 12 >> 2];
  d25 = +HEAPF32[i5 + (i14 * 36 | 0) + 12 >> 2];
  d23 = +(d24 > d23 ? d24 : d23);
  d24 = +(d22 > d25 ? d22 : d25);
  i21 = i5 + (i6 * 36 | 0) + 8 | 0;
  HEAPF32[i21 >> 2] = d23;
  HEAPF32[i21 + 4 >> 2] = d24;
  d24 = +HEAPF32[i16 >> 2];
  d23 = +HEAPF32[i5 + (i6 * 36 | 0) + 4 >> 2];
  d22 = +HEAPF32[i5 + (i15 * 36 | 0) + 4 >> 2];
  d19 = +(d19 < d24 ? d19 : d24);
  d22 = +(d23 < d22 ? d23 : d22);
  i21 = i9;
  HEAPF32[i21 >> 2] = d19;
  HEAPF32[i21 + 4 >> 2] = d22;
  d22 = +HEAPF32[i5 + (i6 * 36 | 0) + 8 >> 2];
  d23 = +HEAPF32[i5 + (i15 * 36 | 0) + 8 >> 2];
  d19 = +HEAPF32[i5 + (i6 * 36 | 0) + 12 >> 2];
  d24 = +HEAPF32[i5 + (i15 * 36 | 0) + 12 >> 2];
  d22 = +(d22 > d23 ? d22 : d23);
  d25 = +(d19 > d24 ? d19 : d24);
  i5 = i5 + (i8 * 36 | 0) + 8 | 0;
  HEAPF32[i5 >> 2] = d22;
  HEAPF32[i5 + 4 >> 2] = d25;
  i3 = HEAP32[i3 >> 2] | 0;
  i5 = HEAP32[i12 >> 2] | 0;
  i3 = ((i3 | 0) > (i5 | 0) ? i3 : i5) + 1 | 0;
  HEAP32[i2 >> 2] = i3;
  i2 = HEAP32[i11 >> 2] | 0;
  i2 = (i3 | 0) > (i2 | 0) ? i3 : i2;
 }
 HEAP32[i4 >> 2] = i2 + 1;
 i21 = i8;
 STACKTOP = i1;
 return i21 | 0;
}
function __Z10b2DistanceP16b2DistanceOutputP14b2SimplexCachePK15b2DistanceInput(i2, i5, i3) {
 i2 = i2 | 0;
 i5 = i5 | 0;
 i3 = i3 | 0;
 var i1 = 0, i4 = 0, i6 = 0, d7 = 0.0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, d16 = 0.0, d17 = 0.0, d18 = 0.0, d19 = 0.0, i20 = 0, d21 = 0.0, d22 = 0.0, i23 = 0, d24 = 0.0, d25 = 0.0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, d36 = 0.0, d37 = 0.0, d38 = 0.0, i39 = 0, i40 = 0, i41 = 0, i42 = 0, d43 = 0.0, d44 = 0.0, d45 = 0.0, i46 = 0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 176 | 0;
 i11 = i1 + 152 | 0;
 i10 = i1 + 136 | 0;
 i4 = i1 + 24 | 0;
 i14 = i1 + 12 | 0;
 i15 = i1;
 HEAP32[652] = (HEAP32[652] | 0) + 1;
 i9 = i3 + 28 | 0;
 i31 = i3 + 56 | 0;
 HEAP32[i11 + 0 >> 2] = HEAP32[i31 + 0 >> 2];
 HEAP32[i11 + 4 >> 2] = HEAP32[i31 + 4 >> 2];
 HEAP32[i11 + 8 >> 2] = HEAP32[i31 + 8 >> 2];
 HEAP32[i11 + 12 >> 2] = HEAP32[i31 + 12 >> 2];
 i31 = i3 + 72 | 0;
 HEAP32[i10 + 0 >> 2] = HEAP32[i31 + 0 >> 2];
 HEAP32[i10 + 4 >> 2] = HEAP32[i31 + 4 >> 2];
 HEAP32[i10 + 8 >> 2] = HEAP32[i31 + 8 >> 2];
 HEAP32[i10 + 12 >> 2] = HEAP32[i31 + 12 >> 2];
 __ZN9b2Simplex9ReadCacheEPK14b2SimplexCachePK15b2DistanceProxyRK11b2TransformS5_S8_(i4, i5, i3, i11, i9, i10);
 i9 = i4 + 108 | 0;
 i31 = HEAP32[i9 >> 2] | 0;
 if ((i31 | 0) == 3 | (i31 | 0) == 2 | (i31 | 0) == 1) {
  i8 = i4 + 16 | 0;
  i6 = i4 + 20 | 0;
  d17 = +HEAPF32[i11 + 12 >> 2];
  d18 = +HEAPF32[i11 + 8 >> 2];
  i13 = i3 + 16 | 0;
  i12 = i3 + 20 | 0;
  d16 = +HEAPF32[i11 >> 2];
  d21 = +HEAPF32[i11 + 4 >> 2];
  d19 = +HEAPF32[i10 + 12 >> 2];
  d22 = +HEAPF32[i10 + 8 >> 2];
  i23 = i3 + 44 | 0;
  i20 = i3 + 48 | 0;
  d24 = +HEAPF32[i10 >> 2];
  d25 = +HEAPF32[i10 + 4 >> 2];
  i11 = i4 + 52 | 0;
  i10 = i4 + 56 | 0;
  i30 = i4 + 16 | 0;
  i27 = i4 + 36 | 0;
  i26 = i4 + 52 | 0;
  i29 = i4 + 24 | 0;
  i28 = i4 + 60 | 0;
  i33 = 0;
  L3 : while (1) {
   i32 = (i31 | 0) > 0;
   if (i32) {
    i34 = 0;
    do {
     HEAP32[i14 + (i34 << 2) >> 2] = HEAP32[i4 + (i34 * 36 | 0) + 28 >> 2];
     HEAP32[i15 + (i34 << 2) >> 2] = HEAP32[i4 + (i34 * 36 | 0) + 32 >> 2];
     i34 = i34 + 1 | 0;
    } while ((i34 | 0) != (i31 | 0));
   }
   do {
    if ((i31 | 0) == 2) {
     i46 = i30;
     d45 = +HEAPF32[i46 >> 2];
     d36 = +HEAPF32[i46 + 4 >> 2];
     i46 = i26;
     d38 = +HEAPF32[i46 >> 2];
     d37 = +HEAPF32[i46 + 4 >> 2];
     d43 = d38 - d45;
     d44 = d37 - d36;
     d36 = d45 * d43 + d36 * d44;
     if (d36 >= -0.0) {
      HEAPF32[i29 >> 2] = 1.0;
      HEAP32[i9 >> 2] = 1;
      i35 = 17;
      break;
     }
     d37 = d38 * d43 + d37 * d44;
     if (!(d37 <= 0.0)) {
      d45 = 1.0 / (d37 - d36);
      HEAPF32[i29 >> 2] = d37 * d45;
      HEAPF32[i28 >> 2] = -(d36 * d45);
      HEAP32[i9 >> 2] = 2;
      i35 = 18;
      break;
     } else {
      HEAPF32[i28 >> 2] = 1.0;
      HEAP32[i9 >> 2] = 1;
      i34 = i4 + 0 | 0;
      i39 = i27 + 0 | 0;
      i35 = i34 + 36 | 0;
      do {
       HEAP32[i34 >> 2] = HEAP32[i39 >> 2];
       i34 = i34 + 4 | 0;
       i39 = i39 + 4 | 0;
      } while ((i34 | 0) < (i35 | 0));
      i35 = 17;
      break;
     }
    } else if ((i31 | 0) == 3) {
     __ZN9b2Simplex6Solve3Ev(i4);
     i34 = HEAP32[i9 >> 2] | 0;
     if ((i34 | 0) == 1) {
      i35 = 17;
     } else if ((i34 | 0) == 0) {
      i35 = 15;
      break L3;
     } else if ((i34 | 0) == 2) {
      i35 = 18;
     } else if ((i34 | 0) == 3) {
      i35 = 42;
      break L3;
     } else {
      i35 = 16;
      break L3;
     }
    } else if ((i31 | 0) == 1) {
     i35 = 17;
    } else {
     i35 = 13;
     break L3;
    }
   } while (0);
   do {
    if ((i35 | 0) == 17) {
     d36 = -+HEAPF32[i8 >> 2];
     d37 = -+HEAPF32[i6 >> 2];
     i34 = 1;
    } else if ((i35 | 0) == 18) {
     d44 = +HEAPF32[i8 >> 2];
     d37 = +HEAPF32[i11 >> 2] - d44;
     d45 = +HEAPF32[i6 >> 2];
     d36 = +HEAPF32[i10 >> 2] - d45;
     if (d44 * d36 - d37 * d45 > 0.0) {
      d36 = -d36;
      i34 = 2;
      break;
     } else {
      d37 = -d37;
      i34 = 2;
      break;
     }
    }
   } while (0);
   if (d37 * d37 + d36 * d36 < 1.4210854715202004e-14) {
    i35 = 42;
    break;
   }
   i39 = i4 + (i34 * 36 | 0) | 0;
   d44 = -d36;
   d45 = -d37;
   d43 = d17 * d44 + d18 * d45;
   d44 = d17 * d45 - d18 * d44;
   i40 = HEAP32[i13 >> 2] | 0;
   i41 = HEAP32[i12 >> 2] | 0;
   if ((i41 | 0) > 1) {
    i42 = 0;
    d45 = d44 * +HEAPF32[i40 + 4 >> 2] + d43 * +HEAPF32[i40 >> 2];
    i46 = 1;
    while (1) {
     d38 = d43 * +HEAPF32[i40 + (i46 << 3) >> 2] + d44 * +HEAPF32[i40 + (i46 << 3) + 4 >> 2];
     i35 = d38 > d45;
     i42 = i35 ? i46 : i42;
     i46 = i46 + 1 | 0;
     if ((i46 | 0) == (i41 | 0)) {
      break;
     } else {
      d45 = i35 ? d38 : d45;
     }
    }
    i35 = i4 + (i34 * 36 | 0) + 28 | 0;
    HEAP32[i35 >> 2] = i42;
    if (!((i42 | 0) > -1)) {
     i35 = 28;
     break;
    }
   } else {
    i35 = i4 + (i34 * 36 | 0) + 28 | 0;
    HEAP32[i35 >> 2] = 0;
    i42 = 0;
   }
   if ((i41 | 0) <= (i42 | 0)) {
    i35 = 28;
    break;
   }
   d45 = +HEAPF32[i40 + (i42 << 3) >> 2];
   d43 = +HEAPF32[i40 + (i42 << 3) + 4 >> 2];
   d38 = d16 + (d17 * d45 - d18 * d43);
   d44 = +d38;
   d43 = +(d45 * d18 + d17 * d43 + d21);
   i40 = i39;
   HEAPF32[i40 >> 2] = d44;
   HEAPF32[i40 + 4 >> 2] = d43;
   d43 = d36 * d19 + d37 * d22;
   d44 = d37 * d19 - d36 * d22;
   i40 = HEAP32[i23 >> 2] | 0;
   i39 = HEAP32[i20 >> 2] | 0;
   if ((i39 | 0) > 1) {
    i41 = 0;
    d37 = d44 * +HEAPF32[i40 + 4 >> 2] + d43 * +HEAPF32[i40 >> 2];
    i42 = 1;
    while (1) {
     d36 = d43 * +HEAPF32[i40 + (i42 << 3) >> 2] + d44 * +HEAPF32[i40 + (i42 << 3) + 4 >> 2];
     i46 = d36 > d37;
     i41 = i46 ? i42 : i41;
     i42 = i42 + 1 | 0;
     if ((i42 | 0) == (i39 | 0)) {
      break;
     } else {
      d37 = i46 ? d36 : d37;
     }
    }
    i42 = i4 + (i34 * 36 | 0) + 32 | 0;
    HEAP32[i42 >> 2] = i41;
    if (!((i41 | 0) > -1)) {
     i35 = 35;
     break;
    }
   } else {
    i42 = i4 + (i34 * 36 | 0) + 32 | 0;
    HEAP32[i42 >> 2] = 0;
    i41 = 0;
   }
   if ((i39 | 0) <= (i41 | 0)) {
    i35 = 35;
    break;
   }
   d37 = +HEAPF32[i40 + (i41 << 3) >> 2];
   d45 = +HEAPF32[i40 + (i41 << 3) + 4 >> 2];
   d44 = d24 + (d19 * d37 - d22 * d45);
   d43 = +d44;
   d45 = +(d37 * d22 + d19 * d45 + d25);
   i46 = i4 + (i34 * 36 | 0) + 8 | 0;
   HEAPF32[i46 >> 2] = d43;
   HEAPF32[i46 + 4 >> 2] = d45;
   d44 = +(d44 - d38);
   d45 = +(+HEAPF32[i4 + (i34 * 36 | 0) + 12 >> 2] - +HEAPF32[i4 + (i34 * 36 | 0) + 4 >> 2]);
   i46 = i4 + (i34 * 36 | 0) + 16 | 0;
   HEAPF32[i46 >> 2] = d44;
   HEAPF32[i46 + 4 >> 2] = d45;
   i33 = i33 + 1 | 0;
   HEAP32[654] = (HEAP32[654] | 0) + 1;
   if (i32) {
    i34 = HEAP32[i35 >> 2] | 0;
    i32 = 0;
    do {
     if ((i34 | 0) == (HEAP32[i14 + (i32 << 2) >> 2] | 0) ? (HEAP32[i42 >> 2] | 0) == (HEAP32[i15 + (i32 << 2) >> 2] | 0) : 0) {
      i35 = 42;
      break L3;
     }
     i32 = i32 + 1 | 0;
    } while ((i32 | 0) < (i31 | 0));
   }
   i31 = (HEAP32[i9 >> 2] | 0) + 1 | 0;
   HEAP32[i9 >> 2] = i31;
   if ((i33 | 0) >= 20) {
    i35 = 42;
    break;
   }
  }
  if ((i35 | 0) == 13) {
   ___assert_fail(2712, 2672, 498, 2720);
  } else if ((i35 | 0) == 15) {
   ___assert_fail(2712, 2672, 194, 2856);
  } else if ((i35 | 0) == 16) {
   ___assert_fail(2712, 2672, 207, 2856);
  } else if ((i35 | 0) == 28) {
   ___assert_fail(2776, 2808, 103, 2840);
  } else if ((i35 | 0) == 35) {
   ___assert_fail(2776, 2808, 103, 2840);
  } else if ((i35 | 0) == 42) {
   i12 = HEAP32[656] | 0;
   HEAP32[656] = (i12 | 0) > (i33 | 0) ? i12 : i33;
   i14 = i2 + 8 | 0;
   __ZNK9b2Simplex16GetWitnessPointsEP6b2Vec2S1_(i4, i2, i14);
   d44 = +HEAPF32[i2 >> 2] - +HEAPF32[i14 >> 2];
   i13 = i2 + 4 | 0;
   i12 = i2 + 12 | 0;
   d45 = +HEAPF32[i13 >> 2] - +HEAPF32[i12 >> 2];
   i15 = i2 + 16 | 0;
   HEAPF32[i15 >> 2] = +Math_sqrt(+(d44 * d44 + d45 * d45));
   HEAP32[i2 + 20 >> 2] = i33;
   i9 = HEAP32[i9 >> 2] | 0;
   if ((i9 | 0) == 2) {
    d45 = +HEAPF32[i8 >> 2] - +HEAPF32[i11 >> 2];
    d7 = +HEAPF32[i6 >> 2] - +HEAPF32[i10 >> 2];
    d7 = +Math_sqrt(+(d45 * d45 + d7 * d7));
   } else if ((i9 | 0) == 3) {
    d7 = +HEAPF32[i8 >> 2];
    d45 = +HEAPF32[i6 >> 2];
    d7 = (+HEAPF32[i11 >> 2] - d7) * (+HEAPF32[i4 + 92 >> 2] - d45) - (+HEAPF32[i10 >> 2] - d45) * (+HEAPF32[i4 + 88 >> 2] - d7);
   } else if ((i9 | 0) == 1) {
    d7 = 0.0;
   } else if ((i9 | 0) == 0) {
    ___assert_fail(2712, 2672, 246, 2736);
   } else {
    ___assert_fail(2712, 2672, 259, 2736);
   }
   HEAPF32[i5 >> 2] = d7;
   HEAP16[i5 + 4 >> 1] = i9;
   i6 = 0;
   do {
    HEAP8[i5 + i6 + 6 | 0] = HEAP32[i4 + (i6 * 36 | 0) + 28 >> 2];
    HEAP8[i5 + i6 + 9 | 0] = HEAP32[i4 + (i6 * 36 | 0) + 32 >> 2];
    i6 = i6 + 1 | 0;
   } while ((i6 | 0) < (i9 | 0));
   if ((HEAP8[i3 + 88 | 0] | 0) == 0) {
    STACKTOP = i1;
    return;
   }
   d7 = +HEAPF32[i3 + 24 >> 2];
   d16 = +HEAPF32[i3 + 52 >> 2];
   d18 = +HEAPF32[i15 >> 2];
   d17 = d7 + d16;
   if (!(d18 > d17 & d18 > 1.1920928955078125e-7)) {
    d44 = +((+HEAPF32[i2 >> 2] + +HEAPF32[i14 >> 2]) * .5);
    d45 = +((+HEAPF32[i13 >> 2] + +HEAPF32[i12 >> 2]) * .5);
    i46 = i2;
    HEAPF32[i46 >> 2] = d44;
    HEAPF32[i46 + 4 >> 2] = d45;
    i46 = i14;
    HEAPF32[i46 >> 2] = d44;
    HEAPF32[i46 + 4 >> 2] = d45;
    HEAPF32[i15 >> 2] = 0.0;
    STACKTOP = i1;
    return;
   }
   HEAPF32[i15 >> 2] = d18 - d17;
   d18 = +HEAPF32[i14 >> 2];
   d21 = +HEAPF32[i2 >> 2];
   d24 = d18 - d21;
   d17 = +HEAPF32[i12 >> 2];
   d19 = +HEAPF32[i13 >> 2];
   d22 = d17 - d19;
   d25 = +Math_sqrt(+(d24 * d24 + d22 * d22));
   if (!(d25 < 1.1920928955078125e-7)) {
    d45 = 1.0 / d25;
    d24 = d24 * d45;
    d22 = d22 * d45;
   }
   HEAPF32[i2 >> 2] = d7 * d24 + d21;
   HEAPF32[i13 >> 2] = d7 * d22 + d19;
   HEAPF32[i14 >> 2] = d18 - d16 * d24;
   HEAPF32[i12 >> 2] = d17 - d16 * d22;
   STACKTOP = i1;
   return;
  }
 } else if ((i31 | 0) == 0) {
  ___assert_fail(2712, 2672, 194, 2856);
 } else {
  ___assert_fail(2712, 2672, 207, 2856);
 }
}
function __ZN8b2Island5SolveEP9b2ProfileRK10b2TimeStepRK6b2Vec2b(i4, i8, i11, i17, i7) {
 i4 = i4 | 0;
 i8 = i8 | 0;
 i11 = i11 | 0;
 i17 = i17 | 0;
 i7 = i7 | 0;
 var i1 = 0, i2 = 0, i3 = 0, d5 = 0.0, i6 = 0, i9 = 0, i10 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i18 = 0, i19 = 0, i20 = 0, d21 = 0.0, i22 = 0, d23 = 0.0, d24 = 0.0, d25 = 0.0, d26 = 0.0, d27 = 0.0, d28 = 0.0, d29 = 0.0, i30 = 0;
 i3 = STACKTOP;
 STACKTOP = STACKTOP + 160 | 0;
 i6 = i3 + 128 | 0;
 i9 = i3 + 148 | 0;
 i10 = i3 + 96 | 0;
 i16 = i3 + 52 | 0;
 i2 = i3;
 __ZN7b2TimerC2Ev(i9);
 d5 = +HEAPF32[i11 >> 2];
 i1 = i4 + 28 | 0;
 if ((HEAP32[i1 >> 2] | 0) > 0) {
  i13 = i4 + 8 | 0;
  i12 = i17 + 4 | 0;
  i15 = i4 + 20 | 0;
  i14 = i4 + 24 | 0;
  i19 = 0;
  do {
   i22 = HEAP32[(HEAP32[i13 >> 2] | 0) + (i19 << 2) >> 2] | 0;
   i18 = i22 + 44 | 0;
   i20 = HEAP32[i18 >> 2] | 0;
   i18 = HEAP32[i18 + 4 >> 2] | 0;
   d21 = +HEAPF32[i22 + 56 >> 2];
   i30 = i22 + 64 | 0;
   d27 = +HEAPF32[i30 >> 2];
   d24 = +HEAPF32[i30 + 4 >> 2];
   d23 = +HEAPF32[i22 + 72 >> 2];
   i30 = i22 + 36 | 0;
   HEAP32[i30 >> 2] = i20;
   HEAP32[i30 + 4 >> 2] = i18;
   HEAPF32[i22 + 52 >> 2] = d21;
   if ((HEAP32[i22 >> 2] | 0) == 2) {
    d25 = +HEAPF32[i22 + 140 >> 2];
    d26 = +HEAPF32[i22 + 120 >> 2];
    d28 = 1.0 - d5 * +HEAPF32[i22 + 132 >> 2];
    d28 = d28 < 1.0 ? d28 : 1.0;
    d28 = d28 < 0.0 ? 0.0 : d28;
    d29 = 1.0 - d5 * +HEAPF32[i22 + 136 >> 2];
    d29 = d29 < 1.0 ? d29 : 1.0;
    d27 = (d27 + d5 * (d25 * +HEAPF32[i17 >> 2] + d26 * +HEAPF32[i22 + 76 >> 2])) * d28;
    d24 = (d24 + d5 * (d25 * +HEAPF32[i12 >> 2] + d26 * +HEAPF32[i22 + 80 >> 2])) * d28;
    d23 = (d23 + d5 * +HEAPF32[i22 + 128 >> 2] * +HEAPF32[i22 + 84 >> 2]) * (d29 < 0.0 ? 0.0 : d29);
   }
   i30 = (HEAP32[i15 >> 2] | 0) + (i19 * 12 | 0) | 0;
   HEAP32[i30 >> 2] = i20;
   HEAP32[i30 + 4 >> 2] = i18;
   HEAPF32[(HEAP32[i15 >> 2] | 0) + (i19 * 12 | 0) + 8 >> 2] = d21;
   d28 = +d27;
   d29 = +d24;
   i30 = (HEAP32[i14 >> 2] | 0) + (i19 * 12 | 0) | 0;
   HEAPF32[i30 >> 2] = d28;
   HEAPF32[i30 + 4 >> 2] = d29;
   HEAPF32[(HEAP32[i14 >> 2] | 0) + (i19 * 12 | 0) + 8 >> 2] = d23;
   i19 = i19 + 1 | 0;
  } while ((i19 | 0) < (HEAP32[i1 >> 2] | 0));
 } else {
  i14 = i4 + 24 | 0;
  i15 = i4 + 20 | 0;
 }
 HEAP32[i10 + 0 >> 2] = HEAP32[i11 + 0 >> 2];
 HEAP32[i10 + 4 >> 2] = HEAP32[i11 + 4 >> 2];
 HEAP32[i10 + 8 >> 2] = HEAP32[i11 + 8 >> 2];
 HEAP32[i10 + 12 >> 2] = HEAP32[i11 + 12 >> 2];
 HEAP32[i10 + 16 >> 2] = HEAP32[i11 + 16 >> 2];
 HEAP32[i10 + 20 >> 2] = HEAP32[i11 + 20 >> 2];
 i22 = HEAP32[i15 >> 2] | 0;
 HEAP32[i10 + 24 >> 2] = i22;
 i30 = HEAP32[i14 >> 2] | 0;
 HEAP32[i10 + 28 >> 2] = i30;
 HEAP32[i16 + 0 >> 2] = HEAP32[i11 + 0 >> 2];
 HEAP32[i16 + 4 >> 2] = HEAP32[i11 + 4 >> 2];
 HEAP32[i16 + 8 >> 2] = HEAP32[i11 + 8 >> 2];
 HEAP32[i16 + 12 >> 2] = HEAP32[i11 + 12 >> 2];
 HEAP32[i16 + 16 >> 2] = HEAP32[i11 + 16 >> 2];
 HEAP32[i16 + 20 >> 2] = HEAP32[i11 + 20 >> 2];
 i13 = i4 + 12 | 0;
 HEAP32[i16 + 24 >> 2] = HEAP32[i13 >> 2];
 i12 = i4 + 36 | 0;
 HEAP32[i16 + 28 >> 2] = HEAP32[i12 >> 2];
 HEAP32[i16 + 32 >> 2] = i22;
 HEAP32[i16 + 36 >> 2] = i30;
 HEAP32[i16 + 40 >> 2] = HEAP32[i4 >> 2];
 __ZN15b2ContactSolverC2EP18b2ContactSolverDef(i2, i16);
 __ZN15b2ContactSolver29InitializeVelocityConstraintsEv(i2);
 if ((HEAP8[i11 + 20 | 0] | 0) != 0) {
  __ZN15b2ContactSolver9WarmStartEv(i2);
 }
 i16 = i4 + 32 | 0;
 if ((HEAP32[i16 >> 2] | 0) > 0) {
  i18 = i4 + 16 | 0;
  i17 = 0;
  do {
   i30 = HEAP32[(HEAP32[i18 >> 2] | 0) + (i17 << 2) >> 2] | 0;
   FUNCTION_TABLE_vii[HEAP32[(HEAP32[i30 >> 2] | 0) + 28 >> 2] & 15](i30, i10);
   i17 = i17 + 1 | 0;
  } while ((i17 | 0) < (HEAP32[i16 >> 2] | 0));
 }
 HEAPF32[i8 + 12 >> 2] = +__ZNK7b2Timer15GetMillisecondsEv(i9);
 i17 = i11 + 12 | 0;
 if ((HEAP32[i17 >> 2] | 0) > 0) {
  i20 = i4 + 16 | 0;
  i19 = 0;
  do {
   if ((HEAP32[i16 >> 2] | 0) > 0) {
    i18 = 0;
    do {
     i30 = HEAP32[(HEAP32[i20 >> 2] | 0) + (i18 << 2) >> 2] | 0;
     FUNCTION_TABLE_vii[HEAP32[(HEAP32[i30 >> 2] | 0) + 32 >> 2] & 15](i30, i10);
     i18 = i18 + 1 | 0;
    } while ((i18 | 0) < (HEAP32[i16 >> 2] | 0));
   }
   __ZN15b2ContactSolver24SolveVelocityConstraintsEv(i2);
   i19 = i19 + 1 | 0;
  } while ((i19 | 0) < (HEAP32[i17 >> 2] | 0));
 }
 __ZN15b2ContactSolver13StoreImpulsesEv(i2);
 HEAPF32[i8 + 16 >> 2] = +__ZNK7b2Timer15GetMillisecondsEv(i9);
 if ((HEAP32[i1 >> 2] | 0) > 0) {
  i19 = HEAP32[i14 >> 2] | 0;
  i18 = 0;
  do {
   i30 = HEAP32[i15 >> 2] | 0;
   i17 = i30 + (i18 * 12 | 0) | 0;
   i22 = i17;
   d23 = +HEAPF32[i22 >> 2];
   d21 = +HEAPF32[i22 + 4 >> 2];
   d24 = +HEAPF32[i30 + (i18 * 12 | 0) + 8 >> 2];
   i30 = i19 + (i18 * 12 | 0) | 0;
   d26 = +HEAPF32[i30 >> 2];
   d27 = +HEAPF32[i30 + 4 >> 2];
   d25 = +HEAPF32[i19 + (i18 * 12 | 0) + 8 >> 2];
   d29 = d5 * d26;
   d28 = d5 * d27;
   d28 = d29 * d29 + d28 * d28;
   if (d28 > 4.0) {
    d29 = 2.0 / +Math_sqrt(+d28);
    d26 = d26 * d29;
    d27 = d27 * d29;
   }
   d28 = d5 * d25;
   if (d28 * d28 > 2.4674012660980225) {
    if (!(d28 > 0.0)) {
     d28 = -d28;
    }
    d25 = d25 * (1.5707963705062866 / d28);
   }
   d29 = +(d23 + d5 * d26);
   d28 = +(d21 + d5 * d27);
   i19 = i17;
   HEAPF32[i19 >> 2] = d29;
   HEAPF32[i19 + 4 >> 2] = d28;
   HEAPF32[(HEAP32[i15 >> 2] | 0) + (i18 * 12 | 0) + 8 >> 2] = d24 + d5 * d25;
   d28 = +d26;
   d29 = +d27;
   i19 = (HEAP32[i14 >> 2] | 0) + (i18 * 12 | 0) | 0;
   HEAPF32[i19 >> 2] = d28;
   HEAPF32[i19 + 4 >> 2] = d29;
   i19 = HEAP32[i14 >> 2] | 0;
   HEAPF32[i19 + (i18 * 12 | 0) + 8 >> 2] = d25;
   i18 = i18 + 1 | 0;
  } while ((i18 | 0) < (HEAP32[i1 >> 2] | 0));
 }
 i11 = i11 + 16 | 0;
 L41 : do {
  if ((HEAP32[i11 >> 2] | 0) > 0) {
   i17 = i4 + 16 | 0;
   i19 = 0;
   while (1) {
    i18 = __ZN15b2ContactSolver24SolvePositionConstraintsEv(i2) | 0;
    if ((HEAP32[i16 >> 2] | 0) > 0) {
     i20 = 0;
     i22 = 1;
     do {
      i30 = HEAP32[(HEAP32[i17 >> 2] | 0) + (i20 << 2) >> 2] | 0;
      i22 = i22 & (FUNCTION_TABLE_iii[HEAP32[(HEAP32[i30 >> 2] | 0) + 36 >> 2] & 3](i30, i10) | 0);
      i20 = i20 + 1 | 0;
     } while ((i20 | 0) < (HEAP32[i16 >> 2] | 0));
    } else {
     i22 = 1;
    }
    i19 = i19 + 1 | 0;
    if (i18 & i22) {
     i10 = 0;
     break L41;
    }
    if ((i19 | 0) >= (HEAP32[i11 >> 2] | 0)) {
     i10 = 1;
     break;
    }
   }
  } else {
   i10 = 1;
  }
 } while (0);
 if ((HEAP32[i1 >> 2] | 0) > 0) {
  i11 = i4 + 8 | 0;
  i16 = 0;
  do {
   i30 = HEAP32[(HEAP32[i11 >> 2] | 0) + (i16 << 2) >> 2] | 0;
   i22 = (HEAP32[i15 >> 2] | 0) + (i16 * 12 | 0) | 0;
   i20 = HEAP32[i22 >> 2] | 0;
   i22 = HEAP32[i22 + 4 >> 2] | 0;
   i17 = i30 + 44 | 0;
   HEAP32[i17 >> 2] = i20;
   HEAP32[i17 + 4 >> 2] = i22;
   d27 = +HEAPF32[(HEAP32[i15 >> 2] | 0) + (i16 * 12 | 0) + 8 >> 2];
   HEAPF32[i30 + 56 >> 2] = d27;
   i17 = (HEAP32[i14 >> 2] | 0) + (i16 * 12 | 0) | 0;
   i18 = HEAP32[i17 + 4 >> 2] | 0;
   i19 = i30 + 64 | 0;
   HEAP32[i19 >> 2] = HEAP32[i17 >> 2];
   HEAP32[i19 + 4 >> 2] = i18;
   HEAPF32[i30 + 72 >> 2] = +HEAPF32[(HEAP32[i14 >> 2] | 0) + (i16 * 12 | 0) + 8 >> 2];
   d25 = +Math_sin(+d27);
   HEAPF32[i30 + 20 >> 2] = d25;
   d27 = +Math_cos(+d27);
   HEAPF32[i30 + 24 >> 2] = d27;
   d26 = +HEAPF32[i30 + 28 >> 2];
   d29 = +HEAPF32[i30 + 32 >> 2];
   d28 = (HEAP32[tempDoublePtr >> 2] = i20, +HEAPF32[tempDoublePtr >> 2]) - (d27 * d26 - d25 * d29);
   d29 = (HEAP32[tempDoublePtr >> 2] = i22, +HEAPF32[tempDoublePtr >> 2]) - (d25 * d26 + d27 * d29);
   d28 = +d28;
   d29 = +d29;
   i30 = i30 + 12 | 0;
   HEAPF32[i30 >> 2] = d28;
   HEAPF32[i30 + 4 >> 2] = d29;
   i16 = i16 + 1 | 0;
  } while ((i16 | 0) < (HEAP32[i1 >> 2] | 0));
 }
 HEAPF32[i8 + 20 >> 2] = +__ZNK7b2Timer15GetMillisecondsEv(i9);
 i9 = HEAP32[i2 + 40 >> 2] | 0;
 i8 = i4 + 4 | 0;
 if ((HEAP32[i8 >> 2] | 0) != 0 ? (HEAP32[i12 >> 2] | 0) > 0 : 0) {
  i11 = i6 + 16 | 0;
  i14 = 0;
  do {
   i15 = HEAP32[(HEAP32[i13 >> 2] | 0) + (i14 << 2) >> 2] | 0;
   i16 = HEAP32[i9 + (i14 * 152 | 0) + 144 >> 2] | 0;
   HEAP32[i11 >> 2] = i16;
   if ((i16 | 0) > 0) {
    i17 = 0;
    do {
     HEAPF32[i6 + (i17 << 2) >> 2] = +HEAPF32[i9 + (i14 * 152 | 0) + (i17 * 36 | 0) + 16 >> 2];
     HEAPF32[i6 + (i17 << 2) + 8 >> 2] = +HEAPF32[i9 + (i14 * 152 | 0) + (i17 * 36 | 0) + 20 >> 2];
     i17 = i17 + 1 | 0;
    } while ((i17 | 0) != (i16 | 0));
   }
   i30 = HEAP32[i8 >> 2] | 0;
   FUNCTION_TABLE_viii[HEAP32[(HEAP32[i30 >> 2] | 0) + 20 >> 2] & 3](i30, i15, i6);
   i14 = i14 + 1 | 0;
  } while ((i14 | 0) < (HEAP32[i12 >> 2] | 0));
 }
 if (!i7) {
  __ZN15b2ContactSolverD2Ev(i2);
  STACKTOP = i3;
  return;
 }
 i7 = HEAP32[i1 >> 2] | 0;
 i6 = (i7 | 0) > 0;
 if (i6) {
  i8 = HEAP32[i4 + 8 >> 2] | 0;
  i9 = 0;
  d21 = 3.4028234663852886e+38;
  do {
   i11 = HEAP32[i8 + (i9 << 2) >> 2] | 0;
   do {
    if ((HEAP32[i11 >> 2] | 0) != 0) {
     if ((!((HEAP16[i11 + 4 >> 1] & 4) == 0) ? (d29 = +HEAPF32[i11 + 72 >> 2], !(d29 * d29 > .001218469929881394)) : 0) ? (d28 = +HEAPF32[i11 + 64 >> 2], d29 = +HEAPF32[i11 + 68 >> 2], !(d28 * d28 + d29 * d29 > 9999999747378752.0e-20)) : 0) {
      i30 = i11 + 144 | 0;
      d23 = d5 + +HEAPF32[i30 >> 2];
      HEAPF32[i30 >> 2] = d23;
      d21 = d21 < d23 ? d21 : d23;
      break;
     }
     HEAPF32[i11 + 144 >> 2] = 0.0;
     d21 = 0.0;
    }
   } while (0);
   i9 = i9 + 1 | 0;
  } while ((i9 | 0) < (i7 | 0));
 } else {
  d21 = 3.4028234663852886e+38;
 }
 if (!(d21 >= .5) | i10 | i6 ^ 1) {
  __ZN15b2ContactSolverD2Ev(i2);
  STACKTOP = i3;
  return;
 }
 i4 = i4 + 8 | 0;
 i6 = 0;
 do {
  i30 = HEAP32[(HEAP32[i4 >> 2] | 0) + (i6 << 2) >> 2] | 0;
  i22 = i30 + 4 | 0;
  HEAP16[i22 >> 1] = HEAP16[i22 >> 1] & 65533;
  HEAPF32[i30 + 144 >> 2] = 0.0;
  i30 = i30 + 64 | 0;
  HEAP32[i30 + 0 >> 2] = 0;
  HEAP32[i30 + 4 >> 2] = 0;
  HEAP32[i30 + 8 >> 2] = 0;
  HEAP32[i30 + 12 >> 2] = 0;
  HEAP32[i30 + 16 >> 2] = 0;
  HEAP32[i30 + 20 >> 2] = 0;
  i6 = i6 + 1 | 0;
 } while ((i6 | 0) < (HEAP32[i1 >> 2] | 0));
 __ZN15b2ContactSolverD2Ev(i2);
 STACKTOP = i3;
 return;
}
function __ZN15b2ContactSolver24SolveVelocityConstraintsEv(i4) {
 i4 = i4 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, d9 = 0.0, d10 = 0.0, d11 = 0.0, d12 = 0.0, d13 = 0.0, d14 = 0.0, d15 = 0.0, d16 = 0.0, d17 = 0.0, d18 = 0.0, i19 = 0, d20 = 0.0, d21 = 0.0, i22 = 0, d23 = 0.0, d24 = 0.0, d25 = 0.0, d26 = 0.0, d27 = 0.0, d28 = 0.0, d29 = 0.0, d30 = 0.0, d31 = 0.0, i32 = 0, i33 = 0, d34 = 0.0, d35 = 0.0, d36 = 0.0, d37 = 0.0, d38 = 0.0, d39 = 0.0, d40 = 0.0, i41 = 0, i42 = 0, d43 = 0.0, d44 = 0.0;
 i1 = STACKTOP;
 i2 = i4 + 48 | 0;
 if ((HEAP32[i2 >> 2] | 0) <= 0) {
  STACKTOP = i1;
  return;
 }
 i3 = i4 + 40 | 0;
 i4 = i4 + 28 | 0;
 i42 = HEAP32[i4 >> 2] | 0;
 i5 = 0;
 L4 : while (1) {
  i19 = HEAP32[i3 >> 2] | 0;
  i22 = i19 + (i5 * 152 | 0) | 0;
  i8 = HEAP32[i19 + (i5 * 152 | 0) + 112 >> 2] | 0;
  i6 = HEAP32[i19 + (i5 * 152 | 0) + 116 >> 2] | 0;
  d12 = +HEAPF32[i19 + (i5 * 152 | 0) + 120 >> 2];
  d10 = +HEAPF32[i19 + (i5 * 152 | 0) + 128 >> 2];
  d11 = +HEAPF32[i19 + (i5 * 152 | 0) + 124 >> 2];
  d9 = +HEAPF32[i19 + (i5 * 152 | 0) + 132 >> 2];
  i32 = i19 + (i5 * 152 | 0) + 144 | 0;
  i33 = HEAP32[i32 >> 2] | 0;
  i7 = i42 + (i8 * 12 | 0) | 0;
  i41 = i7;
  d21 = +HEAPF32[i41 >> 2];
  d20 = +HEAPF32[i41 + 4 >> 2];
  i41 = i42 + (i6 * 12 | 0) | 0;
  d14 = +HEAPF32[i41 >> 2];
  d13 = +HEAPF32[i41 + 4 >> 2];
  i41 = i19 + (i5 * 152 | 0) + 72 | 0;
  d17 = +HEAPF32[i41 >> 2];
  d16 = +HEAPF32[i41 + 4 >> 2];
  d23 = -d17;
  d24 = +HEAPF32[i19 + (i5 * 152 | 0) + 136 >> 2];
  if ((i33 + -1 | 0) >>> 0 < 2) {
   i41 = 0;
   d18 = +HEAPF32[i42 + (i8 * 12 | 0) + 8 >> 2];
   d15 = +HEAPF32[i42 + (i6 * 12 | 0) + 8 >> 2];
  } else {
   i2 = 4;
   break;
  }
  do {
   d30 = +HEAPF32[i19 + (i5 * 152 | 0) + (i41 * 36 | 0) + 12 >> 2];
   d25 = +HEAPF32[i19 + (i5 * 152 | 0) + (i41 * 36 | 0) + 8 >> 2];
   d26 = +HEAPF32[i19 + (i5 * 152 | 0) + (i41 * 36 | 0) + 4 >> 2];
   d27 = +HEAPF32[i19 + (i5 * 152 | 0) + (i41 * 36 | 0) >> 2];
   d34 = d24 * +HEAPF32[i19 + (i5 * 152 | 0) + (i41 * 36 | 0) + 16 >> 2];
   i42 = i19 + (i5 * 152 | 0) + (i41 * 36 | 0) + 20 | 0;
   d28 = +HEAPF32[i42 >> 2];
   d31 = d28 - +HEAPF32[i19 + (i5 * 152 | 0) + (i41 * 36 | 0) + 28 >> 2] * (d16 * (d14 - d15 * d30 - d21 + d18 * d26) + (d13 + d15 * d25 - d20 - d18 * d27) * d23);
   d29 = -d34;
   d31 = d31 < d34 ? d31 : d34;
   d40 = d31 < d29 ? d29 : d31;
   d39 = d40 - d28;
   HEAPF32[i42 >> 2] = d40;
   d40 = d16 * d39;
   d39 = d39 * d23;
   d21 = d21 - d12 * d40;
   d20 = d20 - d12 * d39;
   d18 = d18 - d10 * (d27 * d39 - d26 * d40);
   d14 = d14 + d11 * d40;
   d13 = d13 + d11 * d39;
   d15 = d15 + d9 * (d25 * d39 - d30 * d40);
   i41 = i41 + 1 | 0;
  } while ((i41 | 0) != (i33 | 0));
  do {
   if ((HEAP32[i32 >> 2] | 0) != 1) {
    i32 = i19 + (i5 * 152 | 0) + 16 | 0;
    d31 = +HEAPF32[i32 >> 2];
    i33 = i19 + (i5 * 152 | 0) + 52 | 0;
    d34 = +HEAPF32[i33 >> 2];
    if (!(d31 >= 0.0) | !(d34 >= 0.0)) {
     i2 = 9;
     break L4;
    }
    d23 = +HEAPF32[i19 + (i5 * 152 | 0) + 12 >> 2];
    d24 = +HEAPF32[i19 + (i5 * 152 | 0) + 8 >> 2];
    d26 = +HEAPF32[i19 + (i5 * 152 | 0) + 4 >> 2];
    d30 = +HEAPF32[i22 >> 2];
    d27 = +HEAPF32[i19 + (i5 * 152 | 0) + 48 >> 2];
    d25 = +HEAPF32[i19 + (i5 * 152 | 0) + 44 >> 2];
    d28 = +HEAPF32[i19 + (i5 * 152 | 0) + 40 >> 2];
    d29 = +HEAPF32[i19 + (i5 * 152 | 0) + 36 >> 2];
    d37 = +HEAPF32[i19 + (i5 * 152 | 0) + 104 >> 2];
    d38 = +HEAPF32[i19 + (i5 * 152 | 0) + 100 >> 2];
    d35 = d17 * (d14 - d15 * d23 - d21 + d18 * d26) + d16 * (d13 + d15 * d24 - d20 - d18 * d30) - +HEAPF32[i19 + (i5 * 152 | 0) + 32 >> 2] - (d31 * +HEAPF32[i19 + (i5 * 152 | 0) + 96 >> 2] + d34 * d37);
    d36 = d17 * (d14 - d15 * d27 - d21 + d18 * d28) + d16 * (d13 + d15 * d25 - d20 - d18 * d29) - +HEAPF32[i19 + (i5 * 152 | 0) + 68 >> 2] - (d31 * d38 + d34 * +HEAPF32[i19 + (i5 * 152 | 0) + 108 >> 2]);
    d44 = +HEAPF32[i19 + (i5 * 152 | 0) + 80 >> 2] * d35 + +HEAPF32[i19 + (i5 * 152 | 0) + 88 >> 2] * d36;
    d43 = d35 * +HEAPF32[i19 + (i5 * 152 | 0) + 84 >> 2] + d36 * +HEAPF32[i19 + (i5 * 152 | 0) + 92 >> 2];
    d40 = -d44;
    d39 = -d43;
    if (!(!(d44 <= -0.0) | !(d43 <= -0.0))) {
     d37 = d40 - d31;
     d43 = d39 - d34;
     d38 = d17 * d37;
     d37 = d16 * d37;
     d44 = d17 * d43;
     d43 = d16 * d43;
     d35 = d38 + d44;
     d36 = d37 + d43;
     HEAPF32[i32 >> 2] = d40;
     HEAPF32[i33 >> 2] = d39;
     d21 = d21 - d12 * d35;
     d20 = d20 - d12 * d36;
     d14 = d14 + d11 * d35;
     d13 = d13 + d11 * d36;
     d18 = d18 - d10 * (d30 * d37 - d26 * d38 + (d29 * d43 - d28 * d44));
     d15 = d15 + d9 * (d24 * d37 - d23 * d38 + (d25 * d43 - d27 * d44));
     break;
    }
    d44 = d35 * +HEAPF32[i19 + (i5 * 152 | 0) + 24 >> 2];
    d39 = -d44;
    if (d44 <= -0.0 ? d36 + d38 * d39 >= 0.0 : 0) {
     d38 = d39 - d31;
     d43 = 0.0 - d34;
     d40 = d17 * d38;
     d38 = d16 * d38;
     d44 = d17 * d43;
     d43 = d16 * d43;
     d36 = d44 + d40;
     d37 = d43 + d38;
     HEAPF32[i32 >> 2] = d39;
     HEAPF32[i33 >> 2] = 0.0;
     d21 = d21 - d12 * d36;
     d20 = d20 - d12 * d37;
     d14 = d14 + d11 * d36;
     d13 = d13 + d11 * d37;
     d18 = d18 - d10 * (d38 * d30 - d40 * d26 + (d43 * d29 - d44 * d28));
     d15 = d15 + d9 * (d38 * d24 - d40 * d23 + (d43 * d25 - d44 * d27));
     break;
    }
    d44 = d36 * +HEAPF32[i19 + (i5 * 152 | 0) + 60 >> 2];
    d38 = -d44;
    if (d44 <= -0.0 ? d35 + d37 * d38 >= 0.0 : 0) {
     d39 = 0.0 - d31;
     d43 = d38 - d34;
     d40 = d17 * d39;
     d39 = d16 * d39;
     d44 = d17 * d43;
     d43 = d16 * d43;
     d36 = d40 + d44;
     d37 = d39 + d43;
     HEAPF32[i32 >> 2] = 0.0;
     HEAPF32[i33 >> 2] = d38;
     d21 = d21 - d12 * d36;
     d20 = d20 - d12 * d37;
     d14 = d14 + d11 * d36;
     d13 = d13 + d11 * d37;
     d18 = d18 - d10 * (d39 * d30 - d40 * d26 + (d43 * d29 - d44 * d28));
     d15 = d15 + d9 * (d39 * d24 - d40 * d23 + (d43 * d25 - d44 * d27));
     break;
    }
    if (!(!(d35 >= 0.0) | !(d36 >= 0.0))) {
     d39 = 0.0 - d31;
     d43 = 0.0 - d34;
     d40 = d17 * d39;
     d39 = d16 * d39;
     d44 = d17 * d43;
     d43 = d16 * d43;
     d37 = d40 + d44;
     d38 = d39 + d43;
     HEAPF32[i32 >> 2] = 0.0;
     HEAPF32[i33 >> 2] = 0.0;
     d21 = d21 - d12 * d37;
     d20 = d20 - d12 * d38;
     d14 = d14 + d11 * d37;
     d13 = d13 + d11 * d38;
     d18 = d18 - d10 * (d39 * d30 - d40 * d26 + (d43 * d29 - d44 * d28));
     d15 = d15 + d9 * (d39 * d24 - d40 * d23 + (d43 * d25 - d44 * d27));
    }
   } else {
    d23 = +HEAPF32[i19 + (i5 * 152 | 0) + 12 >> 2];
    d24 = +HEAPF32[i19 + (i5 * 152 | 0) + 8 >> 2];
    d25 = +HEAPF32[i19 + (i5 * 152 | 0) + 4 >> 2];
    d26 = +HEAPF32[i22 >> 2];
    i22 = i19 + (i5 * 152 | 0) + 16 | 0;
    d27 = +HEAPF32[i22 >> 2];
    d28 = d27 - +HEAPF32[i19 + (i5 * 152 | 0) + 24 >> 2] * (d17 * (d14 - d15 * d23 - d21 + d18 * d25) + d16 * (d13 + d15 * d24 - d20 - d18 * d26) - +HEAPF32[i19 + (i5 * 152 | 0) + 32 >> 2]);
    d44 = d28 > 0.0 ? d28 : 0.0;
    d43 = d44 - d27;
    HEAPF32[i22 >> 2] = d44;
    d44 = d17 * d43;
    d43 = d16 * d43;
    d21 = d21 - d12 * d44;
    d20 = d20 - d12 * d43;
    d14 = d14 + d11 * d44;
    d13 = d13 + d11 * d43;
    d18 = d18 - d10 * (d26 * d43 - d25 * d44);
    d15 = d15 + d9 * (d24 * d43 - d23 * d44);
   }
  } while (0);
  d44 = +d21;
  d43 = +d20;
  i42 = i7;
  HEAPF32[i42 >> 2] = d44;
  HEAPF32[i42 + 4 >> 2] = d43;
  i42 = HEAP32[i4 >> 2] | 0;
  HEAPF32[i42 + (i8 * 12 | 0) + 8 >> 2] = d18;
  d43 = +d14;
  d44 = +d13;
  i42 = i42 + (i6 * 12 | 0) | 0;
  HEAPF32[i42 >> 2] = d43;
  HEAPF32[i42 + 4 >> 2] = d44;
  i42 = HEAP32[i4 >> 2] | 0;
  HEAPF32[i42 + (i6 * 12 | 0) + 8 >> 2] = d15;
  i5 = i5 + 1 | 0;
  if ((i5 | 0) >= (HEAP32[i2 >> 2] | 0)) {
   i2 = 21;
   break;
  }
 }
 if ((i2 | 0) == 4) {
  ___assert_fail(6648, 6520, 311, 6688);
 } else if ((i2 | 0) == 9) {
  ___assert_fail(6720, 6520, 406, 6688);
 } else if ((i2 | 0) == 21) {
  STACKTOP = i1;
  return;
 }
}
function __Z14b2TimeOfImpactP11b2TOIOutputPK10b2TOIInput(i3, i11) {
 i3 = i3 | 0;
 i11 = i11 | 0;
 var i1 = 0, i2 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i12 = 0, i13 = 0, d14 = 0.0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, d28 = 0.0, i29 = 0, d30 = 0.0, d31 = 0.0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, i38 = 0, i39 = 0, d40 = 0.0, i41 = 0, d42 = 0.0, d43 = 0.0, i44 = 0, i45 = 0, d46 = 0.0, i47 = 0, d48 = 0.0, d49 = 0.0, d50 = 0.0, d51 = 0.0, i52 = 0, d53 = 0.0, d54 = 0.0, d55 = 0.0, d56 = 0.0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 320 | 0;
 i12 = i1 + 276 | 0;
 i10 = i1 + 240 | 0;
 i13 = i1 + 228 | 0;
 i5 = i1 + 136 | 0;
 i7 = i1 + 112 | 0;
 i8 = i1 + 8 | 0;
 i9 = i1 + 4 | 0;
 i4 = i1;
 HEAP32[874] = (HEAP32[874] | 0) + 1;
 HEAP32[i3 >> 2] = 0;
 i19 = i11 + 128 | 0;
 i2 = i3 + 4 | 0;
 HEAPF32[i2 >> 2] = +HEAPF32[i19 >> 2];
 i6 = i11 + 28 | 0;
 i16 = i12 + 0 | 0;
 i15 = i11 + 56 | 0;
 i17 = i16 + 36 | 0;
 do {
  HEAP32[i16 >> 2] = HEAP32[i15 >> 2];
  i16 = i16 + 4 | 0;
  i15 = i15 + 4 | 0;
 } while ((i16 | 0) < (i17 | 0));
 i16 = i10 + 0 | 0;
 i15 = i11 + 92 | 0;
 i17 = i16 + 36 | 0;
 do {
  HEAP32[i16 >> 2] = HEAP32[i15 >> 2];
  i16 = i16 + 4 | 0;
  i15 = i15 + 4 | 0;
 } while ((i16 | 0) < (i17 | 0));
 i15 = i12 + 24 | 0;
 d42 = +HEAPF32[i15 >> 2];
 d43 = +Math_floor(+(d42 / 6.2831854820251465)) * 6.2831854820251465;
 d42 = d42 - d43;
 HEAPF32[i15 >> 2] = d42;
 i16 = i12 + 28 | 0;
 d43 = +HEAPF32[i16 >> 2] - d43;
 HEAPF32[i16 >> 2] = d43;
 i17 = i10 + 24 | 0;
 d46 = +HEAPF32[i17 >> 2];
 d40 = +Math_floor(+(d46 / 6.2831854820251465)) * 6.2831854820251465;
 d46 = d46 - d40;
 HEAPF32[i17 >> 2] = d46;
 i18 = i10 + 28 | 0;
 d40 = +HEAPF32[i18 >> 2] - d40;
 HEAPF32[i18 >> 2] = d40;
 d14 = +HEAPF32[i19 >> 2];
 d28 = +HEAPF32[i11 + 24 >> 2] + +HEAPF32[i11 + 52 >> 2] + -.014999999664723873;
 d28 = d28 < .004999999888241291 ? .004999999888241291 : d28;
 if (!(d28 > .0012499999720603228)) {
  ___assert_fail(3536, 3560, 280, 3600);
 }
 HEAP16[i13 + 4 >> 1] = 0;
 HEAP32[i5 + 0 >> 2] = HEAP32[i11 + 0 >> 2];
 HEAP32[i5 + 4 >> 2] = HEAP32[i11 + 4 >> 2];
 HEAP32[i5 + 8 >> 2] = HEAP32[i11 + 8 >> 2];
 HEAP32[i5 + 12 >> 2] = HEAP32[i11 + 12 >> 2];
 HEAP32[i5 + 16 >> 2] = HEAP32[i11 + 16 >> 2];
 HEAP32[i5 + 20 >> 2] = HEAP32[i11 + 20 >> 2];
 HEAP32[i5 + 24 >> 2] = HEAP32[i11 + 24 >> 2];
 i38 = i5 + 28 | 0;
 HEAP32[i38 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
 HEAP32[i38 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
 HEAP32[i38 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
 HEAP32[i38 + 12 >> 2] = HEAP32[i6 + 12 >> 2];
 HEAP32[i38 + 16 >> 2] = HEAP32[i6 + 16 >> 2];
 HEAP32[i38 + 20 >> 2] = HEAP32[i6 + 20 >> 2];
 HEAP32[i38 + 24 >> 2] = HEAP32[i6 + 24 >> 2];
 HEAP8[i5 + 88 | 0] = 0;
 i38 = i12 + 8 | 0;
 i27 = i12 + 12 | 0;
 i29 = i12 + 16 | 0;
 i22 = i12 + 20 | 0;
 i32 = i12 + 4 | 0;
 i34 = i10 + 8 | 0;
 i36 = i10 + 12 | 0;
 i35 = i10 + 16 | 0;
 i37 = i10 + 20 | 0;
 i33 = i10 + 4 | 0;
 i26 = i5 + 56 | 0;
 i25 = i5 + 64 | 0;
 i24 = i5 + 68 | 0;
 i23 = i5 + 72 | 0;
 i20 = i5 + 80 | 0;
 i19 = i5 + 84 | 0;
 i21 = i7 + 16 | 0;
 d30 = d28 + .0012499999720603228;
 d31 = d28 + -.0012499999720603228;
 d48 = d40;
 i39 = 0;
 d40 = 0.0;
 L4 : while (1) {
  d56 = 1.0 - d40;
  d49 = d56 * d42 + d40 * d43;
  d43 = +Math_sin(+d49);
  d49 = +Math_cos(+d49);
  d55 = +HEAPF32[i12 >> 2];
  d54 = +HEAPF32[i32 >> 2];
  d42 = d56 * d46 + d40 * d48;
  d53 = +Math_sin(+d42);
  d42 = +Math_cos(+d42);
  d46 = +HEAPF32[i10 >> 2];
  d51 = +HEAPF32[i33 >> 2];
  d50 = d56 * +HEAPF32[i34 >> 2] + d40 * +HEAPF32[i35 >> 2] - (d42 * d46 - d53 * d51);
  d51 = d56 * +HEAPF32[i36 >> 2] + d40 * +HEAPF32[i37 >> 2] - (d53 * d46 + d42 * d51);
  d46 = +(d56 * +HEAPF32[i38 >> 2] + d40 * +HEAPF32[i29 >> 2] - (d49 * d55 - d43 * d54));
  d48 = +(d56 * +HEAPF32[i27 >> 2] + d40 * +HEAPF32[i22 >> 2] - (d43 * d55 + d49 * d54));
  i52 = i26;
  HEAPF32[i52 >> 2] = d46;
  HEAPF32[i52 + 4 >> 2] = d48;
  HEAPF32[i25 >> 2] = d43;
  HEAPF32[i24 >> 2] = d49;
  d50 = +d50;
  d51 = +d51;
  i52 = i23;
  HEAPF32[i52 >> 2] = d50;
  HEAPF32[i52 + 4 >> 2] = d51;
  HEAPF32[i20 >> 2] = d53;
  HEAPF32[i19 >> 2] = d42;
  __Z10b2DistanceP16b2DistanceOutputP14b2SimplexCachePK15b2DistanceInput(i7, i13, i5);
  d42 = +HEAPF32[i21 >> 2];
  if (d42 <= 0.0) {
   i4 = 5;
   break;
  }
  if (d42 < d30) {
   i4 = 7;
   break;
  }
  +__ZN20b2SeparationFunction10InitializeEPK14b2SimplexCachePK15b2DistanceProxyRK7b2SweepS5_S8_f(i8, i13, i11, i12, i6, i10, d40);
  i41 = 0;
  d42 = d14;
  do {
   d50 = +__ZNK20b2SeparationFunction17FindMinSeparationEPiS0_f(i8, i9, i4, d42);
   if (d50 > d30) {
    i4 = 10;
    break L4;
   }
   if (d50 > d31) {
    d40 = d42;
    break;
   }
   i45 = HEAP32[i9 >> 2] | 0;
   i44 = HEAP32[i4 >> 2] | 0;
   d48 = +__ZNK20b2SeparationFunction8EvaluateEiif(i8, i45, i44, d40);
   if (d48 < d31) {
    i4 = 13;
    break L4;
   }
   if (!(d48 <= d30)) {
    d43 = d40;
    d46 = d42;
    i47 = 0;
   } else {
    i4 = 15;
    break L4;
   }
   while (1) {
    if ((i47 & 1 | 0) == 0) {
     d49 = (d43 + d46) * .5;
    } else {
     d49 = d43 + (d28 - d48) * (d46 - d43) / (d50 - d48);
    }
    d51 = +__ZNK20b2SeparationFunction8EvaluateEiif(i8, i45, i44, d49);
    d53 = d51 - d28;
    if (!(d53 > 0.0)) {
     d53 = -d53;
    }
    if (d53 < .0012499999720603228) {
     d42 = d49;
     break;
    }
    i52 = d51 > d28;
    i47 = i47 + 1 | 0;
    HEAP32[880] = (HEAP32[880] | 0) + 1;
    if ((i47 | 0) == 50) {
     i47 = 50;
     break;
    } else {
     d43 = i52 ? d49 : d43;
     d46 = i52 ? d46 : d49;
     d48 = i52 ? d51 : d48;
     d50 = i52 ? d50 : d51;
    }
   }
   i44 = HEAP32[882] | 0;
   HEAP32[882] = (i44 | 0) > (i47 | 0) ? i44 : i47;
   i41 = i41 + 1 | 0;
  } while ((i41 | 0) != 8);
  i39 = i39 + 1 | 0;
  HEAP32[876] = (HEAP32[876] | 0) + 1;
  if ((i39 | 0) == 20) {
   i4 = 27;
   break;
  }
  d42 = +HEAPF32[i15 >> 2];
  d43 = +HEAPF32[i16 >> 2];
  d46 = +HEAPF32[i17 >> 2];
  d48 = +HEAPF32[i18 >> 2];
 }
 if ((i4 | 0) == 5) {
  HEAP32[i3 >> 2] = 2;
  HEAPF32[i2 >> 2] = 0.0;
  i2 = HEAP32[878] | 0;
  i52 = (i2 | 0) > (i39 | 0);
  i52 = i52 ? i2 : i39;
  HEAP32[878] = i52;
  STACKTOP = i1;
  return;
 } else if ((i4 | 0) == 7) {
  HEAP32[i3 >> 2] = 3;
  HEAPF32[i2 >> 2] = d40;
  i2 = HEAP32[878] | 0;
  i52 = (i2 | 0) > (i39 | 0);
  i52 = i52 ? i2 : i39;
  HEAP32[878] = i52;
  STACKTOP = i1;
  return;
 } else if ((i4 | 0) == 10) {
  HEAP32[i3 >> 2] = 4;
  HEAPF32[i2 >> 2] = d14;
 } else if ((i4 | 0) == 13) {
  HEAP32[i3 >> 2] = 1;
  HEAPF32[i2 >> 2] = d40;
 } else if ((i4 | 0) == 15) {
  HEAP32[i3 >> 2] = 3;
  HEAPF32[i2 >> 2] = d40;
 } else if ((i4 | 0) == 27) {
  HEAP32[i3 >> 2] = 1;
  HEAPF32[i2 >> 2] = d40;
  i39 = 20;
  i2 = HEAP32[878] | 0;
  i52 = (i2 | 0) > (i39 | 0);
  i52 = i52 ? i2 : i39;
  HEAP32[878] = i52;
  STACKTOP = i1;
  return;
 }
 HEAP32[876] = (HEAP32[876] | 0) + 1;
 i39 = i39 + 1 | 0;
 i2 = HEAP32[878] | 0;
 i52 = (i2 | 0) > (i39 | 0);
 i52 = i52 ? i2 : i39;
 HEAP32[878] = i52;
 STACKTOP = i1;
 return;
}
function __ZN7b2World5SolveERK10b2TimeStep(i5, i15) {
 i5 = i5 | 0;
 i15 = i15 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, i38 = 0, d39 = 0.0;
 i3 = STACKTOP;
 STACKTOP = STACKTOP + 96 | 0;
 i4 = i3 + 32 | 0;
 i9 = i3;
 i2 = i3 + 84 | 0;
 i11 = i5 + 103008 | 0;
 HEAPF32[i11 >> 2] = 0.0;
 i14 = i5 + 103012 | 0;
 HEAPF32[i14 >> 2] = 0.0;
 i8 = i5 + 103016 | 0;
 HEAPF32[i8 >> 2] = 0.0;
 i16 = i5 + 102960 | 0;
 i1 = i5 + 102872 | 0;
 i6 = i5 + 68 | 0;
 __ZN8b2IslandC2EiiiP16b2StackAllocatorP17b2ContactListener(i4, HEAP32[i16 >> 2] | 0, HEAP32[i5 + 102936 >> 2] | 0, HEAP32[i5 + 102964 >> 2] | 0, i6, HEAP32[i5 + 102944 >> 2] | 0);
 i7 = i5 + 102952 | 0;
 i17 = HEAP32[i7 >> 2] | 0;
 if ((i17 | 0) != 0) {
  do {
   i38 = i17 + 4 | 0;
   HEAP16[i38 >> 1] = HEAP16[i38 >> 1] & 65534;
   i17 = HEAP32[i17 + 96 >> 2] | 0;
  } while ((i17 | 0) != 0);
 }
 i17 = HEAP32[i5 + 102932 >> 2] | 0;
 if ((i17 | 0) != 0) {
  do {
   i38 = i17 + 4 | 0;
   HEAP32[i38 >> 2] = HEAP32[i38 >> 2] & -2;
   i17 = HEAP32[i17 + 12 >> 2] | 0;
  } while ((i17 | 0) != 0);
 }
 i17 = HEAP32[i5 + 102956 >> 2] | 0;
 if ((i17 | 0) != 0) {
  do {
   HEAP8[i17 + 60 | 0] = 0;
   i17 = HEAP32[i17 + 12 >> 2] | 0;
  } while ((i17 | 0) != 0);
 }
 i24 = HEAP32[i16 >> 2] | 0;
 i16 = __ZN16b2StackAllocator8AllocateEi(i6, i24 << 2) | 0;
 i32 = HEAP32[i7 >> 2] | 0;
 L13 : do {
  if ((i32 | 0) != 0) {
   i18 = i4 + 28 | 0;
   i30 = i4 + 36 | 0;
   i27 = i4 + 32 | 0;
   i17 = i4 + 40 | 0;
   i23 = i4 + 8 | 0;
   i29 = i4 + 48 | 0;
   i28 = i4 + 16 | 0;
   i26 = i4 + 44 | 0;
   i31 = i4 + 12 | 0;
   i25 = i5 + 102968 | 0;
   i22 = i5 + 102976 | 0;
   i21 = i9 + 12 | 0;
   i20 = i9 + 16 | 0;
   i19 = i9 + 20 | 0;
   L15 : while (1) {
    i33 = i32 + 4 | 0;
    i34 = HEAP16[i33 >> 1] | 0;
    if ((i34 & 35) == 34 ? (HEAP32[i32 >> 2] | 0) != 0 : 0) {
     HEAP32[i18 >> 2] = 0;
     HEAP32[i30 >> 2] = 0;
     HEAP32[i27 >> 2] = 0;
     HEAP32[i16 >> 2] = i32;
     HEAP16[i33 >> 1] = i34 & 65535 | 1;
     i35 = 1;
     do {
      i35 = i35 + -1 | 0;
      i33 = HEAP32[i16 + (i35 << 2) >> 2] | 0;
      i34 = i33 + 4 | 0;
      i36 = HEAP16[i34 >> 1] | 0;
      if ((i36 & 32) == 0) {
       i8 = 13;
       break L15;
      }
      i37 = HEAP32[i18 >> 2] | 0;
      if ((i37 | 0) >= (HEAP32[i17 >> 2] | 0)) {
       i8 = 15;
       break L15;
      }
      HEAP32[i33 + 8 >> 2] = i37;
      i38 = HEAP32[i18 >> 2] | 0;
      HEAP32[(HEAP32[i23 >> 2] | 0) + (i38 << 2) >> 2] = i33;
      HEAP32[i18 >> 2] = i38 + 1;
      i36 = i36 & 65535;
      if ((i36 & 2 | 0) == 0) {
       HEAP16[i34 >> 1] = i36 | 2;
       HEAPF32[i33 + 144 >> 2] = 0.0;
      }
      if ((HEAP32[i33 >> 2] | 0) != 0) {
       i34 = HEAP32[i33 + 112 >> 2] | 0;
       if ((i34 | 0) != 0) {
        do {
         i38 = HEAP32[i34 + 4 >> 2] | 0;
         i36 = i38 + 4 | 0;
         if (((HEAP32[i36 >> 2] & 7 | 0) == 6 ? (HEAP8[(HEAP32[i38 + 48 >> 2] | 0) + 38 | 0] | 0) == 0 : 0) ? (HEAP8[(HEAP32[i38 + 52 >> 2] | 0) + 38 | 0] | 0) == 0 : 0) {
          i37 = HEAP32[i30 >> 2] | 0;
          if ((i37 | 0) >= (HEAP32[i26 >> 2] | 0)) {
           i8 = 25;
           break L15;
          }
          HEAP32[i30 >> 2] = i37 + 1;
          HEAP32[(HEAP32[i31 >> 2] | 0) + (i37 << 2) >> 2] = i38;
          HEAP32[i36 >> 2] = HEAP32[i36 >> 2] | 1;
          i38 = HEAP32[i34 >> 2] | 0;
          i36 = i38 + 4 | 0;
          i37 = HEAP16[i36 >> 1] | 0;
          if ((i37 & 1) == 0) {
           if ((i35 | 0) >= (i24 | 0)) {
            i8 = 28;
            break L15;
           }
           HEAP32[i16 + (i35 << 2) >> 2] = i38;
           HEAP16[i36 >> 1] = i37 & 65535 | 1;
           i35 = i35 + 1 | 0;
          }
         }
         i34 = HEAP32[i34 + 12 >> 2] | 0;
        } while ((i34 | 0) != 0);
       }
       i33 = HEAP32[i33 + 108 >> 2] | 0;
       if ((i33 | 0) != 0) {
        do {
         i37 = i33 + 4 | 0;
         i36 = HEAP32[i37 >> 2] | 0;
         if ((HEAP8[i36 + 60 | 0] | 0) == 0 ? (i10 = HEAP32[i33 >> 2] | 0, i13 = i10 + 4 | 0, i12 = HEAP16[i13 >> 1] | 0, !((i12 & 32) == 0)) : 0) {
          i34 = HEAP32[i27 >> 2] | 0;
          if ((i34 | 0) >= (HEAP32[i29 >> 2] | 0)) {
           i8 = 35;
           break L15;
          }
          HEAP32[i27 >> 2] = i34 + 1;
          HEAP32[(HEAP32[i28 >> 2] | 0) + (i34 << 2) >> 2] = i36;
          HEAP8[(HEAP32[i37 >> 2] | 0) + 60 | 0] = 1;
          if ((i12 & 1) == 0) {
           if ((i35 | 0) >= (i24 | 0)) {
            i8 = 38;
            break L15;
           }
           HEAP32[i16 + (i35 << 2) >> 2] = i10;
           HEAP16[i13 >> 1] = i12 & 65535 | 1;
           i35 = i35 + 1 | 0;
          }
         }
         i33 = HEAP32[i33 + 12 >> 2] | 0;
        } while ((i33 | 0) != 0);
       }
      }
     } while ((i35 | 0) > 0);
     __ZN8b2Island5SolveEP9b2ProfileRK10b2TimeStepRK6b2Vec2b(i4, i9, i15, i25, (HEAP8[i22] | 0) != 0);
     HEAPF32[i11 >> 2] = +HEAPF32[i21 >> 2] + +HEAPF32[i11 >> 2];
     HEAPF32[i14 >> 2] = +HEAPF32[i20 >> 2] + +HEAPF32[i14 >> 2];
     HEAPF32[i8 >> 2] = +HEAPF32[i19 >> 2] + +HEAPF32[i8 >> 2];
     i35 = HEAP32[i18 >> 2] | 0;
     if ((i35 | 0) > 0) {
      i33 = HEAP32[i23 >> 2] | 0;
      i36 = 0;
      do {
       i34 = HEAP32[i33 + (i36 << 2) >> 2] | 0;
       if ((HEAP32[i34 >> 2] | 0) == 0) {
        i38 = i34 + 4 | 0;
        HEAP16[i38 >> 1] = HEAP16[i38 >> 1] & 65534;
       }
       i36 = i36 + 1 | 0;
      } while ((i36 | 0) < (i35 | 0));
     }
    }
    i32 = HEAP32[i32 + 96 >> 2] | 0;
    if ((i32 | 0) == 0) {
     break L13;
    }
   }
   if ((i8 | 0) == 13) {
    ___assert_fail(2232, 2184, 445, 2256);
   } else if ((i8 | 0) == 15) {
    ___assert_fail(2520, 2440, 54, 2472);
   } else if ((i8 | 0) == 25) {
    ___assert_fail(2480, 2440, 62, 2472);
   } else if ((i8 | 0) == 28) {
    ___assert_fail(2264, 2184, 495, 2256);
   } else if ((i8 | 0) == 35) {
    ___assert_fail(2408, 2440, 68, 2472);
   } else if ((i8 | 0) == 38) {
    ___assert_fail(2264, 2184, 524, 2256);
   }
  }
 } while (0);
 __ZN16b2StackAllocator4FreeEPv(i6, i16);
 __ZN7b2TimerC2Ev(i2);
 i6 = HEAP32[i7 >> 2] | 0;
 if ((i6 | 0) == 0) {
  __ZN16b2ContactManager15FindNewContactsEv(i1);
  d39 = +__ZNK7b2Timer15GetMillisecondsEv(i2);
  i38 = i5 + 103020 | 0;
  HEAPF32[i38 >> 2] = d39;
  __ZN8b2IslandD2Ev(i4);
  STACKTOP = i3;
  return;
 }
 do {
  if (!((HEAP16[i6 + 4 >> 1] & 1) == 0) ? (HEAP32[i6 >> 2] | 0) != 0 : 0) {
   __ZN6b2Body19SynchronizeFixturesEv(i6);
  }
  i6 = HEAP32[i6 + 96 >> 2] | 0;
 } while ((i6 | 0) != 0);
 __ZN16b2ContactManager15FindNewContactsEv(i1);
 d39 = +__ZNK7b2Timer15GetMillisecondsEv(i2);
 i38 = i5 + 103020 | 0;
 HEAPF32[i38 >> 2] = d39;
 __ZN8b2IslandD2Ev(i4);
 STACKTOP = i3;
 return;
}
function __ZN15b2ContactSolver29InitializeVelocityConstraintsEv(i10) {
 i10 = i10 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, d17 = 0.0, d18 = 0.0, d19 = 0.0, d20 = 0.0, d21 = 0.0, d22 = 0.0, d23 = 0.0, d24 = 0.0, d25 = 0.0, d26 = 0.0, d27 = 0.0, d28 = 0.0, d29 = 0.0, d30 = 0.0, i31 = 0, d32 = 0.0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, d38 = 0.0, d39 = 0.0, d40 = 0.0, d41 = 0.0, i42 = 0, d43 = 0.0, d44 = 0.0, d45 = 0.0, d46 = 0.0, d47 = 0.0, d48 = 0.0, i49 = 0, i50 = 0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 64 | 0;
 i8 = i1 + 40 | 0;
 i3 = i1 + 24 | 0;
 i5 = i1;
 i4 = i10 + 48 | 0;
 if ((HEAP32[i4 >> 2] | 0) <= 0) {
  STACKTOP = i1;
  return;
 }
 i9 = i10 + 40 | 0;
 i2 = i10 + 36 | 0;
 i7 = i10 + 44 | 0;
 i6 = i10 + 24 | 0;
 i13 = i10 + 28 | 0;
 i14 = i8 + 8 | 0;
 i12 = i8 + 12 | 0;
 i11 = i3 + 8 | 0;
 i10 = i3 + 12 | 0;
 i16 = 0;
 while (1) {
  i15 = HEAP32[i9 >> 2] | 0;
  i33 = HEAP32[i2 >> 2] | 0;
  i31 = HEAP32[(HEAP32[i7 >> 2] | 0) + (HEAP32[i15 + (i16 * 152 | 0) + 148 >> 2] << 2) >> 2] | 0;
  i35 = HEAP32[i15 + (i16 * 152 | 0) + 112 >> 2] | 0;
  i42 = HEAP32[i15 + (i16 * 152 | 0) + 116 >> 2] | 0;
  d30 = +HEAPF32[i15 + (i16 * 152 | 0) + 120 >> 2];
  d24 = +HEAPF32[i15 + (i16 * 152 | 0) + 124 >> 2];
  d17 = +HEAPF32[i15 + (i16 * 152 | 0) + 128 >> 2];
  d18 = +HEAPF32[i15 + (i16 * 152 | 0) + 132 >> 2];
  i36 = i33 + (i16 * 88 | 0) + 48 | 0;
  d39 = +HEAPF32[i36 >> 2];
  d40 = +HEAPF32[i36 + 4 >> 2];
  i36 = i33 + (i16 * 88 | 0) + 56 | 0;
  d41 = +HEAPF32[i36 >> 2];
  d43 = +HEAPF32[i36 + 4 >> 2];
  i36 = HEAP32[i6 >> 2] | 0;
  i37 = i36 + (i35 * 12 | 0) | 0;
  d26 = +HEAPF32[i37 >> 2];
  d27 = +HEAPF32[i37 + 4 >> 2];
  d32 = +HEAPF32[i36 + (i35 * 12 | 0) + 8 >> 2];
  i37 = HEAP32[i13 >> 2] | 0;
  i34 = i37 + (i35 * 12 | 0) | 0;
  d22 = +HEAPF32[i34 >> 2];
  d25 = +HEAPF32[i34 + 4 >> 2];
  d23 = +HEAPF32[i37 + (i35 * 12 | 0) + 8 >> 2];
  i35 = i36 + (i42 * 12 | 0) | 0;
  d28 = +HEAPF32[i35 >> 2];
  d29 = +HEAPF32[i35 + 4 >> 2];
  d38 = +HEAPF32[i36 + (i42 * 12 | 0) + 8 >> 2];
  i36 = i37 + (i42 * 12 | 0) | 0;
  d20 = +HEAPF32[i36 >> 2];
  d19 = +HEAPF32[i36 + 4 >> 2];
  d21 = +HEAPF32[i37 + (i42 * 12 | 0) + 8 >> 2];
  if ((HEAP32[i31 + 124 >> 2] | 0) <= 0) {
   i2 = 4;
   break;
  }
  d44 = +HEAPF32[i33 + (i16 * 88 | 0) + 80 >> 2];
  d45 = +HEAPF32[i33 + (i16 * 88 | 0) + 76 >> 2];
  d47 = +Math_sin(+d32);
  HEAPF32[i14 >> 2] = d47;
  d48 = +Math_cos(+d32);
  HEAPF32[i12 >> 2] = d48;
  d32 = +Math_sin(+d38);
  HEAPF32[i11 >> 2] = d32;
  d38 = +Math_cos(+d38);
  HEAPF32[i10 >> 2] = d38;
  d46 = +(d26 - (d39 * d48 - d40 * d47));
  d40 = +(d27 - (d40 * d48 + d39 * d47));
  i37 = i8;
  HEAPF32[i37 >> 2] = d46;
  HEAPF32[i37 + 4 >> 2] = d40;
  d40 = +(d28 - (d41 * d38 - d43 * d32));
  d43 = +(d29 - (d43 * d38 + d41 * d32));
  i37 = i3;
  HEAPF32[i37 >> 2] = d40;
  HEAPF32[i37 + 4 >> 2] = d43;
  __ZN15b2WorldManifold10InitializeEPK10b2ManifoldRK11b2TransformfS5_f(i5, i31 + 64 | 0, i8, d45, i3, d44);
  i37 = i15 + (i16 * 152 | 0) + 72 | 0;
  i42 = i5;
  i33 = HEAP32[i42 + 4 >> 2] | 0;
  i31 = i37;
  HEAP32[i31 >> 2] = HEAP32[i42 >> 2];
  HEAP32[i31 + 4 >> 2] = i33;
  i31 = i15 + (i16 * 152 | 0) + 144 | 0;
  i33 = HEAP32[i31 >> 2] | 0;
  do {
   if ((i33 | 0) > 0) {
    i36 = i15 + (i16 * 152 | 0) + 76 | 0;
    d32 = d30 + d24;
    i35 = i15 + (i16 * 152 | 0) + 140 | 0;
    i34 = 0;
    do {
     i49 = i5 + (i34 << 3) + 8 | 0;
     d41 = +HEAPF32[i49 >> 2] - d26;
     i42 = i5 + (i34 << 3) + 12 | 0;
     d39 = +d41;
     d40 = +(+HEAPF32[i42 >> 2] - d27);
     i50 = i15 + (i16 * 152 | 0) + (i34 * 36 | 0) | 0;
     HEAPF32[i50 >> 2] = d39;
     HEAPF32[i50 + 4 >> 2] = d40;
     d40 = +HEAPF32[i49 >> 2] - d28;
     d39 = +d40;
     d47 = +(+HEAPF32[i42 >> 2] - d29);
     i42 = i15 + (i16 * 152 | 0) + (i34 * 36 | 0) + 8 | 0;
     HEAPF32[i42 >> 2] = d39;
     HEAPF32[i42 + 4 >> 2] = d47;
     d47 = +HEAPF32[i36 >> 2];
     d39 = +HEAPF32[i15 + (i16 * 152 | 0) + (i34 * 36 | 0) + 4 >> 2];
     d43 = +HEAPF32[i37 >> 2];
     d48 = d41 * d47 - d39 * d43;
     d38 = +HEAPF32[i15 + (i16 * 152 | 0) + (i34 * 36 | 0) + 12 >> 2];
     d43 = d47 * d40 - d43 * d38;
     d43 = d32 + d48 * d17 * d48 + d43 * d18 * d43;
     if (d43 > 0.0) {
      d43 = 1.0 / d43;
     } else {
      d43 = 0.0;
     }
     HEAPF32[i15 + (i16 * 152 | 0) + (i34 * 36 | 0) + 24 >> 2] = d43;
     d43 = +HEAPF32[i36 >> 2];
     d47 = -+HEAPF32[i37 >> 2];
     d48 = d41 * d47 - d43 * d39;
     d43 = d40 * d47 - d43 * d38;
     d43 = d32 + d48 * d17 * d48 + d43 * d18 * d43;
     if (d43 > 0.0) {
      d43 = 1.0 / d43;
     } else {
      d43 = 0.0;
     }
     HEAPF32[i15 + (i16 * 152 | 0) + (i34 * 36 | 0) + 28 >> 2] = d43;
     i42 = i15 + (i16 * 152 | 0) + (i34 * 36 | 0) + 32 | 0;
     HEAPF32[i42 >> 2] = 0.0;
     d38 = +HEAPF32[i37 >> 2] * (d20 - d21 * d38 - d22 + d23 * d39) + +HEAPF32[i36 >> 2] * (d19 + d21 * d40 - d25 - d23 * d41);
     if (d38 < -1.0) {
      HEAPF32[i42 >> 2] = -(d38 * +HEAPF32[i35 >> 2]);
     }
     i34 = i34 + 1 | 0;
    } while ((i34 | 0) != (i33 | 0));
    if ((HEAP32[i31 >> 2] | 0) == 2) {
     d45 = +HEAPF32[i15 + (i16 * 152 | 0) + 76 >> 2];
     d20 = +HEAPF32[i37 >> 2];
     d44 = +HEAPF32[i15 + (i16 * 152 | 0) >> 2] * d45 - +HEAPF32[i15 + (i16 * 152 | 0) + 4 >> 2] * d20;
     d19 = d45 * +HEAPF32[i15 + (i16 * 152 | 0) + 8 >> 2] - d20 * +HEAPF32[i15 + (i16 * 152 | 0) + 12 >> 2];
     d47 = d45 * +HEAPF32[i15 + (i16 * 152 | 0) + 36 >> 2] - d20 * +HEAPF32[i15 + (i16 * 152 | 0) + 40 >> 2];
     d20 = d45 * +HEAPF32[i15 + (i16 * 152 | 0) + 44 >> 2] - d20 * +HEAPF32[i15 + (i16 * 152 | 0) + 48 >> 2];
     d45 = d30 + d24;
     d46 = d17 * d44;
     d48 = d18 * d19;
     d19 = d45 + d44 * d46 + d19 * d48;
     d18 = d45 + d47 * d17 * d47 + d20 * d18 * d20;
     d17 = d45 + d46 * d47 + d48 * d20;
     d20 = d19 * d18 - d17 * d17;
     if (!(d19 * d19 < d20 * 1.0e3)) {
      HEAP32[i31 >> 2] = 1;
      break;
     }
     HEAPF32[i15 + (i16 * 152 | 0) + 96 >> 2] = d19;
     HEAPF32[i15 + (i16 * 152 | 0) + 100 >> 2] = d17;
     HEAPF32[i15 + (i16 * 152 | 0) + 104 >> 2] = d17;
     HEAPF32[i15 + (i16 * 152 | 0) + 108 >> 2] = d18;
     if (d20 != 0.0) {
      d20 = 1.0 / d20;
     }
     d48 = -(d20 * d17);
     HEAPF32[i15 + (i16 * 152 | 0) + 80 >> 2] = d18 * d20;
     HEAPF32[i15 + (i16 * 152 | 0) + 84 >> 2] = d48;
     HEAPF32[i15 + (i16 * 152 | 0) + 88 >> 2] = d48;
     HEAPF32[i15 + (i16 * 152 | 0) + 92 >> 2] = d19 * d20;
    }
   }
  } while (0);
  i16 = i16 + 1 | 0;
  if ((i16 | 0) >= (HEAP32[i4 >> 2] | 0)) {
   i2 = 21;
   break;
  }
 }
 if ((i2 | 0) == 4) {
  ___assert_fail(6584, 6520, 168, 6616);
 } else if ((i2 | 0) == 21) {
  STACKTOP = i1;
  return;
 }
}
function __Z17b2CollidePolygonsP10b2ManifoldPK14b2PolygonShapeRK11b2TransformS3_S6_(i5, i27, i28, i24, i14) {
 i5 = i5 | 0;
 i27 = i27 | 0;
 i28 = i28 | 0;
 i24 = i24 | 0;
 i14 = i14 | 0;
 var i1 = 0, i2 = 0, d3 = 0.0, i4 = 0, d6 = 0.0, d7 = 0.0, d8 = 0.0, d9 = 0.0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, d15 = 0.0, d16 = 0.0, i17 = 0, d18 = 0.0, d19 = 0.0, i20 = 0, d21 = 0.0, d22 = 0.0, d23 = 0.0, d25 = 0.0, d26 = 0.0, d29 = 0.0, d30 = 0.0, i31 = 0, d32 = 0.0, i33 = 0, i34 = 0, d35 = 0.0, d36 = 0.0, d37 = 0.0, d38 = 0.0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 96 | 0;
 i17 = i1 + 92 | 0;
 i20 = i1 + 88 | 0;
 i13 = i1;
 i11 = i1 + 80 | 0;
 i12 = i1 + 56 | 0;
 i4 = i1 + 32 | 0;
 i10 = i1 + 24 | 0;
 i2 = i5 + 60 | 0;
 HEAP32[i2 >> 2] = 0;
 d3 = +HEAPF32[i27 + 8 >> 2] + +HEAPF32[i24 + 8 >> 2];
 HEAP32[i17 >> 2] = 0;
 d7 = +__ZL19b2FindMaxSeparationPiPK14b2PolygonShapeRK11b2TransformS2_S5_(i17, i27, i28, i24, i14);
 if (d7 > d3) {
  STACKTOP = i1;
  return;
 }
 HEAP32[i20 >> 2] = 0;
 d6 = +__ZL19b2FindMaxSeparationPiPK14b2PolygonShapeRK11b2TransformS2_S5_(i20, i24, i14, i27, i28);
 if (d6 > d3) {
  STACKTOP = i1;
  return;
 }
 if (d6 > d7 * .9800000190734863 + .0010000000474974513) {
  d18 = +HEAPF32[i14 >> 2];
  d19 = +HEAPF32[i14 + 4 >> 2];
  d15 = +HEAPF32[i14 + 8 >> 2];
  d16 = +HEAPF32[i14 + 12 >> 2];
  d9 = +HEAPF32[i28 >> 2];
  d6 = +HEAPF32[i28 + 4 >> 2];
  d7 = +HEAPF32[i28 + 8 >> 2];
  d8 = +HEAPF32[i28 + 12 >> 2];
  i17 = HEAP32[i20 >> 2] | 0;
  HEAP32[i5 + 56 >> 2] = 2;
  i14 = 1;
  i20 = i24;
 } else {
  d18 = +HEAPF32[i28 >> 2];
  d19 = +HEAPF32[i28 + 4 >> 2];
  d15 = +HEAPF32[i28 + 8 >> 2];
  d16 = +HEAPF32[i28 + 12 >> 2];
  d9 = +HEAPF32[i14 >> 2];
  d6 = +HEAPF32[i14 + 4 >> 2];
  d7 = +HEAPF32[i14 + 8 >> 2];
  d8 = +HEAPF32[i14 + 12 >> 2];
  i17 = HEAP32[i17 >> 2] | 0;
  HEAP32[i5 + 56 >> 2] = 1;
  i14 = 0;
  i20 = i27;
  i27 = i24;
 }
 i28 = HEAP32[i27 + 148 >> 2] | 0;
 if (!((i17 | 0) > -1)) {
  ___assert_fail(5640, 5688, 151, 5728);
 }
 i24 = HEAP32[i20 + 148 >> 2] | 0;
 if ((i24 | 0) <= (i17 | 0)) {
  ___assert_fail(5640, 5688, 151, 5728);
 }
 d21 = +HEAPF32[i20 + (i17 << 3) + 84 >> 2];
 d36 = +HEAPF32[i20 + (i17 << 3) + 88 >> 2];
 d22 = d16 * d21 - d15 * d36;
 d36 = d15 * d21 + d16 * d36;
 d21 = d8 * d22 + d7 * d36;
 d22 = d8 * d36 - d7 * d22;
 if ((i28 | 0) > 0) {
  i33 = 0;
  i34 = 0;
  d23 = 3.4028234663852886e+38;
  while (1) {
   d25 = d21 * +HEAPF32[i27 + (i33 << 3) + 84 >> 2] + d22 * +HEAPF32[i27 + (i33 << 3) + 88 >> 2];
   i31 = d25 < d23;
   i34 = i31 ? i33 : i34;
   i33 = i33 + 1 | 0;
   if ((i33 | 0) == (i28 | 0)) {
    break;
   } else {
    d23 = i31 ? d25 : d23;
   }
  }
 } else {
  i34 = 0;
 }
 i31 = i34 + 1 | 0;
 i33 = (i31 | 0) < (i28 | 0) ? i31 : 0;
 d35 = +HEAPF32[i27 + (i34 << 3) + 20 >> 2];
 d32 = +HEAPF32[i27 + (i34 << 3) + 24 >> 2];
 d36 = +(d9 + (d8 * d35 - d7 * d32));
 d32 = +(d6 + (d7 * d35 + d8 * d32));
 i31 = i13;
 HEAPF32[i31 >> 2] = d36;
 HEAPF32[i31 + 4 >> 2] = d32;
 i31 = i17 & 255;
 i28 = i13 + 8 | 0;
 HEAP8[i28] = i31;
 HEAP8[i28 + 1 | 0] = i34;
 HEAP8[i28 + 2 | 0] = 1;
 HEAP8[i28 + 3 | 0] = 0;
 d32 = +HEAPF32[i27 + (i33 << 3) + 20 >> 2];
 d36 = +HEAPF32[i27 + (i33 << 3) + 24 >> 2];
 d35 = +(d9 + (d8 * d32 - d7 * d36));
 d36 = +(d6 + (d7 * d32 + d8 * d36));
 i27 = i13 + 12 | 0;
 HEAPF32[i27 >> 2] = d35;
 HEAPF32[i27 + 4 >> 2] = d36;
 i27 = i13 + 20 | 0;
 HEAP8[i27] = i31;
 HEAP8[i27 + 1 | 0] = i33;
 HEAP8[i27 + 2 | 0] = 1;
 HEAP8[i27 + 3 | 0] = 0;
 i27 = i17 + 1 | 0;
 i24 = (i27 | 0) < (i24 | 0) ? i27 : 0;
 i34 = i20 + (i17 << 3) + 20 | 0;
 d26 = +HEAPF32[i34 >> 2];
 d25 = +HEAPF32[i34 + 4 >> 2];
 i34 = i20 + (i24 << 3) + 20 | 0;
 d30 = +HEAPF32[i34 >> 2];
 d29 = +HEAPF32[i34 + 4 >> 2];
 d32 = d30 - d26;
 d35 = d29 - d25;
 d21 = +Math_sqrt(+(d32 * d32 + d35 * d35));
 if (!(d21 < 1.1920928955078125e-7)) {
  d36 = 1.0 / d21;
  d32 = d32 * d36;
  d35 = d35 * d36;
 }
 d36 = d16 * d32 - d15 * d35;
 d21 = d16 * d35 + d15 * d32;
 HEAPF32[i11 >> 2] = d36;
 HEAPF32[i11 + 4 >> 2] = d21;
 d22 = -d36;
 d38 = d18 + (d16 * d26 - d15 * d25);
 d37 = d19 + (d15 * d26 + d16 * d25);
 d23 = d38 * d21 + d37 * d22;
 HEAPF32[i10 >> 2] = d22;
 HEAPF32[i10 + 4 >> 2] = -d21;
 if ((__Z19b2ClipSegmentToLineP12b2ClipVertexPKS_RK6b2Vec2fi(i12, i13, i10, d3 - (d38 * d36 + d37 * d21), i17) | 0) < 2) {
  STACKTOP = i1;
  return;
 }
 if ((__Z19b2ClipSegmentToLineP12b2ClipVertexPKS_RK6b2Vec2fi(i4, i12, i11, d3 + ((d18 + (d16 * d30 - d15 * d29)) * d36 + (d19 + (d15 * d30 + d16 * d29)) * d21), i24) | 0) < 2) {
  STACKTOP = i1;
  return;
 }
 d16 = +d35;
 d15 = +-d32;
 i10 = i5 + 40 | 0;
 HEAPF32[i10 >> 2] = d16;
 HEAPF32[i10 + 4 >> 2] = d15;
 d15 = +((d26 + d30) * .5);
 d16 = +((d25 + d29) * .5);
 i10 = i5 + 48 | 0;
 HEAPF32[i10 >> 2] = d15;
 HEAPF32[i10 + 4 >> 2] = d16;
 d16 = +HEAPF32[i4 >> 2];
 d15 = +HEAPF32[i4 + 4 >> 2];
 i10 = !(d21 * d16 + d15 * d22 - d23 <= d3);
 if (i14 << 24 >> 24 == 0) {
  if (i10) {
   i10 = 0;
  } else {
   d38 = d16 - d9;
   d36 = d15 - d6;
   d37 = +(d8 * d38 + d7 * d36);
   d38 = +(d8 * d36 - d7 * d38);
   i10 = i5;
   HEAPF32[i10 >> 2] = d37;
   HEAPF32[i10 + 4 >> 2] = d38;
   HEAP32[i5 + 16 >> 2] = HEAP32[i4 + 8 >> 2];
   i10 = 1;
  }
  d16 = +HEAPF32[i4 + 12 >> 2];
  d15 = +HEAPF32[i4 + 16 >> 2];
  if (d21 * d16 + d15 * d22 - d23 <= d3) {
   d38 = d16 - d9;
   d36 = d15 - d6;
   d37 = +(d8 * d38 + d7 * d36);
   d38 = +(d8 * d36 - d7 * d38);
   i34 = i5 + (i10 * 20 | 0) | 0;
   HEAPF32[i34 >> 2] = d37;
   HEAPF32[i34 + 4 >> 2] = d38;
   HEAP32[i5 + (i10 * 20 | 0) + 16 >> 2] = HEAP32[i4 + 20 >> 2];
   i10 = i10 + 1 | 0;
  }
 } else {
  if (i10) {
   i10 = 0;
  } else {
   d38 = d16 - d9;
   d36 = d15 - d6;
   d37 = +(d8 * d38 + d7 * d36);
   d38 = +(d8 * d36 - d7 * d38);
   i10 = i5;
   HEAPF32[i10 >> 2] = d37;
   HEAPF32[i10 + 4 >> 2] = d38;
   i10 = i5 + 16 | 0;
   i34 = HEAP32[i4 + 8 >> 2] | 0;
   HEAP32[i10 >> 2] = i34;
   HEAP8[i10] = i34 >>> 8;
   HEAP8[i10 + 1 | 0] = i34;
   HEAP8[i10 + 2 | 0] = i34 >>> 24;
   HEAP8[i10 + 3 | 0] = i34 >>> 16;
   i10 = 1;
  }
  d16 = +HEAPF32[i4 + 12 >> 2];
  d15 = +HEAPF32[i4 + 16 >> 2];
  if (d21 * d16 + d15 * d22 - d23 <= d3) {
   d38 = d16 - d9;
   d36 = d15 - d6;
   d37 = +(d8 * d38 + d7 * d36);
   d38 = +(d8 * d36 - d7 * d38);
   i34 = i5 + (i10 * 20 | 0) | 0;
   HEAPF32[i34 >> 2] = d37;
   HEAPF32[i34 + 4 >> 2] = d38;
   i34 = i5 + (i10 * 20 | 0) + 16 | 0;
   i33 = HEAP32[i4 + 20 >> 2] | 0;
   HEAP32[i34 >> 2] = i33;
   HEAP8[i34] = i33 >>> 8;
   HEAP8[i34 + 1 | 0] = i33;
   HEAP8[i34 + 2 | 0] = i33 >>> 24;
   HEAP8[i34 + 3 | 0] = i33 >>> 16;
   i10 = i10 + 1 | 0;
  }
 }
 HEAP32[i2 >> 2] = i10;
 STACKTOP = i1;
 return;
}
function __ZN8b2Island8SolveTOIERK10b2TimeStepii(i4, i11, i15, i18) {
 i4 = i4 | 0;
 i11 = i11 | 0;
 i15 = i15 | 0;
 i18 = i18 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, d12 = 0.0, d13 = 0.0, d14 = 0.0, d16 = 0.0, d17 = 0.0, d19 = 0.0, d20 = 0.0, d21 = 0.0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, d26 = 0.0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 128 | 0;
 i2 = i1 + 96 | 0;
 i10 = i1 + 52 | 0;
 i3 = i1;
 i6 = i4 + 28 | 0;
 i5 = HEAP32[i6 >> 2] | 0;
 if ((i5 | 0) <= (i15 | 0)) {
  ___assert_fail(5464, 5488, 386, 5520);
 }
 if ((i5 | 0) <= (i18 | 0)) {
  ___assert_fail(5536, 5488, 387, 5520);
 }
 if ((i5 | 0) > 0) {
  i9 = i4 + 8 | 0;
  i8 = i4 + 20 | 0;
  i7 = i4 + 24 | 0;
  i22 = 0;
  while (1) {
   i23 = HEAP32[(HEAP32[i9 >> 2] | 0) + (i22 << 2) >> 2] | 0;
   i5 = i23 + 44 | 0;
   i24 = HEAP32[i5 + 4 >> 2] | 0;
   i25 = (HEAP32[i8 >> 2] | 0) + (i22 * 12 | 0) | 0;
   HEAP32[i25 >> 2] = HEAP32[i5 >> 2];
   HEAP32[i25 + 4 >> 2] = i24;
   HEAPF32[(HEAP32[i8 >> 2] | 0) + (i22 * 12 | 0) + 8 >> 2] = +HEAPF32[i23 + 56 >> 2];
   i25 = i23 + 64 | 0;
   i24 = HEAP32[i25 + 4 >> 2] | 0;
   i5 = (HEAP32[i7 >> 2] | 0) + (i22 * 12 | 0) | 0;
   HEAP32[i5 >> 2] = HEAP32[i25 >> 2];
   HEAP32[i5 + 4 >> 2] = i24;
   i5 = HEAP32[i7 >> 2] | 0;
   HEAPF32[i5 + (i22 * 12 | 0) + 8 >> 2] = +HEAPF32[i23 + 72 >> 2];
   i22 = i22 + 1 | 0;
   if ((i22 | 0) >= (HEAP32[i6 >> 2] | 0)) {
    i22 = i5;
    break;
   }
  }
 } else {
  i8 = i4 + 20 | 0;
  i22 = HEAP32[i4 + 24 >> 2] | 0;
 }
 i5 = i4 + 12 | 0;
 HEAP32[i10 + 24 >> 2] = HEAP32[i5 >> 2];
 i7 = i4 + 36 | 0;
 HEAP32[i10 + 28 >> 2] = HEAP32[i7 >> 2];
 HEAP32[i10 + 40 >> 2] = HEAP32[i4 >> 2];
 HEAP32[i10 + 0 >> 2] = HEAP32[i11 + 0 >> 2];
 HEAP32[i10 + 4 >> 2] = HEAP32[i11 + 4 >> 2];
 HEAP32[i10 + 8 >> 2] = HEAP32[i11 + 8 >> 2];
 HEAP32[i10 + 12 >> 2] = HEAP32[i11 + 12 >> 2];
 HEAP32[i10 + 16 >> 2] = HEAP32[i11 + 16 >> 2];
 HEAP32[i10 + 20 >> 2] = HEAP32[i11 + 20 >> 2];
 HEAP32[i10 + 32 >> 2] = HEAP32[i8 >> 2];
 i9 = i4 + 24 | 0;
 HEAP32[i10 + 36 >> 2] = i22;
 __ZN15b2ContactSolverC2EP18b2ContactSolverDef(i3, i10);
 i10 = i11 + 16 | 0;
 L13 : do {
  if ((HEAP32[i10 >> 2] | 0) > 0) {
   i22 = 0;
   do {
    i22 = i22 + 1 | 0;
    if (__ZN15b2ContactSolver27SolveTOIPositionConstraintsEii(i3, i15, i18) | 0) {
     break L13;
    }
   } while ((i22 | 0) < (HEAP32[i10 >> 2] | 0));
  }
 } while (0);
 i10 = i4 + 8 | 0;
 i24 = (HEAP32[i8 >> 2] | 0) + (i15 * 12 | 0) | 0;
 i25 = HEAP32[i24 + 4 >> 2] | 0;
 i23 = (HEAP32[(HEAP32[i10 >> 2] | 0) + (i15 << 2) >> 2] | 0) + 36 | 0;
 HEAP32[i23 >> 2] = HEAP32[i24 >> 2];
 HEAP32[i23 + 4 >> 2] = i25;
 i23 = HEAP32[i8 >> 2] | 0;
 i25 = HEAP32[i10 >> 2] | 0;
 HEAPF32[(HEAP32[i25 + (i15 << 2) >> 2] | 0) + 52 >> 2] = +HEAPF32[i23 + (i15 * 12 | 0) + 8 >> 2];
 i23 = i23 + (i18 * 12 | 0) | 0;
 i24 = HEAP32[i23 + 4 >> 2] | 0;
 i25 = (HEAP32[i25 + (i18 << 2) >> 2] | 0) + 36 | 0;
 HEAP32[i25 >> 2] = HEAP32[i23 >> 2];
 HEAP32[i25 + 4 >> 2] = i24;
 HEAPF32[(HEAP32[(HEAP32[i10 >> 2] | 0) + (i18 << 2) >> 2] | 0) + 52 >> 2] = +HEAPF32[(HEAP32[i8 >> 2] | 0) + (i18 * 12 | 0) + 8 >> 2];
 __ZN15b2ContactSolver29InitializeVelocityConstraintsEv(i3);
 i18 = i11 + 12 | 0;
 if ((HEAP32[i18 >> 2] | 0) > 0) {
  i15 = 0;
  do {
   __ZN15b2ContactSolver24SolveVelocityConstraintsEv(i3);
   i15 = i15 + 1 | 0;
  } while ((i15 | 0) < (HEAP32[i18 >> 2] | 0));
 }
 d16 = +HEAPF32[i11 >> 2];
 if ((HEAP32[i6 >> 2] | 0) > 0) {
  i15 = 0;
  do {
   i25 = HEAP32[i8 >> 2] | 0;
   i11 = i25 + (i15 * 12 | 0) | 0;
   i24 = i11;
   d12 = +HEAPF32[i24 >> 2];
   d14 = +HEAPF32[i24 + 4 >> 2];
   d13 = +HEAPF32[i25 + (i15 * 12 | 0) + 8 >> 2];
   i25 = HEAP32[i9 >> 2] | 0;
   i24 = i25 + (i15 * 12 | 0) | 0;
   d19 = +HEAPF32[i24 >> 2];
   d20 = +HEAPF32[i24 + 4 >> 2];
   d17 = +HEAPF32[i25 + (i15 * 12 | 0) + 8 >> 2];
   d26 = d16 * d19;
   d21 = d16 * d20;
   d21 = d26 * d26 + d21 * d21;
   if (d21 > 4.0) {
    d26 = 2.0 / +Math_sqrt(+d21);
    d19 = d19 * d26;
    d20 = d20 * d26;
   }
   d21 = d16 * d17;
   if (d21 * d21 > 2.4674012660980225) {
    if (!(d21 > 0.0)) {
     d21 = -d21;
    }
    d17 = d17 * (1.5707963705062866 / d21);
   }
   d21 = d12 + d16 * d19;
   d14 = d14 + d16 * d20;
   d26 = d13 + d16 * d17;
   d12 = +d21;
   d13 = +d14;
   i25 = i11;
   HEAPF32[i25 >> 2] = d12;
   HEAPF32[i25 + 4 >> 2] = d13;
   HEAPF32[(HEAP32[i8 >> 2] | 0) + (i15 * 12 | 0) + 8 >> 2] = d26;
   d19 = +d19;
   d20 = +d20;
   i25 = (HEAP32[i9 >> 2] | 0) + (i15 * 12 | 0) | 0;
   HEAPF32[i25 >> 2] = d19;
   HEAPF32[i25 + 4 >> 2] = d20;
   HEAPF32[(HEAP32[i9 >> 2] | 0) + (i15 * 12 | 0) + 8 >> 2] = d17;
   i25 = HEAP32[(HEAP32[i10 >> 2] | 0) + (i15 << 2) >> 2] | 0;
   i24 = i25 + 44 | 0;
   HEAPF32[i24 >> 2] = d12;
   HEAPF32[i24 + 4 >> 2] = d13;
   HEAPF32[i25 + 56 >> 2] = d26;
   i24 = i25 + 64 | 0;
   HEAPF32[i24 >> 2] = d19;
   HEAPF32[i24 + 4 >> 2] = d20;
   HEAPF32[i25 + 72 >> 2] = d17;
   d17 = +Math_sin(+d26);
   HEAPF32[i25 + 20 >> 2] = d17;
   d20 = +Math_cos(+d26);
   HEAPF32[i25 + 24 >> 2] = d20;
   d19 = +HEAPF32[i25 + 28 >> 2];
   d26 = +HEAPF32[i25 + 32 >> 2];
   d21 = +(d21 - (d20 * d19 - d17 * d26));
   d26 = +(d14 - (d17 * d19 + d20 * d26));
   i25 = i25 + 12 | 0;
   HEAPF32[i25 >> 2] = d21;
   HEAPF32[i25 + 4 >> 2] = d26;
   i15 = i15 + 1 | 0;
  } while ((i15 | 0) < (HEAP32[i6 >> 2] | 0));
 }
 i6 = HEAP32[i3 + 40 >> 2] | 0;
 i4 = i4 + 4 | 0;
 if ((HEAP32[i4 >> 2] | 0) == 0) {
  __ZN15b2ContactSolverD2Ev(i3);
  STACKTOP = i1;
  return;
 }
 if ((HEAP32[i7 >> 2] | 0) <= 0) {
  __ZN15b2ContactSolverD2Ev(i3);
  STACKTOP = i1;
  return;
 }
 i8 = i2 + 16 | 0;
 i9 = 0;
 do {
  i10 = HEAP32[(HEAP32[i5 >> 2] | 0) + (i9 << 2) >> 2] | 0;
  i11 = HEAP32[i6 + (i9 * 152 | 0) + 144 >> 2] | 0;
  HEAP32[i8 >> 2] = i11;
  if ((i11 | 0) > 0) {
   i15 = 0;
   do {
    HEAPF32[i2 + (i15 << 2) >> 2] = +HEAPF32[i6 + (i9 * 152 | 0) + (i15 * 36 | 0) + 16 >> 2];
    HEAPF32[i2 + (i15 << 2) + 8 >> 2] = +HEAPF32[i6 + (i9 * 152 | 0) + (i15 * 36 | 0) + 20 >> 2];
    i15 = i15 + 1 | 0;
   } while ((i15 | 0) != (i11 | 0));
  }
  i25 = HEAP32[i4 >> 2] | 0;
  FUNCTION_TABLE_viii[HEAP32[(HEAP32[i25 >> 2] | 0) + 20 >> 2] & 3](i25, i10, i2);
  i9 = i9 + 1 | 0;
 } while ((i9 | 0) < (HEAP32[i7 >> 2] | 0));
 __ZN15b2ContactSolverD2Ev(i3);
 STACKTOP = i1;
 return;
}
function __ZN20b2SeparationFunction10InitializeEPK14b2SimplexCachePK15b2DistanceProxyRK7b2SweepS5_S8_f(i2, i11, i13, i21, i12, i24, d9) {
 i2 = i2 | 0;
 i11 = i11 | 0;
 i13 = i13 | 0;
 i21 = i21 | 0;
 i12 = i12 | 0;
 i24 = i24 | 0;
 d9 = +d9;
 var i1 = 0, d3 = 0.0, d4 = 0.0, d5 = 0.0, d6 = 0.0, d7 = 0.0, d8 = 0.0, d10 = 0.0, i14 = 0, d15 = 0.0, d16 = 0.0, d17 = 0.0, d18 = 0.0, d19 = 0.0, d20 = 0.0, d22 = 0.0, i23 = 0, i25 = 0, i26 = 0, i27 = 0, d28 = 0.0, d29 = 0.0;
 i1 = STACKTOP;
 HEAP32[i2 >> 2] = i13;
 HEAP32[i2 + 4 >> 2] = i12;
 i14 = HEAP16[i11 + 4 >> 1] | 0;
 if (!(i14 << 16 >> 16 != 0 & (i14 & 65535) < 3)) {
  ___assert_fail(3744, 3560, 50, 3768);
 }
 i23 = i2 + 8 | 0;
 i25 = i23 + 0 | 0;
 i27 = i21 + 0 | 0;
 i26 = i25 + 36 | 0;
 do {
  HEAP32[i25 >> 2] = HEAP32[i27 >> 2];
  i25 = i25 + 4 | 0;
  i27 = i27 + 4 | 0;
 } while ((i25 | 0) < (i26 | 0));
 i21 = i2 + 44 | 0;
 i25 = i21 + 0 | 0;
 i27 = i24 + 0 | 0;
 i26 = i25 + 36 | 0;
 do {
  HEAP32[i25 >> 2] = HEAP32[i27 >> 2];
  i25 = i25 + 4 | 0;
  i27 = i27 + 4 | 0;
 } while ((i25 | 0) < (i26 | 0));
 d19 = 1.0 - d9;
 d4 = d19 * +HEAPF32[i2 + 32 >> 2] + +HEAPF32[i2 + 36 >> 2] * d9;
 d3 = +Math_sin(+d4);
 d4 = +Math_cos(+d4);
 d7 = +HEAPF32[i23 >> 2];
 d5 = +HEAPF32[i2 + 12 >> 2];
 d8 = d19 * +HEAPF32[i2 + 16 >> 2] + +HEAPF32[i2 + 24 >> 2] * d9 - (d4 * d7 - d3 * d5);
 d5 = d19 * +HEAPF32[i2 + 20 >> 2] + +HEAPF32[i2 + 28 >> 2] * d9 - (d3 * d7 + d4 * d5);
 d7 = d19 * +HEAPF32[i2 + 68 >> 2] + +HEAPF32[i2 + 72 >> 2] * d9;
 d6 = +Math_sin(+d7);
 d7 = +Math_cos(+d7);
 d20 = +HEAPF32[i21 >> 2];
 d22 = +HEAPF32[i2 + 48 >> 2];
 d10 = d19 * +HEAPF32[i2 + 52 >> 2] + +HEAPF32[i2 + 60 >> 2] * d9 - (d7 * d20 - d6 * d22);
 d9 = d19 * +HEAPF32[i2 + 56 >> 2] + +HEAPF32[i2 + 64 >> 2] * d9 - (d6 * d20 + d7 * d22);
 if (i14 << 16 >> 16 == 1) {
  HEAP32[i2 + 80 >> 2] = 0;
  i14 = HEAPU8[i11 + 6 | 0] | 0;
  if ((HEAP32[i13 + 20 >> 2] | 0) <= (i14 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i27 = (HEAP32[i13 + 16 >> 2] | 0) + (i14 << 3) | 0;
  d15 = +HEAPF32[i27 >> 2];
  d16 = +HEAPF32[i27 + 4 >> 2];
  i11 = HEAPU8[i11 + 9 | 0] | 0;
  if ((HEAP32[i12 + 20 >> 2] | 0) <= (i11 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i11 = (HEAP32[i12 + 16 >> 2] | 0) + (i11 << 3) | 0;
  d20 = +HEAPF32[i11 >> 2];
  d22 = +HEAPF32[i11 + 4 >> 2];
  i11 = i2 + 92 | 0;
  d8 = d10 + (d7 * d20 - d6 * d22) - (d8 + (d4 * d15 - d3 * d16));
  d4 = d9 + (d6 * d20 + d7 * d22) - (d5 + (d3 * d15 + d4 * d16));
  d22 = +d8;
  d3 = +d4;
  i27 = i11;
  HEAPF32[i27 >> 2] = d22;
  HEAPF32[i27 + 4 >> 2] = d3;
  d3 = +Math_sqrt(+(d8 * d8 + d4 * d4));
  if (d3 < 1.1920928955078125e-7) {
   d22 = 0.0;
   STACKTOP = i1;
   return +d22;
  }
  d22 = 1.0 / d3;
  HEAPF32[i11 >> 2] = d8 * d22;
  HEAPF32[i2 + 96 >> 2] = d4 * d22;
  d22 = d3;
  STACKTOP = i1;
  return +d22;
 }
 i14 = i11 + 6 | 0;
 i21 = i11 + 7 | 0;
 i23 = i2 + 80 | 0;
 if ((HEAP8[i14] | 0) == (HEAP8[i21] | 0)) {
  HEAP32[i23 >> 2] = 2;
  i23 = HEAPU8[i11 + 9 | 0] | 0;
  i21 = HEAP32[i12 + 20 >> 2] | 0;
  if ((i21 | 0) <= (i23 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i12 = HEAP32[i12 + 16 >> 2] | 0;
  i27 = i12 + (i23 << 3) | 0;
  d16 = +HEAPF32[i27 >> 2];
  d15 = +HEAPF32[i27 + 4 >> 2];
  i11 = HEAPU8[i11 + 10 | 0] | 0;
  if ((i21 | 0) <= (i11 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i11 = i12 + (i11 << 3) | 0;
  d20 = +HEAPF32[i11 >> 2];
  d18 = +HEAPF32[i11 + 4 >> 2];
  i11 = i2 + 92 | 0;
  d22 = d20 - d16;
  d19 = d18 - d15;
  d17 = -d22;
  d29 = +d19;
  d28 = +d17;
  i27 = i11;
  HEAPF32[i27 >> 2] = d29;
  HEAPF32[i27 + 4 >> 2] = d28;
  d22 = +Math_sqrt(+(d19 * d19 + d22 * d22));
  if (!(d22 < 1.1920928955078125e-7)) {
   d29 = 1.0 / d22;
   d19 = d19 * d29;
   HEAPF32[i11 >> 2] = d19;
   d17 = d29 * d17;
   HEAPF32[i2 + 96 >> 2] = d17;
  }
  d16 = (d16 + d20) * .5;
  d15 = (d15 + d18) * .5;
  d28 = +d16;
  d29 = +d15;
  i2 = i2 + 84 | 0;
  HEAPF32[i2 >> 2] = d28;
  HEAPF32[i2 + 4 >> 2] = d29;
  i2 = HEAPU8[i14] | 0;
  if ((HEAP32[i13 + 20 >> 2] | 0) <= (i2 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i27 = (HEAP32[i13 + 16 >> 2] | 0) + (i2 << 3) | 0;
  d28 = +HEAPF32[i27 >> 2];
  d29 = +HEAPF32[i27 + 4 >> 2];
  d3 = (d7 * d19 - d6 * d17) * (d8 + (d4 * d28 - d3 * d29) - (d10 + (d7 * d16 - d6 * d15))) + (d6 * d19 + d7 * d17) * (d5 + (d3 * d28 + d4 * d29) - (d9 + (d6 * d16 + d7 * d15)));
  if (!(d3 < 0.0)) {
   d29 = d3;
   STACKTOP = i1;
   return +d29;
  }
  d28 = +-d19;
  d29 = +-d17;
  i27 = i11;
  HEAPF32[i27 >> 2] = d28;
  HEAPF32[i27 + 4 >> 2] = d29;
  d29 = -d3;
  STACKTOP = i1;
  return +d29;
 } else {
  HEAP32[i23 >> 2] = 1;
  i23 = HEAPU8[i14] | 0;
  i14 = HEAP32[i13 + 20 >> 2] | 0;
  if ((i14 | 0) <= (i23 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i13 = HEAP32[i13 + 16 >> 2] | 0;
  i27 = i13 + (i23 << 3) | 0;
  d16 = +HEAPF32[i27 >> 2];
  d15 = +HEAPF32[i27 + 4 >> 2];
  i21 = HEAPU8[i21] | 0;
  if ((i14 | 0) <= (i21 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i13 = i13 + (i21 << 3) | 0;
  d20 = +HEAPF32[i13 >> 2];
  d18 = +HEAPF32[i13 + 4 >> 2];
  i13 = i2 + 92 | 0;
  d22 = d20 - d16;
  d19 = d18 - d15;
  d17 = -d22;
  d28 = +d19;
  d29 = +d17;
  i27 = i13;
  HEAPF32[i27 >> 2] = d28;
  HEAPF32[i27 + 4 >> 2] = d29;
  d22 = +Math_sqrt(+(d19 * d19 + d22 * d22));
  if (!(d22 < 1.1920928955078125e-7)) {
   d29 = 1.0 / d22;
   d19 = d19 * d29;
   HEAPF32[i13 >> 2] = d19;
   d17 = d29 * d17;
   HEAPF32[i2 + 96 >> 2] = d17;
  }
  d16 = (d16 + d20) * .5;
  d15 = (d15 + d18) * .5;
  d28 = +d16;
  d29 = +d15;
  i2 = i2 + 84 | 0;
  HEAPF32[i2 >> 2] = d28;
  HEAPF32[i2 + 4 >> 2] = d29;
  i2 = HEAPU8[i11 + 9 | 0] | 0;
  if ((HEAP32[i12 + 20 >> 2] | 0) <= (i2 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i27 = (HEAP32[i12 + 16 >> 2] | 0) + (i2 << 3) | 0;
  d28 = +HEAPF32[i27 >> 2];
  d29 = +HEAPF32[i27 + 4 >> 2];
  d3 = (d4 * d19 - d3 * d17) * (d10 + (d7 * d28 - d6 * d29) - (d8 + (d4 * d16 - d3 * d15))) + (d3 * d19 + d4 * d17) * (d9 + (d6 * d28 + d7 * d29) - (d5 + (d3 * d16 + d4 * d15)));
  if (!(d3 < 0.0)) {
   d29 = d3;
   STACKTOP = i1;
   return +d29;
  }
  d28 = +-d19;
  d29 = +-d17;
  i27 = i13;
  HEAPF32[i27 >> 2] = d28;
  HEAPF32[i27 + 4 >> 2] = d29;
  d29 = -d3;
  STACKTOP = i1;
  return +d29;
 }
 return 0.0;
}
function __ZNK20b2SeparationFunction17FindMinSeparationEPiS0_f(i12, i10, i9, d5) {
 i12 = i12 | 0;
 i10 = i10 | 0;
 i9 = i9 | 0;
 d5 = +d5;
 var i1 = 0, d2 = 0.0, d3 = 0.0, d4 = 0.0, d6 = 0.0, d7 = 0.0, d8 = 0.0, d11 = 0.0, d13 = 0.0, d14 = 0.0, i15 = 0, i16 = 0, d17 = 0.0, d18 = 0.0, i19 = 0, d20 = 0.0, d21 = 0.0, i22 = 0, d23 = 0.0, d24 = 0.0, i25 = 0, i26 = 0, i27 = 0;
 i1 = STACKTOP;
 d21 = 1.0 - d5;
 d6 = d21 * +HEAPF32[i12 + 32 >> 2] + +HEAPF32[i12 + 36 >> 2] * d5;
 d7 = +Math_sin(+d6);
 d6 = +Math_cos(+d6);
 d3 = +HEAPF32[i12 + 8 >> 2];
 d8 = +HEAPF32[i12 + 12 >> 2];
 d11 = d21 * +HEAPF32[i12 + 16 >> 2] + +HEAPF32[i12 + 24 >> 2] * d5 - (d6 * d3 - d7 * d8);
 d8 = d21 * +HEAPF32[i12 + 20 >> 2] + +HEAPF32[i12 + 28 >> 2] * d5 - (d7 * d3 + d6 * d8);
 d3 = d21 * +HEAPF32[i12 + 68 >> 2] + +HEAPF32[i12 + 72 >> 2] * d5;
 d2 = +Math_sin(+d3);
 d3 = +Math_cos(+d3);
 d23 = +HEAPF32[i12 + 44 >> 2];
 d24 = +HEAPF32[i12 + 48 >> 2];
 d4 = d21 * +HEAPF32[i12 + 52 >> 2] + +HEAPF32[i12 + 60 >> 2] * d5 - (d3 * d23 - d2 * d24);
 d5 = d21 * +HEAPF32[i12 + 56 >> 2] + +HEAPF32[i12 + 64 >> 2] * d5 - (d2 * d23 + d3 * d24);
 i19 = HEAP32[i12 + 80 >> 2] | 0;
 if ((i19 | 0) == 1) {
  d23 = +HEAPF32[i12 + 92 >> 2];
  d14 = +HEAPF32[i12 + 96 >> 2];
  d13 = d6 * d23 - d7 * d14;
  d14 = d7 * d23 + d6 * d14;
  d23 = +HEAPF32[i12 + 84 >> 2];
  d24 = +HEAPF32[i12 + 88 >> 2];
  d11 = d11 + (d6 * d23 - d7 * d24);
  d6 = d8 + (d7 * d23 + d6 * d24);
  d7 = -d13;
  d24 = -d14;
  d8 = d3 * d7 + d2 * d24;
  d7 = d3 * d24 - d2 * d7;
  HEAP32[i10 >> 2] = -1;
  i25 = i12 + 4 | 0;
  i22 = HEAP32[i25 >> 2] | 0;
  i19 = HEAP32[i22 + 16 >> 2] | 0;
  i22 = HEAP32[i22 + 20 >> 2] | 0;
  if ((i22 | 0) > 1) {
   i10 = 0;
   d18 = d7 * +HEAPF32[i19 + 4 >> 2] + d8 * +HEAPF32[i19 >> 2];
   i12 = 1;
   while (1) {
    d17 = d8 * +HEAPF32[i19 + (i12 << 3) >> 2] + d7 * +HEAPF32[i19 + (i12 << 3) + 4 >> 2];
    i16 = d17 > d18;
    i10 = i16 ? i12 : i10;
    i12 = i12 + 1 | 0;
    if ((i12 | 0) == (i22 | 0)) {
     break;
    } else {
     d18 = i16 ? d17 : d18;
    }
   }
   HEAP32[i9 >> 2] = i10;
   if ((i10 | 0) > -1) {
    i15 = i10;
   } else {
    ___assert_fail(3640, 3672, 103, 3704);
   }
  } else {
   HEAP32[i9 >> 2] = 0;
   i15 = 0;
  }
  i9 = HEAP32[i25 >> 2] | 0;
  if ((HEAP32[i9 + 20 >> 2] | 0) <= (i15 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i27 = (HEAP32[i9 + 16 >> 2] | 0) + (i15 << 3) | 0;
  d23 = +HEAPF32[i27 >> 2];
  d24 = +HEAPF32[i27 + 4 >> 2];
  d24 = d13 * (d4 + (d3 * d23 - d2 * d24) - d11) + d14 * (d5 + (d2 * d23 + d3 * d24) - d6);
  STACKTOP = i1;
  return +d24;
 } else if ((i19 | 0) == 0) {
  d13 = +HEAPF32[i12 + 92 >> 2];
  d14 = +HEAPF32[i12 + 96 >> 2];
  d21 = d6 * d13 + d7 * d14;
  d24 = d6 * d14 - d7 * d13;
  d17 = -d13;
  d23 = -d14;
  d18 = d3 * d17 + d2 * d23;
  d17 = d3 * d23 - d2 * d17;
  i15 = HEAP32[i12 >> 2] | 0;
  i16 = HEAP32[i15 + 16 >> 2] | 0;
  i15 = i15 + 20 | 0;
  i19 = HEAP32[i15 >> 2] | 0;
  if ((i19 | 0) > 1) {
   i25 = 0;
   d23 = d24 * +HEAPF32[i16 + 4 >> 2] + d21 * +HEAPF32[i16 >> 2];
   i26 = 1;
   while (1) {
    d20 = d21 * +HEAPF32[i16 + (i26 << 3) >> 2] + d24 * +HEAPF32[i16 + (i26 << 3) + 4 >> 2];
    i22 = d20 > d23;
    i25 = i22 ? i26 : i25;
    i26 = i26 + 1 | 0;
    if ((i26 | 0) == (i19 | 0)) {
     break;
    } else {
     d23 = i22 ? d20 : d23;
    }
   }
  } else {
   i25 = 0;
  }
  HEAP32[i10 >> 2] = i25;
  i19 = HEAP32[i12 + 4 >> 2] | 0;
  i12 = HEAP32[i19 + 16 >> 2] | 0;
  i19 = i19 + 20 | 0;
  i25 = HEAP32[i19 >> 2] | 0;
  if ((i25 | 0) > 1) {
   i27 = 0;
   d20 = d17 * +HEAPF32[i12 + 4 >> 2] + d18 * +HEAPF32[i12 >> 2];
   i26 = 1;
   while (1) {
    d21 = d18 * +HEAPF32[i12 + (i26 << 3) >> 2] + d17 * +HEAPF32[i12 + (i26 << 3) + 4 >> 2];
    i22 = d21 > d20;
    i27 = i22 ? i26 : i27;
    i26 = i26 + 1 | 0;
    if ((i26 | 0) == (i25 | 0)) {
     break;
    } else {
     d20 = i22 ? d21 : d20;
    }
   }
  } else {
   i27 = 0;
  }
  HEAP32[i9 >> 2] = i27;
  i9 = HEAP32[i10 >> 2] | 0;
  if (!((i9 | 0) > -1)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  if ((HEAP32[i15 >> 2] | 0) <= (i9 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i26 = i16 + (i9 << 3) | 0;
  d18 = +HEAPF32[i26 >> 2];
  d17 = +HEAPF32[i26 + 4 >> 2];
  if (!((i27 | 0) > -1)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  if ((HEAP32[i19 >> 2] | 0) <= (i27 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i27 = i12 + (i27 << 3) | 0;
  d23 = +HEAPF32[i27 >> 2];
  d24 = +HEAPF32[i27 + 4 >> 2];
  d24 = d13 * (d4 + (d3 * d23 - d2 * d24) - (d11 + (d6 * d18 - d7 * d17))) + d14 * (d5 + (d2 * d23 + d3 * d24) - (d8 + (d7 * d18 + d6 * d17)));
  STACKTOP = i1;
  return +d24;
 } else if ((i19 | 0) == 2) {
  d23 = +HEAPF32[i12 + 92 >> 2];
  d13 = +HEAPF32[i12 + 96 >> 2];
  d14 = d3 * d23 - d2 * d13;
  d13 = d2 * d23 + d3 * d13;
  d23 = +HEAPF32[i12 + 84 >> 2];
  d24 = +HEAPF32[i12 + 88 >> 2];
  d4 = d4 + (d3 * d23 - d2 * d24);
  d2 = d5 + (d2 * d23 + d3 * d24);
  d3 = -d14;
  d24 = -d13;
  d5 = d6 * d3 + d7 * d24;
  d3 = d6 * d24 - d7 * d3;
  HEAP32[i9 >> 2] = -1;
  i22 = HEAP32[i12 >> 2] | 0;
  i15 = HEAP32[i22 + 16 >> 2] | 0;
  i22 = HEAP32[i22 + 20 >> 2] | 0;
  if ((i22 | 0) > 1) {
   i9 = 0;
   d17 = d3 * +HEAPF32[i15 + 4 >> 2] + d5 * +HEAPF32[i15 >> 2];
   i19 = 1;
   while (1) {
    d18 = d5 * +HEAPF32[i15 + (i19 << 3) >> 2] + d3 * +HEAPF32[i15 + (i19 << 3) + 4 >> 2];
    i25 = d18 > d17;
    i9 = i25 ? i19 : i9;
    i19 = i19 + 1 | 0;
    if ((i19 | 0) == (i22 | 0)) {
     break;
    } else {
     d17 = i25 ? d18 : d17;
    }
   }
   HEAP32[i10 >> 2] = i9;
   if ((i9 | 0) > -1) {
    i16 = i9;
   } else {
    ___assert_fail(3640, 3672, 103, 3704);
   }
  } else {
   HEAP32[i10 >> 2] = 0;
   i16 = 0;
  }
  i9 = HEAP32[i12 >> 2] | 0;
  if ((HEAP32[i9 + 20 >> 2] | 0) <= (i16 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i27 = (HEAP32[i9 + 16 >> 2] | 0) + (i16 << 3) | 0;
  d23 = +HEAPF32[i27 >> 2];
  d24 = +HEAPF32[i27 + 4 >> 2];
  d24 = d14 * (d11 + (d6 * d23 - d7 * d24) - d4) + d13 * (d8 + (d7 * d23 + d6 * d24) - d2);
  STACKTOP = i1;
  return +d24;
 } else {
  ___assert_fail(3616, 3560, 183, 3720);
 }
 return 0.0;
}
function __ZN13b2DynamicTree10InsertLeafEi(i3, i4) {
 i3 = i3 | 0;
 i4 = i4 | 0;
 var i1 = 0, i2 = 0, d5 = 0.0, d6 = 0.0, d7 = 0.0, d8 = 0.0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, d13 = 0.0, d14 = 0.0, d15 = 0.0, d16 = 0.0, d17 = 0.0, d18 = 0.0, d19 = 0.0, d20 = 0.0, d21 = 0.0, d22 = 0.0, d23 = 0.0, i24 = 0;
 i1 = STACKTOP;
 i11 = i3 + 24 | 0;
 HEAP32[i11 >> 2] = (HEAP32[i11 >> 2] | 0) + 1;
 i11 = HEAP32[i3 >> 2] | 0;
 if ((i11 | 0) == -1) {
  HEAP32[i3 >> 2] = i4;
  HEAP32[(HEAP32[i3 + 4 >> 2] | 0) + (i4 * 36 | 0) + 20 >> 2] = -1;
  STACKTOP = i1;
  return;
 }
 i2 = i3 + 4 | 0;
 i9 = HEAP32[i2 >> 2] | 0;
 d8 = +HEAPF32[i9 + (i4 * 36 | 0) >> 2];
 d7 = +HEAPF32[i9 + (i4 * 36 | 0) + 4 >> 2];
 d6 = +HEAPF32[i9 + (i4 * 36 | 0) + 8 >> 2];
 d5 = +HEAPF32[i9 + (i4 * 36 | 0) + 12 >> 2];
 i10 = HEAP32[i9 + (i11 * 36 | 0) + 24 >> 2] | 0;
 L5 : do {
  if (!((i10 | 0) == -1)) {
   do {
    i12 = HEAP32[i9 + (i11 * 36 | 0) + 28 >> 2] | 0;
    d14 = +HEAPF32[i9 + (i11 * 36 | 0) + 8 >> 2];
    d15 = +HEAPF32[i9 + (i11 * 36 | 0) >> 2];
    d17 = +HEAPF32[i9 + (i11 * 36 | 0) + 12 >> 2];
    d16 = +HEAPF32[i9 + (i11 * 36 | 0) + 4 >> 2];
    d21 = ((d14 > d6 ? d14 : d6) - (d15 < d8 ? d15 : d8) + ((d17 > d5 ? d17 : d5) - (d16 < d7 ? d16 : d7))) * 2.0;
    d13 = d21 * 2.0;
    d14 = (d21 - (d14 - d15 + (d17 - d16)) * 2.0) * 2.0;
    d21 = +HEAPF32[i9 + (i10 * 36 | 0) >> 2];
    d16 = d8 < d21 ? d8 : d21;
    d17 = +HEAPF32[i9 + (i10 * 36 | 0) + 4 >> 2];
    d18 = d7 < d17 ? d7 : d17;
    d19 = +HEAPF32[i9 + (i10 * 36 | 0) + 8 >> 2];
    d20 = d6 > d19 ? d6 : d19;
    d15 = +HEAPF32[i9 + (i10 * 36 | 0) + 12 >> 2];
    d22 = d5 > d15 ? d5 : d15;
    if ((HEAP32[i9 + (i10 * 36 | 0) + 24 >> 2] | 0) == -1) {
     d15 = (d20 - d16 + (d22 - d18)) * 2.0;
    } else {
     d15 = (d20 - d16 + (d22 - d18)) * 2.0 - (d19 - d21 + (d15 - d17)) * 2.0;
    }
    d15 = d14 + d15;
    d17 = +HEAPF32[i9 + (i12 * 36 | 0) >> 2];
    d18 = d8 < d17 ? d8 : d17;
    d23 = +HEAPF32[i9 + (i12 * 36 | 0) + 4 >> 2];
    d22 = d7 < d23 ? d7 : d23;
    d21 = +HEAPF32[i9 + (i12 * 36 | 0) + 8 >> 2];
    d20 = d6 > d21 ? d6 : d21;
    d19 = +HEAPF32[i9 + (i12 * 36 | 0) + 12 >> 2];
    d16 = d5 > d19 ? d5 : d19;
    if ((HEAP32[i9 + (i12 * 36 | 0) + 24 >> 2] | 0) == -1) {
     d16 = (d20 - d18 + (d16 - d22)) * 2.0;
    } else {
     d16 = (d20 - d18 + (d16 - d22)) * 2.0 - (d21 - d17 + (d19 - d23)) * 2.0;
    }
    d14 = d14 + d16;
    if (d13 < d15 & d13 < d14) {
     break L5;
    }
    i11 = d15 < d14 ? i10 : i12;
    i10 = HEAP32[i9 + (i11 * 36 | 0) + 24 >> 2] | 0;
   } while (!((i10 | 0) == -1));
  }
 } while (0);
 i9 = HEAP32[i9 + (i11 * 36 | 0) + 20 >> 2] | 0;
 i10 = __ZN13b2DynamicTree12AllocateNodeEv(i3) | 0;
 i12 = HEAP32[i2 >> 2] | 0;
 HEAP32[i12 + (i10 * 36 | 0) + 20 >> 2] = i9;
 HEAP32[i12 + (i10 * 36 | 0) + 16 >> 2] = 0;
 i12 = HEAP32[i2 >> 2] | 0;
 d14 = +HEAPF32[i12 + (i11 * 36 | 0) >> 2];
 d13 = +HEAPF32[i12 + (i11 * 36 | 0) + 4 >> 2];
 d8 = +(d8 < d14 ? d8 : d14);
 d7 = +(d7 < d13 ? d7 : d13);
 i24 = i12 + (i10 * 36 | 0) | 0;
 HEAPF32[i24 >> 2] = d8;
 HEAPF32[i24 + 4 >> 2] = d7;
 d8 = +HEAPF32[i12 + (i11 * 36 | 0) + 8 >> 2];
 d7 = +HEAPF32[i12 + (i11 * 36 | 0) + 12 >> 2];
 d6 = +(d6 > d8 ? d6 : d8);
 d23 = +(d5 > d7 ? d5 : d7);
 i12 = i12 + (i10 * 36 | 0) + 8 | 0;
 HEAPF32[i12 >> 2] = d6;
 HEAPF32[i12 + 4 >> 2] = d23;
 i12 = HEAP32[i2 >> 2] | 0;
 HEAP32[i12 + (i10 * 36 | 0) + 32 >> 2] = (HEAP32[i12 + (i11 * 36 | 0) + 32 >> 2] | 0) + 1;
 if ((i9 | 0) == -1) {
  HEAP32[i12 + (i10 * 36 | 0) + 24 >> 2] = i11;
  HEAP32[i12 + (i10 * 36 | 0) + 28 >> 2] = i4;
  HEAP32[i12 + (i11 * 36 | 0) + 20 >> 2] = i10;
  i24 = i12 + (i4 * 36 | 0) + 20 | 0;
  HEAP32[i24 >> 2] = i10;
  HEAP32[i3 >> 2] = i10;
  i10 = HEAP32[i24 >> 2] | 0;
 } else {
  i24 = i12 + (i9 * 36 | 0) + 24 | 0;
  if ((HEAP32[i24 >> 2] | 0) == (i11 | 0)) {
   HEAP32[i24 >> 2] = i10;
  } else {
   HEAP32[i12 + (i9 * 36 | 0) + 28 >> 2] = i10;
  }
  HEAP32[i12 + (i10 * 36 | 0) + 24 >> 2] = i11;
  HEAP32[i12 + (i10 * 36 | 0) + 28 >> 2] = i4;
  HEAP32[i12 + (i11 * 36 | 0) + 20 >> 2] = i10;
  HEAP32[i12 + (i4 * 36 | 0) + 20 >> 2] = i10;
 }
 if ((i10 | 0) == -1) {
  STACKTOP = i1;
  return;
 }
 while (1) {
  i9 = __ZN13b2DynamicTree7BalanceEi(i3, i10) | 0;
  i4 = HEAP32[i2 >> 2] | 0;
  i11 = HEAP32[i4 + (i9 * 36 | 0) + 24 >> 2] | 0;
  i10 = HEAP32[i4 + (i9 * 36 | 0) + 28 >> 2] | 0;
  if ((i11 | 0) == -1) {
   i2 = 20;
   break;
  }
  if ((i10 | 0) == -1) {
   i2 = 22;
   break;
  }
  i12 = HEAP32[i4 + (i11 * 36 | 0) + 32 >> 2] | 0;
  i24 = HEAP32[i4 + (i10 * 36 | 0) + 32 >> 2] | 0;
  HEAP32[i4 + (i9 * 36 | 0) + 32 >> 2] = ((i12 | 0) > (i24 | 0) ? i12 : i24) + 1;
  d7 = +HEAPF32[i4 + (i11 * 36 | 0) >> 2];
  d8 = +HEAPF32[i4 + (i10 * 36 | 0) >> 2];
  d5 = +HEAPF32[i4 + (i11 * 36 | 0) + 4 >> 2];
  d6 = +HEAPF32[i4 + (i10 * 36 | 0) + 4 >> 2];
  d7 = +(d7 < d8 ? d7 : d8);
  d5 = +(d5 < d6 ? d5 : d6);
  i24 = i4 + (i9 * 36 | 0) | 0;
  HEAPF32[i24 >> 2] = d7;
  HEAPF32[i24 + 4 >> 2] = d5;
  d5 = +HEAPF32[i4 + (i11 * 36 | 0) + 8 >> 2];
  d6 = +HEAPF32[i4 + (i10 * 36 | 0) + 8 >> 2];
  d7 = +HEAPF32[i4 + (i11 * 36 | 0) + 12 >> 2];
  d8 = +HEAPF32[i4 + (i10 * 36 | 0) + 12 >> 2];
  d5 = +(d5 > d6 ? d5 : d6);
  d23 = +(d7 > d8 ? d7 : d8);
  i10 = i4 + (i9 * 36 | 0) + 8 | 0;
  HEAPF32[i10 >> 2] = d5;
  HEAPF32[i10 + 4 >> 2] = d23;
  i10 = HEAP32[(HEAP32[i2 >> 2] | 0) + (i9 * 36 | 0) + 20 >> 2] | 0;
  if ((i10 | 0) == -1) {
   i2 = 24;
   break;
  }
 }
 if ((i2 | 0) == 20) {
  ___assert_fail(3168, 2944, 307, 3184);
 } else if ((i2 | 0) == 22) {
  ___assert_fail(3200, 2944, 308, 3184);
 } else if ((i2 | 0) == 24) {
  STACKTOP = i1;
  return;
 }
}
function __ZN15b2ContactSolverC2EP18b2ContactSolverDef(i7, i5) {
 i7 = i7 | 0;
 i5 = i5 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, i6 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, d15 = 0.0, d16 = 0.0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0;
 i1 = STACKTOP;
 HEAP32[i7 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
 HEAP32[i7 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
 HEAP32[i7 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
 HEAP32[i7 + 12 >> 2] = HEAP32[i5 + 12 >> 2];
 HEAP32[i7 + 16 >> 2] = HEAP32[i5 + 16 >> 2];
 HEAP32[i7 + 20 >> 2] = HEAP32[i5 + 20 >> 2];
 i14 = HEAP32[i5 + 40 >> 2] | 0;
 i9 = i7 + 32 | 0;
 HEAP32[i9 >> 2] = i14;
 i2 = HEAP32[i5 + 28 >> 2] | 0;
 i4 = i7 + 48 | 0;
 HEAP32[i4 >> 2] = i2;
 i3 = i7 + 36 | 0;
 HEAP32[i3 >> 2] = __ZN16b2StackAllocator8AllocateEi(i14, i2 * 88 | 0) | 0;
 i2 = i7 + 40 | 0;
 HEAP32[i2 >> 2] = __ZN16b2StackAllocator8AllocateEi(HEAP32[i9 >> 2] | 0, (HEAP32[i4 >> 2] | 0) * 152 | 0) | 0;
 HEAP32[i7 + 24 >> 2] = HEAP32[i5 + 32 >> 2];
 HEAP32[i7 + 28 >> 2] = HEAP32[i5 + 36 >> 2];
 i9 = HEAP32[i5 + 24 >> 2] | 0;
 i5 = i7 + 44 | 0;
 HEAP32[i5 >> 2] = i9;
 if ((HEAP32[i4 >> 2] | 0) <= 0) {
  STACKTOP = i1;
  return;
 }
 i6 = i7 + 20 | 0;
 i7 = i7 + 8 | 0;
 i8 = 0;
 while (1) {
  i10 = HEAP32[i9 + (i8 << 2) >> 2] | 0;
  i11 = HEAP32[i10 + 48 >> 2] | 0;
  i12 = HEAP32[i10 + 52 >> 2] | 0;
  i14 = HEAP32[i11 + 8 >> 2] | 0;
  i13 = HEAP32[i12 + 8 >> 2] | 0;
  i9 = HEAP32[i10 + 124 >> 2] | 0;
  if ((i9 | 0) <= 0) {
   i2 = 4;
   break;
  }
  d15 = +HEAPF32[(HEAP32[i12 + 12 >> 2] | 0) + 8 >> 2];
  d16 = +HEAPF32[(HEAP32[i11 + 12 >> 2] | 0) + 8 >> 2];
  i12 = HEAP32[i2 >> 2] | 0;
  HEAPF32[i12 + (i8 * 152 | 0) + 136 >> 2] = +HEAPF32[i10 + 136 >> 2];
  HEAPF32[i12 + (i8 * 152 | 0) + 140 >> 2] = +HEAPF32[i10 + 140 >> 2];
  i22 = i14 + 8 | 0;
  HEAP32[i12 + (i8 * 152 | 0) + 112 >> 2] = HEAP32[i22 >> 2];
  i21 = i13 + 8 | 0;
  HEAP32[i12 + (i8 * 152 | 0) + 116 >> 2] = HEAP32[i21 >> 2];
  i19 = i14 + 120 | 0;
  HEAPF32[i12 + (i8 * 152 | 0) + 120 >> 2] = +HEAPF32[i19 >> 2];
  i20 = i13 + 120 | 0;
  HEAPF32[i12 + (i8 * 152 | 0) + 124 >> 2] = +HEAPF32[i20 >> 2];
  i18 = i14 + 128 | 0;
  HEAPF32[i12 + (i8 * 152 | 0) + 128 >> 2] = +HEAPF32[i18 >> 2];
  i17 = i13 + 128 | 0;
  HEAPF32[i12 + (i8 * 152 | 0) + 132 >> 2] = +HEAPF32[i17 >> 2];
  HEAP32[i12 + (i8 * 152 | 0) + 148 >> 2] = i8;
  HEAP32[i12 + (i8 * 152 | 0) + 144 >> 2] = i9;
  i11 = i12 + (i8 * 152 | 0) + 80 | 0;
  HEAP32[i11 + 0 >> 2] = 0;
  HEAP32[i11 + 4 >> 2] = 0;
  HEAP32[i11 + 8 >> 2] = 0;
  HEAP32[i11 + 12 >> 2] = 0;
  HEAP32[i11 + 16 >> 2] = 0;
  HEAP32[i11 + 20 >> 2] = 0;
  HEAP32[i11 + 24 >> 2] = 0;
  HEAP32[i11 + 28 >> 2] = 0;
  i11 = HEAP32[i3 >> 2] | 0;
  HEAP32[i11 + (i8 * 88 | 0) + 32 >> 2] = HEAP32[i22 >> 2];
  HEAP32[i11 + (i8 * 88 | 0) + 36 >> 2] = HEAP32[i21 >> 2];
  HEAPF32[i11 + (i8 * 88 | 0) + 40 >> 2] = +HEAPF32[i19 >> 2];
  HEAPF32[i11 + (i8 * 88 | 0) + 44 >> 2] = +HEAPF32[i20 >> 2];
  i20 = i14 + 28 | 0;
  i14 = HEAP32[i20 + 4 >> 2] | 0;
  i19 = i11 + (i8 * 88 | 0) + 48 | 0;
  HEAP32[i19 >> 2] = HEAP32[i20 >> 2];
  HEAP32[i19 + 4 >> 2] = i14;
  i19 = i13 + 28 | 0;
  i14 = HEAP32[i19 + 4 >> 2] | 0;
  i13 = i11 + (i8 * 88 | 0) + 56 | 0;
  HEAP32[i13 >> 2] = HEAP32[i19 >> 2];
  HEAP32[i13 + 4 >> 2] = i14;
  HEAPF32[i11 + (i8 * 88 | 0) + 64 >> 2] = +HEAPF32[i18 >> 2];
  HEAPF32[i11 + (i8 * 88 | 0) + 68 >> 2] = +HEAPF32[i17 >> 2];
  i13 = i10 + 104 | 0;
  i14 = HEAP32[i13 + 4 >> 2] | 0;
  i17 = i11 + (i8 * 88 | 0) + 16 | 0;
  HEAP32[i17 >> 2] = HEAP32[i13 >> 2];
  HEAP32[i17 + 4 >> 2] = i14;
  i17 = i10 + 112 | 0;
  i14 = HEAP32[i17 + 4 >> 2] | 0;
  i13 = i11 + (i8 * 88 | 0) + 24 | 0;
  HEAP32[i13 >> 2] = HEAP32[i17 >> 2];
  HEAP32[i13 + 4 >> 2] = i14;
  HEAP32[i11 + (i8 * 88 | 0) + 84 >> 2] = i9;
  HEAPF32[i11 + (i8 * 88 | 0) + 76 >> 2] = d16;
  HEAPF32[i11 + (i8 * 88 | 0) + 80 >> 2] = d15;
  HEAP32[i11 + (i8 * 88 | 0) + 72 >> 2] = HEAP32[i10 + 120 >> 2];
  i13 = 0;
  do {
   i14 = i10 + (i13 * 20 | 0) + 64 | 0;
   if ((HEAP8[i6] | 0) == 0) {
    HEAPF32[i12 + (i8 * 152 | 0) + (i13 * 36 | 0) + 16 >> 2] = 0.0;
    HEAPF32[i12 + (i8 * 152 | 0) + (i13 * 36 | 0) + 20 >> 2] = 0.0;
   } else {
    HEAPF32[i12 + (i8 * 152 | 0) + (i13 * 36 | 0) + 16 >> 2] = +HEAPF32[i7 >> 2] * +HEAPF32[i10 + (i13 * 20 | 0) + 72 >> 2];
    HEAPF32[i12 + (i8 * 152 | 0) + (i13 * 36 | 0) + 20 >> 2] = +HEAPF32[i7 >> 2] * +HEAPF32[i10 + (i13 * 20 | 0) + 76 >> 2];
   }
   i20 = i12 + (i8 * 152 | 0) + (i13 * 36 | 0) | 0;
   HEAPF32[i12 + (i8 * 152 | 0) + (i13 * 36 | 0) + 24 >> 2] = 0.0;
   HEAPF32[i12 + (i8 * 152 | 0) + (i13 * 36 | 0) + 28 >> 2] = 0.0;
   HEAPF32[i12 + (i8 * 152 | 0) + (i13 * 36 | 0) + 32 >> 2] = 0.0;
   i22 = i11 + (i8 * 88 | 0) + (i13 << 3) | 0;
   HEAP32[i20 + 0 >> 2] = 0;
   HEAP32[i20 + 4 >> 2] = 0;
   HEAP32[i20 + 8 >> 2] = 0;
   HEAP32[i20 + 12 >> 2] = 0;
   i20 = i14;
   i21 = HEAP32[i20 + 4 >> 2] | 0;
   HEAP32[i22 >> 2] = HEAP32[i20 >> 2];
   HEAP32[i22 + 4 >> 2] = i21;
   i13 = i13 + 1 | 0;
  } while ((i13 | 0) != (i9 | 0));
  i8 = i8 + 1 | 0;
  if ((i8 | 0) >= (HEAP32[i4 >> 2] | 0)) {
   i2 = 12;
   break;
  }
  i9 = HEAP32[i5 >> 2] | 0;
 }
 if ((i2 | 0) == 4) {
  ___assert_fail(6504, 6520, 71, 6568);
 } else if ((i2 | 0) == 12) {
  STACKTOP = i1;
  return;
 }
}
function __Z25b2CollidePolygonAndCircleP10b2ManifoldPK14b2PolygonShapeRK11b2TransformPK13b2CircleShapeS6_(i1, i4, i11, i9, i10) {
 i1 = i1 | 0;
 i4 = i4 | 0;
 i11 = i11 | 0;
 i9 = i9 | 0;
 i10 = i10 | 0;
 var i2 = 0, i3 = 0, i5 = 0, d6 = 0.0, d7 = 0.0, d8 = 0.0, i12 = 0, d13 = 0.0, d14 = 0.0, i15 = 0, d16 = 0.0, d17 = 0.0, d18 = 0.0, d19 = 0.0, d20 = 0.0, d21 = 0.0, i22 = 0;
 i3 = STACKTOP;
 i5 = i1 + 60 | 0;
 HEAP32[i5 >> 2] = 0;
 i2 = i9 + 12 | 0;
 d20 = +HEAPF32[i10 + 12 >> 2];
 d7 = +HEAPF32[i2 >> 2];
 d6 = +HEAPF32[i10 + 8 >> 2];
 d21 = +HEAPF32[i9 + 16 >> 2];
 d8 = +HEAPF32[i10 >> 2] + (d20 * d7 - d6 * d21) - +HEAPF32[i11 >> 2];
 d21 = d7 * d6 + d20 * d21 + +HEAPF32[i10 + 4 >> 2] - +HEAPF32[i11 + 4 >> 2];
 d20 = +HEAPF32[i11 + 12 >> 2];
 d6 = +HEAPF32[i11 + 8 >> 2];
 d7 = d8 * d20 + d21 * d6;
 d6 = d20 * d21 - d8 * d6;
 d8 = +HEAPF32[i4 + 8 >> 2] + +HEAPF32[i9 + 8 >> 2];
 i12 = HEAP32[i4 + 148 >> 2] | 0;
 do {
  if ((i12 | 0) > 0) {
   i10 = 0;
   i9 = 0;
   d13 = -3.4028234663852886e+38;
   while (1) {
    d14 = (d7 - +HEAPF32[i4 + (i10 << 3) + 20 >> 2]) * +HEAPF32[i4 + (i10 << 3) + 84 >> 2] + (d6 - +HEAPF32[i4 + (i10 << 3) + 24 >> 2]) * +HEAPF32[i4 + (i10 << 3) + 88 >> 2];
    if (d14 > d8) {
     i10 = 19;
     break;
    }
    i11 = d14 > d13;
    d13 = i11 ? d14 : d13;
    i9 = i11 ? i10 : i9;
    i10 = i10 + 1 | 0;
    if ((i10 | 0) >= (i12 | 0)) {
     i10 = 4;
     break;
    }
   }
   if ((i10 | 0) == 4) {
    i22 = d13 < 1.1920928955078125e-7;
    break;
   } else if ((i10 | 0) == 19) {
    STACKTOP = i3;
    return;
   }
  } else {
   i9 = 0;
   i22 = 1;
  }
 } while (0);
 i15 = i9 + 1 | 0;
 i11 = i4 + (i9 << 3) + 20 | 0;
 i10 = HEAP32[i11 >> 2] | 0;
 i11 = HEAP32[i11 + 4 >> 2] | 0;
 d14 = (HEAP32[tempDoublePtr >> 2] = i10, +HEAPF32[tempDoublePtr >> 2]);
 d13 = (HEAP32[tempDoublePtr >> 2] = i11, +HEAPF32[tempDoublePtr >> 2]);
 i12 = i4 + (((i15 | 0) < (i12 | 0) ? i15 : 0) << 3) + 20 | 0;
 i15 = HEAP32[i12 >> 2] | 0;
 i12 = HEAP32[i12 + 4 >> 2] | 0;
 d21 = (HEAP32[tempDoublePtr >> 2] = i15, +HEAPF32[tempDoublePtr >> 2]);
 d18 = (HEAP32[tempDoublePtr >> 2] = i12, +HEAPF32[tempDoublePtr >> 2]);
 if (i22) {
  HEAP32[i5 >> 2] = 1;
  HEAP32[i1 + 56 >> 2] = 1;
  i22 = i4 + (i9 << 3) + 84 | 0;
  i15 = HEAP32[i22 + 4 >> 2] | 0;
  i12 = i1 + 40 | 0;
  HEAP32[i12 >> 2] = HEAP32[i22 >> 2];
  HEAP32[i12 + 4 >> 2] = i15;
  d20 = +((d14 + d21) * .5);
  d21 = +((d13 + d18) * .5);
  i12 = i1 + 48 | 0;
  HEAPF32[i12 >> 2] = d20;
  HEAPF32[i12 + 4 >> 2] = d21;
  i12 = i2;
  i15 = HEAP32[i12 + 4 >> 2] | 0;
  i22 = i1;
  HEAP32[i22 >> 2] = HEAP32[i12 >> 2];
  HEAP32[i22 + 4 >> 2] = i15;
  HEAP32[i1 + 16 >> 2] = 0;
  STACKTOP = i3;
  return;
 }
 d16 = d7 - d14;
 d20 = d6 - d13;
 d19 = d7 - d21;
 d17 = d6 - d18;
 if (d16 * (d21 - d14) + d20 * (d18 - d13) <= 0.0) {
  if (d16 * d16 + d20 * d20 > d8 * d8) {
   STACKTOP = i3;
   return;
  }
  HEAP32[i5 >> 2] = 1;
  HEAP32[i1 + 56 >> 2] = 1;
  i4 = i1 + 40 | 0;
  d21 = +d16;
  d6 = +d20;
  i22 = i4;
  HEAPF32[i22 >> 2] = d21;
  HEAPF32[i22 + 4 >> 2] = d6;
  d6 = +Math_sqrt(+(d16 * d16 + d20 * d20));
  if (!(d6 < 1.1920928955078125e-7)) {
   d21 = 1.0 / d6;
   HEAPF32[i4 >> 2] = d16 * d21;
   HEAPF32[i1 + 44 >> 2] = d20 * d21;
  }
  i12 = i1 + 48 | 0;
  HEAP32[i12 >> 2] = i10;
  HEAP32[i12 + 4 >> 2] = i11;
  i12 = i2;
  i15 = HEAP32[i12 + 4 >> 2] | 0;
  i22 = i1;
  HEAP32[i22 >> 2] = HEAP32[i12 >> 2];
  HEAP32[i22 + 4 >> 2] = i15;
  HEAP32[i1 + 16 >> 2] = 0;
  STACKTOP = i3;
  return;
 }
 if (!(d19 * (d14 - d21) + d17 * (d13 - d18) <= 0.0)) {
  d14 = (d14 + d21) * .5;
  d13 = (d13 + d18) * .5;
  i10 = i4 + (i9 << 3) + 84 | 0;
  if ((d7 - d14) * +HEAPF32[i10 >> 2] + (d6 - d13) * +HEAPF32[i4 + (i9 << 3) + 88 >> 2] > d8) {
   STACKTOP = i3;
   return;
  }
  HEAP32[i5 >> 2] = 1;
  HEAP32[i1 + 56 >> 2] = 1;
  i22 = i10;
  i15 = HEAP32[i22 + 4 >> 2] | 0;
  i12 = i1 + 40 | 0;
  HEAP32[i12 >> 2] = HEAP32[i22 >> 2];
  HEAP32[i12 + 4 >> 2] = i15;
  d20 = +d14;
  d21 = +d13;
  i12 = i1 + 48 | 0;
  HEAPF32[i12 >> 2] = d20;
  HEAPF32[i12 + 4 >> 2] = d21;
  i12 = i2;
  i15 = HEAP32[i12 + 4 >> 2] | 0;
  i22 = i1;
  HEAP32[i22 >> 2] = HEAP32[i12 >> 2];
  HEAP32[i22 + 4 >> 2] = i15;
  HEAP32[i1 + 16 >> 2] = 0;
  STACKTOP = i3;
  return;
 }
 if (d19 * d19 + d17 * d17 > d8 * d8) {
  STACKTOP = i3;
  return;
 }
 HEAP32[i5 >> 2] = 1;
 HEAP32[i1 + 56 >> 2] = 1;
 i4 = i1 + 40 | 0;
 d21 = +d19;
 d6 = +d17;
 i22 = i4;
 HEAPF32[i22 >> 2] = d21;
 HEAPF32[i22 + 4 >> 2] = d6;
 d6 = +Math_sqrt(+(d19 * d19 + d17 * d17));
 if (!(d6 < 1.1920928955078125e-7)) {
  d21 = 1.0 / d6;
  HEAPF32[i4 >> 2] = d19 * d21;
  HEAPF32[i1 + 44 >> 2] = d17 * d21;
 }
 i22 = i1 + 48 | 0;
 HEAP32[i22 >> 2] = i15;
 HEAP32[i22 + 4 >> 2] = i12;
 i12 = i2;
 i15 = HEAP32[i12 + 4 >> 2] | 0;
 i22 = i1;
 HEAP32[i22 >> 2] = HEAP32[i12 >> 2];
 HEAP32[i22 + 4 >> 2] = i15;
 HEAP32[i1 + 16 >> 2] = 0;
 STACKTOP = i3;
 return;
}
function __ZN15b2WorldManifold10InitializeEPK10b2ManifoldRK11b2TransformfS5_f(i1, i5, i7, d4, i8, d3) {
 i1 = i1 | 0;
 i5 = i5 | 0;
 i7 = i7 | 0;
 d4 = +d4;
 i8 = i8 | 0;
 d3 = +d3;
 var i2 = 0, i6 = 0, d9 = 0.0, d10 = 0.0, i11 = 0, i12 = 0, i13 = 0, d14 = 0.0, d15 = 0.0, i16 = 0, d17 = 0.0, d18 = 0.0, d19 = 0.0, i20 = 0, d21 = 0.0, d22 = 0.0;
 i2 = STACKTOP;
 i6 = i5 + 60 | 0;
 if ((HEAP32[i6 >> 2] | 0) == 0) {
  STACKTOP = i2;
  return;
 }
 i11 = HEAP32[i5 + 56 >> 2] | 0;
 if ((i11 | 0) == 2) {
  i13 = i8 + 12 | 0;
  d17 = +HEAPF32[i13 >> 2];
  d18 = +HEAPF32[i5 + 40 >> 2];
  i16 = i8 + 8 | 0;
  d19 = +HEAPF32[i16 >> 2];
  d15 = +HEAPF32[i5 + 44 >> 2];
  d14 = d17 * d18 - d19 * d15;
  d15 = d18 * d19 + d17 * d15;
  d17 = +d14;
  d19 = +d15;
  i12 = i1;
  HEAPF32[i12 >> 2] = d17;
  HEAPF32[i12 + 4 >> 2] = d19;
  d19 = +HEAPF32[i13 >> 2];
  d17 = +HEAPF32[i5 + 48 >> 2];
  d18 = +HEAPF32[i16 >> 2];
  d10 = +HEAPF32[i5 + 52 >> 2];
  d9 = +HEAPF32[i8 >> 2] + (d19 * d17 - d18 * d10);
  d10 = d17 * d18 + d19 * d10 + +HEAPF32[i8 + 4 >> 2];
  if ((HEAP32[i6 >> 2] | 0) > 0) {
   i8 = i7 + 12 | 0;
   i11 = i7 + 8 | 0;
   i12 = i7 + 4 | 0;
   i13 = i1 + 4 | 0;
   i16 = 0;
   do {
    d18 = +HEAPF32[i8 >> 2];
    d22 = +HEAPF32[i5 + (i16 * 20 | 0) >> 2];
    d21 = +HEAPF32[i11 >> 2];
    d17 = +HEAPF32[i5 + (i16 * 20 | 0) + 4 >> 2];
    d19 = +HEAPF32[i7 >> 2] + (d18 * d22 - d21 * d17);
    d17 = d22 * d21 + d18 * d17 + +HEAPF32[i12 >> 2];
    d18 = d3 - (d14 * (d19 - d9) + (d17 - d10) * d15);
    d19 = +((d19 - d14 * d4 + (d19 + d14 * d18)) * .5);
    d14 = +((d17 - d15 * d4 + (d17 + d15 * d18)) * .5);
    i20 = i1 + (i16 << 3) + 8 | 0;
    HEAPF32[i20 >> 2] = d19;
    HEAPF32[i20 + 4 >> 2] = d14;
    i16 = i16 + 1 | 0;
    d14 = +HEAPF32[i1 >> 2];
    d15 = +HEAPF32[i13 >> 2];
   } while ((i16 | 0) < (HEAP32[i6 >> 2] | 0));
  }
  d21 = +-d14;
  d22 = +-d15;
  i20 = i1;
  HEAPF32[i20 >> 2] = d21;
  HEAPF32[i20 + 4 >> 2] = d22;
  STACKTOP = i2;
  return;
 } else if ((i11 | 0) == 1) {
  i16 = i7 + 12 | 0;
  d19 = +HEAPF32[i16 >> 2];
  d21 = +HEAPF32[i5 + 40 >> 2];
  i20 = i7 + 8 | 0;
  d22 = +HEAPF32[i20 >> 2];
  d15 = +HEAPF32[i5 + 44 >> 2];
  d14 = d19 * d21 - d22 * d15;
  d15 = d21 * d22 + d19 * d15;
  d19 = +d14;
  d22 = +d15;
  i13 = i1;
  HEAPF32[i13 >> 2] = d19;
  HEAPF32[i13 + 4 >> 2] = d22;
  d22 = +HEAPF32[i16 >> 2];
  d19 = +HEAPF32[i5 + 48 >> 2];
  d21 = +HEAPF32[i20 >> 2];
  d10 = +HEAPF32[i5 + 52 >> 2];
  d9 = +HEAPF32[i7 >> 2] + (d22 * d19 - d21 * d10);
  d10 = d19 * d21 + d22 * d10 + +HEAPF32[i7 + 4 >> 2];
  if ((HEAP32[i6 >> 2] | 0) <= 0) {
   STACKTOP = i2;
   return;
  }
  i12 = i8 + 12 | 0;
  i11 = i8 + 8 | 0;
  i7 = i8 + 4 | 0;
  i13 = i1 + 4 | 0;
  i16 = 0;
  while (1) {
   d22 = +HEAPF32[i12 >> 2];
   d17 = +HEAPF32[i5 + (i16 * 20 | 0) >> 2];
   d18 = +HEAPF32[i11 >> 2];
   d19 = +HEAPF32[i5 + (i16 * 20 | 0) + 4 >> 2];
   d21 = +HEAPF32[i8 >> 2] + (d22 * d17 - d18 * d19);
   d19 = d17 * d18 + d22 * d19 + +HEAPF32[i7 >> 2];
   d22 = d4 - (d14 * (d21 - d9) + (d19 - d10) * d15);
   d21 = +((d21 - d14 * d3 + (d21 + d14 * d22)) * .5);
   d22 = +((d19 - d15 * d3 + (d19 + d15 * d22)) * .5);
   i20 = i1 + (i16 << 3) + 8 | 0;
   HEAPF32[i20 >> 2] = d21;
   HEAPF32[i20 + 4 >> 2] = d22;
   i16 = i16 + 1 | 0;
   if ((i16 | 0) >= (HEAP32[i6 >> 2] | 0)) {
    break;
   }
   d14 = +HEAPF32[i1 >> 2];
   d15 = +HEAPF32[i13 >> 2];
  }
  STACKTOP = i2;
  return;
 } else if ((i11 | 0) == 0) {
  HEAPF32[i1 >> 2] = 1.0;
  i6 = i1 + 4 | 0;
  HEAPF32[i6 >> 2] = 0.0;
  d21 = +HEAPF32[i7 + 12 >> 2];
  d22 = +HEAPF32[i5 + 48 >> 2];
  d19 = +HEAPF32[i7 + 8 >> 2];
  d10 = +HEAPF32[i5 + 52 >> 2];
  d9 = +HEAPF32[i7 >> 2] + (d21 * d22 - d19 * d10);
  d10 = d22 * d19 + d21 * d10 + +HEAPF32[i7 + 4 >> 2];
  d21 = +HEAPF32[i8 + 12 >> 2];
  d19 = +HEAPF32[i5 >> 2];
  d22 = +HEAPF32[i8 + 8 >> 2];
  d15 = +HEAPF32[i5 + 4 >> 2];
  d14 = +HEAPF32[i8 >> 2] + (d21 * d19 - d22 * d15);
  d15 = d19 * d22 + d21 * d15 + +HEAPF32[i8 + 4 >> 2];
  d21 = d9 - d14;
  d22 = d10 - d15;
  if (d21 * d21 + d22 * d22 > 1.4210854715202004e-14) {
   d19 = d14 - d9;
   d17 = d15 - d10;
   d22 = +d19;
   d18 = +d17;
   i20 = i1;
   HEAPF32[i20 >> 2] = d22;
   HEAPF32[i20 + 4 >> 2] = d18;
   d18 = +Math_sqrt(+(d19 * d19 + d17 * d17));
   if (!(d18 < 1.1920928955078125e-7)) {
    d22 = 1.0 / d18;
    d19 = d19 * d22;
    HEAPF32[i1 >> 2] = d19;
    d17 = d17 * d22;
    HEAPF32[i6 >> 2] = d17;
   }
  } else {
   d19 = 1.0;
   d17 = 0.0;
  }
  d21 = +((d9 + d19 * d4 + (d14 - d19 * d3)) * .5);
  d22 = +((d10 + d17 * d4 + (d15 - d17 * d3)) * .5);
  i20 = i1 + 8 | 0;
  HEAPF32[i20 >> 2] = d21;
  HEAPF32[i20 + 4 >> 2] = d22;
  STACKTOP = i2;
  return;
 } else {
  STACKTOP = i2;
  return;
 }
}
function _main(i3, i2) {
 i3 = i3 | 0;
 i2 = i2 | 0;
 var i1 = 0, i4 = 0, i5 = 0, d6 = 0.0, d7 = 0.0, i8 = 0, i9 = 0, d10 = 0.0, d11 = 0.0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, d22 = 0.0, d23 = 0.0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 240 | 0;
 i5 = i1;
 i12 = i1 + 224 | 0;
 i4 = i1 + 168 | 0;
 i9 = i1 + 160 | 0;
 i8 = i1 + 152 | 0;
 L1 : do {
  if ((i3 | 0) > 1) {
   i14 = HEAP8[HEAP32[i2 + 4 >> 2] | 0] | 0;
   switch (i14 | 0) {
   case 49:
    {
     HEAP32[2] = 5;
     HEAP32[4] = 35;
     i15 = 35;
     i14 = 5;
     break L1;
    }
   case 50:
    {
     HEAP32[2] = 32;
     HEAP32[4] = 161;
     i15 = 161;
     i14 = 32;
     break L1;
    }
   case 51:
    {
     i13 = 5;
     break L1;
    }
   case 52:
    {
     HEAP32[2] = 320;
     HEAP32[4] = 2331;
     i15 = 2331;
     i14 = 320;
     break L1;
    }
   case 53:
    {
     HEAP32[2] = 640;
     HEAP32[4] = 5661;
     i15 = 5661;
     i14 = 640;
     break L1;
    }
   case 48:
    {
     i20 = 0;
     STACKTOP = i1;
     return i20 | 0;
    }
   default:
    {
     HEAP32[i5 >> 2] = i14 + -48;
     _printf(80, i5 | 0) | 0;
     i20 = -1;
     STACKTOP = i1;
     return i20 | 0;
    }
   }
  } else {
   i13 = 5;
  }
 } while (0);
 if ((i13 | 0) == 5) {
  HEAP32[2] = 64;
  HEAP32[4] = 333;
  i15 = 333;
  i14 = 64;
 }
 i13 = i15 + i14 | 0;
 HEAP32[4] = i13;
 HEAP32[2] = 0;
 HEAP32[8] = __Znaj(i13 >>> 0 > 1073741823 ? -1 : i13 << 2) | 0;
 HEAPF32[i12 >> 2] = 0.0;
 HEAPF32[i12 + 4 >> 2] = -10.0;
 i15 = __Znwj(103028) | 0;
 __ZN7b2WorldC2ERK6b2Vec2(i15, i12);
 HEAP32[6] = i15;
 __ZN7b2World16SetAllowSleepingEb(i15, 0);
 HEAP32[i5 + 44 >> 2] = 0;
 i15 = i5 + 4 | 0;
 i14 = i5 + 36 | 0;
 HEAP32[i15 + 0 >> 2] = 0;
 HEAP32[i15 + 4 >> 2] = 0;
 HEAP32[i15 + 8 >> 2] = 0;
 HEAP32[i15 + 12 >> 2] = 0;
 HEAP32[i15 + 16 >> 2] = 0;
 HEAP32[i15 + 20 >> 2] = 0;
 HEAP32[i15 + 24 >> 2] = 0;
 HEAP32[i15 + 28 >> 2] = 0;
 HEAP8[i14] = 1;
 HEAP8[i5 + 37 | 0] = 1;
 HEAP8[i5 + 38 | 0] = 0;
 HEAP8[i5 + 39 | 0] = 0;
 HEAP32[i5 >> 2] = 0;
 HEAP8[i5 + 40 | 0] = 1;
 HEAPF32[i5 + 48 >> 2] = 1.0;
 i14 = __ZN7b2World10CreateBodyEPK9b2BodyDef(HEAP32[6] | 0, i5) | 0;
 HEAP32[i4 >> 2] = 240;
 HEAP32[i4 + 4 >> 2] = 1;
 HEAPF32[i4 + 8 >> 2] = .009999999776482582;
 i15 = i4 + 28 | 0;
 HEAP32[i15 + 0 >> 2] = 0;
 HEAP32[i15 + 4 >> 2] = 0;
 HEAP32[i15 + 8 >> 2] = 0;
 HEAP32[i15 + 12 >> 2] = 0;
 HEAP16[i15 + 16 >> 1] = 0;
 HEAPF32[i9 >> 2] = -40.0;
 HEAPF32[i9 + 4 >> 2] = 0.0;
 HEAPF32[i8 >> 2] = 40.0;
 HEAPF32[i8 + 4 >> 2] = 0.0;
 __ZN11b2EdgeShape3SetERK6b2Vec2S2_(i4, i9, i8);
 __ZN6b2Body13CreateFixtureEPK7b2Shapef(i14, i4, 0.0) | 0;
 HEAP32[i5 >> 2] = 504;
 HEAP32[i5 + 4 >> 2] = 2;
 HEAPF32[i5 + 8 >> 2] = .009999999776482582;
 HEAP32[i5 + 148 >> 2] = 0;
 HEAPF32[i5 + 12 >> 2] = 0.0;
 HEAPF32[i5 + 16 >> 2] = 0.0;
 __ZN14b2PolygonShape8SetAsBoxEff(i5, .5, .5);
 i14 = i4 + 44 | 0;
 i15 = i4 + 4 | 0;
 i8 = i4 + 36 | 0;
 i17 = i4 + 37 | 0;
 i18 = i4 + 38 | 0;
 i19 = i4 + 39 | 0;
 i20 = i4 + 40 | 0;
 i13 = i4 + 48 | 0;
 i12 = i4 + 4 | 0;
 d11 = -7.0;
 d10 = .75;
 i9 = 0;
 while (1) {
  d7 = d11;
  d6 = d10;
  i16 = i9;
  while (1) {
   HEAP32[i14 >> 2] = 0;
   HEAP32[i15 + 0 >> 2] = 0;
   HEAP32[i15 + 4 >> 2] = 0;
   HEAP32[i15 + 8 >> 2] = 0;
   HEAP32[i15 + 12 >> 2] = 0;
   HEAP32[i15 + 16 >> 2] = 0;
   HEAP32[i15 + 20 >> 2] = 0;
   HEAP32[i15 + 24 >> 2] = 0;
   HEAP32[i15 + 28 >> 2] = 0;
   HEAP8[i8] = 1;
   HEAP8[i17] = 1;
   HEAP8[i18] = 0;
   HEAP8[i19] = 0;
   HEAP8[i20] = 1;
   HEAPF32[i13 >> 2] = 1.0;
   HEAP32[i4 >> 2] = 2;
   d23 = +d7;
   d22 = +d6;
   i21 = i12;
   HEAPF32[i21 >> 2] = d23;
   HEAPF32[i21 + 4 >> 2] = d22;
   i21 = __ZN7b2World10CreateBodyEPK9b2BodyDef(HEAP32[6] | 0, i4) | 0;
   __ZN6b2Body13CreateFixtureEPK7b2Shapef(i21, i5, 5.0) | 0;
   HEAP32[14] = i21;
   i16 = i16 + 1 | 0;
   if ((i16 | 0) >= 40) {
    break;
   } else {
    d7 = d7 + 1.125;
    d6 = d6 + 0.0;
   }
  }
  i9 = i9 + 1 | 0;
  if ((i9 | 0) >= 40) {
   break;
  } else {
   d11 = d11 + .5625;
   d10 = d10 + 1.0;
  }
 }
 if ((HEAP32[2] | 0) > 0) {
  i4 = 0;
  do {
   __ZN7b2World4StepEfii(HEAP32[6] | 0, .01666666753590107, 3, 3);
   i4 = i4 + 1 | 0;
  } while ((i4 | 0) < (HEAP32[2] | 0));
 }
 if ((i3 | 0) > 2) {
  i21 = (HEAP8[HEAP32[i2 + 8 >> 2] | 0] | 0) + -48 | 0;
  HEAP32[18] = i21;
  if ((i21 | 0) != 0) {
   _puts(208) | 0;
   _emscripten_set_main_loop(2, 60, 1);
   i21 = 0;
   STACKTOP = i1;
   return i21 | 0;
  }
 } else {
  HEAP32[18] = 0;
 }
 while (1) {
  __Z4iterv();
  if ((HEAP32[16] | 0) > (HEAP32[4] | 0)) {
   i2 = 0;
   break;
  }
 }
 STACKTOP = i1;
 return i2 | 0;
}
function __ZN9b2Simplex9ReadCacheEPK14b2SimplexCachePK15b2DistanceProxyRK11b2TransformS5_S8_(i2, i11, i10, i4, i3, i5) {
 i2 = i2 | 0;
 i11 = i11 | 0;
 i10 = i10 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i5 = i5 | 0;
 var i1 = 0, i6 = 0, i7 = 0, d8 = 0.0, i9 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, d24 = 0.0, d25 = 0.0, i26 = 0, d27 = 0.0, d28 = 0.0, d29 = 0.0, d30 = 0.0, d31 = 0.0, d32 = 0.0;
 i1 = STACKTOP;
 i13 = HEAP16[i11 + 4 >> 1] | 0;
 if (!((i13 & 65535) < 4)) {
  ___assert_fail(2872, 2672, 102, 2896);
 }
 i12 = i13 & 65535;
 i6 = i2 + 108 | 0;
 HEAP32[i6 >> 2] = i12;
 L4 : do {
  if (!(i13 << 16 >> 16 == 0)) {
   i17 = i10 + 20 | 0;
   i21 = i10 + 16 | 0;
   i13 = i3 + 20 | 0;
   i14 = i3 + 16 | 0;
   i15 = i4 + 12 | 0;
   i16 = i4 + 8 | 0;
   i12 = i4 + 4 | 0;
   i18 = i5 + 12 | 0;
   i19 = i5 + 8 | 0;
   i20 = i5 + 4 | 0;
   i22 = 0;
   while (1) {
    i26 = HEAPU8[i11 + i22 + 6 | 0] | 0;
    HEAP32[i2 + (i22 * 36 | 0) + 28 >> 2] = i26;
    i23 = HEAPU8[i11 + i22 + 9 | 0] | 0;
    HEAP32[i2 + (i22 * 36 | 0) + 32 >> 2] = i23;
    if ((HEAP32[i17 >> 2] | 0) <= (i26 | 0)) {
     i9 = 6;
     break;
    }
    i26 = (HEAP32[i21 >> 2] | 0) + (i26 << 3) | 0;
    d25 = +HEAPF32[i26 >> 2];
    d24 = +HEAPF32[i26 + 4 >> 2];
    if ((HEAP32[i13 >> 2] | 0) <= (i23 | 0)) {
     i9 = 8;
     break;
    }
    i23 = (HEAP32[i14 >> 2] | 0) + (i23 << 3) | 0;
    d29 = +HEAPF32[i23 >> 2];
    d31 = +HEAPF32[i23 + 4 >> 2];
    d32 = +HEAPF32[i15 >> 2];
    d30 = +HEAPF32[i16 >> 2];
    d27 = +HEAPF32[i4 >> 2] + (d25 * d32 - d24 * d30);
    d28 = +d27;
    d30 = +(d24 * d32 + d25 * d30 + +HEAPF32[i12 >> 2]);
    i23 = i2 + (i22 * 36 | 0) | 0;
    HEAPF32[i23 >> 2] = d28;
    HEAPF32[i23 + 4 >> 2] = d30;
    d30 = +HEAPF32[i18 >> 2];
    d25 = +HEAPF32[i19 >> 2];
    d24 = +HEAPF32[i5 >> 2] + (d29 * d30 - d31 * d25);
    d28 = +d24;
    d25 = +(d31 * d30 + d29 * d25 + +HEAPF32[i20 >> 2]);
    i23 = i2 + (i22 * 36 | 0) + 8 | 0;
    HEAPF32[i23 >> 2] = d28;
    HEAPF32[i23 + 4 >> 2] = d25;
    d24 = +(d24 - d27);
    d25 = +(+HEAPF32[i2 + (i22 * 36 | 0) + 12 >> 2] - +HEAPF32[i2 + (i22 * 36 | 0) + 4 >> 2]);
    i23 = i2 + (i22 * 36 | 0) + 16 | 0;
    HEAPF32[i23 >> 2] = d24;
    HEAPF32[i23 + 4 >> 2] = d25;
    HEAPF32[i2 + (i22 * 36 | 0) + 24 >> 2] = 0.0;
    i22 = i22 + 1 | 0;
    i23 = HEAP32[i6 >> 2] | 0;
    if ((i22 | 0) >= (i23 | 0)) {
     i7 = i23;
     break L4;
    }
   }
   if ((i9 | 0) == 6) {
    ___assert_fail(2776, 2808, 103, 2840);
   } else if ((i9 | 0) == 8) {
    ___assert_fail(2776, 2808, 103, 2840);
   }
  } else {
   i7 = i12;
  }
 } while (0);
 do {
  if ((i7 | 0) > 1) {
   d24 = +HEAPF32[i11 >> 2];
   if ((i7 | 0) == 2) {
    d32 = +HEAPF32[i2 + 16 >> 2] - +HEAPF32[i2 + 52 >> 2];
    d8 = +HEAPF32[i2 + 20 >> 2] - +HEAPF32[i2 + 56 >> 2];
    d8 = +Math_sqrt(+(d32 * d32 + d8 * d8));
   } else if ((i7 | 0) == 3) {
    d8 = +HEAPF32[i2 + 16 >> 2];
    d32 = +HEAPF32[i2 + 20 >> 2];
    d8 = (+HEAPF32[i2 + 52 >> 2] - d8) * (+HEAPF32[i2 + 92 >> 2] - d32) - (+HEAPF32[i2 + 56 >> 2] - d32) * (+HEAPF32[i2 + 88 >> 2] - d8);
   } else {
    ___assert_fail(2712, 2672, 259, 2736);
   }
   if (!(d8 < d24 * .5) ? !(d24 * 2.0 < d8 | d8 < 1.1920928955078125e-7) : 0) {
    i9 = 18;
    break;
   }
   HEAP32[i6 >> 2] = 0;
  } else {
   i9 = 18;
  }
 } while (0);
 if ((i9 | 0) == 18 ? (i7 | 0) != 0 : 0) {
  STACKTOP = i1;
  return;
 }
 HEAP32[i2 + 28 >> 2] = 0;
 HEAP32[i2 + 32 >> 2] = 0;
 if ((HEAP32[i10 + 20 >> 2] | 0) <= 0) {
  ___assert_fail(2776, 2808, 103, 2840);
 }
 i26 = HEAP32[i10 + 16 >> 2] | 0;
 d8 = +HEAPF32[i26 >> 2];
 d24 = +HEAPF32[i26 + 4 >> 2];
 if ((HEAP32[i3 + 20 >> 2] | 0) <= 0) {
  ___assert_fail(2776, 2808, 103, 2840);
 }
 i26 = HEAP32[i3 + 16 >> 2] | 0;
 d27 = +HEAPF32[i26 >> 2];
 d25 = +HEAPF32[i26 + 4 >> 2];
 d30 = +HEAPF32[i4 + 12 >> 2];
 d32 = +HEAPF32[i4 + 8 >> 2];
 d31 = +HEAPF32[i4 >> 2] + (d8 * d30 - d24 * d32);
 d32 = d24 * d30 + d8 * d32 + +HEAPF32[i4 + 4 >> 2];
 d30 = +d31;
 d28 = +d32;
 i26 = i2;
 HEAPF32[i26 >> 2] = d30;
 HEAPF32[i26 + 4 >> 2] = d28;
 d28 = +HEAPF32[i5 + 12 >> 2];
 d30 = +HEAPF32[i5 + 8 >> 2];
 d29 = +HEAPF32[i5 >> 2] + (d27 * d28 - d25 * d30);
 d30 = d25 * d28 + d27 * d30 + +HEAPF32[i5 + 4 >> 2];
 d27 = +d29;
 d28 = +d30;
 i26 = i2 + 8 | 0;
 HEAPF32[i26 >> 2] = d27;
 HEAPF32[i26 + 4 >> 2] = d28;
 d31 = +(d29 - d31);
 d32 = +(d30 - d32);
 i26 = i2 + 16 | 0;
 HEAPF32[i26 >> 2] = d31;
 HEAPF32[i26 + 4 >> 2] = d32;
 HEAP32[i6 >> 2] = 1;
 STACKTOP = i1;
 return;
}
function __ZNSt3__17__sort4IRPFbRK6b2PairS3_EPS1_EEjT0_S8_S8_S8_T_(i6, i7, i5, i4, i1) {
 i6 = i6 | 0;
 i7 = i7 | 0;
 i5 = i5 | 0;
 i4 = i4 | 0;
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i8 = 0, i9 = 0;
 i2 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 i3 = i2;
 i9 = FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i7, i6) | 0;
 i8 = FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i5, i7) | 0;
 do {
  if (i9) {
   if (i8) {
    HEAP32[i3 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
    HEAP32[i3 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
    HEAP32[i3 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
    HEAP32[i6 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
    HEAP32[i6 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
    HEAP32[i6 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
    HEAP32[i5 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
    HEAP32[i5 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
    HEAP32[i5 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
    i8 = 1;
    break;
   }
   HEAP32[i3 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
   HEAP32[i3 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
   HEAP32[i3 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
   HEAP32[i6 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
   HEAP32[i6 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
   HEAP32[i6 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
   HEAP32[i7 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
   HEAP32[i7 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
   HEAP32[i7 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
   if (FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i5, i7) | 0) {
    HEAP32[i3 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
    HEAP32[i3 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
    HEAP32[i3 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
    HEAP32[i7 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
    HEAP32[i7 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
    HEAP32[i7 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
    HEAP32[i5 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
    HEAP32[i5 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
    HEAP32[i5 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
    i8 = 2;
   } else {
    i8 = 1;
   }
  } else {
   if (i8) {
    HEAP32[i3 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
    HEAP32[i3 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
    HEAP32[i3 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
    HEAP32[i7 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
    HEAP32[i7 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
    HEAP32[i7 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
    HEAP32[i5 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
    HEAP32[i5 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
    HEAP32[i5 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
    if (FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i7, i6) | 0) {
     HEAP32[i3 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
     HEAP32[i3 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
     HEAP32[i3 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
     HEAP32[i6 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
     HEAP32[i6 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
     HEAP32[i6 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
     HEAP32[i7 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
     HEAP32[i7 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
     HEAP32[i7 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
     i8 = 2;
    } else {
     i8 = 1;
    }
   } else {
    i8 = 0;
   }
  }
 } while (0);
 if (!(FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i4, i5) | 0)) {
  i9 = i8;
  STACKTOP = i2;
  return i9 | 0;
 }
 HEAP32[i3 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
 HEAP32[i3 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
 HEAP32[i3 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
 HEAP32[i5 + 0 >> 2] = HEAP32[i4 + 0 >> 2];
 HEAP32[i5 + 4 >> 2] = HEAP32[i4 + 4 >> 2];
 HEAP32[i5 + 8 >> 2] = HEAP32[i4 + 8 >> 2];
 HEAP32[i4 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
 HEAP32[i4 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
 HEAP32[i4 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
 if (!(FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i5, i7) | 0)) {
  i9 = i8 + 1 | 0;
  STACKTOP = i2;
  return i9 | 0;
 }
 HEAP32[i3 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
 HEAP32[i3 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
 HEAP32[i3 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
 HEAP32[i7 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
 HEAP32[i7 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
 HEAP32[i7 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
 HEAP32[i5 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
 HEAP32[i5 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
 HEAP32[i5 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
 if (!(FUNCTION_TABLE_iii[HEAP32[i1 >> 2] & 3](i7, i6) | 0)) {
  i9 = i8 + 2 | 0;
  STACKTOP = i2;
  return i9 | 0;
 }
 HEAP32[i3 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
 HEAP32[i3 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
 HEAP32[i3 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
 HEAP32[i6 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
 HEAP32[i6 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
 HEAP32[i6 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
 HEAP32[i7 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
 HEAP32[i7 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
 HEAP32[i7 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
 i9 = i8 + 3 | 0;
 STACKTOP = i2;
 return i9 | 0;
}
function __ZN15b2ContactSolver27SolveTOIPositionConstraintsEii(i9, i2, i5) {
 i9 = i9 | 0;
 i2 = i2 | 0;
 i5 = i5 | 0;
 var i1 = 0, i3 = 0, i4 = 0, i6 = 0, i7 = 0, i8 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, d21 = 0.0, d22 = 0.0, d23 = 0.0, d24 = 0.0, d25 = 0.0, d26 = 0.0, d27 = 0.0, d28 = 0.0, d29 = 0.0, d30 = 0.0, d31 = 0.0, d32 = 0.0, d33 = 0.0, d34 = 0.0, d35 = 0.0, d36 = 0.0, i37 = 0, d38 = 0.0, d39 = 0.0, d40 = 0.0, d41 = 0.0, d42 = 0.0, d43 = 0.0, d44 = 0.0, d45 = 0.0, i46 = 0, d47 = 0.0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 64 | 0;
 i8 = i1 + 40 | 0;
 i3 = i1 + 24 | 0;
 i4 = i1;
 i6 = i9 + 48 | 0;
 if ((HEAP32[i6 >> 2] | 0) <= 0) {
  d45 = 0.0;
  i37 = d45 >= -.007499999832361937;
  STACKTOP = i1;
  return i37 | 0;
 }
 i7 = i9 + 36 | 0;
 i14 = i9 + 24 | 0;
 i9 = i8 + 8 | 0;
 i15 = i8 + 12 | 0;
 i10 = i3 + 8 | 0;
 i11 = i3 + 12 | 0;
 i12 = i4 + 8 | 0;
 i13 = i4 + 16 | 0;
 i16 = 0;
 d34 = 0.0;
 do {
  i37 = HEAP32[i7 >> 2] | 0;
  i19 = i37 + (i16 * 88 | 0) | 0;
  i17 = HEAP32[i37 + (i16 * 88 | 0) + 32 >> 2] | 0;
  i18 = HEAP32[i37 + (i16 * 88 | 0) + 36 >> 2] | 0;
  i20 = i37 + (i16 * 88 | 0) + 48 | 0;
  d21 = +HEAPF32[i20 >> 2];
  d22 = +HEAPF32[i20 + 4 >> 2];
  i20 = i37 + (i16 * 88 | 0) + 56 | 0;
  d23 = +HEAPF32[i20 >> 2];
  d24 = +HEAPF32[i20 + 4 >> 2];
  i20 = HEAP32[i37 + (i16 * 88 | 0) + 84 >> 2] | 0;
  if ((i17 | 0) == (i2 | 0) | (i17 | 0) == (i5 | 0)) {
   d26 = +HEAPF32[i37 + (i16 * 88 | 0) + 64 >> 2];
   d27 = +HEAPF32[i37 + (i16 * 88 | 0) + 40 >> 2];
  } else {
   d26 = 0.0;
   d27 = 0.0;
  }
  d25 = +HEAPF32[i37 + (i16 * 88 | 0) + 44 >> 2];
  d28 = +HEAPF32[i37 + (i16 * 88 | 0) + 68 >> 2];
  i37 = HEAP32[i14 >> 2] | 0;
  i46 = i37 + (i17 * 12 | 0) | 0;
  d33 = +HEAPF32[i46 >> 2];
  d35 = +HEAPF32[i46 + 4 >> 2];
  d29 = +HEAPF32[i37 + (i17 * 12 | 0) + 8 >> 2];
  i46 = i37 + (i18 * 12 | 0) | 0;
  d32 = +HEAPF32[i46 >> 2];
  d36 = +HEAPF32[i46 + 4 >> 2];
  d31 = +HEAPF32[i37 + (i18 * 12 | 0) + 8 >> 2];
  if ((i20 | 0) > 0) {
   d30 = d27 + d25;
   i37 = 0;
   do {
    d38 = +Math_sin(+d29);
    HEAPF32[i9 >> 2] = d38;
    d44 = +Math_cos(+d29);
    HEAPF32[i15 >> 2] = d44;
    d43 = +Math_sin(+d31);
    HEAPF32[i10 >> 2] = d43;
    d41 = +Math_cos(+d31);
    HEAPF32[i11 >> 2] = d41;
    d40 = +(d33 - (d21 * d44 - d22 * d38));
    d38 = +(d35 - (d22 * d44 + d21 * d38));
    i46 = i8;
    HEAPF32[i46 >> 2] = d40;
    HEAPF32[i46 + 4 >> 2] = d38;
    d38 = +(d32 - (d23 * d41 - d24 * d43));
    d43 = +(d36 - (d24 * d41 + d23 * d43));
    i46 = i3;
    HEAPF32[i46 >> 2] = d38;
    HEAPF32[i46 + 4 >> 2] = d43;
    __ZN24b2PositionSolverManifold10InitializeEP27b2ContactPositionConstraintRK11b2TransformS4_i(i4, i19, i8, i3, i37);
    i46 = i4;
    d43 = +HEAPF32[i46 >> 2];
    d38 = +HEAPF32[i46 + 4 >> 2];
    i46 = i12;
    d41 = +HEAPF32[i46 >> 2];
    d40 = +HEAPF32[i46 + 4 >> 2];
    d44 = +HEAPF32[i13 >> 2];
    d39 = d41 - d33;
    d42 = d40 - d35;
    d41 = d41 - d32;
    d40 = d40 - d36;
    d34 = d34 < d44 ? d34 : d44;
    d44 = (d44 + .004999999888241291) * .75;
    d44 = d44 < 0.0 ? d44 : 0.0;
    d45 = d38 * d39 - d43 * d42;
    d47 = d38 * d41 - d43 * d40;
    d45 = d47 * d28 * d47 + (d30 + d45 * d26 * d45);
    if (d45 > 0.0) {
     d44 = -(d44 < -.20000000298023224 ? -.20000000298023224 : d44) / d45;
    } else {
     d44 = 0.0;
    }
    d47 = d43 * d44;
    d45 = d38 * d44;
    d33 = d33 - d27 * d47;
    d35 = d35 - d27 * d45;
    d29 = d29 - d26 * (d39 * d45 - d42 * d47);
    d32 = d32 + d25 * d47;
    d36 = d36 + d25 * d45;
    d31 = d31 + d28 * (d41 * d45 - d40 * d47);
    i37 = i37 + 1 | 0;
   } while ((i37 | 0) != (i20 | 0));
   i37 = HEAP32[i14 >> 2] | 0;
  }
  d47 = +d33;
  d45 = +d35;
  i46 = i37 + (i17 * 12 | 0) | 0;
  HEAPF32[i46 >> 2] = d47;
  HEAPF32[i46 + 4 >> 2] = d45;
  i46 = HEAP32[i14 >> 2] | 0;
  HEAPF32[i46 + (i17 * 12 | 0) + 8 >> 2] = d29;
  d45 = +d32;
  d47 = +d36;
  i46 = i46 + (i18 * 12 | 0) | 0;
  HEAPF32[i46 >> 2] = d45;
  HEAPF32[i46 + 4 >> 2] = d47;
  HEAPF32[(HEAP32[i14 >> 2] | 0) + (i18 * 12 | 0) + 8 >> 2] = d31;
  i16 = i16 + 1 | 0;
 } while ((i16 | 0) < (HEAP32[i6 >> 2] | 0));
 i46 = d34 >= -.007499999832361937;
 STACKTOP = i1;
 return i46 | 0;
}
function __ZN15b2ContactSolver24SolvePositionConstraintsEv(i7) {
 i7 = i7 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, d17 = 0.0, d18 = 0.0, d19 = 0.0, d20 = 0.0, i21 = 0, d22 = 0.0, d23 = 0.0, d24 = 0.0, d25 = 0.0, i26 = 0, d27 = 0.0, d28 = 0.0, d29 = 0.0, d30 = 0.0, d31 = 0.0, d32 = 0.0, d33 = 0.0, d34 = 0.0, i35 = 0, d36 = 0.0, d37 = 0.0, d38 = 0.0, d39 = 0.0, d40 = 0.0, d41 = 0.0, d42 = 0.0, d43 = 0.0, i44 = 0, d45 = 0.0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 64 | 0;
 i4 = i1 + 40 | 0;
 i5 = i1 + 24 | 0;
 i3 = i1;
 i2 = i7 + 48 | 0;
 if ((HEAP32[i2 >> 2] | 0) <= 0) {
  d43 = 0.0;
  i35 = d43 >= -.014999999664723873;
  STACKTOP = i1;
  return i35 | 0;
 }
 i6 = i7 + 36 | 0;
 i9 = i7 + 24 | 0;
 i13 = i4 + 8 | 0;
 i7 = i4 + 12 | 0;
 i8 = i5 + 8 | 0;
 i12 = i5 + 12 | 0;
 i10 = i3 + 8 | 0;
 i11 = i3 + 16 | 0;
 i35 = HEAP32[i9 >> 2] | 0;
 i15 = 0;
 d32 = 0.0;
 do {
  i21 = HEAP32[i6 >> 2] | 0;
  i26 = i21 + (i15 * 88 | 0) | 0;
  i16 = HEAP32[i21 + (i15 * 88 | 0) + 32 >> 2] | 0;
  i14 = HEAP32[i21 + (i15 * 88 | 0) + 36 >> 2] | 0;
  i44 = i21 + (i15 * 88 | 0) + 48 | 0;
  d22 = +HEAPF32[i44 >> 2];
  d23 = +HEAPF32[i44 + 4 >> 2];
  d25 = +HEAPF32[i21 + (i15 * 88 | 0) + 40 >> 2];
  d18 = +HEAPF32[i21 + (i15 * 88 | 0) + 64 >> 2];
  i44 = i21 + (i15 * 88 | 0) + 56 | 0;
  d24 = +HEAPF32[i44 >> 2];
  d19 = +HEAPF32[i44 + 4 >> 2];
  d17 = +HEAPF32[i21 + (i15 * 88 | 0) + 44 >> 2];
  d20 = +HEAPF32[i21 + (i15 * 88 | 0) + 68 >> 2];
  i21 = HEAP32[i21 + (i15 * 88 | 0) + 84 >> 2] | 0;
  i44 = i35 + (i16 * 12 | 0) | 0;
  d28 = +HEAPF32[i44 >> 2];
  d33 = +HEAPF32[i44 + 4 >> 2];
  d29 = +HEAPF32[i35 + (i16 * 12 | 0) + 8 >> 2];
  i44 = i35 + (i14 * 12 | 0) | 0;
  d30 = +HEAPF32[i44 >> 2];
  d34 = +HEAPF32[i44 + 4 >> 2];
  d31 = +HEAPF32[i35 + (i14 * 12 | 0) + 8 >> 2];
  if ((i21 | 0) > 0) {
   d27 = d25 + d17;
   i35 = 0;
   do {
    d41 = +Math_sin(+d29);
    HEAPF32[i13 >> 2] = d41;
    d42 = +Math_cos(+d29);
    HEAPF32[i7 >> 2] = d42;
    d39 = +Math_sin(+d31);
    HEAPF32[i8 >> 2] = d39;
    d38 = +Math_cos(+d31);
    HEAPF32[i12 >> 2] = d38;
    d40 = +(d28 - (d22 * d42 - d23 * d41));
    d41 = +(d33 - (d23 * d42 + d22 * d41));
    i44 = i4;
    HEAPF32[i44 >> 2] = d40;
    HEAPF32[i44 + 4 >> 2] = d41;
    d41 = +(d30 - (d24 * d38 - d19 * d39));
    d39 = +(d34 - (d19 * d38 + d24 * d39));
    i44 = i5;
    HEAPF32[i44 >> 2] = d41;
    HEAPF32[i44 + 4 >> 2] = d39;
    __ZN24b2PositionSolverManifold10InitializeEP27b2ContactPositionConstraintRK11b2TransformS4_i(i3, i26, i4, i5, i35);
    i44 = i3;
    d39 = +HEAPF32[i44 >> 2];
    d41 = +HEAPF32[i44 + 4 >> 2];
    i44 = i10;
    d38 = +HEAPF32[i44 >> 2];
    d40 = +HEAPF32[i44 + 4 >> 2];
    d42 = +HEAPF32[i11 >> 2];
    d36 = d38 - d28;
    d37 = d40 - d33;
    d38 = d38 - d30;
    d40 = d40 - d34;
    d32 = d32 < d42 ? d32 : d42;
    d42 = (d42 + .004999999888241291) * .20000000298023224;
    d43 = d42 < 0.0 ? d42 : 0.0;
    d42 = d41 * d36 - d39 * d37;
    d45 = d41 * d38 - d39 * d40;
    d42 = d45 * d20 * d45 + (d27 + d42 * d18 * d42);
    if (d42 > 0.0) {
     d42 = -(d43 < -.20000000298023224 ? -.20000000298023224 : d43) / d42;
    } else {
     d42 = 0.0;
    }
    d45 = d39 * d42;
    d43 = d41 * d42;
    d28 = d28 - d25 * d45;
    d33 = d33 - d25 * d43;
    d29 = d29 - d18 * (d36 * d43 - d37 * d45);
    d30 = d30 + d17 * d45;
    d34 = d34 + d17 * d43;
    d31 = d31 + d20 * (d38 * d43 - d40 * d45);
    i35 = i35 + 1 | 0;
   } while ((i35 | 0) != (i21 | 0));
   i35 = HEAP32[i9 >> 2] | 0;
  }
  d45 = +d28;
  d43 = +d33;
  i35 = i35 + (i16 * 12 | 0) | 0;
  HEAPF32[i35 >> 2] = d45;
  HEAPF32[i35 + 4 >> 2] = d43;
  i35 = HEAP32[i9 >> 2] | 0;
  HEAPF32[i35 + (i16 * 12 | 0) + 8 >> 2] = d29;
  d43 = +d30;
  d45 = +d34;
  i35 = i35 + (i14 * 12 | 0) | 0;
  HEAPF32[i35 >> 2] = d43;
  HEAPF32[i35 + 4 >> 2] = d45;
  i35 = HEAP32[i9 >> 2] | 0;
  HEAPF32[i35 + (i14 * 12 | 0) + 8 >> 2] = d31;
  i15 = i15 + 1 | 0;
 } while ((i15 | 0) < (HEAP32[i2 >> 2] | 0));
 i44 = d32 >= -.014999999664723873;
 STACKTOP = i1;
 return i44 | 0;
}
function __Z22b2CollideEdgeAndCircleP10b2ManifoldPK11b2EdgeShapeRK11b2TransformPK13b2CircleShapeS6_(i1, i7, i6, i22, i5) {
 i1 = i1 | 0;
 i7 = i7 | 0;
 i6 = i6 | 0;
 i22 = i22 | 0;
 i5 = i5 | 0;
 var i2 = 0, i3 = 0, i4 = 0, d8 = 0.0, d9 = 0.0, d10 = 0.0, d11 = 0.0, d12 = 0.0, d13 = 0.0, i14 = 0, i15 = 0, d16 = 0.0, d17 = 0.0, d18 = 0.0, d19 = 0.0, d20 = 0.0, d21 = 0.0, d23 = 0.0, d24 = 0.0;
 i4 = STACKTOP;
 i2 = i1 + 60 | 0;
 HEAP32[i2 >> 2] = 0;
 i3 = i22 + 12 | 0;
 d9 = +HEAPF32[i5 + 12 >> 2];
 d23 = +HEAPF32[i3 >> 2];
 d17 = +HEAPF32[i5 + 8 >> 2];
 d18 = +HEAPF32[i22 + 16 >> 2];
 d21 = +HEAPF32[i5 >> 2] + (d9 * d23 - d17 * d18) - +HEAPF32[i6 >> 2];
 d18 = d23 * d17 + d9 * d18 + +HEAPF32[i5 + 4 >> 2] - +HEAPF32[i6 + 4 >> 2];
 d9 = +HEAPF32[i6 + 12 >> 2];
 d17 = +HEAPF32[i6 + 8 >> 2];
 d23 = d21 * d9 + d18 * d17;
 d17 = d9 * d18 - d21 * d17;
 i6 = i7 + 12 | 0;
 i5 = HEAP32[i6 >> 2] | 0;
 i6 = HEAP32[i6 + 4 >> 2] | 0;
 d21 = (HEAP32[tempDoublePtr >> 2] = i5, +HEAPF32[tempDoublePtr >> 2]);
 d18 = (HEAP32[tempDoublePtr >> 2] = i6, +HEAPF32[tempDoublePtr >> 2]);
 i15 = i7 + 20 | 0;
 i14 = HEAP32[i15 >> 2] | 0;
 i15 = HEAP32[i15 + 4 >> 2] | 0;
 d9 = (HEAP32[tempDoublePtr >> 2] = i14, +HEAPF32[tempDoublePtr >> 2]);
 d10 = (HEAP32[tempDoublePtr >> 2] = i15, +HEAPF32[tempDoublePtr >> 2]);
 d8 = d9 - d21;
 d16 = d10 - d18;
 d19 = d8 * (d9 - d23) + d16 * (d10 - d17);
 d13 = d23 - d21;
 d12 = d17 - d18;
 d20 = d13 * d8 + d12 * d16;
 d11 = +HEAPF32[i7 + 8 >> 2] + +HEAPF32[i22 + 8 >> 2];
 if (d20 <= 0.0) {
  if (d13 * d13 + d12 * d12 > d11 * d11) {
   STACKTOP = i4;
   return;
  }
  if ((HEAP8[i7 + 44 | 0] | 0) != 0 ? (i22 = i7 + 28 | 0, d24 = +HEAPF32[i22 >> 2], (d21 - d23) * (d21 - d24) + (d18 - d17) * (d18 - +HEAPF32[i22 + 4 >> 2]) > 0.0) : 0) {
   STACKTOP = i4;
   return;
  }
  HEAP32[i2 >> 2] = 1;
  HEAP32[i1 + 56 >> 2] = 0;
  HEAPF32[i1 + 40 >> 2] = 0.0;
  HEAPF32[i1 + 44 >> 2] = 0.0;
  i14 = i1 + 48 | 0;
  HEAP32[i14 >> 2] = i5;
  HEAP32[i14 + 4 >> 2] = i6;
  i14 = i1 + 16 | 0;
  HEAP32[i14 >> 2] = 0;
  HEAP8[i14] = 0;
  HEAP8[i14 + 1 | 0] = 0;
  HEAP8[i14 + 2 | 0] = 0;
  HEAP8[i14 + 3 | 0] = 0;
  i14 = i3;
  i15 = HEAP32[i14 + 4 >> 2] | 0;
  i22 = i1;
  HEAP32[i22 >> 2] = HEAP32[i14 >> 2];
  HEAP32[i22 + 4 >> 2] = i15;
  STACKTOP = i4;
  return;
 }
 if (d19 <= 0.0) {
  d8 = d23 - d9;
  d12 = d17 - d10;
  if (d8 * d8 + d12 * d12 > d11 * d11) {
   STACKTOP = i4;
   return;
  }
  if ((HEAP8[i7 + 45 | 0] | 0) != 0 ? (i22 = i7 + 36 | 0, d24 = +HEAPF32[i22 >> 2], d8 * (d24 - d9) + d12 * (+HEAPF32[i22 + 4 >> 2] - d10) > 0.0) : 0) {
   STACKTOP = i4;
   return;
  }
  HEAP32[i2 >> 2] = 1;
  HEAP32[i1 + 56 >> 2] = 0;
  HEAPF32[i1 + 40 >> 2] = 0.0;
  HEAPF32[i1 + 44 >> 2] = 0.0;
  i22 = i1 + 48 | 0;
  HEAP32[i22 >> 2] = i14;
  HEAP32[i22 + 4 >> 2] = i15;
  i14 = i1 + 16 | 0;
  HEAP32[i14 >> 2] = 0;
  HEAP8[i14] = 1;
  HEAP8[i14 + 1 | 0] = 0;
  HEAP8[i14 + 2 | 0] = 0;
  HEAP8[i14 + 3 | 0] = 0;
  i14 = i3;
  i15 = HEAP32[i14 + 4 >> 2] | 0;
  i22 = i1;
  HEAP32[i22 >> 2] = HEAP32[i14 >> 2];
  HEAP32[i22 + 4 >> 2] = i15;
  STACKTOP = i4;
  return;
 }
 d24 = d8 * d8 + d16 * d16;
 if (!(d24 > 0.0)) {
  ___assert_fail(5560, 5576, 127, 5616);
 }
 d24 = 1.0 / d24;
 d23 = d23 - (d21 * d19 + d9 * d20) * d24;
 d24 = d17 - (d18 * d19 + d10 * d20) * d24;
 if (d23 * d23 + d24 * d24 > d11 * d11) {
  STACKTOP = i4;
  return;
 }
 d9 = -d16;
 if (d8 * d12 + d13 * d9 < 0.0) {
  d8 = -d8;
 } else {
  d16 = d9;
 }
 d9 = +Math_sqrt(+(d8 * d8 + d16 * d16));
 if (!(d9 < 1.1920928955078125e-7)) {
  d24 = 1.0 / d9;
  d16 = d16 * d24;
  d8 = d8 * d24;
 }
 HEAP32[i2 >> 2] = 1;
 HEAP32[i1 + 56 >> 2] = 1;
 d23 = +d16;
 d24 = +d8;
 i14 = i1 + 40 | 0;
 HEAPF32[i14 >> 2] = d23;
 HEAPF32[i14 + 4 >> 2] = d24;
 i14 = i1 + 48 | 0;
 HEAP32[i14 >> 2] = i5;
 HEAP32[i14 + 4 >> 2] = i6;
 i14 = i1 + 16 | 0;
 HEAP32[i14 >> 2] = 0;
 HEAP8[i14] = 0;
 HEAP8[i14 + 1 | 0] = 0;
 HEAP8[i14 + 2 | 0] = 1;
 HEAP8[i14 + 3 | 0] = 0;
 i14 = i3;
 i15 = HEAP32[i14 + 4 >> 2] | 0;
 i22 = i1;
 HEAP32[i22 >> 2] = HEAP32[i14 >> 2];
 HEAP32[i22 + 4 >> 2] = i15;
 STACKTOP = i4;
 return;
}
function __ZN6b2BodyC2EPK9b2BodyDefP7b2World(i1, i2, i5) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i5 = i5 | 0;
 var i3 = 0, i4 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, d13 = 0.0;
 i3 = STACKTOP;
 i9 = i2 + 4 | 0;
 d13 = +HEAPF32[i9 >> 2];
 if (!(d13 == d13 & 0.0 == 0.0 & d13 > -inf & d13 < inf)) {
  ___assert_fail(1496, 1520, 27, 1552);
 }
 d13 = +HEAPF32[i2 + 8 >> 2];
 if (!(d13 == d13 & 0.0 == 0.0 & d13 > -inf & d13 < inf)) {
  ___assert_fail(1496, 1520, 27, 1552);
 }
 i6 = i2 + 16 | 0;
 d13 = +HEAPF32[i6 >> 2];
 if (!(d13 == d13 & 0.0 == 0.0 & d13 > -inf & d13 < inf)) {
  ___assert_fail(1560, 1520, 28, 1552);
 }
 d13 = +HEAPF32[i2 + 20 >> 2];
 if (!(d13 == d13 & 0.0 == 0.0 & d13 > -inf & d13 < inf)) {
  ___assert_fail(1560, 1520, 28, 1552);
 }
 i7 = i2 + 12 | 0;
 d13 = +HEAPF32[i7 >> 2];
 if (!(d13 == d13 & 0.0 == 0.0 & d13 > -inf & d13 < inf)) {
  ___assert_fail(1592, 1520, 29, 1552);
 }
 i8 = i2 + 24 | 0;
 d13 = +HEAPF32[i8 >> 2];
 if (!(d13 == d13 & 0.0 == 0.0 & d13 > -inf & d13 < inf)) {
  ___assert_fail(1616, 1520, 30, 1552);
 }
 i4 = i2 + 32 | 0;
 d13 = +HEAPF32[i4 >> 2];
 if (!(d13 >= 0.0) | d13 == d13 & 0.0 == 0.0 & d13 > -inf & d13 < inf ^ 1) {
  ___assert_fail(1648, 1520, 31, 1552);
 }
 i10 = i2 + 28 | 0;
 d13 = +HEAPF32[i10 >> 2];
 if (!(d13 >= 0.0) | d13 == d13 & 0.0 == 0.0 & d13 > -inf & d13 < inf ^ 1) {
  ___assert_fail(1712, 1520, 32, 1552);
 }
 i11 = i1 + 4 | 0;
 i12 = (HEAP8[i2 + 39 | 0] | 0) == 0 ? 0 : 8;
 HEAP16[i11 >> 1] = i12;
 if ((HEAP8[i2 + 38 | 0] | 0) != 0) {
  i12 = (i12 & 65535 | 16) & 65535;
  HEAP16[i11 >> 1] = i12;
 }
 if ((HEAP8[i2 + 36 | 0] | 0) != 0) {
  i12 = (i12 & 65535 | 4) & 65535;
  HEAP16[i11 >> 1] = i12;
 }
 if ((HEAP8[i2 + 37 | 0] | 0) != 0) {
  i12 = (i12 & 65535 | 2) & 65535;
  HEAP16[i11 >> 1] = i12;
 }
 if ((HEAP8[i2 + 40 | 0] | 0) != 0) {
  HEAP16[i11 >> 1] = i12 & 65535 | 32;
 }
 HEAP32[i1 + 88 >> 2] = i5;
 i11 = i9;
 i12 = HEAP32[i11 >> 2] | 0;
 i11 = HEAP32[i11 + 4 >> 2] | 0;
 i9 = i1 + 12 | 0;
 HEAP32[i9 >> 2] = i12;
 HEAP32[i9 + 4 >> 2] = i11;
 d13 = +HEAPF32[i7 >> 2];
 HEAPF32[i1 + 20 >> 2] = +Math_sin(+d13);
 HEAPF32[i1 + 24 >> 2] = +Math_cos(+d13);
 HEAPF32[i1 + 28 >> 2] = 0.0;
 HEAPF32[i1 + 32 >> 2] = 0.0;
 i9 = i1 + 36 | 0;
 HEAP32[i9 >> 2] = i12;
 HEAP32[i9 + 4 >> 2] = i11;
 i9 = i1 + 44 | 0;
 HEAP32[i9 >> 2] = i12;
 HEAP32[i9 + 4 >> 2] = i11;
 HEAPF32[i1 + 52 >> 2] = +HEAPF32[i7 >> 2];
 HEAPF32[i1 + 56 >> 2] = +HEAPF32[i7 >> 2];
 HEAPF32[i1 + 60 >> 2] = 0.0;
 HEAP32[i1 + 108 >> 2] = 0;
 HEAP32[i1 + 112 >> 2] = 0;
 HEAP32[i1 + 92 >> 2] = 0;
 HEAP32[i1 + 96 >> 2] = 0;
 i9 = i6;
 i11 = HEAP32[i9 + 4 >> 2] | 0;
 i12 = i1 + 64 | 0;
 HEAP32[i12 >> 2] = HEAP32[i9 >> 2];
 HEAP32[i12 + 4 >> 2] = i11;
 HEAPF32[i1 + 72 >> 2] = +HEAPF32[i8 >> 2];
 HEAPF32[i1 + 132 >> 2] = +HEAPF32[i10 >> 2];
 HEAPF32[i1 + 136 >> 2] = +HEAPF32[i4 >> 2];
 HEAPF32[i1 + 140 >> 2] = +HEAPF32[i2 + 48 >> 2];
 HEAPF32[i1 + 76 >> 2] = 0.0;
 HEAPF32[i1 + 80 >> 2] = 0.0;
 HEAPF32[i1 + 84 >> 2] = 0.0;
 HEAPF32[i1 + 144 >> 2] = 0.0;
 i12 = HEAP32[i2 >> 2] | 0;
 HEAP32[i1 >> 2] = i12;
 i4 = i1 + 116 | 0;
 if ((i12 | 0) == 2) {
  HEAPF32[i4 >> 2] = 1.0;
  HEAPF32[i1 + 120 >> 2] = 1.0;
  i11 = i1 + 124 | 0;
  HEAPF32[i11 >> 2] = 0.0;
  i11 = i1 + 128 | 0;
  HEAPF32[i11 >> 2] = 0.0;
  i11 = i2 + 44 | 0;
  i11 = HEAP32[i11 >> 2] | 0;
  i12 = i1 + 148 | 0;
  HEAP32[i12 >> 2] = i11;
  i12 = i1 + 100 | 0;
  HEAP32[i12 >> 2] = 0;
  i12 = i1 + 104 | 0;
  HEAP32[i12 >> 2] = 0;
  STACKTOP = i3;
  return;
 } else {
  HEAPF32[i4 >> 2] = 0.0;
  HEAPF32[i1 + 120 >> 2] = 0.0;
  i11 = i1 + 124 | 0;
  HEAPF32[i11 >> 2] = 0.0;
  i11 = i1 + 128 | 0;
  HEAPF32[i11 >> 2] = 0.0;
  i11 = i2 + 44 | 0;
  i11 = HEAP32[i11 >> 2] | 0;
  i12 = i1 + 148 | 0;
  HEAP32[i12 >> 2] = i11;
  i12 = i1 + 100 | 0;
  HEAP32[i12 >> 2] = 0;
  i12 = i1 + 104 | 0;
  HEAP32[i12 >> 2] = 0;
  STACKTOP = i3;
  return;
 }
}
function __ZN24b2PositionSolverManifold10InitializeEP27b2ContactPositionConstraintRK11b2TransformS4_i(i2, i1, i13, i12, i15) {
 i2 = i2 | 0;
 i1 = i1 | 0;
 i13 = i13 | 0;
 i12 = i12 | 0;
 i15 = i15 | 0;
 var i3 = 0, d4 = 0.0, d5 = 0.0, d6 = 0.0, d7 = 0.0, d8 = 0.0, d9 = 0.0, d10 = 0.0, d11 = 0.0, i14 = 0, d16 = 0.0, d17 = 0.0, d18 = 0.0, i19 = 0, i20 = 0;
 i3 = STACKTOP;
 if ((HEAP32[i1 + 84 >> 2] | 0) <= 0) {
  ___assert_fail(6752, 6520, 617, 6776);
 }
 i14 = HEAP32[i1 + 72 >> 2] | 0;
 if ((i14 | 0) == 1) {
  i19 = i13 + 12 | 0;
  d5 = +HEAPF32[i19 >> 2];
  d6 = +HEAPF32[i1 + 16 >> 2];
  i14 = i13 + 8 | 0;
  d7 = +HEAPF32[i14 >> 2];
  d9 = +HEAPF32[i1 + 20 >> 2];
  d4 = d5 * d6 - d7 * d9;
  d9 = d6 * d7 + d5 * d9;
  d5 = +d4;
  d7 = +d9;
  i20 = i2;
  HEAPF32[i20 >> 2] = d5;
  HEAPF32[i20 + 4 >> 2] = d7;
  d7 = +HEAPF32[i19 >> 2];
  d5 = +HEAPF32[i1 + 24 >> 2];
  d6 = +HEAPF32[i14 >> 2];
  d8 = +HEAPF32[i1 + 28 >> 2];
  d16 = +HEAPF32[i12 + 12 >> 2];
  d18 = +HEAPF32[i1 + (i15 << 3) >> 2];
  d17 = +HEAPF32[i12 + 8 >> 2];
  d11 = +HEAPF32[i1 + (i15 << 3) + 4 >> 2];
  d10 = +HEAPF32[i12 >> 2] + (d16 * d18 - d17 * d11);
  d11 = d18 * d17 + d16 * d11 + +HEAPF32[i12 + 4 >> 2];
  HEAPF32[i2 + 16 >> 2] = d4 * (d10 - (+HEAPF32[i13 >> 2] + (d7 * d5 - d6 * d8))) + (d11 - (d5 * d6 + d7 * d8 + +HEAPF32[i13 + 4 >> 2])) * d9 - +HEAPF32[i1 + 76 >> 2] - +HEAPF32[i1 + 80 >> 2];
  d10 = +d10;
  d11 = +d11;
  i15 = i2 + 8 | 0;
  HEAPF32[i15 >> 2] = d10;
  HEAPF32[i15 + 4 >> 2] = d11;
  STACKTOP = i3;
  return;
 } else if ((i14 | 0) == 2) {
  i19 = i12 + 12 | 0;
  d7 = +HEAPF32[i19 >> 2];
  d8 = +HEAPF32[i1 + 16 >> 2];
  i20 = i12 + 8 | 0;
  d9 = +HEAPF32[i20 >> 2];
  d18 = +HEAPF32[i1 + 20 >> 2];
  d17 = d7 * d8 - d9 * d18;
  d18 = d8 * d9 + d7 * d18;
  d7 = +d17;
  d9 = +d18;
  i14 = i2;
  HEAPF32[i14 >> 2] = d7;
  HEAPF32[i14 + 4 >> 2] = d9;
  d9 = +HEAPF32[i19 >> 2];
  d7 = +HEAPF32[i1 + 24 >> 2];
  d8 = +HEAPF32[i20 >> 2];
  d10 = +HEAPF32[i1 + 28 >> 2];
  d6 = +HEAPF32[i13 + 12 >> 2];
  d4 = +HEAPF32[i1 + (i15 << 3) >> 2];
  d5 = +HEAPF32[i13 + 8 >> 2];
  d16 = +HEAPF32[i1 + (i15 << 3) + 4 >> 2];
  d11 = +HEAPF32[i13 >> 2] + (d6 * d4 - d5 * d16);
  d16 = d4 * d5 + d6 * d16 + +HEAPF32[i13 + 4 >> 2];
  HEAPF32[i2 + 16 >> 2] = d17 * (d11 - (+HEAPF32[i12 >> 2] + (d9 * d7 - d8 * d10))) + (d16 - (d7 * d8 + d9 * d10 + +HEAPF32[i12 + 4 >> 2])) * d18 - +HEAPF32[i1 + 76 >> 2] - +HEAPF32[i1 + 80 >> 2];
  d11 = +d11;
  d16 = +d16;
  i20 = i2 + 8 | 0;
  HEAPF32[i20 >> 2] = d11;
  HEAPF32[i20 + 4 >> 2] = d16;
  d17 = +-d17;
  d18 = +-d18;
  i20 = i2;
  HEAPF32[i20 >> 2] = d17;
  HEAPF32[i20 + 4 >> 2] = d18;
  STACKTOP = i3;
  return;
 } else if ((i14 | 0) == 0) {
  d7 = +HEAPF32[i13 + 12 >> 2];
  d8 = +HEAPF32[i1 + 24 >> 2];
  d18 = +HEAPF32[i13 + 8 >> 2];
  d6 = +HEAPF32[i1 + 28 >> 2];
  d4 = +HEAPF32[i13 >> 2] + (d7 * d8 - d18 * d6);
  d6 = d8 * d18 + d7 * d6 + +HEAPF32[i13 + 4 >> 2];
  d7 = +HEAPF32[i12 + 12 >> 2];
  d18 = +HEAPF32[i1 >> 2];
  d8 = +HEAPF32[i12 + 8 >> 2];
  d9 = +HEAPF32[i1 + 4 >> 2];
  d5 = +HEAPF32[i12 >> 2] + (d7 * d18 - d8 * d9);
  d9 = d18 * d8 + d7 * d9 + +HEAPF32[i12 + 4 >> 2];
  d7 = d5 - d4;
  d8 = d9 - d6;
  d18 = +d7;
  d10 = +d8;
  i20 = i2;
  HEAPF32[i20 >> 2] = d18;
  HEAPF32[i20 + 4 >> 2] = d10;
  d10 = +Math_sqrt(+(d7 * d7 + d8 * d8));
  if (d10 < 1.1920928955078125e-7) {
   d10 = d7;
   d11 = d8;
  } else {
   d11 = 1.0 / d10;
   d10 = d7 * d11;
   HEAPF32[i2 >> 2] = d10;
   d11 = d8 * d11;
   HEAPF32[i2 + 4 >> 2] = d11;
  }
  d17 = +((d4 + d5) * .5);
  d18 = +((d6 + d9) * .5);
  i20 = i2 + 8 | 0;
  HEAPF32[i20 >> 2] = d17;
  HEAPF32[i20 + 4 >> 2] = d18;
  HEAPF32[i2 + 16 >> 2] = d7 * d10 + d8 * d11 - +HEAPF32[i1 + 76 >> 2] - +HEAPF32[i1 + 80 >> 2];
  STACKTOP = i3;
  return;
 } else {
  STACKTOP = i3;
  return;
 }
}
function __ZNSt3__118__insertion_sort_3IRPFbRK6b2PairS3_EPS1_EEvT0_S8_T_(i5, i1, i2) {
 i5 = i5 | 0;
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0;
 i4 = STACKTOP;
 STACKTOP = STACKTOP + 32 | 0;
 i6 = i4 + 12 | 0;
 i3 = i4;
 i7 = i5 + 24 | 0;
 i8 = i5 + 12 | 0;
 i10 = FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i8, i5) | 0;
 i9 = FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i7, i8) | 0;
 do {
  if (i10) {
   if (i9) {
    HEAP32[i6 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
    HEAP32[i6 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
    HEAP32[i6 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
    HEAP32[i5 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
    HEAP32[i5 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
    HEAP32[i5 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
    HEAP32[i7 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
    HEAP32[i7 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
    HEAP32[i7 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
    break;
   }
   HEAP32[i6 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
   HEAP32[i6 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
   HEAP32[i6 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
   HEAP32[i5 + 0 >> 2] = HEAP32[i8 + 0 >> 2];
   HEAP32[i5 + 4 >> 2] = HEAP32[i8 + 4 >> 2];
   HEAP32[i5 + 8 >> 2] = HEAP32[i8 + 8 >> 2];
   HEAP32[i8 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
   HEAP32[i8 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
   HEAP32[i8 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
   if (FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i7, i8) | 0) {
    HEAP32[i6 + 0 >> 2] = HEAP32[i8 + 0 >> 2];
    HEAP32[i6 + 4 >> 2] = HEAP32[i8 + 4 >> 2];
    HEAP32[i6 + 8 >> 2] = HEAP32[i8 + 8 >> 2];
    HEAP32[i8 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
    HEAP32[i8 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
    HEAP32[i8 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
    HEAP32[i7 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
    HEAP32[i7 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
    HEAP32[i7 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
   }
  } else {
   if (i9) {
    HEAP32[i6 + 0 >> 2] = HEAP32[i8 + 0 >> 2];
    HEAP32[i6 + 4 >> 2] = HEAP32[i8 + 4 >> 2];
    HEAP32[i6 + 8 >> 2] = HEAP32[i8 + 8 >> 2];
    HEAP32[i8 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
    HEAP32[i8 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
    HEAP32[i8 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
    HEAP32[i7 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
    HEAP32[i7 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
    HEAP32[i7 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
    if (FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i8, i5) | 0) {
     HEAP32[i6 + 0 >> 2] = HEAP32[i5 + 0 >> 2];
     HEAP32[i6 + 4 >> 2] = HEAP32[i5 + 4 >> 2];
     HEAP32[i6 + 8 >> 2] = HEAP32[i5 + 8 >> 2];
     HEAP32[i5 + 0 >> 2] = HEAP32[i8 + 0 >> 2];
     HEAP32[i5 + 4 >> 2] = HEAP32[i8 + 4 >> 2];
     HEAP32[i5 + 8 >> 2] = HEAP32[i8 + 8 >> 2];
     HEAP32[i8 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
     HEAP32[i8 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
     HEAP32[i8 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
    }
   }
  }
 } while (0);
 i6 = i5 + 36 | 0;
 if ((i6 | 0) == (i1 | 0)) {
  STACKTOP = i4;
  return;
 }
 while (1) {
  if (FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i6, i7) | 0) {
   HEAP32[i3 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
   HEAP32[i3 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
   HEAP32[i3 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
   i8 = i6;
   while (1) {
    HEAP32[i8 + 0 >> 2] = HEAP32[i7 + 0 >> 2];
    HEAP32[i8 + 4 >> 2] = HEAP32[i7 + 4 >> 2];
    HEAP32[i8 + 8 >> 2] = HEAP32[i7 + 8 >> 2];
    if ((i7 | 0) == (i5 | 0)) {
     break;
    }
    i8 = i7 + -12 | 0;
    if (FUNCTION_TABLE_iii[HEAP32[i2 >> 2] & 3](i3, i8) | 0) {
     i10 = i7;
     i7 = i8;
     i8 = i10;
    } else {
     break;
    }
   }
   HEAP32[i7 + 0 >> 2] = HEAP32[i3 + 0 >> 2];
   HEAP32[i7 + 4 >> 2] = HEAP32[i3 + 4 >> 2];
   HEAP32[i7 + 8 >> 2] = HEAP32[i3 + 8 >> 2];
  }
  i7 = i6 + 12 | 0;
  if ((i7 | 0) == (i1 | 0)) {
   break;
  } else {
   i10 = i6;
   i6 = i7;
   i7 = i10;
  }
 }
 STACKTOP = i4;
 return;
}
function __ZNK20b2SeparationFunction8EvaluateEiif(i10, i12, i11, d9) {
 i10 = i10 | 0;
 i12 = i12 | 0;
 i11 = i11 | 0;
 d9 = +d9;
 var d1 = 0.0, d2 = 0.0, d3 = 0.0, d4 = 0.0, d5 = 0.0, d6 = 0.0, i7 = 0, d8 = 0.0, d13 = 0.0, d14 = 0.0, d15 = 0.0, d16 = 0.0, i17 = 0, d18 = 0.0, d19 = 0.0;
 i7 = STACKTOP;
 d14 = 1.0 - d9;
 d3 = d14 * +HEAPF32[i10 + 32 >> 2] + +HEAPF32[i10 + 36 >> 2] * d9;
 d4 = +Math_sin(+d3);
 d3 = +Math_cos(+d3);
 d5 = +HEAPF32[i10 + 8 >> 2];
 d6 = +HEAPF32[i10 + 12 >> 2];
 d2 = d14 * +HEAPF32[i10 + 16 >> 2] + +HEAPF32[i10 + 24 >> 2] * d9 - (d3 * d5 - d4 * d6);
 d6 = d14 * +HEAPF32[i10 + 20 >> 2] + +HEAPF32[i10 + 28 >> 2] * d9 - (d4 * d5 + d3 * d6);
 d5 = d14 * +HEAPF32[i10 + 68 >> 2] + +HEAPF32[i10 + 72 >> 2] * d9;
 d1 = +Math_sin(+d5);
 d5 = +Math_cos(+d5);
 d15 = +HEAPF32[i10 + 44 >> 2];
 d16 = +HEAPF32[i10 + 48 >> 2];
 d8 = d14 * +HEAPF32[i10 + 52 >> 2] + +HEAPF32[i10 + 60 >> 2] * d9 - (d5 * d15 - d1 * d16);
 d9 = d14 * +HEAPF32[i10 + 56 >> 2] + +HEAPF32[i10 + 64 >> 2] * d9 - (d1 * d15 + d5 * d16);
 i17 = HEAP32[i10 + 80 >> 2] | 0;
 if ((i17 | 0) == 0) {
  d14 = +HEAPF32[i10 + 92 >> 2];
  d13 = +HEAPF32[i10 + 96 >> 2];
  i17 = HEAP32[i10 >> 2] | 0;
  if (!((i12 | 0) > -1)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  if ((HEAP32[i17 + 20 >> 2] | 0) <= (i12 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i17 = (HEAP32[i17 + 16 >> 2] | 0) + (i12 << 3) | 0;
  d15 = +HEAPF32[i17 >> 2];
  d16 = +HEAPF32[i17 + 4 >> 2];
  i10 = HEAP32[i10 + 4 >> 2] | 0;
  if (!((i11 | 0) > -1)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  if ((HEAP32[i10 + 20 >> 2] | 0) <= (i11 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i17 = (HEAP32[i10 + 16 >> 2] | 0) + (i11 << 3) | 0;
  d19 = +HEAPF32[i17 >> 2];
  d18 = +HEAPF32[i17 + 4 >> 2];
  d16 = d14 * (d8 + (d5 * d19 - d1 * d18) - (d2 + (d3 * d15 - d4 * d16))) + d13 * (d9 + (d1 * d19 + d5 * d18) - (d6 + (d4 * d15 + d3 * d16)));
  STACKTOP = i7;
  return +d16;
 } else if ((i17 | 0) == 1) {
  d14 = +HEAPF32[i10 + 92 >> 2];
  d13 = +HEAPF32[i10 + 96 >> 2];
  d16 = +HEAPF32[i10 + 84 >> 2];
  d15 = +HEAPF32[i10 + 88 >> 2];
  i10 = HEAP32[i10 + 4 >> 2] | 0;
  if (!((i11 | 0) > -1)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  if ((HEAP32[i10 + 20 >> 2] | 0) <= (i11 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i17 = (HEAP32[i10 + 16 >> 2] | 0) + (i11 << 3) | 0;
  d18 = +HEAPF32[i17 >> 2];
  d19 = +HEAPF32[i17 + 4 >> 2];
  d19 = (d3 * d14 - d4 * d13) * (d8 + (d5 * d18 - d1 * d19) - (d2 + (d3 * d16 - d4 * d15))) + (d4 * d14 + d3 * d13) * (d9 + (d1 * d18 + d5 * d19) - (d6 + (d4 * d16 + d3 * d15)));
  STACKTOP = i7;
  return +d19;
 } else if ((i17 | 0) == 2) {
  d16 = +HEAPF32[i10 + 92 >> 2];
  d15 = +HEAPF32[i10 + 96 >> 2];
  d14 = +HEAPF32[i10 + 84 >> 2];
  d13 = +HEAPF32[i10 + 88 >> 2];
  i10 = HEAP32[i10 >> 2] | 0;
  if (!((i12 | 0) > -1)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  if ((HEAP32[i10 + 20 >> 2] | 0) <= (i12 | 0)) {
   ___assert_fail(3640, 3672, 103, 3704);
  }
  i17 = (HEAP32[i10 + 16 >> 2] | 0) + (i12 << 3) | 0;
  d18 = +HEAPF32[i17 >> 2];
  d19 = +HEAPF32[i17 + 4 >> 2];
  d19 = (d5 * d16 - d1 * d15) * (d2 + (d3 * d18 - d4 * d19) - (d8 + (d5 * d14 - d1 * d13))) + (d1 * d16 + d5 * d15) * (d6 + (d4 * d18 + d3 * d19) - (d9 + (d1 * d14 + d5 * d13)));
  STACKTOP = i7;
  return +d19;
 } else {
  ___assert_fail(3616, 3560, 242, 3624);
 }
 return 0.0;
}
function __ZN6b2Body13ResetMassDataEv(i2) {
 i2 = i2 | 0;
 var d1 = 0.0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, d14 = 0.0, d15 = 0.0, d16 = 0.0, i17 = 0, d18 = 0.0, d19 = 0.0, i20 = 0, d21 = 0.0;
 i3 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 i10 = i3;
 i8 = i2 + 116 | 0;
 i9 = i2 + 120 | 0;
 i4 = i2 + 124 | 0;
 i5 = i2 + 128 | 0;
 i6 = i2 + 28 | 0;
 HEAPF32[i6 >> 2] = 0.0;
 HEAPF32[i2 + 32 >> 2] = 0.0;
 HEAP32[i8 + 0 >> 2] = 0;
 HEAP32[i8 + 4 >> 2] = 0;
 HEAP32[i8 + 8 >> 2] = 0;
 HEAP32[i8 + 12 >> 2] = 0;
 i11 = HEAP32[i2 >> 2] | 0;
 if ((i11 | 0) == 2) {
  i17 = 3784;
  d16 = +HEAPF32[i17 >> 2];
  d18 = +HEAPF32[i17 + 4 >> 2];
  i17 = HEAP32[i2 + 100 >> 2] | 0;
  if ((i17 | 0) != 0) {
   i11 = i10 + 4 | 0;
   i12 = i10 + 8 | 0;
   i13 = i10 + 12 | 0;
   d14 = 0.0;
   d15 = 0.0;
   do {
    d19 = +HEAPF32[i17 >> 2];
    if (!(d19 == 0.0)) {
     i20 = HEAP32[i17 + 12 >> 2] | 0;
     FUNCTION_TABLE_viid[HEAP32[(HEAP32[i20 >> 2] | 0) + 28 >> 2] & 3](i20, i10, d19);
     d14 = +HEAPF32[i10 >> 2];
     d15 = d14 + +HEAPF32[i8 >> 2];
     HEAPF32[i8 >> 2] = d15;
     d16 = d16 + d14 * +HEAPF32[i11 >> 2];
     d18 = d18 + d14 * +HEAPF32[i12 >> 2];
     d14 = +HEAPF32[i13 >> 2] + +HEAPF32[i4 >> 2];
     HEAPF32[i4 >> 2] = d14;
    }
    i17 = HEAP32[i17 + 4 >> 2] | 0;
   } while ((i17 | 0) != 0);
   if (d15 > 0.0) {
    d19 = 1.0 / d15;
    HEAPF32[i9 >> 2] = d19;
    d16 = d16 * d19;
    d18 = d18 * d19;
   } else {
    i7 = 11;
   }
  } else {
   d14 = 0.0;
   i7 = 11;
  }
  if ((i7 | 0) == 11) {
   HEAPF32[i8 >> 2] = 1.0;
   HEAPF32[i9 >> 2] = 1.0;
   d15 = 1.0;
  }
  do {
   if (d14 > 0.0 ? (HEAP16[i2 + 4 >> 1] & 16) == 0 : 0) {
    d14 = d14 - (d18 * d18 + d16 * d16) * d15;
    HEAPF32[i4 >> 2] = d14;
    if (d14 > 0.0) {
     d1 = 1.0 / d14;
     break;
    } else {
     ___assert_fail(1872, 1520, 319, 1856);
    }
   } else {
    i7 = 17;
   }
  } while (0);
  if ((i7 | 0) == 17) {
   HEAPF32[i4 >> 2] = 0.0;
   d1 = 0.0;
  }
  HEAPF32[i5 >> 2] = d1;
  i20 = i2 + 44 | 0;
  i17 = i20;
  d19 = +HEAPF32[i17 >> 2];
  d14 = +HEAPF32[i17 + 4 >> 2];
  d21 = +d16;
  d1 = +d18;
  i17 = i6;
  HEAPF32[i17 >> 2] = d21;
  HEAPF32[i17 + 4 >> 2] = d1;
  d1 = +HEAPF32[i2 + 24 >> 2];
  d21 = +HEAPF32[i2 + 20 >> 2];
  d15 = +HEAPF32[i2 + 12 >> 2] + (d1 * d16 - d21 * d18);
  d16 = d16 * d21 + d1 * d18 + +HEAPF32[i2 + 16 >> 2];
  d1 = +d15;
  d18 = +d16;
  HEAPF32[i20 >> 2] = d1;
  HEAPF32[i20 + 4 >> 2] = d18;
  i20 = i2 + 36 | 0;
  HEAPF32[i20 >> 2] = d1;
  HEAPF32[i20 + 4 >> 2] = d18;
  d18 = +HEAPF32[i2 + 72 >> 2];
  i20 = i2 + 64 | 0;
  HEAPF32[i20 >> 2] = +HEAPF32[i20 >> 2] - d18 * (d16 - d14);
  i20 = i2 + 68 | 0;
  HEAPF32[i20 >> 2] = d18 * (d15 - d19) + +HEAPF32[i20 >> 2];
  STACKTOP = i3;
  return;
 } else if ((i11 | 0) == 1 | (i11 | 0) == 0) {
  i17 = i2 + 12 | 0;
  i13 = HEAP32[i17 >> 2] | 0;
  i17 = HEAP32[i17 + 4 >> 2] | 0;
  i20 = i2 + 36 | 0;
  HEAP32[i20 >> 2] = i13;
  HEAP32[i20 + 4 >> 2] = i17;
  i20 = i2 + 44 | 0;
  HEAP32[i20 >> 2] = i13;
  HEAP32[i20 + 4 >> 2] = i17;
  HEAPF32[i2 + 52 >> 2] = +HEAPF32[i2 + 56 >> 2];
  STACKTOP = i3;
  return;
 } else {
  ___assert_fail(1824, 1520, 284, 1856);
 }
}
function __ZN9b2Contact6UpdateEP17b2ContactListener(i1, i4) {
 i1 = i1 | 0;
 i4 = i4 | 0;
 var i2 = 0, i3 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0;
 i3 = STACKTOP;
 STACKTOP = STACKTOP + 64 | 0;
 i2 = i3;
 i10 = i1 + 64 | 0;
 i6 = i2 + 0 | 0;
 i7 = i10 + 0 | 0;
 i5 = i6 + 64 | 0;
 do {
  HEAP32[i6 >> 2] = HEAP32[i7 >> 2];
  i6 = i6 + 4 | 0;
  i7 = i7 + 4 | 0;
 } while ((i6 | 0) < (i5 | 0));
 i6 = i1 + 4 | 0;
 i11 = HEAP32[i6 >> 2] | 0;
 HEAP32[i6 >> 2] = i11 | 4;
 i11 = i11 >>> 1;
 i14 = HEAP32[i1 + 48 >> 2] | 0;
 i15 = HEAP32[i1 + 52 >> 2] | 0;
 i5 = (HEAP8[i15 + 38 | 0] | HEAP8[i14 + 38 | 0]) << 24 >> 24 != 0;
 i8 = HEAP32[i14 + 8 >> 2] | 0;
 i7 = HEAP32[i15 + 8 >> 2] | 0;
 i12 = i8 + 12 | 0;
 i13 = i7 + 12 | 0;
 if (!i5) {
  FUNCTION_TABLE_viiii[HEAP32[HEAP32[i1 >> 2] >> 2] & 15](i1, i10, i12, i13);
  i12 = i1 + 124 | 0;
  i10 = (HEAP32[i12 >> 2] | 0) > 0;
  L4 : do {
   if (i10) {
    i19 = HEAP32[i2 + 60 >> 2] | 0;
    if ((i19 | 0) > 0) {
     i18 = 0;
    } else {
     i9 = 0;
     while (1) {
      HEAPF32[i1 + (i9 * 20 | 0) + 72 >> 2] = 0.0;
      HEAPF32[i1 + (i9 * 20 | 0) + 76 >> 2] = 0.0;
      i9 = i9 + 1 | 0;
      if ((i9 | 0) >= (HEAP32[i12 >> 2] | 0)) {
       break L4;
      }
     }
    }
    do {
     i16 = i1 + (i18 * 20 | 0) + 72 | 0;
     HEAPF32[i16 >> 2] = 0.0;
     i15 = i1 + (i18 * 20 | 0) + 76 | 0;
     HEAPF32[i15 >> 2] = 0.0;
     i14 = HEAP32[i1 + (i18 * 20 | 0) + 80 >> 2] | 0;
     i17 = 0;
     while (1) {
      i13 = i17 + 1 | 0;
      if ((HEAP32[i2 + (i17 * 20 | 0) + 16 >> 2] | 0) == (i14 | 0)) {
       i9 = 7;
       break;
      }
      if ((i13 | 0) < (i19 | 0)) {
       i17 = i13;
      } else {
       break;
      }
     }
     if ((i9 | 0) == 7) {
      i9 = 0;
      HEAPF32[i16 >> 2] = +HEAPF32[i2 + (i17 * 20 | 0) + 8 >> 2];
      HEAPF32[i15 >> 2] = +HEAPF32[i2 + (i17 * 20 | 0) + 12 >> 2];
     }
     i18 = i18 + 1 | 0;
    } while ((i18 | 0) < (HEAP32[i12 >> 2] | 0));
   }
  } while (0);
  i9 = i11 & 1;
  if (i10 ^ (i9 | 0) != 0) {
   i11 = i8 + 4 | 0;
   i12 = HEAPU16[i11 >> 1] | 0;
   if ((i12 & 2 | 0) == 0) {
    HEAP16[i11 >> 1] = i12 | 2;
    HEAPF32[i8 + 144 >> 2] = 0.0;
   }
   i8 = i7 + 4 | 0;
   i11 = HEAPU16[i8 >> 1] | 0;
   if ((i11 & 2 | 0) == 0) {
    HEAP16[i8 >> 1] = i11 | 2;
    HEAPF32[i7 + 144 >> 2] = 0.0;
   }
  }
 } else {
  i10 = __Z13b2TestOverlapPK7b2ShapeiS1_iRK11b2TransformS4_(HEAP32[i14 + 12 >> 2] | 0, HEAP32[i1 + 56 >> 2] | 0, HEAP32[i15 + 12 >> 2] | 0, HEAP32[i1 + 60 >> 2] | 0, i12, i13) | 0;
  HEAP32[i1 + 124 >> 2] = 0;
  i9 = i11 & 1;
 }
 i7 = HEAP32[i6 >> 2] | 0;
 HEAP32[i6 >> 2] = i10 ? i7 | 2 : i7 & -3;
 i8 = (i9 | 0) == 0;
 i6 = i10 ^ 1;
 i7 = (i4 | 0) == 0;
 if (!(i8 ^ 1 | i6 | i7)) {
  FUNCTION_TABLE_vii[HEAP32[(HEAP32[i4 >> 2] | 0) + 8 >> 2] & 15](i4, i1);
 }
 if (!(i8 | i10 | i7)) {
  FUNCTION_TABLE_vii[HEAP32[(HEAP32[i4 >> 2] | 0) + 12 >> 2] & 15](i4, i1);
 }
 if (i5 | i6 | i7) {
  STACKTOP = i3;
  return;
 }
 FUNCTION_TABLE_viii[HEAP32[(HEAP32[i4 >> 2] | 0) + 16 >> 2] & 3](i4, i1, i2);
 STACKTOP = i3;
 return;
}
function __ZN13b2DynamicTree10RemoveLeafEi(i1, i12) {
 i1 = i1 | 0;
 i12 = i12 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, d8 = 0.0, d9 = 0.0, d10 = 0.0, d11 = 0.0, i13 = 0;
 i2 = STACKTOP;
 if ((HEAP32[i1 >> 2] | 0) == (i12 | 0)) {
  HEAP32[i1 >> 2] = -1;
  STACKTOP = i2;
  return;
 }
 i3 = i1 + 4 | 0;
 i5 = HEAP32[i3 >> 2] | 0;
 i6 = HEAP32[i5 + (i12 * 36 | 0) + 20 >> 2] | 0;
 i4 = i5 + (i6 * 36 | 0) + 20 | 0;
 i7 = HEAP32[i4 >> 2] | 0;
 i13 = HEAP32[i5 + (i6 * 36 | 0) + 24 >> 2] | 0;
 if ((i13 | 0) == (i12 | 0)) {
  i13 = HEAP32[i5 + (i6 * 36 | 0) + 28 >> 2] | 0;
 }
 if ((i7 | 0) == -1) {
  HEAP32[i1 >> 2] = i13;
  HEAP32[i5 + (i13 * 36 | 0) + 20 >> 2] = -1;
  if (!((i6 | 0) > -1)) {
   ___assert_fail(3e3, 2944, 97, 3040);
  }
  if ((HEAP32[i1 + 12 >> 2] | 0) <= (i6 | 0)) {
   ___assert_fail(3e3, 2944, 97, 3040);
  }
  i3 = i1 + 8 | 0;
  if ((HEAP32[i3 >> 2] | 0) <= 0) {
   ___assert_fail(3056, 2944, 98, 3040);
  }
  i13 = i1 + 16 | 0;
  HEAP32[i4 >> 2] = HEAP32[i13 >> 2];
  HEAP32[i5 + (i6 * 36 | 0) + 32 >> 2] = -1;
  HEAP32[i13 >> 2] = i6;
  HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) + -1;
  STACKTOP = i2;
  return;
 }
 i12 = i5 + (i7 * 36 | 0) + 24 | 0;
 if ((HEAP32[i12 >> 2] | 0) == (i6 | 0)) {
  HEAP32[i12 >> 2] = i13;
 } else {
  HEAP32[i5 + (i7 * 36 | 0) + 28 >> 2] = i13;
 }
 HEAP32[i5 + (i13 * 36 | 0) + 20 >> 2] = i7;
 if (!((i6 | 0) > -1)) {
  ___assert_fail(3e3, 2944, 97, 3040);
 }
 if ((HEAP32[i1 + 12 >> 2] | 0) <= (i6 | 0)) {
  ___assert_fail(3e3, 2944, 97, 3040);
 }
 i12 = i1 + 8 | 0;
 if ((HEAP32[i12 >> 2] | 0) <= 0) {
  ___assert_fail(3056, 2944, 98, 3040);
 }
 i13 = i1 + 16 | 0;
 HEAP32[i4 >> 2] = HEAP32[i13 >> 2];
 HEAP32[i5 + (i6 * 36 | 0) + 32 >> 2] = -1;
 HEAP32[i13 >> 2] = i6;
 HEAP32[i12 >> 2] = (HEAP32[i12 >> 2] | 0) + -1;
 do {
  i4 = __ZN13b2DynamicTree7BalanceEi(i1, i7) | 0;
  i7 = HEAP32[i3 >> 2] | 0;
  i6 = HEAP32[i7 + (i4 * 36 | 0) + 24 >> 2] | 0;
  i5 = HEAP32[i7 + (i4 * 36 | 0) + 28 >> 2] | 0;
  d10 = +HEAPF32[i7 + (i6 * 36 | 0) >> 2];
  d11 = +HEAPF32[i7 + (i5 * 36 | 0) >> 2];
  d9 = +HEAPF32[i7 + (i6 * 36 | 0) + 4 >> 2];
  d8 = +HEAPF32[i7 + (i5 * 36 | 0) + 4 >> 2];
  d10 = +(d10 < d11 ? d10 : d11);
  d11 = +(d9 < d8 ? d9 : d8);
  i13 = i7 + (i4 * 36 | 0) | 0;
  HEAPF32[i13 >> 2] = d10;
  HEAPF32[i13 + 4 >> 2] = d11;
  d11 = +HEAPF32[i7 + (i6 * 36 | 0) + 8 >> 2];
  d10 = +HEAPF32[i7 + (i5 * 36 | 0) + 8 >> 2];
  d9 = +HEAPF32[i7 + (i6 * 36 | 0) + 12 >> 2];
  d8 = +HEAPF32[i7 + (i5 * 36 | 0) + 12 >> 2];
  d10 = +(d11 > d10 ? d11 : d10);
  d11 = +(d9 > d8 ? d9 : d8);
  i7 = i7 + (i4 * 36 | 0) + 8 | 0;
  HEAPF32[i7 >> 2] = d10;
  HEAPF32[i7 + 4 >> 2] = d11;
  i7 = HEAP32[i3 >> 2] | 0;
  i6 = HEAP32[i7 + (i6 * 36 | 0) + 32 >> 2] | 0;
  i5 = HEAP32[i7 + (i5 * 36 | 0) + 32 >> 2] | 0;
  HEAP32[i7 + (i4 * 36 | 0) + 32 >> 2] = ((i6 | 0) > (i5 | 0) ? i6 : i5) + 1;
  i7 = HEAP32[i7 + (i4 * 36 | 0) + 20 >> 2] | 0;
 } while (!((i7 | 0) == -1));
 STACKTOP = i2;
 return;
}
function __ZN9b2Simplex6Solve3Ev(i7) {
 i7 = i7 | 0;
 var i1 = 0, i2 = 0, i3 = 0, d4 = 0.0, d5 = 0.0, d6 = 0.0, d8 = 0.0, d9 = 0.0, d10 = 0.0, d11 = 0.0, d12 = 0.0, d13 = 0.0, d14 = 0.0, d15 = 0.0, d16 = 0.0, d17 = 0.0, d18 = 0.0, d19 = 0.0, d20 = 0.0, d21 = 0.0, i22 = 0;
 i1 = STACKTOP;
 i2 = i7 + 16 | 0;
 d17 = +HEAPF32[i2 >> 2];
 d15 = +HEAPF32[i2 + 4 >> 2];
 i2 = i7 + 36 | 0;
 i3 = i7 + 52 | 0;
 d14 = +HEAPF32[i3 >> 2];
 d16 = +HEAPF32[i3 + 4 >> 2];
 i3 = i7 + 72 | 0;
 i22 = i7 + 88 | 0;
 d18 = +HEAPF32[i22 >> 2];
 d11 = +HEAPF32[i22 + 4 >> 2];
 d20 = d14 - d17;
 d10 = d16 - d15;
 d9 = d17 * d20 + d15 * d10;
 d8 = d14 * d20 + d16 * d10;
 d4 = d18 - d17;
 d19 = d11 - d15;
 d6 = d17 * d4 + d15 * d19;
 d5 = d18 * d4 + d11 * d19;
 d21 = d18 - d14;
 d12 = d11 - d16;
 d13 = d14 * d21 + d16 * d12;
 d12 = d18 * d21 + d11 * d12;
 d4 = d20 * d19 - d10 * d4;
 d10 = (d14 * d11 - d16 * d18) * d4;
 d11 = (d15 * d18 - d17 * d11) * d4;
 d4 = (d17 * d16 - d15 * d14) * d4;
 if (!(!(d9 >= -0.0) | !(d6 >= -0.0))) {
  HEAPF32[i7 + 24 >> 2] = 1.0;
  HEAP32[i7 + 108 >> 2] = 1;
  STACKTOP = i1;
  return;
 }
 if (!(!(d9 < -0.0) | !(d8 > 0.0) | !(d4 <= 0.0))) {
  d21 = 1.0 / (d8 - d9);
  HEAPF32[i7 + 24 >> 2] = d8 * d21;
  HEAPF32[i7 + 60 >> 2] = -(d9 * d21);
  HEAP32[i7 + 108 >> 2] = 2;
  STACKTOP = i1;
  return;
 }
 if (!(!(d6 < -0.0) | !(d5 > 0.0) | !(d11 <= 0.0))) {
  d21 = 1.0 / (d5 - d6);
  HEAPF32[i7 + 24 >> 2] = d5 * d21;
  HEAPF32[i7 + 96 >> 2] = -(d6 * d21);
  HEAP32[i7 + 108 >> 2] = 2;
  i7 = i2 + 0 | 0;
  i3 = i3 + 0 | 0;
  i2 = i7 + 36 | 0;
  do {
   HEAP32[i7 >> 2] = HEAP32[i3 >> 2];
   i7 = i7 + 4 | 0;
   i3 = i3 + 4 | 0;
  } while ((i7 | 0) < (i2 | 0));
  STACKTOP = i1;
  return;
 }
 if (!(!(d8 <= 0.0) | !(d13 >= -0.0))) {
  HEAPF32[i7 + 60 >> 2] = 1.0;
  HEAP32[i7 + 108 >> 2] = 1;
  i7 = i7 + 0 | 0;
  i3 = i2 + 0 | 0;
  i2 = i7 + 36 | 0;
  do {
   HEAP32[i7 >> 2] = HEAP32[i3 >> 2];
   i7 = i7 + 4 | 0;
   i3 = i3 + 4 | 0;
  } while ((i7 | 0) < (i2 | 0));
  STACKTOP = i1;
  return;
 }
 if (!(!(d5 <= 0.0) | !(d12 <= 0.0))) {
  HEAPF32[i7 + 96 >> 2] = 1.0;
  HEAP32[i7 + 108 >> 2] = 1;
  i7 = i7 + 0 | 0;
  i3 = i3 + 0 | 0;
  i2 = i7 + 36 | 0;
  do {
   HEAP32[i7 >> 2] = HEAP32[i3 >> 2];
   i7 = i7 + 4 | 0;
   i3 = i3 + 4 | 0;
  } while ((i7 | 0) < (i2 | 0));
  STACKTOP = i1;
  return;
 }
 if (!(d13 < -0.0) | !(d12 > 0.0) | !(d10 <= 0.0)) {
  d21 = 1.0 / (d4 + (d10 + d11));
  HEAPF32[i7 + 24 >> 2] = d10 * d21;
  HEAPF32[i7 + 60 >> 2] = d11 * d21;
  HEAPF32[i7 + 96 >> 2] = d4 * d21;
  HEAP32[i7 + 108 >> 2] = 3;
  STACKTOP = i1;
  return;
 } else {
  d21 = 1.0 / (d12 - d13);
  HEAPF32[i7 + 60 >> 2] = d12 * d21;
  HEAPF32[i7 + 96 >> 2] = -(d13 * d21);
  HEAP32[i7 + 108 >> 2] = 2;
  i7 = i7 + 0 | 0;
  i3 = i3 + 0 | 0;
  i2 = i7 + 36 | 0;
  do {
   HEAP32[i7 >> 2] = HEAP32[i3 >> 2];
   i7 = i7 + 4 | 0;
   i3 = i3 + 4 | 0;
  } while ((i7 | 0) < (i2 | 0));
  STACKTOP = i1;
  return;
 }
}
function __ZN16b2ContactManager7CollideEv(i3) {
 i3 = i3 | 0;
 var i1 = 0, i2 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0;
 i2 = STACKTOP;
 i8 = HEAP32[i3 + 60 >> 2] | 0;
 if ((i8 | 0) == 0) {
  STACKTOP = i2;
  return;
 }
 i7 = i3 + 12 | 0;
 i6 = i3 + 4 | 0;
 i5 = i3 + 72 | 0;
 i4 = i3 + 68 | 0;
 L4 : while (1) {
  i12 = HEAP32[i8 + 48 >> 2] | 0;
  i10 = HEAP32[i8 + 52 >> 2] | 0;
  i11 = HEAP32[i8 + 56 >> 2] | 0;
  i9 = HEAP32[i8 + 60 >> 2] | 0;
  i15 = HEAP32[i12 + 8 >> 2] | 0;
  i13 = HEAP32[i10 + 8 >> 2] | 0;
  i16 = i8 + 4 | 0;
  do {
   if ((HEAP32[i16 >> 2] & 8 | 0) == 0) {
    i1 = 11;
   } else {
    if (!(__ZNK6b2Body13ShouldCollideEPKS_(i13, i15) | 0)) {
     i16 = HEAP32[i8 + 12 >> 2] | 0;
     __ZN16b2ContactManager7DestroyEP9b2Contact(i3, i8);
     i8 = i16;
     break;
    }
    i14 = HEAP32[i4 >> 2] | 0;
    if ((i14 | 0) != 0 ? !(FUNCTION_TABLE_iiii[HEAP32[(HEAP32[i14 >> 2] | 0) + 8 >> 2] & 7](i14, i12, i10) | 0) : 0) {
     i16 = HEAP32[i8 + 12 >> 2] | 0;
     __ZN16b2ContactManager7DestroyEP9b2Contact(i3, i8);
     i8 = i16;
     break;
    }
    HEAP32[i16 >> 2] = HEAP32[i16 >> 2] & -9;
    i1 = 11;
   }
  } while (0);
  do {
   if ((i1 | 0) == 11) {
    i1 = 0;
    if ((HEAP16[i15 + 4 >> 1] & 2) == 0) {
     i14 = 0;
    } else {
     i14 = (HEAP32[i15 >> 2] | 0) != 0;
    }
    if ((HEAP16[i13 + 4 >> 1] & 2) == 0) {
     i13 = 0;
    } else {
     i13 = (HEAP32[i13 >> 2] | 0) != 0;
    }
    if (!(i14 | i13)) {
     i8 = HEAP32[i8 + 12 >> 2] | 0;
     break;
    }
    i11 = HEAP32[(HEAP32[i12 + 24 >> 2] | 0) + (i11 * 28 | 0) + 24 >> 2] | 0;
    i9 = HEAP32[(HEAP32[i10 + 24 >> 2] | 0) + (i9 * 28 | 0) + 24 >> 2] | 0;
    if (!((i11 | 0) > -1)) {
     i1 = 19;
     break L4;
    }
    i10 = HEAP32[i7 >> 2] | 0;
    if ((i10 | 0) <= (i11 | 0)) {
     i1 = 19;
     break L4;
    }
    i12 = HEAP32[i6 >> 2] | 0;
    if (!((i9 | 0) > -1 & (i10 | 0) > (i9 | 0))) {
     i1 = 21;
     break L4;
    }
    if (+HEAPF32[i12 + (i9 * 36 | 0) >> 2] - +HEAPF32[i12 + (i11 * 36 | 0) + 8 >> 2] > 0.0 | +HEAPF32[i12 + (i9 * 36 | 0) + 4 >> 2] - +HEAPF32[i12 + (i11 * 36 | 0) + 12 >> 2] > 0.0 | +HEAPF32[i12 + (i11 * 36 | 0) >> 2] - +HEAPF32[i12 + (i9 * 36 | 0) + 8 >> 2] > 0.0 | +HEAPF32[i12 + (i11 * 36 | 0) + 4 >> 2] - +HEAPF32[i12 + (i9 * 36 | 0) + 12 >> 2] > 0.0) {
     i16 = HEAP32[i8 + 12 >> 2] | 0;
     __ZN16b2ContactManager7DestroyEP9b2Contact(i3, i8);
     i8 = i16;
     break;
    } else {
     __ZN9b2Contact6UpdateEP17b2ContactListener(i8, HEAP32[i5 >> 2] | 0);
     i8 = HEAP32[i8 + 12 >> 2] | 0;
     break;
    }
   }
  } while (0);
  if ((i8 | 0) == 0) {
   i1 = 25;
   break;
  }
 }
 if ((i1 | 0) == 19) {
  ___assert_fail(1904, 1952, 159, 2008);
 } else if ((i1 | 0) == 21) {
  ___assert_fail(1904, 1952, 159, 2008);
 } else if ((i1 | 0) == 25) {
  STACKTOP = i2;
  return;
 }
}
function __ZN16b2ContactManager7AddPairEPvS0_(i1, i5, i6) {
 i1 = i1 | 0;
 i5 = i5 | 0;
 i6 = i6 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0;
 i2 = STACKTOP;
 i4 = HEAP32[i5 + 16 >> 2] | 0;
 i3 = HEAP32[i6 + 16 >> 2] | 0;
 i5 = HEAP32[i5 + 20 >> 2] | 0;
 i6 = HEAP32[i6 + 20 >> 2] | 0;
 i8 = HEAP32[i4 + 8 >> 2] | 0;
 i7 = HEAP32[i3 + 8 >> 2] | 0;
 if ((i8 | 0) == (i7 | 0)) {
  STACKTOP = i2;
  return;
 }
 i10 = HEAP32[i7 + 112 >> 2] | 0;
 L4 : do {
  if ((i10 | 0) != 0) {
   while (1) {
    if ((HEAP32[i10 >> 2] | 0) == (i8 | 0)) {
     i9 = HEAP32[i10 + 4 >> 2] | 0;
     i12 = HEAP32[i9 + 48 >> 2] | 0;
     i13 = HEAP32[i9 + 52 >> 2] | 0;
     i11 = HEAP32[i9 + 56 >> 2] | 0;
     i9 = HEAP32[i9 + 60 >> 2] | 0;
     if ((i12 | 0) == (i4 | 0) & (i13 | 0) == (i3 | 0) & (i11 | 0) == (i5 | 0) & (i9 | 0) == (i6 | 0)) {
      i9 = 22;
      break;
     }
     if ((i12 | 0) == (i3 | 0) & (i13 | 0) == (i4 | 0) & (i11 | 0) == (i6 | 0) & (i9 | 0) == (i5 | 0)) {
      i9 = 22;
      break;
     }
    }
    i10 = HEAP32[i10 + 12 >> 2] | 0;
    if ((i10 | 0) == 0) {
     break L4;
    }
   }
   if ((i9 | 0) == 22) {
    STACKTOP = i2;
    return;
   }
  }
 } while (0);
 if (!(__ZNK6b2Body13ShouldCollideEPKS_(i7, i8) | 0)) {
  STACKTOP = i2;
  return;
 }
 i7 = HEAP32[i1 + 68 >> 2] | 0;
 if ((i7 | 0) != 0 ? !(FUNCTION_TABLE_iiii[HEAP32[(HEAP32[i7 >> 2] | 0) + 8 >> 2] & 7](i7, i4, i3) | 0) : 0) {
  STACKTOP = i2;
  return;
 }
 i5 = __ZN9b2Contact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator(i4, i5, i3, i6, HEAP32[i1 + 76 >> 2] | 0) | 0;
 if ((i5 | 0) == 0) {
  STACKTOP = i2;
  return;
 }
 i4 = HEAP32[(HEAP32[i5 + 48 >> 2] | 0) + 8 >> 2] | 0;
 i3 = HEAP32[(HEAP32[i5 + 52 >> 2] | 0) + 8 >> 2] | 0;
 HEAP32[i5 + 8 >> 2] = 0;
 i7 = i1 + 60 | 0;
 HEAP32[i5 + 12 >> 2] = HEAP32[i7 >> 2];
 i6 = HEAP32[i7 >> 2] | 0;
 if ((i6 | 0) != 0) {
  HEAP32[i6 + 8 >> 2] = i5;
 }
 HEAP32[i7 >> 2] = i5;
 i8 = i5 + 16 | 0;
 HEAP32[i5 + 20 >> 2] = i5;
 HEAP32[i8 >> 2] = i3;
 HEAP32[i5 + 24 >> 2] = 0;
 i6 = i4 + 112 | 0;
 HEAP32[i5 + 28 >> 2] = HEAP32[i6 >> 2];
 i7 = HEAP32[i6 >> 2] | 0;
 if ((i7 | 0) != 0) {
  HEAP32[i7 + 8 >> 2] = i8;
 }
 HEAP32[i6 >> 2] = i8;
 i6 = i5 + 32 | 0;
 HEAP32[i5 + 36 >> 2] = i5;
 HEAP32[i6 >> 2] = i4;
 HEAP32[i5 + 40 >> 2] = 0;
 i7 = i3 + 112 | 0;
 HEAP32[i5 + 44 >> 2] = HEAP32[i7 >> 2];
 i5 = HEAP32[i7 >> 2] | 0;
 if ((i5 | 0) != 0) {
  HEAP32[i5 + 8 >> 2] = i6;
 }
 HEAP32[i7 >> 2] = i6;
 i5 = i4 + 4 | 0;
 i6 = HEAPU16[i5 >> 1] | 0;
 if ((i6 & 2 | 0) == 0) {
  HEAP16[i5 >> 1] = i6 | 2;
  HEAPF32[i4 + 144 >> 2] = 0.0;
 }
 i4 = i3 + 4 | 0;
 i5 = HEAPU16[i4 >> 1] | 0;
 if ((i5 & 2 | 0) == 0) {
  HEAP16[i4 >> 1] = i5 | 2;
  HEAPF32[i3 + 144 >> 2] = 0.0;
 }
 i13 = i1 + 64 | 0;
 HEAP32[i13 >> 2] = (HEAP32[i13 >> 2] | 0) + 1;
 STACKTOP = i2;
 return;
}
function __ZN12b2BroadPhase11UpdatePairsI16b2ContactManagerEEvPT_(i5, i2) {
 i5 = i5 | 0;
 i2 = i2 | 0;
 var i1 = 0, i3 = 0, i4 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0;
 i3 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 i6 = i3;
 i1 = i5 + 52 | 0;
 HEAP32[i1 >> 2] = 0;
 i4 = i5 + 40 | 0;
 i12 = HEAP32[i4 >> 2] | 0;
 do {
  if ((i12 | 0) > 0) {
   i9 = i5 + 32 | 0;
   i11 = i5 + 56 | 0;
   i8 = i5 + 12 | 0;
   i10 = i5 + 4 | 0;
   i13 = 0;
   while (1) {
    i14 = HEAP32[(HEAP32[i9 >> 2] | 0) + (i13 << 2) >> 2] | 0;
    HEAP32[i11 >> 2] = i14;
    if (!((i14 | 0) == -1)) {
     if (!((i14 | 0) > -1)) {
      i8 = 6;
      break;
     }
     if ((HEAP32[i8 >> 2] | 0) <= (i14 | 0)) {
      i8 = 6;
      break;
     }
     __ZNK13b2DynamicTree5QueryI12b2BroadPhaseEEvPT_RK6b2AABB(i5, i5, (HEAP32[i10 >> 2] | 0) + (i14 * 36 | 0) | 0);
     i12 = HEAP32[i4 >> 2] | 0;
    }
    i13 = i13 + 1 | 0;
    if ((i13 | 0) >= (i12 | 0)) {
     i8 = 9;
     break;
    }
   }
   if ((i8 | 0) == 6) {
    ___assert_fail(1904, 1952, 159, 2008);
   } else if ((i8 | 0) == 9) {
    i7 = HEAP32[i1 >> 2] | 0;
    break;
   }
  } else {
   i7 = 0;
  }
 } while (0);
 HEAP32[i4 >> 2] = 0;
 i4 = i5 + 44 | 0;
 i14 = HEAP32[i4 >> 2] | 0;
 HEAP32[i6 >> 2] = 3;
 __ZNSt3__16__sortIRPFbRK6b2PairS3_EPS1_EEvT0_S8_T_(i14, i14 + (i7 * 12 | 0) | 0, i6);
 if ((HEAP32[i1 >> 2] | 0) <= 0) {
  STACKTOP = i3;
  return;
 }
 i6 = i5 + 12 | 0;
 i7 = i5 + 4 | 0;
 i9 = 0;
 L18 : while (1) {
  i8 = HEAP32[i4 >> 2] | 0;
  i5 = i8 + (i9 * 12 | 0) | 0;
  i10 = HEAP32[i5 >> 2] | 0;
  if (!((i10 | 0) > -1)) {
   i8 = 14;
   break;
  }
  i12 = HEAP32[i6 >> 2] | 0;
  if ((i12 | 0) <= (i10 | 0)) {
   i8 = 14;
   break;
  }
  i11 = HEAP32[i7 >> 2] | 0;
  i8 = i8 + (i9 * 12 | 0) + 4 | 0;
  i13 = HEAP32[i8 >> 2] | 0;
  if (!((i13 | 0) > -1 & (i12 | 0) > (i13 | 0))) {
   i8 = 16;
   break;
  }
  __ZN16b2ContactManager7AddPairEPvS0_(i2, HEAP32[i11 + (i10 * 36 | 0) + 16 >> 2] | 0, HEAP32[i11 + (i13 * 36 | 0) + 16 >> 2] | 0);
  i10 = HEAP32[i1 >> 2] | 0;
  while (1) {
   i9 = i9 + 1 | 0;
   if ((i9 | 0) >= (i10 | 0)) {
    i8 = 21;
    break L18;
   }
   i11 = HEAP32[i4 >> 2] | 0;
   if ((HEAP32[i11 + (i9 * 12 | 0) >> 2] | 0) != (HEAP32[i5 >> 2] | 0)) {
    continue L18;
   }
   if ((HEAP32[i11 + (i9 * 12 | 0) + 4 >> 2] | 0) != (HEAP32[i8 >> 2] | 0)) {
    continue L18;
   }
  }
 }
 if ((i8 | 0) == 14) {
  ___assert_fail(1904, 1952, 153, 1992);
 } else if ((i8 | 0) == 16) {
  ___assert_fail(1904, 1952, 153, 1992);
 } else if ((i8 | 0) == 21) {
  STACKTOP = i3;
  return;
 }
}
function __ZNK13b2DynamicTree5QueryI12b2BroadPhaseEEvPT_RK6b2AABB(i9, i4, i7) {
 i9 = i9 | 0;
 i4 = i4 | 0;
 i7 = i7 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i5 = 0, i6 = 0, i8 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0;
 i2 = STACKTOP;
 STACKTOP = STACKTOP + 1040 | 0;
 i3 = i2;
 i1 = i3 + 4 | 0;
 HEAP32[i3 >> 2] = i1;
 i5 = i3 + 1028 | 0;
 HEAP32[i5 >> 2] = 0;
 i6 = i3 + 1032 | 0;
 HEAP32[i6 >> 2] = 256;
 i14 = HEAP32[i3 >> 2] | 0;
 HEAP32[i14 + (HEAP32[i5 >> 2] << 2) >> 2] = HEAP32[i9 >> 2];
 i15 = HEAP32[i5 >> 2] | 0;
 i16 = i15 + 1 | 0;
 HEAP32[i5 >> 2] = i16;
 L1 : do {
  if ((i15 | 0) > -1) {
   i9 = i9 + 4 | 0;
   i11 = i7 + 4 | 0;
   i12 = i7 + 8 | 0;
   i10 = i7 + 12 | 0;
   while (1) {
    i16 = i16 + -1 | 0;
    HEAP32[i5 >> 2] = i16;
    i13 = HEAP32[i14 + (i16 << 2) >> 2] | 0;
    do {
     if (!((i13 | 0) == -1) ? (i8 = HEAP32[i9 >> 2] | 0, !(+HEAPF32[i7 >> 2] - +HEAPF32[i8 + (i13 * 36 | 0) + 8 >> 2] > 0.0 | +HEAPF32[i11 >> 2] - +HEAPF32[i8 + (i13 * 36 | 0) + 12 >> 2] > 0.0 | +HEAPF32[i8 + (i13 * 36 | 0) >> 2] - +HEAPF32[i12 >> 2] > 0.0 | +HEAPF32[i8 + (i13 * 36 | 0) + 4 >> 2] - +HEAPF32[i10 >> 2] > 0.0)) : 0) {
      i15 = i8 + (i13 * 36 | 0) + 24 | 0;
      if ((HEAP32[i15 >> 2] | 0) == -1) {
       if (!(__ZN12b2BroadPhase13QueryCallbackEi(i4, i13) | 0)) {
        break L1;
       }
       i16 = HEAP32[i5 >> 2] | 0;
       break;
      }
      if ((i16 | 0) == (HEAP32[i6 >> 2] | 0) ? (HEAP32[i6 >> 2] = i16 << 1, i16 = __Z7b2Alloci(i16 << 3) | 0, HEAP32[i3 >> 2] = i16, _memcpy(i16 | 0, i14 | 0, HEAP32[i5 >> 2] << 2 | 0) | 0, (i14 | 0) != (i1 | 0)) : 0) {
       __Z6b2FreePv(i14);
      }
      i14 = HEAP32[i3 >> 2] | 0;
      HEAP32[i14 + (HEAP32[i5 >> 2] << 2) >> 2] = HEAP32[i15 >> 2];
      i15 = (HEAP32[i5 >> 2] | 0) + 1 | 0;
      HEAP32[i5 >> 2] = i15;
      i13 = i8 + (i13 * 36 | 0) + 28 | 0;
      if ((i15 | 0) == (HEAP32[i6 >> 2] | 0) ? (HEAP32[i6 >> 2] = i15 << 1, i16 = __Z7b2Alloci(i15 << 3) | 0, HEAP32[i3 >> 2] = i16, _memcpy(i16 | 0, i14 | 0, HEAP32[i5 >> 2] << 2 | 0) | 0, (i14 | 0) != (i1 | 0)) : 0) {
       __Z6b2FreePv(i14);
      }
      HEAP32[(HEAP32[i3 >> 2] | 0) + (HEAP32[i5 >> 2] << 2) >> 2] = HEAP32[i13 >> 2];
      i16 = (HEAP32[i5 >> 2] | 0) + 1 | 0;
      HEAP32[i5 >> 2] = i16;
     }
    } while (0);
    if ((i16 | 0) <= 0) {
     break L1;
    }
    i14 = HEAP32[i3 >> 2] | 0;
   }
  }
 } while (0);
 i4 = HEAP32[i3 >> 2] | 0;
 if ((i4 | 0) == (i1 | 0)) {
  STACKTOP = i2;
  return;
 }
 __Z6b2FreePv(i4);
 HEAP32[i3 >> 2] = 0;
 STACKTOP = i2;
 return;
}
function __ZN15b2ContactSolver9WarmStartEv(i4) {
 i4 = i4 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, d10 = 0.0, d11 = 0.0, d12 = 0.0, i13 = 0, d14 = 0.0, d15 = 0.0, d16 = 0.0, d17 = 0.0, d18 = 0.0, d19 = 0.0, d20 = 0.0, d21 = 0.0, i22 = 0, d23 = 0.0, i24 = 0, d25 = 0.0, d26 = 0.0, d27 = 0.0;
 i1 = STACKTOP;
 i2 = i4 + 48 | 0;
 if ((HEAP32[i2 >> 2] | 0) <= 0) {
  STACKTOP = i1;
  return;
 }
 i3 = i4 + 40 | 0;
 i5 = i4 + 28 | 0;
 i22 = HEAP32[i5 >> 2] | 0;
 i8 = 0;
 do {
  i9 = HEAP32[i3 >> 2] | 0;
  i7 = HEAP32[i9 + (i8 * 152 | 0) + 112 >> 2] | 0;
  i6 = HEAP32[i9 + (i8 * 152 | 0) + 116 >> 2] | 0;
  d10 = +HEAPF32[i9 + (i8 * 152 | 0) + 120 >> 2];
  d14 = +HEAPF32[i9 + (i8 * 152 | 0) + 128 >> 2];
  d12 = +HEAPF32[i9 + (i8 * 152 | 0) + 124 >> 2];
  d11 = +HEAPF32[i9 + (i8 * 152 | 0) + 132 >> 2];
  i13 = HEAP32[i9 + (i8 * 152 | 0) + 144 >> 2] | 0;
  i4 = i22 + (i7 * 12 | 0) | 0;
  i24 = i4;
  d17 = +HEAPF32[i24 >> 2];
  d19 = +HEAPF32[i24 + 4 >> 2];
  d20 = +HEAPF32[i22 + (i7 * 12 | 0) + 8 >> 2];
  i24 = i22 + (i6 * 12 | 0) | 0;
  d21 = +HEAPF32[i24 >> 2];
  d23 = +HEAPF32[i24 + 4 >> 2];
  d18 = +HEAPF32[i22 + (i6 * 12 | 0) + 8 >> 2];
  i22 = i9 + (i8 * 152 | 0) + 72 | 0;
  d15 = +HEAPF32[i22 >> 2];
  d16 = +HEAPF32[i22 + 4 >> 2];
  if ((i13 | 0) > 0) {
   i22 = 0;
   do {
    d27 = +HEAPF32[i9 + (i8 * 152 | 0) + (i22 * 36 | 0) + 16 >> 2];
    d25 = +HEAPF32[i9 + (i8 * 152 | 0) + (i22 * 36 | 0) + 20 >> 2];
    d26 = d15 * d27 + d16 * d25;
    d25 = d16 * d27 - d15 * d25;
    d20 = d20 - d14 * (+HEAPF32[i9 + (i8 * 152 | 0) + (i22 * 36 | 0) >> 2] * d25 - +HEAPF32[i9 + (i8 * 152 | 0) + (i22 * 36 | 0) + 4 >> 2] * d26);
    d17 = d17 - d10 * d26;
    d19 = d19 - d10 * d25;
    d18 = d18 + d11 * (d25 * +HEAPF32[i9 + (i8 * 152 | 0) + (i22 * 36 | 0) + 8 >> 2] - d26 * +HEAPF32[i9 + (i8 * 152 | 0) + (i22 * 36 | 0) + 12 >> 2]);
    d21 = d21 + d12 * d26;
    d23 = d23 + d12 * d25;
    i22 = i22 + 1 | 0;
   } while ((i22 | 0) != (i13 | 0));
  }
  d27 = +d17;
  d26 = +d19;
  i22 = i4;
  HEAPF32[i22 >> 2] = d27;
  HEAPF32[i22 + 4 >> 2] = d26;
  i22 = HEAP32[i5 >> 2] | 0;
  HEAPF32[i22 + (i7 * 12 | 0) + 8 >> 2] = d20;
  d26 = +d21;
  d27 = +d23;
  i22 = i22 + (i6 * 12 | 0) | 0;
  HEAPF32[i22 >> 2] = d26;
  HEAPF32[i22 + 4 >> 2] = d27;
  i22 = HEAP32[i5 >> 2] | 0;
  HEAPF32[i22 + (i6 * 12 | 0) + 8 >> 2] = d18;
  i8 = i8 + 1 | 0;
 } while ((i8 | 0) < (HEAP32[i2 >> 2] | 0));
 STACKTOP = i1;
 return;
}
function __ZNK14b2PolygonShape7RayCastEP15b2RayCastOutputRK14b2RayCastInputRK11b2Transformi(i1, i5, i8, i7, i4) {
 i1 = i1 | 0;
 i5 = i5 | 0;
 i8 = i8 | 0;
 i7 = i7 | 0;
 i4 = i4 | 0;
 var i2 = 0, d3 = 0.0, i6 = 0, d9 = 0.0, d10 = 0.0, d11 = 0.0, d12 = 0.0, d13 = 0.0, i14 = 0, i15 = 0, i16 = 0, d17 = 0.0, d18 = 0.0, d19 = 0.0, d20 = 0.0;
 i4 = STACKTOP;
 d10 = +HEAPF32[i7 >> 2];
 d9 = +HEAPF32[i8 >> 2] - d10;
 d18 = +HEAPF32[i7 + 4 >> 2];
 d11 = +HEAPF32[i8 + 4 >> 2] - d18;
 i6 = i7 + 12 | 0;
 d17 = +HEAPF32[i6 >> 2];
 i7 = i7 + 8 | 0;
 d19 = +HEAPF32[i7 >> 2];
 d12 = d9 * d17 + d11 * d19;
 d9 = d17 * d11 - d9 * d19;
 d10 = +HEAPF32[i8 + 8 >> 2] - d10;
 d18 = +HEAPF32[i8 + 12 >> 2] - d18;
 d11 = d17 * d10 + d19 * d18 - d12;
 d10 = d17 * d18 - d19 * d10 - d9;
 i8 = i8 + 16 | 0;
 i14 = HEAP32[i1 + 148 >> 2] | 0;
 do {
  if ((i14 | 0) > 0) {
   i16 = 0;
   i15 = -1;
   d13 = 0.0;
   d17 = +HEAPF32[i8 >> 2];
   L3 : while (1) {
    d20 = +HEAPF32[i1 + (i16 << 3) + 84 >> 2];
    d19 = +HEAPF32[i1 + (i16 << 3) + 88 >> 2];
    d18 = (+HEAPF32[i1 + (i16 << 3) + 20 >> 2] - d12) * d20 + (+HEAPF32[i1 + (i16 << 3) + 24 >> 2] - d9) * d19;
    d19 = d11 * d20 + d10 * d19;
    do {
     if (d19 == 0.0) {
      if (d18 < 0.0) {
       i1 = 0;
       i14 = 18;
       break L3;
      }
     } else {
      if (d19 < 0.0 ? d18 < d13 * d19 : 0) {
       i15 = i16;
       d13 = d18 / d19;
       break;
      }
      if (d19 > 0.0 ? d18 < d17 * d19 : 0) {
       d17 = d18 / d19;
      }
     }
    } while (0);
    i16 = i16 + 1 | 0;
    if (d17 < d13) {
     i1 = 0;
     i14 = 18;
     break;
    }
    if ((i16 | 0) >= (i14 | 0)) {
     i14 = 13;
     break;
    }
   }
   if ((i14 | 0) == 13) {
    if (d13 >= 0.0) {
     i2 = i15;
     d3 = d13;
     break;
    }
    ___assert_fail(376, 328, 249, 424);
   } else if ((i14 | 0) == 18) {
    STACKTOP = i4;
    return i1 | 0;
   }
  } else {
   i2 = -1;
   d3 = 0.0;
  }
 } while (0);
 if (!(d3 <= +HEAPF32[i8 >> 2])) {
  ___assert_fail(376, 328, 249, 424);
 }
 if (!((i2 | 0) > -1)) {
  i16 = 0;
  STACKTOP = i4;
  return i16 | 0;
 }
 HEAPF32[i5 + 8 >> 2] = d3;
 d18 = +HEAPF32[i6 >> 2];
 d13 = +HEAPF32[i1 + (i2 << 3) + 84 >> 2];
 d17 = +HEAPF32[i7 >> 2];
 d20 = +HEAPF32[i1 + (i2 << 3) + 88 >> 2];
 d19 = +(d18 * d13 - d17 * d20);
 d20 = +(d13 * d17 + d18 * d20);
 i16 = i5;
 HEAPF32[i16 >> 2] = d19;
 HEAPF32[i16 + 4 >> 2] = d20;
 i16 = 1;
 STACKTOP = i4;
 return i16 | 0;
}
function __ZN7b2World4StepEfii(i1, d9, i11, i12) {
 i1 = i1 | 0;
 d9 = +d9;
 i11 = i11 | 0;
 i12 = i12 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i10 = 0, i13 = 0;
 i4 = STACKTOP;
 STACKTOP = STACKTOP + 32 | 0;
 i3 = i4 + 27 | 0;
 i5 = i4;
 i8 = i4 + 26 | 0;
 i10 = i4 + 25 | 0;
 i7 = i4 + 24 | 0;
 __ZN7b2TimerC2Ev(i3);
 i2 = i1 + 102868 | 0;
 i13 = HEAP32[i2 >> 2] | 0;
 if ((i13 & 1 | 0) != 0) {
  __ZN16b2ContactManager15FindNewContactsEv(i1 + 102872 | 0);
  i13 = HEAP32[i2 >> 2] & -2;
  HEAP32[i2 >> 2] = i13;
 }
 HEAP32[i2 >> 2] = i13 | 2;
 HEAPF32[i5 >> 2] = d9;
 HEAP32[i5 + 12 >> 2] = i11;
 HEAP32[i5 + 16 >> 2] = i12;
 if (d9 > 0.0) {
  HEAPF32[i5 + 4 >> 2] = 1.0 / d9;
 } else {
  HEAPF32[i5 + 4 >> 2] = 0.0;
 }
 i11 = i1 + 102988 | 0;
 HEAPF32[i5 + 8 >> 2] = +HEAPF32[i11 >> 2] * d9;
 HEAP8[i5 + 20 | 0] = HEAP8[i1 + 102992 | 0] | 0;
 __ZN7b2TimerC2Ev(i8);
 __ZN16b2ContactManager7CollideEv(i1 + 102872 | 0);
 HEAPF32[i1 + 103e3 >> 2] = +__ZNK7b2Timer15GetMillisecondsEv(i8);
 if ((HEAP8[i1 + 102995 | 0] | 0) != 0 ? +HEAPF32[i5 >> 2] > 0.0 : 0) {
  __ZN7b2TimerC2Ev(i10);
  __ZN7b2World5SolveERK10b2TimeStep(i1, i5);
  HEAPF32[i1 + 103004 >> 2] = +__ZNK7b2Timer15GetMillisecondsEv(i10);
 }
 if ((HEAP8[i1 + 102993 | 0] | 0) != 0) {
  d9 = +HEAPF32[i5 >> 2];
  if (d9 > 0.0) {
   __ZN7b2TimerC2Ev(i7);
   __ZN7b2World8SolveTOIERK10b2TimeStep(i1, i5);
   HEAPF32[i1 + 103024 >> 2] = +__ZNK7b2Timer15GetMillisecondsEv(i7);
   i6 = 12;
  }
 } else {
  i6 = 12;
 }
 if ((i6 | 0) == 12) {
  d9 = +HEAPF32[i5 >> 2];
 }
 if (d9 > 0.0) {
  HEAPF32[i11 >> 2] = +HEAPF32[i5 + 4 >> 2];
 }
 i5 = HEAP32[i2 >> 2] | 0;
 if ((i5 & 4 | 0) == 0) {
  i13 = i5 & -3;
  HEAP32[i2 >> 2] = i13;
  d9 = +__ZNK7b2Timer15GetMillisecondsEv(i3);
  i13 = i1 + 102996 | 0;
  HEAPF32[i13 >> 2] = d9;
  STACKTOP = i4;
  return;
 }
 i6 = HEAP32[i1 + 102952 >> 2] | 0;
 if ((i6 | 0) == 0) {
  i13 = i5 & -3;
  HEAP32[i2 >> 2] = i13;
  d9 = +__ZNK7b2Timer15GetMillisecondsEv(i3);
  i13 = i1 + 102996 | 0;
  HEAPF32[i13 >> 2] = d9;
  STACKTOP = i4;
  return;
 }
 do {
  HEAPF32[i6 + 76 >> 2] = 0.0;
  HEAPF32[i6 + 80 >> 2] = 0.0;
  HEAPF32[i6 + 84 >> 2] = 0.0;
  i6 = HEAP32[i6 + 96 >> 2] | 0;
 } while ((i6 | 0) != 0);
 i13 = i5 & -3;
 HEAP32[i2 >> 2] = i13;
 d9 = +__ZNK7b2Timer15GetMillisecondsEv(i3);
 i13 = i1 + 102996 | 0;
 HEAPF32[i13 >> 2] = d9;
 STACKTOP = i4;
 return;
}
function __ZL19b2FindMaxSeparationPiPK14b2PolygonShapeRK11b2TransformS2_S5_(i1, i5, i6, i3, i4) {
 i1 = i1 | 0;
 i5 = i5 | 0;
 i6 = i6 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 var i2 = 0, i7 = 0, d8 = 0.0, d9 = 0.0, d10 = 0.0, d11 = 0.0, i12 = 0, i13 = 0, i14 = 0, d15 = 0.0, d16 = 0.0, d17 = 0.0, d18 = 0.0, d19 = 0.0;
 i2 = STACKTOP;
 i7 = HEAP32[i5 + 148 >> 2] | 0;
 d17 = +HEAPF32[i4 + 12 >> 2];
 d19 = +HEAPF32[i3 + 12 >> 2];
 d18 = +HEAPF32[i4 + 8 >> 2];
 d16 = +HEAPF32[i3 + 16 >> 2];
 d15 = +HEAPF32[i6 + 12 >> 2];
 d10 = +HEAPF32[i5 + 12 >> 2];
 d8 = +HEAPF32[i6 + 8 >> 2];
 d9 = +HEAPF32[i5 + 16 >> 2];
 d11 = +HEAPF32[i4 >> 2] + (d17 * d19 - d18 * d16) - (+HEAPF32[i6 >> 2] + (d15 * d10 - d8 * d9));
 d9 = d19 * d18 + d17 * d16 + +HEAPF32[i4 + 4 >> 2] - (d10 * d8 + d15 * d9 + +HEAPF32[i6 + 4 >> 2]);
 d10 = d15 * d11 + d8 * d9;
 d8 = d15 * d9 - d11 * d8;
 if ((i7 | 0) > 0) {
  i14 = 0;
  i13 = 0;
  d9 = -3.4028234663852886e+38;
  while (1) {
   d11 = d10 * +HEAPF32[i5 + (i13 << 3) + 84 >> 2] + d8 * +HEAPF32[i5 + (i13 << 3) + 88 >> 2];
   i12 = d11 > d9;
   i14 = i12 ? i13 : i14;
   i13 = i13 + 1 | 0;
   if ((i13 | 0) == (i7 | 0)) {
    break;
   } else {
    d9 = i12 ? d11 : d9;
   }
  }
 } else {
  i14 = 0;
 }
 d9 = +__ZL16b2EdgeSeparationPK14b2PolygonShapeRK11b2TransformiS1_S4_(i5, i6, i14, i3, i4);
 i12 = ((i14 | 0) > 0 ? i14 : i7) + -1 | 0;
 d8 = +__ZL16b2EdgeSeparationPK14b2PolygonShapeRK11b2TransformiS1_S4_(i5, i6, i12, i3, i4);
 i13 = i14 + 1 | 0;
 i13 = (i13 | 0) < (i7 | 0) ? i13 : 0;
 d10 = +__ZL16b2EdgeSeparationPK14b2PolygonShapeRK11b2TransformiS1_S4_(i5, i6, i13, i3, i4);
 if (d8 > d9 & d8 > d10) {
  while (1) {
   i13 = ((i12 | 0) > 0 ? i12 : i7) + -1 | 0;
   d9 = +__ZL16b2EdgeSeparationPK14b2PolygonShapeRK11b2TransformiS1_S4_(i5, i6, i13, i3, i4);
   if (d9 > d8) {
    i12 = i13;
    d8 = d9;
   } else {
    break;
   }
  }
  HEAP32[i1 >> 2] = i12;
  STACKTOP = i2;
  return +d8;
 }
 if (d10 > d9) {
  i12 = i13;
  d8 = d10;
 } else {
  d19 = d9;
  HEAP32[i1 >> 2] = i14;
  STACKTOP = i2;
  return +d19;
 }
 while (1) {
  i13 = i12 + 1 | 0;
  i13 = (i13 | 0) < (i7 | 0) ? i13 : 0;
  d9 = +__ZL16b2EdgeSeparationPK14b2PolygonShapeRK11b2TransformiS1_S4_(i5, i6, i13, i3, i4);
  if (d9 > d8) {
   i12 = i13;
   d8 = d9;
  } else {
   break;
  }
 }
 HEAP32[i1 >> 2] = i12;
 STACKTOP = i2;
 return +d8;
}
function __ZN9b2Fixture11SynchronizeEP12b2BroadPhaseRK11b2TransformS4_(i10, i8, i7, i2) {
 i10 = i10 | 0;
 i8 = i8 | 0;
 i7 = i7 | 0;
 i2 = i2 | 0;
 var i1 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i9 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, d23 = 0.0, d24 = 0.0, d25 = 0.0, d26 = 0.0, i27 = 0;
 i9 = STACKTOP;
 STACKTOP = STACKTOP + 48 | 0;
 i5 = i9 + 24 | 0;
 i6 = i9 + 8 | 0;
 i3 = i9;
 i4 = i10 + 28 | 0;
 if ((HEAP32[i4 >> 2] | 0) <= 0) {
  STACKTOP = i9;
  return;
 }
 i1 = i10 + 24 | 0;
 i18 = i10 + 12 | 0;
 i19 = i5 + 4 | 0;
 i20 = i6 + 4 | 0;
 i13 = i5 + 8 | 0;
 i14 = i6 + 8 | 0;
 i15 = i5 + 12 | 0;
 i16 = i6 + 12 | 0;
 i11 = i2 + 4 | 0;
 i22 = i7 + 4 | 0;
 i12 = i3 + 4 | 0;
 i21 = 0;
 do {
  i10 = HEAP32[i1 >> 2] | 0;
  i27 = HEAP32[i18 >> 2] | 0;
  i17 = i10 + (i21 * 28 | 0) + 20 | 0;
  FUNCTION_TABLE_viiii[HEAP32[(HEAP32[i27 >> 2] | 0) + 24 >> 2] & 15](i27, i5, i7, HEAP32[i17 >> 2] | 0);
  i27 = HEAP32[i18 >> 2] | 0;
  FUNCTION_TABLE_viiii[HEAP32[(HEAP32[i27 >> 2] | 0) + 24 >> 2] & 15](i27, i6, i2, HEAP32[i17 >> 2] | 0);
  i17 = i10 + (i21 * 28 | 0) | 0;
  d25 = +HEAPF32[i5 >> 2];
  d26 = +HEAPF32[i6 >> 2];
  d24 = +HEAPF32[i19 >> 2];
  d23 = +HEAPF32[i20 >> 2];
  d25 = +(d25 < d26 ? d25 : d26);
  d26 = +(d24 < d23 ? d24 : d23);
  i27 = i17;
  HEAPF32[i27 >> 2] = d25;
  HEAPF32[i27 + 4 >> 2] = d26;
  d25 = +HEAPF32[i13 >> 2];
  d26 = +HEAPF32[i14 >> 2];
  d23 = +HEAPF32[i15 >> 2];
  d24 = +HEAPF32[i16 >> 2];
  d25 = +(d25 > d26 ? d25 : d26);
  d26 = +(d23 > d24 ? d23 : d24);
  i27 = i10 + (i21 * 28 | 0) + 8 | 0;
  HEAPF32[i27 >> 2] = d25;
  HEAPF32[i27 + 4 >> 2] = d26;
  d26 = +HEAPF32[i11 >> 2] - +HEAPF32[i22 >> 2];
  HEAPF32[i3 >> 2] = +HEAPF32[i2 >> 2] - +HEAPF32[i7 >> 2];
  HEAPF32[i12 >> 2] = d26;
  __ZN12b2BroadPhase9MoveProxyEiRK6b2AABBRK6b2Vec2(i8, HEAP32[i10 + (i21 * 28 | 0) + 24 >> 2] | 0, i17, i3);
  i21 = i21 + 1 | 0;
 } while ((i21 | 0) < (HEAP32[i4 >> 2] | 0));
 STACKTOP = i9;
 return;
}
function __ZN12b2EPCollider24ComputePolygonSeparationEv(i2, i9) {
 i2 = i2 | 0;
 i9 = i9 | 0;
 var i1 = 0, i3 = 0, i4 = 0, i5 = 0, d6 = 0.0, d7 = 0.0, i8 = 0, d10 = 0.0, d11 = 0.0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, d16 = 0.0, d17 = 0.0, d18 = 0.0, d19 = 0.0, i20 = 0, d21 = 0.0, d22 = 0.0, d23 = 0.0, d24 = 0.0, d25 = 0.0, d26 = 0.0;
 i15 = STACKTOP;
 HEAP32[i2 >> 2] = 0;
 i3 = i2 + 4 | 0;
 HEAP32[i3 >> 2] = -1;
 i4 = i2 + 8 | 0;
 HEAPF32[i4 >> 2] = -3.4028234663852886e+38;
 d7 = +HEAPF32[i9 + 216 >> 2];
 d6 = +HEAPF32[i9 + 212 >> 2];
 i5 = HEAP32[i9 + 128 >> 2] | 0;
 if ((i5 | 0) <= 0) {
  STACKTOP = i15;
  return;
 }
 d17 = +HEAPF32[i9 + 164 >> 2];
 d18 = +HEAPF32[i9 + 168 >> 2];
 d11 = +HEAPF32[i9 + 172 >> 2];
 d10 = +HEAPF32[i9 + 176 >> 2];
 d16 = +HEAPF32[i9 + 244 >> 2];
 i12 = i9 + 228 | 0;
 i13 = i9 + 232 | 0;
 i14 = i9 + 236 | 0;
 i1 = i9 + 240 | 0;
 d19 = -3.4028234663852886e+38;
 i20 = 0;
 while (1) {
  d23 = +HEAPF32[i9 + (i20 << 3) + 64 >> 2];
  d21 = -d23;
  d22 = -+HEAPF32[i9 + (i20 << 3) + 68 >> 2];
  d26 = +HEAPF32[i9 + (i20 << 3) >> 2];
  d25 = +HEAPF32[i9 + (i20 << 3) + 4 >> 2];
  d24 = (d26 - d17) * d21 + (d25 - d18) * d22;
  d25 = (d26 - d11) * d21 + (d25 - d10) * d22;
  d24 = d24 < d25 ? d24 : d25;
  if (d24 > d16) {
   break;
  }
  if (!(d7 * d23 + d6 * d22 >= 0.0)) {
   if (!((d21 - +HEAPF32[i12 >> 2]) * d6 + (d22 - +HEAPF32[i13 >> 2]) * d7 < -.03490658849477768) & d24 > d19) {
    i8 = 8;
   }
  } else {
   if (!((d21 - +HEAPF32[i14 >> 2]) * d6 + (d22 - +HEAPF32[i1 >> 2]) * d7 < -.03490658849477768) & d24 > d19) {
    i8 = 8;
   }
  }
  if ((i8 | 0) == 8) {
   i8 = 0;
   HEAP32[i2 >> 2] = 2;
   HEAP32[i3 >> 2] = i20;
   HEAPF32[i4 >> 2] = d24;
   d19 = d24;
  }
  i20 = i20 + 1 | 0;
  if ((i20 | 0) >= (i5 | 0)) {
   i8 = 10;
   break;
  }
 }
 if ((i8 | 0) == 10) {
  STACKTOP = i15;
  return;
 }
 HEAP32[i2 >> 2] = 2;
 HEAP32[i3 >> 2] = i20;
 HEAPF32[i4 >> 2] = d24;
 STACKTOP = i15;
 return;
}
function __ZNK11b2EdgeShape7RayCastEP15b2RayCastOutputRK14b2RayCastInputRK11b2Transformi(i17, i1, i2, i18, i3) {
 i17 = i17 | 0;
 i1 = i1 | 0;
 i2 = i2 | 0;
 i18 = i18 | 0;
 i3 = i3 | 0;
 var d4 = 0.0, d5 = 0.0, d6 = 0.0, d7 = 0.0, d8 = 0.0, d9 = 0.0, d10 = 0.0, d11 = 0.0, d12 = 0.0, d13 = 0.0, d14 = 0.0, d15 = 0.0, d16 = 0.0;
 i3 = STACKTOP;
 d6 = +HEAPF32[i18 >> 2];
 d7 = +HEAPF32[i2 >> 2] - d6;
 d9 = +HEAPF32[i18 + 4 >> 2];
 d4 = +HEAPF32[i2 + 4 >> 2] - d9;
 d11 = +HEAPF32[i18 + 12 >> 2];
 d5 = +HEAPF32[i18 + 8 >> 2];
 d8 = d7 * d11 + d4 * d5;
 d7 = d11 * d4 - d7 * d5;
 d6 = +HEAPF32[i2 + 8 >> 2] - d6;
 d9 = +HEAPF32[i2 + 12 >> 2] - d9;
 d4 = d11 * d6 + d5 * d9 - d8;
 d6 = d11 * d9 - d5 * d6 - d7;
 i18 = i17 + 12 | 0;
 d5 = +HEAPF32[i18 >> 2];
 d9 = +HEAPF32[i18 + 4 >> 2];
 i18 = i17 + 20 | 0;
 d11 = +HEAPF32[i18 >> 2];
 d11 = d11 - d5;
 d12 = +HEAPF32[i18 + 4 >> 2] - d9;
 d15 = -d11;
 d10 = d11 * d11 + d12 * d12;
 d13 = +Math_sqrt(+d10);
 if (d13 < 1.1920928955078125e-7) {
  d13 = d12;
 } else {
  d16 = 1.0 / d13;
  d13 = d12 * d16;
  d15 = d16 * d15;
 }
 d14 = (d9 - d7) * d15 + (d5 - d8) * d13;
 d16 = d6 * d15 + d4 * d13;
 if (d16 == 0.0) {
  i18 = 0;
  STACKTOP = i3;
  return i18 | 0;
 }
 d16 = d14 / d16;
 if (d16 < 0.0) {
  i18 = 0;
  STACKTOP = i3;
  return i18 | 0;
 }
 if (+HEAPF32[i2 + 16 >> 2] < d16 | d10 == 0.0) {
  i18 = 0;
  STACKTOP = i3;
  return i18 | 0;
 }
 d12 = (d11 * (d8 + d4 * d16 - d5) + d12 * (d7 + d6 * d16 - d9)) / d10;
 if (d12 < 0.0 | d12 > 1.0) {
  i18 = 0;
  STACKTOP = i3;
  return i18 | 0;
 }
 HEAPF32[i1 + 8 >> 2] = d16;
 if (d14 > 0.0) {
  d14 = +-d13;
  d16 = +-d15;
  i18 = i1;
  HEAPF32[i18 >> 2] = d14;
  HEAPF32[i18 + 4 >> 2] = d16;
  i18 = 1;
  STACKTOP = i3;
  return i18 | 0;
 } else {
  d14 = +d13;
  d16 = +d15;
  i18 = i1;
  HEAPF32[i18 >> 2] = d14;
  HEAPF32[i18 + 4 >> 2] = d16;
  i18 = 1;
  STACKTOP = i3;
  return i18 | 0;
 }
 return 0;
}
function ___dynamic_cast(i7, i6, i11, i5) {
 i7 = i7 | 0;
 i6 = i6 | 0;
 i11 = i11 | 0;
 i5 = i5 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, i8 = 0, i9 = 0, i10 = 0, i12 = 0, i13 = 0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 64 | 0;
 i2 = i1;
 i3 = HEAP32[i7 >> 2] | 0;
 i4 = i7 + (HEAP32[i3 + -8 >> 2] | 0) | 0;
 i3 = HEAP32[i3 + -4 >> 2] | 0;
 HEAP32[i2 >> 2] = i11;
 HEAP32[i2 + 4 >> 2] = i7;
 HEAP32[i2 + 8 >> 2] = i6;
 HEAP32[i2 + 12 >> 2] = i5;
 i9 = i2 + 16 | 0;
 i10 = i2 + 20 | 0;
 i6 = i2 + 24 | 0;
 i8 = i2 + 28 | 0;
 i5 = i2 + 32 | 0;
 i7 = i2 + 40 | 0;
 i12 = (i3 | 0) == (i11 | 0);
 i13 = i9 + 0 | 0;
 i11 = i13 + 36 | 0;
 do {
  HEAP32[i13 >> 2] = 0;
  i13 = i13 + 4 | 0;
 } while ((i13 | 0) < (i11 | 0));
 HEAP16[i9 + 36 >> 1] = 0;
 HEAP8[i9 + 38 | 0] = 0;
 if (i12) {
  HEAP32[i2 + 48 >> 2] = 1;
  FUNCTION_TABLE_viiiiii[HEAP32[(HEAP32[i3 >> 2] | 0) + 20 >> 2] & 3](i3, i2, i4, i4, 1, 0);
  i13 = (HEAP32[i6 >> 2] | 0) == 1 ? i4 : 0;
  STACKTOP = i1;
  return i13 | 0;
 }
 FUNCTION_TABLE_viiiii[HEAP32[(HEAP32[i3 >> 2] | 0) + 24 >> 2] & 3](i3, i2, i4, 1, 0);
 i2 = HEAP32[i2 + 36 >> 2] | 0;
 if ((i2 | 0) == 0) {
  if ((HEAP32[i7 >> 2] | 0) != 1) {
   i13 = 0;
   STACKTOP = i1;
   return i13 | 0;
  }
  if ((HEAP32[i8 >> 2] | 0) != 1) {
   i13 = 0;
   STACKTOP = i1;
   return i13 | 0;
  }
  i13 = (HEAP32[i5 >> 2] | 0) == 1 ? HEAP32[i10 >> 2] | 0 : 0;
  STACKTOP = i1;
  return i13 | 0;
 } else if ((i2 | 0) == 1) {
  if ((HEAP32[i6 >> 2] | 0) != 1) {
   if ((HEAP32[i7 >> 2] | 0) != 0) {
    i13 = 0;
    STACKTOP = i1;
    return i13 | 0;
   }
   if ((HEAP32[i8 >> 2] | 0) != 1) {
    i13 = 0;
    STACKTOP = i1;
    return i13 | 0;
   }
   if ((HEAP32[i5 >> 2] | 0) != 1) {
    i13 = 0;
    STACKTOP = i1;
    return i13 | 0;
   }
  }
  i13 = HEAP32[i9 >> 2] | 0;
  STACKTOP = i1;
  return i13 | 0;
 } else {
  i13 = 0;
  STACKTOP = i1;
  return i13 | 0;
 }
 return 0;
}
function __ZNK14b2PolygonShape11ComputeMassEP10b2MassDataf(i4, i1, d2) {
 i4 = i4 | 0;
 i1 = i1 | 0;
 d2 = +d2;
 var i3 = 0, i5 = 0, d6 = 0.0, d7 = 0.0, d8 = 0.0, d9 = 0.0, d10 = 0.0, d11 = 0.0, i12 = 0, d13 = 0.0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, d18 = 0.0, i19 = 0, d20 = 0.0, d21 = 0.0, d22 = 0.0, d23 = 0.0;
 i3 = STACKTOP;
 i5 = HEAP32[i4 + 148 >> 2] | 0;
 if ((i5 | 0) > 2) {
  d7 = 0.0;
  d6 = 0.0;
  i12 = 0;
 } else {
  ___assert_fail(432, 328, 306, 456);
 }
 do {
  d6 = d6 + +HEAPF32[i4 + (i12 << 3) + 20 >> 2];
  d7 = d7 + +HEAPF32[i4 + (i12 << 3) + 24 >> 2];
  i12 = i12 + 1 | 0;
 } while ((i12 | 0) < (i5 | 0));
 d11 = 1.0 / +(i5 | 0);
 d6 = d6 * d11;
 d11 = d7 * d11;
 i16 = i4 + 20 | 0;
 i19 = i4 + 24 | 0;
 d9 = 0.0;
 d10 = 0.0;
 d7 = 0.0;
 d8 = 0.0;
 i17 = 0;
 do {
  d18 = +HEAPF32[i4 + (i17 << 3) + 20 >> 2] - d6;
  d13 = +HEAPF32[i4 + (i17 << 3) + 24 >> 2] - d11;
  i17 = i17 + 1 | 0;
  i12 = (i17 | 0) < (i5 | 0);
  if (i12) {
   i14 = i4 + (i17 << 3) + 20 | 0;
   i15 = i4 + (i17 << 3) + 24 | 0;
  } else {
   i14 = i16;
   i15 = i19;
  }
  d21 = +HEAPF32[i14 >> 2] - d6;
  d20 = +HEAPF32[i15 >> 2] - d11;
  d22 = d18 * d20 - d13 * d21;
  d23 = d22 * .5;
  d8 = d8 + d23;
  d23 = d23 * .3333333432674408;
  d9 = d9 + (d18 + d21) * d23;
  d10 = d10 + (d13 + d20) * d23;
  d7 = d7 + d22 * .0833333358168602 * (d21 * d21 + (d18 * d18 + d18 * d21) + (d20 * d20 + (d13 * d13 + d13 * d20)));
 } while (i12);
 d13 = d8 * d2;
 HEAPF32[i1 >> 2] = d13;
 if (d8 > 1.1920928955078125e-7) {
  d23 = 1.0 / d8;
  d22 = d9 * d23;
  d23 = d10 * d23;
  d20 = d6 + d22;
  d21 = d11 + d23;
  d11 = +d20;
  d18 = +d21;
  i19 = i1 + 4 | 0;
  HEAPF32[i19 >> 2] = d11;
  HEAPF32[i19 + 4 >> 2] = d18;
  HEAPF32[i1 + 12 >> 2] = d7 * d2 + d13 * (d20 * d20 + d21 * d21 - (d22 * d22 + d23 * d23));
  STACKTOP = i3;
  return;
 } else {
  ___assert_fail(472, 328, 352, 456);
 }
}
function __ZN16b2ContactManager7DestroyEP9b2Contact(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0;
 i3 = STACKTOP;
 i5 = HEAP32[(HEAP32[i2 + 48 >> 2] | 0) + 8 >> 2] | 0;
 i4 = HEAP32[(HEAP32[i2 + 52 >> 2] | 0) + 8 >> 2] | 0;
 i6 = HEAP32[i1 + 72 >> 2] | 0;
 if ((i6 | 0) != 0 ? (HEAP32[i2 + 4 >> 2] & 2 | 0) != 0 : 0) {
  FUNCTION_TABLE_vii[HEAP32[(HEAP32[i6 >> 2] | 0) + 12 >> 2] & 15](i6, i2);
 }
 i7 = i2 + 8 | 0;
 i8 = HEAP32[i7 >> 2] | 0;
 i6 = i2 + 12 | 0;
 if ((i8 | 0) != 0) {
  HEAP32[i8 + 12 >> 2] = HEAP32[i6 >> 2];
 }
 i8 = HEAP32[i6 >> 2] | 0;
 if ((i8 | 0) != 0) {
  HEAP32[i8 + 8 >> 2] = HEAP32[i7 >> 2];
 }
 i7 = i1 + 60 | 0;
 if ((HEAP32[i7 >> 2] | 0) == (i2 | 0)) {
  HEAP32[i7 >> 2] = HEAP32[i6 >> 2];
 }
 i7 = i2 + 24 | 0;
 i8 = HEAP32[i7 >> 2] | 0;
 i6 = i2 + 28 | 0;
 if ((i8 | 0) != 0) {
  HEAP32[i8 + 12 >> 2] = HEAP32[i6 >> 2];
 }
 i8 = HEAP32[i6 >> 2] | 0;
 if ((i8 | 0) != 0) {
  HEAP32[i8 + 8 >> 2] = HEAP32[i7 >> 2];
 }
 i5 = i5 + 112 | 0;
 if ((i2 + 16 | 0) == (HEAP32[i5 >> 2] | 0)) {
  HEAP32[i5 >> 2] = HEAP32[i6 >> 2];
 }
 i6 = i2 + 40 | 0;
 i7 = HEAP32[i6 >> 2] | 0;
 i5 = i2 + 44 | 0;
 if ((i7 | 0) != 0) {
  HEAP32[i7 + 12 >> 2] = HEAP32[i5 >> 2];
 }
 i7 = HEAP32[i5 >> 2] | 0;
 if ((i7 | 0) != 0) {
  HEAP32[i7 + 8 >> 2] = HEAP32[i6 >> 2];
 }
 i4 = i4 + 112 | 0;
 if ((i2 + 32 | 0) != (HEAP32[i4 >> 2] | 0)) {
  i8 = i1 + 76 | 0;
  i8 = HEAP32[i8 >> 2] | 0;
  __ZN9b2Contact7DestroyEPS_P16b2BlockAllocator(i2, i8);
  i8 = i1 + 64 | 0;
  i7 = HEAP32[i8 >> 2] | 0;
  i7 = i7 + -1 | 0;
  HEAP32[i8 >> 2] = i7;
  STACKTOP = i3;
  return;
 }
 HEAP32[i4 >> 2] = HEAP32[i5 >> 2];
 i8 = i1 + 76 | 0;
 i8 = HEAP32[i8 >> 2] | 0;
 __ZN9b2Contact7DestroyEPS_P16b2BlockAllocator(i2, i8);
 i8 = i1 + 64 | 0;
 i7 = HEAP32[i8 >> 2] | 0;
 i7 = i7 + -1 | 0;
 HEAP32[i8 >> 2] = i7;
 STACKTOP = i3;
 return;
}
function __ZNK10__cxxabiv120__si_class_type_info16search_below_dstEPNS_19__dynamic_cast_infoEPKvib(i6, i3, i4, i8, i7) {
 i6 = i6 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 i8 = i8 | 0;
 i7 = i7 | 0;
 var i1 = 0, i2 = 0, i5 = 0, i9 = 0, i10 = 0;
 i1 = STACKTOP;
 if ((i6 | 0) == (HEAP32[i3 + 8 >> 2] | 0)) {
  if ((HEAP32[i3 + 4 >> 2] | 0) != (i4 | 0)) {
   STACKTOP = i1;
   return;
  }
  i2 = i3 + 28 | 0;
  if ((HEAP32[i2 >> 2] | 0) == 1) {
   STACKTOP = i1;
   return;
  }
  HEAP32[i2 >> 2] = i8;
  STACKTOP = i1;
  return;
 }
 if ((i6 | 0) != (HEAP32[i3 >> 2] | 0)) {
  i9 = HEAP32[i6 + 8 >> 2] | 0;
  FUNCTION_TABLE_viiiii[HEAP32[(HEAP32[i9 >> 2] | 0) + 24 >> 2] & 3](i9, i3, i4, i8, i7);
  STACKTOP = i1;
  return;
 }
 if ((HEAP32[i3 + 16 >> 2] | 0) != (i4 | 0) ? (i5 = i3 + 20 | 0, (HEAP32[i5 >> 2] | 0) != (i4 | 0)) : 0) {
  HEAP32[i3 + 32 >> 2] = i8;
  i8 = i3 + 44 | 0;
  if ((HEAP32[i8 >> 2] | 0) == 4) {
   STACKTOP = i1;
   return;
  }
  i9 = i3 + 52 | 0;
  HEAP8[i9] = 0;
  i10 = i3 + 53 | 0;
  HEAP8[i10] = 0;
  i6 = HEAP32[i6 + 8 >> 2] | 0;
  FUNCTION_TABLE_viiiiii[HEAP32[(HEAP32[i6 >> 2] | 0) + 20 >> 2] & 3](i6, i3, i4, i4, 1, i7);
  if ((HEAP8[i10] | 0) != 0) {
   if ((HEAP8[i9] | 0) == 0) {
    i6 = 1;
    i2 = 13;
   }
  } else {
   i6 = 0;
   i2 = 13;
  }
  do {
   if ((i2 | 0) == 13) {
    HEAP32[i5 >> 2] = i4;
    i10 = i3 + 40 | 0;
    HEAP32[i10 >> 2] = (HEAP32[i10 >> 2] | 0) + 1;
    if ((HEAP32[i3 + 36 >> 2] | 0) == 1 ? (HEAP32[i3 + 24 >> 2] | 0) == 2 : 0) {
     HEAP8[i3 + 54 | 0] = 1;
     if (i6) {
      break;
     }
    } else {
     i2 = 16;
    }
    if ((i2 | 0) == 16 ? i6 : 0) {
     break;
    }
    HEAP32[i8 >> 2] = 4;
    STACKTOP = i1;
    return;
   }
  } while (0);
  HEAP32[i8 >> 2] = 3;
  STACKTOP = i1;
  return;
 }
 if ((i8 | 0) != 1) {
  STACKTOP = i1;
  return;
 }
 HEAP32[i3 + 32 >> 2] = 1;
 STACKTOP = i1;
 return;
}
function __ZN16b2BlockAllocator8AllocateEi(i4, i2) {
 i4 = i4 | 0;
 i2 = i2 | 0;
 var i1 = 0, i3 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0;
 i1 = STACKTOP;
 if ((i2 | 0) == 0) {
  i9 = 0;
  STACKTOP = i1;
  return i9 | 0;
 }
 if ((i2 | 0) <= 0) {
  ___assert_fail(1376, 1312, 104, 1392);
 }
 if ((i2 | 0) > 640) {
  i9 = __Z7b2Alloci(i2) | 0;
  STACKTOP = i1;
  return i9 | 0;
 }
 i9 = HEAP8[632 + i2 | 0] | 0;
 i5 = i9 & 255;
 if (!((i9 & 255) < 14)) {
  ___assert_fail(1408, 1312, 112, 1392);
 }
 i2 = i4 + (i5 << 2) + 12 | 0;
 i3 = HEAP32[i2 >> 2] | 0;
 if ((i3 | 0) != 0) {
  HEAP32[i2 >> 2] = HEAP32[i3 >> 2];
  i9 = i3;
  STACKTOP = i1;
  return i9 | 0;
 }
 i3 = i4 + 4 | 0;
 i6 = HEAP32[i3 >> 2] | 0;
 i7 = i4 + 8 | 0;
 if ((i6 | 0) == (HEAP32[i7 >> 2] | 0)) {
  i9 = HEAP32[i4 >> 2] | 0;
  i6 = i6 + 128 | 0;
  HEAP32[i7 >> 2] = i6;
  i6 = __Z7b2Alloci(i6 << 3) | 0;
  HEAP32[i4 >> 2] = i6;
  _memcpy(i6 | 0, i9 | 0, HEAP32[i3 >> 2] << 3 | 0) | 0;
  _memset((HEAP32[i4 >> 2] | 0) + (HEAP32[i3 >> 2] << 3) | 0, 0, 1024) | 0;
  __Z6b2FreePv(i9);
  i6 = HEAP32[i3 >> 2] | 0;
 }
 i9 = HEAP32[i4 >> 2] | 0;
 i7 = __Z7b2Alloci(16384) | 0;
 i4 = i9 + (i6 << 3) + 4 | 0;
 HEAP32[i4 >> 2] = i7;
 i5 = HEAP32[576 + (i5 << 2) >> 2] | 0;
 HEAP32[i9 + (i6 << 3) >> 2] = i5;
 i6 = 16384 / (i5 | 0) | 0;
 if ((Math_imul(i6, i5) | 0) >= 16385) {
  ___assert_fail(1448, 1312, 140, 1392);
 }
 i6 = i6 + -1 | 0;
 if ((i6 | 0) > 0) {
  i9 = 0;
  while (1) {
   i8 = i9 + 1 | 0;
   HEAP32[i7 + (Math_imul(i9, i5) | 0) >> 2] = i7 + (Math_imul(i8, i5) | 0);
   i7 = HEAP32[i4 >> 2] | 0;
   if ((i8 | 0) == (i6 | 0)) {
    break;
   } else {
    i9 = i8;
   }
  }
 }
 HEAP32[i7 + (Math_imul(i6, i5) | 0) >> 2] = 0;
 HEAP32[i2 >> 2] = HEAP32[HEAP32[i4 >> 2] >> 2];
 HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) + 1;
 i9 = HEAP32[i4 >> 2] | 0;
 STACKTOP = i1;
 return i9 | 0;
}
function __ZN9b2Contact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator(i4, i5, i1, i3, i6) {
 i4 = i4 | 0;
 i5 = i5 | 0;
 i1 = i1 | 0;
 i3 = i3 | 0;
 i6 = i6 | 0;
 var i2 = 0, i7 = 0, i8 = 0, i9 = 0;
 i2 = STACKTOP;
 if ((HEAP8[4200] | 0) == 0) {
  HEAP32[1002] = 3;
  HEAP32[4012 >> 2] = 3;
  HEAP8[4016 | 0] = 1;
  HEAP32[4104 >> 2] = 4;
  HEAP32[4108 >> 2] = 4;
  HEAP8[4112 | 0] = 1;
  HEAP32[4032 >> 2] = 4;
  HEAP32[4036 >> 2] = 4;
  HEAP8[4040 | 0] = 0;
  HEAP32[4128 >> 2] = 5;
  HEAP32[4132 >> 2] = 5;
  HEAP8[4136 | 0] = 1;
  HEAP32[4056 >> 2] = 6;
  HEAP32[4060 >> 2] = 6;
  HEAP8[4064 | 0] = 1;
  HEAP32[4020 >> 2] = 6;
  HEAP32[4024 >> 2] = 6;
  HEAP8[4028 | 0] = 0;
  HEAP32[4080 >> 2] = 7;
  HEAP32[4084 >> 2] = 7;
  HEAP8[4088 | 0] = 1;
  HEAP32[4116 >> 2] = 7;
  HEAP32[4120 >> 2] = 7;
  HEAP8[4124 | 0] = 0;
  HEAP32[4152 >> 2] = 8;
  HEAP32[4156 >> 2] = 8;
  HEAP8[4160 | 0] = 1;
  HEAP32[4044 >> 2] = 8;
  HEAP32[4048 >> 2] = 8;
  HEAP8[4052 | 0] = 0;
  HEAP32[4176 >> 2] = 9;
  HEAP32[4180 >> 2] = 9;
  HEAP8[4184 | 0] = 1;
  HEAP32[4140 >> 2] = 9;
  HEAP32[4144 >> 2] = 9;
  HEAP8[4148 | 0] = 0;
  HEAP8[4200] = 1;
 }
 i7 = HEAP32[(HEAP32[i4 + 12 >> 2] | 0) + 4 >> 2] | 0;
 i8 = HEAP32[(HEAP32[i1 + 12 >> 2] | 0) + 4 >> 2] | 0;
 if (!(i7 >>> 0 < 4)) {
  ___assert_fail(4208, 4256, 80, 4344);
 }
 if (!(i8 >>> 0 < 4)) {
  ___assert_fail(4296, 4256, 81, 4344);
 }
 i9 = HEAP32[4008 + (i7 * 48 | 0) + (i8 * 12 | 0) >> 2] | 0;
 if ((i9 | 0) == 0) {
  i9 = 0;
  STACKTOP = i2;
  return i9 | 0;
 }
 if ((HEAP8[4008 + (i7 * 48 | 0) + (i8 * 12 | 0) + 8 | 0] | 0) == 0) {
  i9 = FUNCTION_TABLE_iiiiii[i9 & 15](i1, i3, i4, i5, i6) | 0;
  STACKTOP = i2;
  return i9 | 0;
 } else {
  i9 = FUNCTION_TABLE_iiiiii[i9 & 15](i4, i5, i1, i3, i6) | 0;
  STACKTOP = i2;
  return i9 | 0;
 }
 return 0;
}
function __ZN13b2DynamicTree9MoveProxyEiRK6b2AABBRK6b2Vec2(i1, i2, i13, i9) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i13 = i13 | 0;
 i9 = i9 | 0;
 var i3 = 0, i4 = 0, d5 = 0.0, d6 = 0.0, d7 = 0.0, d8 = 0.0, d10 = 0.0, d11 = 0.0, i12 = 0;
 i4 = STACKTOP;
 if (!((i2 | 0) > -1)) {
  ___assert_fail(3072, 2944, 135, 3152);
 }
 if ((HEAP32[i1 + 12 >> 2] | 0) <= (i2 | 0)) {
  ___assert_fail(3072, 2944, 135, 3152);
 }
 i3 = i1 + 4 | 0;
 i12 = HEAP32[i3 >> 2] | 0;
 if (!((HEAP32[i12 + (i2 * 36 | 0) + 24 >> 2] | 0) == -1)) {
  ___assert_fail(3120, 2944, 137, 3152);
 }
 if (((+HEAPF32[i12 + (i2 * 36 | 0) >> 2] <= +HEAPF32[i13 >> 2] ? +HEAPF32[i12 + (i2 * 36 | 0) + 4 >> 2] <= +HEAPF32[i13 + 4 >> 2] : 0) ? +HEAPF32[i13 + 8 >> 2] <= +HEAPF32[i12 + (i2 * 36 | 0) + 8 >> 2] : 0) ? +HEAPF32[i13 + 12 >> 2] <= +HEAPF32[i12 + (i2 * 36 | 0) + 12 >> 2] : 0) {
  i13 = 0;
  STACKTOP = i4;
  return i13 | 0;
 }
 __ZN13b2DynamicTree10RemoveLeafEi(i1, i2);
 i12 = i13;
 d6 = +HEAPF32[i12 >> 2];
 d8 = +HEAPF32[i12 + 4 >> 2];
 i13 = i13 + 8 | 0;
 d10 = +HEAPF32[i13 >> 2];
 d6 = d6 + -.10000000149011612;
 d8 = d8 + -.10000000149011612;
 d10 = d10 + .10000000149011612;
 d5 = +HEAPF32[i13 + 4 >> 2] + .10000000149011612;
 d11 = +HEAPF32[i9 >> 2] * 2.0;
 d7 = +HEAPF32[i9 + 4 >> 2] * 2.0;
 if (d11 < 0.0) {
  d6 = d6 + d11;
 } else {
  d10 = d11 + d10;
 }
 if (d7 < 0.0) {
  d8 = d8 + d7;
 } else {
  d5 = d7 + d5;
 }
 i13 = HEAP32[i3 >> 2] | 0;
 d7 = +d6;
 d11 = +d8;
 i12 = i13 + (i2 * 36 | 0) | 0;
 HEAPF32[i12 >> 2] = d7;
 HEAPF32[i12 + 4 >> 2] = d11;
 d10 = +d10;
 d11 = +d5;
 i13 = i13 + (i2 * 36 | 0) + 8 | 0;
 HEAPF32[i13 >> 2] = d10;
 HEAPF32[i13 + 4 >> 2] = d11;
 __ZN13b2DynamicTree10InsertLeafEi(i1, i2);
 i13 = 1;
 STACKTOP = i4;
 return i13 | 0;
}
function __ZNK9b2Simplex16GetWitnessPointsEP6b2Vec2S1_(i1, i4, i5) {
 i1 = i1 | 0;
 i4 = i4 | 0;
 i5 = i5 | 0;
 var i2 = 0, i3 = 0, d6 = 0.0, d7 = 0.0, d8 = 0.0, i9 = 0, i10 = 0, d11 = 0.0;
 i2 = STACKTOP;
 i3 = HEAP32[i1 + 108 >> 2] | 0;
 if ((i3 | 0) == 2) {
  i9 = i1 + 24 | 0;
  d7 = +HEAPF32[i9 >> 2];
  i3 = i1 + 60 | 0;
  d8 = +HEAPF32[i3 >> 2];
  d6 = +(d7 * +HEAPF32[i1 >> 2] + d8 * +HEAPF32[i1 + 36 >> 2]);
  d8 = +(d7 * +HEAPF32[i1 + 4 >> 2] + d8 * +HEAPF32[i1 + 40 >> 2]);
  HEAPF32[i4 >> 2] = d6;
  HEAPF32[i4 + 4 >> 2] = d8;
  d8 = +HEAPF32[i9 >> 2];
  d6 = +HEAPF32[i3 >> 2];
  d7 = +(d8 * +HEAPF32[i1 + 8 >> 2] + d6 * +HEAPF32[i1 + 44 >> 2]);
  d6 = +(d8 * +HEAPF32[i1 + 12 >> 2] + d6 * +HEAPF32[i1 + 48 >> 2]);
  HEAPF32[i5 >> 2] = d7;
  HEAPF32[i5 + 4 >> 2] = d6;
  STACKTOP = i2;
  return;
 } else if ((i3 | 0) == 1) {
  i10 = i1;
  i9 = HEAP32[i10 + 4 >> 2] | 0;
  i3 = i4;
  HEAP32[i3 >> 2] = HEAP32[i10 >> 2];
  HEAP32[i3 + 4 >> 2] = i9;
  i3 = i1 + 8 | 0;
  i4 = HEAP32[i3 + 4 >> 2] | 0;
  i9 = i5;
  HEAP32[i9 >> 2] = HEAP32[i3 >> 2];
  HEAP32[i9 + 4 >> 2] = i4;
  STACKTOP = i2;
  return;
 } else if ((i3 | 0) == 0) {
  ___assert_fail(2712, 2672, 217, 2752);
 } else if ((i3 | 0) == 3) {
  d11 = +HEAPF32[i1 + 24 >> 2];
  d6 = +HEAPF32[i1 + 60 >> 2];
  d8 = +HEAPF32[i1 + 96 >> 2];
  d7 = +(d11 * +HEAPF32[i1 >> 2] + d6 * +HEAPF32[i1 + 36 >> 2] + d8 * +HEAPF32[i1 + 72 >> 2]);
  d8 = +(d11 * +HEAPF32[i1 + 4 >> 2] + d6 * +HEAPF32[i1 + 40 >> 2] + d8 * +HEAPF32[i1 + 76 >> 2]);
  i10 = i4;
  HEAPF32[i10 >> 2] = d7;
  HEAPF32[i10 + 4 >> 2] = d8;
  i10 = i5;
  HEAPF32[i10 >> 2] = d7;
  HEAPF32[i10 + 4 >> 2] = d8;
  STACKTOP = i2;
  return;
 } else {
  ___assert_fail(2712, 2672, 236, 2752);
 }
}
function __ZNK12b2ChainShape12GetChildEdgeEP11b2EdgeShapei(i4, i3, i1) {
 i4 = i4 | 0;
 i3 = i3 | 0;
 i1 = i1 | 0;
 var i2 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0;
 i2 = STACKTOP;
 if (!((i1 | 0) > -1)) {
  ___assert_fail(6832, 6792, 89, 6872);
 }
 i5 = i4 + 16 | 0;
 if (((HEAP32[i5 >> 2] | 0) + -1 | 0) <= (i1 | 0)) {
  ___assert_fail(6832, 6792, 89, 6872);
 }
 HEAP32[i3 + 4 >> 2] = 1;
 HEAPF32[i3 + 8 >> 2] = +HEAPF32[i4 + 8 >> 2];
 i6 = i4 + 12 | 0;
 i7 = (HEAP32[i6 >> 2] | 0) + (i1 << 3) | 0;
 i8 = HEAP32[i7 + 4 >> 2] | 0;
 i9 = i3 + 12 | 0;
 HEAP32[i9 >> 2] = HEAP32[i7 >> 2];
 HEAP32[i9 + 4 >> 2] = i8;
 i9 = (HEAP32[i6 >> 2] | 0) + (i1 + 1 << 3) | 0;
 i8 = HEAP32[i9 + 4 >> 2] | 0;
 i7 = i3 + 20 | 0;
 HEAP32[i7 >> 2] = HEAP32[i9 >> 2];
 HEAP32[i7 + 4 >> 2] = i8;
 i7 = i3 + 28 | 0;
 if ((i1 | 0) > 0) {
  i10 = (HEAP32[i6 >> 2] | 0) + (i1 + -1 << 3) | 0;
  i8 = HEAP32[i10 + 4 >> 2] | 0;
  i9 = i7;
  HEAP32[i9 >> 2] = HEAP32[i10 >> 2];
  HEAP32[i9 + 4 >> 2] = i8;
  HEAP8[i3 + 44 | 0] = 1;
 } else {
  i8 = i4 + 20 | 0;
  i9 = HEAP32[i8 + 4 >> 2] | 0;
  i10 = i7;
  HEAP32[i10 >> 2] = HEAP32[i8 >> 2];
  HEAP32[i10 + 4 >> 2] = i9;
  HEAP8[i3 + 44 | 0] = HEAP8[i4 + 36 | 0] | 0;
 }
 i7 = i3 + 36 | 0;
 if (((HEAP32[i5 >> 2] | 0) + -2 | 0) > (i1 | 0)) {
  i8 = (HEAP32[i6 >> 2] | 0) + (i1 + 2 << 3) | 0;
  i9 = HEAP32[i8 + 4 >> 2] | 0;
  i10 = i7;
  HEAP32[i10 >> 2] = HEAP32[i8 >> 2];
  HEAP32[i10 + 4 >> 2] = i9;
  HEAP8[i3 + 45 | 0] = 1;
  STACKTOP = i2;
  return;
 } else {
  i8 = i4 + 28 | 0;
  i9 = HEAP32[i8 + 4 >> 2] | 0;
  i10 = i7;
  HEAP32[i10 >> 2] = HEAP32[i8 >> 2];
  HEAP32[i10 + 4 >> 2] = i9;
  HEAP8[i3 + 45 | 0] = HEAP8[i4 + 37 | 0] | 0;
  STACKTOP = i2;
  return;
 }
}
function __ZN15b2DistanceProxy3SetEPK7b2Shapei(i3, i1, i5) {
 i3 = i3 | 0;
 i1 = i1 | 0;
 i5 = i5 | 0;
 var i2 = 0, i4 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0;
 i2 = STACKTOP;
 i4 = HEAP32[i1 + 4 >> 2] | 0;
 if ((i4 | 0) == 1) {
  HEAP32[i3 + 16 >> 2] = i1 + 12;
  HEAP32[i3 + 20 >> 2] = 2;
  HEAPF32[i3 + 24 >> 2] = +HEAPF32[i1 + 8 >> 2];
  STACKTOP = i2;
  return;
 } else if ((i4 | 0) == 3) {
  if (!((i5 | 0) > -1)) {
   ___assert_fail(2632, 2672, 53, 2704);
  }
  i4 = i1 + 16 | 0;
  if ((HEAP32[i4 >> 2] | 0) <= (i5 | 0)) {
   ___assert_fail(2632, 2672, 53, 2704);
  }
  i7 = i1 + 12 | 0;
  i9 = (HEAP32[i7 >> 2] | 0) + (i5 << 3) | 0;
  i8 = HEAP32[i9 + 4 >> 2] | 0;
  i6 = i3;
  HEAP32[i6 >> 2] = HEAP32[i9 >> 2];
  HEAP32[i6 + 4 >> 2] = i8;
  i6 = i5 + 1 | 0;
  i5 = i3 + 8 | 0;
  i7 = HEAP32[i7 >> 2] | 0;
  if ((i6 | 0) < (HEAP32[i4 >> 2] | 0)) {
   i7 = i7 + (i6 << 3) | 0;
   i8 = HEAP32[i7 + 4 >> 2] | 0;
   i9 = i5;
   HEAP32[i9 >> 2] = HEAP32[i7 >> 2];
   HEAP32[i9 + 4 >> 2] = i8;
  } else {
   i8 = HEAP32[i7 + 4 >> 2] | 0;
   i9 = i5;
   HEAP32[i9 >> 2] = HEAP32[i7 >> 2];
   HEAP32[i9 + 4 >> 2] = i8;
  }
  HEAP32[i3 + 16 >> 2] = i3;
  HEAP32[i3 + 20 >> 2] = 2;
  HEAPF32[i3 + 24 >> 2] = +HEAPF32[i1 + 8 >> 2];
  STACKTOP = i2;
  return;
 } else if ((i4 | 0) == 2) {
  HEAP32[i3 + 16 >> 2] = i1 + 20;
  HEAP32[i3 + 20 >> 2] = HEAP32[i1 + 148 >> 2];
  HEAPF32[i3 + 24 >> 2] = +HEAPF32[i1 + 8 >> 2];
  STACKTOP = i2;
  return;
 } else if ((i4 | 0) == 0) {
  HEAP32[i3 + 16 >> 2] = i1 + 12;
  HEAP32[i3 + 20 >> 2] = 1;
  HEAPF32[i3 + 24 >> 2] = +HEAPF32[i1 + 8 >> 2];
  STACKTOP = i2;
  return;
 } else {
  ___assert_fail(2712, 2672, 81, 2704);
 }
}
function __ZL16b2EdgeSeparationPK14b2PolygonShapeRK11b2TransformiS1_S4_(i2, i7, i4, i5, i6) {
 i2 = i2 | 0;
 i7 = i7 | 0;
 i4 = i4 | 0;
 i5 = i5 | 0;
 i6 = i6 | 0;
 var d1 = 0.0, d3 = 0.0, d8 = 0.0, d9 = 0.0, d10 = 0.0, d11 = 0.0, i12 = 0, i13 = 0, d14 = 0.0, d15 = 0.0, d16 = 0.0, d17 = 0.0, i18 = 0, i19 = 0, i20 = 0;
 i12 = STACKTOP;
 i13 = HEAP32[i5 + 148 >> 2] | 0;
 if (!((i4 | 0) > -1)) {
  ___assert_fail(5640, 5688, 32, 5752);
 }
 if ((HEAP32[i2 + 148 >> 2] | 0) <= (i4 | 0)) {
  ___assert_fail(5640, 5688, 32, 5752);
 }
 d11 = +HEAPF32[i7 + 12 >> 2];
 d9 = +HEAPF32[i2 + (i4 << 3) + 84 >> 2];
 d1 = +HEAPF32[i7 + 8 >> 2];
 d3 = +HEAPF32[i2 + (i4 << 3) + 88 >> 2];
 d8 = d11 * d9 - d1 * d3;
 d3 = d9 * d1 + d11 * d3;
 d9 = +HEAPF32[i6 + 12 >> 2];
 d10 = +HEAPF32[i6 + 8 >> 2];
 d16 = d9 * d8 + d10 * d3;
 d14 = d9 * d3 - d8 * d10;
 if ((i13 | 0) > 0) {
  i19 = 0;
  i20 = 0;
  d15 = 3.4028234663852886e+38;
  while (1) {
   d17 = d16 * +HEAPF32[i5 + (i19 << 3) + 20 >> 2] + d14 * +HEAPF32[i5 + (i19 << 3) + 24 >> 2];
   i18 = d17 < d15;
   i20 = i18 ? i19 : i20;
   i19 = i19 + 1 | 0;
   if ((i19 | 0) == (i13 | 0)) {
    break;
   } else {
    d15 = i18 ? d17 : d15;
   }
  }
 } else {
  i20 = 0;
 }
 d16 = +HEAPF32[i2 + (i4 << 3) + 20 >> 2];
 d17 = +HEAPF32[i2 + (i4 << 3) + 24 >> 2];
 d14 = +HEAPF32[i5 + (i20 << 3) + 20 >> 2];
 d15 = +HEAPF32[i5 + (i20 << 3) + 24 >> 2];
 STACKTOP = i12;
 return +(d8 * (+HEAPF32[i6 >> 2] + (d9 * d14 - d10 * d15) - (+HEAPF32[i7 >> 2] + (d11 * d16 - d1 * d17))) + d3 * (d14 * d10 + d9 * d15 + +HEAPF32[i6 + 4 >> 2] - (d16 * d1 + d11 * d17 + +HEAPF32[i7 + 4 >> 2])));
}
function __Z4iterv() {
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, d5 = 0.0, d6 = 0.0, d7 = 0.0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 48 | 0;
 i2 = i1;
 i3 = i1 + 32 | 0;
 i4 = HEAP32[16] | 0;
 if ((i4 | 0) >= (HEAP32[4] | 0)) {
  HEAP32[16] = i4 + 1;
  __Z7measurePl(i3, HEAP32[8] | 0);
  d7 = +HEAPF32[i3 + 4 >> 2];
  d6 = +(HEAP32[10] | 0) / 1.0e6 * 1.0e3;
  d5 = +(HEAP32[12] | 0) / 1.0e6 * 1.0e3;
  HEAPF64[tempDoublePtr >> 3] = +HEAPF32[i3 >> 2];
  HEAP32[i2 >> 2] = HEAP32[tempDoublePtr >> 2];
  HEAP32[i2 + 4 >> 2] = HEAP32[tempDoublePtr + 4 >> 2];
  i4 = i2 + 8 | 0;
  HEAPF64[tempDoublePtr >> 3] = d7;
  HEAP32[i4 >> 2] = HEAP32[tempDoublePtr >> 2];
  HEAP32[i4 + 4 >> 2] = HEAP32[tempDoublePtr + 4 >> 2];
  i4 = i2 + 16 | 0;
  HEAPF64[tempDoublePtr >> 3] = d6;
  HEAP32[i4 >> 2] = HEAP32[tempDoublePtr >> 2];
  HEAP32[i4 + 4 >> 2] = HEAP32[tempDoublePtr + 4 >> 2];
  i4 = i2 + 24 | 0;
  HEAPF64[tempDoublePtr >> 3] = d5;
  HEAP32[i4 >> 2] = HEAP32[tempDoublePtr >> 2];
  HEAP32[i4 + 4 >> 2] = HEAP32[tempDoublePtr + 4 >> 2];
  _printf(96, i2 | 0) | 0;
  _emscripten_run_script(152);
  if ((HEAP32[18] | 0) == 0) {
   STACKTOP = i1;
   return;
  }
  _emscripten_cancel_main_loop();
  STACKTOP = i1;
  return;
 }
 i3 = _clock() | 0;
 __ZN7b2World4StepEfii(HEAP32[6] | 0, .01666666753590107, 3, 3);
 i3 = (_clock() | 0) - i3 | 0;
 i2 = HEAP32[16] | 0;
 HEAP32[(HEAP32[8] | 0) + (i2 << 2) >> 2] = i3;
 if ((i3 | 0) < (HEAP32[10] | 0)) {
  HEAP32[10] = i3;
 }
 if ((i3 | 0) > (HEAP32[12] | 0)) {
  HEAP32[12] = i3;
 }
 HEAP32[16] = i2 + 1;
 STACKTOP = i1;
 return;
}
function __ZN13b2DynamicTree12AllocateNodeEv(i5) {
 i5 = i5 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, i6 = 0, i7 = 0;
 i1 = STACKTOP;
 i2 = i5 + 16 | 0;
 i3 = HEAP32[i2 >> 2] | 0;
 if ((i3 | 0) == -1) {
  i4 = i5 + 8 | 0;
  i6 = HEAP32[i4 >> 2] | 0;
  i3 = i5 + 12 | 0;
  if ((i6 | 0) != (HEAP32[i3 >> 2] | 0)) {
   ___assert_fail(2912, 2944, 61, 2984);
  }
  i5 = i5 + 4 | 0;
  i7 = HEAP32[i5 >> 2] | 0;
  HEAP32[i3 >> 2] = i6 << 1;
  i6 = __Z7b2Alloci(i6 * 72 | 0) | 0;
  HEAP32[i5 >> 2] = i6;
  _memcpy(i6 | 0, i7 | 0, (HEAP32[i4 >> 2] | 0) * 36 | 0) | 0;
  __Z6b2FreePv(i7);
  i6 = HEAP32[i4 >> 2] | 0;
  i7 = (HEAP32[i3 >> 2] | 0) + -1 | 0;
  i5 = HEAP32[i5 >> 2] | 0;
  if ((i6 | 0) < (i7 | 0)) {
   i7 = i6;
   while (1) {
    i6 = i7 + 1 | 0;
    HEAP32[i5 + (i7 * 36 | 0) + 20 >> 2] = i6;
    HEAP32[i5 + (i7 * 36 | 0) + 32 >> 2] = -1;
    i7 = (HEAP32[i3 >> 2] | 0) + -1 | 0;
    if ((i6 | 0) < (i7 | 0)) {
     i7 = i6;
    } else {
     break;
    }
   }
  }
  HEAP32[i5 + (i7 * 36 | 0) + 20 >> 2] = -1;
  HEAP32[i5 + (((HEAP32[i3 >> 2] | 0) + -1 | 0) * 36 | 0) + 32 >> 2] = -1;
  i3 = HEAP32[i4 >> 2] | 0;
  HEAP32[i2 >> 2] = i3;
 } else {
  i4 = i5 + 8 | 0;
  i5 = HEAP32[i5 + 4 >> 2] | 0;
 }
 i7 = i5 + (i3 * 36 | 0) + 20 | 0;
 HEAP32[i2 >> 2] = HEAP32[i7 >> 2];
 HEAP32[i7 >> 2] = -1;
 HEAP32[i5 + (i3 * 36 | 0) + 24 >> 2] = -1;
 HEAP32[i5 + (i3 * 36 | 0) + 28 >> 2] = -1;
 HEAP32[i5 + (i3 * 36 | 0) + 32 >> 2] = 0;
 HEAP32[i5 + (i3 * 36 | 0) + 16 >> 2] = 0;
 HEAP32[i4 >> 2] = (HEAP32[i4 >> 2] | 0) + 1;
 STACKTOP = i1;
 return i3 | 0;
}
function __ZN9b2Fixture6CreateEP16b2BlockAllocatorP6b2BodyPK12b2FixtureDef(i1, i5, i4, i3) {
 i1 = i1 | 0;
 i5 = i5 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 var i2 = 0, i6 = 0, i7 = 0, d8 = 0.0;
 i2 = STACKTOP;
 HEAP32[i1 + 40 >> 2] = HEAP32[i3 + 4 >> 2];
 HEAPF32[i1 + 16 >> 2] = +HEAPF32[i3 + 8 >> 2];
 HEAPF32[i1 + 20 >> 2] = +HEAPF32[i3 + 12 >> 2];
 HEAP32[i1 + 8 >> 2] = i4;
 HEAP32[i1 + 4 >> 2] = 0;
 i4 = i1 + 32 | 0;
 i6 = i3 + 22 | 0;
 HEAP16[i4 + 0 >> 1] = HEAP16[i6 + 0 >> 1] | 0;
 HEAP16[i4 + 2 >> 1] = HEAP16[i6 + 2 >> 1] | 0;
 HEAP16[i4 + 4 >> 1] = HEAP16[i6 + 4 >> 1] | 0;
 HEAP8[i1 + 38 | 0] = HEAP8[i3 + 20 | 0] | 0;
 i4 = HEAP32[i3 >> 2] | 0;
 i4 = FUNCTION_TABLE_iii[HEAP32[(HEAP32[i4 >> 2] | 0) + 8 >> 2] & 3](i4, i5) | 0;
 HEAP32[i1 + 12 >> 2] = i4;
 i4 = FUNCTION_TABLE_ii[HEAP32[(HEAP32[i4 >> 2] | 0) + 12 >> 2] & 3](i4) | 0;
 i6 = __ZN16b2BlockAllocator8AllocateEi(i5, i4 * 28 | 0) | 0;
 i5 = i1 + 24 | 0;
 HEAP32[i5 >> 2] = i6;
 if ((i4 | 0) > 0) {
  i7 = 0;
 } else {
  i7 = i1 + 28 | 0;
  HEAP32[i7 >> 2] = 0;
  i7 = i3 + 16 | 0;
  d8 = +HEAPF32[i7 >> 2];
  HEAPF32[i1 >> 2] = d8;
  STACKTOP = i2;
  return;
 }
 do {
  HEAP32[i6 + (i7 * 28 | 0) + 16 >> 2] = 0;
  i6 = HEAP32[i5 >> 2] | 0;
  HEAP32[i6 + (i7 * 28 | 0) + 24 >> 2] = -1;
  i7 = i7 + 1 | 0;
 } while ((i7 | 0) != (i4 | 0));
 i7 = i1 + 28 | 0;
 HEAP32[i7 >> 2] = 0;
 i7 = i3 + 16 | 0;
 d8 = +HEAPF32[i7 >> 2];
 HEAPF32[i1 >> 2] = d8;
 STACKTOP = i2;
 return;
}
function __Z19b2ClipSegmentToLineP12b2ClipVertexPKS_RK6b2Vec2fi(i4, i1, i5, d9, i2) {
 i4 = i4 | 0;
 i1 = i1 | 0;
 i5 = i5 | 0;
 d9 = +d9;
 i2 = i2 | 0;
 var i3 = 0, i6 = 0, d7 = 0.0, i8 = 0, i10 = 0, d11 = 0.0, d12 = 0.0, i13 = 0;
 i3 = STACKTOP;
 d12 = +HEAPF32[i5 >> 2];
 d11 = +HEAPF32[i5 + 4 >> 2];
 i5 = i1 + 4 | 0;
 d7 = d12 * +HEAPF32[i1 >> 2] + d11 * +HEAPF32[i5 >> 2] - d9;
 i6 = i1 + 12 | 0;
 i8 = i1 + 16 | 0;
 d9 = d12 * +HEAPF32[i6 >> 2] + d11 * +HEAPF32[i8 >> 2] - d9;
 if (!(d7 <= 0.0)) {
  i10 = 0;
 } else {
  HEAP32[i4 + 0 >> 2] = HEAP32[i1 + 0 >> 2];
  HEAP32[i4 + 4 >> 2] = HEAP32[i1 + 4 >> 2];
  HEAP32[i4 + 8 >> 2] = HEAP32[i1 + 8 >> 2];
  i10 = 1;
 }
 if (d9 <= 0.0) {
  i13 = i10 + 1 | 0;
  i10 = i4 + (i10 * 12 | 0) | 0;
  HEAP32[i10 + 0 >> 2] = HEAP32[i6 + 0 >> 2];
  HEAP32[i10 + 4 >> 2] = HEAP32[i6 + 4 >> 2];
  HEAP32[i10 + 8 >> 2] = HEAP32[i6 + 8 >> 2];
  i10 = i13;
 }
 if (!(d7 * d9 < 0.0)) {
  i13 = i10;
  STACKTOP = i3;
  return i13 | 0;
 }
 d9 = d7 / (d7 - d9);
 d11 = +HEAPF32[i1 >> 2];
 d12 = +HEAPF32[i5 >> 2];
 d11 = +(d11 + d9 * (+HEAPF32[i6 >> 2] - d11));
 d12 = +(d12 + d9 * (+HEAPF32[i8 >> 2] - d12));
 i13 = i4 + (i10 * 12 | 0) | 0;
 HEAPF32[i13 >> 2] = d11;
 HEAPF32[i13 + 4 >> 2] = d12;
 i13 = i4 + (i10 * 12 | 0) + 8 | 0;
 HEAP8[i13] = i2;
 HEAP8[i13 + 1 | 0] = HEAP8[i1 + 9 | 0] | 0;
 HEAP8[i13 + 2 | 0] = 0;
 HEAP8[i13 + 3 | 0] = 1;
 i13 = i10 + 1 | 0;
 STACKTOP = i3;
 return i13 | 0;
}
function __Z16b2CollideCirclesP10b2ManifoldPK13b2CircleShapeRK11b2TransformS3_S6_(i1, i7, i8, i6, i9) {
 i1 = i1 | 0;
 i7 = i7 | 0;
 i8 = i8 | 0;
 i6 = i6 | 0;
 i9 = i9 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, d10 = 0.0, d11 = 0.0, d12 = 0.0, d13 = 0.0, d14 = 0.0, d15 = 0.0, d16 = 0.0, d17 = 0.0, d18 = 0.0;
 i2 = STACKTOP;
 i4 = i1 + 60 | 0;
 HEAP32[i4 >> 2] = 0;
 i3 = i7 + 12 | 0;
 d10 = +HEAPF32[i8 + 12 >> 2];
 d14 = +HEAPF32[i3 >> 2];
 d13 = +HEAPF32[i8 + 8 >> 2];
 d11 = +HEAPF32[i7 + 16 >> 2];
 i5 = i6 + 12 | 0;
 d16 = +HEAPF32[i9 + 12 >> 2];
 d18 = +HEAPF32[i5 >> 2];
 d17 = +HEAPF32[i9 + 8 >> 2];
 d15 = +HEAPF32[i6 + 16 >> 2];
 d12 = +HEAPF32[i9 >> 2] + (d16 * d18 - d17 * d15) - (+HEAPF32[i8 >> 2] + (d10 * d14 - d13 * d11));
 d11 = d18 * d17 + d16 * d15 + +HEAPF32[i9 + 4 >> 2] - (d14 * d13 + d10 * d11 + +HEAPF32[i8 + 4 >> 2]);
 d10 = +HEAPF32[i7 + 8 >> 2] + +HEAPF32[i6 + 8 >> 2];
 if (d12 * d12 + d11 * d11 > d10 * d10) {
  STACKTOP = i2;
  return;
 }
 HEAP32[i1 + 56 >> 2] = 0;
 i9 = i3;
 i8 = HEAP32[i9 + 4 >> 2] | 0;
 i7 = i1 + 48 | 0;
 HEAP32[i7 >> 2] = HEAP32[i9 >> 2];
 HEAP32[i7 + 4 >> 2] = i8;
 HEAPF32[i1 + 40 >> 2] = 0.0;
 HEAPF32[i1 + 44 >> 2] = 0.0;
 HEAP32[i4 >> 2] = 1;
 i7 = i5;
 i8 = HEAP32[i7 + 4 >> 2] | 0;
 i9 = i1;
 HEAP32[i9 >> 2] = HEAP32[i7 >> 2];
 HEAP32[i9 + 4 >> 2] = i8;
 HEAP32[i1 + 16 >> 2] = 0;
 STACKTOP = i2;
 return;
}
function __ZNK14b2PolygonShape11ComputeAABBEP6b2AABBRK11b2Transformi(i1, i2, i7, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i7 = i7 | 0;
 i3 = i3 | 0;
 var d4 = 0.0, d5 = 0.0, d6 = 0.0, d8 = 0.0, d9 = 0.0, d10 = 0.0, d11 = 0.0, d12 = 0.0, i13 = 0, d14 = 0.0, d15 = 0.0, d16 = 0.0;
 i3 = STACKTOP;
 d4 = +HEAPF32[i7 + 12 >> 2];
 d15 = +HEAPF32[i1 + 20 >> 2];
 d5 = +HEAPF32[i7 + 8 >> 2];
 d12 = +HEAPF32[i1 + 24 >> 2];
 d6 = +HEAPF32[i7 >> 2];
 d9 = d6 + (d4 * d15 - d5 * d12);
 d8 = +HEAPF32[i7 + 4 >> 2];
 d12 = d15 * d5 + d4 * d12 + d8;
 i7 = HEAP32[i1 + 148 >> 2] | 0;
 if ((i7 | 0) > 1) {
  d10 = d9;
  d11 = d12;
  i13 = 1;
  do {
   d16 = +HEAPF32[i1 + (i13 << 3) + 20 >> 2];
   d14 = +HEAPF32[i1 + (i13 << 3) + 24 >> 2];
   d15 = d6 + (d4 * d16 - d5 * d14);
   d14 = d16 * d5 + d4 * d14 + d8;
   d10 = d10 < d15 ? d10 : d15;
   d11 = d11 < d14 ? d11 : d14;
   d9 = d9 > d15 ? d9 : d15;
   d12 = d12 > d14 ? d12 : d14;
   i13 = i13 + 1 | 0;
  } while ((i13 | 0) < (i7 | 0));
 } else {
  d11 = d12;
  d10 = d9;
 }
 d16 = +HEAPF32[i1 + 8 >> 2];
 d14 = +(d10 - d16);
 d15 = +(d11 - d16);
 i13 = i2;
 HEAPF32[i13 >> 2] = d14;
 HEAPF32[i13 + 4 >> 2] = d15;
 d15 = +(d9 + d16);
 d16 = +(d12 + d16);
 i13 = i2 + 8 | 0;
 HEAPF32[i13 >> 2] = d15;
 HEAPF32[i13 + 4 >> 2] = d16;
 STACKTOP = i3;
 return;
}
function __ZNK10__cxxabiv120__si_class_type_info16search_above_dstEPNS_19__dynamic_cast_infoEPKvS4_ib(i5, i1, i4, i6, i3, i7) {
 i5 = i5 | 0;
 i1 = i1 | 0;
 i4 = i4 | 0;
 i6 = i6 | 0;
 i3 = i3 | 0;
 i7 = i7 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 if ((i5 | 0) != (HEAP32[i1 + 8 >> 2] | 0)) {
  i5 = HEAP32[i5 + 8 >> 2] | 0;
  FUNCTION_TABLE_viiiiii[HEAP32[(HEAP32[i5 >> 2] | 0) + 20 >> 2] & 3](i5, i1, i4, i6, i3, i7);
  STACKTOP = i2;
  return;
 }
 HEAP8[i1 + 53 | 0] = 1;
 if ((HEAP32[i1 + 4 >> 2] | 0) != (i6 | 0)) {
  STACKTOP = i2;
  return;
 }
 HEAP8[i1 + 52 | 0] = 1;
 i5 = i1 + 16 | 0;
 i6 = HEAP32[i5 >> 2] | 0;
 if ((i6 | 0) == 0) {
  HEAP32[i5 >> 2] = i4;
  HEAP32[i1 + 24 >> 2] = i3;
  HEAP32[i1 + 36 >> 2] = 1;
  if (!((HEAP32[i1 + 48 >> 2] | 0) == 1 & (i3 | 0) == 1)) {
   STACKTOP = i2;
   return;
  }
  HEAP8[i1 + 54 | 0] = 1;
  STACKTOP = i2;
  return;
 }
 if ((i6 | 0) != (i4 | 0)) {
  i7 = i1 + 36 | 0;
  HEAP32[i7 >> 2] = (HEAP32[i7 >> 2] | 0) + 1;
  HEAP8[i1 + 54 | 0] = 1;
  STACKTOP = i2;
  return;
 }
 i4 = i1 + 24 | 0;
 i5 = HEAP32[i4 >> 2] | 0;
 if ((i5 | 0) == 2) {
  HEAP32[i4 >> 2] = i3;
 } else {
  i3 = i5;
 }
 if (!((HEAP32[i1 + 48 >> 2] | 0) == 1 & (i3 | 0) == 1)) {
  STACKTOP = i2;
  return;
 }
 HEAP8[i1 + 54 | 0] = 1;
 STACKTOP = i2;
 return;
}
function __ZN6b2Body13CreateFixtureEPK12b2FixtureDef(i1, i5) {
 i1 = i1 | 0;
 i5 = i5 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i6 = 0;
 i3 = STACKTOP;
 i2 = i1 + 88 | 0;
 i4 = HEAP32[i2 >> 2] | 0;
 if ((HEAP32[i4 + 102868 >> 2] & 2 | 0) != 0) {
  ___assert_fail(1776, 1520, 153, 1808);
 }
 i6 = __ZN16b2BlockAllocator8AllocateEi(i4, 44) | 0;
 if ((i6 | 0) == 0) {
  i6 = 0;
 } else {
  __ZN9b2FixtureC2Ev(i6);
 }
 __ZN9b2Fixture6CreateEP16b2BlockAllocatorP6b2BodyPK12b2FixtureDef(i6, i4, i1, i5);
 if (!((HEAP16[i1 + 4 >> 1] & 32) == 0)) {
  __ZN9b2Fixture13CreateProxiesEP12b2BroadPhaseRK11b2Transform(i6, (HEAP32[i2 >> 2] | 0) + 102872 | 0, i1 + 12 | 0);
 }
 i5 = i1 + 100 | 0;
 HEAP32[i6 + 4 >> 2] = HEAP32[i5 >> 2];
 HEAP32[i5 >> 2] = i6;
 i5 = i1 + 104 | 0;
 HEAP32[i5 >> 2] = (HEAP32[i5 >> 2] | 0) + 1;
 HEAP32[i6 + 8 >> 2] = i1;
 if (!(+HEAPF32[i6 >> 2] > 0.0)) {
  i5 = HEAP32[i2 >> 2] | 0;
  i5 = i5 + 102868 | 0;
  i4 = HEAP32[i5 >> 2] | 0;
  i4 = i4 | 1;
  HEAP32[i5 >> 2] = i4;
  STACKTOP = i3;
  return i6 | 0;
 }
 __ZN6b2Body13ResetMassDataEv(i1);
 i5 = HEAP32[i2 >> 2] | 0;
 i5 = i5 + 102868 | 0;
 i4 = HEAP32[i5 >> 2] | 0;
 i4 = i4 | 1;
 HEAP32[i5 >> 2] = i4;
 STACKTOP = i3;
 return i6 | 0;
}
function __Z13b2TestOverlapPK7b2ShapeiS1_iRK11b2TransformS4_(i6, i5, i4, i3, i2, i1) {
 i6 = i6 | 0;
 i5 = i5 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 var i7 = 0, i8 = 0, i9 = 0, i10 = 0;
 i8 = STACKTOP;
 STACKTOP = STACKTOP + 128 | 0;
 i9 = i8 + 36 | 0;
 i10 = i8 + 24 | 0;
 i7 = i8;
 HEAP32[i9 + 16 >> 2] = 0;
 HEAP32[i9 + 20 >> 2] = 0;
 HEAPF32[i9 + 24 >> 2] = 0.0;
 HEAP32[i9 + 44 >> 2] = 0;
 HEAP32[i9 + 48 >> 2] = 0;
 HEAPF32[i9 + 52 >> 2] = 0.0;
 __ZN15b2DistanceProxy3SetEPK7b2Shapei(i9, i6, i5);
 __ZN15b2DistanceProxy3SetEPK7b2Shapei(i9 + 28 | 0, i4, i3);
 i6 = i9 + 56 | 0;
 HEAP32[i6 + 0 >> 2] = HEAP32[i2 + 0 >> 2];
 HEAP32[i6 + 4 >> 2] = HEAP32[i2 + 4 >> 2];
 HEAP32[i6 + 8 >> 2] = HEAP32[i2 + 8 >> 2];
 HEAP32[i6 + 12 >> 2] = HEAP32[i2 + 12 >> 2];
 i6 = i9 + 72 | 0;
 HEAP32[i6 + 0 >> 2] = HEAP32[i1 + 0 >> 2];
 HEAP32[i6 + 4 >> 2] = HEAP32[i1 + 4 >> 2];
 HEAP32[i6 + 8 >> 2] = HEAP32[i1 + 8 >> 2];
 HEAP32[i6 + 12 >> 2] = HEAP32[i1 + 12 >> 2];
 HEAP8[i9 + 88 | 0] = 1;
 HEAP16[i10 + 4 >> 1] = 0;
 __Z10b2DistanceP16b2DistanceOutputP14b2SimplexCachePK15b2DistanceInput(i7, i10, i9);
 STACKTOP = i8;
 return +HEAPF32[i7 + 16 >> 2] < 11920928955078125.0e-22 | 0;
}
function __ZNK10__cxxabiv117__class_type_info16search_above_dstEPNS_19__dynamic_cast_infoEPKvS4_ib(i6, i1, i4, i5, i2, i3) {
 i6 = i6 | 0;
 i1 = i1 | 0;
 i4 = i4 | 0;
 i5 = i5 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i3 = STACKTOP;
 if ((HEAP32[i1 + 8 >> 2] | 0) != (i6 | 0)) {
  STACKTOP = i3;
  return;
 }
 HEAP8[i1 + 53 | 0] = 1;
 if ((HEAP32[i1 + 4 >> 2] | 0) != (i5 | 0)) {
  STACKTOP = i3;
  return;
 }
 HEAP8[i1 + 52 | 0] = 1;
 i5 = i1 + 16 | 0;
 i6 = HEAP32[i5 >> 2] | 0;
 if ((i6 | 0) == 0) {
  HEAP32[i5 >> 2] = i4;
  HEAP32[i1 + 24 >> 2] = i2;
  HEAP32[i1 + 36 >> 2] = 1;
  if (!((HEAP32[i1 + 48 >> 2] | 0) == 1 & (i2 | 0) == 1)) {
   STACKTOP = i3;
   return;
  }
  HEAP8[i1 + 54 | 0] = 1;
  STACKTOP = i3;
  return;
 }
 if ((i6 | 0) != (i4 | 0)) {
  i6 = i1 + 36 | 0;
  HEAP32[i6 >> 2] = (HEAP32[i6 >> 2] | 0) + 1;
  HEAP8[i1 + 54 | 0] = 1;
  STACKTOP = i3;
  return;
 }
 i4 = i1 + 24 | 0;
 i5 = HEAP32[i4 >> 2] | 0;
 if ((i5 | 0) == 2) {
  HEAP32[i4 >> 2] = i2;
 } else {
  i2 = i5;
 }
 if (!((HEAP32[i1 + 48 >> 2] | 0) == 1 & (i2 | 0) == 1)) {
  STACKTOP = i3;
  return;
 }
 HEAP8[i1 + 54 | 0] = 1;
 STACKTOP = i3;
 return;
}
function __ZNK11b2EdgeShape5CloneEP16b2BlockAllocator(i1, i3) {
 i1 = i1 | 0;
 i3 = i3 | 0;
 var i2 = 0, i4 = 0, i5 = 0, i6 = 0;
 i2 = STACKTOP;
 i3 = __ZN16b2BlockAllocator8AllocateEi(i3, 48) | 0;
 if ((i3 | 0) == 0) {
  i3 = 0;
 } else {
  HEAP32[i3 >> 2] = 240;
  HEAP32[i3 + 4 >> 2] = 1;
  HEAPF32[i3 + 8 >> 2] = .009999999776482582;
  i4 = i3 + 28 | 0;
  HEAP32[i4 + 0 >> 2] = 0;
  HEAP32[i4 + 4 >> 2] = 0;
  HEAP32[i4 + 8 >> 2] = 0;
  HEAP32[i4 + 12 >> 2] = 0;
  HEAP16[i4 + 16 >> 1] = 0;
 }
 i6 = i1 + 4 | 0;
 i5 = HEAP32[i6 + 4 >> 2] | 0;
 i4 = i3 + 4 | 0;
 HEAP32[i4 >> 2] = HEAP32[i6 >> 2];
 HEAP32[i4 + 4 >> 2] = i5;
 i4 = i3 + 12 | 0;
 i1 = i1 + 12 | 0;
 HEAP32[i4 + 0 >> 2] = HEAP32[i1 + 0 >> 2];
 HEAP32[i4 + 4 >> 2] = HEAP32[i1 + 4 >> 2];
 HEAP32[i4 + 8 >> 2] = HEAP32[i1 + 8 >> 2];
 HEAP32[i4 + 12 >> 2] = HEAP32[i1 + 12 >> 2];
 HEAP32[i4 + 16 >> 2] = HEAP32[i1 + 16 >> 2];
 HEAP32[i4 + 20 >> 2] = HEAP32[i1 + 20 >> 2];
 HEAP32[i4 + 24 >> 2] = HEAP32[i1 + 24 >> 2];
 HEAP32[i4 + 28 >> 2] = HEAP32[i1 + 28 >> 2];
 HEAP16[i4 + 32 >> 1] = HEAP16[i1 + 32 >> 1] | 0;
 STACKTOP = i2;
 return i3 | 0;
}
function __ZN7b2WorldC2ERK6b2Vec2(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0;
 i3 = STACKTOP;
 __ZN16b2BlockAllocatorC2Ev(i1);
 __ZN16b2StackAllocatorC2Ev(i1 + 68 | 0);
 __ZN16b2ContactManagerC2Ev(i1 + 102872 | 0);
 i6 = i1 + 102968 | 0;
 HEAP32[i1 + 102980 >> 2] = 0;
 HEAP32[i1 + 102984 >> 2] = 0;
 i4 = i1 + 102952 | 0;
 i5 = i1 + 102992 | 0;
 HEAP32[i4 + 0 >> 2] = 0;
 HEAP32[i4 + 4 >> 2] = 0;
 HEAP32[i4 + 8 >> 2] = 0;
 HEAP32[i4 + 12 >> 2] = 0;
 HEAP8[i5] = 1;
 HEAP8[i1 + 102993 | 0] = 1;
 HEAP8[i1 + 102994 | 0] = 0;
 HEAP8[i1 + 102995 | 0] = 1;
 HEAP8[i1 + 102976 | 0] = 1;
 i5 = i2;
 i4 = HEAP32[i5 + 4 >> 2] | 0;
 i2 = i6;
 HEAP32[i2 >> 2] = HEAP32[i5 >> 2];
 HEAP32[i2 + 4 >> 2] = i4;
 HEAP32[i1 + 102868 >> 2] = 4;
 HEAPF32[i1 + 102988 >> 2] = 0.0;
 HEAP32[i1 + 102948 >> 2] = i1;
 i2 = i1 + 102996 | 0;
 HEAP32[i2 + 0 >> 2] = 0;
 HEAP32[i2 + 4 >> 2] = 0;
 HEAP32[i2 + 8 >> 2] = 0;
 HEAP32[i2 + 12 >> 2] = 0;
 HEAP32[i2 + 16 >> 2] = 0;
 HEAP32[i2 + 20 >> 2] = 0;
 HEAP32[i2 + 24 >> 2] = 0;
 HEAP32[i2 + 28 >> 2] = 0;
 STACKTOP = i3;
 return;
}
function __ZNK10__cxxabiv117__class_type_info16search_below_dstEPNS_19__dynamic_cast_infoEPKvib(i6, i3, i4, i1, i2) {
 i6 = i6 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i5 = 0;
 i2 = STACKTOP;
 if ((HEAP32[i3 + 8 >> 2] | 0) == (i6 | 0)) {
  if ((HEAP32[i3 + 4 >> 2] | 0) != (i4 | 0)) {
   STACKTOP = i2;
   return;
  }
  i3 = i3 + 28 | 0;
  if ((HEAP32[i3 >> 2] | 0) == 1) {
   STACKTOP = i2;
   return;
  }
  HEAP32[i3 >> 2] = i1;
  STACKTOP = i2;
  return;
 }
 if ((HEAP32[i3 >> 2] | 0) != (i6 | 0)) {
  STACKTOP = i2;
  return;
 }
 if ((HEAP32[i3 + 16 >> 2] | 0) != (i4 | 0) ? (i5 = i3 + 20 | 0, (HEAP32[i5 >> 2] | 0) != (i4 | 0)) : 0) {
  HEAP32[i3 + 32 >> 2] = i1;
  HEAP32[i5 >> 2] = i4;
  i6 = i3 + 40 | 0;
  HEAP32[i6 >> 2] = (HEAP32[i6 >> 2] | 0) + 1;
  if ((HEAP32[i3 + 36 >> 2] | 0) == 1 ? (HEAP32[i3 + 24 >> 2] | 0) == 2 : 0) {
   HEAP8[i3 + 54 | 0] = 1;
  }
  HEAP32[i3 + 44 >> 2] = 4;
  STACKTOP = i2;
  return;
 }
 if ((i1 | 0) != 1) {
  STACKTOP = i2;
  return;
 }
 HEAP32[i3 + 32 >> 2] = 1;
 STACKTOP = i2;
 return;
}
function __ZN9b2Contact7DestroyEPS_P16b2BlockAllocator(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0;
 i3 = STACKTOP;
 if ((HEAP8[4200] | 0) == 0) {
  ___assert_fail(4352, 4256, 103, 4376);
 }
 i4 = HEAP32[i1 + 48 >> 2] | 0;
 if ((HEAP32[i1 + 124 >> 2] | 0) > 0) {
  i7 = HEAP32[i4 + 8 >> 2] | 0;
  i6 = i7 + 4 | 0;
  i5 = HEAPU16[i6 >> 1] | 0;
  if ((i5 & 2 | 0) == 0) {
   HEAP16[i6 >> 1] = i5 | 2;
   HEAPF32[i7 + 144 >> 2] = 0.0;
  }
  i7 = HEAP32[i1 + 52 >> 2] | 0;
  i6 = HEAP32[i7 + 8 >> 2] | 0;
  i5 = i6 + 4 | 0;
  i8 = HEAPU16[i5 >> 1] | 0;
  if ((i8 & 2 | 0) == 0) {
   HEAP16[i5 >> 1] = i8 | 2;
   HEAPF32[i6 + 144 >> 2] = 0.0;
  }
 } else {
  i7 = HEAP32[i1 + 52 >> 2] | 0;
 }
 i4 = HEAP32[(HEAP32[i4 + 12 >> 2] | 0) + 4 >> 2] | 0;
 i5 = HEAP32[(HEAP32[i7 + 12 >> 2] | 0) + 4 >> 2] | 0;
 if ((i4 | 0) > -1 & (i5 | 0) < 4) {
  FUNCTION_TABLE_vii[HEAP32[4008 + (i4 * 48 | 0) + (i5 * 12 | 0) + 4 >> 2] & 15](i1, i2);
  STACKTOP = i3;
  return;
 } else {
  ___assert_fail(4384, 4256, 114, 4376);
 }
}
function __ZN9b2Fixture13CreateProxiesEP12b2BroadPhaseRK11b2Transform(i5, i4, i1) {
 i5 = i5 | 0;
 i4 = i4 | 0;
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0;
 i2 = STACKTOP;
 i3 = i5 + 28 | 0;
 if ((HEAP32[i3 >> 2] | 0) != 0) {
  ___assert_fail(2088, 2112, 124, 2144);
 }
 i6 = i5 + 12 | 0;
 i8 = HEAP32[i6 >> 2] | 0;
 i8 = FUNCTION_TABLE_ii[HEAP32[(HEAP32[i8 >> 2] | 0) + 12 >> 2] & 3](i8) | 0;
 HEAP32[i3 >> 2] = i8;
 if ((i8 | 0) <= 0) {
  STACKTOP = i2;
  return;
 }
 i7 = i5 + 24 | 0;
 i8 = 0;
 do {
  i9 = HEAP32[i7 >> 2] | 0;
  i10 = i9 + (i8 * 28 | 0) | 0;
  i11 = HEAP32[i6 >> 2] | 0;
  FUNCTION_TABLE_viiii[HEAP32[(HEAP32[i11 >> 2] | 0) + 24 >> 2] & 15](i11, i10, i1, i8);
  HEAP32[i9 + (i8 * 28 | 0) + 24 >> 2] = __ZN12b2BroadPhase11CreateProxyERK6b2AABBPv(i4, i10, i10) | 0;
  HEAP32[i9 + (i8 * 28 | 0) + 16 >> 2] = i5;
  HEAP32[i9 + (i8 * 28 | 0) + 20 >> 2] = i8;
  i8 = i8 + 1 | 0;
 } while ((i8 | 0) < (HEAP32[i3 >> 2] | 0));
 STACKTOP = i2;
 return;
}
function __ZNK10__cxxabiv117__class_type_info9can_catchEPKNS_16__shim_type_infoERPv(i1, i5, i4) {
 i1 = i1 | 0;
 i5 = i5 | 0;
 i4 = i4 | 0;
 var i2 = 0, i3 = 0, i6 = 0, i7 = 0;
 i2 = STACKTOP;
 STACKTOP = STACKTOP + 64 | 0;
 i3 = i2;
 if ((i1 | 0) == (i5 | 0)) {
  i7 = 1;
  STACKTOP = i2;
  return i7 | 0;
 }
 if ((i5 | 0) == 0) {
  i7 = 0;
  STACKTOP = i2;
  return i7 | 0;
 }
 i5 = ___dynamic_cast(i5, 6952, 7008, 0) | 0;
 if ((i5 | 0) == 0) {
  i7 = 0;
  STACKTOP = i2;
  return i7 | 0;
 }
 i7 = i3 + 0 | 0;
 i6 = i7 + 56 | 0;
 do {
  HEAP32[i7 >> 2] = 0;
  i7 = i7 + 4 | 0;
 } while ((i7 | 0) < (i6 | 0));
 HEAP32[i3 >> 2] = i5;
 HEAP32[i3 + 8 >> 2] = i1;
 HEAP32[i3 + 12 >> 2] = -1;
 HEAP32[i3 + 48 >> 2] = 1;
 FUNCTION_TABLE_viiii[HEAP32[(HEAP32[i5 >> 2] | 0) + 28 >> 2] & 15](i5, i3, HEAP32[i4 >> 2] | 0, 1);
 if ((HEAP32[i3 + 24 >> 2] | 0) != 1) {
  i7 = 0;
  STACKTOP = i2;
  return i7 | 0;
 }
 HEAP32[i4 >> 2] = HEAP32[i3 + 16 >> 2];
 i7 = 1;
 STACKTOP = i2;
 return i7 | 0;
}
function __ZN8b2IslandC2EiiiP16b2StackAllocatorP17b2ContactListener(i1, i4, i3, i2, i5, i6) {
 i1 = i1 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 i5 = i5 | 0;
 i6 = i6 | 0;
 var i7 = 0, i8 = 0;
 i7 = STACKTOP;
 i8 = i1 + 40 | 0;
 HEAP32[i8 >> 2] = i4;
 HEAP32[i1 + 44 >> 2] = i3;
 HEAP32[i1 + 48 >> 2] = i2;
 HEAP32[i1 + 28 >> 2] = 0;
 HEAP32[i1 + 36 >> 2] = 0;
 HEAP32[i1 + 32 >> 2] = 0;
 HEAP32[i1 >> 2] = i5;
 HEAP32[i1 + 4 >> 2] = i6;
 HEAP32[i1 + 8 >> 2] = __ZN16b2StackAllocator8AllocateEi(i5, i4 << 2) | 0;
 HEAP32[i1 + 12 >> 2] = __ZN16b2StackAllocator8AllocateEi(HEAP32[i1 >> 2] | 0, i3 << 2) | 0;
 HEAP32[i1 + 16 >> 2] = __ZN16b2StackAllocator8AllocateEi(HEAP32[i1 >> 2] | 0, i2 << 2) | 0;
 HEAP32[i1 + 24 >> 2] = __ZN16b2StackAllocator8AllocateEi(HEAP32[i1 >> 2] | 0, (HEAP32[i8 >> 2] | 0) * 12 | 0) | 0;
 HEAP32[i1 + 20 >> 2] = __ZN16b2StackAllocator8AllocateEi(HEAP32[i1 >> 2] | 0, (HEAP32[i8 >> 2] | 0) * 12 | 0) | 0;
 STACKTOP = i7;
 return;
}
function __ZNK11b2EdgeShape11ComputeAABBEP6b2AABBRK11b2Transformi(i8, i1, i10, i2) {
 i8 = i8 | 0;
 i1 = i1 | 0;
 i10 = i10 | 0;
 i2 = i2 | 0;
 var d3 = 0.0, d4 = 0.0, d5 = 0.0, d6 = 0.0, d7 = 0.0, d9 = 0.0, d11 = 0.0, d12 = 0.0;
 i2 = STACKTOP;
 d7 = +HEAPF32[i10 + 12 >> 2];
 d9 = +HEAPF32[i8 + 12 >> 2];
 d11 = +HEAPF32[i10 + 8 >> 2];
 d3 = +HEAPF32[i8 + 16 >> 2];
 d6 = +HEAPF32[i10 >> 2];
 d5 = d6 + (d7 * d9 - d11 * d3);
 d12 = +HEAPF32[i10 + 4 >> 2];
 d3 = d9 * d11 + d7 * d3 + d12;
 d9 = +HEAPF32[i8 + 20 >> 2];
 d4 = +HEAPF32[i8 + 24 >> 2];
 d6 = d6 + (d7 * d9 - d11 * d4);
 d4 = d12 + (d11 * d9 + d7 * d4);
 d7 = +HEAPF32[i8 + 8 >> 2];
 d9 = +((d5 < d6 ? d5 : d6) - d7);
 d12 = +((d3 < d4 ? d3 : d4) - d7);
 i10 = i1;
 HEAPF32[i10 >> 2] = d9;
 HEAPF32[i10 + 4 >> 2] = d12;
 d5 = +(d7 + (d5 > d6 ? d5 : d6));
 d12 = +(d7 + (d3 > d4 ? d3 : d4));
 i10 = i1 + 8 | 0;
 HEAPF32[i10 >> 2] = d5;
 HEAPF32[i10 + 4 >> 2] = d12;
 STACKTOP = i2;
 return;
}
function __ZNK14b2PolygonShape9TestPointERK11b2TransformRK6b2Vec2(i2, i3, i6) {
 i2 = i2 | 0;
 i3 = i3 | 0;
 i6 = i6 | 0;
 var i1 = 0, d4 = 0.0, d5 = 0.0, i7 = 0, d8 = 0.0, d9 = 0.0, d10 = 0.0;
 i1 = STACKTOP;
 d8 = +HEAPF32[i6 >> 2] - +HEAPF32[i3 >> 2];
 d9 = +HEAPF32[i6 + 4 >> 2] - +HEAPF32[i3 + 4 >> 2];
 d10 = +HEAPF32[i3 + 12 >> 2];
 d5 = +HEAPF32[i3 + 8 >> 2];
 d4 = d8 * d10 + d9 * d5;
 d5 = d10 * d9 - d8 * d5;
 i3 = HEAP32[i2 + 148 >> 2] | 0;
 if ((i3 | 0) > 0) {
  i6 = 0;
 } else {
  i7 = 1;
  STACKTOP = i1;
  return i7 | 0;
 }
 while (1) {
  i7 = i6 + 1 | 0;
  if ((d4 - +HEAPF32[i2 + (i6 << 3) + 20 >> 2]) * +HEAPF32[i2 + (i6 << 3) + 84 >> 2] + (d5 - +HEAPF32[i2 + (i6 << 3) + 24 >> 2]) * +HEAPF32[i2 + (i6 << 3) + 88 >> 2] > 0.0) {
   i3 = 0;
   i2 = 4;
   break;
  }
  if ((i7 | 0) < (i3 | 0)) {
   i6 = i7;
  } else {
   i3 = 1;
   i2 = 4;
   break;
  }
 }
 if ((i2 | 0) == 4) {
  STACKTOP = i1;
  return i3 | 0;
 }
 return 0;
}
function __ZN16b2StackAllocator8AllocateEi(i4, i5) {
 i4 = i4 | 0;
 i5 = i5 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i6 = 0, i7 = 0, i8 = 0;
 i2 = STACKTOP;
 i3 = i4 + 102796 | 0;
 i6 = HEAP32[i3 >> 2] | 0;
 if ((i6 | 0) >= 32) {
  ___assert_fail(3896, 3808, 38, 3936);
 }
 i1 = i4 + (i6 * 12 | 0) + 102412 | 0;
 HEAP32[i4 + (i6 * 12 | 0) + 102416 >> 2] = i5;
 i7 = i4 + 102400 | 0;
 i8 = HEAP32[i7 >> 2] | 0;
 if ((i8 + i5 | 0) > 102400) {
  HEAP32[i1 >> 2] = __Z7b2Alloci(i5) | 0;
  HEAP8[i4 + (i6 * 12 | 0) + 102420 | 0] = 1;
 } else {
  HEAP32[i1 >> 2] = i4 + i8;
  HEAP8[i4 + (i6 * 12 | 0) + 102420 | 0] = 0;
  HEAP32[i7 >> 2] = (HEAP32[i7 >> 2] | 0) + i5;
 }
 i6 = i4 + 102404 | 0;
 i5 = (HEAP32[i6 >> 2] | 0) + i5 | 0;
 HEAP32[i6 >> 2] = i5;
 i4 = i4 + 102408 | 0;
 i6 = HEAP32[i4 >> 2] | 0;
 HEAP32[i4 >> 2] = (i6 | 0) > (i5 | 0) ? i6 : i5;
 HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) + 1;
 STACKTOP = i2;
 return HEAP32[i1 >> 2] | 0;
}
function __ZN12b2BroadPhase13QueryCallbackEi(i5, i1) {
 i5 = i5 | 0;
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i6 = 0, i7 = 0, i8 = 0;
 i2 = STACKTOP;
 i4 = i5 + 56 | 0;
 i7 = HEAP32[i4 >> 2] | 0;
 if ((i7 | 0) == (i1 | 0)) {
  STACKTOP = i2;
  return 1;
 }
 i3 = i5 + 52 | 0;
 i6 = HEAP32[i3 >> 2] | 0;
 i8 = i5 + 48 | 0;
 i5 = i5 + 44 | 0;
 if ((i6 | 0) == (HEAP32[i8 >> 2] | 0)) {
  i7 = HEAP32[i5 >> 2] | 0;
  HEAP32[i8 >> 2] = i6 << 1;
  i6 = __Z7b2Alloci(i6 * 24 | 0) | 0;
  HEAP32[i5 >> 2] = i6;
  _memcpy(i6 | 0, i7 | 0, (HEAP32[i3 >> 2] | 0) * 12 | 0) | 0;
  __Z6b2FreePv(i7);
  i7 = HEAP32[i4 >> 2] | 0;
  i6 = HEAP32[i3 >> 2] | 0;
 }
 i5 = HEAP32[i5 >> 2] | 0;
 HEAP32[i5 + (i6 * 12 | 0) >> 2] = (i7 | 0) > (i1 | 0) ? i1 : i7;
 i4 = HEAP32[i4 >> 2] | 0;
 HEAP32[i5 + ((HEAP32[i3 >> 2] | 0) * 12 | 0) + 4 >> 2] = (i4 | 0) < (i1 | 0) ? i1 : i4;
 HEAP32[i3 >> 2] = (HEAP32[i3 >> 2] | 0) + 1;
 STACKTOP = i2;
 return 1;
}
function __ZNK10__cxxabiv120__si_class_type_info27has_unambiguous_public_baseEPNS_19__dynamic_cast_infoEPvi(i5, i4, i3, i1) {
 i5 = i5 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i1 = i1 | 0;
 var i2 = 0, i6 = 0;
 i2 = STACKTOP;
 if ((i5 | 0) != (HEAP32[i4 + 8 >> 2] | 0)) {
  i6 = HEAP32[i5 + 8 >> 2] | 0;
  FUNCTION_TABLE_viiii[HEAP32[(HEAP32[i6 >> 2] | 0) + 28 >> 2] & 15](i6, i4, i3, i1);
  STACKTOP = i2;
  return;
 }
 i5 = i4 + 16 | 0;
 i6 = HEAP32[i5 >> 2] | 0;
 if ((i6 | 0) == 0) {
  HEAP32[i5 >> 2] = i3;
  HEAP32[i4 + 24 >> 2] = i1;
  HEAP32[i4 + 36 >> 2] = 1;
  STACKTOP = i2;
  return;
 }
 if ((i6 | 0) != (i3 | 0)) {
  i6 = i4 + 36 | 0;
  HEAP32[i6 >> 2] = (HEAP32[i6 >> 2] | 0) + 1;
  HEAP32[i4 + 24 >> 2] = 2;
  HEAP8[i4 + 54 | 0] = 1;
  STACKTOP = i2;
  return;
 }
 i3 = i4 + 24 | 0;
 if ((HEAP32[i3 >> 2] | 0) != 2) {
  STACKTOP = i2;
  return;
 }
 HEAP32[i3 >> 2] = i1;
 STACKTOP = i2;
 return;
}
function __ZN6b2Body19SynchronizeFixturesEv(i5) {
 i5 = i5 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, d6 = 0.0, d7 = 0.0, d8 = 0.0, d9 = 0.0, d10 = 0.0;
 i1 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 i3 = i1;
 d8 = +HEAPF32[i5 + 52 >> 2];
 d9 = +Math_sin(+d8);
 HEAPF32[i3 + 8 >> 2] = d9;
 d8 = +Math_cos(+d8);
 HEAPF32[i3 + 12 >> 2] = d8;
 d10 = +HEAPF32[i5 + 28 >> 2];
 d6 = +HEAPF32[i5 + 32 >> 2];
 d7 = +(+HEAPF32[i5 + 36 >> 2] - (d8 * d10 - d9 * d6));
 d6 = +(+HEAPF32[i5 + 40 >> 2] - (d10 * d9 + d8 * d6));
 i2 = i3;
 HEAPF32[i2 >> 2] = d7;
 HEAPF32[i2 + 4 >> 2] = d6;
 i2 = (HEAP32[i5 + 88 >> 2] | 0) + 102872 | 0;
 i4 = HEAP32[i5 + 100 >> 2] | 0;
 if ((i4 | 0) == 0) {
  STACKTOP = i1;
  return;
 }
 i5 = i5 + 12 | 0;
 do {
  __ZN9b2Fixture11SynchronizeEP12b2BroadPhaseRK11b2TransformS4_(i4, i2, i3, i5);
  i4 = HEAP32[i4 + 4 >> 2] | 0;
 } while ((i4 | 0) != 0);
 STACKTOP = i1;
 return;
}
function __ZN13b2DynamicTreeC2Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0;
 i4 = STACKTOP;
 HEAP32[i1 >> 2] = -1;
 i3 = i1 + 12 | 0;
 HEAP32[i3 >> 2] = 16;
 HEAP32[i1 + 8 >> 2] = 0;
 i6 = __Z7b2Alloci(576) | 0;
 i2 = i1 + 4 | 0;
 HEAP32[i2 >> 2] = i6;
 _memset(i6 | 0, 0, (HEAP32[i3 >> 2] | 0) * 36 | 0) | 0;
 i6 = (HEAP32[i3 >> 2] | 0) + -1 | 0;
 i2 = HEAP32[i2 >> 2] | 0;
 if ((i6 | 0) > 0) {
  i6 = 0;
  while (1) {
   i5 = i6 + 1 | 0;
   HEAP32[i2 + (i6 * 36 | 0) + 20 >> 2] = i5;
   HEAP32[i2 + (i6 * 36 | 0) + 32 >> 2] = -1;
   i6 = (HEAP32[i3 >> 2] | 0) + -1 | 0;
   if ((i5 | 0) < (i6 | 0)) {
    i6 = i5;
   } else {
    break;
   }
  }
 }
 HEAP32[i2 + (i6 * 36 | 0) + 20 >> 2] = -1;
 HEAP32[i2 + (((HEAP32[i3 >> 2] | 0) + -1 | 0) * 36 | 0) + 32 >> 2] = -1;
 HEAP32[i1 + 16 >> 2] = 0;
 HEAP32[i1 + 20 >> 2] = 0;
 HEAP32[i1 + 24 >> 2] = 0;
 STACKTOP = i4;
 return;
}
function __Z7measurePl(i1, i9) {
 i1 = i1 | 0;
 i9 = i9 | 0;
 var i2 = 0, i3 = 0, i4 = 0, d5 = 0.0, d6 = 0.0, i7 = 0, d8 = 0.0, i10 = 0, d11 = 0.0;
 i2 = STACKTOP;
 i3 = HEAP32[4] | 0;
 i4 = STACKTOP;
 STACKTOP = STACKTOP + ((4 * i3 | 0) + 15 & -16) | 0;
 i7 = (i3 | 0) > 0;
 if (i7) {
  i10 = 0;
  d6 = 0.0;
  do {
   d8 = +(HEAP32[i9 + (i10 << 2) >> 2] | 0) / 1.0e6 * 1.0e3;
   HEAPF32[i4 + (i10 << 2) >> 2] = d8;
   d6 = d6 + d8;
   i10 = i10 + 1 | 0;
  } while ((i10 | 0) < (i3 | 0));
  d5 = +(i3 | 0);
  d6 = d6 / d5;
  HEAPF32[i1 >> 2] = d6;
  if (i7) {
   i7 = 0;
   d8 = 0.0;
   do {
    d11 = +HEAPF32[i4 + (i7 << 2) >> 2] - d6;
    d8 = d8 + d11 * d11;
    i7 = i7 + 1 | 0;
   } while ((i7 | 0) < (i3 | 0));
  } else {
   d8 = 0.0;
  }
 } else {
  d5 = +(i3 | 0);
  HEAPF32[i1 >> 2] = 0.0 / d5;
  d8 = 0.0;
 }
 HEAPF32[i1 + 4 >> 2] = +Math_sqrt(+(d8 / d5));
 STACKTOP = i2;
 return;
}
function __ZN13b2DynamicTree11CreateProxyERK6b2AABBPv(i1, i3, i2) {
 i1 = i1 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 var i4 = 0, i5 = 0, i6 = 0, d7 = 0.0, d8 = 0.0, i9 = 0;
 i5 = STACKTOP;
 i4 = __ZN13b2DynamicTree12AllocateNodeEv(i1) | 0;
 i6 = i1 + 4 | 0;
 d7 = +(+HEAPF32[i3 >> 2] + -.10000000149011612);
 d8 = +(+HEAPF32[i3 + 4 >> 2] + -.10000000149011612);
 i9 = (HEAP32[i6 >> 2] | 0) + (i4 * 36 | 0) | 0;
 HEAPF32[i9 >> 2] = d7;
 HEAPF32[i9 + 4 >> 2] = d8;
 d8 = +(+HEAPF32[i3 + 8 >> 2] + .10000000149011612);
 d7 = +(+HEAPF32[i3 + 12 >> 2] + .10000000149011612);
 i3 = (HEAP32[i6 >> 2] | 0) + (i4 * 36 | 0) + 8 | 0;
 HEAPF32[i3 >> 2] = d8;
 HEAPF32[i3 + 4 >> 2] = d7;
 HEAP32[(HEAP32[i6 >> 2] | 0) + (i4 * 36 | 0) + 16 >> 2] = i2;
 HEAP32[(HEAP32[i6 >> 2] | 0) + (i4 * 36 | 0) + 32 >> 2] = 0;
 __ZN13b2DynamicTree10InsertLeafEi(i1, i4);
 STACKTOP = i5;
 return i4 | 0;
}
function __ZN16b2BlockAllocatorC2Ev(i3) {
 i3 = i3 | 0;
 var i1 = 0, i2 = 0, i4 = 0, i5 = 0;
 i2 = STACKTOP;
 i4 = i3 + 8 | 0;
 HEAP32[i4 >> 2] = 128;
 HEAP32[i3 + 4 >> 2] = 0;
 i5 = __Z7b2Alloci(1024) | 0;
 HEAP32[i3 >> 2] = i5;
 _memset(i5 | 0, 0, HEAP32[i4 >> 2] << 3 | 0) | 0;
 i4 = i3 + 12 | 0;
 i3 = i4 + 56 | 0;
 do {
  HEAP32[i4 >> 2] = 0;
  i4 = i4 + 4 | 0;
 } while ((i4 | 0) < (i3 | 0));
 if ((HEAP8[1280] | 0) == 0) {
  i3 = 1;
  i4 = 0;
 } else {
  STACKTOP = i2;
  return;
 }
 do {
  if ((i4 | 0) >= 14) {
   i1 = 3;
   break;
  }
  if ((i3 | 0) > (HEAP32[576 + (i4 << 2) >> 2] | 0)) {
   i4 = i4 + 1 | 0;
   HEAP8[632 + i3 | 0] = i4;
  } else {
   HEAP8[632 + i3 | 0] = i4;
  }
  i3 = i3 + 1 | 0;
 } while ((i3 | 0) < 641);
 if ((i1 | 0) == 3) {
  ___assert_fail(1288, 1312, 73, 1352);
 }
 HEAP8[1280] = 1;
 STACKTOP = i2;
 return;
}
function __ZN24b2ChainAndPolygonContact8EvaluateEP10b2ManifoldRK11b2TransformS4_(i2, i4, i3, i1) {
 i2 = i2 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i1 = i1 | 0;
 var i5 = 0, i6 = 0, i7 = 0, i8 = 0;
 i5 = STACKTOP;
 STACKTOP = STACKTOP + 48 | 0;
 i6 = i5;
 i7 = HEAP32[(HEAP32[i2 + 48 >> 2] | 0) + 12 >> 2] | 0;
 HEAP32[i6 >> 2] = 240;
 HEAP32[i6 + 4 >> 2] = 1;
 HEAPF32[i6 + 8 >> 2] = .009999999776482582;
 i8 = i6 + 28 | 0;
 HEAP32[i8 + 0 >> 2] = 0;
 HEAP32[i8 + 4 >> 2] = 0;
 HEAP32[i8 + 8 >> 2] = 0;
 HEAP32[i8 + 12 >> 2] = 0;
 HEAP16[i8 + 16 >> 1] = 0;
 __ZNK12b2ChainShape12GetChildEdgeEP11b2EdgeShapei(i7, i6, HEAP32[i2 + 56 >> 2] | 0);
 __Z23b2CollideEdgeAndPolygonP10b2ManifoldPK11b2EdgeShapeRK11b2TransformPK14b2PolygonShapeS6_(i4, i6, i3, HEAP32[(HEAP32[i2 + 52 >> 2] | 0) + 12 >> 2] | 0, i1);
 STACKTOP = i5;
 return;
}
function __ZN23b2ChainAndCircleContact8EvaluateEP10b2ManifoldRK11b2TransformS4_(i2, i4, i3, i1) {
 i2 = i2 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i1 = i1 | 0;
 var i5 = 0, i6 = 0, i7 = 0, i8 = 0;
 i5 = STACKTOP;
 STACKTOP = STACKTOP + 48 | 0;
 i6 = i5;
 i7 = HEAP32[(HEAP32[i2 + 48 >> 2] | 0) + 12 >> 2] | 0;
 HEAP32[i6 >> 2] = 240;
 HEAP32[i6 + 4 >> 2] = 1;
 HEAPF32[i6 + 8 >> 2] = .009999999776482582;
 i8 = i6 + 28 | 0;
 HEAP32[i8 + 0 >> 2] = 0;
 HEAP32[i8 + 4 >> 2] = 0;
 HEAP32[i8 + 8 >> 2] = 0;
 HEAP32[i8 + 12 >> 2] = 0;
 HEAP16[i8 + 16 >> 1] = 0;
 __ZNK12b2ChainShape12GetChildEdgeEP11b2EdgeShapei(i7, i6, HEAP32[i2 + 56 >> 2] | 0);
 __Z22b2CollideEdgeAndCircleP10b2ManifoldPK11b2EdgeShapeRK11b2TransformPK13b2CircleShapeS6_(i4, i6, i3, HEAP32[(HEAP32[i2 + 52 >> 2] | 0) + 12 >> 2] | 0, i1);
 STACKTOP = i5;
 return;
}
function __ZN15b2ContactSolver13StoreImpulsesEv(i4) {
 i4 = i4 | 0;
 var i1 = 0, i2 = 0, i3 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0;
 i1 = STACKTOP;
 i2 = HEAP32[i4 + 48 >> 2] | 0;
 if ((i2 | 0) <= 0) {
  STACKTOP = i1;
  return;
 }
 i3 = HEAP32[i4 + 40 >> 2] | 0;
 i4 = HEAP32[i4 + 44 >> 2] | 0;
 i5 = 0;
 do {
  i6 = HEAP32[i4 + (HEAP32[i3 + (i5 * 152 | 0) + 148 >> 2] << 2) >> 2] | 0;
  i7 = HEAP32[i3 + (i5 * 152 | 0) + 144 >> 2] | 0;
  if ((i7 | 0) > 0) {
   i8 = 0;
   do {
    HEAPF32[i6 + (i8 * 20 | 0) + 72 >> 2] = +HEAPF32[i3 + (i5 * 152 | 0) + (i8 * 36 | 0) + 16 >> 2];
    HEAPF32[i6 + (i8 * 20 | 0) + 76 >> 2] = +HEAPF32[i3 + (i5 * 152 | 0) + (i8 * 36 | 0) + 20 >> 2];
    i8 = i8 + 1 | 0;
   } while ((i8 | 0) < (i7 | 0));
  }
  i5 = i5 + 1 | 0;
 } while ((i5 | 0) < (i2 | 0));
 STACKTOP = i1;
 return;
}
function __ZN16b2StackAllocator4FreeEPv(i1, i5) {
 i1 = i1 | 0;
 i5 = i5 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i6 = 0;
 i3 = STACKTOP;
 i2 = i1 + 102796 | 0;
 i4 = HEAP32[i2 >> 2] | 0;
 if ((i4 | 0) <= 0) {
  ___assert_fail(3952, 3808, 63, 3976);
 }
 i6 = i4 + -1 | 0;
 if ((HEAP32[i1 + (i6 * 12 | 0) + 102412 >> 2] | 0) != (i5 | 0)) {
  ___assert_fail(3984, 3808, 65, 3976);
 }
 if ((HEAP8[i1 + (i6 * 12 | 0) + 102420 | 0] | 0) == 0) {
  i5 = i1 + (i6 * 12 | 0) + 102416 | 0;
  i6 = i1 + 102400 | 0;
  HEAP32[i6 >> 2] = (HEAP32[i6 >> 2] | 0) - (HEAP32[i5 >> 2] | 0);
 } else {
  __Z6b2FreePv(i5);
  i5 = i1 + (i6 * 12 | 0) + 102416 | 0;
  i4 = HEAP32[i2 >> 2] | 0;
 }
 i6 = i1 + 102404 | 0;
 HEAP32[i6 >> 2] = (HEAP32[i6 >> 2] | 0) - (HEAP32[i5 >> 2] | 0);
 HEAP32[i2 >> 2] = i4 + -1;
 STACKTOP = i3;
 return;
}
function __ZNK10__cxxabiv117__class_type_info27has_unambiguous_public_baseEPNS_19__dynamic_cast_infoEPvi(i5, i4, i3, i2) {
 i5 = i5 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 var i1 = 0, i6 = 0;
 i1 = STACKTOP;
 if ((HEAP32[i4 + 8 >> 2] | 0) != (i5 | 0)) {
  STACKTOP = i1;
  return;
 }
 i5 = i4 + 16 | 0;
 i6 = HEAP32[i5 >> 2] | 0;
 if ((i6 | 0) == 0) {
  HEAP32[i5 >> 2] = i3;
  HEAP32[i4 + 24 >> 2] = i2;
  HEAP32[i4 + 36 >> 2] = 1;
  STACKTOP = i1;
  return;
 }
 if ((i6 | 0) != (i3 | 0)) {
  i6 = i4 + 36 | 0;
  HEAP32[i6 >> 2] = (HEAP32[i6 >> 2] | 0) + 1;
  HEAP32[i4 + 24 >> 2] = 2;
  HEAP8[i4 + 54 | 0] = 1;
  STACKTOP = i1;
  return;
 }
 i3 = i4 + 24 | 0;
 if ((HEAP32[i3 >> 2] | 0) != 2) {
  STACKTOP = i1;
  return;
 }
 HEAP32[i3 >> 2] = i2;
 STACKTOP = i1;
 return;
}
function __ZN12b2BroadPhase11CreateProxyERK6b2AABBPv(i2, i4, i3) {
 i2 = i2 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 var i1 = 0, i5 = 0, i6 = 0, i7 = 0;
 i1 = STACKTOP;
 i3 = __ZN13b2DynamicTree11CreateProxyERK6b2AABBPv(i2, i4, i3) | 0;
 i4 = i2 + 28 | 0;
 HEAP32[i4 >> 2] = (HEAP32[i4 >> 2] | 0) + 1;
 i4 = i2 + 40 | 0;
 i5 = HEAP32[i4 >> 2] | 0;
 i6 = i2 + 36 | 0;
 i2 = i2 + 32 | 0;
 if ((i5 | 0) == (HEAP32[i6 >> 2] | 0)) {
  i7 = HEAP32[i2 >> 2] | 0;
  HEAP32[i6 >> 2] = i5 << 1;
  i5 = __Z7b2Alloci(i5 << 3) | 0;
  HEAP32[i2 >> 2] = i5;
  _memcpy(i5 | 0, i7 | 0, HEAP32[i4 >> 2] << 2 | 0) | 0;
  __Z6b2FreePv(i7);
  i5 = HEAP32[i4 >> 2] | 0;
 }
 HEAP32[(HEAP32[i2 >> 2] | 0) + (i5 << 2) >> 2] = i3;
 HEAP32[i4 >> 2] = (HEAP32[i4 >> 2] | 0) + 1;
 STACKTOP = i1;
 return i3 | 0;
}
function __ZN9b2ContactC2EP9b2FixtureiS1_i(i1, i4, i6, i3, i5) {
 i1 = i1 | 0;
 i4 = i4 | 0;
 i6 = i6 | 0;
 i3 = i3 | 0;
 i5 = i5 | 0;
 var i2 = 0, i7 = 0, d8 = 0.0, d9 = 0.0;
 i2 = STACKTOP;
 HEAP32[i1 >> 2] = 4440;
 HEAP32[i1 + 4 >> 2] = 4;
 HEAP32[i1 + 48 >> 2] = i4;
 HEAP32[i1 + 52 >> 2] = i3;
 HEAP32[i1 + 56 >> 2] = i6;
 HEAP32[i1 + 60 >> 2] = i5;
 HEAP32[i1 + 124 >> 2] = 0;
 HEAP32[i1 + 128 >> 2] = 0;
 i5 = i4 + 16 | 0;
 i6 = i1 + 8 | 0;
 i7 = i6 + 40 | 0;
 do {
  HEAP32[i6 >> 2] = 0;
  i6 = i6 + 4 | 0;
 } while ((i6 | 0) < (i7 | 0));
 HEAPF32[i1 + 136 >> 2] = +Math_sqrt(+(+HEAPF32[i5 >> 2] * +HEAPF32[i3 + 16 >> 2]));
 d8 = +HEAPF32[i4 + 20 >> 2];
 d9 = +HEAPF32[i3 + 20 >> 2];
 HEAPF32[i1 + 140 >> 2] = d8 > d9 ? d8 : d9;
 STACKTOP = i2;
 return;
}
function __ZN12b2BroadPhase9MoveProxyEiRK6b2AABBRK6b2Vec2(i3, i1, i5, i4) {
 i3 = i3 | 0;
 i1 = i1 | 0;
 i5 = i5 | 0;
 i4 = i4 | 0;
 var i2 = 0, i6 = 0, i7 = 0;
 i2 = STACKTOP;
 if (!(__ZN13b2DynamicTree9MoveProxyEiRK6b2AABBRK6b2Vec2(i3, i1, i5, i4) | 0)) {
  STACKTOP = i2;
  return;
 }
 i4 = i3 + 40 | 0;
 i5 = HEAP32[i4 >> 2] | 0;
 i6 = i3 + 36 | 0;
 i3 = i3 + 32 | 0;
 if ((i5 | 0) == (HEAP32[i6 >> 2] | 0)) {
  i7 = HEAP32[i3 >> 2] | 0;
  HEAP32[i6 >> 2] = i5 << 1;
  i5 = __Z7b2Alloci(i5 << 3) | 0;
  HEAP32[i3 >> 2] = i5;
  _memcpy(i5 | 0, i7 | 0, HEAP32[i4 >> 2] << 2 | 0) | 0;
  __Z6b2FreePv(i7);
  i5 = HEAP32[i4 >> 2] | 0;
 }
 HEAP32[(HEAP32[i3 >> 2] | 0) + (i5 << 2) >> 2] = i1;
 HEAP32[i4 >> 2] = (HEAP32[i4 >> 2] | 0) + 1;
 STACKTOP = i2;
 return;
}
function __ZN24b2ChainAndPolygonContact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator(i1, i3, i4, i5, i6) {
 i1 = i1 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 i5 = i5 | 0;
 i6 = i6 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 i6 = __ZN16b2BlockAllocator8AllocateEi(i6, 144) | 0;
 if ((i6 | 0) == 0) {
  i6 = 0;
  STACKTOP = i2;
  return i6 | 0;
 }
 __ZN9b2ContactC2EP9b2FixtureiS1_i(i6, i1, i3, i4, i5);
 HEAP32[i6 >> 2] = 6032;
 if ((HEAP32[(HEAP32[(HEAP32[i6 + 48 >> 2] | 0) + 12 >> 2] | 0) + 4 >> 2] | 0) != 3) {
  ___assert_fail(6048, 6096, 43, 6152);
 }
 if ((HEAP32[(HEAP32[(HEAP32[i6 + 52 >> 2] | 0) + 12 >> 2] | 0) + 4 >> 2] | 0) == 2) {
  STACKTOP = i2;
  return i6 | 0;
 } else {
  ___assert_fail(6184, 6096, 44, 6152);
 }
 return 0;
}
function __ZN23b2ChainAndCircleContact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator(i1, i3, i4, i5, i6) {
 i1 = i1 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 i5 = i5 | 0;
 i6 = i6 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 i6 = __ZN16b2BlockAllocator8AllocateEi(i6, 144) | 0;
 if ((i6 | 0) == 0) {
  i6 = 0;
  STACKTOP = i2;
  return i6 | 0;
 }
 __ZN9b2ContactC2EP9b2FixtureiS1_i(i6, i1, i3, i4, i5);
 HEAP32[i6 >> 2] = 5784;
 if ((HEAP32[(HEAP32[(HEAP32[i6 + 48 >> 2] | 0) + 12 >> 2] | 0) + 4 >> 2] | 0) != 3) {
  ___assert_fail(5800, 5848, 43, 5904);
 }
 if ((HEAP32[(HEAP32[(HEAP32[i6 + 52 >> 2] | 0) + 12 >> 2] | 0) + 4 >> 2] | 0) == 0) {
  STACKTOP = i2;
  return i6 | 0;
 } else {
  ___assert_fail(5928, 5848, 44, 5904);
 }
 return 0;
}
function __ZN25b2PolygonAndCircleContact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator(i1, i4, i2, i5, i3) {
 i1 = i1 | 0;
 i4 = i4 | 0;
 i2 = i2 | 0;
 i5 = i5 | 0;
 i3 = i3 | 0;
 i4 = STACKTOP;
 i3 = __ZN16b2BlockAllocator8AllocateEi(i3, 144) | 0;
 if ((i3 | 0) == 0) {
  i5 = 0;
  STACKTOP = i4;
  return i5 | 0;
 }
 __ZN9b2ContactC2EP9b2FixtureiS1_i(i3, i1, 0, i2, 0);
 HEAP32[i3 >> 2] = 4984;
 if ((HEAP32[(HEAP32[(HEAP32[i3 + 48 >> 2] | 0) + 12 >> 2] | 0) + 4 >> 2] | 0) != 2) {
  ___assert_fail(5e3, 5048, 41, 5104);
 }
 if ((HEAP32[(HEAP32[(HEAP32[i3 + 52 >> 2] | 0) + 12 >> 2] | 0) + 4 >> 2] | 0) == 0) {
  i5 = i3;
  STACKTOP = i4;
  return i5 | 0;
 } else {
  ___assert_fail(5136, 5048, 42, 5104);
 }
 return 0;
}
function __ZN23b2EdgeAndPolygonContact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator(i1, i4, i2, i5, i3) {
 i1 = i1 | 0;
 i4 = i4 | 0;
 i2 = i2 | 0;
 i5 = i5 | 0;
 i3 = i3 | 0;
 i4 = STACKTOP;
 i3 = __ZN16b2BlockAllocator8AllocateEi(i3, 144) | 0;
 if ((i3 | 0) == 0) {
  i5 = 0;
  STACKTOP = i4;
  return i5 | 0;
 }
 __ZN9b2ContactC2EP9b2FixtureiS1_i(i3, i1, 0, i2, 0);
 HEAP32[i3 >> 2] = 4736;
 if ((HEAP32[(HEAP32[(HEAP32[i3 + 48 >> 2] | 0) + 12 >> 2] | 0) + 4 >> 2] | 0) != 1) {
  ___assert_fail(4752, 4800, 41, 4856);
 }
 if ((HEAP32[(HEAP32[(HEAP32[i3 + 52 >> 2] | 0) + 12 >> 2] | 0) + 4 >> 2] | 0) == 2) {
  i5 = i3;
  STACKTOP = i4;
  return i5 | 0;
 } else {
  ___assert_fail(4880, 4800, 42, 4856);
 }
 return 0;
}
function __ZN22b2EdgeAndCircleContact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator(i1, i4, i2, i5, i3) {
 i1 = i1 | 0;
 i4 = i4 | 0;
 i2 = i2 | 0;
 i5 = i5 | 0;
 i3 = i3 | 0;
 i4 = STACKTOP;
 i3 = __ZN16b2BlockAllocator8AllocateEi(i3, 144) | 0;
 if ((i3 | 0) == 0) {
  i5 = 0;
  STACKTOP = i4;
  return i5 | 0;
 }
 __ZN9b2ContactC2EP9b2FixtureiS1_i(i3, i1, 0, i2, 0);
 HEAP32[i3 >> 2] = 4488;
 if ((HEAP32[(HEAP32[(HEAP32[i3 + 48 >> 2] | 0) + 12 >> 2] | 0) + 4 >> 2] | 0) != 1) {
  ___assert_fail(4504, 4552, 41, 4608);
 }
 if ((HEAP32[(HEAP32[(HEAP32[i3 + 52 >> 2] | 0) + 12 >> 2] | 0) + 4 >> 2] | 0) == 0) {
  i5 = i3;
  STACKTOP = i4;
  return i5 | 0;
 } else {
  ___assert_fail(4632, 4552, 42, 4608);
 }
 return 0;
}
function __ZN14b2PolygonShape8SetAsBoxEff(i1, d3, d2) {
 i1 = i1 | 0;
 d3 = +d3;
 d2 = +d2;
 var d4 = 0.0, d5 = 0.0;
 HEAP32[i1 + 148 >> 2] = 4;
 d4 = -d3;
 d5 = -d2;
 HEAPF32[i1 + 20 >> 2] = d4;
 HEAPF32[i1 + 24 >> 2] = d5;
 HEAPF32[i1 + 28 >> 2] = d3;
 HEAPF32[i1 + 32 >> 2] = d5;
 HEAPF32[i1 + 36 >> 2] = d3;
 HEAPF32[i1 + 40 >> 2] = d2;
 HEAPF32[i1 + 44 >> 2] = d4;
 HEAPF32[i1 + 48 >> 2] = d2;
 HEAPF32[i1 + 84 >> 2] = 0.0;
 HEAPF32[i1 + 88 >> 2] = -1.0;
 HEAPF32[i1 + 92 >> 2] = 1.0;
 HEAPF32[i1 + 96 >> 2] = 0.0;
 HEAPF32[i1 + 100 >> 2] = 0.0;
 HEAPF32[i1 + 104 >> 2] = 1.0;
 HEAPF32[i1 + 108 >> 2] = -1.0;
 HEAPF32[i1 + 112 >> 2] = 0.0;
 HEAPF32[i1 + 12 >> 2] = 0.0;
 HEAPF32[i1 + 16 >> 2] = 0.0;
 return;
}
function __ZN16b2PolygonContact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator(i1, i4, i2, i5, i3) {
 i1 = i1 | 0;
 i4 = i4 | 0;
 i2 = i2 | 0;
 i5 = i5 | 0;
 i3 = i3 | 0;
 i4 = STACKTOP;
 i3 = __ZN16b2BlockAllocator8AllocateEi(i3, 144) | 0;
 if ((i3 | 0) == 0) {
  i5 = 0;
  STACKTOP = i4;
  return i5 | 0;
 }
 __ZN9b2ContactC2EP9b2FixtureiS1_i(i3, i1, 0, i2, 0);
 HEAP32[i3 >> 2] = 5240;
 if ((HEAP32[(HEAP32[(HEAP32[i3 + 48 >> 2] | 0) + 12 >> 2] | 0) + 4 >> 2] | 0) != 2) {
  ___assert_fail(5256, 5304, 44, 5352);
 }
 if ((HEAP32[(HEAP32[(HEAP32[i3 + 52 >> 2] | 0) + 12 >> 2] | 0) + 4 >> 2] | 0) == 2) {
  i5 = i3;
  STACKTOP = i4;
  return i5 | 0;
 } else {
  ___assert_fail(5376, 5304, 45, 5352);
 }
 return 0;
}
function __ZN15b2CircleContact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator(i1, i4, i2, i5, i3) {
 i1 = i1 | 0;
 i4 = i4 | 0;
 i2 = i2 | 0;
 i5 = i5 | 0;
 i3 = i3 | 0;
 i4 = STACKTOP;
 i3 = __ZN16b2BlockAllocator8AllocateEi(i3, 144) | 0;
 if ((i3 | 0) == 0) {
  i5 = 0;
  STACKTOP = i4;
  return i5 | 0;
 }
 __ZN9b2ContactC2EP9b2FixtureiS1_i(i3, i1, 0, i2, 0);
 HEAP32[i3 >> 2] = 6288;
 if ((HEAP32[(HEAP32[(HEAP32[i3 + 48 >> 2] | 0) + 12 >> 2] | 0) + 4 >> 2] | 0) != 0) {
  ___assert_fail(6304, 6352, 44, 6400);
 }
 if ((HEAP32[(HEAP32[(HEAP32[i3 + 52 >> 2] | 0) + 12 >> 2] | 0) + 4 >> 2] | 0) == 0) {
  i5 = i3;
  STACKTOP = i4;
  return i5 | 0;
 } else {
  ___assert_fail(6416, 6352, 45, 6400);
 }
 return 0;
}
function __ZN7b2World10CreateBodyEPK9b2BodyDef(i1, i4) {
 i1 = i1 | 0;
 i4 = i4 | 0;
 var i2 = 0, i3 = 0, i5 = 0;
 i2 = STACKTOP;
 if ((HEAP32[i1 + 102868 >> 2] & 2 | 0) != 0) {
  ___assert_fail(2160, 2184, 109, 2216);
 }
 i3 = __ZN16b2BlockAllocator8AllocateEi(i1, 152) | 0;
 if ((i3 | 0) == 0) {
  i3 = 0;
 } else {
  __ZN6b2BodyC2EPK9b2BodyDefP7b2World(i3, i4, i1);
 }
 HEAP32[i3 + 92 >> 2] = 0;
 i4 = i1 + 102952 | 0;
 HEAP32[i3 + 96 >> 2] = HEAP32[i4 >> 2];
 i5 = HEAP32[i4 >> 2] | 0;
 if ((i5 | 0) != 0) {
  HEAP32[i5 + 92 >> 2] = i3;
 }
 HEAP32[i4 >> 2] = i3;
 i5 = i1 + 102960 | 0;
 HEAP32[i5 >> 2] = (HEAP32[i5 >> 2] | 0) + 1;
 STACKTOP = i2;
 return i3 | 0;
}
function __ZNK6b2Body13ShouldCollideEPKS_(i4, i2) {
 i4 = i4 | 0;
 i2 = i2 | 0;
 var i1 = 0, i3 = 0;
 i1 = STACKTOP;
 if ((HEAP32[i4 >> 2] | 0) != 2 ? (HEAP32[i2 >> 2] | 0) != 2 : 0) {
  i2 = 0;
 } else {
  i3 = 3;
 }
 L3 : do {
  if ((i3 | 0) == 3) {
   i3 = HEAP32[i4 + 108 >> 2] | 0;
   if ((i3 | 0) == 0) {
    i2 = 1;
   } else {
    while (1) {
     if ((HEAP32[i3 >> 2] | 0) == (i2 | 0) ? (HEAP8[(HEAP32[i3 + 4 >> 2] | 0) + 61 | 0] | 0) == 0 : 0) {
      i2 = 0;
      break L3;
     }
     i3 = HEAP32[i3 + 12 >> 2] | 0;
     if ((i3 | 0) == 0) {
      i2 = 1;
      break;
     }
    }
   }
  }
 } while (0);
 STACKTOP = i1;
 return i2 | 0;
}
function __ZNK14b2PolygonShape5CloneEP16b2BlockAllocator(i1, i3) {
 i1 = i1 | 0;
 i3 = i3 | 0;
 var i2 = 0, i4 = 0, i5 = 0, i6 = 0;
 i2 = STACKTOP;
 i3 = __ZN16b2BlockAllocator8AllocateEi(i3, 152) | 0;
 if ((i3 | 0) == 0) {
  i3 = 0;
 } else {
  HEAP32[i3 >> 2] = 504;
  HEAP32[i3 + 4 >> 2] = 2;
  HEAPF32[i3 + 8 >> 2] = .009999999776482582;
  HEAP32[i3 + 148 >> 2] = 0;
  HEAPF32[i3 + 12 >> 2] = 0.0;
  HEAPF32[i3 + 16 >> 2] = 0.0;
 }
 i6 = i1 + 4 | 0;
 i5 = HEAP32[i6 + 4 >> 2] | 0;
 i4 = i3 + 4 | 0;
 HEAP32[i4 >> 2] = HEAP32[i6 >> 2];
 HEAP32[i4 + 4 >> 2] = i5;
 _memcpy(i3 + 12 | 0, i1 + 12 | 0, 140) | 0;
 STACKTOP = i2;
 return i3 | 0;
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
function __ZN7b2World16SetAllowSleepingEb(i2, i4) {
 i2 = i2 | 0;
 i4 = i4 | 0;
 var i1 = 0, i3 = 0;
 i1 = STACKTOP;
 i3 = i2 + 102976 | 0;
 if ((i4 & 1 | 0) == (HEAPU8[i3] | 0 | 0)) {
  STACKTOP = i1;
  return;
 }
 HEAP8[i3] = i4 & 1;
 if (i4) {
  STACKTOP = i1;
  return;
 }
 i2 = HEAP32[i2 + 102952 >> 2] | 0;
 if ((i2 | 0) == 0) {
  STACKTOP = i1;
  return;
 }
 do {
  i3 = i2 + 4 | 0;
  i4 = HEAPU16[i3 >> 1] | 0;
  if ((i4 & 2 | 0) == 0) {
   HEAP16[i3 >> 1] = i4 | 2;
   HEAPF32[i2 + 144 >> 2] = 0.0;
  }
  i2 = HEAP32[i2 + 96 >> 2] | 0;
 } while ((i2 | 0) != 0);
 STACKTOP = i1;
 return;
}
function __ZN16b2BlockAllocator4FreeEPvi(i3, i1, i4) {
 i3 = i3 | 0;
 i1 = i1 | 0;
 i4 = i4 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 if ((i4 | 0) == 0) {
  STACKTOP = i2;
  return;
 }
 if ((i4 | 0) <= 0) {
  ___assert_fail(1376, 1312, 164, 1488);
 }
 if ((i4 | 0) > 640) {
  __Z6b2FreePv(i1);
  STACKTOP = i2;
  return;
 }
 i4 = HEAP8[632 + i4 | 0] | 0;
 if (!((i4 & 255) < 14)) {
  ___assert_fail(1408, 1312, 173, 1488);
 }
 i4 = i3 + ((i4 & 255) << 2) + 12 | 0;
 HEAP32[i1 >> 2] = HEAP32[i4 >> 2];
 HEAP32[i4 >> 2] = i1;
 STACKTOP = i2;
 return;
}
function __ZN15b2ContactFilter13ShouldCollideEP9b2FixtureS1_(i3, i2, i1) {
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 var i4 = 0;
 i3 = STACKTOP;
 i4 = HEAP16[i2 + 36 >> 1] | 0;
 if (!(i4 << 16 >> 16 != (HEAP16[i1 + 36 >> 1] | 0) | i4 << 16 >> 16 == 0)) {
  i4 = i4 << 16 >> 16 > 0;
  STACKTOP = i3;
  return i4 | 0;
 }
 if ((HEAP16[i1 + 32 >> 1] & HEAP16[i2 + 34 >> 1]) << 16 >> 16 == 0) {
  i4 = 0;
  STACKTOP = i3;
  return i4 | 0;
 }
 i4 = (HEAP16[i1 + 34 >> 1] & HEAP16[i2 + 32 >> 1]) << 16 >> 16 != 0;
 STACKTOP = i3;
 return i4 | 0;
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
function __ZN6b2Body13CreateFixtureEPK7b2Shapef(i1, i3, d2) {
 i1 = i1 | 0;
 i3 = i3 | 0;
 d2 = +d2;
 var i4 = 0, i5 = 0;
 i4 = STACKTOP;
 STACKTOP = STACKTOP + 32 | 0;
 i5 = i4;
 HEAP16[i5 + 22 >> 1] = 1;
 HEAP16[i5 + 24 >> 1] = -1;
 HEAP16[i5 + 26 >> 1] = 0;
 HEAP32[i5 + 4 >> 2] = 0;
 HEAPF32[i5 + 8 >> 2] = .20000000298023224;
 HEAPF32[i5 + 12 >> 2] = 0.0;
 HEAP8[i5 + 20 | 0] = 0;
 HEAP32[i5 >> 2] = i3;
 HEAPF32[i5 + 16 >> 2] = d2;
 i3 = __ZN6b2Body13CreateFixtureEPK12b2FixtureDef(i1, i5) | 0;
 STACKTOP = i4;
 return i3 | 0;
}
function __Znwj(i2) {
 i2 = i2 | 0;
 var i1 = 0, i3 = 0;
 i1 = STACKTOP;
 i2 = (i2 | 0) == 0 ? 1 : i2;
 while (1) {
  i3 = _malloc(i2) | 0;
  if ((i3 | 0) != 0) {
   i2 = 6;
   break;
  }
  i3 = HEAP32[1914] | 0;
  HEAP32[1914] = i3 + 0;
  if ((i3 | 0) == 0) {
   i2 = 5;
   break;
  }
  FUNCTION_TABLE_v[i3 & 3]();
 }
 if ((i2 | 0) == 5) {
  i3 = ___cxa_allocate_exception(4) | 0;
  HEAP32[i3 >> 2] = 7672;
  ___cxa_throw(i3 | 0, 7720, 30);
 } else if ((i2 | 0) == 6) {
  STACKTOP = i1;
  return i3 | 0;
 }
 return 0;
}
function __ZN8b2IslandD2Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZN16b2StackAllocator4FreeEPv(HEAP32[i1 >> 2] | 0, HEAP32[i1 + 20 >> 2] | 0);
 __ZN16b2StackAllocator4FreeEPv(HEAP32[i1 >> 2] | 0, HEAP32[i1 + 24 >> 2] | 0);
 __ZN16b2StackAllocator4FreeEPv(HEAP32[i1 >> 2] | 0, HEAP32[i1 + 16 >> 2] | 0);
 __ZN16b2StackAllocator4FreeEPv(HEAP32[i1 >> 2] | 0, HEAP32[i1 + 12 >> 2] | 0);
 __ZN16b2StackAllocator4FreeEPv(HEAP32[i1 >> 2] | 0, HEAP32[i1 + 8 >> 2] | 0);
 STACKTOP = i2;
 return;
}
function __ZN16b2BlockAllocatorD2Ev(i2) {
 i2 = i2 | 0;
 var i1 = 0, i3 = 0, i4 = 0, i5 = 0;
 i1 = STACKTOP;
 i3 = i2 + 4 | 0;
 i4 = HEAP32[i2 >> 2] | 0;
 if ((HEAP32[i3 >> 2] | 0) > 0) {
  i5 = 0;
 } else {
  i5 = i4;
  __Z6b2FreePv(i5);
  STACKTOP = i1;
  return;
 }
 do {
  __Z6b2FreePv(HEAP32[i4 + (i5 << 3) + 4 >> 2] | 0);
  i5 = i5 + 1 | 0;
  i4 = HEAP32[i2 >> 2] | 0;
 } while ((i5 | 0) < (HEAP32[i3 >> 2] | 0));
 __Z6b2FreePv(i4);
 STACKTOP = i1;
 return;
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
function __ZNK11b2EdgeShape11ComputeMassEP10b2MassDataf(i2, i1, d3) {
 i2 = i2 | 0;
 i1 = i1 | 0;
 d3 = +d3;
 var i4 = 0, d5 = 0.0;
 i4 = STACKTOP;
 HEAPF32[i1 >> 2] = 0.0;
 d5 = +((+HEAPF32[i2 + 12 >> 2] + +HEAPF32[i2 + 20 >> 2]) * .5);
 d3 = +((+HEAPF32[i2 + 16 >> 2] + +HEAPF32[i2 + 24 >> 2]) * .5);
 i2 = i1 + 4 | 0;
 HEAPF32[i2 >> 2] = d5;
 HEAPF32[i2 + 4 >> 2] = d3;
 HEAPF32[i1 + 12 >> 2] = 0.0;
 STACKTOP = i4;
 return;
}
function __ZN11b2EdgeShape3SetERK6b2Vec2S2_(i1, i3, i2) {
 i1 = i1 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 var i4 = 0, i5 = 0;
 i5 = i3;
 i3 = HEAP32[i5 + 4 >> 2] | 0;
 i4 = i1 + 12 | 0;
 HEAP32[i4 >> 2] = HEAP32[i5 >> 2];
 HEAP32[i4 + 4 >> 2] = i3;
 i4 = i2;
 i2 = HEAP32[i4 + 4 >> 2] | 0;
 i3 = i1 + 20 | 0;
 HEAP32[i3 >> 2] = HEAP32[i4 >> 2];
 HEAP32[i3 + 4 >> 2] = i2;
 HEAP8[i1 + 44 | 0] = 0;
 HEAP8[i1 + 45 | 0] = 0;
 return;
}
function __ZN25b2PolygonAndCircleContact8EvaluateEP10b2ManifoldRK11b2TransformS4_(i2, i4, i3, i1) {
 i2 = i2 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i1 = i1 | 0;
 var i5 = 0;
 i5 = STACKTOP;
 __Z25b2CollidePolygonAndCircleP10b2ManifoldPK14b2PolygonShapeRK11b2TransformPK13b2CircleShapeS6_(i4, HEAP32[(HEAP32[i2 + 48 >> 2] | 0) + 12 >> 2] | 0, i3, HEAP32[(HEAP32[i2 + 52 >> 2] | 0) + 12 >> 2] | 0, i1);
 STACKTOP = i5;
 return;
}
function __ZN23b2EdgeAndPolygonContact8EvaluateEP10b2ManifoldRK11b2TransformS4_(i2, i4, i3, i1) {
 i2 = i2 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i1 = i1 | 0;
 var i5 = 0;
 i5 = STACKTOP;
 __Z23b2CollideEdgeAndPolygonP10b2ManifoldPK11b2EdgeShapeRK11b2TransformPK14b2PolygonShapeS6_(i4, HEAP32[(HEAP32[i2 + 48 >> 2] | 0) + 12 >> 2] | 0, i3, HEAP32[(HEAP32[i2 + 52 >> 2] | 0) + 12 >> 2] | 0, i1);
 STACKTOP = i5;
 return;
}
function __ZN22b2EdgeAndCircleContact8EvaluateEP10b2ManifoldRK11b2TransformS4_(i2, i4, i3, i1) {
 i2 = i2 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i1 = i1 | 0;
 var i5 = 0;
 i5 = STACKTOP;
 __Z22b2CollideEdgeAndCircleP10b2ManifoldPK11b2EdgeShapeRK11b2TransformPK13b2CircleShapeS6_(i4, HEAP32[(HEAP32[i2 + 48 >> 2] | 0) + 12 >> 2] | 0, i3, HEAP32[(HEAP32[i2 + 52 >> 2] | 0) + 12 >> 2] | 0, i1);
 STACKTOP = i5;
 return;
}
function __Z23b2CollideEdgeAndPolygonP10b2ManifoldPK11b2EdgeShapeRK11b2TransformPK14b2PolygonShapeS6_(i5, i4, i3, i2, i1) {
 i5 = i5 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 var i6 = 0;
 i6 = STACKTOP;
 STACKTOP = STACKTOP + 256 | 0;
 __ZN12b2EPCollider7CollideEP10b2ManifoldPK11b2EdgeShapeRK11b2TransformPK14b2PolygonShapeS7_(i6, i5, i4, i3, i2, i1);
 STACKTOP = i6;
 return;
}
function __ZN16b2PolygonContact8EvaluateEP10b2ManifoldRK11b2TransformS4_(i2, i4, i3, i1) {
 i2 = i2 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i1 = i1 | 0;
 var i5 = 0;
 i5 = STACKTOP;
 __Z17b2CollidePolygonsP10b2ManifoldPK14b2PolygonShapeRK11b2TransformS3_S6_(i4, HEAP32[(HEAP32[i2 + 48 >> 2] | 0) + 12 >> 2] | 0, i3, HEAP32[(HEAP32[i2 + 52 >> 2] | 0) + 12 >> 2] | 0, i1);
 STACKTOP = i5;
 return;
}
function __ZN15b2CircleContact8EvaluateEP10b2ManifoldRK11b2TransformS4_(i2, i4, i3, i1) {
 i2 = i2 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i1 = i1 | 0;
 var i5 = 0;
 i5 = STACKTOP;
 __Z16b2CollideCirclesP10b2ManifoldPK13b2CircleShapeRK11b2TransformS3_S6_(i4, HEAP32[(HEAP32[i2 + 48 >> 2] | 0) + 12 >> 2] | 0, i3, HEAP32[(HEAP32[i2 + 52 >> 2] | 0) + 12 >> 2] | 0, i1);
 STACKTOP = i5;
 return;
}
function __Z14b2PairLessThanRK6b2PairS1_(i2, i5) {
 i2 = i2 | 0;
 i5 = i5 | 0;
 var i1 = 0, i3 = 0, i4 = 0;
 i1 = STACKTOP;
 i4 = HEAP32[i2 >> 2] | 0;
 i3 = HEAP32[i5 >> 2] | 0;
 if ((i4 | 0) >= (i3 | 0)) {
  if ((i4 | 0) == (i3 | 0)) {
   i2 = (HEAP32[i2 + 4 >> 2] | 0) < (HEAP32[i5 + 4 >> 2] | 0);
  } else {
   i2 = 0;
  }
 } else {
  i2 = 1;
 }
 STACKTOP = i1;
 return i2 | 0;
}
function __ZN9b2FixtureC2Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 HEAP16[i1 + 32 >> 1] = 1;
 HEAP16[i1 + 34 >> 1] = -1;
 HEAP16[i1 + 36 >> 1] = 0;
 HEAP32[i1 + 40 >> 2] = 0;
 HEAP32[i1 + 24 >> 2] = 0;
 HEAP32[i1 + 28 >> 2] = 0;
 HEAP32[i1 + 0 >> 2] = 0;
 HEAP32[i1 + 4 >> 2] = 0;
 HEAP32[i1 + 8 >> 2] = 0;
 HEAP32[i1 + 12 >> 2] = 0;
 STACKTOP = i2;
 return;
}
function __ZN12b2BroadPhaseC2Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZN13b2DynamicTreeC2Ev(i1);
 HEAP32[i1 + 28 >> 2] = 0;
 HEAP32[i1 + 48 >> 2] = 16;
 HEAP32[i1 + 52 >> 2] = 0;
 HEAP32[i1 + 44 >> 2] = __Z7b2Alloci(192) | 0;
 HEAP32[i1 + 36 >> 2] = 16;
 HEAP32[i1 + 40 >> 2] = 0;
 HEAP32[i1 + 32 >> 2] = __Z7b2Alloci(64) | 0;
 STACKTOP = i2;
 return;
}
function __ZN16b2StackAllocatorD2Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 if ((HEAP32[i1 + 102400 >> 2] | 0) != 0) {
  ___assert_fail(3792, 3808, 32, 3848);
 }
 if ((HEAP32[i1 + 102796 >> 2] | 0) == 0) {
  STACKTOP = i2;
  return;
 } else {
  ___assert_fail(3872, 3808, 33, 3848);
 }
}
function __ZN15b2ContactSolverD2Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0;
 i2 = STACKTOP;
 i3 = i1 + 32 | 0;
 __ZN16b2StackAllocator4FreeEPv(HEAP32[i3 >> 2] | 0, HEAP32[i1 + 40 >> 2] | 0);
 __ZN16b2StackAllocator4FreeEPv(HEAP32[i3 >> 2] | 0, HEAP32[i1 + 36 >> 2] | 0);
 STACKTOP = i2;
 return;
}
function __ZN25b2PolygonAndCircleContact7DestroyEP9b2ContactP16b2BlockAllocator(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0;
 i3 = STACKTOP;
 FUNCTION_TABLE_vi[HEAP32[(HEAP32[i1 >> 2] | 0) + 4 >> 2] & 31](i1);
 __ZN16b2BlockAllocator4FreeEPvi(i2, i1, 144);
 STACKTOP = i3;
 return;
}
function __ZN24b2ChainAndPolygonContact7DestroyEP9b2ContactP16b2BlockAllocator(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0;
 i3 = STACKTOP;
 FUNCTION_TABLE_vi[HEAP32[(HEAP32[i1 >> 2] | 0) + 4 >> 2] & 31](i1);
 __ZN16b2BlockAllocator4FreeEPvi(i2, i1, 144);
 STACKTOP = i3;
 return;
}
function __ZN23b2EdgeAndPolygonContact7DestroyEP9b2ContactP16b2BlockAllocator(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0;
 i3 = STACKTOP;
 FUNCTION_TABLE_vi[HEAP32[(HEAP32[i1 >> 2] | 0) + 4 >> 2] & 31](i1);
 __ZN16b2BlockAllocator4FreeEPvi(i2, i1, 144);
 STACKTOP = i3;
 return;
}
function __ZN23b2ChainAndCircleContact7DestroyEP9b2ContactP16b2BlockAllocator(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0;
 i3 = STACKTOP;
 FUNCTION_TABLE_vi[HEAP32[(HEAP32[i1 >> 2] | 0) + 4 >> 2] & 31](i1);
 __ZN16b2BlockAllocator4FreeEPvi(i2, i1, 144);
 STACKTOP = i3;
 return;
}
function __ZN22b2EdgeAndCircleContact7DestroyEP9b2ContactP16b2BlockAllocator(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0;
 i3 = STACKTOP;
 FUNCTION_TABLE_vi[HEAP32[(HEAP32[i1 >> 2] | 0) + 4 >> 2] & 31](i1);
 __ZN16b2BlockAllocator4FreeEPvi(i2, i1, 144);
 STACKTOP = i3;
 return;
}
function __ZN16b2ContactManagerC2Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZN12b2BroadPhaseC2Ev(i1);
 HEAP32[i1 + 60 >> 2] = 0;
 HEAP32[i1 + 64 >> 2] = 0;
 HEAP32[i1 + 68 >> 2] = 1888;
 HEAP32[i1 + 72 >> 2] = 1896;
 HEAP32[i1 + 76 >> 2] = 0;
 STACKTOP = i2;
 return;
}
function __ZN16b2PolygonContact7DestroyEP9b2ContactP16b2BlockAllocator(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0;
 i3 = STACKTOP;
 FUNCTION_TABLE_vi[HEAP32[(HEAP32[i1 >> 2] | 0) + 4 >> 2] & 31](i1);
 __ZN16b2BlockAllocator4FreeEPvi(i2, i1, 144);
 STACKTOP = i3;
 return;
}
function __ZN15b2CircleContact7DestroyEP9b2ContactP16b2BlockAllocator(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0;
 i3 = STACKTOP;
 FUNCTION_TABLE_vi[HEAP32[(HEAP32[i1 >> 2] | 0) + 4 >> 2] & 31](i1);
 __ZN16b2BlockAllocator4FreeEPvi(i2, i1, 144);
 STACKTOP = i3;
 return;
}
function dynCall_viiiiii(i7, i6, i5, i4, i3, i2, i1) {
 i7 = i7 | 0;
 i6 = i6 | 0;
 i5 = i5 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 FUNCTION_TABLE_viiiiii[i7 & 3](i6 | 0, i5 | 0, i4 | 0, i3 | 0, i2 | 0, i1 | 0);
}
function copyTempFloat(i1) {
 i1 = i1 | 0;
 HEAP8[tempDoublePtr] = HEAP8[i1];
 HEAP8[tempDoublePtr + 1 | 0] = HEAP8[i1 + 1 | 0];
 HEAP8[tempDoublePtr + 2 | 0] = HEAP8[i1 + 2 | 0];
 HEAP8[tempDoublePtr + 3 | 0] = HEAP8[i1 + 3 | 0];
}
function dynCall_iiiiii(i6, i5, i4, i3, i2, i1) {
 i6 = i6 | 0;
 i5 = i5 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 return FUNCTION_TABLE_iiiiii[i6 & 15](i5 | 0, i4 | 0, i3 | 0, i2 | 0, i1 | 0) | 0;
}
function dynCall_viiiii(i6, i5, i4, i3, i2, i1) {
 i6 = i6 | 0;
 i5 = i5 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 FUNCTION_TABLE_viiiii[i6 & 3](i5 | 0, i4 | 0, i3 | 0, i2 | 0, i1 | 0);
}
function __ZN16b2ContactManager15FindNewContactsEv(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZN12b2BroadPhase11UpdatePairsI16b2ContactManagerEEvPT_(i1, i1);
 STACKTOP = i2;
 return;
}
function __ZN16b2StackAllocatorC2Ev(i1) {
 i1 = i1 | 0;
 HEAP32[i1 + 102400 >> 2] = 0;
 HEAP32[i1 + 102404 >> 2] = 0;
 HEAP32[i1 + 102408 >> 2] = 0;
 HEAP32[i1 + 102796 >> 2] = 0;
 return;
}
function dynCall_viiii(i5, i4, i3, i2, i1) {
 i5 = i5 | 0;
 i4 = i4 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 FUNCTION_TABLE_viiii[i5 & 15](i4 | 0, i3 | 0, i2 | 0, i1 | 0);
}
function dynCall_iiii(i4, i3, i2, i1) {
 i4 = i4 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 return FUNCTION_TABLE_iiii[i4 & 7](i3 | 0, i2 | 0, i1 | 0) | 0;
}
function dynCall_viii(i4, i3, i2, i1) {
 i4 = i4 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 FUNCTION_TABLE_viii[i4 & 3](i3 | 0, i2 | 0, i1 | 0);
}
function __ZNSt9bad_allocD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZNSt9exceptionD2Ev(i1 | 0);
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function stackAlloc(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 STACKTOP = STACKTOP + i1 | 0;
 STACKTOP = STACKTOP + 7 & -8;
 return i2 | 0;
}
function __ZN13b2DynamicTreeD2Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __Z6b2FreePv(HEAP32[i1 + 4 >> 2] | 0);
 STACKTOP = i2;
 return;
}
function dynCall_viid(i4, i3, i2, d1) {
 i4 = i4 | 0;
 i3 = i3 | 0;
 i2 = i2 | 0;
 d1 = +d1;
 FUNCTION_TABLE_viid[i4 & 3](i3 | 0, i2 | 0, +d1);
}
function __ZN17b2ContactListener9PostSolveEP9b2ContactPK16b2ContactImpulse(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 return;
}
function __ZN10__cxxabiv120__si_class_type_infoD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function __ZN10__cxxabiv117__class_type_infoD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function __ZNSt9bad_allocD2Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZNSt9exceptionD2Ev(i1 | 0);
 STACKTOP = i2;
 return;
}
function dynCall_iii(i3, i2, i1) {
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 return FUNCTION_TABLE_iii[i3 & 3](i2 | 0, i1 | 0) | 0;
}
function b8(i1, i2, i3, i4, i5, i6) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 i5 = i5 | 0;
 i6 = i6 | 0;
 abort(8);
}
function __ZN25b2PolygonAndCircleContactD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function __ZN17b2ContactListener8PreSolveEP9b2ContactPK10b2Manifold(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 return;
}
function __ZN24b2ChainAndPolygonContactD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function __ZN23b2EdgeAndPolygonContactD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function __ZN23b2ChainAndCircleContactD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function __ZNK11b2EdgeShape9TestPointERK11b2TransformRK6b2Vec2(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 return 0;
}
function __ZN22b2EdgeAndCircleContactD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function __ZdlPv(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 if ((i1 | 0) != 0) {
  _free(i1);
 }
 STACKTOP = i2;
 return;
}
function b10(i1, i2, i3, i4, i5) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 i5 = i5 | 0;
 abort(10);
 return 0;
}
function _strlen(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = i1;
 while (HEAP8[i2] | 0) {
  i2 = i2 + 1 | 0;
 }
 return i2 - i1 | 0;
}
function __Z7b2Alloci(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 i1 = _malloc(i1) | 0;
 STACKTOP = i2;
 return i1 | 0;
}
function setThrew(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 if ((__THREW__ | 0) == 0) {
  __THREW__ = i1;
  threwValue = i2;
 }
}
function __ZN17b2ContactListenerD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function __ZN16b2PolygonContactD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function dynCall_vii(i3, i2, i1) {
 i3 = i3 | 0;
 i2 = i2 | 0;
 i1 = i1 | 0;
 FUNCTION_TABLE_vii[i3 & 15](i2 | 0, i1 | 0);
}
function __ZN15b2ContactFilterD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function __ZN15b2CircleContactD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function __ZN14b2PolygonShapeD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function __Znaj(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 i1 = __Znwj(i1) | 0;
 STACKTOP = i2;
 return i1 | 0;
}
function __ZN11b2EdgeShapeD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function __ZN9b2ContactD0Ev(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 __ZdlPv(i1);
 STACKTOP = i2;
 return;
}
function b1(i1, i2, i3, i4, i5) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 i5 = i5 | 0;
 abort(1);
}
function __Z6b2FreePv(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 _free(i1);
 STACKTOP = i2;
 return;
}
function ___clang_call_terminate(i1) {
 i1 = i1 | 0;
 ___cxa_begin_catch(i1 | 0) | 0;
 __ZSt9terminatev();
}
function __ZN17b2ContactListener12BeginContactEP9b2Contact(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 return;
}
function dynCall_ii(i2, i1) {
 i2 = i2 | 0;
 i1 = i1 | 0;
 return FUNCTION_TABLE_ii[i2 & 3](i1 | 0) | 0;
}
function __ZN17b2ContactListener10EndContactEP9b2Contact(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 return;
}
function b11(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 abort(11);
}
function dynCall_vi(i2, i1) {
 i2 = i2 | 0;
 i1 = i1 | 0;
 FUNCTION_TABLE_vi[i2 & 31](i1 | 0);
}
function b0(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 abort(0);
 return 0;
}
function __ZNK10__cxxabiv116__shim_type_info5noop2Ev(i1) {
 i1 = i1 | 0;
 return;
}
function __ZNK10__cxxabiv116__shim_type_info5noop1Ev(i1) {
 i1 = i1 | 0;
 return;
}
function b5(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 abort(5);
}
function __ZNK14b2PolygonShape13GetChildCountEv(i1) {
 i1 = i1 | 0;
 return 1;
}
function __ZN10__cxxabiv116__shim_type_infoD2Ev(i1) {
 i1 = i1 | 0;
 return;
}
function b7(i1, i2, d3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 d3 = +d3;
 abort(7);
}
function __ZNK11b2EdgeShape13GetChildCountEv(i1) {
 i1 = i1 | 0;
 return 1;
}
function __ZNK7b2Timer15GetMillisecondsEv(i1) {
 i1 = i1 | 0;
 return 0.0;
}
function __ZN25b2PolygonAndCircleContactD1Ev(i1) {
 i1 = i1 | 0;
 return;
}
function __ZN24b2ChainAndPolygonContactD1Ev(i1) {
 i1 = i1 | 0;
 return;
}
function b9(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 abort(9);
 return 0;
}
function __ZN23b2EdgeAndPolygonContactD1Ev(i1) {
 i1 = i1 | 0;
 return;
}
function __ZN23b2ChainAndCircleContactD1Ev(i1) {
 i1 = i1 | 0;
 return;
}
function __ZN22b2EdgeAndCircleContactD1Ev(i1) {
 i1 = i1 | 0;
 return;
}
function dynCall_v(i1) {
 i1 = i1 | 0;
 FUNCTION_TABLE_v[i1 & 3]();
}
function __ZNKSt9bad_alloc4whatEv(i1) {
 i1 = i1 | 0;
 return 7688;
}
function ___cxa_pure_virtual__wrapper() {
 ___cxa_pure_virtual();
}
function __ZN17b2ContactListenerD1Ev(i1) {
 i1 = i1 | 0;
 return;
}
function __ZN16b2PolygonContactD1Ev(i1) {
 i1 = i1 | 0;
 return;
}
function __ZN15b2ContactFilterD1Ev(i1) {
 i1 = i1 | 0;
 return;
}
function __ZN15b2CircleContactD1Ev(i1) {
 i1 = i1 | 0;
 return;
}
function __ZN14b2PolygonShapeD1Ev(i1) {
 i1 = i1 | 0;
 return;
}
function b3(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 abort(3);
}
function runPostSets() {
 HEAP32[1932] = __ZTISt9exception;
}
function __ZN11b2EdgeShapeD1Ev(i1) {
 i1 = i1 | 0;
 return;
}
function __ZNSt9type_infoD2Ev(i1) {
 i1 = i1 | 0;
 return;
}
function __ZN7b2Timer5ResetEv(i1) {
 i1 = i1 | 0;
 return;
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
function __ZN9b2ContactD1Ev(i1) {
 i1 = i1 | 0;
 return;
}
function __ZN7b2TimerC2Ev(i1) {
 i1 = i1 | 0;
 return;
}
function b4(i1) {
 i1 = i1 | 0;
 abort(4);
 return 0;
}
function stackSave() {
 return STACKTOP | 0;
}
function b2(i1) {
 i1 = i1 | 0;
 abort(2);
}
function b6() {
 abort(6);
}

// EMSCRIPTEN_END_FUNCS
  var FUNCTION_TABLE_iiii = [b0,__ZNK11b2EdgeShape9TestPointERK11b2TransformRK6b2Vec2,__ZNK14b2PolygonShape9TestPointERK11b2TransformRK6b2Vec2,__ZN15b2ContactFilter13ShouldCollideEP9b2FixtureS1_,__ZNK10__cxxabiv117__class_type_info9can_catchEPKNS_16__shim_type_infoERPv,b0,b0,b0];
  var FUNCTION_TABLE_viiiii = [b1,__ZNK10__cxxabiv117__class_type_info16search_below_dstEPNS_19__dynamic_cast_infoEPKvib,__ZNK10__cxxabiv120__si_class_type_info16search_below_dstEPNS_19__dynamic_cast_infoEPKvib,b1];
  var FUNCTION_TABLE_vi = [b2,__ZN11b2EdgeShapeD1Ev,__ZN11b2EdgeShapeD0Ev,__ZN14b2PolygonShapeD1Ev,__ZN14b2PolygonShapeD0Ev,__ZN17b2ContactListenerD1Ev,__ZN17b2ContactListenerD0Ev,__ZN15b2ContactFilterD1Ev,__ZN15b2ContactFilterD0Ev,__ZN9b2ContactD1Ev,__ZN9b2ContactD0Ev,__ZN22b2EdgeAndCircleContactD1Ev,__ZN22b2EdgeAndCircleContactD0Ev,__ZN23b2EdgeAndPolygonContactD1Ev,__ZN23b2EdgeAndPolygonContactD0Ev,__ZN25b2PolygonAndCircleContactD1Ev,__ZN25b2PolygonAndCircleContactD0Ev,__ZN16b2PolygonContactD1Ev,__ZN16b2PolygonContactD0Ev,__ZN23b2ChainAndCircleContactD1Ev,__ZN23b2ChainAndCircleContactD0Ev,__ZN24b2ChainAndPolygonContactD1Ev,__ZN24b2ChainAndPolygonContactD0Ev,__ZN15b2CircleContactD1Ev,__ZN15b2CircleContactD0Ev,__ZN10__cxxabiv116__shim_type_infoD2Ev,__ZN10__cxxabiv117__class_type_infoD0Ev,__ZNK10__cxxabiv116__shim_type_info5noop1Ev,__ZNK10__cxxabiv116__shim_type_info5noop2Ev
  ,__ZN10__cxxabiv120__si_class_type_infoD0Ev,__ZNSt9bad_allocD2Ev,__ZNSt9bad_allocD0Ev];
  var FUNCTION_TABLE_vii = [b3,__ZN17b2ContactListener12BeginContactEP9b2Contact,__ZN17b2ContactListener10EndContactEP9b2Contact,__ZN15b2CircleContact7DestroyEP9b2ContactP16b2BlockAllocator,__ZN25b2PolygonAndCircleContact7DestroyEP9b2ContactP16b2BlockAllocator,__ZN16b2PolygonContact7DestroyEP9b2ContactP16b2BlockAllocator,__ZN22b2EdgeAndCircleContact7DestroyEP9b2ContactP16b2BlockAllocator,__ZN23b2EdgeAndPolygonContact7DestroyEP9b2ContactP16b2BlockAllocator,__ZN23b2ChainAndCircleContact7DestroyEP9b2ContactP16b2BlockAllocator,__ZN24b2ChainAndPolygonContact7DestroyEP9b2ContactP16b2BlockAllocator,b3,b3,b3,b3,b3,b3];
  var FUNCTION_TABLE_ii = [b4,__ZNK11b2EdgeShape13GetChildCountEv,__ZNK14b2PolygonShape13GetChildCountEv,__ZNKSt9bad_alloc4whatEv];
  var FUNCTION_TABLE_viii = [b5,__ZN17b2ContactListener8PreSolveEP9b2ContactPK10b2Manifold,__ZN17b2ContactListener9PostSolveEP9b2ContactPK16b2ContactImpulse,b5];
  var FUNCTION_TABLE_v = [b6,___cxa_pure_virtual__wrapper,__Z4iterv,b6];
  var FUNCTION_TABLE_viid = [b7,__ZNK11b2EdgeShape11ComputeMassEP10b2MassDataf,__ZNK14b2PolygonShape11ComputeMassEP10b2MassDataf,b7];
  var FUNCTION_TABLE_viiiiii = [b8,__ZNK10__cxxabiv117__class_type_info16search_above_dstEPNS_19__dynamic_cast_infoEPKvS4_ib,__ZNK10__cxxabiv120__si_class_type_info16search_above_dstEPNS_19__dynamic_cast_infoEPKvS4_ib,b8];
  var FUNCTION_TABLE_iii = [b9,__ZNK11b2EdgeShape5CloneEP16b2BlockAllocator,__ZNK14b2PolygonShape5CloneEP16b2BlockAllocator,__Z14b2PairLessThanRK6b2PairS1_];
  var FUNCTION_TABLE_iiiiii = [b10,__ZNK11b2EdgeShape7RayCastEP15b2RayCastOutputRK14b2RayCastInputRK11b2Transformi,__ZNK14b2PolygonShape7RayCastEP15b2RayCastOutputRK14b2RayCastInputRK11b2Transformi,__ZN15b2CircleContact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator,__ZN25b2PolygonAndCircleContact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator,__ZN16b2PolygonContact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator,__ZN22b2EdgeAndCircleContact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator,__ZN23b2EdgeAndPolygonContact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator,__ZN23b2ChainAndCircleContact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator,__ZN24b2ChainAndPolygonContact6CreateEP9b2FixtureiS1_iP16b2BlockAllocator,b10,b10,b10,b10,b10,b10];
  var FUNCTION_TABLE_viiii = [b11,__ZNK11b2EdgeShape11ComputeAABBEP6b2AABBRK11b2Transformi,__ZNK14b2PolygonShape11ComputeAABBEP6b2AABBRK11b2Transformi,__ZN22b2EdgeAndCircleContact8EvaluateEP10b2ManifoldRK11b2TransformS4_,__ZN23b2EdgeAndPolygonContact8EvaluateEP10b2ManifoldRK11b2TransformS4_,__ZN25b2PolygonAndCircleContact8EvaluateEP10b2ManifoldRK11b2TransformS4_,__ZN16b2PolygonContact8EvaluateEP10b2ManifoldRK11b2TransformS4_,__ZN23b2ChainAndCircleContact8EvaluateEP10b2ManifoldRK11b2TransformS4_,__ZN24b2ChainAndPolygonContact8EvaluateEP10b2ManifoldRK11b2TransformS4_,__ZN15b2CircleContact8EvaluateEP10b2ManifoldRK11b2TransformS4_,__ZNK10__cxxabiv117__class_type_info27has_unambiguous_public_baseEPNS_19__dynamic_cast_infoEPvi,__ZNK10__cxxabiv120__si_class_type_info27has_unambiguous_public_baseEPNS_19__dynamic_cast_infoEPvi,b11,b11,b11,b11];

  return { _strlen: _strlen, _free: _free, _main: _main, _memset: _memset, _malloc: _malloc, _memcpy: _memcpy, runPostSets: runPostSets, stackAlloc: stackAlloc, stackSave: stackSave, stackRestore: stackRestore, setThrew: setThrew, setTempRet0: setTempRet0, setTempRet1: setTempRet1, setTempRet2: setTempRet2, setTempRet3: setTempRet3, setTempRet4: setTempRet4, setTempRet5: setTempRet5, setTempRet6: setTempRet6, setTempRet7: setTempRet7, setTempRet8: setTempRet8, setTempRet9: setTempRet9, dynCall_iiii: dynCall_iiii, dynCall_viiiii: dynCall_viiiii, dynCall_vi: dynCall_vi, dynCall_vii: dynCall_vii, dynCall_ii: dynCall_ii, dynCall_viii: dynCall_viii, dynCall_v: dynCall_v, dynCall_viid: dynCall_viid, dynCall_viiiiii: dynCall_viiiiii, dynCall_iii: dynCall_iii, dynCall_iiiiii: dynCall_iiiiii, dynCall_viiii: dynCall_viiii };
})
// EMSCRIPTEN_END_ASM
({ "Math": Math, "Int8Array": Int8Array, "Int16Array": Int16Array, "Int32Array": Int32Array, "Uint8Array": Uint8Array, "Uint16Array": Uint16Array, "Uint32Array": Uint32Array, "Float32Array": Float32Array, "Float64Array": Float64Array }, { "abort": abort, "assert": assert, "asmPrintInt": asmPrintInt, "asmPrintFloat": asmPrintFloat, "min": Math_min, "invoke_iiii": invoke_iiii, "invoke_viiiii": invoke_viiiii, "invoke_vi": invoke_vi, "invoke_vii": invoke_vii, "invoke_ii": invoke_ii, "invoke_viii": invoke_viii, "invoke_v": invoke_v, "invoke_viid": invoke_viid, "invoke_viiiiii": invoke_viiiiii, "invoke_iii": invoke_iii, "invoke_iiiiii": invoke_iiiiii, "invoke_viiii": invoke_viiii, "___cxa_throw": ___cxa_throw, "_emscripten_run_script": _emscripten_run_script, "_cosf": _cosf, "_send": _send, "__ZSt9terminatev": __ZSt9terminatev, "__reallyNegative": __reallyNegative, "___cxa_is_number_type": ___cxa_is_number_type, "___assert_fail": ___assert_fail, "___cxa_allocate_exception": ___cxa_allocate_exception, "___cxa_find_matching_catch": ___cxa_find_matching_catch, "_fflush": _fflush, "_pwrite": _pwrite, "___setErrNo": ___setErrNo, "_sbrk": _sbrk, "___cxa_begin_catch": ___cxa_begin_catch, "_sinf": _sinf, "_fileno": _fileno, "___resumeException": ___resumeException, "__ZSt18uncaught_exceptionv": __ZSt18uncaught_exceptionv, "_sysconf": _sysconf, "_clock": _clock, "_emscripten_memcpy_big": _emscripten_memcpy_big, "_puts": _puts, "_mkport": _mkport, "_floorf": _floorf, "_sqrtf": _sqrtf, "_write": _write, "_emscripten_set_main_loop": _emscripten_set_main_loop, "___errno_location": ___errno_location, "__ZNSt9exceptionD2Ev": __ZNSt9exceptionD2Ev, "_printf": _printf, "___cxa_does_inherit": ___cxa_does_inherit, "__exit": __exit, "_fputc": _fputc, "_abort": _abort, "_fwrite": _fwrite, "_time": _time, "_fprintf": _fprintf, "_emscripten_cancel_main_loop": _emscripten_cancel_main_loop, "__formatString": __formatString, "_fputs": _fputs, "_exit": _exit, "___cxa_pure_virtual": ___cxa_pure_virtual, "STACKTOP": STACKTOP, "STACK_MAX": STACK_MAX, "tempDoublePtr": tempDoublePtr, "ABORT": ABORT, "NaN": NaN, "Infinity": Infinity, "__ZTISt9exception": __ZTISt9exception }, buffer);
assertTrue(%IsAsmWasmCode(ModuleFunc));
var _strlen = Module["_strlen"] = asm["_strlen"];
var _free = Module["_free"] = asm["_free"];
var _main = Module["_main"] = asm["_main"];
var _memset = Module["_memset"] = asm["_memset"];
var _malloc = Module["_malloc"] = asm["_malloc"];
var _memcpy = Module["_memcpy"] = asm["_memcpy"];
var runPostSets = Module["runPostSets"] = asm["runPostSets"];
var dynCall_iiii = Module["dynCall_iiii"] = asm["dynCall_iiii"];
var dynCall_viiiii = Module["dynCall_viiiii"] = asm["dynCall_viiiii"];
var dynCall_vi = Module["dynCall_vi"] = asm["dynCall_vi"];
var dynCall_vii = Module["dynCall_vii"] = asm["dynCall_vii"];
var dynCall_ii = Module["dynCall_ii"] = asm["dynCall_ii"];
var dynCall_viii = Module["dynCall_viii"] = asm["dynCall_viii"];
var dynCall_v = Module["dynCall_v"] = asm["dynCall_v"];
var dynCall_viid = Module["dynCall_viid"] = asm["dynCall_viid"];
var dynCall_viiiiii = Module["dynCall_viiiiii"] = asm["dynCall_viiiiii"];
var dynCall_iii = Module["dynCall_iii"] = asm["dynCall_iii"];
var dynCall_iiiiii = Module["dynCall_iiiiii"] = asm["dynCall_iiiiii"];
var dynCall_viiii = Module["dynCall_viiii"] = asm["dynCall_viiii"];

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
