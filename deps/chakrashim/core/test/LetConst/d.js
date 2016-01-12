//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function testcase() {

  function foo() {
    
    const x = {a: 1, b: 2, c: 3, };  //extra ,
    return ( x.a === 1 && x.b === 2 && x.c === 3 );
  }
  try {
    return foo(); 
  }
  catch (e) {
    return false;
  }
}


WScript.Echo(testcase());
