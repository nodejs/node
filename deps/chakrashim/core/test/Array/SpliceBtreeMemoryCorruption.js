//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//Note: see function  ArraySpliceHelper of JavascriptArray.cpp

if (this.WScript && this.WScript.LoadScriptFile) { // Check for running in ch
    this.WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
}
var Debug = false;
function print(aVal)
{
    if(Debug)
    {
        WScript.Echo(aVal);
    }
}

var tests = [
    {
        name: "Float Splice Test",
        body: function ()
        {
            var FloatArr0 = [9.2];
            var b = -9;
            FloatArr0[8] = 1;
            var v5432 = FloatArr0.splice(b);          // By definition of Splice, this should make FloatArr0 empty and v5432 as the new
                                                      // array with deleted values

            print(v5432.valueOf());            // Works Fine
            assert.areEqual(v5432.toString(),"9.2,,,,,,,,1");
            var FloatArr0 = [];
            print(v5432.valueOf());            // BOOM, assert
            assert.areEqual(v5432.toString(),"9.2,,,,,,,,1");
        }
    },
    {
        name: "Empty Float Array Splice Test",
        body: function ()
        {
            var FloatArr1 = [9.2];
            var b = -9;
            FloatArr1[8] = 1;

            delete FloatArr1[0];

            var v5432 = FloatArr1.splice(b);          // By definition of Splice, this should make FloatArr0 empty and v5432 as the new
                                                      // array with deleted values

            print(v5432.valueOf());            // Works Fine
            assert.areEqual(v5432.toString(),",,,,,,,,1");
            var FloatArr1 = [];
            print(v5432.valueOf());            // BOOM, assert
            assert.areEqual(v5432.toString(),",,,,,,,,1");

        }
    },
    {
        name: "Int Splice Test",
        body: function ()
        {
            var IntArr0 = [9];
            var b = -9;
            IntArr0[8] = 1;
            var intDelArr = IntArr0.splice(b);      // By definition of Splice, this should make IntArr0 empty and intDelArr as the new
                                                   // array with deleted values

            print(intDelArr.valueOf());            // Works Fine
            assert.areEqual(intDelArr.toString(),"9,,,,,,,,1");
            var IntArr0 = [];
            print(intDelArr.valueOf());            // BOOM, assert
            assert.areEqual(intDelArr.toString(),"9,,,,,,,,1");
        }
    },
    {
        name: "Var Splice Test",
        body: function ()
        {
            var StringArr0 = ["hello"];
            var b = -9;
            StringArr0[8] = "hi";
            var strDelArr = StringArr0.splice(b);   // By definition of Splice, this should make StringArr0 empty and strDelArr as the new
                                                    // array with deleted values

            print(strDelArr.valueOf());             // Works Fine
            assert.areEqual(strDelArr.toString(),"hello,,,,,,,,hi");
            var StringArr0 = ["bar"];
            print(strDelArr.valueOf());             // BOOM, assert
            assert.areEqual(strDelArr.toString(),"hello,,,,,,,,hi");
        }
    }];
testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });