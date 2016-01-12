//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function print(x) { WScript.Echo(x+''); }

const f = 'const f';
let g = 'let g';

eval("if (false) { function f() { } }");
eval("if (true) { function g() { } }");


print(f);
print(g);

function h(global) { }
eval("function h(ineval) { }");

print(h);

{
    function i(globalblock) { }
    eval("function i(ineval) { }");
    print(i);
}
print(i);

function j(global) { }

{
    function j(globalblock) { }
    eval("function j(ineval) { }");
    print(j);
}
print(j);

