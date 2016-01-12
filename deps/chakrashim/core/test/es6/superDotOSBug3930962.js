//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var sym1 = Symbol("bart");
var count = 0;
class A {
  constructor() {
    count++;
  }
  increment() {
    count++;
  }
  decrement() {
    count--;
  }
  getCount()
  {
    return count;
  }
  1() { return 1; }
  2() { return 2; }
  1.1() { return 1.1; }
  2.2() { return 2.2; }
  [1+3]() { return 4; }
  [1.1+1]() { return 2.1; }
  ["foo"+1]() { return "foo1"; }
  [sym1](){return "bart";}
}

A.prototype.x = 42;
A.prototype["y"] = 30;
A.prototype[10] = 10;
A.prototype[10.1] = 10.1;
Object.defineProperty(A.prototype, "length", {writable : true, value : 2 });
Object.defineProperty(A, "length", {writable : true, value : -1 });

var tests = [
   {
       name: "Access length",
       body: function ()
       {
            class B extends A {
                constructor() {
                    super();
                    assert.areEqual(2, super.length, "confirm we can make dot property call to access A.prototype.length");
                    var super_arrow = () => {
                        assert.areEqual(2, super.length, "confirm we can make dot property call to access A.prototype.length when it is in a lambda");
                    }
                    super_arrow();
                }
            }
            var bar = new B();
       }
   },
   {
       name: "Access of property fields on super",
       body: function ()
       {
            class B extends A {
                constructor() {
                    super();
                    assert.areEqual(42, super.x, "confirm we can make dot property calls on non function types");
                    assert.areEqual(42, super["x"], "confirm we can make index property calls of properties defined as dot proerties");
                    assert.areEqual(30, super["y"], "confirm we can make index property calls on string properties promoted to dot properties");
                    assert.areEqual(10, super[10], "confirm we can make index property calls on integer properties");
                    assert.areEqual(10.1, super[10.1], "confirm we can make index property calls on float point properties");
                    assert.areEqual(10, super["10"], "confirm we can make index property calls on integer properties accessed as strings");
                    assert.areEqual(10.1, super["10.1"], "confirm we can make index property calls on float point properties accessed as strings");
                }
            }
            var bar = new B();
       }
   },
   {
       name: "Access of property fields on super in a lambda",
       body: function ()
       {
            class B extends A {
                constructor() {
                        super();
                        var super_arrow = () => {
                        assert.areEqual(42, super.x, "confirm we can make dot property calls on non function types");
                        assert.areEqual(42, super["x"], "confirm we can make index property calls of properties defined as dot proerties");
                        assert.areEqual(30, super["y"], "confirm we can make index property calls on string properties promoted to dot properties");
                        assert.areEqual(10, super[10], "confirm we can make index property calls on integer properties");
                        assert.areEqual(10.1, super[10.1], "confirm we can make index property calls on float point properties");
                        assert.areEqual(10, super["10"], "confirm we can make index property calls on integer properties accessed as strings");
                        assert.areEqual(10.1, super["10.1"], "confirm we can make index property calls on float point properties accessed as strings");
                    }
                    super_arrow();
                }
            }
            var bar = new B();
       }
   },
   {
       name: "Access of property fields on super in a lambda with super also inside the lambda",
       body: function ()
       {
            class B extends A {
                constructor() {
                    var super_arrow = () => {
                        super();
                        assert.areEqual(42, super.x, "confirm we can make dot property calls on non function types");
                        assert.areEqual(42, super["x"], "confirm we can make index property calls of properties defined as dot proerties");
                        assert.areEqual(30, super["y"], "confirm we can make index property calls on string properties promoted to dot properties");
                        assert.areEqual(10, super[10], "confirm we can make index property calls on integer properties");
                        assert.areEqual(10.1, super[10.1], "confirm we can make index property calls on float point properties");
                        assert.areEqual(10, super["10"], "confirm we can make index property calls on integer properties accessed as strings");
                        assert.areEqual(10.1, super["10.1"], "confirm we can make index property calls on float point properties accessed as strings");
                    }
                    super_arrow();
                }
            }
            var bar = new B();
       }
   },
   {
       name: "lamda call on super  before making a super call",
       body: function ()
       {
            count = 0;
            class B extends A {
                constructor() {
                    var super_arrow = () => {
                        assert.throws(()=>{ super.increment(); }, ReferenceError, "Use before declaration");
                        assert.areEqual(0,count,"We should not have incremented");
                        assert.throws(()=>{ super.increment.call(5); }, ReferenceError, "Use before declaration");
                        assert.throws(()=>{ super[1](); }, ReferenceError, "Use before declaration");
                        assert.throws(()=>{ super[1].call(5); }, ReferenceError, "Use before declaration");
                        super();
                    }
                    super_arrow();
                }
            }
            var bar = new B();
       }
   },
   {
       name: "super.<method>.call",
       body: function ()
       {
            count = 0;
            class B extends A {
                constructor() {
                    super();
                    var super_arrow = () => {
                        super.increment.call(this);
                        assert.areEqual(2,super.getCount.call(this), "confirm we can make the the method call on class A's method inside a lambda");
                        assert.areEqual(1,super[1].call(this), "confirm we can make index method call on class A's method inside a lambda");
                    }
                    super_arrow();
                    super.decrement.call(this);
                    assert.areEqual(1,super.getCount.call(this),"confirm we can make the the method call on class A's method");
                    assert.areEqual(2,super[2].call(this), "confirm we can make index method call on class A's method");
                }
            }
            var bar = new B();
       }
   },
   {
       name: "lamda super dot calls",
       body: function ()
       {
            count = 0;
            class B extends A {
                constructor() {
                    var super_arrow = () => {
                        super();
                        assert.areEqual(1,super.getCount(),"confirm we can make the the method call on class A's method");
                        super.increment();
                        assert.areEqual(2, super.getCount(), "confirm we can make the the method call on class A's method");
                        super.decrement();
                        assert.areEqual(1, super.getCount(), "confirm we can make the the method call on class A's method");
                    }
                    super_arrow();
                }
            }
            var bar = new B();
       }
   },
   {
       name: "lamda super string index calls",
       body: function ()
       {
            count = 0;
            class B extends A {
                constructor() {
                    var super_arrow = () => {
                        super();
                        assert.areEqual(1,super["getCount"](), "confirm we can make the the method call on class A's method");
                        super["increment"]();
                        assert.areEqual(2, super["getCount"](), "confirm we can make the the method call on class A's method");
                        super["decrement"]();
                        assert.areEqual(1, super["getCount"](), "confirm we can make the the method call on class A's method");
                    }
                    super_arrow();
                }
            }
            var bar = new B();
       }
   },
   {
       name: "lambda super double index calls",
       body: function ()
       {
            class B extends A {
                constructor() {
                    var super_arrow = () => {
                        super();
                        assert.areEqual(1.1,super[1.1](), "confirm we can make the the method call on class A's method");
                        assert.areEqual(2.2, super[2.2](), "confirm we can make the the method call on class A's method");
                    }
                    super_arrow();

                }
            }
            var bar = new B();
       }
   },
   {
       name: "lambda super int index calls",
       body: function ()
       {
            class B extends A {
                constructor() {
                    var super_arrow = () => {
                        super();
                        assert.areEqual(1,super[1](), "confirm we can make the the method call on class A's method");
                        assert.areEqual(2, super[2](), "confirm we can make the the method call on class A's method");
                    }
                    super_arrow();

                }
            }
            var bar = new B();
       }
   },
   {
       name: "lamda super computed property double index calls",
       body: function ()
       {
            class B extends A {
                constructor() {
                    var super_arrow = () => {
                        super();
                        assert.areEqual(2.1,super[2.1](), "confirm we can make the the method call on class A's method");
                    }
                    super_arrow();
                }
            }
            var bar = new B();
       }
   },
   {
       name: "lamda super computed property int index calls",
       body: function ()
       {
            class B extends A {
                constructor() {
                    super();
                    assert.areEqual(4,super[4](), "confirm we can make the the method call on class A's method");
                }
            }
            var bar = new B();
       }
   },
   {
       name: "lamda super computed property string concatenated to integer index calls",
       body: function ()
       {
            class B extends A {
                constructor() {
                    var super_arrow = () => {
                        super();
                        assert.areEqual("foo1",super["foo1"](), "confirm we can make the the method call on class A's method");
                    }
                    super_arrow();
                }
            }
            var bar = new B();
       }
   },
   {
       name: "lamda super computed property Symbol index calls",
       body: function ()
       {
            class B extends A {
                constructor() {
                    var super_arrow = () => {
                        super();
                        assert.areEqual("bart",super[sym1](), "confirm we can make the the method call on class A's method");
                    }
                    super_arrow();
                }
            }
            var bar = new B();
       }
   },
   {
       name: "regular super dot calls",
       body: function ()
       {
            count = 0;
            class B extends A {
                constructor() {
                    super();
                    assert.areEqual(1,super.getCount(), "confirm we can make the the method call on class A's method");
                    super.increment();
                    assert.areEqual(2, super.getCount(), "confirm we can make the the method call on class A's method");
                    super.decrement();
                    assert.areEqual(1, super.getCount(), "confirm we can make the the method call on class A's method");
                }
            }
            var bar = new B();
       }
   },
   {
       name: "regular super string index calls",
       body: function ()
       {
            count = 0;
            class B extends A {
                constructor() {
                    super();
                    assert.areEqual(1,super["getCount"](), "confirm we can make the the method call on class A's method");
                    super["increment"]();
                    assert.areEqual(2, super["getCount"](), "confirm we can make the the method call on class A's method");
                    super["decrement"]();
                    assert.areEqual(1, super["getCount"](), "confirm we can make the the method call on class A's method");
                }
            }
            var bar = new B();
       }
   },
   {
       name: "regular super double index calls",
       body: function ()
       {
            class B extends A {
                constructor() {
                    super();
                    assert.areEqual(1.1,super[1.1](), "confirm we can make the the method call on class A's method");
                    assert.areEqual(2.2, super[2.2](), "confirm we can make the the method call on class A's method");

                }
            }
            var bar = new B();
       }
   },
   {
       name: "regular super int index calls",
       body: function ()
       {
            class B extends A {
                constructor() {
                    super();
                    assert.areEqual(1,super[1](), "confirm we can make the the method call on class A's method");
                    assert.areEqual(2, super[2](), "confirm we can make the the method call on class A's method");

                }
            }
            var bar = new B();
       }
   },
   {
       name: "regular super computed property double index calls",
       body: function ()
       {
            class B extends A {
                constructor() {
                    super();
                    assert.areEqual(2.1,super[2.1](), "confirm we can make the the method call on class A's method");
                }
            }
            var bar = new B();
       }
   },
   {
       name: "regular super computed property int index calls",
       body: function ()
       {
            class B extends A {
                constructor() {
                    super();
                    assert.areEqual(4,super[4](), "confirm we can make the the method call on class A's method");
                }
            }
            var bar = new B();
       }
   },
   {
       name: "regular super computed property string concatenated to integer index calls",
       body: function ()
       {
            class B extends A {
                constructor() {
                    super();
                    assert.areEqual("foo1",super["foo1"](), "confirm we can make the the method call on class A's method");
                }
            }
            var bar = new B();
       }
   },
   {
       name: "regular super computed property Symbol index calls",
       body: function ()
       {
            class B extends A {
                constructor() {
                    super();
                    assert.areEqual("bart",super[sym1](), "confirm we can make the the method call on class A's method");
                }
            }
            var bar = new B();
       }
   }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });

