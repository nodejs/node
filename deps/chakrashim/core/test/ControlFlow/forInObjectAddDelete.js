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

var x = { a: 1, b: 2};

write("1st enumeration");
for(var i in x)
{
    if(x[i] == 1)
    {
        delete x.a;
        delete x.b;
        x.c = 3;
        x.d = 4;
    }
    else
        write(x[i]);
}

write("2nd enumeration");
var obj = { a: 1, b: 2, c: 15};
for (var i in obj) {
    if (obj[i] == 1) {
        delete obj.a;
        delete obj.b;
        obj.c = 3;
        obj.d = 4;
    }
    else
        write(obj[i]);
}
