//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var InvalidationScheme = {
    ReadOnlyData: 0,
    ReadOnlyAccessor: 1,
    ReadWriteAccessors: 2,
    CollectGarbage: 3
};

function GenerateTest(
    useIndexProperties,
    numSetsBeforeCacheInvalidation,
    invalidationScheme,
    numSetsAfterCacheInvalidation) {
    function PropertyName(propertyIndex) {
        var propertyName = "";
        if(!useIndexProperties)
            propertyName += "p";
        return propertyName + propertyIndex;
    }

    f = "var proto = {};";
    for(var i = 0; i < numSetsBeforeCacheInvalidation + numSetsAfterCacheInvalidation; ++i) {
        var propertyName = PropertyName(i);
        f += "proto[" + propertyName + "] = " + i + ";";
    }

    f += "var Construct = function(){};";
    f += "Construct.prototype = proto;";
    f += "var o = new Construct;";

    for(var i = 0; i < numSetsBeforeCacheInvalidation; ++i) {
        var propertyName = PropertyName(i);
        f += "o[" + propertyName + "] = " + i + ";";
    }

    var propertyName = PropertyName(numSetsBeforeCacheInvalidation);
    if(invalidationScheme == InvalidationScheme.ReadOnlyData)
        f += "Object.defineProperty(proto, \"" + propertyName + "\", {writable: false});";
    else if(invalidationScheme == InvalidationScheme.ReadOnlyAccessor)
        f += "Object.defineProperty(proto, \"" + propertyName + "\", {get: function(){return 0;}});";
    else if(invalidationScheme == InvalidationScheme.ReadWriteAccessors) {
        f += "Object.defineProperty(proto, \"" + propertyName + "\", {";
        f += "get: function(){return 0;},";
        f += "set: function(v){}";
        f += "});";
    } else if(invalidationScheme == InvalidationScheme.CollectGarbage)
        f += "CollectGarbage();";

    for(var i = numSetsBeforeCacheInvalidation; i < numSetsAfterCacheInvalidation; ++i) {
        var propertyName = PropertyName(i);
        f += "o[" + propertyName + "] = " + i + ";";
    }

    f += "echo(\"proto:  \", proto);";
    f += "echo(\"o:      \", o);";
    echo(f);
    return f;
}

var bools = [false, true];
for(var useIndexPropertiesPropertyName in bools) {
    var useIndexProperties = bools[useIndexPropertiesPropertyName];
    for(var numSetsBeforeCacheInvalidation = 0; numSetsBeforeCacheInvalidation <= 3; ++numSetsBeforeCacheInvalidation) {
        for(var invalidationSchemePropertyName in InvalidationScheme) {
            var invalidationScheme = InvalidationScheme[invalidationSchemePropertyName];
            for(var numSetsAfterCacheInvalidation = 0;
                numSetsAfterCacheInvalidation <= 3;
                ++numSetsAfterCacheInvalidation) {
                safeCall(
                    eval,
                    GenerateTest(
                        useIndexProperties,
                        numSetsBeforeCacheInvalidation,
                        invalidationScheme,
                        numSetsAfterCacheInvalidation));
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
