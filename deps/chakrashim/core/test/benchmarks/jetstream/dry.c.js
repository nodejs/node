/*
==============================================================================
LLVM Release License
==============================================================================
University of Illinois/NCSA
Open Source License

Copyright (c) 2003, 2004, 2005 University of Illinois at Urbana-Champaign.
All rights reserved.

Developed by:

    LLVM Team

    University of Illinois at Urbana-Champaign

    http://llvm.org/

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal with
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

    * Redistributions of source code must retain the above copyright notice,
        this list of conditions and the following disclaimers.

    * Redistributions in binary form must reproduce the above copyright notice,
        this list of conditions and the following disclaimers in the
        documentation and/or other materials provided with the distribution.

    * Neither the names of the LLVM Team, University of Illinois at
        Urbana-Champaign, nor the names of its contributors may be used to
        endorse or promote products derived from this Software without specific
        prior written permission.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE
SOFTWARE.

Copyright (C) 2014 Apple Inc. All rights reserved.
        
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
        
THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  
*/

var referenceScore = 1699;

if (typeof (WScript) === "undefined") {
    var WScript = {
        Echo: print
    }
}
var print = function () {};

var performance = performance || {};
performance.now = (function () {
    return performance.now ||
        performance.mozNow ||
        performance.msNow ||
        performance.oNow ||
        performance.webkitNow ||
        Date.now;
})();


function reportResult(score) {
    WScript.Echo("### SCORE: " + (100 * referenceScore / score));
}

var top = {};
top.JetStream = {
    goodTime: performance.now,
    reportResult: reportResult
};

var __time_before = top.JetStream.goodTime();

