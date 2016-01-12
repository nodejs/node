//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var foo = function(){}
function letTest() {
    let sc1 = 0;
    with({})
    {
        sc1 = foo;
        sc1();
    }
    this.method0.apply(this, arguments);
}

function constTest() {
    const sc1 = 0;
    with({})
    {
        sc1 = foo;
        sc1();
    }
    this.method0.apply(this, arguments);
}

function varTest() {
    with({})
    {
        var sc1 = foo;
        sc1();
    }
    this.method0.apply(this, arguments);
}

function TryFunction(f)
{
    try
    {
        f();
    }catch (e) {
        if (e instanceof TypeError) { // Unable to get property 'apply' of undefined or null reference (method0)
            return true;
        }
        if (e instanceof ReferenceError) { // Assignment to const
            return true;
        }
    }
}
if(TryFunction(letTest) && TryFunction(constTest) && TryFunction(varTest))
{
    print("Pass");
}
