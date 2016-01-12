//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
   {
       name: "String.prototype is a String",
       body: function ()
       {
            try
            {
                String.prototype.valueOf();
            }
            catch(e)
            {
                if (e instanceof TypeError && e.message === "String.prototype.valueOf: 'this' is not a String object") {
                    assert.isFalse(true,"String.prototype is not a generic object, it should be a String object")
                }
                assert.isFalse(true, "Investigate " + e);
            }
       }
   },
   {
       name: "Boolean.prototype is a Boolean",
       body: function ()
       {
            try
            {
                Boolean.prototype.valueOf();
            }
            catch(e)
            {
                if (e instanceof TypeError && e.message === "Boolean.prototype.valueOf: 'this' is not a Boolean object") {
                    assert.isFalse(true,"Boolean.prototype is not a generic object, it should be a Boolean object")
                }
                assert.isFalse(true, "Investigate " + e);
            }
       }
   },
   {
       name: "Number.prototype is a Number",
       body: function ()
       {
            try
            {
                Number.prototype.valueOf();
            }
            catch(e)
            {
                if (e instanceof TypeError && e.message === "Number.prototype.valueOf: 'this' is not a Number object") {
                    assert.isFalse(true,"Number.prototype is not a generic object, it should be a Number object")
                }
                assert.isFalse(true, "Investigate " + e);
            }
       }
   }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
