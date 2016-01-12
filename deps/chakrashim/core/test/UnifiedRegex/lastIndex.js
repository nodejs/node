//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// BEGIN PRELUDE
function echo(o) {
    try {
        document.write(o + "<br/>");
        echo = function(o) { document.write(o + "<br/>"); };
    } catch (ex) {
        try {
            WScript.Echo("" + o);
            echo = function(o) { WScript.Echo("" + o); };
        } catch (ex2) {
            print("" + o);
            echo = function(o) { print("" + o); };
        }
    }
}

var suppressLastIndex = false;
var suppressRegExp = false;
var suppressIndex = false;

function safeCall(f) {
    var args = [];
    for (var a = 1; a < arguments.length; ++a)
        args.push(arguments[a]);
    try {
        return f.apply(this, args);
    } catch (ex) {
        echo("EXCEPTION");
    }
}

hex = "0123456789abcdef";

function dump(o) {
    var sb = [];
    if (o === null)
        sb.push("null");
    else if (o === undefined)
        sb.push("undefined");
    else if (o === true)
        sb.push("true");
    else if (o === false)
        sb.push("false");
    else if (typeof o === "number")
        sb.push(o.toString());
    else if (typeof o == "string") {
        if (o.length > 8192)
            sb.push("<long string>");
        else {
            sb.push("\"");
            var start = -1;
            for (var i = 0; i < o.length; i++) {
                var c = o.charCodeAt(i);
                if (c < 32 || c > 127 || c == '"'.charCodeAt(0) || c == '\\'.charCodeAt(0)) {
                    if (start >= 0)
                        sb.push(o.substring(start, i));
                    start = -1;
                    sb.push("\\u");
                    sb.push(String.fromCharCode(hex.charCodeAt((c >> 12) & 0xf)));
                    sb.push(String.fromCharCode(hex.charCodeAt((c >> 8) & 0xf)));
                    sb.push(String.fromCharCode(hex.charCodeAt((c >> 4) & 0xf)));
                    sb.push(String.fromCharCode(hex.charCodeAt((c >> 0) & 0xf)));
                }
                else {
                    if (start < 0)
                        start = i;
                }
            }
            if (start >= 0)
                sb.push(o.substring(start, o.length));
            sb.push("\"");
        }
    }
    else if (o instanceof RegExp) {
        var body = o.source;
        sb.push("/");
        var start = -1;
        for (var i = 0; i < body.length; i++) {
            var c = body.charCodeAt(i);
            if (c < 32 || c > 127) {
                if (start >= 0)
                    sb.push(body.substring(start, i));
                start = -1;
                sb.push("\\u");
                sb.push(String.fromCharCode(hex.charCodeAt((c >> 12) & 0xf)));
                sb.push(String.fromCharCode(hex.charCodeAt((c >> 8) & 0xf)));
                sb.push(String.fromCharCode(hex.charCodeAt((c >> 4) & 0xf)));
                sb.push(String.fromCharCode(hex.charCodeAt((c >> 0) & 0xf)));
            }
            else {
                if (start < 0)
                    start = i;
            }
        }
        if (start >= 0)
            sb.push(body.substring(start, body.length));
        sb.push("/");
        if (o.global)
            sb.push("g");
        if (o.ignoreCase)
            sb.push("i");
        if (o.multiline)
            sb.push("m");
        if (!suppressLastIndex && o.lastIndex !== undefined) {
            sb.push(" /*lastIndex=");
            sb.push(o.lastIndex);
            sb.push("*/ ");
        }
    }
    else if (o.length !== undefined) {
        sb.push("[");
        for (var i = 0; i < o.length; i++) {
            if (i > 0)
                sb.push(",");
            sb.push(dump(o[i]));
        }
        sb.push("]");
        if (!suppressIndex && (o.input !== undefined || o.index !== undefined))
        {
            sb.push(" /*input=");
            sb.push(dump(o.input));
            sb.push(", index=");
            sb.push(dump(o.index));
            // IE only
            // sb.push(", lastIndex=");
            // sb.push(dump(o.lastIndex));
            sb.push("*/ ");
        }
    }
    else if (o.toString !== undefined) {
        sb.push("<object with toString>");
    }
    else
        sb.push(o.toString());
    return sb.join("");
}

