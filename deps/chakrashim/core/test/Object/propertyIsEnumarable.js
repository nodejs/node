//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function foo() { this.y = 10; }
    
var o = new Object();
var f = new foo();
var a = new Array();
var s = new String("hello");
var b = new Boolean(true);
var n = new Number(10);
var d = new Date();
var r = new RegExp();
var e = new Error();

o.x = f.x = foo.x = a.x = s.x = b.x = n.x = d.x = r.x = e.x = 10;

function doEval(str)
{
    //write(str);
    write(str + " : " + eval(str));
}

// Check for standard properties on various built-in constructors
function Test1() {
    var objs = [
        "Object", "Function", "Array", "String", "Boolean", "Number", "Math", "Date", "RegExp", "Error",
        "Object.prototype", "Function.prototype", "Array.prototype", "String.prototype", "Boolean.prototype",
        "Number.prototype", "Date.prototype", "RegExp.prototype", "Error.prototype",
        "o", "f", "foo", "foo.prototype", "a", "s", "b", "n", "d", "r", "e"
    ]
    
    var props = [
        "abs", "acos", "anchor", "apply", "Array", "asin", "atan", "atan2", "big", "bind",
        "blink", "bold", "Boolean", "call", "ceil", "charAt", "charCodeAt", "concat", "constructor", "cos",
        "Date", "decodeURI", "decodeURIComponent", "description", "E", "encodeURI", "encodeURIComponent", "escape", "Error", "eval", "EvalError",
        "every", "exec", "exp", "flags", "filter", "fixed", "floor", "fontcolor", "fontsize", "forEach",
        "fromCharCode", "Function", "getDate", "getDay", "getFullYear", "getHours", "getMilliseconds", "getMinutes", "getMonth", "getSeconds",
        "getTime", "getTimezoneOffset", "getUTCDate", "getUTCDay", "getUTCFullYear", "getUTCHours", "getUTCMilliseconds", "getUTCMinutes", "getUTCMonth",
        "getUTCSeconds", "getYear", "hasOwnProperty", "indexOf", "Infinity", "isFinite", "isNaN", "isPrototypeOf", "italics", "join", "lastIndexOf",
        "length", "link", "LN10", "LN2", "localeCompare", "log", "LOG10E", "LOG2E", "map", "Math",
        "max", "MAX_VALUE", "match", "message", "min", "MIN_VALUE", "NaN", "name", "Now", "Number",
        "number", "NEGATIVE_INFINITY", "Object", "parse", "parseFloat", "parseInt", "PI", "pop", "POSITIVE_INFINITY", "pow",
        "propertyIsEnumerable", "prototype", "push", "random", "RangeError", "reduce", "reduceRight", "ReferenceError", "replace", "reverse",
        "round", "RegExp", "search", "setDate", "setFullYear", "setHours", "setMilliseconds", "setMinutes", "setMonth", "setSeconds",
        "setTime", "setUTCDate", "setUTCFullYear", "setUTCHours", "setUTCMilliseconds", "setUTCMinutes", "setUTCMonth", "setUTCSeconds", "setYear", "shift",
        "sin", "slice", "some", "sort", "source", "splice", "split", "sqrt", "SQRT1_2", "SQRT2",
        "strike", "String", "sub", "substring", "substr", "sup", "SyntaxError", "tan", "test", "toDateString",
        "toExponential", "toFixed", "toISOString", "toJSON", "toLocaleDateString", "toLocaleLowerCase", "toLocaleString", "toLocaleTimeString",
        "toLocaleUpperCase", "toLowerCase", "toPrecision", "toString", "toTimeString", "toUpperCase", "toUTCString", "trim", "TypeError", "undefined",
        "unescape", "unshift", "URIError", "UTC", "valueOf", "enumerable", "configurable", "writable", "value", "get", "set", "defineProperty",
        "defineProperties", "toGMTString", "compile", "global", "lastIndex", "multiline", "ignoreCase", "index", "input",
        "lastMatch", "lastParen", "leftContext", "rightContext",
        "x", "y"
    ];
    
    for (var i=0; i<objs.length; i++)
    {
        for (var j=0; j< props.length; j++)
        {
            doEval(objs[i] + ".propertyIsEnumerable(\"" + props[j] + "\")");
        }
    }
}
   
function Test2() {
    function base() {
        this.x = "base.x";
        this.y = "base.y";
    }
    
    function derived() {
        this.y = "derived.y";
        this.z = "derived.z";
    }
    derived.prototype = new base();
    
    var d = new derived();
    
    write("Test2 d.propertyIsEnumerable(x): " + d.propertyIsEnumerable("x"));
    write("Test2 d.propertyIsEnumerable(y): " + d.propertyIsEnumerable("y"));
    write("Test2 d.propertyIsEnumerable(z): " + d.propertyIsEnumerable("z"));
    
    write("Test2 d.hasOwnProperty(x): " + d.hasOwnProperty("x"));
    write("Test2 d.hasOwnProperty(y): " + d.hasOwnProperty("y"));
    write("Test2 d.hasOwnProperty(z): " + d.hasOwnProperty("z"));    
}

function Test3() {
    try {
        write(Object.prototype.propertyIsEnumerable.call(undefined, "length"));
    } catch (e) {
        write("Exception: " + e + " " + e.message);
    }
    
    try {
        write(Object.prototype.propertyIsEnumerable.call(null, "length"));
    } catch (e) {
        write("Exception: " + e + " " + e.message);
    }
}
var re = new RegExp("d(b+)(d)", "ig");
function TestRegex()
{
 var propso = [
    "lastIndex", "source", "global", "ignoreCase", "multiline", "options"];

 var props = [
     "input","$_","index","lastIndex","lastMatch",'$&',"lastParen",'$+','$`',"rightContext",
      "$'","$2","$3","$4","$5","$6","$7","$8","$9"];


        for (var j=0; j< props.length; j++)
        {
            doEval("re" + ".propertyIsEnumerable(\"" + propso[j] + "\")");
        }

        for (var j=0; j< props.length; j++)
        {
            doEval("RegExp" + ".propertyIsEnumerable(\"" + props[j] + "\")");
        }

}


Test1();
Test2();
Test3();
TestRegex();
