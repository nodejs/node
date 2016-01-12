//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function areEqual(a,b)
{
    assert.areEqual(a.length,b.length);
    for(var i = 0;i < a.length; i++)
    {
        assert.areEqual(a[i],b[i]);
    }
}
function compareConcats(a,b)
{
    var c = a.concat(b);
    b[Symbol.isConcatSpreadable] = false;
    var d = a.concat(b);
    assert.areEqual(b,d[a.length],"Indexing d at a.length should return b");
    for(var i = 0;i < a.length; i++)
    {
        assert.areEqual(a[i],c[i],"confirm array a makes up the first half of array c");
        assert.areEqual(a[i],d[i],"confirm array a makes up the first half of array d");
    }
    for(var i = 0;i < b.length; i++)
    {
        assert.areEqual(b[i],c[a.length+i],"confirm array b makes up the second half of array c");
        assert.areEqual(b[i],d[a.length][i],"confirm array b makes up a sub array at d[a.length]");

    }
        assert.areEqual(a.length+1,d.length,"since we are not flattening the top level array grows only by 1");
        excludeLengthCheck = false;

    delete b[Symbol.isConcatSpreadable];
}
var tests = [
   {
       name: "nativeInt Arrays",
       body: function ()
       {
            var a = [1,2,3];
            var b = [4,5,6];
            compareConcats(a,b);
       }
    },
    {
       name: "nativefloat Arrays",
       body: function ()
       {
            var a = [1.1,2.2,3.3];
            var b = [4.4,5.5,6.6];
            compareConcats(a,b);
       }
    },
    {
       name: "Var Arrays",
       body: function ()
       {
            var a = ["a","b","c"];
            var b = ["d","e","f"];
            compareConcats(a,b);
       }
    },
    {
       name: "intermixed Arrays",
       body: function ()
       {
            var a = [1.1,"b",3];
            var b = [4,5.5,"f"];
            compareConcats(a,b);
       }
    },
    {
       name: "concating spreadable objects",
       body: function ()
       {
            var a = [1,2,3,4,5,6,7,8,9,10];
            var b = [1,2,3].concat(4, 5, { [Symbol.isConcatSpreadable]: true, 0: 6, 1: 7, 2: 8, "length" : 3 }, 9, 10);
            areEqual(a,b);
            // Test to confirm we Spread to nothing when length is not set
            var a = [1,2,3,4,5,9,10];
            var b = [1,2,3].concat(4, 5, { [Symbol.isConcatSpreadable]: true, 0: 6, 1: 7, 2: 8}, 9, 10);
            areEqual(a,b);
       }
    },
    {
       name: " concat with non-arrays",
       body: function ()
       {
            var a = [1.1,2.1,3.1];
            var b = 4.1;
            compareConcats(a,b);
            var a = [1,2,3];
            var b = 4;
            compareConcats(a,b);
            var a = [1,2,3];
            var b = "b";
            compareConcats(a,b);
            var a = [1,2,3];
            var b = true;
            compareConcats(a,b);
       }
    },
    {
       name: "override with constructors",
       body: function ()
       {
            var a = [1,2,3];
            var b = [4.4,5.5,6.6];
            var c = [Object, {}, undefined, Math.sin];
            for(var i = 0; i < c.length;i++)
            {
                a['constructor'] = c[i];
                compareConcats(a,b);
            }
            var o  = [];
            o.constructor = function()
            {
                var a = new Array(100);
                a[0] = 10;
                return a;
            }
            areEqual([1,2,3],o.concat([1,2,3]));
       }
    }
    ];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
