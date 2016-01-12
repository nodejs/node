//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function testMonoInlineSlots() {
    var iterations = 10;
    var a = new Array(iterations);
    for (var i = 0; i < iterations; i++) {
        // This object will have 4 inlined slots
        var o = { a: 0 };
        WScript.Echo("...");
        // This is the field add we want the fast path for.
        o.p = 1;
        WScript.Echo("...");
        // Use o to verify the type got written correctly to the object.
        o.z = -1;
        a[i] = o;
    }
    for (var i = 0; i < iterations; i++) {
        WScript.Echo("{ a: " + a[i].a + ", p: " + a[i].p + ", z: " + a[i].z + "}");
    }
}

WScript.Echo("testMonoInlineSlots: ");
testMonoInlineSlots();
testMonoInlineSlots();
WScript.Echo();


function testMonoInlineSlotsSetOrAdd() {
    var iterations = 10;
    var a = new Array(iterations);
    for (var i = 0; i < iterations; i++) {
        // This object will have 4 inlined slots
        var o = { a: 0 };
        // This adds a property ahead of time for every other object, so the profile indicates we either set or add a property.
        if (i % 2 == 0) {
            o.p = 1;
        }
        WScript.Echo("...");
        // This is the field add we want the fast path for.
        o.p = 1;
        WScript.Echo("...");
        // Use o to verify the type got written correctly to the object.
        o.z = -1;
        a[i] = o;
    }
    for (var i = 0; i < iterations; i++) {
        WScript.Echo("{ a: " + a[i].a + ", p: " + a[i].p + ", z: " + a[i].z + "}");
    }
}

WScript.Echo("testMonoInlineSlotsSetOrAdd: ");
testMonoInlineSlotsSetOrAdd();
testMonoInlineSlotsSetOrAdd();
WScript.Echo();


function testMonoAuxSlots() {
    var iterations = 10;
    var a = new Array(iterations);
    for (var i = 0; i < iterations; i++) {
        // This object will have 0 inlined slots.
        var o = {};
        // This will add aux slot array.
        o.a = 1;
        WScript.Echo("...");
        // This is the field add we want the fast path for.
        // Adding property now will stick it into an aux slot but won't require slot array re-allocation.
        o.p = 1;
        WScript.Echo("...");
        // Use o to verify the type got written correctly to the object.
        o.z = -1;
        a[i] = o;
    }
    for (var i = 0; i < iterations; i++) {
        WScript.Echo("{ a: " + a[i].a + ", p: " + a[i].p + ", z: " + a[i].z + "}");
    }
}

WScript.Echo("testMonoAuxSlots: ");
testMonoAuxSlots();
testMonoAuxSlots();
WScript.Echo();


function testMonoAuxSlotsAdjustmentRequired1() {
    var iterations = 10;
    var a = new Array(iterations);
    for (var i = 0; i < iterations; i++) {
        // This object will have 0 inlined slots.
        var o = {};
        WScript.Echo("...");
        // This is the field add we want the fast path for.
        // Adding property now will require slot array allocation and then will stick it into an aux slot.
        o.p = 1;
        WScript.Echo("...");
        // Use o to verify the type got written correctly to the object.
        o.z = -1;
        a[i] = o;
    }
    for (var i = 0; i < iterations; i++) {
        WScript.Echo("{ p: " + a[i].p + ", z: " + a[i].z + "}");
    }
}

WScript.Echo("testMonoAuxSlotsAdjustmentRequired1: ");
testMonoAuxSlotsAdjustmentRequired1();
testMonoAuxSlotsAdjustmentRequired1();
WScript.Echo();


function testMonoAuxSlotsAdjustmentRequired2() {
    var iterations = 10;
    var a = new Array(iterations);
    for (var i = 0; i < iterations; i++) {
        // This object will have 0 inlined slots.
        var o = {};
        // This will add aux slot array of size 4, but it will fill every slot.
        o.a = 0;
        o.b = 1;
        o.c = 2;
        o.d = 3;
        WScript.Echo("...");
        // This is the field add we want the fast path for.
        // Adding property now will require slot array allocation and then will stick it into an aux slot.
        o.p = 1;
        WScript.Echo("...");
        // Use o to verify the type got written correctly to the object.
        o.z = -1;
        a[i] = o;
    }
    for (var i = 0; i < iterations; i++) {
        WScript.Echo("{ a: " + a[i].a + ", b: " + a[i].b + ", c: " + a[i].c + ", d: " + a[i].d + ", p: " + a[i].p + ", z: " + a[i].z + "}");
    }
}

WScript.Echo("testMonoAuxSlotsAdjustmentRequired2: ");
testMonoAuxSlotsAdjustmentRequired2();
testMonoAuxSlotsAdjustmentRequired2();
WScript.Echo();


