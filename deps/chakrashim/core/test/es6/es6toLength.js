//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
   {
       name: "Concat toLength tests",
       body: function ()
       {
            var c = [];
            c[0] = 1;
            c[4294967294] = 2;
            var obj = {length : 3, 0 : 3, 1 : 4, 2: 5, [Symbol.isConcatSpreadable] : true}
            c = c.concat(obj);
            assert.areEqual(1, c[0], "confirm indices of array concated to did not change")
            assert.areEqual(2, c[4294967294], "confirm indices of array concated to did not change");
            assert.areEqual(3, c[4294967295], "confirm obj is spread as properties beyond array length bound");
            assert.areEqual(4, c[4294967296], "confirm obj is spread as properties beyond array length bound");
            assert.areEqual(5, c[4294967297], "confirm obj is spread as properties beyond array length bound");
            assert.areEqual(4294967295, c.length, "length maxes out at 4294967295");

            var c = [];
            c[0] = 1;
            c[4294967294] = 2;
            c = c.concat(3,4,5);
            assert.areEqual(1, c[0], "confirm indices of array concated to did not change")
            assert.areEqual(2, c[4294967294], "confirm indices of array concated to did not change");
            assert.areEqual(3, c[4294967295], "confirm integers are spread as properties beyond array length bound");
            assert.areEqual(4, c[4294967296], "confirm integers are spread as properties beyond array length bound");
            assert.areEqual(5, c[4294967297], "confirm integers are spread as properties beyond array length bound");
            assert.areEqual(4294967295, c.length, "length maxes out at 4294967295");

            var c = [];
            c[0] = 1;
            c[4294967294] = 2;
            c = c.concat([3,4,5]);
            assert.areEqual(1, c[0], "confirm indices of array concated to did not change")
            assert.areEqual(2, c[4294967294], "confirm indices of array concated to did not change");
            assert.areEqual(3, c[4294967295], "confirm array is spread as properties beyond array length bound");
            assert.areEqual(4, c[4294967296], "confirm array is spread as properties beyond array length bound");
            assert.areEqual(5, c[4294967297], "confirm array is spread as properties beyond array length bound");
            assert.areEqual(4294967295, c.length, "length maxes out at 4294967295");

            var c = [];
            c[0] = 1;
            c[4294967294] = 2;
            var oNeg = { length : -1, 0 : 3, 1: 4, [Symbol.isConcatSpreadable] : true};
            c = c.concat(oNeg);
            assert.areEqual(1, c[0], "confirm indices of array concated to did not change")
            assert.areEqual(2, c[4294967294], "confirm indices of array concated to did not change");
            assert.areEqual(undefined, c[4294967295], "Length of oNeg is coerced to 0 nothing is concated here");
       }
   },
   {
       name: "IndexOf toLength tests",
       body: function ()
       {
            var a = [];
            a[4294967294] = 2;
            a[4294967295] = 3;
            a[4294967296] = 4;
            var o32 = { length : 4294967296, 4294967294 : 2, 4294967295: 3 };
            var maxLength64 = Math.pow(2,53)-1;
            var o64 = { length : maxLength64, [maxLength64-2] : 2, [maxLength64-1]: 3 };
            var oNeg = { length : -1, [-5] : 2, [-2]: 3 };

            assert.areEqual(4294967294,a.indexOf(2, 4294967290), "confirm we can find value 2 in the index range given");
            assert.areEqual(-1,a.indexOf(3, 4294967290), "indexOf on an array is bound by array max length");
            assert.areEqual(-1,a.indexOf(4, 4294967290), "indexOf on an array is bound by array max length");

            assert.areEqual(4294967294,Array.prototype.indexOf.call(o32, 2, 4294967290), "confirm we can find value 2 in the index range given");
            assert.areEqual(4294967295,Array.prototype.indexOf.call(o32, 3, 4294967290), "objects don't have a bounded length so we should find the index given any length");
            var o32 = { length : 4294967297, 4294967294 : 2, 4294967295: 3, 4294967296: 4};
            assert.areEqual(4294967296,Array.prototype.indexOf.call(o32, 4, 4294967290), "objects don't have a bounded length so we should find the index given any length");

            assert.areEqual(maxLength64-2,Array.prototype.indexOf.call(o64, 2, maxLength64-10), "confirm indexOf can have an index up to length set to the max integer without loss of precision");
            assert.areEqual(maxLength64-2,Array.prototype.indexOf.call(o64, 2, maxLength64-10), "confirm indexOf can have an index up to length set to the max integer without loss of precision");
            assert.areEqual(maxLength64-1,Array.prototype.indexOf.call(o64, 3, maxLength64-10), "confirm indexOf can have an index up to length set to the max integer without loss of precision");
            assert.areEqual(maxLength64-1,Array.prototype.indexOf.call(o64, 3, maxLength64-10), "confirm indexOf can have an index up to length set to the max integer without loss of precision");

            assert.areEqual(-1, Array.prototype.indexOf.call(oNeg,2), "confirm negative lengths are coerced to 0, so we should not find any of these indices");

            //Note a.indexOf(2) will spin for a very long time, we should probably Consider enumerating instead of walking all indices
       }
   },
   {
       name: "Reverse toLength tests",
       body: function ()
       {
            var a = [];
            a[0] = 0;
            a[4294967294] = 1;
            a.reverse();
            a[4294967295] = 2;
            a[4294967296] = 3;
            assert.areEqual(1, a[0], " confirm reverse still happens on the bounds of an array");
            assert.areEqual(0, a[4294967294], " confirm reverse still happens on the bounds of an array");

            a.reverse();
            assert.areEqual(0, a[0]," confirm reverse still happens on the bounds of an array");
            assert.areEqual(1, a[4294967294], " confirm reverse still happens on the bounds of an array");
            assert.areEqual(2, a[4294967295], "index 4294967295 is beyond the bounds of an array so it does not reverse");
            assert.areEqual(3, a[4294967296], "index 4294967296 is beyond the bounds of an array so it does not reverse");

            var o32 = { length : -1, 4294967294 : 1, 0: 0 };
            Array.prototype.reverse.call(o32);
            assert.areEqual(0, o32[0], "confirm length does not get converted to 4294967295");
            assert.areEqual(1, o32[4294967294],"confirm length does not get converted to 4294967295");
            assert.areEqual(-1,o32.length, "length returns -1");

            // Note it is not practical to reverse an object length 4294967295 or larger since we will hit an
            // Out of memory exception before computation ever completes. As a result we will have a test coverage hole,
            // but at the moment it is not a real world scenario.
       }
   },
   {
       name: "Shift toLength tests",
       body: function ()
       {
            var a = [];
            a[0] = 0;
            a[4294967294] = 2;
            a[4294967295] = 3;
            a[4294967296] = 4;

            var o32 = { length : -1, 4294967294 : 1, 0: 0 };

            var shifted = a.shift();
            assert.areEqual(0, shifted);
            assert.areEqual(undefined, a[0]);
            assert.areEqual(undefined, a[4294967294], "confirm 4294967294 is now empty after shift");
            assert.areEqual(2, a[4294967293], "confirm 4294967293 now has contents of index 4294967294 after shift");
            assert.areEqual(3, a[4294967295], "confirm index 4294967295 does not shift b\c it past array length max");
            assert.areEqual(4, a[4294967296], "confirm index 4294967296 does not shift b\c it past array length max");

            Array.prototype.shift.call(o32);
            assert.areEqual(0, o32[0], "confirm length does not get converted to 4294967295");
            assert.areEqual(1, o32[4294967294],"confirm length does not get converted to 4294967295");

            // Note it is not practical to shift an object length 4294967295 or larger since we will hit an
            // Out of memory exception before computation ever completes. As a result we will have a test coverage hole,
            // but at the moment it is not a real world scenario.

       }
   },
   {
       name: "UnShift toLength tests",
       body: function ()
       {
            /*var o = {
                0 : 0,
                4294967294 : 2,
                4294967295 : 3,
                4294967296 : 4,
                length : 4294967297
                }//consider property enumeration*/

            var o32 = { length : -1, 4294967294 : 1, 0: 0 };
            Array.prototype.unshift.call(o32, -1);
            assert.areEqual(-1, o32[0], "confirm length does not get converted to 4294967295");
            assert.areEqual(undefined, o32[1], "since length was negative, we can not account for any indicies we over write and so 0 does not shift down");
            assert.areEqual(1, o32["length"], "confirm length does not get converted to 4294967295 and instead is updated after an unshift");
            assert.areEqual(1, o32[4294967294],"length will not account this b\c we added it when length was invalid");

            // Note it is not practical to unshift an object length 4294967295 or larger since we will hit an
            // Out of memory exception before computation ever completes. As a result we will have a test coverage hole,
            // but at the moment it is not a real world scenario.

       }
   },
   {
       name: "Push toLength tests",
       body: function ()
       {
            var o = {
                0 : 0,
                4294967294 : 2,
                4294967295 : 3,
                4294967296 : 4,
                length : 4294967297
                }
            assert.areEqual(4294967297, o.length, "confirm length is 4294967297");
            Array.prototype.push.call(o,5);
            assert.areEqual(5, o[4294967297]);
            assert.areEqual(4294967298, o.length, "confirm length incremented by 1");
            Array.prototype.push.call(o,6,7);
            assert.areEqual(6, o[4294967298]);
            assert.areEqual(7, o[4294967299]);
            assert.areEqual(4294967300, o.length, "confirm length incremented by 2");
       }
   },
   {
       name: "Pop toLength tests",
       body: function ()
       {
            var o = {
                0 : 0,
                4294967294 : 2,
                4294967295 : 3,
                4294967296 : 4,
                length : 4294967297
                }
            var popped = Array.prototype.pop.call(o);
            assert.areEqual(4, popped);
            assert.areEqual(4294967296, o.length, "confirm length decremented by 1");
            var popped = Array.prototype.pop.call(o);
            assert.areEqual(3, popped);
            assert.areEqual(4294967295, o.length, "confirm length decremented by 1");
            var popped = Array.prototype.pop.call(o);
            assert.areEqual(2, popped);
            assert.areEqual(4294967294, o.length, "confirm length decremented by 1");
            var popped = Array.prototype.pop.call(o);
            assert.areEqual(undefined, popped, "pop decrements by one so and we already popped off the top part of this sparse object");
            assert.areEqual(4294967293, o.length, "confirm length decremented by 1");

            var o32 = { length : -1, 4294967294 : 1, 0: 0 };
            var popped = Array.prototype.pop.call(o32);
            assert.areEqual(undefined,popped, "confirm we were unable to pop anything b\c -1 length no longer converts to uint max and instead is coerced to 0");
            assert.areEqual(0, o32[0], "nothing was popped b\c of invalid length");
            assert.areEqual(1, o32[4294967294], "nothing was popped b\c of invalid length");
            assert.areEqual(0, o32.length, "length should get set to 0");

            var a = [0]
            a[4294967294] = 2;
            a[4294967295] = 3;
            a[4294967296] = 4;
            assert.areEqual(4294967295, a.length, "length is at max");
            var popped = a.pop();
            assert.areEqual(2, popped, "confirm we start popping at index 4294967294 and get value 2 from it");
            assert.areEqual(4294967294, a.length, "confirm length decremented by 1");
       }
   },
   {
       name: "Slice toLength tests",
       body: function ()
       {
            var a = [];
            a[0] = 0;
            a[4294967294/2] = 1;
            a[4294967294] = 2;
            a[4294967295] = 3;
            a[4294967296] = 4;
            var o32 = { length : 4294967296, 4294967294 : 2, 4294967295: 3 };
            var oNeg = { length : -1, [-5] : 2, [-2]: 3 };

            var aCopy  = a.slice();
            var aFront = a.slice(0,4294967294/2 +1);
            var aBack  = a.slice(4294967294/2,4294967295);

            assert.areEqual(a[0], aCopy[0]);
            assert.areEqual(a[4294967294/2], aCopy[4294967294/2]);
            assert.areEqual(a[4294967294], aCopy[4294967294]);
            assert.areEqual(undefined, aCopy[4294967295], "slice only copies indices up to uint32max");
            assert.areEqual(undefined, aCopy[4294967296], "slice only copies indices up to uint32max");

            assert.areEqual(a[0], aFront[0]);
            assert.areEqual(a[4294967294/2], aFront[4294967294/2]);

            assert.areEqual(a[4294967294/2], aBack[0]);
            assert.areEqual(a[4294967294],   aBack[4294967294/2]);

            assert.throws(function() { Array.prototype.slice.call(o32); }, RangeError, "slice can't make an array larger than the array max length", "Array length must be a finite positive integer");
            assert.areEqual([], Array.prototype.slice.call(oNeg), "negative length get converted to 0, so slice returns an empty array");

            // If we change the Array species it does not throw but its to slow to test
            /*
            class MyArray extends Array {
                // Overwrite species to the parent Array constructor
                static get [Symbol.species]() { return Object; }
            }
            var myArr = new MyArray();
            myArr[0] = 0;
            myArr[4294967294] = 1;
            myArr[4294967295] = 2;
            Array.prototype.slice.call(myArr);*/
       }
   },
   {
       name: "Splice toLength tests",
       body: function ()
       {
            //TODO when we change splice
       }
   },
   {
       name: "Every toLength tests",
       body: function ()
       {
            var a = [];
            a[0] = 0;
            a[4294967294/2] = 1;
            a[4294967294] = 2;
            a[4294967295] = 3;
            function isEven(element, index, array)
            {
                return element %2 == 0;
            }
            //a.every(isEven); // Note perf issue: spec says callback is invoked only for indexes of the array which have
                               // assigned values; it is not invoked for indexes which have been deleted or
                               // which have never been assigned values.
                               // Sounds like we should do some kind of sparse array optimization or
                               // enumerating instead of walking all indices
            var oNeg = { length : -1, [-5] : 2, [-2]: 3 };
            assert.areEqual(true, Array.prototype.every.call(oNeg, isEven), "oNeg has length coerced 0, so we never find an index that proves our comparison false");

            // Given that is not practical to write tests for arrays with boundary numbers I'm not going to bother with typed Arrays and other objects
       }
   },
   {
       name: "Some toLength tests",
       body: function ()
       {
            var a = [];
            a[0] = 1;
            a[4294967294/2] = 1;
            a[4294967294] = 2;
            a[4294967295] = 3;
            function isEven(element, index, array)
            {
                return element %2 == 0;
            }
            //a.some(isEven); // same issue as Map, ForEach, Filter, & Every
                               // not as bad as Array.prototype.every b\c we can quit as soon as we find a true case
            var oNeg = { length : -1, [-5] : 2, [-2]: 3 };
            assert.areEqual(false, Array.prototype.some.call(oNeg, isEven), "oNeg has length coerced 0, so we never find an index that proves our comparison true");

            // Given that is not practical to write tests for arrays with boundary numbers I'm not going to bother with typed Arrays and other objects
       }
   },
      {
       name: "ForEach toLength tests",
       body: function ()
       {
            var a = [];
            a[0] = 1;
            a[4294967294/2] = 2;
            a[4294967294] = 3;
            a[4294967295] = 4;
            var sum = 0;
            function sumation(element, index, array) {
                sum += element;
            }

            //a.forEach(sumation); // same issue as Map, Filter, Some, & Every
            //assert.areEqual(6,sum);
            sum = 0;
            var oNeg = { length : -1, [-5] : 2, [-2]: 3 };
            Array.prototype.forEach.call(oNeg, sumation);
            assert.areEqual(0,sum,"oNeg has length coerced 0, so we never perform a summation");

            // Given that is not practical to write tests for arrays with boundary numbers I'm not going to bother with typed Arrays and other objects
       }
   },
      {
       name: "Map toLength tests",
       body: function ()
       {
            var a = [];
            a[0] = 1;
            a[4294967294/2] = 4;
            a[4294967294] = 9;
            a[4294967295] = 16;
            //var b = a.map(Math.sqrt); // same issue as ForEach, Some, & Every
            var oNeg = { length : -1, [-5] : 2, [-2]: 3 };
            assert.areEqual([], Array.prototype.map.call(oNeg, Math.sqrt));

            var o32 = { length : 4294967296, 4294967294 : 4, 4294967295: 9 };
            assert.throws(function() { Array.prototype.map.call(o32, Math.sqrt); }, RangeError, "Map can't make an array larger than the array max length", "Array length must be a finite positive integer");

            // If we change the Array species it does not throw but its to slow to test
            /*class MyArray extends Array {
                // Overwrite species to the parent Array constructor
                static get [Symbol.species]() { return Object; }
            }
            var myArr = new MyArray();
            myArr[0] = 0;
            myArr[4294967294] = 1;
            myArr[4294967295] = 2;
            Array.prototype.map.call(myArr, Math.sqrt);*/
       }
   },
   {
       name: "Filter toLength tests",
       body: function ()
       {
            var a = [];
            a[0] = 1;
            a[4294967294/2] = 4;
            a[4294967294] = 9;
            a[4294967295] = 16;

            function biggerThanFive(element)
            {
                return element > 5;
            }
            //a.filter(biggerThanFive); // same issue as Map, ForEach, Some, & Every
            var oNeg = { length : -1, [-5] : 2, [-2]: 3 };
            assert.areEqual([], Array.prototype.filter.call(oNeg, biggerThanFive));
       }
   },
   {
       name: "Reduce toLength tests",
       body: function ()
       {
            var a = [];
            a[0] = 1;
            a[4294967294/2] = 2;
            a[4294967294] = 3;
            a[4294967295] = 4;
            var sum = function(a, b) {
                return a + b;
            }
            // a.reduce(sum); // same issue as Map, ForEach, Filter, Some, & Every

            var oNeg = { length : -1, [-5] : 2, [-2]: 3 };
            assert.throws(function() { Array.prototype.reduce.call(oNeg, sum)},TypeError,"Reduce needs a length greater than 0","Object doesn't support this action");
       }
   },
   {
       name: "ReduceRight toLength tests",
       body: function ()
       {
            var a = [];
            a[0] = 1;
            a[4294967294/2] = 2;
            a[4294967294] = 3;
            a[4294967295] = 4;
            var sum = function(a, b) {
                return a + b;
            }
            // a.reduceRight(sum); // same issue as Reduce, Map, ForEach, Filter, Some, & Every

            var oNeg = { length : -1, [-5] : 2, [-2]: 3 };
            assert.throws(function() { Array.prototype.reduceRight.call(oNeg, sum)},TypeError,"Reduce needs a length greater than 0","Object doesn't support this action");
       }
   },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
