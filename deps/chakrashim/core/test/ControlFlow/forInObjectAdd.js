//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(args)
{
   if(typeof(WScript) == "undefined")
      print(args);
   else
     WScript.Echo(args);
}

write("Scenario 1: Adding properties on the fly");
var x = { a: 1, b: 2};

for(var i in x)
{
    if(x[i] == 2)
    {
        x.c = 3;
        x.d = 4;
    }

    write(x[i]);
}

write("Scenario 2: Large number of properties in forin");
var largeObj = {};
for(var k=0; k < 25; k++)
{
   largeObj["p"+k] = k + 0.3;
}

for(var i in largeObj)
{
    write(largeObj[i]);
}

write("Sceanrio 3: Nested Forin");
var outerObj = { a: 3, b: 4, c: 5 };
var innerObj = { a: 3, b: 4, c: 5 };
for(var i in outerObj)
{
   write(i);
   for(var j in innerObj)
   {
       write(j);
   }
}

write("Scenario 4: Properties and numerical indices in object");
var objWithNumber= { a: 12, b: 13, c:23 };
objWithNumber[13] = "Number13";
objWithNumber[15] = "Number15";

for(var i in objWithNumber)
{
    write(objWithNumber[i]);
}

var undef;

for(var i in undef)
{
   write("FAILED: Entering enumeration of undefined");
}

var nullValue = null;

for(var i in nullValue)
{
   write("FAILED: Entering enumeration of null value");
}

var integer = 3;

for(var i in integer)
{
   write("FAILED: Entering enumeration of integer");
}

var double = 3.4;

for(var i in double)
{
   write("FAILED: Entering enumeration of double");
}