function pre(w, origargs, n) {
    var sb = [w];
    sb.push("(");
    for (var i = 0; i < n; i++) {
        if (i > 0) sb.push(", ");
        sb.push(dump(origargs[i]));
    }
    if (origargs.length > n) {
        sb.push(", ");
        sb.push(dump(origargs[n]));
        origargs[0].lastIndex = origargs[n];
    }
    sb.push(");");
    echo(sb.join(""));
}

function post(r) {
    if (!suppressLastIndex) {
        echo("r.lastIndex=" + dump(r.lastIndex));
    }
    if (!suppressRegExp) {
        // IE only
        // echo("RegExp.index=" + dump(RegExp.index));
        // echo("RegExp.lastIndex=" + dump(RegExp.lastIndex));
        var sb = [];
        sb.push("RegExp.${_,1,...,9}=[");
        sb.push(dump(RegExp.$_));
        for (var i = 1; i <= 9; i++) {
            sb.push(",");
            sb.push(dump(RegExp["$" + i]));
        }
        sb.push("]");
        echo(sb.join(""));
    }
}

function exec(r, s) {
    pre("exec", arguments, 2);
    echo(dump(r.exec(s)));
    post(r);
}

function test(r, s) {
    pre("test", arguments, 2);
    echo(dump(r.test(s)));
    post(r);
}

function replace(r, s, o) {
    pre("replace", arguments, 3);
    echo(dump(s.replace(r, o)));
    post(r);
}

function split(r, s) {
    pre("split", arguments, 2);
    echo(dump(s.split(r)));
    post(r);
}

function match(r, s) {
    pre("match", arguments, 2);
    echo(dump(s.match(r)));
    post(r);
}

function search(r, s) {
    pre("search", arguments, 2);
    echo(dump(s.search(r)));
    post(r);
}

function bogus(r, o) {
    echo("bogus(" + dump(r) + ", " + dump(o) + ");");
    try { new RegExp(r, o); echo("FAILED"); } catch (e) { echo("PASSED"); }
}
// END PRELUDE

var rl = /x/;
var rg = /x/g;
var rel = /(?:)/;
var reg = /(?:)/g;
var s1 = "axbxcxd";
var s2 = "abc";

echo("************");
echo("*** Exec ***");
echo("************");
echo("****** Local regex, matching string");
exec(rl, s1, 0);
exec(rl, s1, 4);
echo("****** Global regex, matching string");
exec(rg, s1, 0);
exec(rg, s1, 4);
echo("****** Local regex, non-matching string");
exec(rl, s2, 0);
suppressLastIndex = true;  // v8.exe bug: does not reset lastIndex
exec(rl, s2, 4);
suppressLastIndex = false;
echo("****** Global regex, non-matching string");
exec(rg, s2, 0);
exec(rg, s2, 4);
echo("****** Local empty regex");
exec(rel, s1, 0);
exec(rel, s1, 4);
echo("****** Global empty regex");
exec(reg, s1, 0);
exec(reg, s1, 4);
echo("****** Special cases");
exec(/^/, "");

echo("************");
echo("*** Test ***");
echo("************");
echo("****** Local regex, matching string");
rl.lastIndex = 0;
test(rl, s1, 0);
test(rl, s1, 4);
echo("****** Global regex, matching string");
test(rg, s1, 0);
test(rg, s1, 4);
echo("****** Local regex, non-matching string");
test(rl, s2, 0);
suppressLastIndex = true;  // v8.exe bug: does not reset lastIndex
test(rl, s2, 4);
suppressLastIndex = false;
echo("****** Global regex, non-matching string");
test(rg, s2, 0);
test(rg, s2, 4);
echo("****** Local empty regex");
test(rel, s1, 0);
test(rel, s1, 4);
echo("****** Global empty regex");
test(reg, s1, 0);
test(reg, s1, 4);

