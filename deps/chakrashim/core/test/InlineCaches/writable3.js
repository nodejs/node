//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//
// Test changing [writable] attribute should trigger Type transition.
//
var echo = this.WScript ? WScript.Echo : function () { console.log([].join.apply(arguments, [", "])); };
function assert(value, msg) { if (!value) { throw new Error("Failed: " + msg); } }
function endTest() { echo("pass"); }

//
// Win8: 713428
//
//  When SET writable on a NON-shared SimpleTypeHandler, we fail to ChangeType, thus fail to invalidate cache.
//  This test exploits the bug with inline cache.
//

function changePrototype(f, expectedSucceed, msg) {
    var tmp = new Object();

    // This exploits inline cache
    f.prototype = tmp;

    var succeeded = (f.prototype === tmp);
    assert(succeeded === expectedSucceed, msg);
}

// Make f to use a NON-shared SimpleTypeHandler
var f = new Boolean(true);  // NullTypeHandler
f.prototype = 12;           // Evolve to Non-shared SimpleTypeHandler

Object.defineProperty(f, "prototype", { writable: false }); // Clear writable
changePrototype(f, false, "Should not be able to change f.prototype, writable false");

Object.defineProperty(f, "prototype", { writable: true });  // SET writable
changePrototype(f, true, "f.prototype is now writable, should be changed");

endTest();
