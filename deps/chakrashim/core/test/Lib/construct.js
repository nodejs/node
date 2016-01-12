//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var errorCount = 0;

// global

var globalList = [
    ["eval",1],
    ["parseInt",2],
    ["parseFloat",1],
    ["isNaN",1],
    ["isFinite",1],
    ["decodeURI",1],
    ["encodeURI",1],
    ["decodeURIComponent",1],
    ["encodeURIComponent",1],
    ["Object",0],
    ["Function",1],
    ["Array",1],
    ["String",1],
    ["Boolean",1],
    ["Number",1],
    ["Date",7],
    ["RegExp",2],
    ["Error",1],
    ["EvalError",1],
    ["RangeError",1],
    ["ReferenceError",1],
    ["SyntaxError",1],
    ["TypeError",1],
    ["URIError",1]
]

// Object
var objList = [
    //TODO ["constructor",0],
    ["toString",0],
    ["toLocaleString",0],
    ["valueOf",0],
    ["hasOwnProperty",1] ,
    ["isPrototypeOf",1] ,
    ["propertyIsEnumerable",1]
]

// Function
var funcList = [
    ["constructor",1],
    ["toString",0],
    ["apply",2 ],
    ["call", 1]
]

// Array
var arrList = [
    // FuncName, length
    ["constructor",1],
    ["toString",0],
    ["toLocaleString", 0],
    ["concat", 1],
    ["join",1],
    ["pop",0],
    ["push",1],
    ["reverse",0],
    ["shift",0],
    ["slice",2],
    ["sort",1],
    ["splice",2],
    ["unshift",1]
];


// String
var stringList = [
    ["constructor",1],
    ["toString",0],
    ["valueOf",0],
    ["charAt",1],
    ["charCodeAt",1],
    ["concat",1],
    ["indexOf",2],
    ["lastIndexOf",2],
    ["localeCompare",1],
    ["match",1],
    ["replace",1],
    ["search",0],
    ["slice",0],
    ["split",2],
    ["substring",2],
    ["toLowerCase",0],
    ["toLocaleLowerCase",0],
    ["toUpperCase",0],
    ["toLocaleUpperCase",0],

    // not in ECMA spec

    ["anchor",1],
    ["big",0],
    ["blink",0],
    ["bold",0],
    ["fixed",0],
    ["fontcolor",1],
    ["fontsize",1],
    ["italics",0],
    ["link",1],
    ["small",0],
    ["strike",0],
    ["sub",0],
    ["sup",0],
    ["substr",2]
]

var stringList2 = [
    ["fromCharCode", 0]
];

// Boolean
var boolList = [
    ["constructor",1],
    ["toString",0],
    ["valueOf",0]
]

//Number

var numberList = [
    ["constructor",1],
    ["toString",1],
    ["toLocaleString",0],
    ["valueOf",0],
    ["toFixed",1],
    ["toExponential",1],
    ["toPrecision",1]
]

// Math object is a single object  - 18
var mathList = [

    ["abs",1],
    ["acos",1],
    ["asin",1],
    ["atan",1],
    ["atan2",2],
    ["ceil",1],
    ["cos",1],
    ["exp",1],
    ["floor",1],
    ["log",1],
    ["max",2],
    ["min",2],
    ["pow",2],
    ["random",0],
    ["round",1],
    ["sin",1],
    ["sqrt",1],
    ["tan",1]
]

// Date Prototype object
var dateList = [
    ["constructor",7],

    ["toString",0],
    ["toDateString",0],
    ["toTimeString",0],
    ["toLocaleString",0],
    ["toLocaleDateString",0],

    ["valueOf",0],

    ["getTime",0],

    ["getFullYear",0],
    ["getUTCFullYear",0],

    ["getMonth",0],
    ["getUTCMonth",0],

    ["getDate",0],
    ["getUTCDate",0],

    ["getDay",0],
    ["getUTCDay",0],

    ["getHours",0],
    ["getUTCHours",0],

    ["getMinutes",0],
    ["getUTCMinutes",0],

    ["getSeconds",0],
    ["getUTCSeconds",0],

    ["getMilliseconds",0],
    ["getUTCMilliseconds",0],

    ["getTimezoneOffset",0],

    ["setTime",1],

    ["setMilliseconds",1],
    ["setUTCMilliseconds",1],

    ["setSeconds",2],
    ["setUTCSeconds",2],

    ["setMinutes",3],
    ["setUTCMinutes",3],

    ["setHours",4],
    ["setUTCHours",4],

    ["setDate",1],
    ["setUTCDate",1],

    ["setMonth",2],
    ["setUTCMonth",2],

    ["setFullYear",3],
    ["setUTCFullYear",3],

    ["toUTCString",0],

    // not in ECMA spec

    ["toGMTString",0],
    ["toLocaleTimeString",0 ],
    ["toUTCString",0],

    ["setYear",1],
    ["getYear",0]

    // TODO ["getVarDate",0]
]

// static date functions
var dateList2 = [
    ["parse",1],
    ["UTC",7]
]



// RegExp object

var regxpList = [
    ["constructor",2],
    ["exec",1],
    ["test",1],
    ["toString",0] //,
    // not in ECMA spec
    // TODO ["compile",2]

]


// Error object

var errorList = [
    ["constructor",1],
    ["name",5],
    ["message",0],
    ["toString",0]
]


function doEval(scen) {
    try {
        var res = eval(scen);
        write("No Exception: " + scen);
    } catch (e) {
        write("   Exception: " + scen + ".  :: " + e.message);
    }
}

var somevar = 0;

function TestNew(list, str) {
    for (var i=0; i<list.length; i++) {
        var fname = list[i][0];
        doEval("somevar = new " + str+fname+"()");
    }
}

TestNew(globalList,"");

TestNew(objList, "Object.prototype.");

TestNew(funcList, "Function.prototype.");

TestNew(arrList, "Array.prototype.");

TestNew(stringList, "String.prototype.");
TestNew(stringList2, "String.");

TestNew(boolList, "Boolean.prototype.");

TestNew(numberList, "Number.prototype.");

TestNew(mathList, "Math.");

TestNew(dateList, "Date.prototype.");
TestNew(dateList2,"Date.");

TestNew(regxpList, "RegExp.prototype.");

TestNew(errorList, "Error.prototype.");


if ( errorCount > 0 ) {
    throw Error(errorCount + " Test(s) Failed");
}
