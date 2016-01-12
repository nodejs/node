//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function print(x) { WScript.Echo(x+''); }

function filter(name) {
    return /^[a-z]$/.test(name) || /^shadow_(let|const)$/.test(name);
}

print('\n==== Basic let and const variables at global scope ====\n');
// Since the let/const globals are implmemented as special properties
// in the [Simple]DictionaryTypeHandler, try out the other types of
// global properties to sanity check that they are still property-
// like (on global object, enumerable)

var      a = 'global var a';
         b = 'global undecl b';
let      c = 'global let c';
const    d = 'global const d';
function e () { }

print('\nNaked references\n');

print(a);
print(b);
print(c);
print(d);
print(e);

print('\nthis. references\n');

print(this.a);
print(this.b);
print(this.c);
print(this.d);
print(this.e);

print('\nfor-in enumeration of this\n');

for (let p in this)
{
    if (filter(p))
    {
        print(p + ': ' + this[p]);
    }
}

