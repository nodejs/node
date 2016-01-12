//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo0(o,i)
{
    if (o==10 && i++,o)
    {
    }
    else
    {
        WScript.Echo("FAILED");
    }
}
foo0(9, 0);

// - At 'o.p && 1', 'BrTrue 1' is const-folded to 'Br' to the loop exit block with the 'break'
// - 'a' becomes live as a float on the right side of '||' and is only live as an int on the left side
// - Since both of those blocks are predecessors to the loop exit block with the 'break', 'a' is kept live as a float on exit
//   out of the loop
// - When compensating in the 'BrTrue 1' block, we don't need an airlock block to convert 'a' to a float only on exit out of the
//   loop because that branch was already const-folded into 'Br' and always flows into the exit block
function foo1() {
    var o = { p: 0 };
    var a = 0;
    for(var i = 0; i < 2; ++i) {
        a = 1;
        if(o.p && 1 || (a /= 2))
            break;
    }
}
foo1();
foo1();

function foo2(){
  var ary = new Array(10);
  var c = -1;
  var e = 1;
  var g = 1;
  ary[ary.length-1] = 1;
  ary.length = 100;
  g =((e < c)||(g < c));
  if(g)
        c=((e < c));
  c =((e < c)) + g;
  ary[ary.length-1];
};

foo2();
foo2();
foo2();

WScript.Echo("Passed");
