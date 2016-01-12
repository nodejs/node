//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//Note: see function  ArraySpliceHelper of JavascriptArray.cpp

if (this.WScript && this.WScript.LoadScriptFile) { // Check for running in ch
    this.WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
}

var tests = [
    {
        name: "Calling Array.prototype.slice()",
        body: function ()
        {
            var a=[1,2,3,4,5];
            var b=Array.prototype.slice.call(a,1,3);
            assert.areEqual([2,3], b, "Incorrect result from Array.prototype.slice()");
        }
    },
    {
        name: "Calling Array.prototype.push()",
        body: function ()
        {
            var a=[1,2];
            Array.prototype.push.call(a,1);
            assert.areEqual([1,2,1], a, "Incorrect result from Array.prototype.push()");
        }
    },
    {
        name: "Calling Array.isArray()",
        body: function ()
        {
            var a=[1,2,3,4,5,6,7];
            assert.areEqual(true, Array.isArray(a), "Incorrect result from Array.isArray()");
        }
    },
    {
        name: "Calling Array.prototype.unshift()",
        body: function ()
        {
            var a=[2,1,3,4];
            Array.prototype.unshift.call(a,0);
            assert.areEqual([0,2,1,3,4], a, "Incorrect result from Array.prototype.unshift()");
        }
    },
    {
        name: "Calling Array.prototype.shift()",
        body: function ()
        {
            var a=[1,2,3,4];
            var c=Array.prototype.shift.call(a);
            assert.areEqual([2,3,4], a, "Incorrect result from Array.prototype.shift()");
            assert.areEqual(1, c, "Incorrect result from Array.prototype.shift()");
        }
    },
    {
        name: "Calling Array.prototype.entries()",
        body: function ()
        {
            var a=[1,2,3,4];
            var c=Array.prototype.entries.call(a);
            for (var e of c)
            {
                print(e);
            }
        }
    },
    {
        name: "Calling Array.prototype.keys()",
        body: function ()
        {
            var a=[1,2,3,4];
            var c=Array.prototype.keys.call(a);
            for (var e of c)
            {
                print(e);
            }
        }
    },
    {
        name: "Calling Array.prototype.reverse()",
        body: function ()
        {
            var a=[1,2,3,4];
            Array.prototype.reverse.call(a);
            assert.areEqual([4,3,2,1], a, "Incorrect result from Array.prototype.reverse()");
        }
    },
    {
        name: "Calling Object.prototype.toString()",
        body: function ()
        {
            var a=[1,2,3,4,5,6];
            var c=Object.prototype.toString.call(a);
            assert.areEqual("[object Array]", c, "Incorrect result from Object.prototype.toString()");
        }
    },
    {
        name: "OS3713376: Accessing COA through proxy",
        body: function ()
        {
            var p = new Proxy([0,0,0,0,0], {});
            p.length = 1;
            assert.areEqual('0', p.toString(), 'Setting length of an array through Proxy');

            var q = new Proxy([0,0,0,0,0], {});
            q[0] = 1;
            assert.areEqual('1,0,0,0,0', q.toString(), 'Setting array element through Proxy');
        }
    },
];
testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
