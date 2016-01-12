//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// WARNING: See Bug 1335253
// This file will not generate Trie branches when symbols share prefixes under certain circumstances (thus skipping cases).

objArgs = WScript.Arguments;

function emitToken(token, d, indent) {
    r = "";
    indent += "    ";
    if (d)
        r += indent + "p += " + d + ";\r\n";
    if (token.res == 1) {
        if (token.tk === "tkYIELD") {
            r += indent + "if (this->m_fYieldIsKeyword || !this->m_parser || this->m_parser->IsStrictMode()) {" + "\r\n";
            r += indent + "    token = " + token.tk + ";\r\n";
            r += indent + "    goto LReserved;\r\n";
            r += indent + "}\r\n";
            r += indent + "goto LIdentifier;\r\n";
        } else if (token.tk === "tkAWAIT") {
            r += indent + "if (this->m_fAwaitIsKeyword || !this->m_parser || this->m_parser->IsStrictMode()) {" + "\r\n";
            r += indent + "    token = " + token.tk + ";\r\n";
            r += indent + "    goto LReserved;\r\n";
            r += indent + "}\r\n";
            r += indent + "goto LIdentifier;\r\n";
        } else {
            r += indent + "token = " + token.tk + ";\r\n";
            r += indent + "goto LReserved;\r\n";
        }
    } else if (token.res == 2) {
        r += indent + "if (!this->m_parser || this->m_parser->IsStrictMode()) {" + "\r\n";
        r += indent + "    " + "token = " + token.tk + ";\r\n";
        r += indent + "    " + "goto LReserved;\r\n";
        r += indent + "}\r\n";
        r += indent + "goto LIdentifier;\r\n";
    } else if (token.res == 3) {
        // These are special case of identifiers that we have a well known PID for and always want to be filled
        // whether or not we have suppressed generated pids. (e.g. eval and arguments)
        r += indent + "goto " + token.tk + ";\r\n";
    } else {
        WScript.Echo("Error: Unsupported Keyword type");
    }

    return r;
}

function noMoreBranches(token) {
    for (var c = token; c.length; c = c[0]) {
        if (c.length > 1) return false;
    }
    return true;
}

function emit(token, d, indent) {
    var r = "";
    if (token.length > 1) {
        r += indent + "switch (";
        if (d < 0) r += "ch";
        else r += "p[" + d + "]";
        r += ") {\r\n";
        for (var i = 0; i < token.length; i++) {
            var tk = token[i];
            r += indent + "case '" + tk.char + "':\r\n";
            r += emit(tk, d + 1, indent + "    ");
            if (tk.tk && tk.length) {
                r += indent + "    if (!IsIdContinueNext(p+" +(d + 1) + ",last)) {\r\n" + emitToken(tk, d + 1, indent + "    ") + indent + "    }\r\n";
            }
            r += indent + "    break;\r\n";
        }
        r += indent + "}\r\n";
    }
    else if (noMoreBranches(token)) {
        r += indent + "if (";
        for (var c = token; c.length; c = c[0]) {
            r += "p[" + d++ + "] == '" + c[0].char + "' && ";
        }
        r += "!IsIdContinueNext(p+" + d + ", last)) {\r\n";
        r += emitToken(c, d, indent);
        r += indent + "}\r\n";
    }
    else {
        r += indent + "if (p[" + d + "] == '" + token[0].char + "') {\r\n";
        r += emit(token[0], d + 1, indent + "    ");
        r += indent + "}\r\n";
    }
    return r;
}

if (objArgs.length != 1 && objArgs.length != 2) {
    WScript.Echo("Supply the header file name and optional output file");
}
else {
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    var file = fso.OpenTextFile(objArgs(0), 1);
    var text = file.ReadAll();
    file.Close();
    var reg = /KEYWORD\((tk[A-Z]+)\s*,([1-2]),.*,\s*([a-z]+)\)/g;
    var s_reg = /S_KEYWORD\((L[A-Z][a-z]+)\s*,(3),\s*([a-z]+)\)/g;
    var t = [];
    var s = text.replace(reg, function (a, p1, p2, p3, offset) {
        t.push({ tk: p1, res: p2, word: p3 });
    });

    var s_s = text.replace(s_reg, function (a, p1, p2, p3, offset) {
        t.push({ tk: p1, res: p2, word: p3 });
    });

    var tokens = [];
    var counter = 0;

    for (var i = 0; i < t.length; i++) {
        var token = t[i];
        var current = tokens;
        for (var j = 0; j < token.word.length; j++) {
            l = token.word.substring(j, j + 1);
            var n = current[l];
            if (n)
                current = n;
            else {
                var nt = [];
                nt.char = l;
                current[l] = nt;
                current.push(nt);
                current = nt;
            }
            counter++;
        }
        current.tk = token.tk;
        current.res = token.res;
    }

    var indent = "    ";
    var r = "";
    r += "//-------------------------------------------------------------------------------------------------------\r\n";
    r += "// Copyright (C) Microsoft. All rights reserved.\r\n";
    r += "// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.\r\n";
    r += "//-------------------------------------------------------------------------------------------------------\r\n";
    r += "// GENERATED FILE, DO NOT HAND-MODIFY!\r\n";
    r += "// Generated with the following command line: wscript jsscan.js " + objArgs(0) + " " + objArgs(1) + "\r\n";
    r += "// This should be regenerated whenever the keywords change.\r\n";
    r += "\r\n";

    // Generate the reserved word recognizer
    for (var i = 0; i < tokens.length; i++) {
        var tk = tokens[i];
        r += indent + "case '" + tk.char + "':\r\n";
        r += indent + "    if (identifyKwds)\r\n";
        r += indent + "    {\r\n";
        var simple = tk.length == 1 && noMoreBranches(tk);
        r += emit(tk, 0, indent + "        ");
        r += indent + "    }\r\n";
        r += indent + "    goto LIdentifier;\r\n";
    }
    r += "\r\n";

    // Generate lower case letters that are not part of the recognizer
    r += indent + "// characters not in a reserved word\r\n";
    var c = 0;
    var chars = "abcdefghijklmnopqrstuvwxyz";
    for (var i = 0; i < chars.length; i++) {
        if (c == 0) r += indent;
        var ch = chars.substring(i, i + 1);
        if (!tokens[ch])
            r += "case '" + ch + "': ";
        else
            r += "          ";
        if (++c == 5) {
            c = 0;
            r += "\r\n";
        }
    }
    r += "\r\n";
    r += indent + "    goto LIdentifier;\r\n";

    if (objArgs.length == 2) {
        var outfile = fso.CreateTextFile(objArgs(1), true);
        outfile.Write(r);
        outfile.Close();
    } else {
        WScript.Echo(r);
    }
}
