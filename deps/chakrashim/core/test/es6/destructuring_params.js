//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
  {
    name: "Basic destructuring syntax as params",
    body: function () {
      assert.doesNotThrow(function () { eval("function foo({x:x}) {}"); }, "Object destructuring pattern as a formal is valid syntax");
      assert.doesNotThrow(function () { eval("function foo({x}) {}"); },   "Object destructuring pattern (shorthand) as a formal is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([x]) {}"); },   "Array destructuring pattern as a formal is valid syntax");

      assert.doesNotThrow(function () { eval("function foo({x:x}, y) {}"); },   "Object destructuring pattern as a first formal is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([x], y) {}"); },   "Array destructuring pattern as a first formal is valid syntax");

      assert.doesNotThrow(function () { eval("function foo(y, {x:x}) {}"); },   "Object destructuring pattern as a second formal is valid syntax");
      assert.doesNotThrow(function () { eval("function foo(y, [x]) {}"); },   "Array destructuring pattern as a second formal is valid syntax");


      assert.doesNotThrow(function () { eval("function foo({}) {}"); },   "Object destructuring pattern (with empty syntax) as a formal is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([]) {}"); },   "Array destructuring pattern (with empty syntax) as a formal is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([,]) {}"); },   "Array destructuring pattern (with empty syntax and comma) as a formal is valid syntax");
    }
  },
  {
    name: "Destructuring syntax as params - invalid syntax",
    body: function () {
      assert.throws(function () { eval("function foo({,}) {}"); }, SyntaxError,  "Object destructuring pattern as a formal with empty names is not valid syntax", "Expected identifier, string or number");
      assert.throws(function () { eval("function foo(({})) {}"); }, SyntaxError,  "Object destructuring pattern as a formal surrounded with parens is not valid syntax", "Expected identifier");
      assert.throws(function () { eval("function foo(([])) {}"); }, SyntaxError,  "Array destructuring pattern as a formal surrounded with parens is not valid syntax", "Expected identifier");
      assert.throws(function () { eval("function foo([x}) {}"); }, SyntaxError,  "Array destructuring pattern without right bracket is not valid syntax", "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("function foo({x:abc+1}) {}"); }, SyntaxError,  "Object destructuring pattern with operator is not valid syntax", "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("function foo({x:abc.d}) {}"); }, SyntaxError,  "Object destructuring pattern as a formal with property reference is not valid syntax", "Syntax error");
      assert.throws(function () { eval("function foo([abc.d]) {}"); }, SyntaxError,  "Array destructuring pattern as a formal with property reference is not valid syntax", "Syntax error");
      assert.throws(function () { eval("function foo([super]) {}"); }, SyntaxError,  "Array destructuring pattern as a formal identifier as super is not valid syntax", "The use of a keyword for an identifier is invalid");
      assert.throws(function () { eval("function foo({x:super}) {}"); },  SyntaxError, "Object destructuring pattern as a formal identifier as super is not valid syntax", "The use of a keyword for an identifier is invalid");
      assert.throws(function () { eval("function foo({x:super()}) {}"); }, SyntaxError,  "Object destructuring pattern as a formal identifier as super call is not valid syntax", "The use of a keyword for an identifier is invalid");
      assert.throws(function () { eval("function foo([super()]) {}"); }, SyntaxError,  "Array destructuring pattern as a formal identifier as super call is not valid syntax", "The use of a keyword for an identifier is invalid");
    }
  },
  {
    name: "Destructuring syntax as params - Multiple and mixed patterns",
    body: function () {
      assert.doesNotThrow(function () { eval("function foo({x:x}, {y:y}, {z:z}) {}"); },   "Three params as object pattern is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([x], [y], [z]) {}"); },   "Three params as array pattern is valid syntax");

      assert.doesNotThrow(function () { eval("function foo({x1:x1, x2:x2, x3:x3}, {y1:y1, y1:y2}) {}"); },   "Two params as object pattern, and each pattern has more than one matching name is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([x1, x2, x3], [y1, y2, y3]) {}"); },   "Two params as array pattern, and each pattern has more than one matching name is valid syntax");

      assert.doesNotThrow(function () { eval("function foo({x1:x1}, [y1]) {}"); },   "Two params with first object pattern and second array pattern is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([x1], {y1:y1}) {}"); },   "Two params with first array pattern and second object pattern is valid syntax");

      assert.doesNotThrow(function () { eval("function foo(x1, {x2, x3}, [x4, x5], x6) {}"); }, "Multiple params with mixed of binding identifier and binding pattern is valid syntax");

      assert.doesNotThrow(function () { eval("function foo({x1:[y1]}) {}"); },   "Object destructuring pattern nesting array pattern as a formal is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([x1, {y1:y1}]) {}"); },   "Array destructuring pattern nesting object pattern as a formal is valid syntax");
    }
  },
  {
    name: "Destructuring syntax as params - class/function expression/function expression with names/lambda",
    body: function () {
      assert.doesNotThrow(function () { eval("({x}) => x;"); },   "Object destructuring pattern as a formal on lambda is valid syntax");
      assert.doesNotThrow(function () { eval("([x]) => x;"); },   "Array destructuring pattern as a formal on lambda is valid syntax");

      assert.doesNotThrow(function () { eval("class foo { constructor({x1}){} }"); },   "Object destructuring pattern as a formal on constructor of class is valid syntax");
      assert.doesNotThrow(function () { eval("class foo { constructor([x1]){} }"); },   "Array destructuring pattern as a formal on constructor of class is valid syntax");
      assert.doesNotThrow(function () { eval("class foo { method({x1}){ }; set prop({x1}){} }"); },   "Object destructuring pattern on method and setter of class is valid syntax");
      assert.doesNotThrow(function () { eval("class foo { method([x1]){ }; set prop([x1]){} }"); },   "Array destructuring pattern on method and setter of class is valid syntax");

      assert.doesNotThrow(function () { eval("let foo = function ({x1}, [x2]){};"); },   "Destructuring patterns as formals on function expression is valid syntax");
      assert.doesNotThrow(function () { eval("(function({x1}, [x2]){})"); },   "Destructuring patterns as formals on anonymous function is valid syntax");

      assert.doesNotThrow(function () { eval("let bar = function foo({x1}, [x2]){};"); },   "Destructuring patterns as formals on named function expression is valid syntax");
      assert.doesNotThrow(function () { eval("new Function('{x}', '[y]', 'return x + y');"); },   "Destructuring patterns as formals on function constructor is valid syntax");
      assert.doesNotThrow(function () { eval("let obj = { foo({x}) {}, set prop([x]) {} }"); },   "Destructuring pattern as a formal on functions of object literal is valid syntax");
    }
  },
  {
    name: "Destructuring syntax as params - Initializer",
    body: function () {
      assert.doesNotThrow(function () { eval("function foo({x:x = 10}) {}"); },   "One param as object destructuring pattern with initializer is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([x = 20]) {}"); },   "One param as array destructuring pattern with initializer is valid syntax");

      assert.doesNotThrow(function () { eval("function foo({x1:x1 = 1}, {y1:y1 = 2}) {}"); },   "Two params as object destructuring pattern with initializer is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([x1 = 1], [y1 = 2]) {}"); },   "Two params as array destructuring pattern with initializer is valid syntax");

      assert.doesNotThrow(function () { eval("function foo({x1:x1 = 1, x2:x2 = 2, x3:x3 = 3}) {}"); },   "One param as object destructuring pattern has three names with initializer is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([x1 = 1, x2 = 2, x3 = 3]) {}"); },   "One param as array destructuring pattern has three names with initializer is valid syntax");

      assert.doesNotThrow(function () { eval("function foo({x1:x1 = 1}, [y1 = 2]) {}"); },   "First param as object pattern and second as array pattern with initializer is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([x1 = 1], {y1:y1 = 2}) {}"); },   "First param as array pattern and second as object pattern with initializer is valid syntax");

      assert.doesNotThrow(function () { eval("function foo({x:x} = {x:1}) {}"); }, "Object destructuring pattern has default value is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([x] = [1]) {}"); },  "Array destructuring pattern has default value is valid syntax");

      assert.doesNotThrow(function () { eval("function foo({x:x = 1} = {x:2}) {}"); }, "Object destructuring pattern has default and identifier has initializer is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([x = 1] = [2]) {}"); }, "Array destructuring pattern has default and identifier has initializer is valid syntax");

      assert.doesNotThrow(function () { eval("function foo({x1:[y1 = 1]}) {}"); },   "Object destructuring pattern nesting array pattern which has initializer is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([x1, {y1:y1 = 1}]) {}"); },   "Array destructuring pattern nesting object pattern which has initializer is valid syntax");

      assert.doesNotThrow(function () { eval("function foo({x1:[y1 = 1] = [2]} = {x1:[3]}) {}"); },   "Object destructuring pattern has default and nesting array pattern which has initializer is valid syntax");
      assert.doesNotThrow(function () { eval("function foo([{y1:y1 = 1} = {y1:2}] = [{y1:3}]) {}"); },   "Array destructuring pattern has default and nesting object pattern which has initializer is valid syntax");
    }
  },
  {
    name: "Destructuring syntax as params - redeclarations",
    body: function () {
      assert.throws(function () { eval("function foo({x:x, x:x}) {}"); },  SyntaxError,  "One formal as object pattern has duplicate binding identifiers is not valid syntax", "Let/Const redeclaration");
      assert.throws(function () { eval("function foo({x:x}, {x:x}) {}"); },  SyntaxError,  "Two formals as object patterns have duplicate binding identifiers is not valid syntax", "Let/Const redeclaration");
      assert.throws(function () { eval("function foo([x, x]) {}"); }, SyntaxError,   "One formal as array pattern has duplicate binding identifiers is not valid syntax", "Let/Const redeclaration");
      assert.throws(function () { eval("function foo([x], [x]) {}"); },  SyntaxError,  "Two formals as array patterns have duplicate binding identifiers is not valid syntax", "Let/Const redeclaration");
      assert.throws(function () { eval("function foo([x], {x:x}) {}"); },  SyntaxError,  "Mixed array and object pattern with duplicate identifiers is not valid syntax", "Let/Const redeclaration");
      assert.throws(function () { eval("function foo([x], x) {}"); },  SyntaxError,  "First formal as array pattern has matching name with the second formal is not valid syntax", "Duplicate formal parameter names not allowed in this context");
      assert.throws(function () { eval("function foo(x, [x]) {}"); },  SyntaxError,  "First normal formal is matching with the second formal which is array pattern is not valid syntax", "Let/Const redeclaration");
      assert.throws(function () { eval("function foo({x:{z:[z1]}}, z1) {}"); },  SyntaxError,  "First formal as nesting object pattern has matching name with the second formal is not valid syntax", "Duplicate formal parameter names not allowed in this context");
      assert.throws(function () { eval("function foo([x]) { let x = 10;}"); },  SyntaxError,  "Object destructuring pattern as a formal is valid syntax", "Let/Const redeclaration");
      assert.doesNotThrow(function () { eval("function foo([x]) { var x = 10;}"); },  "var declared names matching with formal (as array pattern) is valid syntax");
    }
  },
  {
    name: "Destructuring on param - basic functionality",
    body : function () {
            function f1({x:x}) {
                return x;
            }
            let ret = f1({x:1});
            assert.areEqual(ret, 1, "Object pattern as a formal matches with actual param and initializes the identifier correctly");

            function f2([x]) {
                return x;
            }
            ret = f2([2]);
            assert.areEqual(ret, 2,  "Array pattern as a formal matches with actual param and initializes the identifier correctly");

            function f3({x}, [y], z) {
                return x + y + z;
            }
            ret = f3({x:1}, [2], 3);
            assert.areEqual(ret, 6,  "First formal as object pattern and second formal as array pattern should match and initialize identifiers correctly");

            let f4 = function ([x], {y:y}, z) {
                return x + y + z;
            }
            ret = f4([1], {y:2}, 3);
            assert.areEqual(ret, 6,  "First formal as array pattern and second formal as object pattern should match and initialize identifiers correctly");
        }
   },
  {
    name: "Destructuring on param - initializer",
    body : function () {
            function f1({x:x1 = 11}, [x2 = 22]) {
                assert.areEqual(x1, 1, "Identifier from the first pattern has initializer but matches the actual first param and initializes correctly");
                assert.areEqual(x2, 2, "Identifier from the second pattern has initializer byt matches the actual second param and initializes correctly");
            }
            f1({x:1}, [2]);

            function f2({x:x1 = 11}, [x2 = 22]) {
                assert.areEqual(x1, 11, "Identifier from the first pattern gets undefined from the actual first param so initializes with initializer");
                assert.areEqual(x2, 22, "Identifier from the second pattern gets undefined from the actual second param so initializes with initializer");
            }
            f2({}, []);

            (function ({x:x1 = 11} = {x:111}, [x2 = 22] = [222]) {
                assert.areEqual(x1, 111, "First pattern matches with default as the actual first param is undefined");
                assert.areEqual(x2, 222, "Second pattern matches with default as the actual second param is undefined");
            })(undefined, undefined);
        }
   },
  {
    name: "Destructuring on param - functionality on lambda/function expression",
    body : function () {
            (function({x:x1}) {
                assert.areEqual(x1, 1,  "Anonymous function - object pattern as a formal matches with actual param and initializes identifier correctly");
            })({x:1});

            let f1 = ([x1]) => x1 * 2;
            assert.areEqual(f1([2]), 4,  "Lambda - array pattern as a formal matches with actual param and initializes identifier correctly");

            let f2 = ({x:x2}) => x2 * 4;
            assert.areEqual(f2({x:2}), 8,  "Lambda - object pattern as a formal matches with actual param and initializes identifier correctly");

            let f3 = function foo({x:x1}) {
                assert.areEqual(x1, 1,  "Named function expression - object pattern as a formal matches with actual param and initializes identifier correctly");
            }
            f3({x:1});

            let f4 = new Function("{x}", "[y]", "return x + y");
            assert.areEqual(f4({x:1}, [2]), 3,  "Function constructor - patterns as formals match with actual params and initialize identifiers correctly");
        }
   },
  {
    name: "Destructuring on param - captures",
    body : function () {
        function f1({x:x1}) {
            function f1_1() {
                assert.areEqual(x1, 1,  "Identifier from object pattern is captured and initialized correctly");
            }
            f1_1();
        }
        f1({x:1});

        function f2([x1]) {
            function f2_1() {
                assert.areEqual(x1, 2,  "Identifier from array pattern is captured and initialized correctly");
            }
            f2_1();
        }
        f2([2]);

        function f3({x:x1}, [y1], z) {
            (function () {
                x1++;
            })();
            (function () {
                y1++;
            })();
            var k = x1 + y1 + z;
            assert.areEqual(k, 62,  "Identifiers from patterns are captured and initialized correctly");
        }
        f3({x:10}, [20], 30);
     }
   },
  {
    name: "Destructuring on param - capture due to eval",
    body : function () {
        function f1({x:x1}, [x2]) {
            eval('');
            assert.areEqual(x1, 1,  "Function has eval - identifier from the object pattern is initialized correctly");
            assert.areEqual(x2, 2,  "Function has eval - identifier from the array pattern is initialized correctly under eval");
        }
        f1({x:1}, [2]);

        function f2({x:x1}, [x2]) {
            eval('');
            (function () {
                x1;
                x2;
            })();
            assert.areEqual(x1, 3, "Function has eval and inner function - identifier from the object pattern is initialized correctly");
            assert.areEqual(x2, 4, "Function has eval and inner function - identifier from the array pattern is initialized correctly");
        }
        f2({x:3}, [4]);

        function f3({x:x1}, [x2]) {
            (function () {
                eval('');
                assert.areEqual(x1, 5,  "Function's inner function has eval - identifier from the object pattern is initialized correctly");
                assert.areEqual(x2, 6,  "Function's inner function has eval - identifier from the object pattern is initialized correctly");
            })();
        }
        f3({x:5}, [6]);
     }
   },
  {
    name: "Destructuring on param - captures (eval and arguments)",
    body : function () {
        function f1({x:x1}, [x2]) {
            assert.areEqual(arguments[0], {x:1},  "arguments[0] is initialized correctly with first actual argument");
            assert.areEqual(arguments[1], [2],  "arguments[1] is initialized correctly with second actual argument");
        }
        f1({x:1}, [2]);

        function f2({x:x1}, [x2]) {
            (function() {
            })();
            eval('');
            assert.areEqual(arguments[0], {x:1},  "Function has inner function and eval - arguments[0] is initialized correctly with first actual argument");
            assert.areEqual(arguments[1], [2],  "Function has inner function and eval - arguments[1] is initialized correctly with second actual argument");
        }
        f2({x:1}, [2]);

        function f3({x:x1}, [x2]) {
            (function() {
                eval('');
            })();
            assert.areEqual(arguments[0], {x:1},  "Function's inner function has eval - arguments[0] is initialized correctly with first actual argument");
            assert.areEqual(arguments[1], [2],  "Function's inner function has eval - arguments[1] is initialized correctly with second actual argument");
        }
        f3({x:1}, [2]);

        (function({x:x1}, x2) {
            x2 = 3;
            assert.areEqual(arguments[1], 2,  "arguments object is unmapped - changing second formal does not change arguments[1]");
        })({x:1}, 2);

        (function ([x1], x2) {
            arguments[1] = 2;
            assert.areEqual(x2, 1,  "arguments object is unmapped - changing arguments[1] does not change second param");
        })([], 1);

        (function ({x:x1}, x2) {
            x2 = 2;
            (function() {
                eval('');
            })();
            assert.areEqual(arguments[1], 1,  "Function's inner function has eval - arguments object is unmapped - changing second formal does not change arguments[1]");
        })({}, 1);

        (function ([x1], x2) {
            (function() {
                eval('');
            })();
            arguments[1] = 2;
            assert.areEqual(x2, 1,  "Function's inner function has eval - arguments object is unmapped - changing arguments[1] does not change second param");
        })([], 1);

    }
  },
  {
    name: "Destructuring on param - multiple/mixed/nested parameters",
    body : function () {
        (function (x1, {x2, x3}, [x4, x5], x6) {
            var k = x1 + x2 + x3 + x4 + x5 + x6;
            assert.areEqual(k, 21,  "Identifiers under various patterns are matched and initialized correctly");
        })(1, {x2:2, x3:3}, [4, 5], 6);

        (function (x1, {x2, x3}, [x4, x5], x6) {
            var k = x1 + x2 + x3 + x4 + x5 + x6;
            eval('');
            assert.areEqual(k, 21,  "Function has eval - identifiers under various patterns are matched and initialized correctly");
        })(1, {x2:2, x3:3}, [4, 5], 6);

        (function (x1, {x2, x3}, [x4, x5], x6) {
            var k = x1 + x2 + x3 + x4 + x5 + x6;
            (function() {
                eval('');
            });
            assert.areEqual(k, 21,  "Function's inner function has eval - identifiers under various patterns are matched and initialized correctly");
        })(1, {x2:2, x3:3}, [4, 5], 6);

     }
   },
  {
    name: "Destructuring on param - misc",
    body : function () {
        let f1 = function foo(x0, {x:x1}, [x2]) {
            assert.areEqual(x1, 1,  "Function expression with name - identifier from second formal is matched and initialized correctly");
            assert.areEqual(x2, 2,  "Function expression with name - identifier from third formal is matched and initialized correctly");
        }
        f1(0, {x:1}, [2]);

        let f2 = function foo1(x0, {x:x1}, [x2]) {
            eval('');
            assert.areEqual(x1, 1,  "Function expression with name has eval - identifier from second formal is matched and initialized correctly");
            assert.areEqual(x2, 2,  "Function expression with name has eval - identifier from third formal is matched and initialized correctly");
        }
        f2(0, {x:1}, [2]);

        let f3 = function foo2(x0, {x:x1}, [x2]) {
            with(x1) {
                assert.areEqual(x1_1, 1,  "Function expression with name has 'with' - identifier from second formal is matched and initialized correctly");
                assert.areEqual(x2, 2,  "Function expression with name has 'with' - identifier from third formal is matched and initialized correctly");
            }
        }
        f3(0, {x:{x1_1:1}}, [2]);

        let f4 = function foo3(x0, {x:x1}, [x2]) {
            try {
                throw 'abc';
            }
            catch(e) {
                (function () {
                    assert.areEqual(x1, 1,  "Function expression with name has 'try/catch' - identifier from second formal is matched and initialized correctly");
                    assert.areEqual(x2, 2,  "Function expression with name has 'try/catch' - identifier from third formal is matched and initialized correctly");
                })();
            }
        }
        f4(0, {x:1}, [2]);

        let f5 = function foo4(x0, {x:x1}, [x2]) {
            try {
                throw 'abc';
            }
            catch(e) {
                (function () {
                    eval('');
                    assert.areEqual(x1, 1,  "Function expression with name has 'try/catch and eval' - identifier from second formal is matched and initialized correctly");
                    assert.areEqual(x2, 2,  "Function expression with name has 'try/catch and eval' - identifier from third formal is matched and initialized correctly");
                })();
            }
        }
        f5(0, {x:1}, [2]);
     }
   },
  {
    name: "Destructuring on param - class",
    body : function () {
        class Foo {
            add([x1]) {
                this.x1 += x1;
            }

            set prop({x1}) {
                this.x1 = x1;
            }

            get prop() {
                return this.x1;
            }

            static Avg({x1}, [x2], x3) {
                return (x1+x2+x3)/3;
            }
        }

        assert.areEqual(Foo.Avg({x1:3}, [4], 5), 4,  "Class's static function - identifiers from formal patterns are matched and initialized correctly");

        let obj = new Foo();
        obj.prop = {x1:1};
        assert.areEqual(obj.prop, 1, "Class's setter - identifier from the formal object pattern is matched and initialized correctly");

        obj.add([2]);
        assert.areEqual(obj.prop, 3, "Class's method - identifier from the formal array pattern is matched and initialized correctly");
     }
   }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
