//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var m = function(stdlib,imports,buffer){
    "use asm";
    //var F32=stdlib.Int32Array; 
    var F32=stdlib.Float32Array; 
    var f32=new F32(buffer);
    var len=stdlib.byteLength;
    var f4 = stdlib.SIMD.Float32x4;
    var f4load = f4.load;
    var f4store = f4.store;
    var f4check = f4.check; 
    function ch(newBuffer) 
    { 
        if(len(newBuffer) & 0xffffff || len(newBuffer) <= 0xffffff || len(newBuffer) > 0x80000000) 
            return false; 
        f32=new F32(newBuffer);
        buffer=newBuffer; 
        return true 
    }
    function store(value, loc) { value=f4check(value); loc = loc|0; loc = loc<<2; f4store(f32, loc>>2, value);  }
    function load(loc) {loc = loc|0; loc = loc<<2; return f4load(f32, loc>>2);  }
    
    return { load:load
            ,store:store
            ,changeHeap:ch}
    };
var buf1 = new ArrayBuffer(0x1000000);
var f32 = new Float32Array(buf1);

this['byteLength'] =
  Function.prototype.call.bind(Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get);
var o = m(this,{},buf1);

o.store(SIMD.Float32x4(5.1,6.1,-0.0,0.0),4);

var ret = o.load(4);
print(ret.toString());

o.store(SIMD.Float32x4(5.1,6.1,7.1,8.1), f32.length-4);
var ret = o.load(f32.length-4);
print(ret.toString());
try {o.store(SIMD.Float32x4(5.1,6.1,7.1,8.1), f32.length);print("Wrong");} catch(err) { print("Correct"); }

var buf2 = new ArrayBuffer(0x2000000);
print(o.changeHeap(buf2));

o.store(SIMD.Float32x4(5.1,6.1,7.1,8.1), f32.length);
var ret = o.load(f32.length);
print(ret.toString());
o.store(SIMD.Float32x4(5.1,6.1,7.1,8.1), f32.length * 2 - 4);