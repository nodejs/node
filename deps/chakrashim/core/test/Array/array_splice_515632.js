//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Test cases for BLUE 515632
// Found that arguments to Array.prototype.splice which altered the length
// of the array caused an assert.

if (this.WScript && this.WScript.LoadScriptFile) { // Check for running in ch
    this.WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
}

function getArray() {
    var a = new Array(10);
    for(var i = 0; i < a.length; i++) {
        a[i] = i;
    }
    return a;
}

var tests = [
    {
        name: "Arguments to Array.prototype.splice reduce the length of array by one",
        body: function () {
            var a = getArray();
            var pop = { valueOf: function() { a.pop(); return 0; } };
            var s = a.splice(0, pop);

            // pop decreases the length of the array but we've already calculated the length by that
            // time. When we are done with splice, we'll set the length back to the original value
            // which means we should now have n undefined values at the end of the array where n is
            // equal to the number of calls to pop.

            assert.areEqual([], s, "Result of splice is empty array");
            assert.areEqual(10, a.length, "Array has unchanged length");
            for(var i = 0; i < 9; i++) {
                assert.areEqual(i, a[i], "Array elements before the last are unchanged");
            }
            assert.areEqual(undefined, a[9], "Array is unchanged except last element is undefined now");
        }
    },
    {
        name: "Arguments to Array.prototype.splice reduce the length of array and we don't start splice at 0",
        body: function () {
            var a = getArray();
            var pop = { valueOf: function() { a.pop(); return 0; } };
            var s = a.splice(3, pop);

            assert.areEqual([], s, "Result of splice is empty array");
            assert.areEqual(10, a.length, "Array has unchanged length");
            for(var i = 0; i < 9; i++) {
                assert.areEqual(i, a[i], "Array elements before the last are unchanged");
            }
            assert.areEqual(undefined, a[9], "Array is unchanged except last element is undefined now");
        }
    },
    {
        name: "Arguments to Array.prototype.splice reduce the length of array, we don't start splice at 0, and we have a delete length",
        body: function () {
            var a = getArray();
            var pop = { valueOf: function() { a.pop(); return 2; } };
            var s = a.splice(3, pop);

            assert.areEqual([3,4], s, "Result of splice contains removed elements");
            assert.areEqual(8, a.length, "Array has length reduced by length of removed");
            for(var i = 0; i < 3; i++) {
                assert.areEqual(i, a[i], "Array elements before the removed are unchanged");
            }
            assert.areEqual(5, a[3], "Array elements after the removed are correct");
            assert.areEqual(6, a[4], "Array elements after the removed are correct");
            assert.areEqual(7, a[5], "Array elements after the removed are correct");
            assert.areEqual(8, a[6], "Array elements after the removed are correct");
            assert.areEqual(undefined, a[7], "Last element of array is undefined now");
        }
    },
    {
        name: "Arguments to Array.prototype.splice increases the length of array by one",
        body: function () {
            var a = getArray();
            var push = { valueOf: function() { a.push(10); return 0; } };
            var s = a.splice(0, push);

            // push increases the length of the array but we've already calculated the length by that
            // time and when we are done with splice, we'll set the length back to the original value.

            assert.areEqual(0, s.length, "Result of splice has length of zero");
            assert.areEqual([], s, "Result of splice is empty array");
            assert.areEqual(10, a.length, "Array has unchanged length");
            assert.areEqual([0,1,2,3,4,5,6,7,8,9], a, "Array is unchanged by the splice call");
        }
    },
    {
        name: "Arguments to Array.prototype.splice increases the length of array with start and length",
        body: function () {
            var a = getArray();
            var push = { valueOf: function() { a.push(10); a.push(11); return 3; } };
            var s = a.splice(4, push);

            assert.areEqual(3, s.length, "Result of splice has the correct number of elements");
            assert.areEqual([4,5,6], s, "Result of splice contains removed elements");
            assert.areEqual(7, a.length, "Array has length reduced by length of removed");
            assert.areEqual([0,1,2,3,7,8,9], a, "Array elements before/after the removed are correct");
        }
    },
    {
        name: "Arguments to Array.prototype.splice reduces the length of array to 0",
        body: function () {
            var a = getArray();
            var kill = { valueOf: function() { while(a.length > 0) { a.pop(); } return 0; } };
            var s = a.splice(0, kill);

            assert.areEqual(0, s.length, "Result of splice has length of zero");
            assert.areEqual([], s, "Result of splice is empty");
            assert.areEqual(10, a.length, "Array length is unchanged");
            for(var i = 0; i < 10; i++) {
                assert.areEqual(undefined, a[i], "Array elements are all undefined");
            }
        }
    },
    {
        name: "Arguments to Array.prototype.splice reduces the length of array to 0 and we delete some elements",
        body: function () {
            var a = getArray();
            var kill = { valueOf: function() { while(a.length > 0) { a.pop(); } return 2; } };
            var s = a.splice(5, kill);

            assert.areEqual(2, s.length, "Result of splice is array of undefined values");
            for(var i = 0; i < 2; i++) {
                assert.areEqual(undefined, s[i], "Splice result elements are all undefined");
            }
            assert.areEqual(8, a.length, "Array length is reduced by number of removed elements");
            for(var i = 0; i < 8; i++) {
                assert.areEqual(undefined, a[i], "Array elements are all undefined");
            }
        }
    },
    {
        name: "Arguments to Array.prototype.splice reduces the length of array to 0 and we delete some elements, taking some elements from the unchanged length and some from the changed length",
        body: function () {
            var a = getArray();
            var kill = { valueOf: function() { while(a.length > 6) { a.pop(); } return 2; } };
            var s = a.splice(5, kill);

            assert.areEqual(2, s.length, "Result of splice contains an element from array and undefined (since array size was shrunk)");
            assert.areEqual(5, s[0], "Splice result first element is from array");
            assert.areEqual(undefined, s[1], "Splice result remaining elements are undefined");

            assert.areEqual(8, a.length, "Array length is reduced by number of removed elements");
            for(var i = 0; i < 5; i++) {
                assert.areEqual(i, a[i], "Array elements are unchanged except where popped");
            }
            for(var i = 5; i < 8; i++) {
                assert.areEqual(undefined, a[i], "Array elements after the popped point are undefined");
            }
        }
    }
];

testRunner.runTests(tests);
