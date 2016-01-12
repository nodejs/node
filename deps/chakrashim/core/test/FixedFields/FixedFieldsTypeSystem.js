//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.Echo("Testing final type constructor with SimpleDictionaryTypeHandler:");

var callFunctionOnObject = function(o) {
    WScript.Echo(o.theFunction());
}

var LargeObject = function(f) {
    this.p0 = 0;
    this.p1 = 1;
    this.p2 = 2;
    this.p3 = 3;
    this.p4 = 4;
    this.p5 = 5;
    this.p6 = 6;
    this.p7 = 7;
    this.p8 = 8;
    this.p9 = 9;
    this.p10 = 10;
    this.p11 = 11;
    this.p12 = 12;
    this.p13 = 13;
    this.p14 = 14;
    this.p15 = 15;
    this.p16 = 16;
    this.p17 = 17;
    this.p18 = 18;
    this.p19 = 19;
    this.p20 = 20;
    this.p21 = 21;
    this.p22 = 22;
    this.p23 = 23;
    this.p24 = 24;
    this.p25 = 25;
    this.p26 = 26;
    this.p27 = 27;
    this.p28 = 28;
    this.p29 = 29;
    this.p30 = 30;
    this.p31 = 31;
    this.p32 = 32;
    this.p33 = 33;
    this.p34 = 34;
    this.p35 = 35;
    this.p36 = 36;
    this.p37 = 37;
    this.p38 = 38;
    this.p39 = 39;
    this.p40 = 40;
    this.p41 = 41;
    this.p42 = 42;
    this.p43 = 43;
    this.p44 = 44;
    this.p45 = 45;
    this.p46 = 46;
    this.p47 = 47;
    this.p48 = 48;
    this.p49 = 49;
    this.p50 = 50;
    this.p51 = 51;
    this.p52 = 52;
    this.p53 = 53;
    this.p54 = 54;
    this.p55 = 55;
    this.p56 = 56;
    this.p57 = 57;
    this.p58 = 58;
    this.p59 = 59;
    this.p60 = 60;
    this.p61 = 61;
    this.p62 = 62;
    this.p63 = 63;
    this.p64 = 64;
    this.p65 = 65;
    this.p66 = 66;
    this.p67 = 67;
    this.p68 = 68;
    this.p69 = 69;
    this.p70 = 70;
    this.p71 = 71;
    this.p72 = 72;
    this.p73 = 73;
    this.p74 = 74;
    this.p75 = 75;
    this.p76 = 76;
    this.p77 = 77;
    this.p78 = 78;
    this.p79 = 79;
    this.p80 = 80;
    this.p81 = 81;
    this.p82 = 82;
    this.p83 = 83;
    this.p84 = 84;
    this.p85 = 85;
    this.p86 = 86;
    this.p87 = 87;
    this.p88 = 88;
    this.p89 = 89;
    this.p90 = 90;
    this.p91 = 91;
    this.p92 = 92;
    this.p93 = 93;
    this.p94 = 94;
    this.p95 = 95;
    this.p96 = 96;
    this.p97 = 97;
    this.p98 = 98;
    this.p99 = 99;
    this.p100 = 100;
    this.p101 = 101;
    this.p102 = 102;
    this.p103 = 103;
    this.p104 = 104;
    this.p105 = 105;
    this.p106 = 106;
    this.p107 = 107;
    this.p108 = 108;
    this.p109 = 109;
    this.p110 = 110;
    this.p111 = 111;
    this.p112 = 112;
    this.p113 = 113;
    this.p114 = 114;
    this.p115 = 115;
    this.p116 = 116;
    this.p117 = 117;
    this.p118 = 118;
    this.p119 = 119;
    this.p120 = 120;
    this.p121 = 121;
    this.p122 = 122;
    this.p123 = 123;
    this.p124 = 124;
    this.p125 = 125;
    this.p126 = 126;
    this.p127 = 127;
    // Adding this property should switch the object to SimpleDictionaryTypeHandler
    this.p128 = 128;
    this.theFunction = f;
}

function testLargeObjectConstructorWithFinalType() {
    // Let's create the first (singleton) instance of LargeObject...
    var largeObject1 = new LargeObject(function() { return "function on the first object"; });
    callFunctionOnObject(largeObject1);
    // Let's JIT the function now...
    callFunctionOnObject(largeObject1);

    // Now let's create the second one, which should first clear the singleton instance (if any), 
    // by which we should stop reporting any existing fixed fields as such.  Then each remaining
    // fixed field (if any) should get cleared and invalidated as individual properties are added 
    // in constructor.
    var largeObject2 = new LargeObject(function() { return "function on the second object"; });

    // Now let's make sure we still call the right functions...
    callFunctionOnObject(largeObject1);
    callFunctionOnObject(largeObject2);
}

testLargeObjectConstructorWithFinalType();

WScript.Echo();