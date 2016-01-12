//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function testHoistability(unknown_i, unknown_n) {
    var sum = 0, a;

    a = [1, 2, 3, 4];
    for(var i = 0; i < 4; ++i)
        sum += a[i];

    a = [1, 2, 3, 4];
    for(var i = 3; i >= 0; --i)
        sum += a[i];

    a = [1, 2, 3, 4];
    for(var i = 0, j = 0; i < 4; ++i, ++j)
        sum += a[j];

    a = [1, 2, 3, 4];
    for(var i = 3, j = 3; i >= 0; --i, --j)
        sum += a[j];

    var a = [1, 2, 3, 4];
    for(var i = unknown_i; i < unknown_n; ++i)
        sum += a[i];

    a = [1, 2, 3, 4];
    for(var i = unknown_n - 1; i >= unknown_i; --i)
        sum += a[i];

    a = [1, 2, 3, 4];
    for(var i = unknown_i, j = unknown_i; i < unknown_n; ++i, ++j)
        sum += a[j];

    a = [1, 2, 3, 4];
    for(var i = unknown_n - 1, j = unknown_n - 1; i >= unknown_i; --i, --j)
        sum += a[j];

    return sum;
}
WScript.Echo("testHoistability: " + testHoistability(0, 4));
WScript.Echo("testHoistability: " + testHoistability(0, 4));
WScript.Echo("");

function testUnhoistability(unknown_i, unknown_n) {
    var sum = 0, a;

    a = [1, 2, 3, 4];
    for(var i = -1; i < 4; ++i)
        sum += a[i];

    a = [1, 2, 3, 4];
    for(var i = 3; i >= -1; --i)
        sum += a[i];

    a = [1, 2, 3, 4];
    for(var i = 0; i < 5; ++i)
        sum += a[i];

    a = [1, 2, 3, 4];
    for(var i = 4; i >= 0; --i)
        sum += a[i];

    a = [1, 2, 3, 4];
    for(var i = -1, j = -1; i < 4; ++i, ++j)
        sum += a[j];

    a = [1, 2, 3, 4];
    for(var i = 3, j = 3; i >= -1; --i, --j)
        sum += a[j];

    a = [1, 2, 3, 4];
    for(var i = 0, j = 0; i < 5; ++i, ++j)
        sum += a[j];

    a = [1, 2, 3, 4];
    for(var i = 4, j = 4; i >= 0; --i, --j)
        sum += a[j];

    var a = [1, 2, 3, 4];
    for(var i = unknown_i - 1; i < unknown_n; ++i)
        sum += a[i];

    a = [1, 2, 3, 4];
    for(var i = unknown_n - 1; i >= unknown_i - 1; --i)
        sum += a[i];

    var a = [1, 2, 3, 4];
    for(var i = unknown_i; i <= unknown_n; ++i)
        sum += a[i];

    a = [1, 2, 3, 4];
    for(var i = unknown_n; i >= unknown_i; --i)
        sum += a[i];

    a = [1, 2, 3, 4];
    for(var i = unknown_i - 1, j = unknown_i - 1; i < unknown_n; ++i, ++j)
        sum += a[j];

    a = [1, 2, 3, 4];
    for(var i = unknown_n - 1, j = unknown_n - 1; i >= unknown_i - 1; --i, --j)
        sum += a[j];

    a = [1, 2, 3, 4];
    for(var i = unknown_i, j = unknown_i; i <= unknown_n; ++i, ++j)
        sum += a[j];

    a = [1, 2, 3, 4];
    for(var i = unknown_n, j = unknown_n; i >= unknown_i; --i, --j)
        sum += a[j];

    return sum;
}
WScript.Echo("testUnhoistability: " + testUnhoistability(0, 4));
WScript.Echo("testUnhoistability: " + testUnhoistability(0, 4));
WScript.Echo("");

function testInductionVariableWithConstantValue_0(i) {
    var a = [1, 2];
    var sum = 0;
    for(; i == 1; ++i)
        sum += a[i];
    return sum;
}
WScript.Echo("testInductionVariableWithConstantValue_0: " + testInductionVariableWithConstantValue_0(1));
WScript.Echo("testInductionVariableWithConstantValue_0: " + testInductionVariableWithConstantValue_0(1));
WScript.Echo("");

function testInductionVariableWithConstantValue_1(i) {
    var a = [1, 2];
    var sum = 0;
    for(; i == 1; --i)
        sum += a[i];
    return sum;
}
WScript.Echo("testInductionVariableWithConstantValue_1: " + testInductionVariableWithConstantValue_1(1));
WScript.Echo("testInductionVariableWithConstantValue_1: " + testInductionVariableWithConstantValue_1(1));
WScript.Echo("");

function testInductionVariableWithConstantValue_2(i) {
    var a = [1, 2];
    var sum = 0;
    for(var j = i; i == 1; ++i, ++j)
        sum += a[j];
    return sum;
}
WScript.Echo("testInductionVariableWithConstantValue_2: " + testInductionVariableWithConstantValue_2(1));
WScript.Echo("testInductionVariableWithConstantValue_2: " + testInductionVariableWithConstantValue_2(1));
WScript.Echo("");

function testInductionVariableWithConstantValue_3(i) {
    var a = [1, 2];
    var sum = 0;
    for(var j = i; i == 1; --i, --j)
        sum += a[j];
    return sum;
}
WScript.Echo("testInductionVariableWithConstantValue_3: " + testInductionVariableWithConstantValue_3(1));
WScript.Echo("testInductionVariableWithConstantValue_3: " + testInductionVariableWithConstantValue_3(1));
WScript.Echo("");

(function() {
    function testInductionVariableEqualsConstantAndHoistability(a, i, n) {
        var sum = 0;
        for(; i < n; ++i) {
            if(i === 0)
                sum = 0;
            sum += a[i];
        }
        return sum;
    }
    var a = [1, 2];
    testInductionVariableEqualsConstantAndHoistability(a, 0, 2);
    testInductionVariableEqualsConstantAndHoistability(a, 0, 2);
    WScript.Echo("");
})();

(function() {
    function testHoistabilityAndCompatibilityAndBoundInfoPropagationOutOfLoop(a, b) {
        var sum = 0;
        var n = a.length;
        for(var i = 0; i < n; ++i) {
            if(a[i] === 0) {
                ++sum;
                break;
            }
        }
        if(b)
            a[n] = 0;
        return sum;
    }
    var a = [1, 2];
    testHoistabilityAndCompatibilityAndBoundInfoPropagationOutOfLoop(a);
    testHoistabilityAndCompatibilityAndBoundInfoPropagationOutOfLoop(a);
    WScript.Echo("");
})();
