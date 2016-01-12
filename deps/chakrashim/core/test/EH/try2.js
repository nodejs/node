//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// basic try/catch testcases, including functions

function print(x)
{
        WScript.Echo(x);
}

function f(n, str)
{
        try
        {
                if(n == 0)
                        throw str;
                else
                        f(n-1, str);
        }
        finally
        {
                print("f(" + n + "): " + str);
        }
}

try
{
        f(10, "test 1");
}
catch(a)
{
        print(a);
}

try {
    throw "Hello";
}
catch (e)
{
    with({}) { print(e); }
}

var d = {toISOString:1,toJSON:Date.prototype.toJSON};
try {
        d.toJSON()
}
catch(e)
{
    with({})
    {
        print(e);
    }
}

var a = "global";
try
{ 
  throw "abc";
}
catch(e)
{
  eval("var a = 'catch-local';")
}
print(a);