////////////////////////////////////////////////////////////////////////////////
// Test
////////////////////////////////////////////////////////////////////////////////
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
if (!Module) Module = eval('(function() { try { return Module || {} } catch(e) { return {} } })()');

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

    Module['readBinary'] = function readBinary(filename) {
        return Module['read'](filename, true)
    };

    Module['load'] = function load(f) {
        globalEval(read(f));
    };

    Module['arguments'] = process['argv'].slice(2);

    module['exports'] = Module;
} else if (ENVIRONMENT_IS_SHELL) {
    if (!Module['print']) Module['print'] = print;
    if (typeof printErr != 'undefined') Module['printErr'] = printErr; // not present in v8 or older sm

    if (typeof read != 'undefined') {
        Module['read'] = read;
    } else {
        Module['read'] = function read() {
            throw 'no read() available (jsc?)'
        };
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
} else if (ENVIRONMENT_IS_WEB || ENVIRONMENT_IS_WORKER) {
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
        if (!Module['print']) Module['print'] = (TRY_USE_DUMP && (typeof (dump) !== "undefined") ? (function (x) {
            dump(x);
        }) : (function (x) {
            // self.postMessage(x); // enable this if you want stdout to be sent as messages
        }));
    }

    if (ENVIRONMENT_IS_WEB) {
        this['Module'] = Module;
    } else {
        Module['load'] = importScripts;
    }
} else {
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
    Module['print'] = function () {};
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
            return Math.ceil(target / quantum) * quantum;
        } else if (isNumber(quantum) && isPowerOfTwo(quantum)) {
            return '(((' + target + ')+' + (quantum - 1) + ')&' + -quantum + ')';
        }
        return 'Math.ceil((' + target + ')/' + quantum + ')*' + quantum;
    },
    isNumberType: function (type) {
        return type in Runtime.INT_TYPES || type in Runtime.FLOAT_TYPES;
    },
    isPointerType: function isPointerType(type) {
        return type[type.length - 1] == '*';
    },
    isStructType: function isStructType(type) {
        if (isPointerType(type)) return false;
        if (isArrayType(type)) return true;
        if (/<?\{ ?[^}]* ?\}>?/.test(type)) return true; // { i32, i8 } etc. - anonymous struct types
        // See comment in isStructPointerType()
        return type[0] == '%';
    },
    INT_TYPES: {
        "i1": 0,
        "i8": 0,
        "i16": 0,
        "i32": 0,
        "i64": 0
    },
    FLOAT_TYPES: {
        "float": 0,
        "double": 0
    },
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
        case 'i1':
        case 'i8':
            return 1;
        case 'i16':
            return 2;
        case 'i32':
            return 4;
        case 'i64':
            return 8;
        case 'float':
            return 4;
        case 'double':
            return 8;
        default:
            {
                if (type[type.length - 1] === '*') {
                    return Runtime.QUANTUM_SIZE; // A pointer
                } else if (type[0] === 'i') {
                    var bits = parseInt(type.substr(1));
                    assert(bits % 8 === 0);
                    return bits / 8;
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
            return items.filter(function (item) {
                if (seen[item[ident]]) return false;
                seen[item[ident]] = true;
                return true;
            });
        } else {
            return items.filter(function (item) {
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
        type.flatIndexes = type.fields.map(function (field) {
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
                size = field.substr(1) | 0;
                alignSize = 1;
            } else if (field[0] === '<') {
                // vector type
                size = alignSize = Types.types[field].flatSize; // fully aligned
            } else if (field[0] === 'i') {
                // illegal integer field, that could not be legalized because it is an internal structure field
                // it is ok to have such fields, if we just use them as markers of field size and nothing more complex
                size = alignSize = parseInt(field.substr(1)) / 8;
                assert(size % 1 === 0, 'cannot handle non-byte-size field ' + field);
            } else {
                assert(false, 'invalid type for calculateStructAlignment');
            }
            if (type.packed) alignSize = 1;
            type.alignSize = Math.max(type.alignSize, alignSize);
            var curr = Runtime.alignMemory(type.flatSize, alignSize); // if necessary, place this on aligned memory
            type.flatSize = curr + size;
            if (prev >= 0) {
                diffs.push(curr - prev);
            }
            prev = curr;
            return curr;
        });
        if (type.name_ && type.name_[0] === '[') {
            // arrays have 2 elements, so we get the proper difference. then we scale here. that way we avoid
            // allocating a potentially huge array for [999999 x i8] etc.
            type.flatSize = parseInt(type.name_.substr(1)) * type.flatSize / 2;
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
            var type = {
                fields: struct.map(function (item) {
                    return item[0]
                })
            };
            alignment = Runtime.calculateStructAlignment(type);
        }
        var ret = {
            __size__: type.flatSize
        };
        if (typeName) {
            struct.forEach(function (item, i) {
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
            struct.forEach(function (item, i) {
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
                return 2 * (1 + i);
            }
        }
        throw 'Finished up all reserved function pointers. Use a higher value for RESERVED_FUNCTION_POINTERS.';
    },
    removeFunction: function (index) {
        Runtime.functionPointers[(index - 2) / 2] = null;
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
        code = Pointer_stringify(code);
        if (code[0] === '"') {
            // tolerate EM_ASM("..code..") even though EM_ASM(..code..) is correct
            if (code.indexOf('"', 1) === code.length - 1) {
                code = code.substr(1, code.length - 2);
            } else {
                // something invalid happened, e.g. EM_ASM("..code($0)..", input)
                abort('invalid EM_ASM input |' + code + '|. Please use EM_ASM(..code..) (no quotes) or EM_ASM({ ..code($0).. }, input) (to input values)');
            }
        }
        return Runtime.asmConstCache[code] = eval('(function(' + args.join(',') + '){ ' + code + ' })'); // new Function does not allow upvars in node
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
                if ((code & 0x80) == 0x00) { // 0xxxxxxx
                    return String.fromCharCode(code);
                }
                buffer.push(code);
                if ((code & 0xE0) == 0xC0) { // 110xxxxx
                    needed = 1;
                } else if ((code & 0xF0) == 0xE0) { // 1110xxxx
                    needed = 2;
                } else { // 11110xxx
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
                ret = String.fromCharCode(((c1 & 0x1F) << 6) | (c2 & 0x3F));
            } else if (buffer.length == 3) {
                ret = String.fromCharCode(((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F));
            } else {
                // http://mathiasbynens.be/notes/javascript-encoding#surrogate-formulae
                var codePoint = ((c1 & 0x07) << 18) | ((c2 & 0x3F) << 12) |
                    ((c3 & 0x3F) << 6) | (c4 & 0x3F);
                ret = String.fromCharCode(
                    Math.floor((codePoint - 0x10000) / 0x400) + 0xD800, (codePoint - 0x10000) % 0x400 + 0xDC00);
            }
            buffer.length = 0;
            return ret;
        }
        this.processJSString = function processJSString(string) {
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
    stackAlloc: function (size) {
        var ret = STACKTOP;
        STACKTOP = (STACKTOP + size) | 0;
        STACKTOP = (((STACKTOP) + 7) & -8);
        return ret;
    },
    staticAlloc: function (size) {
        var ret = STATICTOP;
        STATICTOP = (STATICTOP + size) | 0;
        STATICTOP = (((STATICTOP) + 7) & -8);
        return ret;
    },
    dynamicAlloc: function (size) {
        var ret = DYNAMICTOP;
        DYNAMICTOP = (DYNAMICTOP + size) | 0;
        DYNAMICTOP = (((DYNAMICTOP) + 7) & -8);
        if (DYNAMICTOP >= TOTAL_MEMORY) enlargeMemory();;
        return ret;
    },
    alignMemory: function (size, quantum) {
        var ret = size = Math.ceil((size) / (quantum ? quantum : 8)) * (quantum ? quantum : 8);
        return ret;
    },
    makeBigInt: function (low, high, unsigned) {
        var ret = (unsigned ? ((+((low >>> 0))) + ((+((high >>> 0))) * (+4294967296))) : ((+((low >>> 0))) + ((+((high | 0))) * (+4294967296))));
        return ret;
    },
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
    } catch (e) {}
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
    var cArgs = args ? args.map(function (arg) {
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
    return function () {
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
    if (type.charAt(type.length - 1) === '*') type = 'i32'; // pointers are 32-bit
    switch (type) {
    case 'i1':
        HEAP8[(ptr)] = value;
        break;
    case 'i8':
        HEAP8[(ptr)] = value;
        break;
    case 'i16':
        HEAP16[((ptr) >> 1)] = value;
        break;
    case 'i32':
        HEAP32[((ptr) >> 2)] = value;
        break;
    case 'i64':
        (tempI64 = [value >>> 0, (tempDouble = value, (+(Math_abs(tempDouble))) >= (+1) ? (tempDouble > (+0) ? ((Math_min((+(Math_floor((tempDouble) / (+4294967296)))), (+4294967295))) | 0) >>> 0 : (~~((+(Math_ceil((tempDouble - +(((~~(tempDouble))) >>> 0)) / (+4294967296)))))) >>> 0) : 0)], HEAP32[((ptr) >> 2)] = tempI64[0], HEAP32[(((ptr) + (4)) >> 2)] = tempI64[1]);
        break;
    case 'float':
        HEAPF32[((ptr) >> 2)] = value;
        break;
    case 'double':
        HEAPF64[((ptr) >> 3)] = value;
        break;
    default:
        abort('invalid type for setValue: ' + type);
    }
}
Module['setValue'] = setValue;

// Parallel to setValue.
function getValue(ptr, type, noSafe) {
    type = type || 'i8';
    if (type.charAt(type.length - 1) === '*') type = 'i32'; // pointers are 32-bit
    switch (type) {
    case 'i1':
        return HEAP8[(ptr)];
    case 'i8':
        return HEAP8[(ptr)];
    case 'i16':
        return HEAP16[((ptr) >> 1)];
    case 'i32':
        return HEAP32[((ptr) >> 2)];
    case 'i64':
        return HEAP32[((ptr) >> 2)];
    case 'float':
        return HEAPF32[((ptr) >> 2)];
    case 'double':
        return HEAPF64[((ptr) >> 3)];
    default:
        abort('invalid type for setValue: ' + type);
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
        var ptr = ret,
            stop;
        assert((ret & 3) == 0);
        stop = ret + (size & ~3);
        for (; ptr < stop; ptr += 4) {
            HEAP32[((ptr) >> 2)] = 0;
        }
        stop = ret + size;
        while (ptr < stop) {
            HEAP8[((ptr++) | 0)] = 0;
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

    var i = 0,
        type, typeSize, previousType;
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

        setValue(ret + i, curr, type);

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
        t = HEAPU8[(((ptr) + (i)) | 0)];
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
        t = HEAPU8[(((ptr) + (i)) | 0)];
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
        var codeUnit = HEAP16[(((ptr) + (i * 2)) >> 1)];
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
    for (var i = 0; i < str.length; ++i) {
        // charCodeAt returns a UTF-16 encoded code unit, so it can be directly written to the HEAP.
        var codeUnit = str.charCodeAt(i); // possibly a lead surrogate
        HEAP16[(((outPtr) + (i * 2)) >> 1)] = codeUnit;
    }
    // Null-terminate the pointer to the HEAP.
    HEAP16[(((outPtr) + (str.length * 2)) >> 1)] = 0;
}
Module['stringToUTF16'] = stringToUTF16;

// Given a pointer 'ptr' to a null-terminated UTF32LE-encoded string in the emscripten HEAP, returns
// a copy of that string as a Javascript String object.
function UTF32ToString(ptr) {
    var i = 0;

    var str = '';
    while (1) {
        var utf32 = HEAP32[(((ptr) + (i * 4)) >> 2)];
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
    for (var iCodeUnit = 0; iCodeUnit < str.length; ++iCodeUnit) {
        // Gotcha: charCodeAt returns a 16-bit word that is a UTF-16 encoded code unit, not a Unicode code point of the character! We must decode the string to UTF-32 to the heap.
        var codeUnit = str.charCodeAt(iCodeUnit); // possibly a lead surrogate
        if (codeUnit >= 0xD800 && codeUnit <= 0xDFFF) {
            var trailSurrogate = str.charCodeAt(++iCodeUnit);
            codeUnit = 0x10000 + ((codeUnit & 0x3FF) << 10) | (trailSurrogate & 0x3FF);
        }
        HEAP32[(((outPtr) + (iChar * 4)) >> 2)] = codeUnit;
        ++iChar;
    }
    // Null-terminate the pointer to the HEAP.
    HEAP32[(((outPtr) + (iChar * 4)) >> 2)] = 0;
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
        Module.print(pre + '^');
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
                i = next + 1;
                continue;
            }
            if (func[i] === 'C') { // constructor
                parts.push(parts[parts.length - 1]);
                i += 2;
                continue;
            }
            var size = parseInt(func.substr(i));
            var pre = size.toString().length;
            if (!size || !pre) {
                i--;
                break;
            } // counter i++ below us
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
        var ret = '',
            list = [];

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
                case 'P':
                    list.push(parse(true, 1, true)[0] + '*');
                    break; // pointer
                case 'R':
                    list.push(parse(true, 1, true)[0] + '&');
                    break; // reference
                case 'L':
                    { // literal
                        i++; // skip basic type
                        var end = func.indexOf('E', i);
                        var size = end - i;
                        list.push(func.substr(i, size));
                        i += size + 2; // size + 'EE'
                        break;
                    }
                case 'A':
                    { // array
                        var size = parseInt(func.substr(i));
                        i += size.toString().length;
                        if (func[i] !== '_') throw '?';
                        i++; // skip _
                        list.push(parse(true, 1, true)[0] + ' [' + size + ']');
                        break;
                    }
                case 'E':
                    break paramLoop;
                default:
                    ret += '?' + c;
                    break paramLoop;
                }
            }
        }
        if (!allowVoid && list.length === 1 && list[0] === 'void') list = []; // avoid (void)
        return rawList ? list : ret + flushList();
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
        case 'n':
            return 'operator new()';
        case 'd':
            return 'operator delete()';
        }
        return parse();
    } catch (e) {
        return func;
    }
}

function demangleAll(text) {
    return text.replace(/__Z[\w\d_]+/g, function (x) {
        var y = demangle(x);
        return x === y ? x : (x + ' [' + y + ']')
    });
}

function stackTrace() {
    var stack = new Error().stack;
    return stack ? demangleAll(stack) : '(no stack trace available)'; // Stack trace is not available at least on IE10 and Safari 6.
}

// Memory management

var PAGE_SIZE = 4096;

function alignMemoryPage(x) {
    return (x + 4095) & -4096;
}

var HEAP;
var HEAP8, HEAPU8, HEAP16, HEAPU16, HEAP32, HEAPU32, HEAPF32, HEAPF64;

var STATIC_BASE = 0,
    STATICTOP = 0,
    staticSealed = false; // static area
var STACK_BASE = 0,
    STACKTOP = 0,
    STACK_MAX = 0; // stack area
var DYNAMIC_BASE = 0,
    DYNAMICTOP = 0; // dynamic area handled by sbrk

function enlargeMemory() {
    abort('Cannot enlarge memory arrays. Either (1) compile with -s TOTAL_MEMORY=X with X higher than the current value ' + TOTAL_MEMORY + ', (2) compile with ALLOW_MEMORY_GROWTH which adjusts the size at runtime but prevents some optimizations, or (3) set Module.TOTAL_MEMORY before the program runs.');
}

var TOTAL_STACK = Module['TOTAL_STACK'] || 5242880;
var TOTAL_MEMORY = Module['TOTAL_MEMORY'] || 16777216;
var FAST_MEMORY = Module['FAST_MEMORY'] || 2097152;

var totalMemory = 4096;
while (totalMemory < TOTAL_MEMORY || totalMemory < 2 * TOTAL_STACK) {
    if (totalMemory < 16 * 1024 * 1024) {
        totalMemory *= 2;
    } else {
        totalMemory += 16 * 1024 * 1024
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
    while (callbacks.length > 0) {
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

var __ATPRERUN__ = []; // functions called before the runtime is initialized
var __ATINIT__ = []; // functions called during startup
var __ATMAIN__ = []; // functions called when main() is to be run
var __ATEXIT__ = []; // functions called during shutdown
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
function intArrayFromString(stringy, dontAddNull, length /* optional */ ) {
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
        HEAP8[(((buffer) + (i)) | 0)] = chr;
        i = i + 1;
    }
}
Module['writeStringToMemory'] = writeStringToMemory;

function writeArrayToMemory(array, buffer) {
    for (var i = 0; i < array.length; i++) {
        HEAP8[(((buffer) + (i)) | 0)] = array[i];
    }
}
Module['writeArrayToMemory'] = writeArrayToMemory;

function writeAsciiToMemory(str, buffer, dontAddNull) {
    for (var i = 0; i < str.length; i++) {
        HEAP8[(((buffer) + (i)) | 0)] = str.charCodeAt(i);
    }
    if (!dontAddNull) HEAP8[(((buffer) + (str.length)) | 0)] = 0;
}
Module['writeAsciiToMemory'] = writeAsciiToMemory;

function unSign(value, bits, ignore) {
    if (value >= 0) {
        return value;
    }
    return bits <= 32 ? 2 * Math.abs(1 << (bits - 1)) + value // Need some trickery, since if bits == 32, we are right at the limit of the bits JS uses in bitshifts
        : Math.pow(2, bits) + value;
}

function reSign(value, bits, ignore) {
    if (value <= 0) {
        return value;
    }
    var half = bits <= 32 ? Math.abs(1 << (bits - 1)) // abs is needed if bits == 32
        : Math.pow(2, bits - 1);
    if (value >= half && (bits <= 32 || value > half)) { // for huge values, we can hit the precision limit and always get true here. so don't do that
        // but, in general there is no perfect solution here. With 64-bit ints, we get rounding and errors
        // TODO: In i64 mode 1, resign the two parts separately and safely
        value = -2 * half + value; // Cannot bitshift half, as it may be at the limit of the bits JS uses in bitshifts
    }
    return value;
}

// check for imul support, and also for correctness ( https://bugs.webkit.org/show_bug.cgi?id=126345 )
if (!Math['imul'] || Math['imul'](0xffffffff, 5) !== -5) Math['imul'] = function imul(a, b) {
    var ah = a >>> 16;
    var al = a & 0xffff;
    var bh = b >>> 16;
    var bl = b & 0xffff;
    return (al * bl + ((ah * bl + al * bh) << 16)) | 0;
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

STATICTOP = STATIC_BASE + Runtime.alignMemory(11195);
/* global initializers */
__ATINIT__.push();


/* memory initializer */
allocate([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 68, 72, 82, 89, 83, 84, 79, 78, 69, 32, 80, 82, 79, 71, 82, 65, 77, 44, 32, 83, 79, 77, 69, 32, 83, 84, 82, 73, 78, 71], "i8", ALLOC_NONE, Runtime.GLOBAL_BASE);




var tempDoublePtr = Runtime.alignMemory(allocate(12, "i8", ALLOC_STATIC), 8);

assert(tempDoublePtr % 8 == 0);

function copyTempFloat(ptr) { // functions, because inlining this code increases code size too much

    HEAP8[tempDoublePtr] = HEAP8[ptr];

    HEAP8[tempDoublePtr + 1] = HEAP8[ptr + 1];

    HEAP8[tempDoublePtr + 2] = HEAP8[ptr + 2];

    HEAP8[tempDoublePtr + 3] = HEAP8[ptr + 3];

}

function copyTempDouble(ptr) {

    HEAP8[tempDoublePtr] = HEAP8[ptr];

    HEAP8[tempDoublePtr + 1] = HEAP8[ptr + 1];

    HEAP8[tempDoublePtr + 2] = HEAP8[ptr + 2];

    HEAP8[tempDoublePtr + 3] = HEAP8[ptr + 3];

    HEAP8[tempDoublePtr + 4] = HEAP8[ptr + 4];

    HEAP8[tempDoublePtr + 5] = HEAP8[ptr + 5];

    HEAP8[tempDoublePtr + 6] = HEAP8[ptr + 6];

    HEAP8[tempDoublePtr + 7] = HEAP8[ptr + 7];

}




function _emscripten_memcpy_big(dest, src, num) {
    HEAPU8.set(HEAPU8.subarray(src, src + num), dest);
    return dest;
}
Module["_memcpy"] = _memcpy;
var _llvm_memcpy_p0i8_p0i8_i32 = _memcpy;

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
        Runtime.dynamicAlloc = function () {
            abort('cannot dynamically allocate, sbrk now has control')
        };
    }
    var ret = DYNAMICTOP;
    if (bytes != 0) self.alloc(bytes);
    return ret; // Previous break location.
}



var ___errno_state = 0;

function ___setErrNo(value) {
    // For convenient setting and returning of errno.
    HEAP32[((___errno_state) >> 2)] = value;
    return value;
}

var ERRNO_CODES = {
    EPERM: 1,
    ENOENT: 2,
    ESRCH: 3,
    EINTR: 4,
    EIO: 5,
    ENXIO: 6,
    E2BIG: 7,
    ENOEXEC: 8,
    EBADF: 9,
    ECHILD: 10,
    EAGAIN: 11,
    EWOULDBLOCK: 11,
    ENOMEM: 12,
    EACCES: 13,
    EFAULT: 14,
    ENOTBLK: 15,
    EBUSY: 16,
    EEXIST: 17,
    EXDEV: 18,
    ENODEV: 19,
    ENOTDIR: 20,
    EISDIR: 21,
    EINVAL: 22,
    ENFILE: 23,
    EMFILE: 24,
    ENOTTY: 25,
    ETXTBSY: 26,
    EFBIG: 27,
    ENOSPC: 28,
    ESPIPE: 29,
    EROFS: 30,
    EMLINK: 31,
    EPIPE: 32,
    EDOM: 33,
    ERANGE: 34,
    ENOMSG: 42,
    EIDRM: 43,
    ECHRNG: 44,
    EL2NSYNC: 45,
    EL3HLT: 46,
    EL3RST: 47,
    ELNRNG: 48,
    EUNATCH: 49,
    ENOCSI: 50,
    EL2HLT: 51,
    EDEADLK: 35,
    ENOLCK: 37,
    EBADE: 52,
    EBADR: 53,
    EXFULL: 54,
    ENOANO: 55,
    EBADRQC: 56,
    EBADSLT: 57,
    EDEADLOCK: 35,
    EBFONT: 59,
    ENOSTR: 60,
    ENODATA: 61,
    ETIME: 62,
    ENOSR: 63,
    ENONET: 64,
    ENOPKG: 65,
    EREMOTE: 66,
    ENOLINK: 67,
    EADV: 68,
    ESRMNT: 69,
    ECOMM: 70,
    EPROTO: 71,
    EMULTIHOP: 72,
    EDOTDOT: 73,
    EBADMSG: 74,
    ENOTUNIQ: 76,
    EBADFD: 77,
    EREMCHG: 78,
    ELIBACC: 79,
    ELIBBAD: 80,
    ELIBSCN: 81,
    ELIBMAX: 82,
    ELIBEXEC: 83,
    ENOSYS: 38,
    ENOTEMPTY: 39,
    ENAMETOOLONG: 36,
    ELOOP: 40,
    EOPNOTSUPP: 95,
    EPFNOSUPPORT: 96,
    ECONNRESET: 104,
    ENOBUFS: 105,
    EAFNOSUPPORT: 97,
    EPROTOTYPE: 91,
    ENOTSOCK: 88,
    ENOPROTOOPT: 92,
    ESHUTDOWN: 108,
    ECONNREFUSED: 111,
    EADDRINUSE: 98,
    ECONNABORTED: 103,
    ENETUNREACH: 101,
    ENETDOWN: 100,
    ETIMEDOUT: 110,
    EHOSTDOWN: 112,
    EHOSTUNREACH: 113,
    EINPROGRESS: 115,
    EALREADY: 114,
    EDESTADDRREQ: 89,
    EMSGSIZE: 90,
    EPROTONOSUPPORT: 93,
    ESOCKTNOSUPPORT: 94,
    EADDRNOTAVAIL: 99,
    ENETRESET: 102,
    EISCONN: 106,
    ENOTCONN: 107,
    ETOOMANYREFS: 109,
    EUSERS: 87,
    EDQUOT: 122,
    ESTALE: 116,
    ENOTSUP: 95,
    ENOMEDIUM: 123,
    EILSEQ: 84,
    EOVERFLOW: 75,
    ECANCELED: 125,
    ENOTRECOVERABLE: 131,
    EOWNERDEAD: 130,
    ESTRPIPE: 86
};

function _sysconf(name) {
    // long sysconf(int name);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/sysconf.html
    switch (name) {
    case 30:
        return PAGE_SIZE;
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
    case 0:
        return 2097152;
    case 3:
        return 65536;
    case 28:
        return 32768;
    case 44:
        return 32767;
    case 75:
        return 16384;
    case 39:
        return 1000;
    case 89:
        return 700;
    case 71:
        return 256;
    case 40:
        return 255;
    case 2:
        return 100;
    case 180:
        return 64;
    case 25:
        return 20;
    case 5:
        return 16;
    case 6:
        return 6;
    case 73:
        return 4;
    case 84:
        return 1;
    }
    ___setErrNo(ERRNO_CODES.EINVAL);
    return -1;
}

function _clock() {
    if (_clock.start === undefined) _clock.start = Date.now();
    return Math.floor((Date.now() - _clock.start) * (1000000 / 1000));
}


Module["_memset"] = _memset;

function ___errno_location() {
    return ___errno_state;
}

function _abort() {
    Module['abort']();
}




var ERRNO_MESSAGES = {
    0: "Success",
    1: "Not super-user",
    2: "No such file or directory",
    3: "No such process",
    4: "Interrupted system call",
    5: "I/O error",
    6: "No such device or address",
    7: "Arg list too long",
    8: "Exec format error",
    9: "Bad file number",
    10: "No children",
    11: "No more processes",
    12: "Not enough core",
    13: "Permission denied",
    14: "Bad address",
    15: "Block device required",
    16: "Mount device busy",
    17: "File exists",
    18: "Cross-device link",
    19: "No such device",
    20: "Not a directory",
    21: "Is a directory",
    22: "Invalid argument",
    23: "Too many open files in system",
    24: "Too many open files",
    25: "Not a typewriter",
    26: "Text file busy",
    27: "File too large",
    28: "No space left on device",
    29: "Illegal seek",
    30: "Read only file system",
    31: "Too many links",
    32: "Broken pipe",
    33: "Math arg out of domain of func",
    34: "Math result not representable",
    35: "File locking deadlock error",
    36: "File or path name too long",
    37: "No record locks available",
    38: "Function not implemented",
    39: "Directory not empty",
    40: "Too many symbolic links",
    42: "No message of desired type",
    43: "Identifier removed",
    44: "Channel number out of range",
    45: "Level 2 not synchronized",
    46: "Level 3 halted",
    47: "Level 3 reset",
    48: "Link number out of range",
    49: "Protocol driver not attached",
    50: "No CSI structure available",
    51: "Level 2 halted",
    52: "Invalid exchange",
    53: "Invalid request descriptor",
    54: "Exchange full",
    55: "No anode",
    56: "Invalid request code",
    57: "Invalid slot",
    59: "Bad font file fmt",
    60: "Device not a stream",
    61: "No data (for no delay io)",
    62: "Timer expired",
    63: "Out of streams resources",
    64: "Machine is not on the network",
    65: "Package not installed",
    66: "The object is remote",
    67: "The link has been severed",
    68: "Advertise error",
    69: "Srmount error",
    70: "Communication error on send",
    71: "Protocol error",
    72: "Multihop attempted",
    73: "Cross mount point (not really error)",
    74: "Trying to read unreadable message",
    75: "Value too large for defined data type",
    76: "Given log. name not unique",
    77: "f.d. invalid for this operation",
    78: "Remote address changed",
    79: "Can   access a needed shared lib",
    80: "Accessing a corrupted shared lib",
    81: ".lib section in a.out corrupted",
    82: "Attempting to link in too many libs",
    83: "Attempting to exec a shared library",
    84: "Illegal byte sequence",
    86: "Streams pipe error",
    87: "Too many users",
    88: "Socket operation on non-socket",
    89: "Destination address required",
    90: "Message too long",
    91: "Protocol wrong type for socket",
    92: "Protocol not available",
    93: "Unknown protocol",
    94: "Socket type not supported",
    95: "Not supported",
    96: "Protocol family not supported",
    97: "Address family not supported by protocol family",
    98: "Address already in use",
    99: "Address not available",
    100: "Network interface is not configured",
    101: "Network is unreachable",
    102: "Connection reset by network",
    103: "Connection aborted",
    104: "Connection reset by peer",
    105: "No buffer space available",
    106: "Socket is already connected",
    107: "Socket is not connected",
    108: "Can't send after socket shutdown",
    109: "Too many references",
    110: "Connection timed out",
    111: "Connection refused",
    112: "Host is down",
    113: "Host is unreachable",
    114: "Socket already connected",
    115: "Connection already in progress",
    116: "Stale file handle",
    122: "Quota exceeded",
    123: "No medium (in tape drive)",
    125: "Operation canceled",
    130: "Previous owner died",
    131: "State not recoverable"
};

var TTY = {
    ttys: [],
    init: function () {
        // https://github.com/kripken/emscripten/pull/1555
        // if (ENVIRONMENT_IS_NODE) {
        //   // currently, FS.init does not distinguish if process.stdin is a file or TTY
        //   // device, it always assumes it's a TTY device. because of this, we're forcing
        //   // process.stdin to UTF8 encoding to at least make stdin reading compatible
        //   // with text files until FS.init can be refactored.
        //   process['stdin']['setEncoding']('utf8');
        // }
    },
    shutdown: function () {
        // https://github.com/kripken/emscripten/pull/1555
        // if (ENVIRONMENT_IS_NODE) {
        //   // inolen: any idea as to why node -e 'process.stdin.read()' wouldn't exit immediately (with process.stdin being a tty)?
        //   // isaacs: because now it's reading from the stream, you've expressed interest in it, so that read() kicks off a _read() which creates a ReadReq operation
        //   // inolen: I thought read() in that case was a synchronous operation that just grabbed some amount of buffered data if it exists?
        //   // isaacs: it is. but it also triggers a _read() call, which calls readStart() on the handle
        //   // isaacs: do process.stdin.pause() and i'd think it'd probably close the pending call
        //   process['stdin']['pause']();
        // }
    },
    register: function (dev, ops) {
        TTY.ttys[dev] = {
            input: [],
            output: [],
            ops: ops
        };
        FS.registerDevice(dev, TTY.stream_ops);
    },
    stream_ops: {
        open: function (stream) {
            var tty = TTY.ttys[stream.node.rdev];
            if (!tty) {
                throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
            }
            stream.tty = tty;
            stream.seekable = false;
        },
        close: function (stream) {
            // flush any pending line data
            if (stream.tty.output.length) {
                stream.tty.ops.put_char(stream.tty, 10);
            }
        },
        read: function (stream, buffer, offset, length, pos /* ignored */ ) {
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
                buffer[offset + i] = result;
            }
            if (bytesRead) {
                stream.node.timestamp = Date.now();
            }
            return bytesRead;
        },
        write: function (stream, buffer, offset, length, pos) {
            if (!stream.tty || !stream.tty.ops.put_char) {
                throw new FS.ErrnoError(ERRNO_CODES.ENXIO);
            }
            for (var i = 0; i < length; i++) {
                try {
                    stream.tty.ops.put_char(stream.tty, buffer[offset + i]);
                } catch (e) {
                    throw new FS.ErrnoError(ERRNO_CODES.EIO);
                }
            }
            if (length) {
                stream.node.timestamp = Date.now();
            }
            return i;
        }
    },
    default_tty_ops: {
        get_char: function (tty) {
            if (!tty.input.length) {
                var result = null;
                if (ENVIRONMENT_IS_NODE) {
                    result = process['stdin']['read']();
                    if (!result) {
                        if (process['stdin']['_readableState'] && process['stdin']['_readableState']['ended']) {
                            return null; // EOF
                        }
                        return undefined; // no data available
                    }
                } else if (typeof window != 'undefined' &&
                    typeof window.prompt == 'function') {
                    // Browser.
                    result = window.prompt('Input: '); // returns null on cancel
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
        },
        put_char: function (tty, val) {
            if (val === null || val === 10) {
                Module['print'](tty.output.join(''));
                tty.output = [];
            } else {
                tty.output.push(TTY.utf8.processCChar(val));
            }
        }
    },
    default_tty1_ops: {
        put_char: function (tty, val) {
            if (val === null || val === 10) {
                Module['printErr'](tty.output.join(''));
                tty.output = [];
            } else {
                tty.output.push(TTY.utf8.processCChar(val));
            }
        }
    }
};

var MEMFS = {
    ops_table: null,
    CONTENT_OWNING: 1,
    CONTENT_FLEXIBLE: 2,
    CONTENT_FIXED: 3,
    mount: function (mount) {
        return MEMFS.createNode(null, '/', 16384 | 511 /* 0777 */ , 0);
    },
    createNode: function (parent, name, mode, dev) {
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
    },
    ensureFlexible: function (node) {
        if (node.contentMode !== MEMFS.CONTENT_FLEXIBLE) {
            var contents = node.contents;
            node.contents = Array.prototype.slice.call(contents);
            node.contentMode = MEMFS.CONTENT_FLEXIBLE;
        }
    },
    node_ops: {
        getattr: function (node) {
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
        },
        setattr: function (node, attr) {
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
                else
                    while (attr.size > contents.length) contents.push(0);
            }
        },
        lookup: function (parent, name) {
            throw FS.genericErrors[ERRNO_CODES.ENOENT];
        },
        mknod: function (parent, name, mode, dev) {
            return MEMFS.createNode(parent, name, mode, dev);
        },
        rename: function (old_node, new_dir, new_name) {
            // if we're overwriting a directory at new_name, make sure it's empty.
            if (FS.isDir(old_node.mode)) {
                var new_node;
                try {
                    new_node = FS.lookupNode(new_dir, new_name);
                } catch (e) {}
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
        },
        unlink: function (parent, name) {
            delete parent.contents[name];
        },
        rmdir: function (parent, name) {
            var node = FS.lookupNode(parent, name);
            for (var i in node.contents) {
                throw new FS.ErrnoError(ERRNO_CODES.ENOTEMPTY);
            }
            delete parent.contents[name];
        },
        readdir: function (node) {
            var entries = ['.', '..']
            for (var key in node.contents) {
                if (!node.contents.hasOwnProperty(key)) {
                    continue;
                }
                entries.push(key);
            }
            return entries;
        },
        symlink: function (parent, newname, oldpath) {
            var node = MEMFS.createNode(parent, newname, 511 /* 0777 */ | 40960, 0);
            node.link = oldpath;
            return node;
        },
        readlink: function (node) {
            if (!FS.isLink(node.mode)) {
                throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
            }
            return node.link;
        }
    },
    stream_ops: {
        read: function (stream, buffer, offset, length, position) {
            var contents = stream.node.contents;
            if (position >= contents.length)
                return 0;
            var size = Math.min(contents.length - position, length);
            assert(size >= 0);
            if (size > 8 && contents.subarray) { // non-trivial, and typed array
                buffer.set(contents.subarray(position, position + size), offset);
            } else {
                for (var i = 0; i < size; i++) {
                    buffer[offset + i] = contents[position + i];
                }
            }
            return size;
        },
        write: function (stream, buffer, offset, length, position, canOwn) {
            var node = stream.node;
            node.timestamp = Date.now();
            var contents = node.contents;
            if (length && contents.length === 0 && position === 0 && buffer.subarray) {
                // just replace it with the new data
                if (canOwn && offset === 0) {
                    node.contents = buffer; // this could be a subarray of Emscripten HEAP, or allocated from some other source.
                    node.contentMode = (buffer.buffer === HEAP8.buffer) ? MEMFS.CONTENT_OWNING : MEMFS.CONTENT_FIXED;
                } else {
                    node.contents = new Uint8Array(buffer.subarray(offset, offset + length));
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
        },
        llseek: function (stream, offset, whence) {
            var position = offset;
            if (whence === 1) { // SEEK_CUR.
                position += stream.position;
            } else if (whence === 2) { // SEEK_END.
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
        },
        allocate: function (stream, offset, length) {
            MEMFS.ensureFlexible(stream.node);
            var contents = stream.node.contents;
            var limit = offset + length;
            while (limit > contents.length) contents.push(0);
        },
        mmap: function (stream, buffer, offset, length, position, prot, flags) {
            if (!FS.isFile(stream.node.mode)) {
                throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
            }
            var ptr;
            var allocated;
            var contents = stream.node.contents;
            // Only make a new copy when MAP_PRIVATE is specified.
            if (!(flags & 2) &&
                (contents.buffer === buffer || contents.buffer === buffer.buffer)) {
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
            return {
                ptr: ptr,
                allocated: allocated
            };
        }
    }
};

var IDBFS = {
    dbs: {},
    indexedDB: function () {
        return window.indexedDB || window.mozIndexedDB || window.webkitIndexedDB || window.msIndexedDB;
    },
    DB_VERSION: 21,
    DB_STORE_NAME: "FILE_DATA",
    mount: function (mount) {
        // reuse all of the core MEMFS functionality
        return MEMFS.mount.apply(null, arguments);
    },
    syncfs: function (mount, populate, callback) {
        IDBFS.getLocalSet(mount, function (err, local) {
            if (err) return callback(err);

            IDBFS.getRemoteSet(mount, function (err, remote) {
                if (err) return callback(err);

                var src = populate ? remote : local;
                var dst = populate ? local : remote;

                IDBFS.reconcile(src, dst, callback);
            });
        });
    },
    getDB: function (name, callback) {
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
        req.onupgradeneeded = function (e) {
            var db = e.target.result;
            var transaction = e.target.transaction;

            var fileStore;

            if (db.objectStoreNames.contains(IDBFS.DB_STORE_NAME)) {
                fileStore = transaction.objectStore(IDBFS.DB_STORE_NAME);
            } else {
                fileStore = db.createObjectStore(IDBFS.DB_STORE_NAME);
            }

            fileStore.createIndex('timestamp', 'timestamp', {
                unique: false
            });
        };
        req.onsuccess = function () {
            db = req.result;

            // add to the cache
            IDBFS.dbs[name] = db;
            callback(null, db);
        };
        req.onerror = function () {
            callback(this.error);
        };
    },
    getLocalSet: function (mount, callback) {
        var entries = {};

        function isRealDir(p) {
            return p !== '.' && p !== '..';
        };

        function toAbsolute(root) {
            return function (p) {
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

            entries[path] = {
                timestamp: stat.mtime
            };
        }

        return callback(null, {
            type: 'local',
            entries: entries
        });
    },
    getRemoteSet: function (mount, callback) {
        var entries = {};

        IDBFS.getDB(mount.mountpoint, function (err, db) {
            if (err) return callback(err);

            var transaction = db.transaction([IDBFS.DB_STORE_NAME], 'readonly');
            transaction.onerror = function () {
                callback(this.error);
            };

            var store = transaction.objectStore(IDBFS.DB_STORE_NAME);
            var index = store.index('timestamp');

            index.openKeyCursor().onsuccess = function (event) {
                var cursor = event.target.result;

                if (!cursor) {
                    return callback(null, {
                        type: 'remote',
                        db: db,
                        entries: entries
                    });
                }

                entries[cursor.primaryKey] = {
                    timestamp: cursor.key
                };

                cursor.continue();
            };
        });
    },
    loadLocalEntry: function (path, callback) {
        var stat, node;

        try {
            var lookup = FS.lookupPath(path);
            node = lookup.node;
            stat = FS.stat(path);
        } catch (e) {
            return callback(e);
        }

        if (FS.isDir(stat.mode)) {
            return callback(null, {
                timestamp: stat.mtime,
                mode: stat.mode
            });
        } else if (FS.isFile(stat.mode)) {
            return callback(null, {
                timestamp: stat.mtime,
                mode: stat.mode,
                contents: node.contents
            });
        } else {
            return callback(new Error('node type not supported'));
        }
    },
    storeLocalEntry: function (path, entry, callback) {
        try {
            if (FS.isDir(entry.mode)) {
                FS.mkdir(path, entry.mode);
            } else if (FS.isFile(entry.mode)) {
                FS.writeFile(path, entry.contents, {
                    encoding: 'binary',
                    canOwn: true
                });
            } else {
                return callback(new Error('node type not supported'));
            }

            FS.utime(path, entry.timestamp, entry.timestamp);
        } catch (e) {
            return callback(e);
        }

        callback(null);
    },
    removeLocalEntry: function (path, callback) {
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
    },
    loadRemoteEntry: function (store, path, callback) {
        var req = store.get(path);
        req.onsuccess = function (event) {
            callback(null, event.target.result);
        };
        req.onerror = function () {
            callback(this.error);
        };
    },
    storeRemoteEntry: function (store, path, entry, callback) {
        var req = store.put(entry, path);
        req.onsuccess = function () {
            callback(null);
        };
        req.onerror = function () {
            callback(this.error);
        };
    },
    removeRemoteEntry: function (store, path, callback) {
        var req = store.delete(path);
        req.onsuccess = function () {
            callback(null);
        };
        req.onerror = function () {
            callback(this.error);
        };
    },
    reconcile: function (src, dst, callback) {
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

        transaction.onerror = function () {
            done(this.error);
        };

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
        remove.sort().reverse().forEach(function (path) {
            if (dst.type === 'local') {
                IDBFS.removeLocalEntry(path, done);
            } else {
                IDBFS.removeRemoteEntry(store, path, done);
            }
        });
    }
};

var NODEFS = {
    isWindows: false,
    staticInit: function () {
        NODEFS.isWindows = !!process.platform.match(/^win/);
    },
    mount: function (mount) {
        assert(ENVIRONMENT_IS_NODE);
        return NODEFS.createNode(null, '/', NODEFS.getMode(mount.opts.root), 0);
    },
    createNode: function (parent, name, mode, dev) {
        if (!FS.isDir(mode) && !FS.isFile(mode) && !FS.isLink(mode)) {
            throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        var node = FS.createNode(parent, name, mode);
        node.node_ops = NODEFS.node_ops;
        node.stream_ops = NODEFS.stream_ops;
        return node;
    },
    getMode: function (path) {
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
    },
    realPath: function (node) {
        var parts = [];
        while (node.parent !== node) {
            parts.push(node.name);
            node = node.parent;
        }
        parts.push(node.mount.opts.root);
        parts.reverse();
        return PATH.join.apply(null, parts);
    },
    flagsToPermissionStringMap: {
        0: "r",
        1: "r+",
        2: "r+",
        64: "r",
        65: "r+",
        66: "r+",
        129: "rx+",
        193: "rx+",
        514: "w+",
        577: "w",
        578: "w+",
        705: "wx",
        706: "wx+",
        1024: "a",
        1025: "a",
        1026: "a+",
        1089: "a",
        1090: "a+",
        1153: "ax",
        1154: "ax+",
        1217: "ax",
        1218: "ax+",
        4096: "rs",
        4098: "rs+"
    },
    flagsToPermissionString: function (flags) {
        if (flags in NODEFS.flagsToPermissionStringMap) {
            return NODEFS.flagsToPermissionStringMap[flags];
        } else {
            return flags;
        }
    },
    node_ops: {
        getattr: function (node) {
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
                stat.blocks = (stat.size + stat.blksize - 1) / stat.blksize | 0;
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
        },
        setattr: function (node, attr) {
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
        },
        lookup: function (parent, name) {
            var path = PATH.join2(NODEFS.realPath(parent), name);
            var mode = NODEFS.getMode(path);
            return NODEFS.createNode(parent, name, mode);
        },
        mknod: function (parent, name, mode, dev) {
            var node = NODEFS.createNode(parent, name, mode, dev);
            // create the backing node for this in the fs root as well
            var path = NODEFS.realPath(node);
            try {
                if (FS.isDir(node.mode)) {
                    fs.mkdirSync(path, node.mode);
                } else {
                    fs.writeFileSync(path, '', {
                        mode: node.mode
                    });
                }
            } catch (e) {
                if (!e.code) throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
            return node;
        },
        rename: function (oldNode, newDir, newName) {
            var oldPath = NODEFS.realPath(oldNode);
            var newPath = PATH.join2(NODEFS.realPath(newDir), newName);
            try {
                fs.renameSync(oldPath, newPath);
            } catch (e) {
                if (!e.code) throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        },
        unlink: function (parent, name) {
            var path = PATH.join2(NODEFS.realPath(parent), name);
            try {
                fs.unlinkSync(path);
            } catch (e) {
                if (!e.code) throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        },
        rmdir: function (parent, name) {
            var path = PATH.join2(NODEFS.realPath(parent), name);
            try {
                fs.rmdirSync(path);
            } catch (e) {
                if (!e.code) throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        },
        readdir: function (node) {
            var path = NODEFS.realPath(node);
            try {
                return fs.readdirSync(path);
            } catch (e) {
                if (!e.code) throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        },
        symlink: function (parent, newName, oldPath) {
            var newPath = PATH.join2(NODEFS.realPath(parent), newName);
            try {
                fs.symlinkSync(oldPath, newPath);
            } catch (e) {
                if (!e.code) throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        },
        readlink: function (node) {
            var path = NODEFS.realPath(node);
            try {
                return fs.readlinkSync(path);
            } catch (e) {
                if (!e.code) throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        }
    },
    stream_ops: {
        open: function (stream) {
            var path = NODEFS.realPath(stream.node);
            try {
                if (FS.isFile(stream.node.mode)) {
                    stream.nfd = fs.openSync(path, NODEFS.flagsToPermissionString(stream.flags));
                }
            } catch (e) {
                if (!e.code) throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        },
        close: function (stream) {
            try {
                if (FS.isFile(stream.node.mode) && stream.nfd) {
                    fs.closeSync(stream.nfd);
                }
            } catch (e) {
                if (!e.code) throw e;
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
        },
        read: function (stream, buffer, offset, length, position) {
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
        },
        write: function (stream, buffer, offset, length, position) {
            // FIXME this is terrible.
            var nbuffer = new Buffer(buffer.subarray(offset, offset + length));
            var res;
            try {
                res = fs.writeSync(stream.nfd, nbuffer, 0, length, position);
            } catch (e) {
                throw new FS.ErrnoError(ERRNO_CODES[e.code]);
            }
            return res;
        },
        llseek: function (stream, offset, whence) {
            var position = offset;
            if (whence === 1) { // SEEK_CUR.
                position += stream.position;
            } else if (whence === 2) { // SEEK_END.
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
        }
    }
};

var _stdin = allocate(1, "i32*", ALLOC_STATIC);

var _stdout = allocate(1, "i32*", ALLOC_STATIC);

var _stderr = allocate(1, "i32*", ALLOC_STATIC);

function _fflush(stream) {
    // int fflush(FILE *stream);
    // http://pubs.opengroup.org/onlinepubs/000095399/functions/fflush.html
    // we don't currently perform any user-space buffering of data
}
var FS = {
    root: null,
    mounts: [],
    devices: [null],
    streams: [],
    nextInode: 1,
    nameTable: null,
    currentPath: "/",
    initialized: false,
    ignorePermissions: true,
    ErrnoError: null,
    genericErrors: {},
    handleFSError: function (e) {
        if (!(e instanceof FS.ErrnoError)) throw e + ' : ' + stackTrace();
        return ___setErrNo(e.errno);
    },
    lookupPath: function (path, opts) {
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

        if (opts.recurse_count > 8) { // max recursive lookup of 8
            throw new FS.ErrnoError(ERRNO_CODES.ELOOP);
        }

        // split the path
        var parts = PATH.normalizeArray(path.split('/').filter(function (p) {
            return !!p;
        }), false);

        // start at the root
        var current = FS.root;
        var current_path = '/';

        for (var i = 0; i < parts.length; i++) {
            var islast = (i === parts.length - 1);
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

                    var lookup = FS.lookupPath(current_path, {
                        recurse_count: opts.recurse_count
                    });
                    current = lookup.node;

                    if (count++ > 40) { // limit max consecutive symlinks to 40 (SYMLOOP_MAX).
                        throw new FS.ErrnoError(ERRNO_CODES.ELOOP);
                    }
                }
            }
        }

        return {
            path: current_path,
            node: current
        };
    },
    getPath: function (node) {
        var path;
        while (true) {
            if (FS.isRoot(node)) {
                var mount = node.mount.mountpoint;
                if (!path) return mount;
                return mount[mount.length - 1] !== '/' ? mount + '/' + path : mount + path;
            }
            path = path ? node.name + '/' + path : node.name;
            node = node.parent;
        }
    },
    hashName: function (parentid, name) {
        var hash = 0;


        for (var i = 0; i < name.length; i++) {
            hash = ((hash << 5) - hash + name.charCodeAt(i)) | 0;
        }
        return ((parentid + hash) >>> 0) % FS.nameTable.length;
    },
    hashAddNode: function (node) {
        var hash = FS.hashName(node.parent.id, node.name);
        node.name_next = FS.nameTable[hash];
        FS.nameTable[hash] = node;
    },
    hashRemoveNode: function (node) {
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
    },
    lookupNode: function (parent, name) {
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
    },
    createNode: function (parent, name, mode, rdev) {
        if (!FS.FSNode) {
            FS.FSNode = function (parent, name, mode, rdev) {
                if (!parent) {
                    parent = this; // root node sets parent to itself
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
                    get: function () {
                        return (this.mode & readMode) === readMode;
                    },
                    set: function (val) {
                        val ? this.mode |= readMode : this.mode &= ~readMode;
                    }
                },
                write: {
                    get: function () {
                        return (this.mode & writeMode) === writeMode;
                    },
                    set: function (val) {
                        val ? this.mode |= writeMode : this.mode &= ~writeMode;
                    }
                },
                isFolder: {
                    get: function () {
                        return FS.isDir(this.mode);
                    },
                },
                isDevice: {
                    get: function () {
                        return FS.isChrdev(this.mode);
                    },
                },
            });
        }

        var node = new FS.FSNode(parent, name, mode, rdev);

        FS.hashAddNode(node);

        return node;
    },
    destroyNode: function (node) {
        FS.hashRemoveNode(node);
    },
    isRoot: function (node) {
        return node === node.parent;
    },
    isMountpoint: function (node) {
        return !!node.mounted;
    },
    isFile: function (mode) {
        return (mode & 61440) === 32768;
    },
    isDir: function (mode) {
        return (mode & 61440) === 16384;
    },
    isLink: function (mode) {
        return (mode & 61440) === 40960;
    },
    isChrdev: function (mode) {
        return (mode & 61440) === 8192;
    },
    isBlkdev: function (mode) {
        return (mode & 61440) === 24576;
    },
    isFIFO: function (mode) {
        return (mode & 61440) === 4096;
    },
    isSocket: function (mode) {
        return (mode & 49152) === 49152;
    },
    flagModes: {
        "r": 0,
        "rs": 1052672,
        "r+": 2,
        "w": 577,
        "wx": 705,
        "xw": 705,
        "w+": 578,
        "wx+": 706,
        "xw+": 706,
        "a": 1089,
        "ax": 1217,
        "xa": 1217,
        "a+": 1090,
        "ax+": 1218,
        "xa+": 1218
    },
    modeStringToFlags: function (str) {
        var flags = FS.flagModes[str];
        if (typeof flags === 'undefined') {
            throw new Error('Unknown file open mode: ' + str);
        }
        return flags;
    },
    flagsToPermissionString: function (flag) {
        var accmode = flag & 2097155;
        var perms = ['r', 'w', 'rw'][accmode];
        if ((flag & 512)) {
            perms += 'w';
        }
        return perms;
    },
    nodePermissions: function (node, perms) {
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
    },
    mayLookup: function (dir) {
        return FS.nodePermissions(dir, 'x');
    },
    mayCreate: function (dir, name) {
        try {
            var node = FS.lookupNode(dir, name);
            return ERRNO_CODES.EEXIST;
        } catch (e) {}
        return FS.nodePermissions(dir, 'wx');
    },
    mayDelete: function (dir, name, isdir) {
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
    },
    mayOpen: function (node, flags) {
        if (!node) {
            return ERRNO_CODES.ENOENT;
        }
        if (FS.isLink(node.mode)) {
            return ERRNO_CODES.ELOOP;
        } else if (FS.isDir(node.mode)) {
            if ((flags & 2097155) !== 0 || // opening for write
                (flags & 512)) {
                return ERRNO_CODES.EISDIR;
            }
        }
        return FS.nodePermissions(node, FS.flagsToPermissionString(flags));
    },
    MAX_OPEN_FDS: 4096,
    nextfd: function (fd_start, fd_end) {
        fd_start = fd_start || 0;
        fd_end = fd_end || FS.MAX_OPEN_FDS;
        for (var fd = fd_start; fd <= fd_end; fd++) {
            if (!FS.streams[fd]) {
                return fd;
            }
        }
        throw new FS.ErrnoError(ERRNO_CODES.EMFILE);
    },
    getStream: function (fd) {
        return FS.streams[fd];
    },
    createStream: function (stream, fd_start, fd_end) {
        if (!FS.FSStream) {
            FS.FSStream = function () {};
            FS.FSStream.prototype = {};
            // compatibility
            Object.defineProperties(FS.FSStream.prototype, {
                object: {
                    get: function () {
                        return this.node;
                    },
                    set: function (val) {
                        this.node = val;
                    }
                },
                isRead: {
                    get: function () {
                        return (this.flags & 2097155) !== 1;
                    }
                },
                isWrite: {
                    get: function () {
                        return (this.flags & 2097155) !== 0;
                    }
                },
                isAppend: {
                    get: function () {
                        return (this.flags & 1024);
                    }
                }
            });
        }
        if (stream.__proto__) {
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
    },
    closeStream: function (fd) {
        FS.streams[fd] = null;
    },
    getStreamFromPtr: function (ptr) {
        return FS.streams[ptr - 1];
    },
    getPtrForStream: function (stream) {
        return stream ? stream.fd + 1 : 0;
    },
    chrdev_stream_ops: {
        open: function (stream) {
            var device = FS.getDevice(stream.node.rdev);
            // override node's stream ops with the device's
            stream.stream_ops = device.stream_ops;
            // forward the open call
            if (stream.stream_ops.open) {
                stream.stream_ops.open(stream);
            }
        },
        llseek: function () {
            throw new FS.ErrnoError(ERRNO_CODES.ESPIPE);
        }
    },
    major: function (dev) {
        return ((dev) >> 8);
    },
    minor: function (dev) {
        return ((dev) & 0xff);
    },
    makedev: function (ma, mi) {
        return ((ma) << 8 | (mi));
    },
    registerDevice: function (dev, ops) {
        FS.devices[dev] = {
            stream_ops: ops
        };
    },
    getDevice: function (dev) {
        return FS.devices[dev];
    },
    getMounts: function (mount) {
        var mounts = [];
        var check = [mount];

        while (check.length) {
            var m = check.pop();

            mounts.push(m);

            check.push.apply(check, m.mounts);
        }

        return mounts;
    },
    syncfs: function (populate, callback) {
        if (typeof (populate) === 'function') {
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
    },
    mount: function (type, opts, mountpoint) {
        var root = mountpoint === '/';
        var pseudo = !mountpoint;
        var node;

        if (root && FS.root) {
            throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
        } else if (!root && !pseudo) {
            var lookup = FS.lookupPath(mountpoint, {
                follow_mount: false
            });

            mountpoint = lookup.path; // use the absolute path
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
    },
    unmount: function (mountpoint) {
        var lookup = FS.lookupPath(mountpoint, {
            follow_mount: false
        });

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
    },
    lookup: function (parent, name) {
        return parent.node_ops.lookup(parent, name);
    },
    mknod: function (path, mode, dev) {
        var lookup = FS.lookupPath(path, {
            parent: true
        });
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
    },
    create: function (path, mode) {
        mode = mode !== undefined ? mode : 438 /* 0666 */ ;
        mode &= 4095;
        mode |= 32768;
        return FS.mknod(path, mode, 0);
    },
    mkdir: function (path, mode) {
        mode = mode !== undefined ? mode : 511 /* 0777 */ ;
        mode &= 511 | 512;
        mode |= 16384;
        return FS.mknod(path, mode, 0);
    },
    mkdev: function (path, mode, dev) {
        if (typeof (dev) === 'undefined') {
            dev = mode;
            mode = 438 /* 0666 */ ;
        }
        mode |= 8192;
        return FS.mknod(path, mode, dev);
    },
    symlink: function (oldpath, newpath) {
        var lookup = FS.lookupPath(newpath, {
            parent: true
        });
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
    },
    rename: function (old_path, new_path) {
        var old_dirname = PATH.dirname(old_path);
        var new_dirname = PATH.dirname(new_path);
        var old_name = PATH.basename(old_path);
        var new_name = PATH.basename(new_path);
        // parents must exist
        var lookup, old_dir, new_dir;
        try {
            lookup = FS.lookupPath(old_path, {
                parent: true
            });
            old_dir = lookup.node;
            lookup = FS.lookupPath(new_path, {
                parent: true
            });
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
    },
    rmdir: function (path) {
        var lookup = FS.lookupPath(path, {
            parent: true
        });
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
    },
    readdir: function (path) {
        var lookup = FS.lookupPath(path, {
            follow: true
        });
        var node = lookup.node;
        if (!node.node_ops.readdir) {
            throw new FS.ErrnoError(ERRNO_CODES.ENOTDIR);
        }
        return node.node_ops.readdir(node);
    },
    unlink: function (path) {
        var lookup = FS.lookupPath(path, {
            parent: true
        });
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
    },
    readlink: function (path) {
        var lookup = FS.lookupPath(path);
        var link = lookup.node;
        if (!link.node_ops.readlink) {
            throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        return link.node_ops.readlink(link);
    },
    stat: function (path, dontFollow) {
        var lookup = FS.lookupPath(path, {
            follow: !dontFollow
        });
        var node = lookup.node;
        if (!node.node_ops.getattr) {
            throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        return node.node_ops.getattr(node);
    },
    lstat: function (path) {
        return FS.stat(path, true);
    },
    chmod: function (path, mode, dontFollow) {
        var node;
        if (typeof path === 'string') {
            var lookup = FS.lookupPath(path, {
                follow: !dontFollow
            });
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
    },
    lchmod: function (path, mode) {
        FS.chmod(path, mode, true);
    },
    fchmod: function (fd, mode) {
        var stream = FS.getStream(fd);
        if (!stream) {
            throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        FS.chmod(stream.node, mode);
    },
    chown: function (path, uid, gid, dontFollow) {
        var node;
        if (typeof path === 'string') {
            var lookup = FS.lookupPath(path, {
                follow: !dontFollow
            });
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
    },
    lchown: function (path, uid, gid) {
        FS.chown(path, uid, gid, true);
    },
    fchown: function (fd, uid, gid) {
        var stream = FS.getStream(fd);
        if (!stream) {
            throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        FS.chown(stream.node, uid, gid);
    },
    truncate: function (path, len) {
        if (len < 0) {
            throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        var node;
        if (typeof path === 'string') {
            var lookup = FS.lookupPath(path, {
                follow: true
            });
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
    },
    ftruncate: function (fd, len) {
        var stream = FS.getStream(fd);
        if (!stream) {
            throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        if ((stream.flags & 2097155) === 0) {
            throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        FS.truncate(stream.node, len);
    },
    utime: function (path, atime, mtime) {
        var lookup = FS.lookupPath(path, {
            follow: true
        });
        var node = lookup.node;
        node.node_ops.setattr(node, {
            timestamp: Math.max(atime, mtime)
        });
    },
    open: function (path, flags, mode, fd_start, fd_end) {
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
            path: FS.getPath(node), // we want the absolute path to the node
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
    },
    close: function (stream) {
        try {
            if (stream.stream_ops.close) {
                stream.stream_ops.close(stream);
            }
        } catch (e) {
            throw e;
        } finally {
            FS.closeStream(stream.fd);
        }
    },
    llseek: function (stream, offset, whence) {
        if (!stream.seekable || !stream.stream_ops.llseek) {
            throw new FS.ErrnoError(ERRNO_CODES.ESPIPE);
        }
        return stream.stream_ops.llseek(stream, offset, whence);
    },
    read: function (stream, buffer, offset, length, position) {
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
    },
    write: function (stream, buffer, offset, length, position, canOwn) {
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
    },
    allocate: function (stream, offset, length) {
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
    },
    mmap: function (stream, buffer, offset, length, position, prot, flags) {
        // TODO if PROT is PROT_WRITE, make sure we have write access
        if ((stream.flags & 2097155) === 1) {
            throw new FS.ErrnoError(ERRNO_CODES.EACCES);
        }
        if (!stream.stream_ops.mmap) {
            throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
        }
        return stream.stream_ops.mmap(stream, buffer, offset, length, position, prot, flags);
    },
    ioctl: function (stream, cmd, arg) {
        if (!stream.stream_ops.ioctl) {
            throw new FS.ErrnoError(ERRNO_CODES.ENOTTY);
        }
        return stream.stream_ops.ioctl(stream, cmd, arg);
    },
    readFile: function (path, opts) {
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
    },
    writeFile: function (path, data, opts) {
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
    },
    cwd: function () {
        return FS.currentPath;
    },
    chdir: function (path) {
        var lookup = FS.lookupPath(path, {
            follow: true
        });
        if (!FS.isDir(lookup.node.mode)) {
            throw new FS.ErrnoError(ERRNO_CODES.ENOTDIR);
        }
        var err = FS.nodePermissions(lookup.node, 'x');
        if (err) {
            throw new FS.ErrnoError(err);
        }
        FS.currentPath = lookup.path;
    },
    createDefaultDirectories: function () {
        FS.mkdir('/tmp');
    },
    createDefaultDevices: function () {
        // create /dev
        FS.mkdir('/dev');
        // setup /dev/null
        FS.registerDevice(FS.makedev(1, 3), {
            read: function () {
                return 0;
            },
            write: function () {
                return 0;
            }
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
    },
    createStandardStreams: function () {
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
        HEAP32[((_stdin) >> 2)] = FS.getPtrForStream(stdin);
        assert(stdin.fd === 0, 'invalid handle for stdin (' + stdin.fd + ')');

        var stdout = FS.open('/dev/stdout', 'w');
        HEAP32[((_stdout) >> 2)] = FS.getPtrForStream(stdout);
        assert(stdout.fd === 1, 'invalid handle for stdout (' + stdout.fd + ')');

        var stderr = FS.open('/dev/stderr', 'w');
        HEAP32[((_stderr) >> 2)] = FS.getPtrForStream(stderr);
        assert(stderr.fd === 2, 'invalid handle for stderr (' + stderr.fd + ')');
    },
    ensureErrnoError: function () {
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
        [ERRNO_CODES.ENOENT].forEach(function (code) {
            FS.genericErrors[code] = new FS.ErrnoError(code);
            FS.genericErrors[code].stack = '<generic error, no stack>';
        });
    },
    staticInit: function () {
        FS.ensureErrnoError();

        FS.nameTable = new Array(4096);

        FS.mount(MEMFS, {}, '/');

        FS.createDefaultDirectories();
        FS.createDefaultDevices();
    },
    init: function (input, output, error) {
        assert(!FS.init.initialized, 'FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)');
        FS.init.initialized = true;

        FS.ensureErrnoError();

        // Allow Module.stdin etc. to provide defaults, if none explicitly passed to us here
        Module['stdin'] = input || Module['stdin'];
        Module['stdout'] = output || Module['stdout'];
        Module['stderr'] = error || Module['stderr'];

        FS.createStandardStreams();
    },
    quit: function () {
        FS.init.initialized = false;
        for (var i = 0; i < FS.streams.length; i++) {
            var stream = FS.streams[i];
            if (!stream) {
                continue;
            }
            FS.close(stream);
        }
    },
    getMode: function (canRead, canWrite) {
        var mode = 0;
        if (canRead) mode |= 292 | 73;
        if (canWrite) mode |= 146;
        return mode;
    },
    joinPath: function (parts, forceRelative) {
        var path = PATH.join.apply(null, parts);
        if (forceRelative && path[0] == '/') path = path.substr(1);
        return path;
    },
    absolutePath: function (relative, base) {
        return PATH.resolve(base, relative);
    },
    standardizePath: function (path) {
        return PATH.normalize(path);
    },
    findObject: function (path, dontResolveLastLink) {
        var ret = FS.analyzePath(path, dontResolveLastLink);
        if (ret.exists) {
            return ret.object;
        } else {
            ___setErrNo(ret.error);
            return null;
        }
    },
    analyzePath: function (path, dontResolveLastLink) {
        // operate from within the context of the symlink's target
        try {
            var lookup = FS.lookupPath(path, {
                follow: !dontResolveLastLink
            });
            path = lookup.path;
        } catch (e) {}
        var ret = {
            isRoot: false,
            exists: false,
            error: 0,
            name: null,
            path: null,
            object: null,
            parentExists: false,
            parentPath: null,
            parentObject: null
        };
        try {
            var lookup = FS.lookupPath(path, {
                parent: true
            });
            ret.parentExists = true;
            ret.parentPath = lookup.path;
            ret.parentObject = lookup.node;
            ret.name = PATH.basename(path);
            lookup = FS.lookupPath(path, {
                follow: !dontResolveLastLink
            });
            ret.exists = true;
            ret.path = lookup.path;
            ret.object = lookup.node;
            ret.name = lookup.node.name;
            ret.isRoot = lookup.path === '/';
        } catch (e) {
            ret.error = e.errno;
        };
        return ret;
    },
    createFolder: function (parent, name, canRead, canWrite) {
        var path = PATH.join2(typeof parent === 'string' ? parent : FS.getPath(parent), name);
        var mode = FS.getMode(canRead, canWrite);
        return FS.mkdir(path, mode);
    },
    createPath: function (parent, path, canRead, canWrite) {
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
    },
    createFile: function (parent, name, properties, canRead, canWrite) {
        var path = PATH.join2(typeof parent === 'string' ? parent : FS.getPath(parent), name);
        var mode = FS.getMode(canRead, canWrite);
        return FS.create(path, mode);
    },
    createDataFile: function (parent, name, data, canRead, canWrite, canOwn) {
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
    },
    createDevice: function (parent, name, input, output) {
        var path = PATH.join2(typeof parent === 'string' ? parent : FS.getPath(parent), name);
        var mode = FS.getMode(!!input, !!output);
        if (!FS.createDevice.major) FS.createDevice.major = 64;
        var dev = FS.makedev(FS.createDevice.major++, 0);
        // Create a fake device that a set of stream ops to emulate
        // the old behavior.
        FS.registerDevice(dev, {
            open: function (stream) {
                stream.seekable = false;
            },
            close: function (stream) {
                // flush any pending line data
                if (output && output.buffer && output.buffer.length) {
                    output(10);
                }
            },
            read: function (stream, buffer, offset, length, pos /* ignored */ ) {
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
                    buffer[offset + i] = result;
                }
                if (bytesRead) {
                    stream.node.timestamp = Date.now();
                }
                return bytesRead;
            },
            write: function (stream, buffer, offset, length, pos) {
                for (var i = 0; i < length; i++) {
                    try {
                        output(buffer[offset + i]);
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
    },
    createLink: function (parent, name, target, canRead, canWrite) {
        var path = PATH.join2(typeof parent === 'string' ? parent : FS.getPath(parent), name);
        return FS.symlink(target, path);
    },
    forceLoadFile: function (obj) {
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
    },
    createLazyFile: function (parent, name, url, canRead, canWrite) {
        if (typeof XMLHttpRequest !== 'undefined') {
            if (!ENVIRONMENT_IS_WORKER) throw 'Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc';
            // Lazy chunked Uint8Array (implements get and length from Uint8Array). Actual getting is abstracted away for eventual reuse.
            function LazyUint8Array() {
                this.lengthKnown = false;
                this.chunks = []; // Loaded chunks. Index is the chunk number
            }
            LazyUint8Array.prototype.get = function LazyUint8Array_get(idx) {
                if (idx > this.length - 1 || idx < 0) {
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
                var chunkSize = 1024 * 1024; // Chunk size in bytes

                if (!hasByteServing) chunkSize = datalength;

                // Function to get a range from the remote URL.
                var doXHR = (function (from, to) {
                    if (from > to) throw new Error("invalid range (" + from + ", " + to + ") or no bytes requested!");
                    if (to > datalength - 1) throw new Error("only " + datalength + " bytes available! programmer error!");

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
                lazyArray.setDataGetter(function (chunkNum) {
                    var start = chunkNum * chunkSize;
                    var end = (chunkNum + 1) * chunkSize - 1; // including this byte
                    end = Math.min(end, datalength - 1); // if datalength-1 is selected, this is the last block
                    if (typeof (lazyArray.chunks[chunkNum]) === "undefined") {
                        lazyArray.chunks[chunkNum] = doXHR(start, end);
                    }
                    if (typeof (lazyArray.chunks[chunkNum]) === "undefined") throw new Error("doXHR failed!");
                    return lazyArray.chunks[chunkNum];
                });

                this._length = datalength;
                this._chunkSize = chunkSize;
                this.lengthKnown = true;
            }

            var lazyArray = new LazyUint8Array();
            Object.defineProperty(lazyArray, "length", {
                get: function () {
                    if (!this.lengthKnown) {
                        this.cacheLength();
                    }
                    return this._length;
                }
            });
            Object.defineProperty(lazyArray, "chunkSize", {
                get: function () {
                    if (!this.lengthKnown) {
                        this.cacheLength();
                    }
                    return this._chunkSize;
                }
            });

            var properties = {
                isDevice: false,
                contents: lazyArray
            };
        } else {
            var properties = {
                isDevice: false,
                url: url
            };
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
        keys.forEach(function (key) {
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
    },
    createPreloadedFile: function (parent, name, url, canRead, canWrite, onload, onerror, dontCreateFile, canOwn) {
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
            Module['preloadPlugins'].forEach(function (plugin) {
                if (handled) return;
                if (plugin['canHandle'](fullname)) {
                    plugin['handle'](byteArray, fullname, finish, function () {
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
            Browser.asyncLoad(url, function (byteArray) {
                processData(byteArray);
            }, onerror);
        } else {
            processData(url);
        }
    },
    indexedDB: function () {
        return window.indexedDB || window.mozIndexedDB || window.webkitIndexedDB || window.msIndexedDB;
    },
    DB_NAME: function () {
        return 'EM_FS_' + window.location.pathname;
    },
    DB_VERSION: 20,
    DB_STORE_NAME: "FILE_DATA",
    saveFilesToDB: function (paths, onload, onerror) {
        onload = onload || function () {};
        onerror = onerror || function () {};
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
            var ok = 0,
                fail = 0,
                total = paths.length;

            function finish() {
                if (fail == 0) onload();
                else onerror();
            }
            paths.forEach(function (path) {
                var putRequest = files.put(FS.analyzePath(path).object.contents, path);
                putRequest.onsuccess = function putRequest_onsuccess() {
                    ok++;
                    if (ok + fail == total) finish()
                };
                putRequest.onerror = function putRequest_onerror() {
                    fail++;
                    if (ok + fail == total) finish()
                };
            });
            transaction.onerror = onerror;
        };
        openRequest.onerror = onerror;
    },
    loadFilesFromDB: function (paths, onload, onerror) {
        onload = onload || function () {};
        onerror = onerror || function () {};
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
            } catch (e) {
                onerror(e);
                return;
            }
            var files = transaction.objectStore(FS.DB_STORE_NAME);
            var ok = 0,
                fail = 0,
                total = paths.length;

            function finish() {
                if (fail == 0) onload();
                else onerror();
            }
            paths.forEach(function (path) {
                var getRequest = files.get(path);
                getRequest.onsuccess = function getRequest_onsuccess() {
                    if (FS.analyzePath(path).exists) {
                        FS.unlink(path);
                    }
                    FS.createDataFile(PATH.dirname(path), PATH.basename(path), getRequest.result, true, true, true);
                    ok++;
                    if (ok + fail == total) finish();
                };
                getRequest.onerror = function getRequest_onerror() {
                    fail++;
                    if (ok + fail == total) finish()
                };
            });
            transaction.onerror = onerror;
        };
        openRequest.onerror = onerror;
    }
};
var PATH = {
    splitPath: function (filename) {
        var splitPathRe = /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
        return splitPathRe.exec(filename).slice(1);
    },
    normalizeArray: function (parts, allowAboveRoot) {
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
    },
    normalize: function (path) {
        var isAbsolute = path.charAt(0) === '/',
            trailingSlash = path.substr(-1) === '/';
        // Normalize the path
        path = PATH.normalizeArray(path.split('/').filter(function (p) {
            return !!p;
        }), !isAbsolute).join('/');
        if (!path && !isAbsolute) {
            path = '.';
        }
        if (path && trailingSlash) {
            path += '/';
        }
        return (isAbsolute ? '/' : '') + path;
    },
    dirname: function (path) {
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
    },
    basename: function (path) {
        // EMSCRIPTEN return '/'' for '/', not an empty string
        if (path === '/') return '/';
        var lastSlash = path.lastIndexOf('/');
        if (lastSlash === -1) return path;
        return path.substr(lastSlash + 1);
    },
    extname: function (path) {
        return PATH.splitPath(path)[3];
    },
    join: function () {
        var paths = Array.prototype.slice.call(arguments, 0);
        return PATH.normalize(paths.join('/'));
    },
    join2: function (l, r) {
        return PATH.normalize(l + '/' + r);
    },
    resolve: function () {
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
        resolvedPath = PATH.normalizeArray(resolvedPath.split('/').filter(function (p) {
            return !!p;
        }), !resolvedAbsolute).join('/');
        return ((resolvedAbsolute ? '/' : '') + resolvedPath) || '.';
    },
    relative: function (from, to) {
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
    }
};
var Browser = {
    mainLoop: {
        scheduler: null,
        method: "",
        shouldPause: false,
        paused: false,
        queue: [],
        pause: function () {
            Browser.mainLoop.shouldPause = true;
        },
        resume: function () {
            if (Browser.mainLoop.paused) {
                Browser.mainLoop.paused = false;
                Browser.mainLoop.scheduler();
            }
            Browser.mainLoop.shouldPause = false;
        },
        updateStatus: function () {
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
        }
    },
    isFullScreen: false,
    pointerLock: false,
    moduleContextCreatedCallbacks: [],
    workers: [],
    init: function () {
        if (!Module["preloadPlugins"]) Module["preloadPlugins"] = []; // needs to exist even in workers

        if (Browser.initted || ENVIRONMENT_IS_WORKER) return;
        Browser.initted = true;

        try {
            new Blob();
            Browser.hasBlobConstructor = true;
        } catch (e) {
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
                    b = new Blob([byteArray], {
                        type: Browser.getMimetype(name)
                    });
                    if (b.size !== byteArray.length) { // Safari bug #118630
                        // Safari's Blob can only take an ArrayBuffer
                        b = new Blob([(new Uint8Array(byteArray)).buffer], {
                            type: Browser.getMimetype(name)
                        });
                    }
                } catch (e) {
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
            return !Module.noAudioDecoding && name.substr(-4) in {
                '.ogg': 1,
                '.wav': 1,
                '.mp3': 1
            };
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
                    var b = new Blob([byteArray], {
                        type: Browser.getMimetype(name)
                    });
                } catch (e) {
                    return fail();
                }
                var url = Browser.URLObject.createObjectURL(b); // XXX we never revoke this!
                var audio = new Audio();
                audio.addEventListener('canplaythrough', function () {
                    finish(audio)
                }, false); // use addEventListener due to chromium bug 124926
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
                                var curr = (leftchar >> (leftbits - 6)) & 0x3f;
                                leftbits -= 6;
                                ret += BASE[curr];
                            }
                        }
                        if (leftbits == 2) {
                            ret += BASE[(leftchar & 3) << 4];
                            ret += PAD + PAD;
                        } else if (leftbits == 4) {
                            ret += BASE[(leftchar & 0xf) << 2];
                            ret += PAD;
                        }
                        return ret;
                    }
                    audio.src = 'data:audio/x-' + name.substr(-3) + ';base64,' + encode64(byteArray);
                    finish(audio); // we don't wait for confirmation this worked - but it's worth trying
                };
                audio.src = url;
                // workaround for chrome bug 124926 - we do not always get oncanplaythrough or onerror
                Browser.safeSetTimeout(function () {
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
            function () {};
        canvas.exitPointerLock = document['exitPointerLock'] ||
            document['mozExitPointerLock'] ||
            document['webkitExitPointerLock'] ||
            document['msExitPointerLock'] ||
            function () {}; // no-op if function does not exist
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
            canvas.addEventListener("click", function (ev) {
                if (!Browser.pointerLock && canvas.requestPointerLock) {
                    canvas.requestPointerLock();
                    ev.preventDefault();
                }
            }, false);
        }
    },
    createContext: function (canvas, useWebGL, setInModule, webGLContextAttributes) {
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
                    ['experimental-webgl', 'webgl'].some(function (webglId) {
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
            canvas.addEventListener('webglcontextlost', function (event) {
                alert('WebGL context lost. You will need to reload the page.');
            }, false);
        }
        if (setInModule) {
            GLctx = Module.ctx = ctx;
            Module.useWebGL = useWebGL;
            Browser.moduleContextCreatedCallbacks.forEach(function (callback) {
                callback()
            });
            Browser.init();
        }
        return ctx;
    },
    destroyContext: function (canvas, useWebGL, setInModule) {},
    fullScreenHandlersInstalled: false,
    lockPointer: undefined,
    resizeCanvas: undefined,
    requestFullScreen: function (lockPointer, resizeCanvas) {
        Browser.lockPointer = lockPointer;
        Browser.resizeCanvas = resizeCanvas;
        if (typeof Browser.lockPointer === 'undefined') Browser.lockPointer = true;
        if (typeof Browser.resizeCanvas === 'undefined') Browser.resizeCanvas = false;

        var canvas = Module['canvas'];
        var canvasContainer = canvas.parentNode;

        function fullScreenChange() {
            Browser.isFullScreen = false;
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
                    function () {};
                canvas.cancelFullScreen = canvas.cancelFullScreen.bind(document);
                if (Browser.lockPointer) canvas.requestPointerLock();
                Browser.isFullScreen = true;
                if (Browser.resizeCanvas) Browser.setFullScreenCanvasSize();
            } else {

                // remove the full screen specific parent of the canvas again to restore the HTML structure from before going full screen
                var canvasContainer = canvas.parentNode;
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
            (canvasContainer['webkitRequestFullScreen'] ? function () {
            canvasContainer['webkitRequestFullScreen'](Element['ALLOW_KEYBOARD_INPUT'])
        } : null);
        canvasContainer.requestFullScreen();
    },
    requestAnimationFrame: function requestAnimationFrame(func) {
        if (typeof window === 'undefined') { // Provide fallback to setTimeout if window is undefined (e.g. in Node.js)
            setTimeout(func, 1000 / 60);
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
    },
    safeCallback: function (func) {
        return function () {
            if (!ABORT) return func.apply(null, arguments);
        };
    },
    safeRequestAnimationFrame: function (func) {
        return Browser.requestAnimationFrame(function () {
            if (!ABORT) func();
        });
    },
    safeSetTimeout: function (func, timeout) {
        return setTimeout(function () {
            if (!ABORT) func();
        }, timeout);
    },
    safeSetInterval: function (func, timeout) {
        return setInterval(function () {
            if (!ABORT) func();
        }, timeout);
    },
    getMimetype: function (name) {
        return {
            'jpg': 'image/jpeg',
            'jpeg': 'image/jpeg',
            'png': 'image/png',
            'bmp': 'image/bmp',
            'ogg': 'audio/ogg',
            'wav': 'audio/wav',
            'mp3': 'audio/mpeg'
        }[name.substr(name.lastIndexOf('.') + 1)];
    },
    getUserMedia: function (func) {
        if (!window.getUserMedia) {
            window.getUserMedia = navigator['getUserMedia'] ||
                navigator['mozGetUserMedia'];
        }
        window.getUserMedia(func);
    },
    getMovementX: function (event) {
        return event['movementX'] ||
            event['mozMovementX'] ||
            event['webkitMovementX'] ||
            0;
    },
    getMovementY: function (event) {
        return event['movementY'] ||
            event['mozMovementY'] ||
            event['webkitMovementY'] ||
            0;
    },
    getMouseWheelDelta: function (event) {
        return Math.max(-1, Math.min(1, event.type === 'DOMMouseScroll' ? event.detail : -event.wheelDelta));
    },
    mouseX: 0,
    mouseY: 0,
    mouseMovementX: 0,
    mouseMovementY: 0,
    calculateMouseEvent: function (event) { // event should be mousemove, mousedown or mouseup
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
    },
    xhrLoad: function (url, onload, onerror) {
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
    },
    asyncLoad: function (url, onload, onerror, noRunDep) {
        Browser.xhrLoad(url, function (arrayBuffer) {
            assert(arrayBuffer, 'Loading data file "' + url + '" failed (no arrayBuffer).');
            onload(new Uint8Array(arrayBuffer));
            if (!noRunDep) removeRunDependency('al ' + url);
        }, function (event) {
            if (onerror) {
                onerror();
            } else {
                throw 'Loading data file "' + url + '" failed.';
            }
        });
        if (!noRunDep) addRunDependency('al ' + url);
    },
    resizeListeners: [],
    updateResizeListeners: function () {
        var canvas = Module['canvas'];
        Browser.resizeListeners.forEach(function (listener) {
            listener(canvas.width, canvas.height);
        });
    },
    setCanvasSize: function (width, height, noUpdates) {
        var canvas = Module['canvas'];
        Browser.updateCanvasDimensions(canvas, width, height);
        if (!noUpdates) Browser.updateResizeListeners();
    },
    windowedWidth: 0,
    windowedHeight: 0,
    setFullScreenCanvasSize: function () {
        // check if SDL is available   
        if (typeof SDL != "undefined") {
            var flags = HEAPU32[((SDL.screen + Runtime.QUANTUM_SIZE * 0) >> 2)];
            flags = flags | 0x00800000; // set SDL_FULLSCREEN flag
            HEAP32[((SDL.screen + Runtime.QUANTUM_SIZE * 0) >> 2)] = flags
        }
        Browser.updateResizeListeners();
    },
    setWindowedCanvasSize: function () {
        // check if SDL is available       
        if (typeof SDL != "undefined") {
            var flags = HEAPU32[((SDL.screen + Runtime.QUANTUM_SIZE * 0) >> 2)];
            flags = flags & ~0x00800000; // clear SDL_FULLSCREEN flag
            HEAP32[((SDL.screen + Runtime.QUANTUM_SIZE * 0) >> 2)] = flags
        }
        Browser.updateResizeListeners();
    },
    updateCanvasDimensions: function (canvas, wNative, hNative) {
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
            if (w / h < Module['forcedAspectRatio']) {
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
            if (canvas.width != w) canvas.width = w;
            if (canvas.height != h) canvas.height = h;
            if (typeof canvas.style != 'undefined') {
                canvas.style.removeProperty("width");
                canvas.style.removeProperty("height");
            }
        } else {
            if (canvas.width != wNative) canvas.width = wNative;
            if (canvas.height != hNative) canvas.height = hNative;
            if (typeof canvas.style != 'undefined') {
                if (w != wNative || h != hNative) {
                    canvas.style.setProperty("width", w + "px", "important");
                    canvas.style.setProperty("height", h + "px", "important");
                } else {
                    canvas.style.removeProperty("width");
                    canvas.style.removeProperty("height");
                }
            }
        }
    }
};

function _time(ptr) {
    var ret = Math.floor(Date.now() / 1000);
    if (ptr) {
        HEAP32[((ptr) >> 2)] = ret;
    }
    return ret;
}


Module["_strlen"] = _strlen;

___errno_state = Runtime.staticAlloc(4);
HEAP32[((___errno_state) >> 2)] = 0;
Module["requestFullScreen"] = function Module_requestFullScreen(lockPointer, resizeCanvas) {
    Browser.requestFullScreen(lockPointer, resizeCanvas)
};
Module["requestAnimationFrame"] = function Module_requestAnimationFrame(func) {
    Browser.requestAnimationFrame(func)
};
Module["setCanvasSize"] = function Module_setCanvasSize(width, height, noUpdates) {
    Browser.setCanvasSize(width, height, noUpdates)
};
Module["pauseMainLoop"] = function Module_pauseMainLoop() {
    Browser.mainLoop.pause()
};
Module["resumeMainLoop"] = function Module_resumeMainLoop() {
    Browser.mainLoop.resume()
};
Module["getUserMedia"] = function Module_getUserMedia() {
    Browser.getUserMedia()
}
FS.staticInit();
__ATINIT__.unshift({
    func: function () {
        if (!Module["noFSInit"] && !FS.init.initialized) FS.init()
    }
});
__ATMAIN__.push({
    func: function () {
        FS.ignorePermissions = false
    }
});
__ATEXIT__.push({
    func: function () {
        FS.quit()
    }
});
Module["FS_createFolder"] = FS.createFolder;
Module["FS_createPath"] = FS.createPath;
Module["FS_createDataFile"] = FS.createDataFile;
Module["FS_createPreloadedFile"] = FS.createPreloadedFile;
Module["FS_createLazyFile"] = FS.createLazyFile;
Module["FS_createLink"] = FS.createLink;
Module["FS_createDevice"] = FS.createDevice;
__ATINIT__.unshift({
    func: function () {
        TTY.init()
    }
});
__ATEXIT__.push({
    func: function () {
        TTY.shutdown()
    }
});
TTY.utf8 = new Runtime.UTF8Processor();
if (ENVIRONMENT_IS_NODE) {
    var fs = require("fs");
    NODEFS.staticInit();
}
STACK_BASE = STACKTOP = Runtime.alignMemory(STATICTOP);

staticSealed = true; // seal the static portion of memory

STACK_MAX = STACK_BASE + 5242880;

DYNAMIC_BASE = DYNAMICTOP = Runtime.alignMemory(STACK_MAX);

assert(DYNAMIC_BASE < TOTAL_MEMORY, "TOTAL_MEMORY not big enough for stack");


var Math_min = Math.min;

function asmPrintInt(x, y) {
    Module.print('int ' + x + ',' + y); // + ' ' + new Error().stack);
}

function asmPrintFloat(x, y) {
    Module.print('float ' + x + ',' + y); // + ' ' + new Error().stack);
}
// EMSCRIPTEN_START_ASM
var asm = (function (global, env, buffer) {
    "use asm";
    var a = new global.Int8Array(buffer);
    var b = new global.Int16Array(buffer);
    var c = new global.Int32Array(buffer);
    var d = new global.Uint8Array(buffer);
    var e = new global.Uint16Array(buffer);
    var f = new global.Uint32Array(buffer);
    var g = new global.Float32Array(buffer);
    var h = new global.Float64Array(buffer);
    var i = env.STACKTOP | 0;
    var j = env.STACK_MAX | 0;
    var k = env.tempDoublePtr | 0;
    var l = env.ABORT | 0;
    var m = 0;
    var n = 0;
    var o = 0;
    var p = 0;
    var q = +env.NaN,
        r = +env.Infinity;
    var s = 0,
        t = 0,
        u = 0,
        v = 0,
        w = 0.0,
        x = 0,
        y = 0,
        z = 0,
        A = 0.0;
    var B = 0;
    var C = 0;
    var D = 0;
    var E = 0;
    var F = 0;
    var G = 0;
    var H = 0;
    var I = 0;
    var J = 0;
    var K = 0;
    var L = global.Math.floor;
    var M = global.Math.abs;
    var N = global.Math.sqrt;
    var O = global.Math.pow;
    var P = global.Math.cos;
    var Q = global.Math.sin;
    var R = global.Math.tan;
    var S = global.Math.acos;
    var T = global.Math.asin;
    var U = global.Math.atan;
    var V = global.Math.atan2;
    var W = global.Math.exp;
    var X = global.Math.log;
    var Y = global.Math.ceil;
    var Z = global.Math.imul;
    var _ = env.abort;
    var $ = env.assert;
    var aa = env.asmPrintInt;
    var ba = env.asmPrintFloat;
    var ca = env.min;
    var da = env._clock;
    var ea = env._fflush;
    var fa = env._abort;
    var ga = env.___setErrNo;
    var ha = env._sbrk;
    var ia = env._time;
    var ja = env._emscripten_memcpy_big;
    var ka = env._sysconf;
    var la = env.___errno_location;
    var ma = 0.0;
    // EMSCRIPTEN_START_FUNCS
    function na(a) {
        a = a | 0;
        var b = 0;
        b = i;
        i = i + a | 0;
        i = i + 7 & -8;
        return b | 0
    }

    function oa() {
        return i | 0
    }

    function pa(a) {
        a = a | 0;
        i = a
    }

    function qa(a, b) {
        a = a | 0;
        b = b | 0;
        if ((m | 0) == 0) {
            m = a;
            n = b
        }
    }

    function ra(b) {
        b = b | 0;
        a[k] = a[b];
        a[k + 1 | 0] = a[b + 1 | 0];
        a[k + 2 | 0] = a[b + 2 | 0];
        a[k + 3 | 0] = a[b + 3 | 0]
    }

    function sa(b) {
        b = b | 0;
        a[k] = a[b];
        a[k + 1 | 0] = a[b + 1 | 0];
        a[k + 2 | 0] = a[b + 2 | 0];
        a[k + 3 | 0] = a[b + 3 | 0];
        a[k + 4 | 0] = a[b + 4 | 0];
        a[k + 5 | 0] = a[b + 5 | 0];
        a[k + 6 | 0] = a[b + 6 | 0];
        a[k + 7 | 0] = a[b + 7 | 0]
    }

    function ta(a) {
        a = a | 0;
        B = a
    }

    function ua(a) {
        a = a | 0;
        C = a
    }

    function va(a) {
        a = a | 0;
        D = a
    }

    function wa(a) {
        a = a | 0;
        E = a
    }

    function xa(a) {
        a = a | 0;
        F = a
    }

    function ya(a) {
        a = a | 0;
        G = a
    }

    function za(a) {
        a = a | 0;
        H = a
    }

    function Aa(a) {
        a = a | 0;
        I = a
    }

    function Ba(a) {
        a = a | 0;
        J = a
    }

    function Ca(a) {
        a = a | 0;
        K = a
    }

    function Da() {
        var b = 0,
            d = 0,
            e = 0,
            f = 0;
        b = i;
        da() | 0;
        da() | 0;
        d = Ea(48) | 0;
        c[2] = d;
        e = Ea(48) | 0;
        c[4] = e;
        c[e >> 2] = d;
        c[e + 4 >> 2] = 0;
        c[e + 8 >> 2] = 10001;
        c[e + 12 >> 2] = 40;
        d = e + 16 | 0;
        e = 24 | 0;
        f = d + 31 | 0;
        do {
            a[d] = a[e] | 0;
            d = d + 1 | 0;
            e = e + 1 | 0
        } while ((d | 0) < (f | 0));
        c[1716 >> 2] = 10;
        da() | 0;
        e = 0;
        do {
            a[10688] = 65;
            a[10680] = 66;
            c[2616] = 1;
            c[10504 >> 2] = 7;
            c[10508 >> 2] = 7;
            c[10624 >> 2] = 8;
            c[1720 >> 2] = 8;
            c[1724 >> 2] = 8;
            c[1716 >> 2] = (c[1716 >> 2] | 0) + 1;
            c[5800 >> 2] = 7;
            c[2674] = 5;
            d = c[4] | 0;
            c[d + 12 >> 2] = 5;
            f = c[d >> 2] | 0;
            c[f + 12 >> 2] = 5;
            c[f >> 2] = f;
            f = a[10680] | 0;
            if (!(f << 24 >> 24 < 65)) {
                d = 65;
                do {
                    d = d + 1 << 24 >> 24;
                } while (!(d << 24 >> 24 > f << 24 >> 24))
            }
            e = e + 1 | 0;
        } while ((e | 0) != 1e8);
        i = b;
        return 0
    }

    function Ea(a) {
        a = a | 0;
        var b = 0,
            d = 0,
            e = 0,
            f = 0,
            g = 0,
            h = 0,
            j = 0,
            k = 0,
            l = 0,
            m = 0,
            n = 0,
            o = 0,
            p = 0,
            q = 0,
            r = 0,
            s = 0,
            t = 0,
            u = 0,
            v = 0,
            w = 0,
            x = 0,
            y = 0,
            z = 0,
            A = 0,
            B = 0,
            C = 0,
            D = 0,
            E = 0,
            F = 0,
            G = 0,
            H = 0,
            I = 0,
            J = 0,
            K = 0,
            L = 0,
            M = 0,
            N = 0,
            O = 0,
            P = 0,
            Q = 0,
            R = 0,
            S = 0,
            T = 0,
            U = 0,
            V = 0,
            W = 0,
            X = 0,
            Y = 0,
            Z = 0,
            _ = 0,
            $ = 0,
            aa = 0,
            ba = 0,
            ca = 0,
            da = 0,
            ea = 0,
            ga = 0,
            ja = 0,
            ma = 0,
            na = 0,
            oa = 0,
            pa = 0,
            qa = 0,
            ra = 0,
            sa = 0,
            ta = 0,
            ua = 0,
            va = 0,
            wa = 0,
            xa = 0,
            ya = 0,
            za = 0,
            Aa = 0,
            Ba = 0,
            Ca = 0,
            Da = 0,
            Ea = 0,
            Fa = 0,
            Ga = 0,
            Ha = 0,
            Ia = 0,
            Ja = 0,
            Ka = 0,
            La = 0,
            Ma = 0,
            Na = 0,
            Oa = 0,
            Pa = 0,
            Qa = 0,
            Ra = 0,
            Sa = 0;
        b = i;
        do {
            if (a >>> 0 < 245) {
                if (a >>> 0 < 11) {
                    d = 16
                } else {
                    d = a + 11 & -8
                }
                e = d >>> 3;
                f = c[2676] | 0;
                g = f >>> e;
                if ((g & 3 | 0) != 0) {
                    h = (g & 1 ^ 1) + e | 0;
                    j = h << 1;
                    k = 10744 + (j << 2) | 0;
                    l = 10744 + (j + 2 << 2) | 0;
                    j = c[l >> 2] | 0;
                    m = j + 8 | 0;
                    n = c[m >> 2] | 0;
                    do {
                        if ((k | 0) == (n | 0)) {
                            c[2676] = f & ~(1 << h)
                        } else {
                            if (n >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                fa()
                            }
                            o = n + 12 | 0;
                            if ((c[o >> 2] | 0) == (j | 0)) {
                                c[o >> 2] = k;
                                c[l >> 2] = n;
                                break
                            } else {
                                fa()
                            }
                        }
                    } while (0);
                    n = h << 3;
                    c[j + 4 >> 2] = n | 3;
                    l = j + (n | 4) | 0;
                    c[l >> 2] = c[l >> 2] | 1;
                    p = m;
                    i = b;
                    return p | 0
                }
                if (!(d >>> 0 > (c[10712 >> 2] | 0) >>> 0)) {
                    q = d;
                    break
                }
                if ((g | 0) != 0) {
                    l = 2 << e;
                    n = g << e & (l | 0 - l);
                    l = (n & 0 - n) + -1 | 0;
                    n = l >>> 12 & 16;
                    k = l >>> n;
                    l = k >>> 5 & 8;
                    o = k >>> l;
                    k = o >>> 2 & 4;
                    r = o >>> k;
                    o = r >>> 1 & 2;
                    s = r >>> o;
                    r = s >>> 1 & 1;
                    t = (l | n | k | o | r) + (s >>> r) | 0;
                    r = t << 1;
                    s = 10744 + (r << 2) | 0;
                    o = 10744 + (r + 2 << 2) | 0;
                    r = c[o >> 2] | 0;
                    k = r + 8 | 0;
                    n = c[k >> 2] | 0;
                    do {
                        if ((s | 0) == (n | 0)) {
                            c[2676] = f & ~(1 << t)
                        } else {
                            if (n >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                fa()
                            }
                            l = n + 12 | 0;
                            if ((c[l >> 2] | 0) == (r | 0)) {
                                c[l >> 2] = s;
                                c[o >> 2] = n;
                                break
                            } else {
                                fa()
                            }
                        }
                    } while (0);
                    n = t << 3;
                    o = n - d | 0;
                    c[r + 4 >> 2] = d | 3;
                    s = r;
                    f = s + d | 0;
                    c[s + (d | 4) >> 2] = o | 1;
                    c[s + n >> 2] = o;
                    n = c[10712 >> 2] | 0;
                    if ((n | 0) != 0) {
                        s = c[10724 >> 2] | 0;
                        e = n >>> 3;
                        n = e << 1;
                        g = 10744 + (n << 2) | 0;
                        m = c[2676] | 0;
                        j = 1 << e;
                        do {
                            if ((m & j | 0) == 0) {
                                c[2676] = m | j;
                                u = 10744 + (n + 2 << 2) | 0;
                                v = g
                            } else {
                                e = 10744 + (n + 2 << 2) | 0;
                                h = c[e >> 2] | 0;
                                if (!(h >>> 0 < (c[10720 >> 2] | 0) >>> 0)) {
                                    u = e;
                                    v = h;
                                    break
                                }
                                fa()
                            }
                        } while (0);
                        c[u >> 2] = s;
                        c[v + 12 >> 2] = s;
                        c[s + 8 >> 2] = v;
                        c[s + 12 >> 2] = g
                    }
                    c[10712 >> 2] = o;
                    c[10724 >> 2] = f;
                    p = k;
                    i = b;
                    return p | 0
                }
                n = c[10708 >> 2] | 0;
                if ((n | 0) == 0) {
                    q = d;
                    break
                }
                j = (n & 0 - n) + -1 | 0;
                n = j >>> 12 & 16;
                m = j >>> n;
                j = m >>> 5 & 8;
                r = m >>> j;
                m = r >>> 2 & 4;
                t = r >>> m;
                r = t >>> 1 & 2;
                h = t >>> r;
                t = h >>> 1 & 1;
                e = c[11008 + ((j | n | m | r | t) + (h >>> t) << 2) >> 2] | 0;
                t = (c[e + 4 >> 2] & -8) - d | 0;
                h = e;
                r = e;
                while (1) {
                    e = c[h + 16 >> 2] | 0;
                    if ((e | 0) == 0) {
                        m = c[h + 20 >> 2] | 0;
                        if ((m | 0) == 0) {
                            break
                        } else {
                            w = m
                        }
                    } else {
                        w = e
                    }
                    e = (c[w + 4 >> 2] & -8) - d | 0;
                    m = e >>> 0 < t >>> 0;
                    t = m ? e : t;
                    h = w;
                    r = m ? w : r
                }
                h = r;
                k = c[10720 >> 2] | 0;
                if (h >>> 0 < k >>> 0) {
                    fa()
                }
                f = h + d | 0;
                o = f;
                if (!(h >>> 0 < f >>> 0)) {
                    fa()
                }
                f = c[r + 24 >> 2] | 0;
                g = c[r + 12 >> 2] | 0;
                do {
                    if ((g | 0) == (r | 0)) {
                        s = r + 20 | 0;
                        m = c[s >> 2] | 0;
                        if ((m | 0) == 0) {
                            e = r + 16 | 0;
                            n = c[e >> 2] | 0;
                            if ((n | 0) == 0) {
                                x = 0;
                                break
                            } else {
                                y = n;
                                z = e
                            }
                        } else {
                            y = m;
                            z = s
                        }
                        while (1) {
                            s = y + 20 | 0;
                            m = c[s >> 2] | 0;
                            if ((m | 0) != 0) {
                                z = s;
                                y = m;
                                continue
                            }
                            m = y + 16 | 0;
                            s = c[m >> 2] | 0;
                            if ((s | 0) == 0) {
                                break
                            } else {
                                y = s;
                                z = m
                            }
                        }
                        if (z >>> 0 < k >>> 0) {
                            fa()
                        } else {
                            c[z >> 2] = 0;
                            x = y;
                            break
                        }
                    } else {
                        m = c[r + 8 >> 2] | 0;
                        if (m >>> 0 < k >>> 0) {
                            fa()
                        }
                        s = m + 12 | 0;
                        if ((c[s >> 2] | 0) != (r | 0)) {
                            fa()
                        }
                        e = g + 8 | 0;
                        if ((c[e >> 2] | 0) == (r | 0)) {
                            c[s >> 2] = g;
                            c[e >> 2] = m;
                            x = g;
                            break
                        } else {
                            fa()
                        }
                    }
                } while (0);
                a: do {
                    if ((f | 0) != 0) {
                        g = c[r + 28 >> 2] | 0;
                        k = 11008 + (g << 2) | 0;
                        do {
                            if ((r | 0) == (c[k >> 2] | 0)) {
                                c[k >> 2] = x;
                                if ((x | 0) != 0) {
                                    break
                                }
                                c[10708 >> 2] = c[10708 >> 2] & ~(1 << g);
                                break a
                            } else {
                                if (f >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                    fa()
                                }
                                m = f + 16 | 0;
                                if ((c[m >> 2] | 0) == (r | 0)) {
                                    c[m >> 2] = x
                                } else {
                                    c[f + 20 >> 2] = x
                                } if ((x | 0) == 0) {
                                    break a
                                }
                            }
                        } while (0);
                        if (x >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                            fa()
                        }
                        c[x + 24 >> 2] = f;
                        g = c[r + 16 >> 2] | 0;
                        do {
                            if ((g | 0) != 0) {
                                if (g >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                    fa()
                                } else {
                                    c[x + 16 >> 2] = g;
                                    c[g + 24 >> 2] = x;
                                    break
                                }
                            }
                        } while (0);
                        g = c[r + 20 >> 2] | 0;
                        if ((g | 0) == 0) {
                            break
                        }
                        if (g >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                            fa()
                        } else {
                            c[x + 20 >> 2] = g;
                            c[g + 24 >> 2] = x;
                            break
                        }
                    }
                } while (0);
                if (t >>> 0 < 16) {
                    f = t + d | 0;
                    c[r + 4 >> 2] = f | 3;
                    g = h + (f + 4) | 0;
                    c[g >> 2] = c[g >> 2] | 1
                } else {
                    c[r + 4 >> 2] = d | 3;
                    c[h + (d | 4) >> 2] = t | 1;
                    c[h + (t + d) >> 2] = t;
                    g = c[10712 >> 2] | 0;
                    if ((g | 0) != 0) {
                        f = c[10724 >> 2] | 0;
                        k = g >>> 3;
                        g = k << 1;
                        m = 10744 + (g << 2) | 0;
                        e = c[2676] | 0;
                        s = 1 << k;
                        do {
                            if ((e & s | 0) == 0) {
                                c[2676] = e | s;
                                A = 10744 + (g + 2 << 2) | 0;
                                B = m
                            } else {
                                k = 10744 + (g + 2 << 2) | 0;
                                n = c[k >> 2] | 0;
                                if (!(n >>> 0 < (c[10720 >> 2] | 0) >>> 0)) {
                                    A = k;
                                    B = n;
                                    break
                                }
                                fa()
                            }
                        } while (0);
                        c[A >> 2] = f;
                        c[B + 12 >> 2] = f;
                        c[f + 8 >> 2] = B;
                        c[f + 12 >> 2] = m
                    }
                    c[10712 >> 2] = t;
                    c[10724 >> 2] = o
                }
                p = r + 8 | 0;
                i = b;
                return p | 0
            } else {
                if (a >>> 0 > 4294967231) {
                    q = -1;
                    break
                }
                g = a + 11 | 0;
                s = g & -8;
                e = c[10708 >> 2] | 0;
                if ((e | 0) == 0) {
                    q = s;
                    break
                }
                h = 0 - s | 0;
                n = g >>> 8;
                do {
                    if ((n | 0) == 0) {
                        C = 0
                    } else {
                        if (s >>> 0 > 16777215) {
                            C = 31;
                            break
                        }
                        g = (n + 1048320 | 0) >>> 16 & 8;
                        k = n << g;
                        j = (k + 520192 | 0) >>> 16 & 4;
                        l = k << j;
                        k = (l + 245760 | 0) >>> 16 & 2;
                        D = 14 - (j | g | k) + (l << k >>> 15) | 0;
                        C = s >>> (D + 7 | 0) & 1 | D << 1
                    }
                } while (0);
                n = c[11008 + (C << 2) >> 2] | 0;
                b: do {
                    if ((n | 0) == 0) {
                        E = h;
                        F = 0;
                        G = 0
                    } else {
                        if ((C | 0) == 31) {
                            H = 0
                        } else {
                            H = 25 - (C >>> 1) | 0
                        }
                        r = h;
                        o = 0;
                        t = s << H;
                        m = n;
                        f = 0;
                        while (1) {
                            D = c[m + 4 >> 2] & -8;
                            k = D - s | 0;
                            if (k >>> 0 < r >>> 0) {
                                if ((D | 0) == (s | 0)) {
                                    E = k;
                                    F = m;
                                    G = m;
                                    break b
                                } else {
                                    I = k;
                                    J = m
                                }
                            } else {
                                I = r;
                                J = f
                            }
                            k = c[m + 20 >> 2] | 0;
                            D = c[m + (t >>> 31 << 2) + 16 >> 2] | 0;
                            l = (k | 0) == 0 | (k | 0) == (D | 0) ? o : k;
                            if ((D | 0) == 0) {
                                E = I;
                                F = l;
                                G = J;
                                break
                            } else {
                                r = I;
                                o = l;
                                t = t << 1;
                                m = D;
                                f = J
                            }
                        }
                    }
                } while (0);
                if ((F | 0) == 0 & (G | 0) == 0) {
                    n = 2 << C;
                    h = e & (n | 0 - n);
                    if ((h | 0) == 0) {
                        q = s;
                        break
                    }
                    n = (h & 0 - h) + -1 | 0;
                    h = n >>> 12 & 16;
                    f = n >>> h;
                    n = f >>> 5 & 8;
                    m = f >>> n;
                    f = m >>> 2 & 4;
                    t = m >>> f;
                    m = t >>> 1 & 2;
                    o = t >>> m;
                    t = o >>> 1 & 1;
                    K = c[11008 + ((n | h | f | m | t) + (o >>> t) << 2) >> 2] | 0
                } else {
                    K = F
                } if ((K | 0) == 0) {
                    L = E;
                    M = G
                } else {
                    t = E;
                    o = K;
                    m = G;
                    while (1) {
                        f = (c[o + 4 >> 2] & -8) - s | 0;
                        h = f >>> 0 < t >>> 0;
                        n = h ? f : t;
                        f = h ? o : m;
                        h = c[o + 16 >> 2] | 0;
                        if ((h | 0) != 0) {
                            N = f;
                            O = n;
                            m = N;
                            o = h;
                            t = O;
                            continue
                        }
                        h = c[o + 20 >> 2] | 0;
                        if ((h | 0) == 0) {
                            L = n;
                            M = f;
                            break
                        } else {
                            N = f;
                            O = n;
                            o = h;
                            m = N;
                            t = O
                        }
                    }
                } if ((M | 0) == 0) {
                    q = s;
                    break
                }
                if (!(L >>> 0 < ((c[10712 >> 2] | 0) - s | 0) >>> 0)) {
                    q = s;
                    break
                }
                t = M;
                m = c[10720 >> 2] | 0;
                if (t >>> 0 < m >>> 0) {
                    fa()
                }
                o = t + s | 0;
                e = o;
                if (!(t >>> 0 < o >>> 0)) {
                    fa()
                }
                h = c[M + 24 >> 2] | 0;
                n = c[M + 12 >> 2] | 0;
                do {
                    if ((n | 0) == (M | 0)) {
                        f = M + 20 | 0;
                        r = c[f >> 2] | 0;
                        if ((r | 0) == 0) {
                            D = M + 16 | 0;
                            l = c[D >> 2] | 0;
                            if ((l | 0) == 0) {
                                P = 0;
                                break
                            } else {
                                Q = l;
                                R = D
                            }
                        } else {
                            Q = r;
                            R = f
                        }
                        while (1) {
                            f = Q + 20 | 0;
                            r = c[f >> 2] | 0;
                            if ((r | 0) != 0) {
                                R = f;
                                Q = r;
                                continue
                            }
                            r = Q + 16 | 0;
                            f = c[r >> 2] | 0;
                            if ((f | 0) == 0) {
                                break
                            } else {
                                Q = f;
                                R = r
                            }
                        }
                        if (R >>> 0 < m >>> 0) {
                            fa()
                        } else {
                            c[R >> 2] = 0;
                            P = Q;
                            break
                        }
                    } else {
                        r = c[M + 8 >> 2] | 0;
                        if (r >>> 0 < m >>> 0) {
                            fa()
                        }
                        f = r + 12 | 0;
                        if ((c[f >> 2] | 0) != (M | 0)) {
                            fa()
                        }
                        D = n + 8 | 0;
                        if ((c[D >> 2] | 0) == (M | 0)) {
                            c[f >> 2] = n;
                            c[D >> 2] = r;
                            P = n;
                            break
                        } else {
                            fa()
                        }
                    }
                } while (0);
                c: do {
                    if ((h | 0) != 0) {
                        n = c[M + 28 >> 2] | 0;
                        m = 11008 + (n << 2) | 0;
                        do {
                            if ((M | 0) == (c[m >> 2] | 0)) {
                                c[m >> 2] = P;
                                if ((P | 0) != 0) {
                                    break
                                }
                                c[10708 >> 2] = c[10708 >> 2] & ~(1 << n);
                                break c
                            } else {
                                if (h >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                    fa()
                                }
                                r = h + 16 | 0;
                                if ((c[r >> 2] | 0) == (M | 0)) {
                                    c[r >> 2] = P
                                } else {
                                    c[h + 20 >> 2] = P
                                } if ((P | 0) == 0) {
                                    break c
                                }
                            }
                        } while (0);
                        if (P >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                            fa()
                        }
                        c[P + 24 >> 2] = h;
                        n = c[M + 16 >> 2] | 0;
                        do {
                            if ((n | 0) != 0) {
                                if (n >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                    fa()
                                } else {
                                    c[P + 16 >> 2] = n;
                                    c[n + 24 >> 2] = P;
                                    break
                                }
                            }
                        } while (0);
                        n = c[M + 20 >> 2] | 0;
                        if ((n | 0) == 0) {
                            break
                        }
                        if (n >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                            fa()
                        } else {
                            c[P + 20 >> 2] = n;
                            c[n + 24 >> 2] = P;
                            break
                        }
                    }
                } while (0);
                d: do {
                    if (L >>> 0 < 16) {
                        h = L + s | 0;
                        c[M + 4 >> 2] = h | 3;
                        n = t + (h + 4) | 0;
                        c[n >> 2] = c[n >> 2] | 1
                    } else {
                        c[M + 4 >> 2] = s | 3;
                        c[t + (s | 4) >> 2] = L | 1;
                        c[t + (L + s) >> 2] = L;
                        n = L >>> 3;
                        if (L >>> 0 < 256) {
                            h = n << 1;
                            m = 10744 + (h << 2) | 0;
                            r = c[2676] | 0;
                            D = 1 << n;
                            do {
                                if ((r & D | 0) == 0) {
                                    c[2676] = r | D;
                                    S = 10744 + (h + 2 << 2) | 0;
                                    T = m
                                } else {
                                    n = 10744 + (h + 2 << 2) | 0;
                                    f = c[n >> 2] | 0;
                                    if (!(f >>> 0 < (c[10720 >> 2] | 0) >>> 0)) {
                                        S = n;
                                        T = f;
                                        break
                                    }
                                    fa()
                                }
                            } while (0);
                            c[S >> 2] = e;
                            c[T + 12 >> 2] = e;
                            c[t + (s + 8) >> 2] = T;
                            c[t + (s + 12) >> 2] = m;
                            break
                        }
                        h = o;
                        D = L >>> 8;
                        do {
                            if ((D | 0) == 0) {
                                U = 0
                            } else {
                                if (L >>> 0 > 16777215) {
                                    U = 31;
                                    break
                                }
                                r = (D + 1048320 | 0) >>> 16 & 8;
                                f = D << r;
                                n = (f + 520192 | 0) >>> 16 & 4;
                                l = f << n;
                                f = (l + 245760 | 0) >>> 16 & 2;
                                k = 14 - (n | r | f) + (l << f >>> 15) | 0;
                                U = L >>> (k + 7 | 0) & 1 | k << 1
                            }
                        } while (0);
                        D = 11008 + (U << 2) | 0;
                        c[t + (s + 28) >> 2] = U;
                        c[t + (s + 20) >> 2] = 0;
                        c[t + (s + 16) >> 2] = 0;
                        m = c[10708 >> 2] | 0;
                        k = 1 << U;
                        if ((m & k | 0) == 0) {
                            c[10708 >> 2] = m | k;
                            c[D >> 2] = h;
                            c[t + (s + 24) >> 2] = D;
                            c[t + (s + 12) >> 2] = h;
                            c[t + (s + 8) >> 2] = h;
                            break
                        }
                        k = c[D >> 2] | 0;
                        if ((U | 0) == 31) {
                            V = 0
                        } else {
                            V = 25 - (U >>> 1) | 0
                        }
                        e: do {
                            if ((c[k + 4 >> 2] & -8 | 0) == (L | 0)) {
                                W = k
                            } else {
                                D = L << V;
                                m = k;
                                while (1) {
                                    X = m + (D >>> 31 << 2) + 16 | 0;
                                    f = c[X >> 2] | 0;
                                    if ((f | 0) == 0) {
                                        break
                                    }
                                    if ((c[f + 4 >> 2] & -8 | 0) == (L | 0)) {
                                        W = f;
                                        break e
                                    } else {
                                        D = D << 1;
                                        m = f
                                    }
                                }
                                if (X >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                    fa()
                                } else {
                                    c[X >> 2] = h;
                                    c[t + (s + 24) >> 2] = m;
                                    c[t + (s + 12) >> 2] = h;
                                    c[t + (s + 8) >> 2] = h;
                                    break d
                                }
                            }
                        } while (0);
                        k = W + 8 | 0;
                        D = c[k >> 2] | 0;
                        f = c[10720 >> 2] | 0;
                        if (W >>> 0 < f >>> 0) {
                            fa()
                        }
                        if (D >>> 0 < f >>> 0) {
                            fa()
                        } else {
                            c[D + 12 >> 2] = h;
                            c[k >> 2] = h;
                            c[t + (s + 8) >> 2] = D;
                            c[t + (s + 12) >> 2] = W;
                            c[t + (s + 24) >> 2] = 0;
                            break
                        }
                    }
                } while (0);
                p = M + 8 | 0;
                i = b;
                return p | 0
            }
        } while (0);
        M = c[10712 >> 2] | 0;
        if (!(q >>> 0 > M >>> 0)) {
            W = M - q | 0;
            X = c[10724 >> 2] | 0;
            if (W >>> 0 > 15) {
                L = X;
                c[10724 >> 2] = L + q;
                c[10712 >> 2] = W;
                c[L + (q + 4) >> 2] = W | 1;
                c[L + M >> 2] = W;
                c[X + 4 >> 2] = q | 3
            } else {
                c[10712 >> 2] = 0;
                c[10724 >> 2] = 0;
                c[X + 4 >> 2] = M | 3;
                W = X + (M + 4) | 0;
                c[W >> 2] = c[W >> 2] | 1
            }
            p = X + 8 | 0;
            i = b;
            return p | 0
        }
        X = c[10716 >> 2] | 0;
        if (q >>> 0 < X >>> 0) {
            W = X - q | 0;
            c[10716 >> 2] = W;
            X = c[10728 >> 2] | 0;
            M = X;
            c[10728 >> 2] = M + q;
            c[M + (q + 4) >> 2] = W | 1;
            c[X + 4 >> 2] = q | 3;
            p = X + 8 | 0;
            i = b;
            return p | 0
        }
        do {
            if ((c[2794] | 0) == 0) {
                X = ka(30) | 0;
                if ((X + -1 & X | 0) == 0) {
                    c[11184 >> 2] = X;
                    c[11180 >> 2] = X;
                    c[11188 >> 2] = -1;
                    c[11192 >> 2] = -1;
                    c[11196 >> 2] = 0;
                    c[11148 >> 2] = 0;
                    c[2794] = (ia(0) | 0) & -16 ^ 1431655768;
                    break
                } else {
                    fa()
                }
            }
        } while (0);
        X = q + 48 | 0;
        W = c[11184 >> 2] | 0;
        M = q + 47 | 0;
        L = W + M | 0;
        V = 0 - W | 0;
        W = L & V;
        if (!(W >>> 0 > q >>> 0)) {
            p = 0;
            i = b;
            return p | 0
        }
        U = c[11144 >> 2] | 0;
        do {
            if ((U | 0) != 0) {
                T = c[11136 >> 2] | 0;
                S = T + W | 0;
                if (S >>> 0 <= T >>> 0 | S >>> 0 > U >>> 0) {
                    p = 0
                } else {
                    break
                }
                i = b;
                return p | 0
            }
        } while (0);
        f: do {
            if ((c[11148 >> 2] & 4 | 0) == 0) {
                U = c[10728 >> 2] | 0;
                g: do {
                    if ((U | 0) == 0) {
                        Y = 182
                    } else {
                        S = U;
                        T = 11152 | 0;
                        while (1) {
                            Z = T;
                            P = c[Z >> 2] | 0;
                            if (!(P >>> 0 > S >>> 0)) {
                                _ = T + 4 | 0;
                                if ((P + (c[_ >> 2] | 0) | 0) >>> 0 > S >>> 0) {
                                    break
                                }
                            }
                            P = c[T + 8 >> 2] | 0;
                            if ((P | 0) == 0) {
                                Y = 182;
                                break g
                            } else {
                                T = P
                            }
                        }
                        if ((T | 0) == 0) {
                            Y = 182;
                            break
                        }
                        S = L - (c[10716 >> 2] | 0) & V;
                        if (!(S >>> 0 < 2147483647)) {
                            $ = 0;
                            break
                        }
                        h = ha(S | 0) | 0;
                        P = (h | 0) == ((c[Z >> 2] | 0) + (c[_ >> 2] | 0) | 0);
                        aa = h;
                        ba = S;
                        ca = P ? h : -1;
                        da = P ? S : 0;
                        Y = 191
                    }
                } while (0);
                do {
                    if ((Y | 0) == 182) {
                        U = ha(0) | 0;
                        if ((U | 0) == (-1 | 0)) {
                            $ = 0;
                            break
                        }
                        S = U;
                        P = c[11180 >> 2] | 0;
                        h = P + -1 | 0;
                        if ((h & S | 0) == 0) {
                            ea = W
                        } else {
                            ea = W - S + (h + S & 0 - P) | 0
                        }
                        P = c[11136 >> 2] | 0;
                        S = P + ea | 0;
                        if (!(ea >>> 0 > q >>> 0 & ea >>> 0 < 2147483647)) {
                            $ = 0;
                            break
                        }
                        h = c[11144 >> 2] | 0;
                        if ((h | 0) != 0) {
                            if (S >>> 0 <= P >>> 0 | S >>> 0 > h >>> 0) {
                                $ = 0;
                                break
                            }
                        }
                        h = ha(ea | 0) | 0;
                        S = (h | 0) == (U | 0);
                        aa = h;
                        ba = ea;
                        ca = S ? U : -1;
                        da = S ? ea : 0;
                        Y = 191
                    }
                } while (0);
                h: do {
                    if ((Y | 0) == 191) {
                        S = 0 - ba | 0;
                        if ((ca | 0) != (-1 | 0)) {
                            ga = ca;
                            ja = da;
                            Y = 202;
                            break f
                        }
                        do {
                            if ((aa | 0) != (-1 | 0) & ba >>> 0 < 2147483647 & ba >>> 0 < X >>> 0) {
                                U = c[11184 >> 2] | 0;
                                h = M - ba + U & 0 - U;
                                if (!(h >>> 0 < 2147483647)) {
                                    ma = ba;
                                    break
                                }
                                if ((ha(h | 0) | 0) == (-1 | 0)) {
                                    ha(S | 0) | 0;
                                    $ = da;
                                    break h
                                } else {
                                    ma = h + ba | 0;
                                    break
                                }
                            } else {
                                ma = ba
                            }
                        } while (0);
                        if ((aa | 0) == (-1 | 0)) {
                            $ = da
                        } else {
                            ga = aa;
                            ja = ma;
                            Y = 202;
                            break f
                        }
                    }
                } while (0);
                c[11148 >> 2] = c[11148 >> 2] | 4;
                na = $;
                Y = 199
            } else {
                na = 0;
                Y = 199
            }
        } while (0);
        do {
            if ((Y | 0) == 199) {
                if (!(W >>> 0 < 2147483647)) {
                    break
                }
                $ = ha(W | 0) | 0;
                ma = ha(0) | 0;
                if (!((ma | 0) != (-1 | 0) & ($ | 0) != (-1 | 0) & $ >>> 0 < ma >>> 0)) {
                    break
                }
                aa = ma - $ | 0;
                ma = aa >>> 0 > (q + 40 | 0) >>> 0;
                if (ma) {
                    ga = $;
                    ja = ma ? aa : na;
                    Y = 202
                }
            }
        } while (0);
        do {
            if ((Y | 0) == 202) {
                na = (c[11136 >> 2] | 0) + ja | 0;
                c[11136 >> 2] = na;
                if (na >>> 0 > (c[11140 >> 2] | 0) >>> 0) {
                    c[11140 >> 2] = na
                }
                na = c[10728 >> 2] | 0;
                i: do {
                    if ((na | 0) == 0) {
                        W = c[10720 >> 2] | 0;
                        if ((W | 0) == 0 | ga >>> 0 < W >>> 0) {
                            c[10720 >> 2] = ga
                        }
                        c[11152 >> 2] = ga;
                        c[11156 >> 2] = ja;
                        c[11164 >> 2] = 0;
                        c[10740 >> 2] = c[2794];
                        c[10736 >> 2] = -1;
                        W = 0;
                        do {
                            aa = W << 1;
                            ma = 10744 + (aa << 2) | 0;
                            c[10744 + (aa + 3 << 2) >> 2] = ma;
                            c[10744 + (aa + 2 << 2) >> 2] = ma;
                            W = W + 1 | 0;
                        } while ((W | 0) != 32);
                        W = ga + 8 | 0;
                        if ((W & 7 | 0) == 0) {
                            oa = 0
                        } else {
                            oa = 0 - W & 7
                        }
                        W = ja + -40 - oa | 0;
                        c[10728 >> 2] = ga + oa;
                        c[10716 >> 2] = W;
                        c[ga + (oa + 4) >> 2] = W | 1;
                        c[ga + (ja + -36) >> 2] = 40;
                        c[10732 >> 2] = c[11192 >> 2]
                    } else {
                        W = 11152 | 0;
                        while (1) {
                            pa = c[W >> 2] | 0;
                            qa = W + 4 | 0;
                            ra = c[qa >> 2] | 0;
                            if ((ga | 0) == (pa + ra | 0)) {
                                Y = 214;
                                break
                            }
                            ma = c[W + 8 >> 2] | 0;
                            if ((ma | 0) == 0) {
                                break
                            } else {
                                W = ma
                            }
                        }
                        do {
                            if ((Y | 0) == 214) {
                                if ((c[W + 12 >> 2] & 8 | 0) != 0) {
                                    break
                                }
                                ma = na;
                                if (!(ma >>> 0 >= pa >>> 0 & ma >>> 0 < ga >>> 0)) {
                                    break
                                }
                                c[qa >> 2] = ra + ja;
                                aa = (c[10716 >> 2] | 0) + ja | 0;
                                $ = na + 8 | 0;
                                if (($ & 7 | 0) == 0) {
                                    sa = 0
                                } else {
                                    sa = 0 - $ & 7
                                }
                                $ = aa - sa | 0;
                                c[10728 >> 2] = ma + sa;
                                c[10716 >> 2] = $;
                                c[ma + (sa + 4) >> 2] = $ | 1;
                                c[ma + (aa + 4) >> 2] = 40;
                                c[10732 >> 2] = c[11192 >> 2];
                                break i
                            }
                        } while (0);
                        if (ga >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                            c[10720 >> 2] = ga
                        }
                        W = ga + ja | 0;
                        aa = 11152 | 0;
                        while (1) {
                            ta = aa;
                            if ((c[ta >> 2] | 0) == (W | 0)) {
                                Y = 224;
                                break
                            }
                            ma = c[aa + 8 >> 2] | 0;
                            if ((ma | 0) == 0) {
                                break
                            } else {
                                aa = ma
                            }
                        }
                        do {
                            if ((Y | 0) == 224) {
                                if ((c[aa + 12 >> 2] & 8 | 0) != 0) {
                                    break
                                }
                                c[ta >> 2] = ga;
                                W = aa + 4 | 0;
                                c[W >> 2] = (c[W >> 2] | 0) + ja;
                                W = ga + 8 | 0;
                                if ((W & 7 | 0) == 0) {
                                    ua = 0
                                } else {
                                    ua = 0 - W & 7
                                }
                                W = ga + (ja + 8) | 0;
                                if ((W & 7 | 0) == 0) {
                                    va = 0
                                } else {
                                    va = 0 - W & 7
                                }
                                W = ga + (va + ja) | 0;
                                ma = W;
                                $ = ua + q | 0;
                                da = ga + $ | 0;
                                ba = da;
                                M = W - (ga + ua) - q | 0;
                                c[ga + (ua + 4) >> 2] = q | 3;
                                j: do {
                                    if ((ma | 0) == (c[10728 >> 2] | 0)) {
                                        X = (c[10716 >> 2] | 0) + M | 0;
                                        c[10716 >> 2] = X;
                                        c[10728 >> 2] = ba;
                                        c[ga + ($ + 4) >> 2] = X | 1
                                    } else {
                                        if ((ma | 0) == (c[10724 >> 2] | 0)) {
                                            X = (c[10712 >> 2] | 0) + M | 0;
                                            c[10712 >> 2] = X;
                                            c[10724 >> 2] = ba;
                                            c[ga + ($ + 4) >> 2] = X | 1;
                                            c[ga + (X + $) >> 2] = X;
                                            break
                                        }
                                        X = ja + 4 | 0;
                                        ca = c[ga + (X + va) >> 2] | 0;
                                        if ((ca & 3 | 0) == 1) {
                                            ea = ca & -8;
                                            _ = ca >>> 3;
                                            k: do {
                                                if (ca >>> 0 < 256) {
                                                    Z = c[ga + ((va | 8) + ja) >> 2] | 0;
                                                    V = c[ga + (ja + 12 + va) >> 2] | 0;
                                                    L = 10744 + (_ << 1 << 2) | 0;
                                                    do {
                                                        if ((Z | 0) != (L | 0)) {
                                                            if (Z >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                                                fa()
                                                            }
                                                            if ((c[Z + 12 >> 2] | 0) == (ma | 0)) {
                                                                break
                                                            }
                                                            fa()
                                                        }
                                                    } while (0);
                                                    if ((V | 0) == (Z | 0)) {
                                                        c[2676] = c[2676] & ~(1 << _);
                                                        break
                                                    }
                                                    do {
                                                        if ((V | 0) == (L | 0)) {
                                                            wa = V + 8 | 0
                                                        } else {
                                                            if (V >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                                                fa()
                                                            }
                                                            S = V + 8 | 0;
                                                            if ((c[S >> 2] | 0) == (ma | 0)) {
                                                                wa = S;
                                                                break
                                                            }
                                                            fa()
                                                        }
                                                    } while (0);
                                                    c[Z + 12 >> 2] = V;
                                                    c[wa >> 2] = Z
                                                } else {
                                                    L = W;
                                                    S = c[ga + ((va | 24) + ja) >> 2] | 0;
                                                    T = c[ga + (ja + 12 + va) >> 2] | 0;
                                                    do {
                                                        if ((T | 0) == (L | 0)) {
                                                            h = va | 16;
                                                            U = ga + (X + h) | 0;
                                                            P = c[U >> 2] | 0;
                                                            if ((P | 0) == 0) {
                                                                Q = ga + (h + ja) | 0;
                                                                h = c[Q >> 2] | 0;
                                                                if ((h | 0) == 0) {
                                                                    xa = 0;
                                                                    break
                                                                } else {
                                                                    ya = h;
                                                                    za = Q
                                                                }
                                                            } else {
                                                                ya = P;
                                                                za = U
                                                            }
                                                            while (1) {
                                                                U = ya + 20 | 0;
                                                                P = c[U >> 2] | 0;
                                                                if ((P | 0) != 0) {
                                                                    za = U;
                                                                    ya = P;
                                                                    continue
                                                                }
                                                                P = ya + 16 | 0;
                                                                U = c[P >> 2] | 0;
                                                                if ((U | 0) == 0) {
                                                                    break
                                                                } else {
                                                                    ya = U;
                                                                    za = P
                                                                }
                                                            }
                                                            if (za >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                                                fa()
                                                            } else {
                                                                c[za >> 2] = 0;
                                                                xa = ya;
                                                                break
                                                            }
                                                        } else {
                                                            P = c[ga + ((va | 8) + ja) >> 2] | 0;
                                                            if (P >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                                                fa()
                                                            }
                                                            U = P + 12 | 0;
                                                            if ((c[U >> 2] | 0) != (L | 0)) {
                                                                fa()
                                                            }
                                                            Q = T + 8 | 0;
                                                            if ((c[Q >> 2] | 0) == (L | 0)) {
                                                                c[U >> 2] = T;
                                                                c[Q >> 2] = P;
                                                                xa = T;
                                                                break
                                                            } else {
                                                                fa()
                                                            }
                                                        }
                                                    } while (0);
                                                    if ((S | 0) == 0) {
                                                        break
                                                    }
                                                    T = c[ga + (ja + 28 + va) >> 2] | 0;
                                                    Z = 11008 + (T << 2) | 0;
                                                    do {
                                                        if ((L | 0) == (c[Z >> 2] | 0)) {
                                                            c[Z >> 2] = xa;
                                                            if ((xa | 0) != 0) {
                                                                break
                                                            }
                                                            c[10708 >> 2] = c[10708 >> 2] & ~(1 << T);
                                                            break k
                                                        } else {
                                                            if (S >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                                                fa()
                                                            }
                                                            V = S + 16 | 0;
                                                            if ((c[V >> 2] | 0) == (L | 0)) {
                                                                c[V >> 2] = xa
                                                            } else {
                                                                c[S + 20 >> 2] = xa
                                                            } if ((xa | 0) == 0) {
                                                                break k
                                                            }
                                                        }
                                                    } while (0);
                                                    if (xa >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                                        fa()
                                                    }
                                                    c[xa + 24 >> 2] = S;
                                                    L = va | 16;
                                                    T = c[ga + (L + ja) >> 2] | 0;
                                                    do {
                                                        if ((T | 0) != 0) {
                                                            if (T >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                                                fa()
                                                            } else {
                                                                c[xa + 16 >> 2] = T;
                                                                c[T + 24 >> 2] = xa;
                                                                break
                                                            }
                                                        }
                                                    } while (0);
                                                    T = c[ga + (X + L) >> 2] | 0;
                                                    if ((T | 0) == 0) {
                                                        break
                                                    }
                                                    if (T >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                                        fa()
                                                    } else {
                                                        c[xa + 20 >> 2] = T;
                                                        c[T + 24 >> 2] = xa;
                                                        break
                                                    }
                                                }
                                            } while (0);
                                            Aa = ga + ((ea | va) + ja) | 0;
                                            Ba = ea + M | 0
                                        } else {
                                            Aa = ma;
                                            Ba = M
                                        }
                                        X = Aa + 4 | 0;
                                        c[X >> 2] = c[X >> 2] & -2;
                                        c[ga + ($ + 4) >> 2] = Ba | 1;
                                        c[ga + (Ba + $) >> 2] = Ba;
                                        X = Ba >>> 3;
                                        if (Ba >>> 0 < 256) {
                                            _ = X << 1;
                                            ca = 10744 + (_ << 2) | 0;
                                            T = c[2676] | 0;
                                            S = 1 << X;
                                            do {
                                                if ((T & S | 0) == 0) {
                                                    c[2676] = T | S;
                                                    Ca = 10744 + (_ + 2 << 2) | 0;
                                                    Da = ca
                                                } else {
                                                    X = 10744 + (_ + 2 << 2) | 0;
                                                    Z = c[X >> 2] | 0;
                                                    if (!(Z >>> 0 < (c[10720 >> 2] | 0) >>> 0)) {
                                                        Ca = X;
                                                        Da = Z;
                                                        break
                                                    }
                                                    fa()
                                                }
                                            } while (0);
                                            c[Ca >> 2] = ba;
                                            c[Da + 12 >> 2] = ba;
                                            c[ga + ($ + 8) >> 2] = Da;
                                            c[ga + ($ + 12) >> 2] = ca;
                                            break
                                        }
                                        _ = da;
                                        S = Ba >>> 8;
                                        do {
                                            if ((S | 0) == 0) {
                                                Ea = 0
                                            } else {
                                                if (Ba >>> 0 > 16777215) {
                                                    Ea = 31;
                                                    break
                                                }
                                                T = (S + 1048320 | 0) >>> 16 & 8;
                                                ea = S << T;
                                                Z = (ea + 520192 | 0) >>> 16 & 4;
                                                X = ea << Z;
                                                ea = (X + 245760 | 0) >>> 16 & 2;
                                                V = 14 - (Z | T | ea) + (X << ea >>> 15) | 0;
                                                Ea = Ba >>> (V + 7 | 0) & 1 | V << 1
                                            }
                                        } while (0);
                                        S = 11008 + (Ea << 2) | 0;
                                        c[ga + ($ + 28) >> 2] = Ea;
                                        c[ga + ($ + 20) >> 2] = 0;
                                        c[ga + ($ + 16) >> 2] = 0;
                                        ca = c[10708 >> 2] | 0;
                                        V = 1 << Ea;
                                        if ((ca & V | 0) == 0) {
                                            c[10708 >> 2] = ca | V;
                                            c[S >> 2] = _;
                                            c[ga + ($ + 24) >> 2] = S;
                                            c[ga + ($ + 12) >> 2] = _;
                                            c[ga + ($ + 8) >> 2] = _;
                                            break
                                        }
                                        V = c[S >> 2] | 0;
                                        if ((Ea | 0) == 31) {
                                            Fa = 0
                                        } else {
                                            Fa = 25 - (Ea >>> 1) | 0
                                        }
                                        l: do {
                                            if ((c[V + 4 >> 2] & -8 | 0) == (Ba | 0)) {
                                                Ga = V
                                            } else {
                                                S = Ba << Fa;
                                                ca = V;
                                                while (1) {
                                                    Ha = ca + (S >>> 31 << 2) + 16 | 0;
                                                    ea = c[Ha >> 2] | 0;
                                                    if ((ea | 0) == 0) {
                                                        break
                                                    }
                                                    if ((c[ea + 4 >> 2] & -8 | 0) == (Ba | 0)) {
                                                        Ga = ea;
                                                        break l
                                                    } else {
                                                        S = S << 1;
                                                        ca = ea
                                                    }
                                                }
                                                if (Ha >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                                    fa()
                                                } else {
                                                    c[Ha >> 2] = _;
                                                    c[ga + ($ + 24) >> 2] = ca;
                                                    c[ga + ($ + 12) >> 2] = _;
                                                    c[ga + ($ + 8) >> 2] = _;
                                                    break j
                                                }
                                            }
                                        } while (0);
                                        V = Ga + 8 | 0;
                                        S = c[V >> 2] | 0;
                                        L = c[10720 >> 2] | 0;
                                        if (Ga >>> 0 < L >>> 0) {
                                            fa()
                                        }
                                        if (S >>> 0 < L >>> 0) {
                                            fa()
                                        } else {
                                            c[S + 12 >> 2] = _;
                                            c[V >> 2] = _;
                                            c[ga + ($ + 8) >> 2] = S;
                                            c[ga + ($ + 12) >> 2] = Ga;
                                            c[ga + ($ + 24) >> 2] = 0;
                                            break
                                        }
                                    }
                                } while (0);
                                p = ga + (ua | 8) | 0;
                                i = b;
                                return p | 0
                            }
                        } while (0);
                        aa = na;
                        $ = 11152 | 0;
                        while (1) {
                            Ia = c[$ >> 2] | 0;
                            if (!(Ia >>> 0 > aa >>> 0)) {
                                Ja = c[$ + 4 >> 2] | 0;
                                Ka = Ia + Ja | 0;
                                if (Ka >>> 0 > aa >>> 0) {
                                    break
                                }
                            }
                            $ = c[$ + 8 >> 2] | 0
                        }
                        $ = Ia + (Ja + -39) | 0;
                        if (($ & 7 | 0) == 0) {
                            La = 0
                        } else {
                            La = 0 - $ & 7
                        }
                        $ = Ia + (Ja + -47 + La) | 0;
                        da = $ >>> 0 < (na + 16 | 0) >>> 0 ? aa : $;
                        $ = da + 8 | 0;
                        ba = $;
                        M = ga + 8 | 0;
                        if ((M & 7 | 0) == 0) {
                            Ma = 0
                        } else {
                            Ma = 0 - M & 7
                        }
                        M = ja + -40 - Ma | 0;
                        c[10728 >> 2] = ga + Ma;
                        c[10716 >> 2] = M;
                        c[ga + (Ma + 4) >> 2] = M | 1;
                        c[ga + (ja + -36) >> 2] = 40;
                        c[10732 >> 2] = c[11192 >> 2];
                        c[da + 4 >> 2] = 27;
                        c[$ + 0 >> 2] = c[11152 >> 2];
                        c[$ + 4 >> 2] = c[11156 >> 2];
                        c[$ + 8 >> 2] = c[11160 >> 2];
                        c[$ + 12 >> 2] = c[11164 >> 2];
                        c[11152 >> 2] = ga;
                        c[11156 >> 2] = ja;
                        c[11164 >> 2] = 0;
                        c[11160 >> 2] = ba;
                        ba = da + 28 | 0;
                        c[ba >> 2] = 7;
                        if ((da + 32 | 0) >>> 0 < Ka >>> 0) {
                            $ = ba;
                            while (1) {
                                ba = $ + 4 | 0;
                                c[ba >> 2] = 7;
                                if (($ + 8 | 0) >>> 0 < Ka >>> 0) {
                                    $ = ba
                                } else {
                                    break
                                }
                            }
                        }
                        if ((da | 0) == (aa | 0)) {
                            break
                        }
                        $ = da - na | 0;
                        ba = aa + ($ + 4) | 0;
                        c[ba >> 2] = c[ba >> 2] & -2;
                        c[na + 4 >> 2] = $ | 1;
                        c[aa + $ >> 2] = $;
                        ba = $ >>> 3;
                        if ($ >>> 0 < 256) {
                            M = ba << 1;
                            ma = 10744 + (M << 2) | 0;
                            W = c[2676] | 0;
                            m = 1 << ba;
                            do {
                                if ((W & m | 0) == 0) {
                                    c[2676] = W | m;
                                    Na = 10744 + (M + 2 << 2) | 0;
                                    Oa = ma
                                } else {
                                    ba = 10744 + (M + 2 << 2) | 0;
                                    S = c[ba >> 2] | 0;
                                    if (!(S >>> 0 < (c[10720 >> 2] | 0) >>> 0)) {
                                        Na = ba;
                                        Oa = S;
                                        break
                                    }
                                    fa()
                                }
                            } while (0);
                            c[Na >> 2] = na;
                            c[Oa + 12 >> 2] = na;
                            c[na + 8 >> 2] = Oa;
                            c[na + 12 >> 2] = ma;
                            break
                        }
                        M = na;
                        m = $ >>> 8;
                        do {
                            if ((m | 0) == 0) {
                                Pa = 0
                            } else {
                                if ($ >>> 0 > 16777215) {
                                    Pa = 31;
                                    break
                                }
                                W = (m + 1048320 | 0) >>> 16 & 8;
                                aa = m << W;
                                da = (aa + 520192 | 0) >>> 16 & 4;
                                S = aa << da;
                                aa = (S + 245760 | 0) >>> 16 & 2;
                                ba = 14 - (da | W | aa) + (S << aa >>> 15) | 0;
                                Pa = $ >>> (ba + 7 | 0) & 1 | ba << 1
                            }
                        } while (0);
                        m = 11008 + (Pa << 2) | 0;
                        c[na + 28 >> 2] = Pa;
                        c[na + 20 >> 2] = 0;
                        c[na + 16 >> 2] = 0;
                        ma = c[10708 >> 2] | 0;
                        ba = 1 << Pa;
                        if ((ma & ba | 0) == 0) {
                            c[10708 >> 2] = ma | ba;
                            c[m >> 2] = M;
                            c[na + 24 >> 2] = m;
                            c[na + 12 >> 2] = na;
                            c[na + 8 >> 2] = na;
                            break
                        }
                        ba = c[m >> 2] | 0;
                        if ((Pa | 0) == 31) {
                            Qa = 0
                        } else {
                            Qa = 25 - (Pa >>> 1) | 0
                        }
                        m: do {
                            if ((c[ba + 4 >> 2] & -8 | 0) == ($ | 0)) {
                                Ra = ba
                            } else {
                                m = $ << Qa;
                                ma = ba;
                                while (1) {
                                    Sa = ma + (m >>> 31 << 2) + 16 | 0;
                                    aa = c[Sa >> 2] | 0;
                                    if ((aa | 0) == 0) {
                                        break
                                    }
                                    if ((c[aa + 4 >> 2] & -8 | 0) == ($ | 0)) {
                                        Ra = aa;
                                        break m
                                    } else {
                                        m = m << 1;
                                        ma = aa
                                    }
                                }
                                if (Sa >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                    fa()
                                } else {
                                    c[Sa >> 2] = M;
                                    c[na + 24 >> 2] = ma;
                                    c[na + 12 >> 2] = na;
                                    c[na + 8 >> 2] = na;
                                    break i
                                }
                            }
                        } while (0);
                        $ = Ra + 8 | 0;
                        ba = c[$ >> 2] | 0;
                        m = c[10720 >> 2] | 0;
                        if (Ra >>> 0 < m >>> 0) {
                            fa()
                        }
                        if (ba >>> 0 < m >>> 0) {
                            fa()
                        } else {
                            c[ba + 12 >> 2] = M;
                            c[$ >> 2] = M;
                            c[na + 8 >> 2] = ba;
                            c[na + 12 >> 2] = Ra;
                            c[na + 24 >> 2] = 0;
                            break
                        }
                    }
                } while (0);
                na = c[10716 >> 2] | 0;
                if (!(na >>> 0 > q >>> 0)) {
                    break
                }
                ba = na - q | 0;
                c[10716 >> 2] = ba;
                na = c[10728 >> 2] | 0;
                $ = na;
                c[10728 >> 2] = $ + q;
                c[$ + (q + 4) >> 2] = ba | 1;
                c[na + 4 >> 2] = q | 3;
                p = na + 8 | 0;
                i = b;
                return p | 0
            }
        } while (0);
        c[(la() | 0) >> 2] = 12;
        p = 0;
        i = b;
        return p | 0
    }

    function Fa(a) {
        a = a | 0;
        var b = 0,
            d = 0,
            e = 0,
            f = 0,
            g = 0,
            h = 0,
            j = 0,
            k = 0,
            l = 0,
            m = 0,
            n = 0,
            o = 0,
            p = 0,
            q = 0,
            r = 0,
            s = 0,
            t = 0,
            u = 0,
            v = 0,
            w = 0,
            x = 0,
            y = 0,
            z = 0,
            A = 0,
            B = 0,
            C = 0,
            D = 0,
            E = 0,
            F = 0,
            G = 0,
            H = 0,
            I = 0,
            J = 0,
            K = 0,
            L = 0,
            M = 0,
            N = 0,
            O = 0,
            P = 0,
            Q = 0;
        b = i;
        if ((a | 0) == 0) {
            i = b;
            return
        }
        d = a + -8 | 0;
        e = d;
        f = c[10720 >> 2] | 0;
        if (d >>> 0 < f >>> 0) {
            fa()
        }
        g = c[a + -4 >> 2] | 0;
        h = g & 3;
        if ((h | 0) == 1) {
            fa()
        }
        j = g & -8;
        k = a + (j + -8) | 0;
        l = k;
        a: do {
            if ((g & 1 | 0) == 0) {
                m = c[d >> 2] | 0;
                if ((h | 0) == 0) {
                    i = b;
                    return
                }
                n = -8 - m | 0;
                o = a + n | 0;
                p = o;
                q = m + j | 0;
                if (o >>> 0 < f >>> 0) {
                    fa()
                }
                if ((p | 0) == (c[10724 >> 2] | 0)) {
                    r = a + (j + -4) | 0;
                    if ((c[r >> 2] & 3 | 0) != 3) {
                        s = p;
                        t = q;
                        break
                    }
                    c[10712 >> 2] = q;
                    c[r >> 2] = c[r >> 2] & -2;
                    c[a + (n + 4) >> 2] = q | 1;
                    c[k >> 2] = q;
                    i = b;
                    return
                }
                r = m >>> 3;
                if (m >>> 0 < 256) {
                    m = c[a + (n + 8) >> 2] | 0;
                    u = c[a + (n + 12) >> 2] | 0;
                    v = 10744 + (r << 1 << 2) | 0;
                    do {
                        if ((m | 0) != (v | 0)) {
                            if (m >>> 0 < f >>> 0) {
                                fa()
                            }
                            if ((c[m + 12 >> 2] | 0) == (p | 0)) {
                                break
                            }
                            fa()
                        }
                    } while (0);
                    if ((u | 0) == (m | 0)) {
                        c[2676] = c[2676] & ~(1 << r);
                        s = p;
                        t = q;
                        break
                    }
                    do {
                        if ((u | 0) == (v | 0)) {
                            w = u + 8 | 0
                        } else {
                            if (u >>> 0 < f >>> 0) {
                                fa()
                            }
                            x = u + 8 | 0;
                            if ((c[x >> 2] | 0) == (p | 0)) {
                                w = x;
                                break
                            }
                            fa()
                        }
                    } while (0);
                    c[m + 12 >> 2] = u;
                    c[w >> 2] = m;
                    s = p;
                    t = q;
                    break
                }
                v = o;
                r = c[a + (n + 24) >> 2] | 0;
                x = c[a + (n + 12) >> 2] | 0;
                do {
                    if ((x | 0) == (v | 0)) {
                        y = a + (n + 20) | 0;
                        z = c[y >> 2] | 0;
                        if ((z | 0) == 0) {
                            A = a + (n + 16) | 0;
                            B = c[A >> 2] | 0;
                            if ((B | 0) == 0) {
                                C = 0;
                                break
                            } else {
                                D = B;
                                E = A
                            }
                        } else {
                            D = z;
                            E = y
                        }
                        while (1) {
                            y = D + 20 | 0;
                            z = c[y >> 2] | 0;
                            if ((z | 0) != 0) {
                                E = y;
                                D = z;
                                continue
                            }
                            z = D + 16 | 0;
                            y = c[z >> 2] | 0;
                            if ((y | 0) == 0) {
                                break
                            } else {
                                D = y;
                                E = z
                            }
                        }
                        if (E >>> 0 < f >>> 0) {
                            fa()
                        } else {
                            c[E >> 2] = 0;
                            C = D;
                            break
                        }
                    } else {
                        z = c[a + (n + 8) >> 2] | 0;
                        if (z >>> 0 < f >>> 0) {
                            fa()
                        }
                        y = z + 12 | 0;
                        if ((c[y >> 2] | 0) != (v | 0)) {
                            fa()
                        }
                        A = x + 8 | 0;
                        if ((c[A >> 2] | 0) == (v | 0)) {
                            c[y >> 2] = x;
                            c[A >> 2] = z;
                            C = x;
                            break
                        } else {
                            fa()
                        }
                    }
                } while (0);
                if ((r | 0) == 0) {
                    s = p;
                    t = q;
                    break
                }
                x = c[a + (n + 28) >> 2] | 0;
                o = 11008 + (x << 2) | 0;
                do {
                    if ((v | 0) == (c[o >> 2] | 0)) {
                        c[o >> 2] = C;
                        if ((C | 0) != 0) {
                            break
                        }
                        c[10708 >> 2] = c[10708 >> 2] & ~(1 << x);
                        s = p;
                        t = q;
                        break a
                    } else {
                        if (r >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                            fa()
                        }
                        m = r + 16 | 0;
                        if ((c[m >> 2] | 0) == (v | 0)) {
                            c[m >> 2] = C
                        } else {
                            c[r + 20 >> 2] = C
                        } if ((C | 0) == 0) {
                            s = p;
                            t = q;
                            break a
                        }
                    }
                } while (0);
                if (C >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                    fa()
                }
                c[C + 24 >> 2] = r;
                v = c[a + (n + 16) >> 2] | 0;
                do {
                    if ((v | 0) != 0) {
                        if (v >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                            fa()
                        } else {
                            c[C + 16 >> 2] = v;
                            c[v + 24 >> 2] = C;
                            break
                        }
                    }
                } while (0);
                v = c[a + (n + 20) >> 2] | 0;
                if ((v | 0) == 0) {
                    s = p;
                    t = q;
                    break
                }
                if (v >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                    fa()
                } else {
                    c[C + 20 >> 2] = v;
                    c[v + 24 >> 2] = C;
                    s = p;
                    t = q;
                    break
                }
            } else {
                s = e;
                t = j
            }
        } while (0);
        e = s;
        if (!(e >>> 0 < k >>> 0)) {
            fa()
        }
        C = a + (j + -4) | 0;
        f = c[C >> 2] | 0;
        if ((f & 1 | 0) == 0) {
            fa()
        }
        do {
            if ((f & 2 | 0) == 0) {
                if ((l | 0) == (c[10728 >> 2] | 0)) {
                    D = (c[10716 >> 2] | 0) + t | 0;
                    c[10716 >> 2] = D;
                    c[10728 >> 2] = s;
                    c[s + 4 >> 2] = D | 1;
                    if ((s | 0) != (c[10724 >> 2] | 0)) {
                        i = b;
                        return
                    }
                    c[10724 >> 2] = 0;
                    c[10712 >> 2] = 0;
                    i = b;
                    return
                }
                if ((l | 0) == (c[10724 >> 2] | 0)) {
                    D = (c[10712 >> 2] | 0) + t | 0;
                    c[10712 >> 2] = D;
                    c[10724 >> 2] = s;
                    c[s + 4 >> 2] = D | 1;
                    c[e + D >> 2] = D;
                    i = b;
                    return
                }
                D = (f & -8) + t | 0;
                E = f >>> 3;
                b: do {
                    if (f >>> 0 < 256) {
                        w = c[a + j >> 2] | 0;
                        h = c[a + (j | 4) >> 2] | 0;
                        d = 10744 + (E << 1 << 2) | 0;
                        do {
                            if ((w | 0) != (d | 0)) {
                                if (w >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                    fa()
                                }
                                if ((c[w + 12 >> 2] | 0) == (l | 0)) {
                                    break
                                }
                                fa()
                            }
                        } while (0);
                        if ((h | 0) == (w | 0)) {
                            c[2676] = c[2676] & ~(1 << E);
                            break
                        }
                        do {
                            if ((h | 0) == (d | 0)) {
                                F = h + 8 | 0
                            } else {
                                if (h >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                    fa()
                                }
                                g = h + 8 | 0;
                                if ((c[g >> 2] | 0) == (l | 0)) {
                                    F = g;
                                    break
                                }
                                fa()
                            }
                        } while (0);
                        c[w + 12 >> 2] = h;
                        c[F >> 2] = w
                    } else {
                        d = k;
                        g = c[a + (j + 16) >> 2] | 0;
                        v = c[a + (j | 4) >> 2] | 0;
                        do {
                            if ((v | 0) == (d | 0)) {
                                r = a + (j + 12) | 0;
                                x = c[r >> 2] | 0;
                                if ((x | 0) == 0) {
                                    o = a + (j + 8) | 0;
                                    m = c[o >> 2] | 0;
                                    if ((m | 0) == 0) {
                                        G = 0;
                                        break
                                    } else {
                                        H = m;
                                        I = o
                                    }
                                } else {
                                    H = x;
                                    I = r
                                }
                                while (1) {
                                    r = H + 20 | 0;
                                    x = c[r >> 2] | 0;
                                    if ((x | 0) != 0) {
                                        I = r;
                                        H = x;
                                        continue
                                    }
                                    x = H + 16 | 0;
                                    r = c[x >> 2] | 0;
                                    if ((r | 0) == 0) {
                                        break
                                    } else {
                                        H = r;
                                        I = x
                                    }
                                }
                                if (I >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                    fa()
                                } else {
                                    c[I >> 2] = 0;
                                    G = H;
                                    break
                                }
                            } else {
                                x = c[a + j >> 2] | 0;
                                if (x >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                    fa()
                                }
                                r = x + 12 | 0;
                                if ((c[r >> 2] | 0) != (d | 0)) {
                                    fa()
                                }
                                o = v + 8 | 0;
                                if ((c[o >> 2] | 0) == (d | 0)) {
                                    c[r >> 2] = v;
                                    c[o >> 2] = x;
                                    G = v;
                                    break
                                } else {
                                    fa()
                                }
                            }
                        } while (0);
                        if ((g | 0) == 0) {
                            break
                        }
                        v = c[a + (j + 20) >> 2] | 0;
                        w = 11008 + (v << 2) | 0;
                        do {
                            if ((d | 0) == (c[w >> 2] | 0)) {
                                c[w >> 2] = G;
                                if ((G | 0) != 0) {
                                    break
                                }
                                c[10708 >> 2] = c[10708 >> 2] & ~(1 << v);
                                break b
                            } else {
                                if (g >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                    fa()
                                }
                                h = g + 16 | 0;
                                if ((c[h >> 2] | 0) == (d | 0)) {
                                    c[h >> 2] = G
                                } else {
                                    c[g + 20 >> 2] = G
                                } if ((G | 0) == 0) {
                                    break b
                                }
                            }
                        } while (0);
                        if (G >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                            fa()
                        }
                        c[G + 24 >> 2] = g;
                        d = c[a + (j + 8) >> 2] | 0;
                        do {
                            if ((d | 0) != 0) {
                                if (d >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                                    fa()
                                } else {
                                    c[G + 16 >> 2] = d;
                                    c[d + 24 >> 2] = G;
                                    break
                                }
                            }
                        } while (0);
                        d = c[a + (j + 12) >> 2] | 0;
                        if ((d | 0) == 0) {
                            break
                        }
                        if (d >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                            fa()
                        } else {
                            c[G + 20 >> 2] = d;
                            c[d + 24 >> 2] = G;
                            break
                        }
                    }
                } while (0);
                c[s + 4 >> 2] = D | 1;
                c[e + D >> 2] = D;
                if ((s | 0) != (c[10724 >> 2] | 0)) {
                    J = D;
                    break
                }
                c[10712 >> 2] = D;
                i = b;
                return
            } else {
                c[C >> 2] = f & -2;
                c[s + 4 >> 2] = t | 1;
                c[e + t >> 2] = t;
                J = t
            }
        } while (0);
        t = J >>> 3;
        if (J >>> 0 < 256) {
            e = t << 1;
            f = 10744 + (e << 2) | 0;
            C = c[2676] | 0;
            G = 1 << t;
            do {
                if ((C & G | 0) == 0) {
                    c[2676] = C | G;
                    K = 10744 + (e + 2 << 2) | 0;
                    L = f
                } else {
                    t = 10744 + (e + 2 << 2) | 0;
                    j = c[t >> 2] | 0;
                    if (!(j >>> 0 < (c[10720 >> 2] | 0) >>> 0)) {
                        K = t;
                        L = j;
                        break
                    }
                    fa()
                }
            } while (0);
            c[K >> 2] = s;
            c[L + 12 >> 2] = s;
            c[s + 8 >> 2] = L;
            c[s + 12 >> 2] = f;
            i = b;
            return
        }
        f = s;
        L = J >>> 8;
        do {
            if ((L | 0) == 0) {
                M = 0
            } else {
                if (J >>> 0 > 16777215) {
                    M = 31;
                    break
                }
                K = (L + 1048320 | 0) >>> 16 & 8;
                e = L << K;
                G = (e + 520192 | 0) >>> 16 & 4;
                C = e << G;
                e = (C + 245760 | 0) >>> 16 & 2;
                j = 14 - (G | K | e) + (C << e >>> 15) | 0;
                M = J >>> (j + 7 | 0) & 1 | j << 1
            }
        } while (0);
        L = 11008 + (M << 2) | 0;
        c[s + 28 >> 2] = M;
        c[s + 20 >> 2] = 0;
        c[s + 16 >> 2] = 0;
        j = c[10708 >> 2] | 0;
        e = 1 << M;
        c: do {
            if ((j & e | 0) == 0) {
                c[10708 >> 2] = j | e;
                c[L >> 2] = f;
                c[s + 24 >> 2] = L;
                c[s + 12 >> 2] = s;
                c[s + 8 >> 2] = s
            } else {
                C = c[L >> 2] | 0;
                if ((M | 0) == 31) {
                    N = 0
                } else {
                    N = 25 - (M >>> 1) | 0
                }
                d: do {
                    if ((c[C + 4 >> 2] & -8 | 0) == (J | 0)) {
                        O = C
                    } else {
                        K = J << N;
                        G = C;
                        while (1) {
                            P = G + (K >>> 31 << 2) + 16 | 0;
                            t = c[P >> 2] | 0;
                            if ((t | 0) == 0) {
                                break
                            }
                            if ((c[t + 4 >> 2] & -8 | 0) == (J | 0)) {
                                O = t;
                                break d
                            } else {
                                K = K << 1;
                                G = t
                            }
                        }
                        if (P >>> 0 < (c[10720 >> 2] | 0) >>> 0) {
                            fa()
                        } else {
                            c[P >> 2] = f;
                            c[s + 24 >> 2] = G;
                            c[s + 12 >> 2] = s;
                            c[s + 8 >> 2] = s;
                            break c
                        }
                    }
                } while (0);
                C = O + 8 | 0;
                D = c[C >> 2] | 0;
                K = c[10720 >> 2] | 0;
                if (O >>> 0 < K >>> 0) {
                    fa()
                }
                if (D >>> 0 < K >>> 0) {
                    fa()
                } else {
                    c[D + 12 >> 2] = f;
                    c[C >> 2] = f;
                    c[s + 8 >> 2] = D;
                    c[s + 12 >> 2] = O;
                    c[s + 24 >> 2] = 0;
                    break
                }
            }
        } while (0);
        s = (c[10736 >> 2] | 0) + -1 | 0;
        c[10736 >> 2] = s;
        if ((s | 0) == 0) {
            Q = 11160 | 0
        } else {
            i = b;
            return
        }
        while (1) {
            s = c[Q >> 2] | 0;
            if ((s | 0) == 0) {
                break
            } else {
                Q = s + 8 | 0
            }
        }
        c[10736 >> 2] = -1;
        i = b;
        return
    }

    function Ga() {}

    function Ha(b, d, e) {
        b = b | 0;
        d = d | 0;
        e = e | 0;
        var f = 0;
        if ((e | 0) >= 4096) return ja(b | 0, d | 0, e | 0) | 0;
        f = b | 0;
        if ((b & 3) == (d & 3)) {
            while (b & 3) {
                if ((e | 0) == 0) return f | 0;
                a[b] = a[d] | 0;
                b = b + 1 | 0;
                d = d + 1 | 0;
                e = e - 1 | 0
            }
            while ((e | 0) >= 4) {
                c[b >> 2] = c[d >> 2];
                b = b + 4 | 0;
                d = d + 4 | 0;
                e = e - 4 | 0
            }
        }
        while ((e | 0) > 0) {
            a[b] = a[d] | 0;
            b = b + 1 | 0;
            d = d + 1 | 0;
            e = e - 1 | 0
        }
        return f | 0
    }

    function Ia(b, d, e) {
        b = b | 0;
        d = d | 0;
        e = e | 0;
        var f = 0,
            g = 0,
            h = 0,
            i = 0;
        f = b + e | 0;
        if ((e | 0) >= 20) {
            d = d & 255;
            g = b & 3;
            h = d | d << 8 | d << 16 | d << 24;
            i = f & ~3;
            if (g) {
                g = b + 4 - g | 0;
                while ((b | 0) < (g | 0)) {
                    a[b] = d;
                    b = b + 1 | 0
                }
            }
            while ((b | 0) < (i | 0)) {
                c[b >> 2] = h;
                b = b + 4 | 0
            }
        }
        while ((b | 0) < (f | 0)) {
            a[b] = d;
            b = b + 1 | 0
        }
        return b - e | 0
    }

    function Ja(b) {
        b = b | 0;
        var c = 0;
        c = b;
        while (a[c] | 0) {
            c = c + 1 | 0
        }
        return c - b | 0
    }




    // EMSCRIPTEN_END_FUNCS
    return {
        _strlen: Ja,
        _free: Fa,
        _main: Da,
        _memset: Ia,
        _malloc: Ea,
        _memcpy: Ha,
        runPostSets: Ga,
        stackAlloc: na,
        stackSave: oa,
        stackRestore: pa,
        setThrew: qa,
        setTempRet0: ta,
        setTempRet1: ua,
        setTempRet2: va,
        setTempRet3: wa,
        setTempRet4: xa,
        setTempRet5: ya,
        setTempRet6: za,
        setTempRet7: Aa,
        setTempRet8: Ba,
        setTempRet9: Ca
    }
})


// EMSCRIPTEN_END_ASM
({
    "Math": Math,
    "Int8Array": Int8Array,
    "Int16Array": Int16Array,
    "Int32Array": Int32Array,
    "Uint8Array": Uint8Array,
    "Uint16Array": Uint16Array,
    "Uint32Array": Uint32Array,
    "Float32Array": Float32Array,
    "Float64Array": Float64Array
}, {
    "abort": abort,
    "assert": assert,
    "asmPrintInt": asmPrintInt,
    "asmPrintFloat": asmPrintFloat,
    "min": Math_min,
    "_clock": _clock,
    "_fflush": _fflush,
    "_abort": _abort,
    "___setErrNo": ___setErrNo,
    "_sbrk": _sbrk,
    "_time": _time,
    "_emscripten_memcpy_big": _emscripten_memcpy_big,
    "_sysconf": _sysconf,
    "___errno_location": ___errno_location,
    "STACKTOP": STACKTOP,
    "STACK_MAX": STACK_MAX,
    "tempDoublePtr": tempDoublePtr,
    "ABORT": ABORT,
    "NaN": NaN,
    "Infinity": Infinity
}, buffer);
var _strlen = Module["_strlen"] = asm["_strlen"];
var _free = Module["_free"] = asm["_free"];
var _main = Module["_main"] = asm["_main"];
var _memset = Module["_memset"] = asm["_memset"];
var _malloc = Module["_malloc"] = asm["_malloc"];
var _memcpy = Module["_memcpy"] = asm["_memcpy"];
var runPostSets = Module["runPostSets"] = asm["runPostSets"];

Runtime.stackAlloc = function (size) {
    return asm['stackAlloc'](size)
};
Runtime.stackSave = function () {
    return asm['stackSave']()
};
Runtime.stackRestore = function (top) {
    asm['stackRestore'](top)
};


// Warning: printing of i64 values may be slightly rounded! No deep i64 math used, so precise i64 code not included
var i64Math = null;

// === Auto-generated postamble setup entry stuff ===

if (memoryInitializer) {
    if (ENVIRONMENT_IS_NODE || ENVIRONMENT_IS_SHELL) {
        var data = Module['readBinary'](memoryInitializer);
        HEAPU8.set(data, STATIC_BASE);
    } else {
        addRunDependency('memory initializer');
        Browser.asyncLoad(memoryInitializer, function (data) {
            HEAPU8.set(data, STATIC_BASE);
            removeRunDependency('memory initializer');
        }, function (data) {
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
    if (!Module['calledRun'] && shouldRunNow) run();
    if (!Module['calledRun']) dependenciesFulfilled = runCaller; // try this again later, after new deps are fulfilled
}

Module['callMain'] = Module.callMain = function callMain(args) {
    assert(runDependencies == 0, 'cannot call main when async dependencies remain! (listen on __ATMAIN__)');
    assert(__ATPRERUN__.length == 0, 'cannot call main when preRun functions remain to be called');

    args = args || [];

    if (ENVIRONMENT_IS_WEB && preloadStartTime !== null) {
        Module.printErr('preload time: ' + (Date.now() - preloadStartTime) + ' ms');
    }

    ensureInitRuntime();

    var argc = args.length + 1;

    function pad() {
        for (var i = 0; i < 4 - 1; i++) {
            argv.push(0);
        }
    }
    var argv = [allocate(intArrayFromString("/bin/this.program"), 'i8', ALLOC_NORMAL)];
    pad();
    for (var i = 0; i < argc - 1; i = i + 1) {
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
    } catch (e) {
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

        if (Module['_main'] && shouldRunNow) {
            Module['callMain'](args);
        }

        postRun();
    }

    if (Module['setStatus']) {
        Module['setStatus']('Running...');
        setTimeout(function () {
            setTimeout(function () {
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


run();

// {{POST_RUN_ADDITIONS}}




// {{MODULE_ADDITIONS}}

////////////////////////////////////////////////////////////////////////////////
// Runner
////////////////////////////////////////////////////////////////////////////////
var __time_after = top.JetStream.goodTime();
top.JetStream.reportResult(__time_after - __time_before);