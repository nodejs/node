//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if(this.useStrict === undefined)
    this.useStrict = false;

var keywords = [
    "break",
    "case",
    "catch",
    "continue",
    "debugger",
    "default",
    "delete",
    "do",
    "else",
    "finally",
    "for",
    "function",
    "if",
    "in",
    "instanceof",
    "new",
    "return",
    "switch",
    "this",
    "throw",
    "try",
    "typeof",
    "var",
    "void",
    "while",
    "with",

    "null",
    "false",
    "true"
];

var futureReservedWordsInStrictAndNonStrictModes = [
    "class",
    "const",
    "enum",
    "export",
    "extends",
    "import",
    "super"
];

var futureReservedWordsOnlyInStrictMode = [
    "implements",
    "interface",
    "let",
    "package",
    "private",
    "protected",
    "public",
    "static",
    "yield"
];

var otherWords = [
    "foo",
    "<"
];

var wordSets = [
    keywords,
    futureReservedWordsInStrictAndNonStrictModes,
    futureReservedWordsOnlyInStrictMode,
    otherWords
];

var wordUsage = {
    varDeclaration: 0,
    functionDeclarationName: 1,
    functionExpressionName: 2,
    functionArgument: 3,
    catchArgument: 4,
    propertyNameInLiteral: 5,
    propertyNameInDotNotation: 6,
    propertyNameInElementNotation: 7
};

function generateAndRunTest(word, usage) {
    var f = "(function(){";
    if(usage === wordUsage.varDeclaration)
        f += "var " + word + ";";
    else if(usage === wordUsage.functionDeclarationName)
        f += "function " + word + "(){}";
    else if(usage === wordUsage.functionExpressionName)
        f += "(function " + word + "(){})";
    else if(usage === wordUsage.functionArgument)
        f += "(function(" + word + "){});";
    else if(usage === wordUsage.catchArgument)
        f += "try{} catch(" + word + "){}";
    else if(usage === wordUsage.propertyNameInLiteral)
        f += "({" + word + ": 0});";
    else if(usage === wordUsage.propertyNameInDotNotation)
        f += "var o = {}; o." + word + " = 0;";
    else if(usage === wordUsage.propertyNameInElementNotation)
        f += "var o = {}; o[\"" + word + "\"] = 0;";
    else
        throw new Error();
    f += "})();";

    echo(f);
    if(useStrict)
        f = "\"use strict\";" + f;
    safeCall(eval, f);
}

for(var wordUsagePropertyName in wordUsage) {
    var usage = wordUsage[wordUsagePropertyName];
    for(var wordSetsPropertyName in wordSets) {
        var wordSet = wordSets[wordSetsPropertyName];
        for(var wordSetPropertyName in wordSet) {
            var word = wordSet[wordSetPropertyName];
            generateAndRunTest(word, usage);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function measureTime(f) {
    var d = new Date();
    f();
    echo(new Date() - d);
}

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
                var s = "" + o;
                var i = s.indexOf("e");
                var end = i === -1 ? s.length : e;
                i = s.indexOf(".");
                var start = i === -1 ? 0 : i + 1;
                if(start !== 0) {
                    if(end % 3 !== 0)
                        end += 3 - end % 3;
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
    if(o instanceof Object) {
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
