//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function AsmModule(glob, imp, b) {
    "use asm"
    var clz = glob.Math.clz32;

    function f1(a)
    {
        a = a|0;
        return clz(a|0)|0;
    }

    function f2()
    {
        return clz(0)|0;
    }
    function f3()
    {
        return clz(0x80000000)|0;
    }
    function f4()
    {
        return clz(32768)|0;
    }
    return {
        f1:f1,
        f2:f2,
        f3:f3,
        f4:f4
    }
}

var global = this;
var env = {}
var heap = new ArrayBuffer(1<<20);
var asmModule = AsmModule(global, env, heap);

print(asmModule.f1(0));
print(asmModule.f1(0x80000000));
print(asmModule.f1(32768));
print(asmModule.f2());
print(asmModule.f3());
print(asmModule.f4());