echo("*************");
echo("*** Match ***");
echo("*************");
echo("****** Local regex, matching string");
rl.lastIndex = 0;
match(rl, s1, 0);
match(rl, s1, 4);
echo("****** Global regex, matching string");
match(rg, s1, 0);
suppressLastIndex = true;  // v8.exe bug: does not reset lastIndex
match(rg, s1, 4);
suppressLastIndex = false;
echo("****** Local regex, non-matching string");
match(rl, s2, 0);
suppressLastIndex = true;  // v8.exe bug: does not reset lastIndex
match(rl, s2, 4);
suppressLastIndex = false;
echo("****** Global regex, non-matching string");
rg.lastIndex = 0;
match(rg, s2, 0);
suppressLastIndex = true;  // v8.exe bug: does not reset lastIndex
match(rg, s2, 4);
suppressLastIndex = false;
echo("****** Local empty regex");
match(rel, s1, 0);
match(rel, s1, 4);
echo("****** Global empty regex");
match(reg, s1, 0);
suppressLastIndex = true;  // v8.exe bug: does not reset lastIndex
match(reg, s1, 4);
suppressLastIndex = false;

// Weird case
match(/(a)/gi, "A");

echo("**************");
echo("*** Search ***");
echo("**************");
echo("****** Local regex, matching string");
rl.lastIndex = 0;
search(rl, s1, 0);
search(rl, s1, 4);
echo("****** Global regex, matching string");
rg.lastIndex = 0;
search(rg, s1, 0);
search(rg, s1, 4);
echo("****** Local regex, non-matching string");
search(rl, s2, 0);
search(rl, s2, 4);
echo("****** Global regex, non-matching string");
search(rg, s2, 0);
search(rg, s2, 4);
echo("****** Local empty regex");
search(rel, s1, 0);
search(rel, s1, 4);
echo("****** Global empty regex");
reg.lastIndex = 0;
search(reg, s1, 0);
search(reg, s1, 4);

echo("*************");
echo("*** Split ***");
echo("*************");

echo("****** Special cases");
split(/a/, "a");
split(/(\b)?/, "a");
// split(/(\b|aa)?/, "aa"); // v8.exe bug: incorrectly matches at beginning and end of line, does not insert groups


echo("*************************");
echo("*** Setting lastIndex ***");
echo("*************************");

exec(rg, s1, "0");
exec(rg, s1, "4");
exec(rg, s1, 0.0);
exec(rg, s1, 4.0);
exec(rg, s1, 0.4);
exec(rg, s1, 0.5);
exec(rg, s1, 3.7);
exec(rg, s1, -4.0);
exec(rg, s1, 2147483647);
exec(rg, s1, Number.NaN);
exec(rg, s1, Number.NEGATIVE_INFINITY);
exec(rg, s1, Number.POSITIVE_INFINITY);
exec(rg, s1, Number.MIN_VALUE);
exec(rg, s1, 2298473438738.99723);
// exec(rg, s1, Number.MAX_VALUE);   // cscript decides this is a valid index of 0... why?...
exec(rg, s1, "bogus");
exec(rg, s1, [3,2,1]);
exec(rg, s1, null);
exec(rg, s1, undefined);
exec(rg, s1, true);
exec(rg, s1, false);
exec(rg, s1, { toString: function() { return "4"; } });

echo("*********************************");
echo("*** lastIndex on result array ***");
echo("*********************************");

var r = /a?/;
echo(r.exec("b").lastIndex);

var r = /a?/g;
echo("b".match(r).lastIndex);
