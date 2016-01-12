//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var NeedLoad = 'ValueX';
function runAnimation(obj) {
    
        switch (obj) {
       case NeedLoad:
            break;
        case 'ValueY':
            WScript.Echo('ValueY');
            break;
        case 'Blah':
            WScript.Echo('Blah');
            break;
        default:
            if (obj === 'ValueY') {
                WScript.Echo('Bug');
            }
            WScript.Echo('default');
            break;
        }
    
}

runAnimation('ValueX');
runAnimation('ValueY');