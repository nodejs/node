//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function print(x) { WScript.Echo(x+""); }

let arguments = 'global let arguments';
print(arguments);

function testLetBlockScope() {
    {
        let arguments = 'let arguments block scoped';
        print(arguments);
    }
    print(arguments);
}
testLetBlockScope();

function testConstBlockScope() {
    {
        const arguments = 'const arguments block scoped';
        print(arguments);
    }
    print(arguments);
}
testConstBlockScope();

eval("let arguments = 'eval global let arguments'; print(arguments);");
eval("const arguments = 'eval global const arguments'; print(arguments);");


function testLetFunctionScope() {
    let arguments = 'let arguments function scope';
    print(arguments);
}
testLetFunctionScope();

function testConstFunctionScope() {
    const arguments = 'const arguments function scope';
    print(arguments);
}
testConstFunctionScope();

// OS 206284
function test() {
  (function() { /*sStart*/ ;
  {
     let veymqa = arguments;
     for (let gvvmwv = 0, arguments; gvvmwv < 12; ++gvvmwv) {}
  };; /*sEnd*/
  })();
}
test();



