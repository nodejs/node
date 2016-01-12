//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var total = 0, accepted = 0, failed = 0;

echo("////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////");
echo("// Definitely valid ISO strings");
echo("");

echo("// Auto-generated");
echo("");
initializeGenerateDateStrings();
var yearDigits = 0, monthDigits = 0, dayDigits = 0, hourMinuteDigits = 0, secondDigits = 0, millisecondDigits = 0;
for (yearDigits = 4; yearDigits <= 6; yearDigits += 2) {
    dayDigits = monthDigits = 0;
    runGenerateTestWithValidTime();
    monthDigits = 2;
    runGenerateTestWithValidTime();
    dayDigits = 2;
    runGenerateTestWithValidTime();
}
function runGenerateTestWithValidTime() {
    millisecondDigits = secondDigits = hourMinuteDigits = 0;
    runGenerateTest();
    hourMinuteDigits = 2;
    runGenerateTest();
    secondDigits = 2;
    runGenerateTest();
    millisecondDigits = 3;
    runGenerateTest();
}

writeStats();

echo("////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////");
echo("// Definitely invalid ISO strings");
echo("");

echo("// Field value outside valid range");
echo("");
runTest("0001-00-01T01:01:01.001Z");
runTest("0001-13-01T01:01:01.001Z");
runTest("0001-01-00T01:01:01.001Z");
runTest("0001-01-32T01:01:01.001Z");
runTest("0001-01-01T25:01:01.001Z");
runTest("0001-01-01T01:01:01.001+25:00");
runTest("0001-01-01T01:60:01.001Z");
runTest("0001-01-01T01:01:01.001+00:60");
runTest("0001-01-01T01:01:60.001Z");

echo("// Time value outside valid range");
echo("");
runTest("-300000-01-01T01:01:01.001Z");
runTest("+300000-01-01T01:01:01.001Z");

// Many other invalid ISO strings are tested in "potential cross-browser compatibility issues" section

writeStats();

echo("////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////");
echo("// Potential cross-browser compatilibity issues");
echo("");

echo("// Leading and trailing whitespace, nulls, or non-whitespace non-nulls");
echo("");
var s = "0001-01-01T01:01:01.001Z";
var spaceNulls = ["", "\0", "\t", "\n", "\v", "\f", "\r", " ", "\u00a0", "\u2028", "\u2029", "\ufeff"];
for (var i = 0; i < spaceNulls.length; ++i) {
    if (s !== "") {
        runTest(spaceNulls[i] + s);
        runTest(s + spaceNulls[i]);
    }
    runTest(s + spaceNulls[i] + "x");
}

echo("// Less and more digits per field");
echo("");
runTest("001-01-01T01:01:01.001Z");
runTest("00001-01-01T01:01:01.001Z");
runTest("0001-1-01T01:01:01.001Z");
runTest("0001-001-01T01:01:01.001Z");
runTest("0001-01-1T01:01:01.001Z");
runTest("0001-01-001T01:01:01.001Z");
runTest("0001-01-01T1:01:01.001Z");
runTest("0001-01-01T001:01:01.001Z");
runTest("0001-01-01T01:1:01.001Z");
runTest("0001-01-01T01:001:01.001Z");
runTest("0001-01-01T01:01:1.001Z");
runTest("0001-01-01T01:01:001.001Z");
runTest("0001-01-01T01:01:01.01Z");
runTest("0001-01-01T01:01:01.0001Z");

echo("// Date-only forms with UTC offset");
echo("");
runTest("0001Z");
runTest("0001-01Z");
runTest("0001-01-01Z"); // note: this is rejected by the ISO parser as it should be, but it's accepted by the fallback parser

echo("// Optionality of minutes");
echo("");
runTest("0001-01-01T01Z");
runTest("0001-01-01T01:01:01.001+01");

echo("// Time-only forms");
echo("");
runTest("T01:01Z");
runTest("T01:01:01Z");
runTest("T01:01:01.001Z");

echo("// Field before missing optional field ending with separator");
echo("");
runTest("0001-");
runTest("0001-01-");
runTest("0001-T01:01:01.001Z");
runTest("0001-01-T01:01:01.001Z");
runTest("0001-01-01T01:01:Z");
runTest("0001-01-01T01:01:01.Z");

echo("// Optionality and type of sign on years");
echo("");
runTest("+0001-01-01T01:01:01.001Z");
runTest("-0001-01-01T01:01:01.001Z");
runTest("010000-01-01T01:01:01.001Z");
runTest("-000000-01-01T01:01:01.001Z");

echo("// Test support for zones without colons (DEVDIV2: 481975)");
echo("");
runTest("2012-02-22T03:08:26+0000");

writeStats();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test-specific helpers

function runTest(s) {
    ++total;
    echo(s);
    safeCall(function () {
        var iso = new Date(s);
        var timeValue1 = iso.getTime();
        if (isNaN(timeValue1)) {
            echo(iso);
        } else {
            iso = iso.toISOString();
            echo(iso);
            var timeValue2 = new Date(iso).getTime();
            echo(timeValue1 + " " + (timeValue1 === timeValue2 ? "===" : "!==") + " " + timeValue2);
            if (iso.indexOf("Invalid", 0) === -1) {
                if (timeValue1 === timeValue2)
                    ++accepted;
                else
                    ++failed;
            }
        }
    });
    echo("");
}

function runGenerateTest() {
    var s =
        generateDateStrings(
            yearDigits,
            monthDigits,
            dayDigits,
            hourMinuteDigits,
            hourMinuteDigits,
            secondDigits,
            millisecondDigits);
    for (var i = 0; i < s.length; ++i)
        runTest(s[i]);
}

