//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if (this.WScript && this.WScript.LoadScriptFile) {
    this.WScript.LoadScriptFile("LongCallStackThrow.js");
}

Dump("-- Error.stackTraceLimit property descriptor");
var desc = Object.getOwnPropertyDescriptor(Error, "stackTraceLimit");
for(var p in desc) {
    Dump(p + ": " + desc[p]);
}
Dump("");

function testLongCallStack(limit, noHeader) {
    if (limit !== undefined) {
        Error.stackTraceLimit = 7; // if next assignment is rejected
        Error.stackTraceLimit = limit;
    }
    if (!noHeader) { Dump("-- Error.stackTraceLimit: " + String(Error.stackTraceLimit)); }
    runtest(30);
    Dump("");
}

testLongCallStack();
testLongCallStack(4);
testLongCallStack(Infinity);
testLongCallStack(1);
testLongCallStack(20);
testLongCallStack(5.1);
testLongCallStack(-1);
testLongCallStack(-3.2);
testLongCallStack(-Infinity);

testLongCallStack(0);
testLongCallStack(+0);
testLongCallStack(-0);
testLongCallStack(Number.NaN);
testLongCallStack("not a number");
testLongCallStack(new Object());

Error.stackTraceLimit = 8;
Dump("-- preset Error.stackTraceLimit: " + Error.stackTraceLimit);
Dump("");

Dump("--Reconfigure to a getter");
Object.defineProperty(Error, "stackTraceLimit", {
    get: function () {
        Dump("** Custom stackTraceLimit getter Called, return 3");
        return 3;
    },
    configurable: true
});
testLongCallStack(undefined, true);

Dump("--Delete it");
delete Error.stackTraceLimit;
testLongCallStack();

Dump("--Available on prototype");
Function.prototype.stackTraceLimit = 2;
testLongCallStack();

Dump("--Set to data property again");
Error.stackTraceLimit = 5;
testLongCallStack();

Dump("--Throw in getter");
Object.defineProperty(Error, "stackTraceLimit", {
    get: function () {
        Dump("** Custom stackTraceLimit getter Called, throw");
        throw "My error in custom stackTraceLimit getter";
    },
    configurable: true
});
testLongCallStack(undefined, true);

Dump("--Throw new Error() in getter");
Object.defineProperty(Error, "stackTraceLimit", {
    get: function () {
        throw new Error("My error in custom stackTraceLimit getter");
    },
    configurable: true
});
testLongCallStack(undefined, true);

Dump("--Throw new Error() in getter for a number of times");
var throwErrorCount = 4;
Object.defineProperty(Error, "stackTraceLimit", {
    get: function () {
        if (throwErrorCount-- > 0) {
            throw new Error("My error in custom stackTraceLimit getter");
        } else {
            return -1;
        }
    },
    configurable: true
});
testLongCallStack(undefined, true);

// Some more tests of different types (appending here to avoid affecting above tests' baseline)
delete Error.stackTraceLimit;
var moreTests = [
    null,
    undefined,
    true,
    false,
    new Boolean(true),
    new Boolean(false),
    "4",
    new String("5"),
    new Number(6),
    new Number(Number.NaN),
    new Number(Number.Infinity),
    new Number(-2),
    [],
    [1, 2, 3],
    {},
    { valueOf: function () { return 2; }, toString: function () { return "valueOf override"; } },
    { valueOf: function () { throw new Error("evil"); }, toString: function () { return "valueOf override throw"; } },
];
moreTests.forEach(function (x) {
    if (x === undefined) {
        Error.stackTraceLimit = undefined;
    }
    testLongCallStack(x);
});
