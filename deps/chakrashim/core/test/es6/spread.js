//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function p(x) {
  WScript.Echo(x);
}

var tests = [
  {
      name: "Testing spread array literals (all should be the same)",
      body: function () {
          var a = [1, 2];
          var b = [3];
          var c = [];
          var d = [4, 5, 6];

          var joined = [];
          joined[0] = [1, 2, 3, 4, 5, 6];
          joined[1] = [...a, ...b, ...c, ...d];
          joined[2] = [1, 2, ...b, ...d];
          joined[3] = [...[1, 2], ...b, ...[], ...[...d]];
          joined[4] = [...[], ...joined[2], ...[], ...c];
          joined[5] = [...a, ...b, ...c, 4, 5, 6];

          for (var i = 0; i < joined.length; ++i) {
            assert.areEqual(joined[i], joined[0], "joined[" + i + "] +  ===  + joined[" + 0 + "]");
          }
      }
  },
  {
    name: "Testing call with spread args (all should be the same)",
    body: function() {
      var a = [1, 2];
      var b = [3];
      var c = [];
      var d = [4, 5, 6];

      function quad(a, b, c, x) {
        return a*x*x + b*x + c;
      }

      var result = [];
      result[0] = quad(1, 2, 3, 4);
      result[1] = quad(...a, ...b, ...c, 4);
      result[2] = quad(...a, ...b, ...[4]);
      result[3] = quad(...[...a, ...b, ...c], 4);

      for (var j = 0; j < result.length; ++j) {
        assert.areEqual(result[j], result[0], "result[" + j + "] === result[0]");
      }
    }
  },
  {
    name: "Testing spread with a lot of padding numbers",
    body: function () {
      function quad(a, b, c, x) {
        return a*x*x + b*x + c;
      }

      var e = [7, 8];
      var f = [12, 13];
      var largeLiteral = [1, 2, 3, 4, 5, 6, ...e, 9, 10, 11, ...f, 14, 15, 16, 17, 18, 19, 20];
      var largeLiteralFull = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20];

      assert.areEqual(largeLiteral, largeLiteralFull, "Array literals equal");
      assert.areEqual(quad(1, 2, 3, 4, 5, 6, ...e, 9, 10, 11, ...f, 14, 15, 16, 17, 18, 19, 20), quad(1, 2, 3, 4, 5, 6), "Calls equal");
    }
  },
  {
    name: "Testing array literals with gaps",
    body: function () {
      assert.areEqual([1, undefined, 3], [...[1,,3]], "Spreading missing values gives undefined in the new array elements");
      assert.areEqual([undefined, undefined, 1, undefined, 2, 3, 1, 2, 3, 3], [...[...[,,1,,2,3], ...[1, 2, 3], 3]], "Spreading missing values gives undefined in the new array elements");
    }
  },
  {
    name: "Testing call spread args with gaps",
    body: function () {
        function spreadValid1(a, b, c, d) {
            assert.areEqual(1,         a, "Spreading call arguments with array gaps: a is 1");
            assert.areEqual(undefined, b, "Spreading call arguments with array gaps: b is undefined");
            assert.areEqual(3,         c, "Spreading call arguments with array gaps: c is 3");
            assert.areEqual(undefined, d, "Spreading call arguments with array gaps: d is undefined");
        }
        spreadValid1(...[1, , 3,]);

        function spreadValid2(e, f, g, h) {
            assert.areEqual(4,         e, "Spreading nested call arguments with array gaps: e is 4");
            assert.areEqual(undefined, f, "Spreading nested call arguments with array gaps: f is undefined");
            assert.areEqual(undefined, g, "Spreading nested call arguments with array gaps: g is undefined");
            assert.areEqual(8,         h, "Spreading nested call arguments with array gaps: h is 8");
        }
        spreadValid2(...[4, , ...[, 8]]);
    }
  },
  {
    name: "Testing spread with string literals",
    body: function () {
      assert.areEqual(["s", "t", "r", "i", "n", "g"], [..."string"], "Spreading a string creates a character array of the string");
    }
  },
  {
    name: "BLUE 491118: Spread outside of a call or array literal",
    body: function () {
      assert.throws(function () { eval("var a = []; ...a") }, SyntaxError, "Spread outside of a call or array literal");
    }
  },
  {
    name: "BLUE 511620: Calling spread operation inside string template results in crash",
    body: function () {
      function foo(a,b){}
      var demo = "hello";
      assert.throws(function() { eval("var strAdd = foo`${...demo}`;"); }, SyntaxError, "Spread inside string template");
    }
  },
  {
    name: "BLUE 490915: ... is a valid operator but ...... (2 spreads combined) is not",
    body: function () {
      assert.throws(function () { eval("var a = []; [......a];") }, SyntaxError, "Two spreads combined is a syntax error");
      assert.throws(function () { eval("function foo(){};var a = []; foo(......a);") }, SyntaxError, "Two spreads combined is a syntax error");
    }
  },
  {
    name: "BLUE 522366: Spreading null or undefined should throw TypeError instead of RuntimeError",
    body: function () {
      function foo(bar) { return bar; }
      assert.throws(function () { foo(...null); }, TypeError);
      assert.throws(function () { [...null]; }, TypeError);
      assert.throws(function () { foo(...undefined); }, TypeError);
      assert.throws(function () { [...undefined]; }, TypeError);
    }
  },
  {
    name: "BLUE 522630: Assignment expresion should be evaluated fisrt and then the results should be spread",
    body: function () {
      let b = { get x() { return "abc"; } }
      a = [1, ...b.x + ""];
      assert.areEqual([1,"a","b","c"], a);
    }
  },
  {
    name: "Spread functionality in non-standard call types",
    body: function () {
      var shortArray = [1,2,3]; // Doesn't use _alloca()
      var longArray = [5,3,8,2,4,8,5,3,1,912341234,33543,12987,3476,-2134124,3245235]; // Uses _alloca()

      // Ctor tests
      assert.areEqual(shortArray, new Array(...shortArray));
      assert.areEqual(longArray, new Array(...longArray));

      function Storage(a,b,c,d,e,f,g,h,i) {
        this.a = a;
        this.b = b;
        this.c = c;
        this.d = d;
        this.e = e;
        this.f = f;
        this.g = g;
        this.h = h;
        this.i = i;
      }

      assert.areEqual(new Storage(1,2,3), new Storage(...shortArray));
      assert.areEqual(new Storage(5,3,8,2,4,8,5,3,1), new Storage(...longArray));

      // Eval
      assert.areEqual("hello", eval(...['"hello"']));
      assert.areEqual(longArray[0], eval(...longArray));
    }
  },
  {
    name: "BLUE: 521584:[ES6][Spread] - Spread inside eval is considered valid without quotes. Currently crashes",
    body: function () {
      assert.areEqual(undefined, eval(...[]));
      assert.areEqual(undefined, eval(...[], ...[]));
      assert.areEqual(undefined, eval(...[], ...[], ...[]));
      assert.areEqual(undefined, eval(...[], ...[], ...[], ...[]));
      assert.areEqual(123123,    eval(...[], ...["123123"]));
      assert.areEqual(123123,    eval(...[], ...["123123"], ...[]));
      assert.areEqual(123123,    eval(...[], ...[], ...["123123"], ...[]));
      assert.areEqual(123123,    eval(...[], ...[], ...[], ...[], ...["123123"]));
      assert.areEqual(123123,    eval(...["123123"], ...[], ...[], ...[], ...[]));
      assert.areEqual(undefined, eval.call(...[]));
      assert.areEqual(undefined, eval.call(...[], ...["123123"]));
    }
  },
  {
    name: "Spread with side effects",
    body: function () {
      var i = 0;
      eval(...["i++"]);
      assert.areEqual(1, i);
      eval(...[], ...["i += 1"]);
      assert.areEqual(2, i);
      eval(...[], ...["i = i - i"], ...[]);
      assert.areEqual(0, i);
    }
  },
  {
    name: "BLUE 584814: Instr::HasFixedFunctionAddressTarget() assertion",
    body: function () {
      function badFunc() {
        function shapeyConstructor(iijcze) {
          if (((NaN += /x/).b)) iijcze.d = (new((4277))(new[1, , ](/x/, ''), ...y));
        }
        for (var z in [/x/, /x/, true, ]) {
                let drjotv = shapeyConstructor(z);
        }
      }
      badFunc(); badFunc();
    }
  },
  {
    name: "BLUE 582720: Spread calls with invalid functions crash in native",
    body: function () {
      function badFunc() {
        (12345)(...[]);
      }
      assert.throws(function () { badFunc(); badFunc(); });
    }
  },
  {
    name: "BLUE 589583: CallIPut with spread causes an assert",
    body: function () {
      function a() {};
      var x = [];
      assert.throws(function() { eval('a(...x)--'); }, ReferenceError, "Spread with CallIPut throws a ReferenceError");
    }
  },
  {
    name: "BLUE 596934, 597412: Incorrect spread argument length handling",
    body: function () {
      function a() {}
      // The implemented version of the spec allows overflow of the length when converting to UInt32.
      assert.throws(function () { a(...new Array (0x50505050)); }, RangeError, "Very large array throws RangeError");
      try {
        a(...new Array(1 << 24 - 1)); // No RangeError throw (max call args)
      } catch (e) {
        // This many args will blow out the stack. But it shouldn't be a length limitation.
        assert.areNotEqual(e.constructor, RangeError, "Max call args should not throw a RangeError");
      }
      assert.throws(function () { a(...new Array(1 << 24)); }, RangeError, "Array size greater than max call arg count throws RangeError");
      assert.throws(function () { a(...new Array(3), ...new Array(1 << 32 - 2)); }, RangeError, "Total spread size greater than max call arg count throws RangeError");
    }
  },
  {
    name: "BLUE 611774: Spread with a prefix operator is allowed anywhere",
    body: function () {
      assert.throws(function () { eval('++...window, z;'); }, SyntaxError, "Invalid use of the ... operator");
    }
  },
  /* This tests is broken and tracked by bug OS 5204357
  {
    name: "Corner case: user changes %ArrayIteratorPrototype%.next property and we should call it",
    body: function () {
      var overrideCalled = false;
      var aip = Object.getPrototypeOf([].values());
      var aipNext = aip.next;
      aip.next = function () {
          overrideCalled = true;
          return aipNext.apply(this, arguments);
      }

      var a = [1];

      function f() { }

      f(...a);

      assert.isTrue(overrideCalled, "Spread of a in call to f should have invoked the overrided %ArrayIteratorPrototype%.next method");

      overrideCalled = false;

      var b = [...a];

      assert.isTrue(overrideCalled, "Spread of a in array initializer should have invoked the overrided %ArrayIteratorPrototype%.next method");

      // restore after test so we don't muck this up for others
      aip.next = aipNext;
    }
  },
  */
  {
    name: "Corner case: Spread of an array with an accessor property (ES5Array) should call that getter and recognize a change in length during iteration",
    body: function () {
      var result = [];
      function foo(a, b, c, d) {
          result.push(a);
          result.push(b);
          result.push(c);
          result.push(d);
      }

      var a = [5, 6, 7];
      a.two = 7;
      Object.defineProperty(a, '2', { get: function () { this[3] = 8; return this.two; } });
      foo(...a);

      assert.areEqual(4, result.length, "Spread for the function argument called the getter and caused the length to be updated to four");
      assert.areEqual(5, result[0], "Spread for the function argument passed the first element of a as the first argument");
      assert.areEqual(6, result[1], "Spread for the function argument passed the second element of a as the second argument");
      assert.areEqual(7, result[2], "Spread for the function argument called the getter of the third element of a and passed the result as the third argument");
      assert.areEqual(8, result[3], "Spread for the function argument recognized the new length of four and passed the fourth element of a as the fourth argument");

      a = [5, 6, 7];
      a.two = 7;
      Object.defineProperty(a, '2', { get: function () { this[3] = 8; return this.two; } });
      var b = [...a];
      result = b;

      assert.areEqual(4, result.length, "Spread for the array initializer called the getter and caused the length to be updated to four");
      assert.areEqual(5, result[0], "Spread for the array initializer copied the first element of a to the first element of b");
      assert.areEqual(6, result[1], "Spread for the array initializer copied the second element of a to the second element of b");
      assert.areEqual(7, result[2], "Spread for the array initializer called the getter of the third element of a and copied the result to the third element of b");
      assert.areEqual(8, result[3], "Spread for the array initializer recognized the new length of four and copied the fourth element of a to the fourth element of b");
    }
  },
  {
    name: "Corner case: Spread of an arguments object with an accessor property (ES5HeapArgumentsObject) should call that getter and recognize a change in length during iteration",
    body: function () {
      var result = [];
      function foo(a, b, c, d) {
          result.push(a);
          result.push(b);
          result.push(c);
          result.push(d);
      }

      function bar() {
        arguments.two = 7;
        // Note arguments object does not change its length like an array when new elements are added,
        // but its length is writable and we can change it ourselves
        Object.defineProperty(arguments, '2', { get: function () { this[3] = 8; this.length++; return this.two; } });
        foo(...arguments);
      }
      bar(5, 6, 7);

      assert.areEqual(4, result.length, "Spread for the function argument called the getter and caused the length to be updated to four");
      assert.areEqual(5, result[0], "Spread for the function argument passed the first element of a as the first argument");
      assert.areEqual(6, result[1], "Spread for the function argument passed the second element of a as the second argument");
      assert.areEqual(7, result[2], "Spread for the function argument called the getter of the third element of a and passed the result as the third argument");
      assert.areEqual(8, result[3], "Spread for the function argument recognized the new length of four and passed the fourth element of a as the fourth argument");

      result = [];

      function fuz() {
        arguments.two = 7;
        // Note arguments object does not change its length like an array when new elements are added,
        // but its length is writable and we can change it ourselves
        Object.defineProperty(arguments, '2', { get: function () { this[3] = 8; this.length++; return this.two; } });
        var b = [...arguments];
        result = b;
      }
      fuz(5, 6, 7);

      assert.areEqual(4, result.length, "Spread for the array initializer called the getter and caused the length to be updated to four");
      assert.areEqual(5, result[0], "Spread for the array initializer copied the first element of a to the first element of b");
      assert.areEqual(6, result[1], "Spread for the array initializer copied the second element of a to the second element of b");
      assert.areEqual(7, result[2], "Spread for the array initializer called the getter of the third element of a and copied the result to the third element of b");
      assert.areEqual(8, result[3], "Spread for the array initializer recognized the new length of four and copied the fourth element of a to the fourth element of b");
    }
  },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
