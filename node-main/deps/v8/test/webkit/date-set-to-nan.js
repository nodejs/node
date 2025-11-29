// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description(
"This tests if the Date setters handle invalid parameters correctly resulting in a NaN date and if a recovery from such a NaN date is only possible by using the date.setTime() and date.set[[UTC]Full]Year() functions."
);

var dateFunctionNameRoots = [
    "Time",
    "Milliseconds",
    "UTCMilliseconds",
    "Seconds",
    "UTCSeconds",
    "Minutes",
    "UTCMinutes",
    "Hours",
    "UTCHours",
    "Date",
    "UTCDate",
    "Month",
    "UTCMonth",
    "FullYear",
    "UTCFullYear",
    "Year"
];

var dateFunctionParameterNum = [
    1,
    1,
    1,
    2,
    2,
    3,
    3,
    4,
    4,
    1,
    1,
    2,
    2,
    3,
    3,
    1
];

var testValues = [
    0,
    Number.NaN,
    Number.POSITIVE_INFINITY,
    Number.NEGATIVE_INFINITY
];

function testDateFunctionWithValueNoRecoverNaN(functionNameRoot, steps)
{
    var date = new Date();
    var setValue = date["get" + functionNameRoot]();
    date.setMilliseconds(Number.NaN);
    var params = [
        "",
        ", 0",
        ", 0, 0",
        ", 0, 0, 0"
    ];
    var setResult = (1 == steps) ?  date["set" + functionNameRoot](setValue)
                  : ((2 == steps) ? date["set" + functionNameRoot](setValue, 0)
                  : ((3 == steps) ? date["set" + functionNameRoot](setValue, 0, 0)
                  :                  date["set" + functionNameRoot](setValue, 0, 0, 0)));
    if (!isNaN(setResult)) {
        testFailed("date(NaN).set" + functionNameRoot + "(" + setValue + params[steps - 1]
                                   + ") was " + setResult + " instead of NaN");
        return false;
    }
    var getResult = date["get" + functionNameRoot]();
    if (!isNaN(getResult)) {
        testFailed("date.get" + functionNameRoot + "() was " + getResult + " instead of NaN");
        return false;
    }
    testPassed ("no recovering from NaN date using date.set" + functionNameRoot
        + "(arg0" + params[steps - 1] + ")");
    return true;
}

function testDateFunctionWithValueRecoverTime(functionNameRoot)
{
    var date = new Date();
    var setValue = date["get" + functionNameRoot]();
    date.setMilliseconds(Number.NaN);
    var setResult = date["set" + functionNameRoot](setValue);
    if (setValue != setResult) {
        testFailed("date(NaN).set" + functionNameRoot + "(" + setValue + ") was " + setResult + " instead of " + setValue);
        return false;
    }
    var getResult = date["get" + functionNameRoot]();
    if (getResult != setValue) {
        testFailed("date.get" + functionNameRoot + "() was " + getResult + " instead of " + setValue);
        return false;
    }
    testPassed ("recover from NaN date using date.set" + functionNameRoot + "()");
    return true;
}

function testDateFunctionWithValueRecoverFullYear(functionNameRoot)
{
    var result = true;
    var date = new Date();
    var setValue = date["get" + functionNameRoot]();
    date.setMilliseconds(Number.NaN);
    var setResult = date["set" + functionNameRoot](setValue);
    var getResult = date["get" + functionNameRoot]();
    if (getResult != setValue) {
        testFailed("date.get" + functionNameRoot + "() was " + getResult + " instead of " + setValue);
        result = false;
    }
    getResult = date.getMilliseconds();
    if (getResult != 0) {
        testFailed("date.getMilliseconds() was " + getResult + " instead of 0");
        result = false;
    }
    getResult = date.getSeconds();
    if (getResult != 0) {
        testFailed("date.getSeconds() was " + getResult + " instead of 0");
        result = false;
    }
    getResult = date.getMinutes();
    if (getResult != 0) {
        testFailed("date.getMinutes() was " + getResult + " instead of 0");
        result = false;
    }
    getResult = date.getHours();
    if (getResult != 0) {
        testFailed("date.getHours() was " + getResult + " instead of 0");
        result = false;
    }
    getResult = date.getDate();
    if (getResult != 1) {
        testFailed("date.getDate() was " + getResult + " instead of 1");
        result = false;
    }
    getResult = date.getMonth();
    if (getResult != 0) {
        testFailed("date.getMonth() was " + getResult + " instead of 0");
        result = false;
    }
    if (result)
        testPassed ("recover from NaN date using date.setFullYear()");
    else
        testFailed ("recover from NaN date using date.setFullYear()");
    return result;
}

