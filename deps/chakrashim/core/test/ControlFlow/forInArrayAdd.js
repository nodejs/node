//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(args)
{
    if (typeof (WScript) == "undefined")
        print(args);
    else
        WScript.Echo(args);
}

var a = [11,22,33,44];
a.x = "a.x";
a.y = "a.y";
a.z = "a.z";

write("Scenario:1 - Adding new array indexes while enumerating expandos")

for( var i in a)
{
  if(i == "y")
  {
    a[5] = 55;
    a[6] = 66;
  }
  write("Index:" + i + " Value:" + a[i]);
}

write("Scenario:2 - Adding new array expandos while enumerating array for second time")

for( var i in a)
{
  if(i == "z")
  {
      a[7] = 77;
      a[9] = 99;
  }
  if(i == "7")
  {
     a.xx = "a.xx";
     a.yy = "a.yy";
  }
  write("Index:" + i + " Value:" + a[i]);
}

write("Scenario:3 - Adding new array expandos while enumerating Object for second time")

var b = [11,22,33,44];
b.x = "b.x";
b.y = "b.y";
b.z = "b.z";

for( var i in b)
{
  if(i == "x")
  {
    b[5] = 55;
    b[7] = 77;
  }
  if(i == "7")
  {
     b.xx = "b.xx";
     b.yy = "b.yy";
  }

  if(i == "xx")
  {
    b[9] = 99;
    b[10] = 1010;
  }

  if(i == "9")
  {
     b.zz = "b.zz";
  }
  write("Index:" + i + " Value:" + b[i]);
}

write("Scenario:3 - Adding new array expandos while enumerating Object for second time")

var b = [11,22,33,44];
b.x = "b.x";
b.y = "b.y";
b.z = "b.z";

for( var i in b)
{
  if(i == "x")
  {
    b[5] = 55;
    b[7] = 77;
  }
  if(i == "7")
  {
     b.xx = "b.xx";
     b.yy = "b.yy";
  }

  if(i == "xx")
  {
    b[9] = 99;
    b[10] = 1010;
  }

  if(i == "9")
  {
     b.zz = "b.zz";
  }
  write("Index:" + i + " Value:" + b[i]);
}

write("Scenario:4 - random additions");

var d = [1];
var counter = 0;

for (var i in d)
{
  if(counter == 25)
  {
    break;
  }
  if(counter%2 == 1)
  {
    d[counter*counter] = counter*counter;
  }
  else
  {
    d["x"+counter] = "d.x"+counter;
  }
  write("Index:" + i + " Value:" + d[i]);
  counter++;
}
