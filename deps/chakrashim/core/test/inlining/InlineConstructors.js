//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var tests = [
    function () { return new Constructor(); },
    function () { return new Constructor(0, 1); },
    function () { return new Constructor(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16); },
    function () { return new Constructor(0, new Constructor(0, 1).p, new ApplyArgsConstructor(0, 1).p); },
    function () { return new ApplyArgsConstructor(); },
    function () { return new ApplyArgsConstructor(0, 1); },
    function () { return new ApplyArgsConstructor(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16); },
    function () { return new ApplyArgsConstructor(0, new Constructor(0, 1).p, new ApplyArgsConstructor(0, 1).p); },
    function () { return new RecursiveConstructor(); },
    function () { return new RecursiveConstructor(0, 1); },
    function () { return new RecursiveConstructor(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16); },
    function () { return new RecursiveConstructor(0, new Constructor(0, 1).p, new ApplyArgsConstructor(0, 1).p); },
    function () { return new RecursiveApplyArgsConstructor(); },
    function () { return new RecursiveApplyArgsConstructor(0, 1); },
    function () { return new RecursiveApplyArgsConstructor(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16); },
    function () { return new RecursiveApplyArgsConstructor(0, new Constructor(0, 1).p, new ApplyArgsConstructor(0, 1).p); },
    function () { return new ReturningConstructor(); },
    function () { return new ReturningConstructor(0, 1); },
    function () { return new ReturningConstructor(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16); },
    function () { return new ReturningConstructor(0, new Constructor(0, 1).p, new ApplyArgsConstructor(0, 1).p); },
    function () { return new ReturningApplyArgsConstructor(); },
    function () { return new ReturningApplyArgsConstructor(0, 1); },
    function () { return new ReturningApplyArgsConstructor(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16); },
    function () { return new ReturningApplyArgsConstructor(0, new Constructor(0, 1).p, new ApplyArgsConstructor(0, 1).p); },
    function () { return new ReturningRecursiveConstructor(); },
    function () { return new ReturningRecursiveConstructor(0, 1); },
    function () { return new ReturningRecursiveConstructor(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16); },
    function () { return new ReturningRecursiveConstructor(0, new Constructor(0, 1).p, new ApplyArgsConstructor(0, 1).p); },
    function () { return new ReturningRecursiveApplyArgsConstructor(); },
    function () { return new ReturningRecursiveApplyArgsConstructor(0, 1); },
    function () { return new ReturningRecursiveApplyArgsConstructor(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16); },
    function () { return new ReturningRecursiveApplyArgsConstructor(0, new Constructor(0, 1).p, new ApplyArgsConstructor(0, 1).p); },
];

for(var i = 0; i < tests.length; ++i) {
    echo(tests[i]);
    echo("New object: ", tests[i]());
    echo();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test helpers

function Constructor(a) {
    echo(arguments);
    this.p = a ? a : 0;
}

function ApplyArgsConstructor() {
    Constructor.apply(this, arguments);
}

function RecursiveConstructor(a) {
    echo(arguments);
    this.p = a ? new RecursiveConstructor(0).p : new Constructor(a).p;
}

function RecursiveApplyArgsConstructor(a) {
    echo(arguments);
    this.p = a ? new RecursiveApplyArgsConstructor(0).p : new ApplyArgsConstructor(a).p;
}

function ReturningConstructor(a) {
    echo(arguments);
    return { q: a ? a : 0 };
}

function ReturningApplyArgsConstructor() {
    return ReturningConstructor.apply(this, arguments);
}

function ReturningRecursiveConstructor(a) {
    echo(arguments);
    return { q: a ? new ReturningRecursiveConstructor(0).q : new ReturningConstructor(a).q };
}

function ReturningRecursiveApplyArgsConstructor(a) {
    echo(arguments);
    return { q: a ? new ReturningRecursiveApplyArgsConstructor(0).q : new ReturningApplyArgsConstructor(a).q };
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function myToString(o, quoteStrings) {
    switch(o) {
        case null:
        case undefined:
        case -Infinity:
        case Infinity:
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
        doEcho = function (s) {
            var div = this.document.createElement("div");
            div.innerText = s;
            this.document.body.appendChild(div);
        };
    else
        doEcho = function (s) { this.print(s); };
    echo = function () {
        var s = "";
        for(var i = 0; i < arguments.length; ++i)
            s += myToString(arguments[i]);
        doEcho(s);
    };
    echo.apply(this, arguments);
}
