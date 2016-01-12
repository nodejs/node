//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = 6;
var giraffe = 8;
var zebra = x + giraffe;
function f(t) {
    return t + t;
}
var cat = f(zebra);
rat = cat * 2;
do {
    rat = rat - 3;
    cat = cat + 4;
}
while (rat > 4);
var dragon = rat / 2;

WScript.Echo(x)
WScript.Echo(giraffe)
WScript.Echo(zebra)
WScript.Echo(cat)
WScript.Echo(rat)
WScript.Echo(dragon);

do 
{
   WScript.Echo("Should print once - 0");
}
while(0);

do 
{
   WScript.Echo("Should print once - false");

}
while(false);

a: do 
{
   WScript.Echo("Should print once - label");
   do 
   {
      break a;
   }while(false);
   WScript.Echo("Should not print");
}
while(false);
