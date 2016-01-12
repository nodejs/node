//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var emptyIterator  = function(){ return {next: function () { return { done: true, value: 0};}}}
var simpleIterator = function(args) {
     var iter = (function(args) {
     var j = 0;
     var length = args.length;
     return function Iterator() {
     return {next: function() {

        if (j < length)
        {
            return { value: args[j++], done: false };
        }
        else
        {
            j = 0;
            return { done: true };
        }
     }}}})(args)
     return iter;};

function __createIterableObject(a, b, c) {
        if (typeof Symbol === "function" && Symbol.iterator) {
          var arr = [a, b, c, ,];
          var iterable = {
            next: function() {
              return { value: arr.shift(), done: arr.length <= 0 };
            },
          };
          iterable[Symbol.iterator] = function(){ return iterable; }
          return iterable;
        }
        else {
          return eval("(function*() { yield a; yield b; yield c; }())");
        }
      }

var tests = [
   {
       name: "Spread TypeError: Function expected",
       body: function ()
       {
             var a = [1, 2];
             a[Symbol.iterator] = 0;
             assert.throws(function () { var b = [...1]; }, TypeError, "1 is not iterable", "Function expected");
             assert.throws(function () { var b = [...a]; }, TypeError, "Spread when the iterator is not a function", "Function expected");
             var noNextIterator = function(){ return {done: true, value: 0};}
             a[Symbol.iterator] = noNextIterator;
             assert.throws(function () { var b = [...a]; }, TypeError, "Spread when the iterator does not have a next is not valid", "Function expected");
             a = function() {}
             assert.throws(function () { var b = [...a]; }, TypeError, "Spread when the iterator is not a function", "Function expected");
             a = {}
             assert.throws(function () { var b = [...a]; }, TypeError, "Spread when the iterator is not a function", "Function expected");
             a =  /r/g;
             assert.throws(function () { var b = [...a]; }, TypeError, "Spread when the iterator is not a function", "Function expected");
       }
   },
   {
       name: "Kangax Tests",
       body: function ()
       {
             assert.areEqual("𠮷", Array(..."𠮷")[0]);
             assert.areEqual("a", Array(..."a")[0]);

             assert.areEqual("𠮷", [..."𠮷"][0]);
             assert.areEqual("a", [..."a" ][0]);

             var iterableNum = __createIterableObject(1, 2, 3);
             //assert.areEqual(3, Math.max(...Object.create(iterableNum))); //TODO look into why this doesn't work
             assert.areEqual(3, Math.max(...iterableNum));
             var iterableStr = __createIterableObject("b", "c", "d");
             assert.areEqual("d", ["a", ...iterableStr, "e"][3]);
             //assert.areEqual("d", ["a", ...Object.create(iterableStr), "e"][3]);
       }
   },
   {
       name: "Spread TypeError: Object expected",
       body: function ()
       {
             var a = [1, 2];
             a[Symbol.iterator] = function() {};
             assert.throws(function () { var b = [...a]; }, TypeError, "the @@iterator function should return an Object", "Object expected");
       }
   },
   {
       name: "A spread argument's @@iterator has changed",
       body: function ()
       {
             var a = [1, 2];
             var c = [4, 5];
             count = 0;
             a[Symbol.iterator] = function() {
             return { next: function() {
                if (count < 3)
                {
                    return { value: count++, done: false };
                }
                else
                {
                    return { done: true };
                }
             }};};

             var result = [-1,0,1,2,3,4,5,6];
             var b = [-1,...a,3,...c,6]
             assert.areEqual(result,b, "confirm only a spreads with a counting iterator");

             assert.areEqual(3,count,"confirm side effect of incrementing count equals the number we expect");

             c[Symbol.iterator] = simpleIterator(c);
             count = 0;
             var b = [-1,...a,3,...c,6];
             assert.areEqual(result,b, "confirm both a and c spread, with c using  an iterator that increments c's array");
       }
   },
         {
       name: "Array Spread Iterator causes parameter side effects",
       body: function ()
       {
            var a = [1,2];
            var b = 4
            var c = { 0 : false };
            var d = "foo";
            var e = function foo() {}
            var simpleIteratorWithParamSideEffect = function(args) {
                var iter = (function(args) {
                var j = 0;
                var length = args.length;
                return function Iterator() {
                b = 5;
                c[0] = true;
                d = "bar";
                e = function goo() {}
                return {next: function() {

                if (j < length)
                {
                    return { value: args[j++], done: false };
                }
                else
                {
                    j = 0;
                    return { done: true };
                }
                }}}})(args)
                return iter;};

            a[Symbol.iterator] = simpleIteratorWithParamSideEffect(a);

            var result = [...a,b,c,d,e];

            assert.areEqual(1,result[0]);
            assert.areEqual(2,result[1]);
            assert.areEqual(5,result[2]);
            assert.areEqual(true,result[3][0]);
            assert.areEqual("bar",result[4]);
            assert.areEqual("goo",result[5].name);
       }
   },
   {
       name: "Spread with String iterators",
       body: function ()
       {
             var a = "foobar";
             var b = [1,2,...a,4];
             var results = [1,2,'f','o','o','b','a','r',4];
             assert.areEqual(results,b, "confirm we can spread strings");

             var aa = new String(a);
             aa[Symbol.iterator] = simpleIterator(aa);
             var b = [1,2,...aa,4];
             assert.areEqual(results,b, "override the string iterator with a custom iterator that emulates the built in iterator");
             aa[Symbol.iterator] = emptyIterator;
             var b = [1,2,...aa,4];
             assert.areEqual([1,2,4],b,  "override the string iterator with an empty iterator");
       }
   },
   {
       name: "Spread with typedArray iterators",
       body: function ()
       {
             var buf = [2, 3, 4, 5];
             var typedArrays = [new Int8Array(buf), new Uint8Array(buf), new Uint8ClampedArray(buf), new Uint16Array(buf),
                                new Int16Array(buf), new Int32Array(buf), new Uint32Array(buf), new Float32Array(buf),
                                new Float64Array(buf)];
             for(var i = 0; i < typedArrays.length;i++)
             {
                var a = typedArrays[i];
                var b = [1,...a,6];
                var results = [1,...buf,6];
                assert.areEqual(results,b, "confirm TypedArrays still spread");

                a[Symbol.iterator] = simpleIterator(a);
                var b = [1,...a,6];
                assert.areEqual(results,b, "force typed arrays to use a user defined iterator that emulates the buitin iterator behavior");
                a[Symbol.iterator] = emptyIterator;
                var b = [1,...a,6];
                assert.areEqual([1,6],b, " make typed arrays use an empty iterator");
             }
       }
   },
   {
       name: "Spread with Maps & Sets",
       body: function ()
       {
             var kvArray = [["one", 1], ["two", 2]];
             var myMap = new Map(kvArray);
             var b = [0,...myMap];
             assert.areEqual([0,["one", 1], ["two", 2]],b);

             var mapIter = myMap.keys();
             var b = [0,...mapIter];
             assert.areEqual([0, "one", "two"],b,"should show 0 and then myMap's keys");

             var mapIter = myMap.values();
             var b = [0,...mapIter];
             assert.areEqual([0, 1, 2],b,"should show 0 and then myMap's keys");

             var aSet = new Set([1, 2, 3, 4, 5, 5, 5, 5]);
             var b = [0,...aSet, 6];
             assert.areEqual([0,1,2,3,4,5, 6],b);
       }
   },
   {
       name: "Spread with Objects",
       body: function ()
       {
             var obj = { 0 : 1, 1: 1, 2 : 1, 3 : 1, length : 4}
             assert.throws(function () { var b = [...obj]; }, TypeError, "Spread is no longer dependent on length", "Function expected");
             obj[Symbol.iterator] = simpleIterator(obj);
             var b = [...obj];
             assert.areEqual([1,1,1,1],b);
       }
   },
   {
       name: "Spread with WeakMaps & WeakSets",
       body: function ()
       {
             var kvArray = [[{animal: "dog"},"foo" ], [{animal: "cat"},"bar" ]];
             var myMap = new WeakMap(kvArray);
             myMap[Symbol.iterator] = emptyIterator;
             var b = [0,...myMap];
             assert.areEqual([0],b, "confirm any object can spread as long as it has an iterator");

             var aSet = new WeakSet([{},{},{},[4], [5], [5], [5], [5]]);
             aSet[Symbol.iterator] = emptyIterator;
             var b = [0,...aSet, 6];
             assert.areEqual([0,6],b,"confirm any object can spread as long as it has an iterator");
       }
   },
   {
       name: "Spread with arguments",
       body: function ()
       {
             var b = [];
             function bar(a,a){b = [...arguments];}
             bar([1],[2]);
             assert.areEqual([[1],[2]], b);
             class d { constructor() {arguments[Symbol.iterator] = simpleIterator(arguments); b = [...arguments]; } };
             new d(1,2,3);
             assert.areEqual([1,2,3],b, "confirm we can override the built iterator");
             new d();
             assert.areEqual([],b);
             function foo(a, b, c, ...rest) { return [a, b, c, ...rest]; }
             b= foo(1,2,3,4,5,6)
             assert.areEqual([1,2,3,4,5,6],b);
             b = foo(1,2,3);
             assert.areEqual([1,2,3],b);
             b = foo(1,2);
             assert.areEqual([1,2,undefined],b);

       }
   },
   {
       name: "Function Spread with typedArray iterators",
       body: function ()
       {
             var buf = [2, 3, 4];

             function test1(a,b,c,d,e)
             {
                var expected = [a, ...buf, e];
                var results  = [a,b,c,d,e];
                assert.areEqual(results,expected, "confirm TypedArrays still spread");
             }
             function test2(a,b,c,d,e)
             {
                assert.areEqual(1,a, "should be nonspreadable numeric primitive 1");
                assert.areEqual(5,b, "should be nonspreadable numeric primitive 5");
                assert.areEqual(undefined,c);
                assert.areEqual(undefined,d);
                assert.areEqual(undefined,e);
             }

             var typedArrays = [new Int8Array(buf), new Uint8Array(buf), new Uint8ClampedArray(buf), new Uint16Array(buf),
                                new Int16Array(buf), new Int32Array(buf), new Uint32Array(buf), new Float32Array(buf),
                                new Float64Array(buf)];
             for(var i = 0; i < typedArrays.length;i++)
             {
                var a = typedArrays[i];
                test1(1,...a,5);

                a[Symbol.iterator] = simpleIterator(a);
                test1(1,...a,5);

                a[Symbol.iterator] = emptyIterator;
                test2(1,...a,5);
             }
       }
   },
   {
       name: "Function Spread TypeError: Function expected",
       body: function ()
       {
            var a = [1,2,3];
            a[Symbol.iterator] = 0;
            function f(z,...v) {}
             assert.throws(function () { f(0,...a); }, TypeError, "Spread when the iterator is not a function", "Function expected");
             var noNextIterator = function(){ return {done: true, value: 0};}
             a[Symbol.iterator] = noNextIterator;
             assert.throws(function () { f(0,...a); }, TypeError, "Spread when the iterator does not have a next is not valid", "Function expected");
             a = function() {}
             assert.throws(function () { f(0,...a); }, TypeError, "Spread when the iterator is not a function", "Function expected");
             a = {}
             assert.throws(function () { f(0,...a); }, TypeError, "Spread when the iterator is not a function", "Function expected");
             a =  /r/g;
             assert.throws(function () { f(0,...a); }, TypeError, "Spread when the iterator is not a function", "Function expected");
             var b = 11;
             assert.throws(function () { f(0,...b); }, TypeError, "b is a primitive and thus is not iterable", "Function expected");
       }
   },
   {
       name: "Function Spread TypeError: Object expected",
       body: function ()
       {
             var a = [1, 2];
             a[Symbol.iterator] = function() {};
             function f(z,v) {}
             assert.throws(function () { f(0,...a); }, TypeError, "the @@iterator function should return an Object", "Object expected");

       }
   },

   {
       name: "Function Spread Iterator with Arguments",
       body: function ()
       {
             // constructor() { super(...arguments); } (equivalent to constructor(...args) { super(...args); } )
             class base {}
             class child extends base {}
             assert.doesNotThrow(function () { var o = new child(); }, "we should not, get a type Error, arguments has an iterator");

             var a = [1];
             var b = 'a';
             var c = new Set([1,1,1,1]);

             function test(a,b,c)
             {
                assert.areEqual([1], a);
                assert.areEqual('a', b);
                assert.areEqual([1], [...c]);
             }
             class child2 extends base {
             constructor(a,b,c) {
               super(...arguments);
               test(...arguments);
               }};
             var o = new child2(a, b, c);
             class child3 extends base {
             constructor(a,b,c) {
               arguments[Symbol.iterator] = simpleIterator(arguments);
               super(...arguments);
               test(...arguments);
               }};
             var o = new child3(a, b, c);
       }
   },
   {
       name: "Function Spread Iterator override for all objects",
       body: function ()
       {
             var a = new String("fox");
             var b = [1,2,3];
             var c = {0 : 1, 1 : 2, 2 : 3, length : 3};
             a[Symbol.iterator] = simpleIterator(a);
             b[Symbol.iterator] = simpleIterator(b);
             c[Symbol.iterator] = simpleIterator(c);
             function f(a, b, c, d, e, f, w, x, y, z)
             {
                assert.areEqual('f',a, "should be value at global string a[0]");
                assert.areEqual('o',b, "should be value at global string a[1]");
                assert.areEqual('x',c, "should be value at global string a[2]");
                assert.areEqual(1,d, "should be value at global array b[0]");
                assert.areEqual(2,e, "should be value at global array b[1]");
                assert.areEqual(3,f, "should be value at global array b[2]");
                assert.areEqual(1,w, "should be value at global object c[0]");
                assert.areEqual(2,x, "should be value at global object c[1]");
                assert.areEqual(3,y, "should be value at global object c[2]");
                assert.areEqual(4,z, "should be nonspreadable numeric primitive 4");

             }
             f(...a, ...b, ...c, 4);
             function d(a, b, c, d, e, f, w, x, y, z)
             {
                assert.areEqual(4,a, "should be nonspreadable numeric primitive 4");
                assert.areEqual(undefined,b);
                assert.areEqual(undefined,c);
                assert.areEqual(undefined,d);
                assert.areEqual(undefined,e);
                assert.areEqual(undefined,f);
                assert.areEqual(undefined,w);
                assert.areEqual(undefined,x);
                assert.areEqual(undefined,y);
                assert.areEqual(undefined,z);

             }
             a[Symbol.iterator] = emptyIterator;
             b[Symbol.iterator] = emptyIterator;
             c[Symbol.iterator] = emptyIterator;
             d(...a, ...b, ...c, 4);
       }
   },
   {
       name: "Function Spread Iterator causes parameter side effects",
       body: function ()
       {
            var a = [1,2];
            var b = [4,5];
            var c = [7,8];
            var simpleIteratorWithParamSideEffect = function(args) {
                var iter = (function(args) {
                var j = 0;
                var length = args.length;
                return function Iterator() {
                c[Symbol.iterator] = emptyIterator;
                a[Symbol.iterator] = emptyIterator;
                return {next: function() {

                if (j < length)
                {
                    return { value: args[j++], done: false };
                }
                else
                {
                    j = 0;
                    return { done: true };
                }
                }}}})(args)
                return iter;};

            b[Symbol.iterator] = simpleIteratorWithParamSideEffect(b);
            function test(a,b,c,d,e,f)
            {
                assert.areEqual(1,a, "iterator side effect on array 'a' happens after spreading 'a'");
                assert.areEqual(2,b, "iterator side effect on array 'a' happens after spreading 'a'");
                assert.areEqual(4,c);
                assert.areEqual(5,d);
                assert.areEqual(undefined,e,"iterator side effect on array 'c' happens before spreading 'c'");
                assert.areEqual(undefined,f,"iterator side effect on array 'c' happens before spreading 'c'");
            }

            test(...a,...b,...c);
       }
   },
      {
       name: "Function Spread Iterator causes parameter side effects",
       body: function ()
       {
            var a = [1,2];
            var b = 4
            var c = { 0 : false };
            var d = "foo";
            var e = function foo() {}
            var simpleIteratorWithParamSideEffect = function(args) {
                var iter = (function(args) {
                var j = 0;
                var length = args.length;
                return function Iterator() {
                b = 5;
                c[0] = true;
                d = "bar";
                e = function goo() {}
                return {next: function() {

                if (j < length)
                {
                    return { value: args[j++], done: false };
                }
                else
                {
                    j = 0;
                    return { done: true };
                }
                }}}})(args)
                return iter;};

            a[Symbol.iterator] = simpleIteratorWithParamSideEffect(a);
            function test(a,b,c,d,e,f)
            {
                assert.areEqual(1,a, "iterator side effect on array 'a' happens after spreading 'a'");
                assert.areEqual(2,b, "iterator side effect on array 'a' happens after spreading 'a'");
                assert.areEqual(5,c);
                assert.areEqual(true,d[0]);
                assert.areEqual("bar",e);
                assert.areEqual("goo",f.name);
            }

            test(...a,b,c,d,e);
       }
   },
   {
       name: "Function Spread Iterator override for  some objects",
       body: function ()
       {
             var a = new String("fox");
             var b = [1,2,3];
             var c = {0 : 1, 1 : 2, 2 : 3, length : 3};
             a[Symbol.iterator] = simpleIterator(a);
             c[Symbol.iterator] = simpleIterator(c);
             function f(a, b, c, d, e, f, w, x, y, z)
             {
                assert.areEqual('f',a, "should be value at global string a[0]");
                assert.areEqual('o',b, "should be value at global string a[1]");
                assert.areEqual('x',c, "should be value at global string a[2]");
                assert.areEqual(1,d, "should be value at global array b[0]");
                assert.areEqual(2,e, "should be value at global array b[1]");
                assert.areEqual(3,f, "should be value at global array b[2]");
                assert.areEqual(1,w, "should be value at global object c[0]");
                assert.areEqual(2,x, "should be value at global object c[1]");
                assert.areEqual(3,y, "should be value at global object c[2]");
                assert.areEqual(4,z, "should be nonspreadable numeric primitive 4");

             }
             f(...a, ...b, ...c, 4);

             function d(a, b, c, d, e, f, w, x, y, z)
             {
                assert.areEqual(1,a, "should be value at global array b[0]");
                assert.areEqual(2,b, "should be value at global array b[1]");
                assert.areEqual(3,c, "should be value at global array b[2]");
                assert.areEqual(4,d, "should be nonspreadable numeric primitive 4");
                assert.areEqual(undefined,e);
                assert.areEqual(undefined,f);
                assert.areEqual(undefined,w);
                assert.areEqual(undefined,x);
                assert.areEqual(undefined,y);
                assert.areEqual(undefined,z);
             }
             a[Symbol.iterator] = emptyIterator;
             c[Symbol.iterator] = emptyIterator;
             d(...a, ...b, ...c, 4);
       }
   }

];
testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
