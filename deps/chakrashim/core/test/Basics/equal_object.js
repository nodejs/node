//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(args)
{
  WScript.Echo(""+args);
}

var o = {toString: function(){write("Inside toString"); return "abc";}, valueOf: function(){write("Inside valueOf");return 10;}};

write("Scenario 1");
write(o==10);
write(o=="abc");
write(10==o);
write("abc"==o);


write("Scenario 2");
o = {valueOf: function(){write("Inside valueOf"); return 1;}};
write(o==true);
write(o==false);
write(true==o);
write(false==o);


write("Scenario 3");
var o = {valueOf: function(){write("Inside valueOf"); return 0;}};
write(o==true);
write(o==false);
write(true==o);
write(false==o);

write("Scenario 4");
o = {toString: function(){write("Inside toString"); return "1";}};
write(o==true);
write(o==false);
write(true==o);
write(false==o);

write("Scenario 5");
o = {toString: function(){write("Inside toString"); return "0";}};
write(o==true);
write(o==false);
write(true==o);
write(false==o);

write("Scenario 6");
var dtBegin = new Date("Thu Aug 5 05:30:00 PDT 2010");
var dtCurrentBegin=dtBegin.getTime();
write(dtCurrentBegin == dtBegin);
write(dtBegin == dtCurrentBegin);


