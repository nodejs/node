//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var sources = [
    "/(?:)/",
    "",
    "(?:)",

    "/a\\tb/",
    "a\tb",
    "a\\tb",

    "/a\\nb/",
    "a\nb",
    "a\\nb",

    "/a\\x0ab/",
    "a\x0ab",
    "a\\x0ab",

    "/a\\u000ab/",
    "a\u000ab",
    "a\\u000ab"
];
var sourceIndex = 0;

var flags = ["g", "i", "m", "gi", "ig", "gm", "mg", "im", "mi", "gim", "gmi", "igm", "img", "mgi", "mig"];
var flagIndex = 0;

var n = Math.max(sources.length, flags.length);
for(var i = 0; i < n; ++i) {
    var s = sources[sourceIndex++ % sources.length];
    var f = flags[flagIndex++ % flags.length];
    var r;
    if(s.charAt(0) === "/")
        r = eval(s + f);
    else
        r = new RegExp(s, f);
    runTest(r);
}

function runTest(r) {
    echo(r.source);
    echo(r.toString());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
            s += arguments[i];
        doEcho(s);
    };
    echo.apply(this, arguments);
}