function testPoly() {
    var iterations = 25;
    var a = new Array(iterations);
    for (var i = 0; i < iterations; i++) {
        var o;
        switch (i % 3) {
            case 0:
                o = {};
                break;
            case 1:
                o = { a: 0 };
                break;
            case 2:
                o = { b: 0 };
                break;
        }
        if (i % 6 >= 3) {
            o.p = 0;
        }
        WScript.Echo("...");
        // This is the field add we want the fast path for.
        o.p = 1;
        WScript.Echo("...");
        o.z = -1;
        a[i] = o;
    }
    for (var i = 0; i < iterations; i++) {
        WScript.Echo("{ a: " + a[i].a + ", b: " + a[i].b + ", p: " + a[i].p + ", z: " + a[i].z + "}");
    }
}

WScript.Echo("testPoly: ");
testPoly();
testPoly();
WScript.Echo();

(function () {
    var Blank = function () {
    }

    Blank.prototype = { a: 0, b: 0, c: 0 }

    function setUpMonoStoreFieldCacheInvalidation() {
        // Make sure properties a, b, and c are not marked as fixed on Blank's type handler, so that
        // add property inline caches can be populated.
        var o = new Blank();
        o.a = 1;
        o.b = 2;
        o.c = 3;

        var o = new Blank();
        o.a = 1;
        o.b = 2;
        o.c = 3;
    }

    function testMonoStoreFieldCacheInvalidation(o, isNative) {
        // Do this only for the JIT-ed iteration to be sure caches aren't populated and we don't do
        // object type spec.
        if (isNative) {
            o.a = 1;
            o.b = 2;
            o.c = 3;
        }
    }

    WScript.Echo("testMonoStoreFieldCacheInvalidation: ");

    setUpMonoStoreFieldCacheInvalidation();

    var o = new Blank();
    testMonoStoreFieldCacheInvalidation(o, false);
    WScript.Echo("{ a: " + o.a + ", b: " + o.b + ", c: " + o.c + "}");

    var o = new Blank();
    testMonoStoreFieldCacheInvalidation(o, true);
    WScript.Echo("{ a: " + o.a + ", b: " + o.b + ", c: " + o.c + "}");

    // Change the writable attribute without actually adding or deleting any properties.
    // This should invalidate all inline caches for property b.
    Object.defineProperty(Blank.prototype, "b", { writable: false });

    var o = new Blank();
    testMonoStoreFieldCacheInvalidation(o, true);
    WScript.Echo("{ a: " + o.a + ", b: " + o.b + ", c: " + o.c + "}");
})();

WScript.Echo();

(function () {
    var Blank = function () {
    }

    Blank.prototype = { a: 0, b: 0, c: 0 }

    function setUpPolyStoreFieldCacheInvalidation() {
        // Make sure properties a, b, and c are not marked as fixed on Blank's type handler, so that
        // add property inline caches can be populated.
        var o = new Blank();
        o.a = 1;
        o.b = 2;
        o.c = 3;
        o.d = 4;

        var o = new Blank();
        o.a = 1;
        o.b = 2;
        o.c = 3;
        o.d = 4;
    }

    function testPolyStoreFieldCacheInvalidation(objects, isNative) {
        // This loop ensures that inline caches for a and b become polymorphic on the second iteration.
        // That's because the type encountered the second time around is the final type with all properties present.
        var o;
        for (var i = 0; i < 2; i++) {
            o = objects[i];
            o.a = 1;
            o.b = 2;
            o.c = 3;
        }
    }

    function reportResults(objects) {
        for (var i = 0; i < 2; i++) {
            var o = objects[i];
            WScript.Echo("{ a: " + o.a + ", b: " + o.b + ", c: " + o.c + "}");
        }
    }

    WScript.Echo("testPolyStoreFieldCacheInvalidation: ");

    setUpPolyStoreFieldCacheInvalidation();

    var warmUpObjects = new Array(2);
    warmUpObjects[0] = new Blank();
    var o = new Blank();
    o.a = 1;
    o.b = 2;
    o.c = 3;
    warmUpObjects[1] = o;
    testPolyStoreFieldCacheInvalidation(warmUpObjects, false);
    reportResults(warmUpObjects);

    // Change the writable attribute without actually adding or deleting any properties.
    // This should invalidate all inline caches for property b, including those that are part of a
    // polymorphic inline cache - provided they got registered for invalidaiton properly.
    Object.defineProperty(Blank.prototype, "b", { writable: false });

    var testObjects = new Array(2);
    testObjects[0] = new Blank();
    testObjects[1] = new Blank();
    testPolyStoreFieldCacheInvalidation(testObjects, true);
    reportResults(testObjects);

})();