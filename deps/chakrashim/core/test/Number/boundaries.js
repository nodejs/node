//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

for (var numDigitsAfterDecimal = -1; numDigitsAfterDecimal <= 2; ++numDigitsAfterDecimal) {
    for (var lastDigit = 0; lastDigit <= (numDigitsAfterDecimal <= 0 ? 0 : 1); ++lastDigit) {
        for (var numDigits = 20; numDigits <= 23; ++numDigits) {
            var s = generateNumberString(numDigits - Math.max(0, numDigitsAfterDecimal), numDigitsAfterDecimal, lastDigit);
            echo(numDigits + " digits");
            echo(s);
            echo(eval(s).toString());
            echo("");
        }
    }
}

function generateNumberString(numDigitsBeforeDecimal, numDigitsAfterDecimal, lastDigit) {
    if (numDigitsBeforeDecimal < 1)
        throw new Error("Invalid numDigits");

    var useDecimal = numDigitsAfterDecimal >= 0;
    if (numDigitsAfterDecimal < 0)
        numDigitsAfterDecimal = 0;

    var s = "1";
    for (var i = 1; i < numDigitsBeforeDecimal; ++i)
        s += "0";
    if (useDecimal)
        s += ".";
    if (numDigitsAfterDecimal !== 0) {
        for (var i = 0; i < numDigitsAfterDecimal - 1; ++i)
            s += "0";
        s += lastDigit;
    }
    return s;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function toString(o, quoteStrings) {
    switch (o) {
        case null:
        case undefined:
            return "" + o;
    }

    switch (typeof o) {
        case "boolean":
        case "number":
            {
                var s = "" + o;
                var e = s.indexOf("e");
                for (var i = (e == -1 ? s.length : e) - 3; i > 0; i -= 3)
                    s = s.substring(0, i) + "," + s.substring(i);
                return s;
            }

        case "string":
            {
                var hex = "0123456789abcdef";
                var s = "";
                for (var i = 0; i < o.length; ++i) {
                    var c = o.charCodeAt(i);
                    if (c === 0)
                        s += "\\0";
                    else if (c >= 0x20 && c < 0x7f)
                        s += quoteStrings && o.charAt(i) === "\"" ? "\\\"" : o.charAt(i);
                    else if (c <= 0xff)
                        s += "\\x" + hex.charAt((c >> 4) & 0xf) + hex.charAt(c & 0xf);
                    else
                        s += "\\u" + hex.charAt((c >> 12) & 0xf) + hex.charAt((c >> 8) & 0xf) + hex.charAt((c >> 4) & 0xf) + hex.charAt(c & 0xf);
                }
                if (quoteStrings)
                    s = "\"" + s + "\"";
                return s;
            }

        case "object":
        case "function":
            break;

        default:
            return "<unknown type '" + typeof o + "'>";
    }

    if (o instanceof Array) {
        var s = "[";
        for (var i = 0; i < o.length; ++i) {
            if (i)
                s += ", ";
            s += toString(o[i], true);
        }
        return s + "]";
    }
    if (o instanceof Error)
        return o.name + ": " + o.message;
    if (o instanceof RegExp)
        return o.toString() + (o.lastIndex === 0 ? "" : " (lastIndex: " + o.lastIndex + ")");
    return "" + o;
}

function echo(o) {
    var s = toString(o);
    try {
        document.write(s + "<br/>");
    } catch (ex) {
        try {
            WScript.Echo(s);
        } catch (ex2) {
            print(s);
        }
    }
}

function safeCall(f) {
    var args = [];
    for (var a = 1; a < arguments.length; ++a)
        args.push(arguments[a]);
    try {
        return f.apply(this, args);
    } catch (ex) {
        echo(ex);
    }
}
