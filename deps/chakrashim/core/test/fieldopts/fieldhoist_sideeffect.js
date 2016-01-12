//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var o = {};

function Ctor() {};
Ctor.prototype.valueOf = function() { return o.x++; }

var c = new Ctor();

var test_add = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        n = o.x + c;
    }
    return n;
}

test_add.test_result1 = 4;

var test_add_assign = function() {
    for (var i = 0 ; i < 1 ; i++){
        var obj={ x:1.23, z:1 }
        for (var j = 0 ; j < 1 ; j++){
            obj.x += obj.z
        }
    }
    return obj.x;
}
test_add_assign.test_result1 = 2.23;

var test_sub = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        n = o.x - c;
    }
    return n;
}
test_sub.test_result1 = 0;

var test_mul = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        n = o.x * c;
    }
    return n;
}
test_mul.test_result1 = 4;

var test_div = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        n = o.x / c;
    }
    return n;
}
test_div.test_result1 = 1;

var test_mod = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        n = o.x % c;
    }
    return n;
}
test_mod.test_result1 = 0;

var test_neg = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        n = -c;
        n += o.x;
    }
    return n;
}
test_neg.test_result1 = 1;

var test_bitand = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        n += o.x & c;
    }
    return n;
}
test_bitand.test_result1 = 3;

var test_bitor = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        n += o.x | c;
    }
    return n;
}
test_bitor.test_result1 = 3;

var test_bitxor = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        n += o.x ^ c;
    }
    return n;
}
test_bitxor.test_result1 = 0;

var test_bitnot = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        n += o.x;
        n += ~c;
    }
    return n;
}
test_bitnot.test_result1 = -2;

var test_bitshiftleft = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        n += o.x << c;
    }
    return n;
}
test_bitshiftleft.test_result1 = 10;

var test_bitshiftright = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        n += (o.x << 10) >> c;
    }
    return n;
}
test_bitshiftright.test_result1 = 1024;

var test_unsignedbitshiftright = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        n += (-o.x << 10) >>> c;
    }
    return n;
}
test_unsignedbitshiftright.test_result1 = 3221224448;

var test_less = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        if (o.x < c)
        {
            n += o.x;
        }
        else
        {
            n -= o.x;
        }
    }
    return n;
}
test_less.test_result1 = -5;

var test_less_equal = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        if (o.x <= c)
        {
            n += o.x;
        }
        else
        {
            n -= o.x;
        }
    }
    return n;
}
test_less_equal.test_result1 = 5;

var test_greater = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        if (o.x > c)
        {
            n += o.x;
        }
        else
        {
            n -= o.x;
        }
    }
    return n;
}
test_greater.test_result1 = -5;

var test_greater_equal = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        if (o.x >= c)
        {
            n += o.x;
        }
        else
        {
            n -= o.x;
        }
    }
    return n;
}
test_greater_equal.test_result1 = 5;

var test_equal = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        if (o.x == c)
        {
            n += o.x;
        }
        else
        {
            n -= o.x;
        }
    }
    return n;
}
test_equal.test_result1 = 5;

var test_not_equal = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        if (o.x != c)
        {
            n += o.x;
        }
        else
        {
            n -= o.x;
        }
    }
    return n;
}
test_not_equal.test_result1 = -5;

var test_compare_less = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        var b = o.x < c;
        if (b)
        {
            n += o.x;
        }
        else
        {
            n -= o.x;
        }
    }
    return n;
}
test_compare_less.test_result1 = -5;

var test_compare_less_equal = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        var b = o.x <= c;
        if (b)
        {
            n += o.x;
        }
        else
        {
            n -= o.x;
        }
    }
    return n;
}
test_compare_less_equal.test_result1 = 5;

var test_compare_greater = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        var b = o.x > c;
        if (b)
        {
            n += o.x;
        }
        else
        {
            n -= o.x;
        }
    }
    return n;
}
test_compare_greater.test_result1 = -5;

var test_compare_greater_equal = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        var b = o.x >= c;
        if (b)
        {
            n += o.x;
        }
        else
        {
            n -= o.x;
        }
    }
    return n;
}
test_compare_greater_equal.test_result1 = 5;

var test_compare_equal = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        var b = (o.x == c);
        if (b)
        {
            n += o.x;
        }
        else
        {
            n -= o.x;
        }
    }
    return n;
}
test_compare_equal.test_result1 = 5;

var test_compare_not_equal = function(o, c)
{
    var n = 0;
    for (var i = 0; i < 2; i++)
    {
        var b = (o.x != c);
        if (b)
        {
            n += o.x;
        }
        else
        {
            n -= o.x;
        }
    }
    return n;
}
test_compare_not_equal.test_result1 = -5;

Object.defineProperty(this, "getme", {get: function() { WScript.Echo('no!')}});
(function() {
    // Try to hoist a property with a getter to verify that we can safely avoid executing the getter in the header.
    for (var i = 0; i < 10; i++) {
        if (this.undefined) {
            var g = getme;
            g.x;
        }
    }
})();

for (var test in this)
{
    if (typeof this[test]  == "function" && test != "Ctor" && this[test].test_result1 != undefined)
    {
        o.x = 1;
        var ret = this[test](o,c);
        if (ret == this[test].test_result1)
        {
            WScript.Echo("PASS: " + test);
        }
        else
        {
            WScript.Echo("FAIL: " + test + ": expected " + this[test].test_result1 + ", got " + ret);
        }
    }
}
