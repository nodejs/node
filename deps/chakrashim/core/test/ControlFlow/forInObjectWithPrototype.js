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

function TestWithProto(proto)
{
    function Construct() {}

    Construct.prototype = proto;

    var derived = new Construct();
    for(var k=0; k < 3; k++)
    {
       derived["p"+k] = k + 0.3;
    }
    function TestForInObjectWithProto()
    {
         for(var p in derived)
         {
             write(p);
         }
    }
    write("Scenario: Testing forin on object with prototype");
    TestForInObjectWithProto();

    // Make a change to the prototype object

    if (proto && proto.hasOwnProperty("a")) {
        delete (proto.a);
    }

    write("Scenario: Testing forin on object with prototype after changing prototype");
    TestForInObjectWithProto();
}

TestWithProto({
    a: 0.27, c: 0.12, g: 0.12, t: 0.23
});

TestWithProto(null);
TestWithProto(undefined);
