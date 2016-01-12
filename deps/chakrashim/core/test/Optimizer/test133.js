//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(x) {
    for(var i = 0; i < 2; ++i) {
        x ? o.length : 1;
        x ? 0 : 1;
    }
};
test0(1);
