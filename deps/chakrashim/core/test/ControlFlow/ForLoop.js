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
while (rat > 4) {
    rat = rat - 3;
    cat = cat + 4;
}
for (var i = 4; i < zebra; ++i) {
    cat = cat - 1;
}
var dragon = rat / 2;
dragon += 8;

WScript.Echo(x)
WScript.Echo(giraffe)
WScript.Echo(zebra)
WScript.Echo(cat)
WScript.Echo(rat)
WScript.Echo(dragon);

function MatchCollectionLocalColllection(collection, value)
{
    for (var i = 0; i < collection.length; i++)
    {
      if (collection[i] == value)
        return true;
    }
    return false;
}
  
WScript.Echo(MatchCollectionLocalColllection(["car", "truck"] , "car"));

WScript.Echo(MatchCollectionLocalColllection(["car", "truck"] , "foo"));

var gCollection = ["car", "truck"];
function MatchCollectionGlobalColllection(value)
{
    for (var i = 0; i < gCollection.length; i++)
    {
      if (gCollection[i] == value)
        return true;
    }
    return false;
}
WScript.Echo(MatchCollectionGlobalColllection("car"));
WScript.Echo(MatchCollectionGlobalColllection("foo"));

function MatchCollectionGlobalColllectionandValue()
{
    for (var i = 0; i < gCollection.length; i++)
    {
      if (gCollection[i] == gValue)
        return true;
    }
    return false;
}

var gValue = "car";
WScript.Echo(MatchCollectionGlobalColllectionandValue());

gValue = "foo";
WScript.Echo(MatchCollectionGlobalColllectionandValue());
