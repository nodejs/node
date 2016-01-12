//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//
// Test case that caught bug 19988
//
function test(x){
    switch(x){
        case "a":
            WScript.Echo("a");
            break;
        case "b":
            WScript.Echo("b");
            break;
        case "a":
            break;
        case "b":
            WScript.Echo("bb");
            break;
    }
}

test("a");
test("b");