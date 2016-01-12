//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test(){
    var obj = {x:true}
    for (var i = 0 ; i < 10 ; i++){
        delete obj.x
    }
}
test()
