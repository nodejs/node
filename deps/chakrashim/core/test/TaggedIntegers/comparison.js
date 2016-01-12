//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Tagged Integers Test Plan, testcase #1
//
// Tests integer comparisons, with special attention to values interesting to
// tagged integers.

function check(cond,str)
{
    if(!cond)
    {
        WScript.Echo("FAIL: " + str);
    }
}

// BEGIN INTEGER DEFINITIONS
var n = 573;
var m = 572;
var t = new Object;

t.zero1 = 0;
t.zero2 = 0x0;
t.zero3 = new Number(0);
t.zero4 = m - m;

t.one1 = 1
t.one2 = 0x1;
t.one3 = new Number(1);
t.one4 = n - m;

t.two1 = 2;
t.two2 = 0x2;
t.two3 = new Number(2);
t.two4 = 2*(n-m);

t.negone1 = -1
t.negone2 = -0x1;
t.negone3 = -new Number(1);
t.negone4 = m-n;

t.negtwo1 = -2;
t.negtwo2 = -0x2;
t.negtwo3 = -new Number(2);
t.negtwo4 = 2*(m-n);

t.tagmax1 = 536870911;
t.tagmax2 = 0x1fffffff;
t.tagmax3 = new Number(536870911);
t.tagmax4 = 936947*n+280;

t.tagmin1 = -536870912;
t.tagmin2 = -0x20000000;
t.tagmin3 = new Number(-536870912);
t.tagmin4 = -(936947*n+280)-1;

t.tagmaxminusone1 = 536870910;
t.tagmaxminusone2 = 0x1ffffffe;
t.tagmaxminusone3 = new Number(536870910);
t.tagmaxminusone4 = 936947*n+280-1;

t.tagminplusone1 = -536870911;
t.tagminplusone2 = -0x1fffffff;
t.tagminplusone3 = new Number(-536870911);
t.tagminplusone4 = -(936947*n+280);

t.uintmax1 = 4294967295;
t.uintmax2 = 0xffffffff;
t.uintmax3 = new Number(4294967295);
t.uintmax4 = 7495579*n+528;

t.uintmaxminusone1 = 4294967294;
t.uintmaxminusone2 = 0xfffffffe;
t.uintmaxminusone3 = new Number(4294967294);
t.uintmaxminusone4 = 7495579*n+528-1;

t.intmax1 = 2147483647;
t.intmax2 = 0x7fffffff;
t.intmax3 = new Number(2147483647);
t.intmax4 = 2147483074+n;

t.intmaxminusone1 = 2147483646;
t.intmaxminusone2 = 0x7ffffffe;
t.intmaxminusone3 = new Number(2147483646);
t.intmaxminusone4 = 2147483073+n;

t.intmin1 = -2147483648;
t.intmin2 = -0x80000000;
t.intmin3 = new Number(-2147483648);
t.intmin4 = -2147483075-n;

t.intminplusone1 = -2147483647;
t.intminplusone2 = -0x7fffffff;
t.intminplusone3 = new Number(-2147483647);
t.intminplusone4 = -2147483074-n;

//
// These values are in increasing numeric value.  It's important to keep
// that ordering.
//
var testvals = [ "intmin",
         "intminplusone",
         "tagmin",
         "tagminplusone",
         "negtwo",
         "negone",
         "zero",
         "one",
         "two",
         "tagmaxminusone",
         "tagmax",
         "intmaxminusone",
         "intmax",
         "uintmaxminusone",
         "uintmax"
        ];

// END DEFINITIONS

// Test for equality
function check_equality()
{
    for(var idx = 0; idx < testvals.length; ++idx)
    {
        for(var i = 1; i <= 4; ++i)
        {
            for(var j = 1; j <= 4; ++j)
            {
                var l1 = testvals[idx]+i;
                var l2 = testvals[idx]+j;

                var result = false;

                if(t[l1] == t[l2])
                {
                    result = true;
                }
                check(result, t[l1] + " == " + t[l2]);
            }
        }
    }
}

// Test for inequality
function check_inequality()
{
    for(var idx1 = 0; idx1 < testvals.length; ++idx1)
    {

        for(var idx2 = 0; idx2 < testvals.length; ++idx2)
        {
            if(idx1 == idx2)
                continue;

            for(var i = 1; i <= 4; ++i)
            {
                for(var j = 1; j <= 4; ++j)
                {
                    var l1 = testvals[idx1]+i;
                    var l2 = testvals[idx2]+j;

                    var result = false;

                    if(t[l1] != t[l2])
                    {
                        result = true;
                    }
                    check(result, t[l1] + " != " + t[l2]);
                }
            }
        }
    }
}

// Test for greater/less than
function check_greaterless()
{
    for(var idx1 = 0; idx1 < testvals.length; ++idx1)
    {

        for(var idx2 = 0; idx2 < testvals.length; ++idx2)
        {
            if(idx1 == idx2)
                continue;

            for(var i = 1; i <= 4; ++i)
            {
                for(var j = 1; j <= 4; ++j)
                {
                    var l1 = testvals[idx1]+i;
                    var l2 = testvals[idx2]+j;
                    var result = false;

                    var str = "ERROR";

                    if(idx1 > idx2)
                    {
                        str = " > ";

                        if(t[l1] > t[l2])
                        {
                            result = true;
                        }
                    }
                    else if(idx1 < idx2)
                    {
                        str = " < ";

                        if(t[l1] < t[l2])
                        {
                            result = true;
                        }
                    }
                    else
                    {
                        WScript.Echo("should never get here!");
                        result = false;
                    }

                    check(result, t[l1] + str + t[l2]);
                }
            }
        }
    }
}

// Test for greaterequals/lessequals
function check_greaterlessequals()
{
    for(var idx1 = 0; idx1 < testvals.length; ++idx1)
    {

        for(var idx2 = 0; idx2 < testvals.length; ++idx2)
        {
            for(var i = 1; i <= 4; ++i)
            {
                for(var j = 1; j <= 4; ++j)
                {
                    var l1 = testvals[idx1]+i;
                    var l2 = testvals[idx2]+j;
                    var result = false;

                    var str = "ERROR";

                    if(idx1 > idx2)
                    {
                        str = " >= ";

                        if(t[l1] >= t[l2])
                        {
                            result = true;
                        }
                    }
                    else if(idx1 < idx2)
                    {
                        str = " <= ";

                        if(t[l1] <= t[l2])
                        {
                            result = true;
                        }
                    }
                    else if(idx1 == idx2)
                    {
                        if(i >= j)
                        {
                            if(t[l1] >= t[l2])
                            {
                                result = true;
                            }
                        }
                        else
                        {
                            if(t[l1] <= t[l2])
                            {
                                result = true;
                            }
                        }
                    }

                    check(result, t[l1] + str + t[l2]);
                }
            }
        }
    }
}

check_equality();
check_inequality();
check_greaterless();
check_greaterlessequals();

WScript.Echo("done");
