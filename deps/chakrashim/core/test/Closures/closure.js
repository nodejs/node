//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function producer() {
   var x=3;
   var z=function() {
      WScript.Echo(x);
   }
   return z;
}

function consumer(f) {
    f();
}

var clo=producer();
consumer(clo);
