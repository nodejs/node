//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var asmModule = 
(function(stdlib, foreign, heap) { 'use asm';   var NaN = stdlib.NaN;
  var abs = stdlib.Math.abs;
  function f(i0, d1)
  {
    i0 = i0|0;
    d1 = +d1;
    var d2 = -1.0078125;
    var d3 = 549755813888.0;
    d3 = (NaN);
    {
      d3 = (d2);
    }
    d1 = (+abs(((d3))));
    return (((abs(((-0xfffff*(-0x8000000))|0))|0) / ((-(1))|0)))|0;
  }
  return f; })
var asmHeap = new ArrayBuffer(1<<20);
var asmFun = asmModule(this, this, asmHeap);
print(asmFun());
