//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    for(var i = 0; i < 26; ++i) {
        if(i === 24) {
            try {
                for(var j = 0; j < 26; ++j) {
                    if(j === 25)
                        throw new Error();
                }
            } catch(ex) {
            }
        }
    }
}
test0();

function test1() {
    for(var i = 0; i < 26; ++i) {
        var n = i >= 24 ? 26 : 0;
        try {
            for(var j = 0; j < n; ++j) {
                if(j === 25)
                    throw new Error();
            }
        } catch(ex) {
        }
    }
}
test1();

WScript.Echo("pass");
