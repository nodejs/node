//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//
// Ensure that large numbers of properties are processed correctly.
//

var obj=new Object();

for(x=0;x<5000;x++)
{
    if(x<1000)
    {
        // Example: "var y15=15"
        eval("var y"+x+"=" + x );

        if(!(eval("y"+x)===x))
            WScript.Echo("FAIL: y"+x+" == " + eval("y"+x) + ".  Expected: " + x);

    }
    else
    {
        // Example: "obj['o57']=57"
        eval("obj['o"+x+"']="+x );
    }
}

// Here it is assumed that the enumeration of properties are accessed in the sequence they were created
// An example error message would look like:
// FAIL: obj[p1] == 23.  Expected: 47

var y=1000;

for(p1 in obj)
{
    if(obj[p1]!==y)
        WScript.Echo("FAIL: obj["+p1+"] == " + (obj[p1]) + ".  Expected: " + y);
    y++;
}
WScript.Echo("done");
