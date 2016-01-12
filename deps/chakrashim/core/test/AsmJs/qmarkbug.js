//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var asmModule = 
(function(stdlib, foreign, heap) { 'use asm';   var Uint8ArrayView = new stdlib.Uint8Array(heap);
  var Int16ArrayView = new stdlib.Int16Array(heap);
  function f(d0, i1)
  {
    d0 = +d0;
    i1 = i1|0;
    var i4 = 0;
    i4 = ((0) ? 0 : ((Uint8ArrayView[0])));
    return +((-7.555786372591432e+22));
  }
  return f; })

  var asmHeap = new ArrayBuffer(1<<24);
  var asmFun = asmModule(this, {}, asmHeap);
  asmFun();
  var asmModule = 
(function(stdlib, foreign, heap) { 'use asm';   var Uint8ArrayView = new stdlib.Uint8Array(heap);
  var Int16ArrayView = new stdlib.Int16Array(heap);
  function f(d0, i1)
  {
    d0 = +d0;
    i1 = i1|0;
    var i4 = 0;
    i4 = ((0) ? ((Uint8ArrayView[0])): 0 );
    return +((-7.555786372591432e+22));
  }
  return f; })

  var asmFun = asmModule(this, {}, asmHeap);
  asmFun();
