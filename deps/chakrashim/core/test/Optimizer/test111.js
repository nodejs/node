//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
var func0 = function(argArr1) {
    for(var __loopvar2 = 0; __loopvar2 < 1 && argArr1[(shouldBailout ? argArr1[0] : 0) ? 0 : 0]; __loopvar2++) {
        var __loopvar3 = 0;
        do {
            __loopvar3++;
        } while(argArr1[(shouldBailout ? argArr1[0] : 0) ? 0 : 0] && __loopvar3 < 1);
    }
    return { xyz: function() { } };
};
var ary = new Array();
var intary = [4];

func0(ary);
func0(ary);
func0(intary);

WScript.Echo("pass");
