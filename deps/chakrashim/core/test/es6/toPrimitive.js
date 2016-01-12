//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
       name: "Number Object Test",
       body: function ()
       {
            var a = new Number(1);
            a[Symbol.toPrimitive] = function(hint)
            {
                if(hint == "string")
                {
                    return "a";
                }
                else
                {
                    return 10;
                }
            }

            assert.isTrue(10 == a,"should now call @@toprimitive and return 10");
            assert.isTrue(11 == a+1,"should now call @@toprimitive with default hint and return 10 then add 1 = 11");
            assert.areEqual("a",String(a),"should now call @@toPrimitive and return a");
        }
    },
   {
       name: "Symbol Tests",
       body: function ()
       {

            var o  = Object.getOwnPropertyDescriptor(Symbol,"toPrimitive");
            assert.isFalse(o.writable,     "Symbol @@toPrimitive is not writable");
            assert.isFalse(o.enumerable,   "Symbol @@toPrimitive is not enumerable");
            assert.isFalse(o.configurable, "Symbol @@toPrimitive is not configurable");


            assert.areEqual("[Symbol.toPrimitive]", Symbol.prototype[Symbol.toPrimitive].name, "string should be [Symbol.toPrimitive]");
            var o  = Object.getOwnPropertyDescriptor(Symbol.prototype,Symbol.toPrimitive);
            assert.isFalse(o.writable,     "Symbol @@toPrimitive is not writable");
            assert.isFalse(o.enumerable,   "Symbol @@toPrimitive is not enumerable");
            assert.isTrue(o.configurable,  "Symbol @@toPrimitive is configurable");
            assert.throws(function() { Symbol.prototype[Symbol.toPrimitive](); }, TypeError, "Symbol[@@toPrimitive] throws no matter the parameters", "Symbol[Symbol.toPrimitive]: invalid argument");
       }
    },
    {
       name: "Date Test",
       body: function ()
       {
            assert.areEqual("[Symbol.toPrimitive]", Date.prototype[Symbol.toPrimitive].name, "string should be [Symbol.toPrimitive]");
            var o  = Object.getOwnPropertyDescriptor(Date.prototype,Symbol.toPrimitive);
            assert.isFalse(o.writable,     "Date @@toPrimitive is not writable");
            assert.isFalse(o.enumerable,   "Date @@toPrimitive is not enumerable");
            assert.isTrue(o.configurable,  "Date @@toPrimitive is configurable");

            var d = new Date(2014,5,30,8, 30,45,2);
            assert.areEqual(d.toString(),d[Symbol.toPrimitive]("string"), "check that the string hint toPrimitive returns toString");
            assert.areEqual(d.toString(),d[Symbol.toPrimitive](),"check that default has the same behaviour as string hint");
            assert.areEqual(d.toString(),d[Symbol.toPrimitive]("default"), "check that default has the same behaviour as string hint");
            assert.areEqual(d.valueOf(),d[Symbol.toPrimitive]("number"),"check that the number hint toPrimitive returns valueOf");

            assert.throws(function() {d[Symbol.toPrimitive]("boolean")}, TypeError, "boolean is an invalid hint");
            assert.throws(function() {d[Symbol.toPrimitive]({})}, TypeError, "provided hint must be strings or they results in a type error");

            assert.areEqual(d.toString()+10,d+10,"addition provides no hint resulting default hint which is string");
            assert.areEqual(d.valueOf(), (new Number(d)).valueOf(),"conversion toNumber calls toPrimitive with hint number");

            delete Date.prototype[Symbol.toPrimitive];
            assert.isFalse(Date.prototype.hasOwnProperty(Symbol.toPrimitive),"Property is configurable, should be able to delete");
            assert.areEqual(d.toString()+10,d+10,"(fall back to OriginalToPrimitve) addition provides no hint resulting default hint which is string");
            assert.areEqual(d.valueOf(), (new Number(d)).valueOf(),"(fall back to OriginalToPrimitve) conversion toNumber calls toPrimitive with hint number");
       }
    },
    {
       name: "Object toPrimitve Test",
       body: function ()
       {
            var o = { toString : function () {return "o"}, valueOf : function() { return 0;}};

            o[Symbol.toPrimitive] = function(hint)
            {
                if("string" == hint)
                {
                    return this.toString()+" (hint String)";
                }
                else if("number" == hint)
                {
                    return this.valueOf()+2;
                }
                else
                {
                    return " (hint default) ";
                }
            }
            assert.areEqual(" (hint default) ", o[Symbol.toPrimitive](), "Test to check the string is properly being returned");
            assert.areEqual("o (hint String)",  o[Symbol.toPrimitive]("string"), "Test to check the string is properly being returned");
            assert.areEqual(2, o[Symbol.toPrimitive]("number"), "Test to check the integer is properly being returned");


           assert.areEqual(2,(new Number(o)).valueOf(),"toNumber should call toPrimitive which should invoke the user defined behaviour for @@toPrimitive with hint number"); // conversion toNumber -> toPrimitive(hint number)
           assert.areEqual("1 (hint default) 1",1+o+1,"add should call toPrimitive which should invoke the user defined behaviour for @@toPrimitive with no hint"); // add -> toPrimitive(hint none)
           assert.areEqual("o (hint String)",(new String(o)).toString(),"toString should call toPrimitive which should invoke the user defined behaviour for @@toPrimitive with hint string"); // conversion toString -> toPrimitive(hint string)
       }
    },
    {
       name: "Object toPrimitve must be Function else Throws typeError",
       body: function ()
       {
            var o = { toString : function () {return "o"}, valueOf : function() { return 0;}};

            o[Symbol.toPrimitive]  = {}; // can only be a  function else type error
            assert.throws(function() {var a = o+1;}, TypeError, "o[Symbol.toPrimitive] must be a function");

        }
    },
// In ScriptLangugageVersion6 the ActiveXObject constructor is removed and is unable to be used for this test. Disabling until different object type can be found
// that can be used instead.
//    {
//       name: "Object toPrimitve must return  ECMA Type else TypeError",
//       body: function ()
//       {
//            var o = { toString : function () {return "o"}, valueOf : function() { return 0;}};
//            var c = new ActiveXObject("Excel.Application");
//            o[Symbol.toPrimitive] = function(hint)
//            {
//                return c;
//            }
//            assert.throws(function() {var a = o+1;}, TypeError, "o[Symbol.toPrimitive] must return an ECMA language value");
//
//        }
//    },
    {
       name: "String Object Test",
       body: function ()
       {
            var a = new String("a");
            a[Symbol.toPrimitive] = function(hint)
            {
                if(hint == "string")
                {
                    return "var_a";
                }
                else
                {
                    return -1;
                }
            }
            assert.isTrue(-1 == a,"should now call @@toprimitive and return -1");
            assert.isTrue("var_a" == String(a),"should now call @@toprimitive and return var_a");
            assert.isTrue(0 == a+1,"should now call @@toprimitive and return -1+1 = 0");

            assert.isTrue("var_a1" == String(a)+1,"should now call @@toprimitive and return var_a1");
            assert.areEqual(-1,Number(a),"should now call @@toPrimitive and return a");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
