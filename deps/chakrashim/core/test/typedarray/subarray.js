//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// basic test of subarray with a begin index beyond the length of the typed array
WScript.Echo('var u8 = new Uint8Array(64); u8[63] = 45;');
var u8 = new Uint8Array(64);
u8[63] = 45;

WScript.Echo('u8[0] = ' + u8[0]);
WScript.Echo('u8[62] = ' + u8[62]);
WScript.Echo('u8[63] = ' + u8[63]);
WScript.Echo('u8[64] = ' + u8[64]);

u8 = u8.subarray(64); // 64 == length of the array, ie: u8[64] == undefined

WScript.Echo();
WScript.Echo('After u8.subarray(64).');

WScript.Echo('u8[0] = ' + u8[0]);
WScript.Echo('u8[62] = ' + u8[62]);
WScript.Echo('u8[63] = ' + u8[63]);
WScript.Echo('u8[64] = ' + u8[64]);

WScript.Echo();

// more exhaustive test
var size = 64;
var u8 = new Uint8Array(size);
for(i = 0; i < size; i++) {
    u8[i] = i;
}
for(i = ((size*-1)*2); i <= (size*2); i++) {
    var u9 = u8.subarray(i);

    var u9str = '';
    for(j = 0; j < u9.length; j++) {
        if(u9str != '') {
            u9str += ', ';
        }
        u9str += u9[j];
    }

    WScript.Echo('u8.subarray(' + i + ') = (' + u9.length + ') [' + u9str + ']');
}
