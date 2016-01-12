//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var verifyMemoryReclaiming = false;
var forceGc = this.CollectGarbage && !verifyMemoryReclaiming;
var nextIndex = 0;

function runTest(numRegexes) {
    var k, s, r;
    for(var i = 0; i < 2; ++i) {
        for(var j = 0; j < numRegexes; ++j) {
            if(verifyMemoryReclaiming && nextIndex & 1)
                k = nextIndex;
            else
                k = j;
            ++nextIndex;
            k = verifyMemoryReclaiming ? nextIndex++ : j;
            s = "a\u0100acdef" + k + " aaababc" + k;
            r = new RegExp("a[a\\u0100](?:ab|ac)(?:abc|def|ghi|jkl)" + k, "g");
            if(verifyMemoryReclaiming)
                s.match(r);
            else
                match(r, s);
        }
        if(forceGc)
            CollectGarbage();
    }
}

do {
    runTest(8);
    runTest(16);
    runTest(32);
    runTest(64);
} while(verifyMemoryReclaiming);

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function exec(r, s) {
    echo("exec(", r, ", ", myToString(s, true), ");");
    var result = r.exec(s);
    echo(result);
    return result;
}

function test(r, s) {
    echo("test(", r, ", ", myToString(s, true), ");");
    var result = r.test(s);
    echo(result);
    return result;
}

function match(r, s) {
    echo("match(", r, ", ", myToString(s, true), ");");
    var result = s.match(r);
    echo(result);
    return result;
}

function replace(r, s, w) {
    echo("replace(", myToString(r, true), ", ", myToString(s, true), ", ", myToString(w, true), ");");
    var result = s.replace(r, w);
    echo(result);
    return result;
}

function split(r, s) {
    echo("split(", r, ", ", myToString(s, true), ");");
    var result = s.split(r);
    echo(result);
    return result;
}

function search(r, s) {
    echo("search(", r, ", ", myToString(s, true), ");");
    var result = s.search(r);
    echo(result);
    return result;
}
