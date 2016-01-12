//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//Refer to bug Eze\125
//Not Functionality

function verify(get_actual,get_expected,testid,testdesc)
{
    if(get_actual!=get_expected)
        WScript.Echo(testid+":"+testdesc+"\t"+"failed");
}

//Test No 1 Checking Not for 0

verify(~0,-1,1,"\"Checking for 0\"");

//Test No 2

verify(~0xffffffff,0,2,"\"Checking result for 0xffffffff\"")

//Test no 3

verify(~~(0xffffffff),-1,3,"\"testing the not of 0xffffffff\"");

//Test No 4 Ensuring that Not retaggs its result for NaN

verify(~NaN,-1,4,"\"Checking result for NaN\"")

//Test No 5: Ensuring that Not retaggs its result for undefined

verify(~undefined,-1,5,"\"Checking for undefined\"")

//Test No 6: Ensuring that Not retaggs its result for Infinity

verify(~Infinity,-1,6,"\"Checking result for Infinity\"")

//Test No 7: Ensuring that Not retaggs its result for -Infinity

verify(~(-Infinity),-1,7,"\"Checking result for -Infinity\"")

//Test No 8

verify(~536870912,-536870913,8,"\"Checking result for Tagged Limits 536870912 \"")

//Test no 9

verify(~536870911,-536870912,9,"\"Checking result for Tagged Limits-1 536870911\"");

//Test No 10

verify(~(-536870912),536870911,10,"\"Checking result for Tagged Limits -536870912 \"")

//Test no 11

verify(~-536870913,536870912,11,"\"Checkings result for Tagged Limits+1 -536870913\"");

//Test No 12

verify(~-0,-1,12,"\"Checking result for -0\"");

WScript.Echo("Done");
