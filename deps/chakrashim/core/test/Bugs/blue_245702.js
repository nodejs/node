//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
function test() {
    var print = function (a) {
        WScript.Echo(a);
    };

    c = 1;
  if (shouldBailout) {
        delete this.c;
    }

    function shapeyConstructor() {
        'use strict';
        for (c in ['']) {
            print(c);
        }
    }
    shapeyConstructor();
}
try{
    test();
}
catch(e){WScript.Echo(e);}

shouldBailout = true;
try{
    test();
}
catch(e){WScript.Echo(e);}
