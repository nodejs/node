// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addBenchmark(name, test) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test)
      ]);
}

addBenchmark('Calls-No-Argument', callsNoArgument);
addBenchmark('Calls-No-Argument-1-Local', callsNoArgument_OneLocal);
addBenchmark('Calls-No-Argument-2-Locals', callsNoArgument_2Locals);
addBenchmark('Calls-No-Argument-3-Locals', callsNoArgument_3Locals);
addBenchmark('Calls-No-Argument-4-Locals', callsNoArgument_4Locals);
addBenchmark('Calls-No-Argument-5-Locals', callsNoArgument_5Locals);
addBenchmark('Calls-No-Argument-10-Locals', callsNoArgument_10Locals);
addBenchmark('Calls-No-Argument-100-Locals', callsNoArgument_100Locals);


function callsNoArgument() {
    function f() {
        return 0;
    }

    for (var i = 0; i < 1000; ++i) {
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
    }
}

function callsNoArgument_OneLocal() {
    function constructObject() {
        this.f = function() {
            var x1 = 1;
            return 0;
        };
    }
    let o = new constructObject();

    for (var i = 0; i < 1000; ++i) {
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
    }
}

function callsNoArgument_2Locals() {
    function constructObject() {
        this.f = function() {
            var x1 = 1;
            var x2 = 2;
            return 0;
        };
    }
    let o = new constructObject();

    for (var i = 0; i < 1000; ++i) {
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
    }
}

function callsNoArgument_3Locals() {
    function constructObject() {
        this.f = function() {
            var x1 = 1;
            var x2 = 2;
            var x3 = 3;
            return 0;
        };
    }
    let o = new constructObject();

    for (var i = 0; i < 1000; ++i) {
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
    }
}

function callsNoArgument_4Locals() {
    function constructObject() {
        this.f = function() {
            var x1 = 1;
            var x2 = 2;
            var x3 = 3;
            var x4 = 4;
            return 0;
        };
    }
    let o = new constructObject();

    for (var i = 0; i < 1000; ++i) {
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
    }
}

function callsNoArgument_5Locals() {
    function constructObject() {
        this.f = function() {
            var x1 = 1;
            var x2 = 2;
            var x3 = 3;
            var x4 = 4;
            var x5 = 5;
            return 0;
        };
    }
    let o = new constructObject();

    for (var i = 0; i < 1000; ++i) {
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
    }
}

function callsNoArgument_10Locals() {
    function constructObject() {
        this.f = function() {
            var x1 = 1;
            var x2 = 2;
            var x3 = 3;
            var x4 = 4;
            var x5 = 5;
            var x6 = 6;
            var x7 = 7;
            var x8 = 8;
            var x9 = 9;
            var x10 = 10;
            return 0;
        };
    }
    let o = new constructObject();

    for (var i = 0; i < 1000; ++i) {
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
    }
}

function callsNoArgument_100Locals() {
    function constructObject() {
        this.f = function() {
            var x1 = 1;
            var x2 = 2;
            var x3 = 3;
            var x4 = 4;
            var x5 = 5;
            var x6 = 6;
            var x7 = 7;
            var x8 = 8;
            var x9 = 9;
            var x10 = 10;
            var x11 = 11;
            var x12 = 12;
            var x13 = 13;
            var x14 = 14;
            var x15 = 15;
            var x16 = 16;
            var x17 = 17;
            var x18 = 18;
            var x19 = 19;
            var x20 = 20;
            var x21 = 21;
            var x22 = 22;
            var x23 = 23;
            var x24 = 24;
            var x25 = 25;
            var x26 = 26;
            var x27 = 27;
            var x28 = 28;
            var x29 = 29;
            var x30 = 30;
            var x31 = 31;
            var x32 = 32;
            var x33 = 33;
            var x34 = 34;
            var x35 = 35;
            var x36 = 36;
            var x37 = 37;
            var x38 = 38;
            var x39 = 39;
            var x40 = 40;
            var x41 = 41;
            var x42 = 42;
            var x43 = 43;
            var x44 = 44;
            var x45 = 45;
            var x46 = 46;
            var x47 = 47;
            var x48 = 48;
            var x49 = 49;
            var x50 = 50;
            var x51 = 51;
            var x52 = 52;
            var x53 = 53;
            var x54 = 54;
            var x55 = 55;
            var x56 = 56;
            var x57 = 57;
            var x58 = 58;
            var x59 = 59;
            var x60 = 60;
            var x61 = 61;
            var x62 = 62;
            var x63 = 63;
            var x64 = 64;
            var x65 = 65;
            var x66 = 66;
            var x67 = 67;
            var x68 = 68;
            var x69 = 69;
            var x70 = 70;
            var x71 = 71;
            var x72 = 72;
            var x73 = 73;
            var x74 = 74;
            var x75 = 75;
            var x76 = 76;
            var x77 = 77;
            var x78 = 78;
            var x79 = 79;
            var x80 = 80;
            var x81 = 81;
            var x82 = 82;
            var x83 = 83;
            var x84 = 84;
            var x85 = 85;
            var x86 = 86;
            var x87 = 87;
            var x88 = 88;
            var x89 = 89;
            var x90 = 90;
            var x91 = 91;
            var x92 = 92;
            var x93 = 93;
            var x94 = 94;
            var x95 = 95;
            var x96 = 96;
            var x97 = 97;
            var x98 = 98;
            var x99 = 99;
            var x100 = 100;
            return 0;
        };
    }
    let o = new constructObject();

    for (var i = 0; i < 1000; ++i) {
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
    }
}
