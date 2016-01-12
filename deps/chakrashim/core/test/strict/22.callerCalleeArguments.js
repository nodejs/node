//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var useStrict = false;

var PropertyAccessType = {
    HasOwnProperty: 0,
    DirectGet: 1,
    DirectSet: 2,
    DefineProperty: 3,
    GetOwnPropertyDescriptor: 4
};

var SpecialProperty =
{
    Caller: "caller",
    Callee: "callee",
    Arguments: "arguments"
};

function GenerateSpecialPropertyAccess(o, propertyAccessType, specialProperty) {
    if(propertyAccessType === PropertyAccessType.HasOwnProperty)
        return "echo(\"hasOwnProperty(" + specialProperty + "): \", " + o + ".hasOwnProperty(\"" + specialProperty + "\"));";
    if(propertyAccessType === PropertyAccessType.DirectGet)
        return o + "." + specialProperty + ";";
    if(propertyAccessType === PropertyAccessType.DirectSet)
        return o + "." + specialProperty + " = 0;";
    if(propertyAccessType === PropertyAccessType.DefineProperty)
        return "Object.defineProperty(" + o + ", \"" + specialProperty + "\", {value: 0});";
    if(propertyAccessType === PropertyAccessType.GetOwnPropertyDescriptor)
        return "var descriptor = Object.getOwnPropertyDescriptor(" + o + ", \"" + specialProperty + "\");" +
            "if(descriptor.hasOwnProperty(\"get\")) safeCall(descriptor.get);" +
            "if(descriptor.hasOwnProperty(\"set\")) safeCall(descriptor.set);";
    throw new Error();
}

function GenerateAndRunArgumentsSpecialPropertyTest(propertyAccessType, specialProperty) {
    if(specialProperty == SpecialProperty.Arguments)
        return;

    var f = "(function(){";
    if(useStrict)
        f += "\"use strict\";";
    f += GenerateSpecialPropertyAccess("arguments", propertyAccessType, specialProperty);
    f += "})();";

    echo(f);
    safeCall(eval, f);
    echo();
}

function GenerateAndRunFunctionSpecialPropertyTest(propertyAccessType, specialProperty, accessFromStrictCode) {
    if(specialProperty == SpecialProperty.Callee)
        return;

    var f = "var foo = function(){";
    if(useStrict)
        f += "\"use strict\";";
    f += "};";
    f += "(function(){";
    if(accessFromStrictCode)
        f += "\"use strict\";";
    f += GenerateSpecialPropertyAccess("foo", propertyAccessType, specialProperty);
    f += "})();";

    echo(f);
    safeCall(eval, f);
    echo();
}

var bools = [false, true];
for(var propertyAccessTypePropertyName in PropertyAccessType) {
    var propertyAccessType = PropertyAccessType[propertyAccessTypePropertyName];
    for(var specialPropertyPropertyName in SpecialProperty) {
        var specialProperty = SpecialProperty[specialPropertyPropertyName];
        GenerateAndRunArgumentsSpecialPropertyTest(propertyAccessType, specialProperty);
        for(var accessFromStrictCodePropertyName in bools) {
            var accessFromStrictCode = bools[accessFromStrictCodePropertyName];
            GenerateAndRunFunctionSpecialPropertyTest(propertyAccessType, specialProperty, accessFromStrictCode);
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
