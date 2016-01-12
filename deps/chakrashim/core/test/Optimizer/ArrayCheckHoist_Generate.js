//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var echoFunctionString = echo.toString();

echo("// The tests in this file are GENERATED. Don't add tests to this file manually; instead, modify");
echo("// ArrayCheckHoist_Generate.js and regenerate this file, or use a different file for the new test.");
echo();

var ArrayType = {
    ObjectWithArray: 0,
    Array: 1,

    LastJsArray: 1,

    Int32Array: 2
};

var JsArrayElementType = {
    Object: 0,
    Int32: 1,
    Float64: 2
};

var ArrayAccessType = {
    Local: 0,
    Field: 1
};

var ElementAccessType = {
    Load: 0,
    Store: 1
};

var NonInvariantLoopType = {
    None: 0,
    Redefine: 1,
    Call: 2,
    ImplicitCall: 3
};

var Indexes = [
    "-1",
    "0",
    "i"
];

var Bools = [
    false,
    true
];

// Test helper constants and variables
var LoopIterations = 2;
function ChangeToEs5ArrayStatement(a, jsArrayElementType) {
    var v = "-" + a + "[0]";
    if(jsArrayElementType == JsArrayElementType.Object)
        v = "{ p: " + v + ".p - 1 }";
    else
        v += " - 1";
    return "Object.defineProperty(" + a + ", 0, { configurable: true, writable: false, enumerable: true, value: " + v + " });";
}
var indent = 0;
var testIndex = 0;

echo("var bailout = !this.WScript || this.WScript.Arguments[0] === \"bailout\";");
echo();

var indexIndex = 0;
var inlineIndex = 0;
var strictIndex = 0;
var indexChanger = 0;
for(var arrayTypeIndex in ArrayType) {
    if(arrayTypeIndex.substring(0, 4) === "Last")
        continue;
    var arrayType = ArrayType[arrayTypeIndex];
    for(var jsArrayElementTypeIndex in JsArrayElementType) {
        var jsArrayElementType = JsArrayElementType[jsArrayElementTypeIndex];
        if(arrayType === ArrayType.Int32Array && jsArrayElementType !== JsArrayElementType.Int32)
            continue;
        for(var arrayAccessTypeIndex in ArrayAccessType) {
            var arrayAccessType = ArrayAccessType[arrayAccessTypeIndex];
            for(var elementAccessTypeIndex in ElementAccessType) {
                var elementAccessType = ElementAccessType[elementAccessTypeIndex];
                for(var nonInvariantLoopTypeIndex in NonInvariantLoopType) {
                    var nonInvariantLoopType = NonInvariantLoopType[nonInvariantLoopTypeIndex];
                    for(var numLoops = 1; numLoops <= 4; numLoops *= 2) {
                        for(var arrayAccessLoopIndex = 0;
                            arrayAccessLoopIndex < numLoops;
                            arrayAccessLoopIndex = (arrayAccessLoopIndex + 1) * 2 - 1) {

                            var arrayAccess2LoopIndex = numLoops - 1 - arrayAccessLoopIndex;
                            for(var nonInvariantLoopIndex = 0;
                                nonInvariantLoopIndex < numLoops;
                                nonInvariantLoopIndex = (nonInvariantLoopIndex + 1) * 2 - 1) {

                                var index = Indexes[indexIndex % Indexes.length];
                                var inline = Bools[inlineIndex % Bools.length];
                                var strict = Bools[strictIndex % Bools.length];

                                ++indexChanger;
                                if(!(indexChanger & 0x1))++indexIndex;
                                if(!(indexChanger & 0x3))++inlineIndex;
                                if(!(indexChanger & 0xf))++strictIndex;

                                generate(
                                    arrayType,
                                    jsArrayElementType,
                                    arrayAccessType,
                                    elementAccessType,
                                    nonInvariantLoopType,
                                    numLoops,
                                    arrayAccessLoopIndex,
                                    arrayAccess2LoopIndex,
                                    nonInvariantLoopIndex,
                                    index,
                                    inline,
                                    strict);
                            }
                        }
                    }
                }
            }
        }
    }
}

echo("////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////");
echo();

var f = "";
f += echos("function changeToEs5Array_object(a) {", 1);
{
    f += preventInline(ChangeToEs5ArrayStatement("a", JsArrayElementType.Object));
}
f += echos("}", -1);
echo(f);

f = "";
f += echos("function changeToEs5Array_int32(a) {", 1);
{
    f += preventInline(ChangeToEs5ArrayStatement("a", JsArrayElementType.Int32));
}
f += echos("}", -1);
echo(f);

f = "";
f += echos("function someCall(a) {", 1);
{
    f += preventInline("a.someProperty = 0;");
}
f += echos("}", -1);
echo(f);

