//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var diff_clock_time = null;

for(var i=0; i<3; i++)
{
        doClock();
}
       
function doClock(firsttime)
{
    var clock = new Date();
    
    if (firsttime == true)
    {
        var real_clock = new Date();
        diff_clock_time = clock * 1 - real_clock * 1;        
    }

    // Update difference between server time and client time
    clock.setTime(clock * 1 - diff_clock_time * 1);
    var hours = clock.getHours();
}

