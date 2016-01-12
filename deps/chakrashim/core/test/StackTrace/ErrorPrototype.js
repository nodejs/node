//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//
// Test throw an object with Error or Error.prototype in its prototype chain
//

function Dump(output) {
    if (this.WScript) {
        WScript.Echo(output);
    } else {
        alert(output);
    }
}

function testErrorStack(throwObject, msg) {
    Dump(msg);
    try {
        throw throwObject;
    } catch (e) {
        Dump(TrimStackTracePath(e.stack));
    }
    Dump("");
}

function testErrorPrototype(proto, msg) {
    function E(msg) {
        this.message = msg;
    }
    E.prototype = proto;

    testErrorStack(new E(msg), msg);
}

function testErrorPrototypeChain(proto, msg) {
    function P(){}
    P.prototype = proto;

    testErrorPrototype(proto, "Prototype is " + msg);
    testErrorPrototype(new P(), "Prototype has " + msg);
}

function runtest() {
    testErrorPrototypeChain(new Error(), "new Error()");
    testErrorPrototypeChain(Error.prototype, "Error.prototype");
    testErrorPrototypeChain(new RangeError(), "new RangeError()");
    testErrorPrototypeChain(RangeError.prototype, "RangeError.prototype");

    testErrorPrototypeChain(123, "123");
    testErrorPrototypeChain(new String(), "new String()");

    testErrorStack(Error.prototype, "throw Error.prototype");
    testErrorStack(RangeError.prototype, "throw RangeError.prototype");
    testErrorStack(TypeError.prototype, "throw TypeError.prototype");
}

if (this.WScript && this.WScript.LoadScriptFile) {
    this.WScript.LoadScriptFile("TrimStackTracePath.js");
}
runtest();
