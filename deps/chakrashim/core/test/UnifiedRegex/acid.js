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

//bogus("a[]]b", "");
//bogus("a[^]b]c");
//bogus("(b.)c(?!\N)", "s");
//bogus("([a-\d]+)", "");
//bogus("a*(*FAIL)", "");
//bogus("a*(*F)", "");
//bogus("(A(A|B(*ACCEPT)|C)D)(E)", "");

exec(/(((a+a?)*)+b+)/, "aaaab");
exec(/.*a(.*aaa.*)a.*/, "aaaaa");
exec(/a/, "xxxxxxxxxxxxaxxxxxxx");
exec(/abcd/, "xxxxxxxabcdxxxxxxxxxx");

// exec(/(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))(?:(a)|(a))b/, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaac");

exec(/(?:r?)*?r|(.{2,4})/, "abcde");
exec(/((\3|b)\2(a)x)+/, "aaxabxbaxbbx");
exec(/^((.)?a\2)+$/, "babadad");
exec(/^(a\1?){4}$/, "aaaaaaaaaa");
exec(/(b.)c(?!\N)/, "a\nb\nc\n");
exec(/^(a\1?){4}$/, "aaaaaa");
exec(/((\3|b)\2(a)){2,}/, "bbaababbabaaaaabbaaaabba");
exec(/^$/, "");
exec(/\n/, "\n");
exec(/(\3)(\1)(a)/, "cat");
exec(/ab(?:cd)/, "abcd");
exec(/[a]bc[d]/, "abcd");
exec(/a(?:b)(?:cd)/, "abcd");
exec(/(?:a)bc(?:d)/, "abcd");
exec(/([^a-z]+)/i, "AB12CD34");
exec(/([^A-Z]+)/, "AB12CD34");
exec(/a|ab/, "abc");
exec(/((a)|(ab))((c)|(bc))/, "xxabcyy");
exec(/a[a-z]{2,4}/, "abcdefghi");
exec(/a[a-z]{2,4}?/, "abcdefghi");
exec(/(aa|aabaac|ba|b|c)*/, "aabaac");
exec(/(z)((a+)?(b+)?(c))*/, "zaacbbbcac");
exec(/(a*)*/, "b");
exec(/(a*)b\1+/, "baaaac");
exec(/abc*de/, "xxabdeyy");
exec(/a*bcde/, "xxaaabcdeyy");
exec(/abcde*/, "xxabcdeeeyy");
exec(/(?=(a+))/, "baaabac");
exec(/(?=(a+))a*b\1/, "baaabac");
exec(/(.*?)aab/, "baaab");
exec(/(.*?)a(?!(a+)b\2c)\2(.*)/, "baaabaac");
exec(/a(.*)a/, "baaaaab");
exec(/a(.*?)a/, "baaaaab");
exec(/([^a-z]*)([a-z\u2000-\u23ff]*)/, "--AbC--");
exec(/([^a-z]*)([a-z\u2000-\u23ff]*)/i, "--AbC--");
exec(/^[\],:{}\s]*$/, "]\x0a] ] ]{]\x0a] ] ] ] ]]:] ]4]0]9]6]\x0a] ] ]}]]");
exec(/"[^"\\\n\r]*"|true|false|null|-?\d+(?:\.\d*)?(:?[eE][+\-]?\d+)?/g, "[\x0a  {\x0a    \"tag\": \"titillation\",\x0a    \"popularity\": 4294967296\x0a  },\x0a  {\x0a    \"tag\": \"foamless\",\x0a    \"popularity\": 1257718401\x0a  } ]");
exec(/(^|.)(ronl|qri-ehf3.wbg)(|fgberf|zbgbef|yvirnhpgvbaf|jvxv|rkcerff|punggre).(pbz(|.nh|.pa|.ux|.zl|.ft|.oe|.zk)|pb(.hx|.xe|.am)|pn|qr|se|vg|ay|or|ng|pu|vr|va|rf|cy|cu|fr)$/i, "cntrf.ronl.pbz");


exec(/function\s*([^(]*)([^{]*\))/, 
"function Common__taskManager$_process$i() {\x0d\x0a        if (this._hasStopped) {\x0d\x0a            return;\x0d\x0a        }\x0d\x0a        if (this._isYielding) {\x0d\x0a            window.setTimeout(this._processDelegate, Common._taskManager._tickInterval);\x0d\x0a            return;\x0d\x0a        }\x0d\x0a    var timeNow = new Date();\x0d\x0a        var nextInterval = Common._taskManager._tickInterval;\x0d\x0a        var timeRemaining = Common._taskManager._tickInterval;\x0d\x0a        var drift = (timeNow.getTime() - this._lastProcessedTime.getTime()) - Common._taskManager._tickInterval;\x0d\x0a        this._lastProcessedTime = timeNow;\x0d\x0a        if (drift > (nextInterval / 2)) {\x0d\x0a            drift = 0;\x0d\x0a        }\x0d\x0a        if (drift < 0 || drift > (nextInterval / 2)) {\x0d\x0a            drift = 0;\x0d\x0a        }\x0d\x0a        var lastKeyStroke = timeNow;\x0d\x0a   if (Common._aFrameworkApplication.get__activeFrame$i()) {\x0d\x0a            lastKeyStroke = Common._aFrameworkApplication.get__activeFrame$i()._theKeyInputManager$i.get__lastKeyStroke$i();\x0d\x0a        }\x0d\x0a        if (!this._pauseRef && timeNow.getTime() - lastKeyStroke.getTime() >= Common._taskManager._tickInterval * 5) {\x0d\x0a            this._isProcessing = true;\x0d\x0a            for (this._processingQueue = 0; this._processingQueue < Common._taskItemPriority.lastPriority; this._processingQueue++) {\x0d\x0a                var maxTask = this._queues[this._processingQueue].length;\x0d\x0a                if (!maxTask) {\x0d\x0a                    continue;\x0d\x0a                }\x0d\x0a        var currentTask = this._lastProcessedTasks[this._processingQueue];\x0d\x0a                var tasksToProcess = maxTask;\x0d\x0a                do {\x0d\x0a                    currentTask = currentTask + 1;\x0d\x0a currentTask = (currentTask >= maxTask) ? 0 : currentTask;\x0d\x0a                    Common.Debug._assertTag$i(currentTask >= 0 && currentTask < maxTask, 964309305);\x0d\x0a                    var task = this._queues[this._processingQueue][currentTask];\x0d\x0a                    if (task && !task.get__isDeleted$i() && timeNow.getTime() - task.get__lastProcessed$i().getTime() > task.get__interval$i() - drift) {\x0d\x0a                        var startTime = new Date();\x0d\x0a                        if (task.get__taskType$i() !== Common._taskItemType.continuous) {\x0d\x0a       Array.removeAt(this._queues[this._processingQueue], currentTask);\x0d\x0a                            maxTask--;\x0d\x0a                            currentTask--;\x0d\x0a                        }\x0d\x0a                        task._process$i();\x0d\x0a                        var endTime = new Date();\x0d\x0a                        timeRemaining -= endTime.getTime() - startTime.getTime();\x0d\x0a                    }\x0d\x0a                    tasksToProcess--;\x0d\x0a               } while (maxTask > 0 && tasksToProcess > 0 && timeRemaining > 0);\x0d\x0a                if (this._pendingDeletes) {\x0d\x0a                    for (var iTask = 0; iTask < maxTask; iTask++) {\x0d\x0a                        var task = this._queues[this._processingQueue][iTask];\x0d\x0a                        if (task.get__isDeleted$i()) {\x0d\x0a                            Array.removeAt(this._queues[this._processingQueue], iTask);\x0d\x0a     if (iTask <= currentTask) {\x0d\x0a                                currentTask--;\x0d\x0a  }\x0d\x0a                            iTask--;\x0d\x0a                            maxTask--;\x0d\x0a     }\x0d\x0a                    }\x0d\x0a                    this._pendingDeletes = false;\x0d\x0a                }\x0d\x0a                this._lastProcessedTasks[this._processingQueue] = currentTask;\x0d\x0a                if (timeRemaining <= 0) {\x0d\x0a                    break;\x0d\x0a                }\x0d\x0a            }\x0d\x0a            this._isProcessing = false;\x0d\x0a        }\x0d\x0a        if (timeRemaining < 0 || (Common._taskManager._tickInterval - timeRemaining) < 0) {\x0d\x0a            timeRemaining = 0;\x0d\x0a        }\x0d\x0a        nextInterval = nextInterval - drift - (Common._taskManager._tickInterval - timeRemaining);\x0d\x0a        if (nextInterval < 10) {\x0d\x0a            nextInterval = 0;\x0d\x0a        }\x0d\x0a        if (nextInterval > Common._taskManager._tickInterval) {\x0d\x0a nextInterval = Common._taskManager._tickInterval;\x0d\x0a        }\x0d\x0a        if (!this._isDisposed) {\x0d\x0a       window.setTimeout(this._processDelegate, nextInterval);\x0d\x0a        }\x0d\x0a    }"
);

exec(/aababa(?:bbaaa)/, "aabababbaaa");
match(/[cgt]gggtaaa|tttaccc[acg]/ig, "GGCCGGGTAAAGTGGCTCACGCCTGTAATCCCAGCACTTTACCCCCCGAGGCGGGCGGA");

exec(/((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((x))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))/, "x");

exec(/a^|b/, "ab");
exec(/(a|ab){0,2}?x/, "abax");
exec(/(^|\s)(([^a]([^ ]+)?)|(a([^#.][^ ]+)+)):(hover|active)/, "#standardCore UL#bloc_adresse_recap LI ADDRESS, #standardCore UL#bloc_adresse_recap LI UL, #cgvPopin #printBonus#standardCore UL#bloc_adresse_recap LI ADDRESS, #standardCore UL#bloc_adresse_recap LI UL, #cgvPopin #printBonus#standardCore UL#bloc_adresse_recap LI ADDRESS, #standardCore UL#bloc_adresse_recap LI UL, #cgvPopin #printBonus#standardCore UL#bloc_adresse_recap LI ADDRESS, #standardCore UL#bloc_adresse_recap LI UL, #cgvPopin #printBonus#standardCore UL#bloc_adresse_recap LI ADDRESS, #standardCore UL#bloc_adresse_recap LI UL, #cgvPopin #printBonus");
// Can't baseline this one using v8 :-)
// exec(/(^|\s)(([^a]([^ ]+)?)|(a([^#.][^ ]+)+)):(hover|active)/, "html, body, div, span, object, iframe, h1, h2, h3, h4, h5, h6, p, blockquote, pre, a, abbr, acronym, address, code, del, dfn, em, img, q, dl, dt, dd, ol, ul, li, fieldset, form, label, legend, table, caption, tbody, tfoot, thead, tr, th, td");

exec(/(^|\s)(([^a]([^ ]+)?)|(a([^#.][^ ]+)+)):(hover|active)/, "html, body");