echo(toSafeInt.toString());
echo();
echo(echoFunctionString);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test helpers

function generate(
    arrayType,
    jsArrayElementType,
    arrayAccessType,
    elementAccessType,
    nonInvariantLoopType,
    numLoops,
    arrayAccessLoopIndex,
    arrayAccess2LoopIndex,
    nonInvariantLoopIndex,
    index,
    inline,
    strict) {

    if(numLoops === 0)
        throw new Error("Invalid numLoops");
    if(arrayAccessLoopIndex < 0 || arrayAccessLoopIndex >= numLoops)
        throw new Error("Invalid arrayAccessLoopIndex");
    if(arrayAccess2LoopIndex < 0 || arrayAccess2LoopIndex >= numLoops)
        throw new Error("Invalid arrayAccess2LoopIndex");
    if(nonInvariantLoopIndex < 0 || nonInvariantLoopIndex >= numLoops)
        throw new Error("Invalid nonInvariantLoopIndex");

    if(nonInvariantLoopType === NonInvariantLoopType.None && nonInvariantLoopIndex !== 0)
        return; // no non-invariant loops, only generate this test once
    if(arrayType >= ArrayType.LastJsArray && nonInvariantLoopType === NonInvariantLoopType.ImplicitCall)
        return; // typed arrays don't need implicit call checks, and their elements are not configurable
    if(strict && elementAccessType === ElementAccessType.Store && nonInvariantLoopType >= NonInvariantLoopType.Call)
        strict = false; // calls and implicit calls make elements read-only and those elements can't be set in strict mode thereafter

    var f = "";
    var testName = "test" + testIndex++;
    f += echos("function " + testName + "() {", 1);
    {
        if(strict) {
            f += echos('"use strict";');
            f += echos("");
        }

        f += createArray(arrayType, jsArrayElementType, nonInvariantLoopType === NonInvariantLoopType.ImplicitCall);
        f += echos("return toSafeInt(" + testName + "_run(o, a, a2));");
        f += echos("");

        f += echos("function " + testName + "_run(o, a, a2) {", 1);
        {
            f += echos("var sum = 0;");
            for(var i = 0; i < numLoops; ++i) {
                var si = "i" + i;
                var n = nonInvariantLoopType !== NonInvariantLoopType.None && i === nonInvariantLoopIndex ? LoopIterations : "a.length";
                f += echos("for(var " + si + " = 0; " + si + " < " + n + "; ++" + si + ") {", 1);
                {
                    f += echos("sum += " + si + ";");
                }
            }
            for(var i = numLoops - 1; i >= 0; --i) {
                var si = "i" + i;
                if(nonInvariantLoopType !== NonInvariantLoopType.None && i === nonInvariantLoopIndex) {
                    if(nonInvariantLoopType === NonInvariantLoopType.Redefine)
                        f += echos("a = a2;");
                    else if(nonInvariantLoopType === NonInvariantLoopType.Call) {
                        if(arrayType <= ArrayType.LastJsArray)
                            f += echos("changeToEs5Array_" + (jsArrayElementType === JsArrayElementType.Object ? "object" : "int32") + "(a);");
                        else
                            f += echos("someCall(a);");
                    }
                    else if(nonInvariantLoopType === NonInvariantLoopType.ImplicitCall) {
                        var n = nonInvariantLoopType !== NonInvariantLoopType.None && i === nonInvariantLoopIndex ? LoopIterations : "a.length";
                        f += echos("if(bailout && " + si + " === (" + n + " >> 1))", 1);
                        {
                            f += echos("o.changeToEs5Array = 0;");
                        }
                        f += echos(null, -1);
                    }
                    else
                        throw new Error("Invalid nonInvariantLoopType");
                }
                if(i === arrayAccessLoopIndex || i === arrayAccess2LoopIndex) {
                    if(inline)
                        f += echos("sum += " + testName + "_access(o, a, " + si + ");");
                    else
                        f += access(arrayType, jsArrayElementType, arrayAccessType, elementAccessType, index === "i" ? si : index);
                }
                f += echos("}", -1);
            }

            if(inline) {
                f += echos("");
                f += echos("function " + testName + "_access(o, a, i) {", 1);
                {
                    f += access(arrayType, jsArrayElementType, arrayAccessType, elementAccessType, index, true);
                }
                f += echos("}", -1);
            }

            f += echos("return sum;");
        }
        f += echos("}", -1);
    }
    f += echos("}", -1);
    f += echos("echo(\"" + testName + ": \" + " + testName + "());");
    echo(f);
}

function createArray(arrayType, jsArrayElementType, includeImplicitCall) {
    var f = "";
    if(arrayType === ArrayType.Int32Array) {
        f += echos("var o = { a: new Int32Array(" + LoopIterations + ") };");
        f += echos("for(var i = 0; i < " + LoopIterations + "; ++i)", 1);
        {
            f += echos("o.a[i] = i + 1;");
        }
        f += echos(null, -1);
    } else {
        if(arrayType > ArrayType.LastJsArray)
            throw new Error("Unknown ArrayType");
        f += echos("var o = {", 1);
        {
            var arrayBegin = arrayType === ArrayType.ObjectWithArray ? "{" : "[";
            var arrayEnd = arrayType === ArrayType.ObjectWithArray ? "}" : "]";
            function arrayElementBegin(i) {
                return arrayType === ArrayType.ObjectWithArray ? "\"" + i + "\": " : "";
            }

            var a = "a: " + arrayBegin + " ";
            for(var i = 1; i <= LoopIterations; ++i) {
                var end = i === LoopIterations ? "" : ", ";
                a += arrayElementBegin(i - 1);
                if(jsArrayElementType === JsArrayElementType.Object)
                    a += "{ p: " + i + " }" + end;
                else if(jsArrayElementType === JsArrayElementType.Int32)
                    a += i + end;
                else if(jsArrayElementType === JsArrayElementType.Float64)
                    a += i + ".1" + end;
                else
                    throw new Error("Unknown JsArrayElementType");
            }
            if(arrayType === ArrayType.ObjectWithArray)
                a += ", length: " + LoopIterations;
            if(includeImplicitCall) {
                f += echos(a + " " + arrayEnd + ",");
                f += echos("set changeToEs5Array(v) {", 1);
                {
                    f += preventInline(ChangeToEs5ArrayStatement("this.a", jsArrayElementType));
                }
                f += echos("}", -1);
            }
            else
                f += echos(a + " " + arrayEnd);
        }
        f += echos("};", -1);
    }

    f += echos("var a = o.a;");
    f += echos("a[-1] = a[0];");
    if(arrayType === ArrayType.ObjectWithArray)
        f += echos("var a2 = [];");
    else
        f += echos("var a2 = { length: a.length };");
    f += echos("for(var i = -1; i < a.length; ++i)", 1);
    {
        if(arrayType <= ArrayType.LastJsArray && jsArrayElementType === JsArrayElementType.Object)
            f += echos("a2[i] = { p: -a[i].p };");
        else
            f += echos("a2[i] = -a[i];");
    }
    f += echos(null, -1);
    return f;
}

function access(arrayType, jsArrayElementType, arrayAccessType, elementAccessType, index, returnAccessed) {
    var f = "";
    var e = "[" + index + "]";

    if(arrayType <= ArrayType.LastJsArray && jsArrayElementType === JsArrayElementType.Object)
        e += ".p";

    if(arrayAccessType === ArrayAccessType.Local)
        e = "a" + e;
    else if(arrayAccessType === ArrayAccessType.Field)
        e = "o.a" + e;
    else
        throw new Error("Unknown ArrayAccessType");

    var beginning = returnAccessed ? "return " : "sum += ";
    if(elementAccessType === ElementAccessType.Load)
        f += echos(beginning + e + ";");
    else if(elementAccessType === ElementAccessType.Store)
        f += echos(beginning + "(" + e + " = -" + e + " - 1, " + e + ");");
    else
        throw new Error("Unknown ElementAccessType");

    return f;
}

function preventInline(functionBodyLine) {
    var f = "";
    f += echos("try {", 1);
    {
        f += echos(functionBodyLine);
    }
    f += echos("} catch(ex) {", -1);
    f += echos(null, 1);
    {
        f += echos('echo("Unexpected exception - " + ex.name + ": " + ex.message);');
    }
    f += echos("}", -1);
    return f;
}

function echos(s, changeIndent) {
    if(changeIndent && changeIndent < 0)
        indent += changeIndent * 4;
    if(s && s !== "") {
        var spaces = "";
        for(var i = 0; i < indent; ++i)
            spaces += " ";
        s = spaces + s + "\n";
    }
    else if(s === "")
        s = "\n";
    else
        s = "";
    if(changeIndent && changeIndent > 0)
        indent += changeIndent * 4;
    return s;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function toSafeInt(n) {
    return Math.round(Math.round(n * 10) / 10);
}

function echo() {
    var doEcho;
    if(this.WScript)
        doEcho = function(s) { this.WScript.Echo(s); };
    else if(this.document)
        doEcho = function(s) {
            var div = this.document.createElement("div");
            div.innerText = s;
            this.document.body.appendChild(div);
        };
    else
        doEcho = function(s) { this.print(s); };
    echo = function() {
        var s = "";
        for(var i = 0; i < arguments.length; ++i)
            s += arguments[i];
        doEcho(s);
    };
    echo.apply(this, arguments);
}
