//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function Ctor()
{
    this.a = 400;
}
function test()
{
    // Test exercising variation of the mark tem objects
    var simple = {};
    simple.blah = 1;

    var literal = { a: 3 };

    var obj = new Ctor();
    
    var arrintlit = [ 1, 2 ];
    var arrfloatlit = [ 1.1 ];

    // this is not stack allocated yet. Need to modified loewring for NewScArray and inline build in constructors
    var typedarr = new Uint8Array(1);
    typedarr[0] = 2;

    var arr = [];
    arr[0] = 1;

    return simple.blah + literal.a + arr[0] + arr.length + typedarr[0] + typedarr.length + arrintlit[0] + obj.a + arrfloatlit[0];
}

WScript.Echo(test());
WScript.Echo(test());
