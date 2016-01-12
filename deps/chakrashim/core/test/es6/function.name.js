//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function testGetProperyNames(foo,checkForName)
{
    var properties = Object.getOwnPropertyNames(foo);
    var len = properties.length;
    var check = 0;
    for(var i = 0; i < len; i++)
    {
        var prop = properties[i].toString();
        if(prop == "prototype" || (checkForName && prop == "name") ||
        prop == "arguments" || prop == "caller" || prop == "length")
        {
            check++;
        }
        if (!checkForName && prop == "name")
        {
            return false;
        }
    }
    return check == len;
}

var tests = [
   {
       name: "function.name",
       body: function ()
       {
            function foo(){} //function declaration
            assert.areEqual("foo",foo.name,"name should be foo");
            foo.name = "bar";
            assert.areEqual("foo",foo.name, "function names are read only");
            assert.areEqual("funcExpr",(function funcExpr(){}).name,"function expression case should still print a name");
            assignment = function(){}; // "assignment"
            assert.areEqual("assignment",assignment.name,"Assignment functions should print the assigned name");
            var lambdaDecl = () => {}; // "lambda assignment"
            assert.areEqual("lambdaDecl",lambdaDecl.name,"lambda assignment should print the assigned name");
            var a = function bar() {}
            var b = foo;
            assert.areEqual("bar",a.name,"Assignment functions should inherit the declaration name in this case bar");
            assert.areEqual("foo",b.name,"Assignment functions should inherit the declaration name in this case foo");
       }
   },
   {
       name: "anonymous function",
       body: function ()
       {
            var f = function() { };
            assert.areEqual("f", f.name, "function name is determined on assignment");
            f.name = "foo";
            assert.areEqual("f", f.name, "function names are read only");
            assert.areEqual(undefined, (function (){}).prototype.name, "function.prototype.name is undefined");
            assert.isFalse(Object.hasOwnProperty.call((function(){}), 'name'), "[hasProperyCheck]: anonymous function does not have a name property");
            assert.areEqual("", (function(){}).name);
            assert.areEqual("", (function(){})["name"]);

            //lambdas () => {}
            assert.isFalse(Object.hasOwnProperty.call((() => {}), 'name'), "[hasProperyCheck]: anonymous lambda function does not have a name property");
            assert.areEqual("", (() => {}).name);
            assert.areEqual("", (() => {})["name"]);

            //generators
            assert.isFalse(Object.hasOwnProperty.call((function*(){}), 'name'), "[hasProperyCheck]: anonymous generator function does not have name property");
            assert.areEqual("", (function*(){}).name);
            assert.areEqual("", (function*(){})["name"]);

            //classes
            assert.isFalse(Object.hasOwnProperty.call(class {}, 'name'), "[hasProperyCheck]: anonymous class does not have a name property");
            assert.areEqual("", class {}.name);
            assert.areEqual("", class {}['name']);

       }
   },
   {
       name: "function.name for external functions",
       body: function ()
       {
            assert.areEqual("LoadScriptFile",WScript.LoadScriptFile.name,"check to make sure external functions are supported");
            assert.areEqual("Quit",WScript.Quit.name,"check to make sure external functions are supported");
            assert.areEqual("LoadScript",WScript.LoadScript.name,"check to make sure external functions are supported");
            assert.areEqual("SetTimeout",WScript.SetTimeout.name,"check to make sure external functions are supported");
            assert.areEqual("ClearTimeout",WScript.ClearTimeout.name,"check to make sure external functions are supported");

            assert.areEqual("prototype,name,caller,arguments",Object.getOwnPropertyNames(WScript.Quit).toString(),"Check to make sure name is exposed");

            //Bug 639652
            var a = WScript.Echo.toString();
            var b = WScript.Echo.name;
            assert.areEqual("Echo",b,"b should be the name of function echo not toString of Echo function body");

       }
    },
    {
       name: "static name method overrides the creation of a name string.",
       body: function ()
       {
             //default constructor case
             var qux = class { static name() {} };
             assert.areEqual("function", typeof qux.name,
                "14.5.15 Runtime Semantics: If the class definition included a 'name' static method then that method is not over-written");
             assert.areEqual("name",qux.name.name,"confirm we get the name 'name'");
             assert.areEqual(qux.name , qux.prototype.constructor.name,
                "confirm qux.prototype.constructor.name is the same functionn as qux.name");
             assert.areEqual("Function",qux.constructor.name,"The function constructor should still have the name Function");

             var qux = class { constructor(a,b) {} static name() {} };
             var quxobj = new qux(1,2);
             assert.areEqual("function", typeof qux.name,
                "14.5.15 Runtime Semantics: If the class definition included a \"name\" static method then that method is not over-written");
             assert.areEqual("name",qux.name.name,"confirm we get the name \"name\"");
             assert.areEqual(qux.name , qux.prototype.constructor.name,
                "confirm qux.prototype.constructor.name is the same function as qux.name");
             assert.areEqual("Function",qux.constructor.name,"The function constructor should still have the name Function");
       }
    },
    {
       name: "function.name's that match IsConstantFunctionName",
       body: function ()
       {
            var o = {
                "" : function() {},
                "Anonymous function" : function() {},
                "Function code" : function() {}
            }
            assert.areEqual("", o[""].name);
            assert.areEqual("Anonymous function", o["Anonymous function"].name, "should not get converted to empty string");
            assert.areEqual("Function code", o["Function code"].name, "should not get converted to Anonymous");
       },
    },
    {
       name: "function.name for built in constructors",
       body: function ()
       {
            function* gf() { }
            assert.areEqual("GeneratorFunction", gf.constructor.name);
            assert.areEqual("Array", Array.name);
            assert.areEqual("ArrayBuffer", ArrayBuffer.name);
            assert.areEqual("DataView", DataView.name);
            assert.areEqual("Error", Error.name);
            assert.areEqual("SyntaxError", SyntaxError.name);
            assert.areEqual("EvalError", EvalError.name);
            assert.areEqual("RangeError", RangeError.name);
            assert.areEqual("ReferenceError", ReferenceError.name);
            assert.areEqual("Boolean", Boolean.name);
            assert.areEqual("Symbol", Symbol.name);
            assert.areEqual("Promise", Promise.name);
            assert.areEqual("Proxy", Proxy.name);
            assert.areEqual("Date", Date.name);
            assert.areEqual("Function", Function.name);
            assert.areEqual("Number", Number.name);
            assert.areEqual("Object", Object.name);
            assert.areEqual("RegExp", RegExp.name);
            assert.areEqual("String", String.name);
            assert.areEqual("Map", Map.name);
            assert.areEqual("Set", Set.name);
            assert.areEqual("WeakMap", WeakMap.name);
            assert.areEqual("WeakSet", WeakSet.name);
       }
    },
    {
       name: "Numeric value test cases",
       body: function ()
       {
            var a = [];
            var b = 1;
            var c = 2;
            a[4] = function() {};
            a[1.2] = function() {};
            function foo()
            {
                return a;
            }
            foo()[5] = function() {};
            a[4+3] = function() {};
            a[b] = function() {};
            a[c] = function() {};
            a[b+c] = function() {};
            var index1 = 4;
            var index2 = 4+8;
            var o = { index1 : function() {}, index2 : function() {}, [index1+1] : function() {}}
            assert.areEqual("index1", o.index1.name);
            assert.areEqual("index2",o.index2.name);
            assert.areEqual("5", o[5].name, "when our name has brackets return the computed name")
            assert.areEqual("b",a[1].name,"expressions are not evaluated, default to expression name");
            assert.areEqual("c",a[2].name,"expressions are not evaluated, default to expression name");
            assert.areEqual("1.2",a[1.2].name,"constants are the given numeric literal");
            var o = { 1.4 : function() {} };
            assert.areEqual("1.4",o[1.4].name,"constants are the given numeric literal");
            assert.areEqual("",a[3].name,"expressions are not evaluated, default to empty string since it lacks a variable name");
            assert.areEqual("4",a[4].name,"constants are the given numeric literal");
            assert.areEqual("5",a[5].name,"constants are the given numeric literal");
            assert.areEqual("",a[7].name,"expressions are not evaluated, default to empty string since it lacks a variable name");
       }
    },
    {
       name: "Strings With Brackets or Periods in them",
       body: function ()
       {
            var o = { "hello.friend" : function() {},
                      "[a" : function() {},
                      "]" : function() {},
                      "]a" : function() {}};
            assert.areEqual("hello.friend",o["hello.friend"].name,"the period is included in the name don't shorten");
            assert.areEqual("[a",o["[a"].name,"the bracket is included in the name don't shorten");
            assert.areEqual("]",o["]"].name,"the bracket is included in the name don't shorten");
            assert.areEqual("]a",o["]a"].name,"the bracket is included in the name don't shorten");

            var o = { "a[" : function() {} };
            assert.areEqual("a[",o["a["].name,"the bracket is included in the name don't shorten");
            var o = { ["a["] : function() {} };
            assert.areEqual("a[",o["a["].name,"computed property names use a different code path");
            var a = [];
            a["["] = function() {};
            a["]"] = function() {};
            assert.areEqual("",a["["].name);
            assert.areEqual("",a["]"].name);
            a["hello.buddy"] = function() {};
            assert.areEqual("",a["hello.buddy"].name);

            class ClassTest
            {
              static [".f"]() {}
              static ["f."]() {}
              static ["f["]() {}
              static ["f]"]() {}
              static ["]]f]]"]() {}
              static ["[f"]() {}
              static ["[[[[[f"]() {}
              static ["]f"]() {}
            }
            assert.areEqual("f.", ClassTest["f."].name);
            assert.areEqual(".f", ClassTest[".f"].name);
            assert.areEqual("f[", ClassTest["f["].name);
            assert.areEqual("f]", ClassTest["f]"].name);
            assert.areEqual("]]f]]", ClassTest["]]f]]"].name);
            assert.areEqual("[f", ClassTest["[f"].name);
            assert.areEqual("[[[[[f", ClassTest["[[[[[f"].name);
            assert.areEqual("]f", ClassTest["]f"].name);

            var a = {"\0" : { f : function() {}, c : class {} }}
            assert.areEqual("f", a["\0"].f.name);
            assert.areEqual("c", a["\0"].c.name);
       }
    },
    {
       name: "Class.name",
       body: function ()
       {
            var a = class foo {}
            assert.areEqual("foo",a.name,"should pick the class name not the assignment name");
            class ClassDecl {} // constructor is "ClassDecl"
            var c = class { method(){}}
            var b = new c();
            assert.areEqual("ClassDecl",ClassDecl.name,"name should be ClassDecl");
            assert.areEqual("c",c.name,"class name should be c");
            assert.areEqual("method",b.method.name,"c is a constructor, b is an instance so method is accessible on b");
            ClassDecl.name = "foo";
            assert.areEqual("ClassDecl",ClassDecl.name, "function names are read only");
            assert.areEqual("ClassExpr",(class ClassExpr {}).name,"class expression case should still print a name");

            var classFoo  = class
            {
                constructor(){}                 // "classFoo "
                static func(){}                 // "func"
                method(){}                      // "method"
                get getter(){}                  // "get getter"
                set setter(v){}                 // "set setter"
            };

            class classFoo2
            {
                constructor(){}
            }
            assert.areEqual("Function",classFoo2.constructor.name, "classFoo2.constructor.name === 'Function'");
            assert.areEqual("classFoo2",classFoo2.prototype.constructor.name, "confirm that the prototype constructors name is the class name");

            var oGet = Object.getOwnPropertyDescriptor(classFoo.prototype,"getter");
            var oSet = Object.getOwnPropertyDescriptor(classFoo.prototype,"setter");
            assert.areEqual("Function",classFoo.constructor.name, "classFoo.constructor.name === 'Function'");
            assert.areEqual("classFoo",classFoo.name, "Name of the class should be classFoo");
            assert.areEqual("classFoo",classFoo.prototype.constructor.name, "Name of the constructor should be the class name");
            assert.areEqual("func",classFoo.func.name, "Name should just be func");
            assert.areEqual("method",classFoo.prototype.method.name, "Name should be method");
            assert.areEqual("get getter",oGet.get.name,"Accessors getter should be prefixed with get");
            assert.areEqual("set setter",oSet.set.name, "Accessors setter should be prefixed with set");

            var instanceFoo = new classFoo();
            var instanceFoo2 = new classFoo2();
            assert.areEqual("classFoo2",instanceFoo2.constructor.name, "instance constructor should be class name");
            assert.areEqual("classFoo",instanceFoo.constructor.name, "instance constructor should be class name");
            assert.areEqual("method",instanceFoo.method.name, "instance should have function name method");
       }
    },
    {
       name: "Generator functions",
       body: function ()
       {
            function* gf() { }
            var gfe = function* () { }
            var obj = { gfm : function* () { } }
            assert.areEqual("gf",gf.name, "Generator Declaration");
            assert.areEqual("gfe",gfe.name, "Generator Expression");
            assert.areEqual("gfm",obj.gfm.name, "Generator Method");
            var GeneratorFunction = Object.getPrototypeOf(gf).constructor;
            assert.areEqual("anonymous",(new GeneratorFunction ).name,"Should be anonymous");
       }
    },
    {
       name: "function inside objects",
       body: function ()
       {
          let obj =
            {
                prop: () => {},
                noOverride: function named(){},
                "literal": function(){},
                5: () => {}
            };

            assert.areEqual("prop",obj.prop.name,"lambda function name is assigned to prop");
            assert.areEqual("named",obj.noOverride.name, "noOverride inherits name from function named");

            assert.areEqual("literal",obj.literal.name, "string function definitions are valid");
            assert.areEqual("5",obj["5"].name, "numeral function definitions are valid");

            var obj2 =
            {
                method(){}
            }
            obj2.property = function(){};

            assert.areEqual("method",obj2.method.name, "tests functions without the function reserved word");
            assert.areEqual("",obj2.property.name, "test to make sure defining a property outside of a function is empty string");
        }
    },
    {
       name: "function.name's are read only",
       body: function ()
       {
            var object =
            {
                f: function() {}
            };
            assert.areEqual("f",object.f.name, "function name is f");
            object.f.name = "foo";
            assert.areEqual("f",object.f.name, "function names are read only");

        }
    },
    {
       name: "function.name test functions named get\set don't get confused for accessors",
       body: function ()
       {
            var foo = {
                value : 0,
                get : function() { return value;},
                set : function (val) {value = val}
            }
            assert.areEqual("get", foo.get.name, "name should be get");
            assert.areEqual("set", foo.set.name, "name should be set");

            var obj3 = { get : function foo () { },
                        set : function bar (v) { }};
            //like the var a = function foo() {} case a inherits foo's name
            assert.areEqual("foo",obj3.get.name, "should inherited name foo");
            assert.areEqual("bar",obj3.set.name, "should inherited name bar");
       }
    },
    {
       name: "function.name accessor test",
       body: function ()
       {
            var oRuntime = Object.getOwnPropertyDescriptor(Map.prototype,"size");
            assert.areEqual("get size",oRuntime.get.name, "Map.prototype.size is a getter");
            assert.areEqual(undefined,oRuntime.set, "Map.prototype.size does not have a setter");
            // Single Property descriptor
            var o = { get foo(){}, set foo(x){} };
            var descriptor = Object.getOwnPropertyDescriptor(o, "foo");
            assert.areEqual("get foo", descriptor.get.name, "get accessors on function foo are prefixed with get");
            assert.areEqual("set foo", descriptor.set.name, "set accessors on function foo are prefixed with set");

            let obj =
            {
                get getter(){ return 0;},
                set setter(v){}
            };

            // Multiple property descriptors
            var oGet = Object.getOwnPropertyDescriptor(obj,"getter")
            var oSet = Object.getOwnPropertyDescriptor(obj,"setter");
            assert.areEqual("get getter",oGet.get.name, "get accessors functions are prefixed with get");
            assert.areEqual("set setter",oSet.set.name, "set accessors functions are prefixed with set");
       }
    },

    {
       name: "function.name Property existence test",
       body: function ()
       {
       	function foo(){}

        assert.areEqual(true,testGetProperyNames(foo,true),"Properties on foo");
        assert.areEqual(0,Object.keys(foo).length,"no enumerable properties in function instance foo");
        Object.defineProperty(foo,"name",{writable: false,enumerable: true,configurable: true});
        var o  = Object.getOwnPropertyDescriptor(foo,"name");
        assert.areEqual(true,  o.enumerable,   "Name is redefined to enumerable");
        for (i in foo)
        {
            assert.areEqual("name",i,"Name should be the only enumerable property");
        }
        assert.areEqual(1,Object.keys(foo).length,"name is now an enumerated property");
        assert.areEqual("name",Object.keys(foo).toString(),"Name should be the only enumerable property");
       }
    },

    {
       name: "function.name delete test",
       body: function ()
       {
            function foo(){}

            assert.areEqual(true,testGetProperyNames(foo,true), "Properties on foo");
            delete foo.name;
            assert.areEqual(true,testGetProperyNames(foo,false),"Properties on foo");
       }
    },

    {
       name: "built-in function.name",
       body: function ()
       {
            assert.areEqual("slice",[].slice.name,"name should be slice");
            [].slice.name = "bar";
            assert.areEqual("slice",[].slice.name, "function names are read only");
       }
    },

   {
       name: "built-in function.name delete test",
       body: function ()
       {
            assert.areEqual(true,testGetProperyNames([].splice,true),"Properties on foo");
            delete [].splice.name;
            assert.areEqual(true,testGetProperyNames([].splice,false),"Properties on foo");
       }
    },
    {
       name: "anonymous function special cases",
       body: function ()
       {
           assert.areEqual("anonymous",(new Function).name,"Should be anonymous");
           assert.areEqual("",Function.prototype.name,"19.2.3 The value of the name property of the Function prototype object is the empty String.");

       }
    },
    {
       name: "nested function assignment names",
       body: function ()
       {
           var obj =
           {
                x : function(){},
                y : () => {},
                z : class {}
           };

           assert.areEqual("x",obj.x.name,"x defined in obj Should be x");
           assert.areEqual("y",obj.y.name,"y defined in obj Should be y");
           assert.areEqual("z",obj.z.name,"z defined in obj Should be z");

           var obj =
           {
                innerObj :
                {
                    x : function(){},
                    y : () => {},
                    z : class {}
                }
           };

           assert.areEqual("x",obj.innerObj.x.name,"Should be x");
           assert.areEqual("y",obj.innerObj.y.name,"Should be y");
           assert.areEqual("z",obj.innerObj.z.name,"Should be z");

           var obj = {};
           obj.x = function(){};
           obj.y = () => {};
           obj.z = class {};

           assert.areEqual("",obj.x.name,"Should be ''");
           assert.areEqual("",obj.y.name,"Should be ''");
           assert.areEqual("",obj.z.name,"Should be ''");

           var obj = {innerObj : {}};

           obj.innerObj.x = function(){};
           obj.innerObj.y = () => {};
           obj.innerObj.z = class {};

           assert.areEqual("",obj.innerObj.x.name,"Should be ''");
           assert.areEqual("",obj.innerObj.y.name,"Should be ''");
           assert.areEqual("",obj.innerObj.z.name,"Should be ''");

       }
    },
    {
       name: "Check the Class of an Object",
       body: function ()
       {
            function foo(){}
            var f = new foo();

            assert.areEqual("foo",f.constructor.name,"The constructor is foo");
            assert.areEqual(undefined,f.name,"f is an instance of the function foo, the name exists only on the constructor");

       }
    },
    {
       name: "Attributes test",
       body: function ()
       {
            function foo(){}
            assert.areEqual(true, foo.hasOwnProperty("name"), "foo should have a name property");
            var o = Object.getOwnPropertyDescriptor(foo,"name");

            assert.areEqual(false, o.writable,     "Name is not writable");
            assert.areEqual(false, o.enumerable,   "Name is not enumerable");
            assert.areEqual(true,  o.configurable, "Name is configurable");
            assert.areEqual("foo", o.value,        "Names value should be foo");

       }
    },
    {
       name: "Symbol names",
       body: function ()
       {
            var sym1 = Symbol("foo");
            var sym2 = Symbol("bar");
            var sym3 = Symbol("baz");
            var sym4 = Symbol();
            var o = {[Symbol.toPrimitive]: function() {},
                     [sym1] : function() {},
                     [sym3] : function bear() {},
                     [sym4] : function() {},
                    }
            o[Symbol.unscopables] = function(){}
            o[sym2] = function() {}
            assert.areEqual("[foo]", o[sym1].name, "9.2.11.4 SetFunctionName: If Type(name) is Symbol, then let name be the concatenation of \"[\", description, and \"]\"");
            assert.areEqual("[Symbol.toPrimitive]",o[Symbol.toPrimitive].name,
            "9.2.11.4 SetFunctionName: If Type(name) is Symbol, then let name be the concatenation of \"[\", description, and \"]\"");
            assert.areEqual("", o[Symbol.unscopables].name, "computed property names are not bound to index yet and builtin symbols are not bound to a name so they are empty strings");
            assert.areEqual("sym2", o[sym2].name, "computed property names are not bound to index yet");
            assert.areEqual("bear", o[sym3].name, "if the function already has a name don't overwrite it");
            assert.areEqual("", o[sym4].name, "empty symbols have empty string as a name");
       }
    },
    {
       name: "Redefine Attributes test",
       body: function ()
       {
            function foo(){}

            Object.defineProperty(foo,"name",{writable: true,enumerable: true,configurable: false});
            foo.name = "bar";
            var o  = Object.getOwnPropertyDescriptor(foo,"name");

            assert.areEqual(true,  o.writable,     "Name is redefined to writable");
            assert.areEqual(true,  o.enumerable,   "Name is redefined to enumerable");
            assert.areEqual(false, o.configurable, "Name redefined not configurable");
            assert.areEqual("bar", o.value,        "Names value should be bar");
            assert.areEqual("bar",foo.name,"foo renamed to bar");

       }
    },
    {
        name: "strings with null terminators sprinkled in",
        body: function()
        {
            var str = "hello\0 foo";
            var a = [];
            a["hello\0 foo"] = function() {};
            var o = {[str] : function() {}, ["h\0h"] : function() {}}
            var b = {}
            b["hello\0 foo"] = function() {}
            var c = { "hello\0 foo" : function() {} }
            assert.areEqual(str, o[str].name);
            assert.areEqual("h\0h", o["h\0h"].name);
            assert.areEqual("hello\0 foo", a["hello\0 foo"].name);
            assert.areEqual("hello\0 foo", b["hello\0 foo"].name);
            assert.areEqual("hello\0 foo", c["hello\0 foo"].name);
            var d = { "goo.\0d" : function() {} }
            var e = { "g\0oo\0.d" : function() {} }
            var f = { "fo\0o" : class {} }
            assert.areEqual("goo.\0d",  d["goo.\0d"].name);
            assert.areEqual("g\0oo\0.d", e["g\0oo\0.d"].name);
            assert.areEqual("fo\0o", f["fo\0o"].name);

        }
    },
    {
        name: "Function Bind",
        body: function()
        {
            function add(x, y)
            {
                return x+y;
            }
            var AddZer0  = add.bind(null,0 /* x */);
            var Add2Nums = add.bind();

            assert.areEqual("bound add",AddZer0.name, "AddZer0  needs a bound prefix on add");
            assert.areEqual("bound add",Add2Nums.name,"Add2Nums needs a bound prefix on add");
        }
    },
    {
        name: "Bug 1642987 & 1242667",
        body: function()
        {
            e = ''['u3 = undefined'] = function () {}
            assert.areEqual('', e.name, "Bug 1642987: we should not AV if we can't shorten the name") ;
            f = ''['[f]o'] = function () {};
            assert.areEqual('', f.name, "Bug 1242667: We need to wrap strings in Brackets") ;
        }
    },
    {
        name: "Bug 2302197",
        body: function()
        {
            var b = {};
            var c = b.x = function Ctor() {}
            var a = new c();
            assert.areEqual('Ctor', b.x.name, "confirm IsNameIdentifierRef does not override IsNamedFunctionExpression");
            assert.areEqual('Ctor', c.name, "confirm IsNameIdentifierRef does not override IsNamedFunctionExpression");
            assert.areEqual('Ctor', a.constructor.name, "confirm IsNameIdentifierRef does not override IsNamedFunctionExpression");
        }
    },
    {
        name: "Bug 3941893 & Bug 4153027",
        body: function()
        {
            class B {
                static ["n"+"a"+"me"]() {}
            }
            assert.areEqual("function", typeof B.name, "Function 'name' attribute should not be inferred in presence of static computed 'name' method");
            assert.areEqual("name", B.name.name, "Make sure the name is correct");
            var o = {
                ['A'+'B'] : class extends B {},
                ['C'+'B'] : class {},

                ['a'+'b'] : class extends B {foo_ab(){}},
                ['c'+'b'] : class {foo_cb(){}},

                ['d'+'f'] : class extends B {static foo_df(){}},
                ['f'+'d'] : class {static foo_fd(){}}
            }

            assert.areEqual("AB", o.AB.name, "confirm empty super class is properly assigned to a computed property");
            assert.areEqual("CB", o.CB.name, "confirm empty base class is properly assigned to a computed property");

            assert.areEqual("ab", o.ab.name, "confirm filled super class is properly assigned to a computed property");
            assert.areEqual("cb", o.cb.name, "confirm filled base class is properly assigned to a computed property");

            assert.areEqual("df", o.df.name, "confirm static filled super class is properly assigned to a computed property");
            assert.areEqual("fd", o.fd.name, "confirm static filled base class is properly assigned to a computed property");
        }
    },
    {
        name: "Bug 3713125",
        body: function()
        {
            var target = Object.defineProperty(function() {}, 'name', { value: 'target' });
            assert.areEqual('bound bound target', target.bind().bind().name, "confirm bound is appended to the front twice");
            d = Object.getOwnPropertyDescriptor(target.bind().bind(), 'name')
            assert.areEqual(false, d.writable);
            assert.areEqual(false, d.enumerable);
            assert.areEqual(true,  d.configurable);
        }
    },
    {
        name: "Bug 3713014",
        body: function()
        {
            var namedSym = Symbol('test');
            var anonSym = Symbol();
            class A {
                set [namedSym](_) {}
                get [namedSym]() {}
            }
            var classASymbolSet = Object.getOwnPropertyDescriptor(A.prototype, namedSym).set;
            var classASymbolGet = Object.getOwnPropertyDescriptor(A.prototype, namedSym).get;

            assert.areEqual("get [test]",classASymbolGet.name, " should not throw b\c of toString call on symbol");
            assert.areEqual("set [test]",classASymbolSet.name, " should not throw b\c of toString call on symbol");

            class B {
                set [anonSym](_) {}
                get [anonSym]() {}
            }
            var classBSymbolSet = Object.getOwnPropertyDescriptor(B.prototype, anonSym).set;
            var classBSymbolGet = Object.getOwnPropertyDescriptor(B.prototype, anonSym).get;

            assert.areEqual("get ",classBSymbolGet.name, " should not throw b\c of toString call on symbol");
            assert.areEqual("set ",classBSymbolSet.name, " should not throw b\c of toString call on symbol");

        }
    },
    {
        name: "OS Bug 3933663 Classes Should not un-defer a class constructor before we store the computed property",
        body: function()
        {
            var first  = 'a';
            var second = 'b';
            var third  = Symbol('c');
            var fourth = Symbol();
            var o;
            o = {
            [first]: class {},
            [second]: class {},
            [third]: class {},
            [fourth]: class {}
            };
            assert.areEqual("a", o[first].name , "confirm class constructor names are the computed property value a");
            assert.areEqual("b", o[second].name, "confirm class constructor names are the computed property value b");
            assert.areEqual("[c]", o[third].name , "confirm class constructor names are the computed property value [c]");
            assert.areEqual("",  o[fourth].name, "confirm class constructor names are the computed property value empty string");
        }
    },
    {
        name: "fix for toString override",
        body: function()
        {
            var b="barzee";
            class foo {
                [b] () {}
            };
            var inst=new foo();
            inst[b].toString();
            assert.areEqual("barzee",inst[b].name);
        }
    }

];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
