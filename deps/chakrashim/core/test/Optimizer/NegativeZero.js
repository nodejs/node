//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var GenerateNegativeZeroScheme = {
    Neg: 0,
    Mul: 1
};

var IntOperation = {
    None: 0,
    Incr: 1,
    Decr: 2,
    Not: 3,
    And: 4,
    Or: 5,
    Xor: 6,
    Shl: 7,
    Shr: 8,
    ShrU: 9
};

var MakeNegativeZeroMatterLocation = {
    None: 0,
    BeforeIntOperation: 1,
    AfterIntOperation: 2
};

var MakeNegativeZeroMatterScheme = {
    Div: 0,
    Throw: 1,
    Ret: 2,
    PassToFunction: 3,
    SetPropertyInLocalObject: 4,
    SetPropertyInNonlocalObjectInScope: 5,
    SetNonlocalVariableInScope: 6,
    SetArrayElement: 7
};

function makeNegativeZeroMatter(scheme, srcVar) {
    var s = "";
    if(scheme === MakeNegativeZeroMatterScheme.Div)
        s += "throw new Error(1 / c < 0 ? '-0' : '0');";
    else if(scheme === MakeNegativeZeroMatterScheme.Throw)
        s += "throw c;";
    else if(scheme === MakeNegativeZeroMatterScheme.Ret)
        s += "return c;";
    else if(scheme === MakeNegativeZeroMatterScheme.PassToFunction)
        s += "echo(c);";
    else if(scheme === MakeNegativeZeroMatterScheme.SetPropertyInLocalObject)
        s += "localObj.p = c;";
    else if(scheme === MakeNegativeZeroMatterScheme.SetPropertyInNonlocalObjectInScope)
        s += "outerObj.p = c;";
    else if(scheme === MakeNegativeZeroMatterScheme.SetNonlocalVariableInScope)
        s += "outerVar = c;";
    else if(scheme === MakeNegativeZeroMatterScheme.SetArrayElement)
        s += "localArr[0] = c;";
    else
        throw new Error("Unknown MakeNegativeZeroMatterScheme");
    return s;
}

function generateAndRunTest(
    generateNegativeZeroScheme,
    intOperation,
    makeNegativeZeroMatterLocation,
    makeNegativeZeroMatterScheme) {

    var f = "(function outer(a, b){";
    f += "var outerObj = {p: 0};"
    f += "var outerVar = 0;";
    f += "return (function test(a, b){";
    f += "a >>= 14;";
    f += "b &= 0x3fff;";
    f += "var localObj = {p: 0};";
    f += "var localArr = [0];";

    if(generateNegativeZeroScheme == GenerateNegativeZeroScheme.Neg)
        f += "var c = -b;";
    else if(generateNegativeZeroScheme == GenerateNegativeZeroScheme.Mul)
        f += "var c = a * b;";
    else
        throw new Error("Unknown GenerateNegativeZeroScheme");

    if(makeNegativeZeroMatterLocation === MakeNegativeZeroMatterLocation.BeforeIntOperation)
        f += makeNegativeZeroMatter(makeNegativeZeroMatterScheme);

    if(intOperation === IntOperation.None)
        f += "var d = c;";
    else if(intOperation === IntOperation.Incr)
        f += "var d = ++c;";
    else if(intOperation === IntOperation.Decr)
        f += "var d = --c;";
    else if(intOperation === IntOperation.Not)
        f += "var d = ~c;";
    else if(intOperation === IntOperation.And)
        f += "var d = c & 0xfffffff;";
    else if(intOperation === IntOperation.Or)
        f += "var d = c | 0xfffffff;";
    else if(intOperation === IntOperation.Xor)
        f += "var d = c ^ 0xfffffff;";
    else if(intOperation === IntOperation.Shl)
        f += "var d = c << 1;";
    else if(intOperation === IntOperation.Shr)
        f += "var d = c >> 1;";
    else if(intOperation === IntOperation.ShrU)
        f += "var d = c >>> 1;";
    else
        throw new Error("Unknown IntOperation");

    if(makeNegativeZeroMatterLocation === MakeNegativeZeroMatterLocation.AfterIntOperation)
        f += makeNegativeZeroMatter(makeNegativeZeroMatterScheme);

    f += "return [d & 0xfffffff, localObj.p, localArr[0], outerObj.p, outerVar];";
    f += "}).apply(this, arguments);";
    f += "});";
    f = eval(f);
    echo(f.toString());

    try {
        echo("Returned: ", f(-2 << 14, 0));
    } catch(ex) {
        echo("Thrown: ", ex);
    }
}

