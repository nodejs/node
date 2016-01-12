//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

(function testInlineSlotCapacityLocking1() {
    WScript.Echo("Test: testInlineSlotCapacityLocking1...");
    // Wrap the constructors into an object literal with PathTypeHandler to make sure we get fixed functions.
    // Note that scope slot arrays don't get fixed functions.
    var Namespace = {
        // This constructor creates objects with 8 inline slots and uses them all.
        ConstructedObject1: function () {
            this.a = 0;
            this.b = 0;
            this.c = 0;
            this.d = 0;
            this.e = 0;
            this.f = 0;
            this.g = 0;
            this.h = 0;
        },

        // This constructor creates objects with 8 inline slots, but leaves the last two slots empty.
        // We need the same number of inline slots to follow the same type path.
        ConstructedObject2: function () {
            this.a = 0;
            this.b = 0;
            this.c = 0;
            this.d = 0;
            this.e = 0;
            this.f = 0;
        }
    };

    // To follow the same type path the objects must be constructed from the same prototype.
    Namespace.ConstructedObject2.prototype = Namespace.ConstructedObject1.prototype;

    var construct1 = function () {
        // After the first call to this constructor we attempt to shrink inline slot capacity,
        // but no shrinking takes place, because we actually use all the slots.
        return new Namespace.ConstructedObject1();
    }

    // Let's construct the first object to populate the constructor cache.
    var o1 = construct1();

    // Running with -maxInterpretCount:1 will ensure we JIT before calling construct1 again.
    // Upon JIT-ing we will try to ensure that the inline slot capacity of the type handlers
    // on the type path to the final type is locked.
    var o2 = construct1();

    // Now let's construct an object from the other constructor, which will send it down the
    // same type path, but land on an earlier type handler.
    var o3 = new Namespace.ConstructedObject2();

    // If we now add more properties to reach the same type handler as o1 and o2, we shoudln't
    // hit any surprises with locked or unlocked inline slot capacity.
    // Bug 170326: EnsureInlineSlotCapacityLocked called when JIT-ing construct1
    // would lock inline slot capacity from the given type handler down the successor tree - 
    // without starting at the root.
    o3.g = 1;
    o3.h = 1;

    WScript.Echo("Passed");
})();