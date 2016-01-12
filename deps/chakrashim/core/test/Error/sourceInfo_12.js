//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//
// Test uncaught exception source info
//
function dummy() {
    // do nothing
}

dummy();
var obj = {
    apply: Function.prototype.apply
};
obj.apply(); //12,1
dummy();