function testDateFunctionWithValueRecoverUTCFullYear(functionNameRoot)
{
    var result = true;
    var date = new Date();
    var setValue = date["get" + functionNameRoot]();
    date.setMilliseconds(Number.NaN);
    var setResult = date["set" + functionNameRoot](setValue);
    var getResult = date["get" + functionNameRoot]();
    if (getResult != setValue) {
        testFailed("date.get" + functionNameRoot + "() was " + getResult + " instead of " + setValue);
        result = false;
    }
    getResult = date.getUTCMilliseconds();
    if (getResult != 0) {
        testFailed("date.getUTCMilliseconds() was " + getResult + " instead of 0");
        result = false;
    }
    getResult = date.getUTCSeconds();
    if (getResult != 0) {
        testFailed("date.getUTCSeconds() was " + getResult + " instead of 0");
        result = false;
    }
    getResult = date.getUTCMinutes();
    if (getResult != 0) {
        testFailed("date.getUTCMinutes() was " + getResult + " instead of 0");
        result = false;
    }
    getResult = date.getUTCHours();
    if (getResult != 0) {
        testFailed("date.getUTCHours() was " + getResult + " instead of 0");
        result = false;
    }
    getResult = date.getUTCDate();
    if (getResult != 1) {
        testFailed("date.getUTCDate() was " + getResult + " instead of 1");
        result = false;
    }
    getResult = date.getUTCMonth();
    if (getResult != 0) {
        testFailed("date.getUTCMonth() was " + getResult + " instead of 0");
        result = false;
    }
    if (result)
        testPassed ("recover from NaN date using date.setUTCFullYear()");
    else
        testFailed ("recover from NaN date using date.setUTCFullYear()");
    return result;
}

function testDateFunctionWithValueRecoverYear(functionNameRoot)
{
    var result = true;
    var is13Compatible = true;

    var date = new Date();
    var setValue = date["get" + functionNameRoot]();
    var fullYears = date.getFullYear() - 1900;
    if (setValue != fullYears) {
        testFailed("date.get" + functionNameRoot + "() was " + setValue + " instead of " + fullYears);
        is13Compatible = false;
    } else
        testPassed("date.getYear() is compatible to JavaScript 1.3 and later");

    date.setMilliseconds(Number.NaN);
    var setResult = date["set" + functionNameRoot](setValue + 1900);
    var getResult = date["get" + functionNameRoot]();
    if (getResult != setValue) {
        testFailed("date.get" + functionNameRoot + "() was " + getResult + " instead of " + setValue);
        result = false;
    }
    getResult = date.getMilliseconds();
    if (getResult != 0) {
        testFailed("date.getMilliseconds() was " + getResult + " instead of 0");
        result = false;
    }
    getResult = date.getSeconds();
    if (getResult != 0) {
        testFailed("date.getSeconds() was " + getResult + " instead of 0");
        result = false;
    }
    getResult = date.getMinutes();
    if (getResult != 0) {
        testFailed("date.getMinutes() was " + getResult + " instead of 0");
        result = false;
    }
    getResult = date.getHours();
    if (getResult != 0) {
        testFailed("date.getHours() was " + getResult + " instead of 0");
        result = false;
    }
    getResult = date.getDate();
    if (getResult != 1) {
        testFailed("date.getDate() was " + getResult + " instead of 1");
        result = false;
    }
    getResult = date.getMonth();
    if (getResult != 0) {
        testFailed("date.getMonth() was " + getResult + " instead of 0");
        result = false;
    }
    if (result)
        testPassed ("recover from NaN date using date.setUTCFullYear()");
    else
        testFailed ("recover from NaN date using date.setUTCFullYear()");
    return result && is13Compatible;
}

function makeIEHappy(functionNameRoot, value)
{
    var date = new Date();
    var setResult = date["set" + functionNameRoot](value);
    if (!isNaN(setResult)) {
        testFailed("date.set" + functionNameRoot
                              + "() was "
                              + setResult + " instead of NaN");
         return false;
    }
    var getResult = date["get" + functionNameRoot]();
    if (!isNaN(getResult)) {
        testFailed("date.get" + functionNameRoot + "() was "
                              + getResult + " instead of NaN");
        return false;
    }
    return true
}

function testDateFunctionWithValueExpectingNaN1(functionNameRoot)
{
    var result = true;
    for (var idx0 in testValues)
        if (idx0 != 0) {
            var date = new Date();
            var setResult = date["set" + functionNameRoot](testValues[idx0]);
            if (!isNaN(setResult)) {
                testFailed("date.set" + functionNameRoot + "("
                                      + testValues[idx0] + ") was "
                                      + setResult + " instead of NaN");
                result = false;
            }
            var getResult = date["get" + functionNameRoot]();
            if (!isNaN(getResult)) {
                testFailed("date.get" + functionNameRoot + "() was "
                                      + getResult + " instead of NaN");
                result = false;
            }
        } else if (!makeIEHappy(functionNameRoot))
            result = false;
    if (result) {
        testPassed("date.set" + functionNameRoot + "(arg0)");
        testPassed("date.set" + functionNameRoot + "()");
    }
    return result;
}