var signs, zones;
function initializeGenerateDateStrings() {
    signs = ["+", "-"];
    zones = ["", "Z"];
    var zoneDigitCombinations = ["00", "01", "10"];
    for (var i = 0; i < zoneDigitCombinations.length; ++i)
        for (var j = 0; j < zoneDigitCombinations.length; ++j)
            for (var k = 0; k < signs.length; ++k) 
                zones.push(signs[k] + zoneDigitCombinations[i] + ":" + zoneDigitCombinations[j]);
}

// Generates date strings in the following format:
//     date format: "[+|-]YYYYYY[-MM[-DD]]"
//     separator:   "T| "
//     time format: "HH:mm[:ss[.sss]]"
//     time zone:   "Z|(+|-)HH:mm"
// - The separator is required only if both the date and time portions are included in the string.
// - Zero-padding is optional
// - Positive sign (+) is optional when the year is nonnegative
// - Negative sign (-) is optional when the year is zero
// - Time zone is optional
// 
// The function will return an array of strings to test against, based on the parameters.
function generateDateStrings(
    yearDigits,         // number of digits to include for the year (0-6), 0 - exclude the year (monthDigits must also be 0)
    monthDigits,        // number of digits to include for the month (0-2), 0 - exclude the month (dayDigits must also be 0)
    dayDigits,          // number of digits to include for the day (0-2), 0 - exclude the day
    hourDigits,         // number of digits to include for the hour (0-2), 0 - exclude the hour (minuteDigits must also be 0)
    minuteDigits,       // number of digits to include for the minute (0-2), 0 - exclude the minute (hourDigits and secondDigits must also be 0)
    secondDigits,       // number of digits to include for the second (0-2), 0 - exclude the second (millisecondDigits must also be 0)
    millisecondDigits)  // number of digits to include for the millisecond (0-3), 0 - exclude the millisecond
{
    if (yearDigits === 0 && monthDigits !== 0 ||
        monthDigits === 0 && dayDigits !== 0 ||
        hourDigits === 0 && minuteDigits !== 0 ||
        minuteDigits === 0 && (hourDigits !== 0 || secondDigits !== 0) ||
        secondDigits === 0 && millisecondDigits !== 0 ||
        yearDigits === 0 && (hourDigits === 0 || minuteDigits === 0))
        return [];

    var s = [""];

    if (yearDigits !== 0) {
        appendDigits(s, yearDigits, true);
        if (monthDigits !== 0) {
            append(s, ["-"]);
            appendDigits(s, monthDigits, false);
            if (dayDigits !== 0) {
                append(s, ["-"]);
                appendDigits(s, dayDigits, false);
            }
        }
    }

    if (hourDigits !== 0 && minuteDigits !== 0) {
        append(s, ["T"]);
        appendDigits(s, hourDigits, true);
        append(s, [":"]);
        appendDigits(s, minuteDigits, true);
        if (secondDigits !== 0) {
            append(s, [":"]);
            appendDigits(s, secondDigits, true);
            if (millisecondDigits !== 0) {
                append(s, ["."]);
                appendDigits(s, millisecondDigits, true);
            }
        }
    }

    if (yearDigits !== 0 && hourDigits !== 0 && minuteDigits !== 0)
        s = applyToEach(s, zones, function (str, zone) { return str + zone; });
    if(yearDigits === 6) {
        s =
            applyToEach(
                s,
                signs,
                function (str, sign) {
                    if(sign === "-" && str.length >= 6 && str.substring(0, 6) === "000000")
                        return undefined; // "-000000" is not allowed
                    return sign + str;
                });
    }

    return s;
}

// Appends interesting combinations of n digits to the string array
function appendDigits(a, n, includeZero) {
    var d = [];
    switch (n) {
        case 0:
            break;

        case 1:
            if (includeZero)
                d.push("0");
            d.push("1");
            append(a, d);
            break;

        case 3:
        case 6:
            if (n === 3)
                d.push("010");
            else
                d.push("010010");

        default:
            var z = zeroes(n - 1);
            if (includeZero)
                d.push(z + "0");
            d.push(z + "1");
            d.push("1" + z);
            append(a, d);
            break;
    }
}

// Returns a string of n zeroes
function zeroes(n) {
    var s = "";
    while (n-- > 0)
        s += "0";
    return s;
}

// Appends patterns to the string array. The array is extended to acommodate the number of patterns, and the patterns are
// repeated to acommodate the length of the array.
function append(a, p) {
    extend(a, p.length);
    for (var i = 0; i < a.length; ++i)
        a[i] += p[i % p.length];
}

// Applies the function 'f' to each combination of elements in 'a' and 'p'. 'f' will receive the element of 'a' on which it
// should apply the pattern from 'p' and it should return the modified string. The string returned by 'f' will be pushed onto a
// new array, which will be returned.
function applyToEach(a, p, f) {
    var a2 = [];
    for(var i = 0; i < a.length; ++i) {
        for(var j = 0; j < p.length; ++j) {
            var transformed = f(a[i], p[j]);
            if(transformed !== undefined)
                a2.push(transformed);
        }
    }
    return a2;
}

// Extends an array to have length n, by copying the last element as necessary
function extend(a, n) {
    var originalLength = a.length;
    for (var i = originalLength; i < n; ++i)
        a.push(a[originalLength - 1]);
}

function writeStats() {
    echo("Total: " + total);
    echo("Accepted: " + accepted);
    echo("Rejected: " + (total - accepted - failed));
    echo("Failed: " + failed);
    echo("");
    failed = accepted = total = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// General helpers

function toString(o, quoteStrings) {
    switch (o) {
        case null:
        case undefined:
            return "" + o;
    }

    switch (typeof o) {
        case "boolean":
        case "number":
            return "" + o;

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

            s += this.toString(o[i], true);
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
    var s = this.toString(o);
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
