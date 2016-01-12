//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function test0(){ 
    //Snippets:stfldprototype.ecs
    function v710235()
    {
    }
    v710235.prototype = 1;
    var v710236 = new v710235();
    // Make sure this literal's type isn't shared with the one with the constructor above
    // as we would have the inline slot count locked for the literal
    var litObj4 = {prop0: 1, prop1: 1, prop2: 1, prop3: 1, prop4: 1};

};
test0(); 

test0();

WScript.Echo("PASS");