function testDateFunctionWithValueExpectingNaN2(functionNameRoot)
{
    var result = true;
    for (var idx0 in testValues)
        for (var idx1 in testValues)
            if (idx0 != 0 || idx1 != 0) {
                var date = new Date();
                var setResult = date["set" + functionNameRoot](testValues[idx0],
                                                               testValues[idx1]);

                if (!isNaN(setResult)) {
                    testFailed("date.set" + functionNameRoot + "("
                                          + testValues[idx0] + ", "
                                          + testValues[idx1] + ") was "
                                          + setResult + " instead of NaN");
                    result = false;
                }
                var getResult = date["get" + functionNameRoot]();
                if (!isNaN(getResult)) {
                    testFailed("date.get" + functionNameRoot + "() was "
                                          + getResult + " instead of NaN");
                    result = false;
                }
            }

    if (result)
        testPassed("date.set" + functionNameRoot + "(arg0, arg1)");
    return result;
}

function testDateFunctionWithValueExpectingNaN3(functionNameRoot)
{
    var result = true;
    for (var idx0 in testValues)
        for (var idx1 in testValues)
            for (var idx2 in testValues)
                if (idx0 != 0 || idx1 != 0 || idx2 != 0) {
                    var date = new Date();
                    var setResult = date["set" + functionNameRoot](testValues[idx0],
                                                                   testValues[idx1],
                                                                   testValues[idx2]);
                    if (!isNaN(setResult)) {
                        testFailed("date.set" + functionNameRoot + "("
                                              + testValues[idx0] + ", "
                                              + testValues[idx1] + ", "
                                              + testValues[idx2] + ") was "
                                              + setResult + " instead of NaN");
                        result = false;
                    }
                    var getResult = date["get" + functionNameRoot]();
                    if (!isNaN(getResult)) {
                        testFailed("date.get" + functionNameRoot + "() was "
                                              + getResult + " instead of NaN");
                        result = false;
                    }
                }

    if (result)
        testPassed("date.set" + functionNameRoot + "(arg0, arg1, arg2)");
    return result;
}

function testDateFunctionWithValueExpectingNaN4(functionNameRoot)
{
    var result = true;
    for (var idx0 in testValues)
        for (var idx1 in testValues)
            for (var idx2 in testValues)
                for (var idx3 in testValues)
                    if (idx0 != 0 || idx1 != 0 || idx2 != 0 || idx3 != 0) {
                        var date = new Date();
                        var setResult = date["set" + functionNameRoot](testValues[idx0],
                                                                       testValues[idx1],
                                                                       testValues[idx2],
                                                                       testValues[idx3]);
                        if (!isNaN(setResult)) {
                            testFailed("date.set" + functionNameRoot + "("
                                                  + testValues[idx0] + ", "
                                                  + testValues[idx1] + ", "
                                                  + testValues[idx2] + ", "
                                                  + testValues[idx3] + ") was "
                                                  + setResult + " instead of NaN");
                            result = false;
                        }
                        var getResult = date["get" + functionNameRoot]();
                        if (!isNaN(getResult)) {
                            testFailed("date.get" + functionNameRoot + "() was "
                                                  + getResult + " instead of NaN");
                            result = false;
                        }
                    }
    if (result)
        testPassed("date.set" + functionNameRoot + "(arg0, arg1, arg2, arg3)");
    return result;
}



function testDateFunction(functionNameRoot, functionParamNum)
{
    var success = true;

    switch (functionParamNum) {
    case 4:
        success &= testDateFunctionWithValueExpectingNaN4(functionNameRoot);
        if (functionNameRoot != "Time" &&
            functionNameRoot != "FullYear" &&
            functionNameRoot != "UTCFullYear" &&
            functionNameRoot != "Year")
            success &= testDateFunctionWithValueNoRecoverNaN(functionNameRoot, 4);

    case 3:
        success &= testDateFunctionWithValueExpectingNaN3(functionNameRoot);
        if (functionNameRoot != "Time" &&
            functionNameRoot != "FullYear" &&
            functionNameRoot != "UTCFullYear" &&
            functionNameRoot != "Year")
            success &= testDateFunctionWithValueNoRecoverNaN(functionNameRoot, 3);

    case 2:
        success &= testDateFunctionWithValueExpectingNaN2(functionNameRoot);
        if (functionNameRoot != "Time" &&
            functionNameRoot != "FullYear" &&
            functionNameRoot != "UTCFullYear" &&
            functionNameRoot != "Year")
            success &= testDateFunctionWithValueNoRecoverNaN(functionNameRoot, 2);

    case 1:
        success &= testDateFunctionWithValueExpectingNaN1(functionNameRoot);
        if (functionNameRoot == "Time")
            success &= testDateFunctionWithValueRecoverTime(functionNameRoot);
        else if (functionNameRoot == "FullYear")
            success &= testDateFunctionWithValueRecoverFullYear(functionNameRoot);
        else if (functionNameRoot == "UTCFullYear")
            success &= testDateFunctionWithValueRecoverUTCFullYear(functionNameRoot);
        else if (functionNameRoot == "Year")
            success &= testDateFunctionWithValueRecoverYear(functionNameRoot);
        else
            success &= testDateFunctionWithValueNoRecoverNaN(functionNameRoot, 1);
    }

    if (success)
        testPassed("date.set" + functionNameRoot + " passed all tests");
}

for (var x in dateFunctionNameRoots)
{
    testDateFunction(dateFunctionNameRoots[x], dateFunctionParameterNum[x]);
}
