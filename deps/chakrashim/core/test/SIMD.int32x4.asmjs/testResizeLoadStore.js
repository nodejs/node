//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var m = function(stdlib,imports,buffer){
    "use asm";
    //var F32=stdlib.Int32Array; 
    var F32=stdlib.Float32Array; 
    var I32=stdlib.Int32Array; 
    var f32=new F32(buffer);
    var i32=new I32(buffer);
    var len=stdlib.byteLength;
    var i4 = stdlib.SIMD.Int32x4;
    var i4load = i4.load;
    var i4store = i4.store;
    var i4check = i4.check; 
    function ch(newBuffer) 
    { 
        if(len(newBuffer) & 0xffffff || len(newBuffer) <= 0xffffff || len(newBuffer) > 0x80000000) 
            return false; 
        f32=new F32(newBuffer);
        i32=new I32(newBuffer);
        buffer=newBuffer; 
        return true 
    }
    function store(value, loc) { value=i4check(value); loc = loc|0; loc = loc<<2; i4store(i32, loc>>2, value);  }
    function load(loc) {loc = loc|0; loc = loc<<2; return i4load(i32, loc>>2);  }
    
    return { load:load
            ,store:store
            ,changeHeap:ch}
    };
var buf1 = new ArrayBuffer(0x1000000);
var f32 = new Float32Array(buf1);
var i32 = new Int32Array(buf1);

this['byteLength'] =
  Function.prototype.call.bind(Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get);
var o = m(this,{},buf1);

o.store(SIMD.Int32x4(-5,6,17,8000),4);
var ret = o.load(4);
print(ret.toString());

o.store(SIMD.Int32x4(-5,6,17,8000), i32.length-4);
var ret = o.load(i32.length-4);
print(ret.toString());
try {o.store(SIMD.Int32x4(11, 304, -22239, 34010), f32.length); print("Wrong")} catch(err) { print("Correct");}

var buf2 = new ArrayBuffer(0x2000000);
print(o.changeHeap(buf2));

// heap doubled, no OOB
o.store(SIMD.Int32x4(511, 304, -22239, 34010), i32.length);
var ret = o.load(i32.length);
print(ret.toString());
o.store(SIMD.Int32x4(511, 304, -22239, 34010), i32.length * 2 - 4);