for(var generateNegativeZeroSchemeIndex in GenerateNegativeZeroScheme) {
    var generateNegativeZeroScheme = GenerateNegativeZeroScheme[generateNegativeZeroSchemeIndex];
    for(var intOperationIndex in IntOperation) {
        var intOperation = IntOperation[intOperationIndex];
        for(var makeNegativeZeroMatterLocationIndex in MakeNegativeZeroMatterLocation) {
            var makeNegativeZeroMatterLocation = MakeNegativeZeroMatterLocation[makeNegativeZeroMatterLocationIndex];
            if(makeNegativeZeroMatterLocation === MakeNegativeZeroMatterLocation.None) {
                var makeNegativeZeroMatterScheme = MakeNegativeZeroMatterScheme[makeNegativeZeroMatterSchemeIndex];
                echo("generateAndRunTest(" + intOperationIndex + ", " + makeNegativeZeroMatterLocationIndex + ");");
                generateAndRunTest(generateNegativeZeroScheme, intOperation, makeNegativeZeroMatterLocation);
                echo();
            } else {
                for(var makeNegativeZeroMatterSchemeIndex in MakeNegativeZeroMatterScheme) {
                    var makeNegativeZeroMatterScheme = MakeNegativeZeroMatterScheme[makeNegativeZeroMatterSchemeIndex];
                    echo(
                    "generateAndRunTest(" +
                    intOperationIndex + ", " +
                    makeNegativeZeroMatterLocationIndex + ", " +
                    makeNegativeZeroMatterSchemeIndex + ");");
                    generateAndRunTest(
                        generateNegativeZeroScheme,
                        intOperation,
                        makeNegativeZeroMatterLocation,
                        makeNegativeZeroMatterScheme);
                    echo();
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function myToString(o, quoteStrings) {
    switch(o) {
        case null:
        case undefined:
            return "" + o;
    }

    switch(typeof o) {
        case "boolean":
            return "" + o;

        case "number":
            {
                if(o === 0 && 1 / o < 0)
                    return "-0";
                var s = "" + o;
                var i = s.indexOf("e");
                var end = i === -1 ? s.length : e;
                i = s.indexOf(".");
                var start = i === -1 ? 0 : i + 1;
                if(start !== 0) {
                    if((end - start) % 3 !== 0)
                        end += 3 - (end - start) % 3;
                    for(i = end - 3; i > start; i -= 3)
                        s = s.substring(0, i) + "," + s.substring(i);
                    end = start - 1;
                    start = 0;
                }
                for(i = end - 3; i > start; i -= 3)
                    s = s.substring(0, i) + "," + s.substring(i);
                return s;
            }

        case "string":
            {
                var hex = "0123456789abcdef";
                var s = "";
                for(var i = 0; i < o.length; ++i) {
                    var c = o.charCodeAt(i);
                    switch(c) {
                        case 0x0:
                            s += "\\0";
                            continue;
                        case 0x8:
                            s += "\\b";
                            continue;
                        case 0xb:
                            s += "\\v";
                            continue;
                        case 0xc:
                            s += "\\f";
                            continue;
                    }
                    if(quoteStrings) {
                        switch(c) {
                            case 0x9:
                                s += "\\t";
                                continue;
                            case 0xa:
                                s += "\\n";
                                continue;
                            case 0xd:
                                s += "\\r";
                                continue;
                            case 0x22:
                                s += "\\\"";
                                continue;
                            case 0x5c:
                                s += "\\\\";
                                continue;
                        }
                    }
                    if(c >= 0x20 && c < 0x7f)
                        s += o.charAt(i);
                    else if(c <= 0xff)
                        s += "\\x" + hex.charAt((c >> 4) & 0xf) + hex.charAt(c & 0xf);
                    else
                        s += "\\u" + hex.charAt((c >> 12) & 0xf) + hex.charAt((c >> 8) & 0xf) + hex.charAt((c >> 4) & 0xf) + hex.charAt(c & 0xf);
                }
                if(quoteStrings)
                    s = "\"" + s + "\"";
                return s;
            }

        case "object":
        case "function":
            break;

        default:
            return "<unknown type '" + typeof o + "'>";
    }

    if(o instanceof Array) {
        var s = "[";
        for(var i = 0; i < o.length; ++i) {
            if(i)
                s += ", ";
            s += myToString(o[i], true);
        }
        return s + "]";
    }
    if(o instanceof Error)
        return o.name + ": " + o.message;
    if(o instanceof RegExp)
        return o.toString() + (o.lastIndex === 0 ? "" : " (lastIndex: " + o.lastIndex + ")");
    if(o instanceof Object && !(o instanceof Function)) {
        var s = "";
        for(var p in o)
            s += myToString(p) + ": " + myToString(o[p], true) + ", ";
        if(s.length !== 0)
            s = s.substring(0, s.length - ", ".length);
        return "{" + s + "}";
    }
    return "" + o;
}

function echo() {
    var doEcho;
    if(this.WScript)
        doEcho = function (s) { this.WScript.Echo(s); };
    else if(this.document)
        doEcho =
            function (s) {
                s = s
                    .replace(/&/g, "&&")
                    .replace(/</g, "&lt;")
                    .replace(/>/g, "&gt;");
                this.document.write(s + "<br/>");
            };
    else
        doEcho = function (s) { this.print(s); };
    echo =
        function () {
            var s = "";
            for(var i = 0; i < arguments.length; ++i)
                s += myToString(arguments[i]);
            doEcho(s);
        };
    echo.apply(this, arguments);
}

function safeCall(f) {
    var args = [];
    for(var a = 1; a < arguments.length; ++a)
        args.push(arguments[a]);
    try {
        return f.apply(this, args);
    } catch(ex) {
        echo(ex);
    }
}
