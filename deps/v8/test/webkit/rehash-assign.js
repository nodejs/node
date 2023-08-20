// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description("Tests that assignments to global variables behave properly when the property table is rehashed.");

var result;

result = (function() {
    a = 0;
    b = 1;
    c = 2;
    d = 3;
    e = 4;
    f = 5;
    g = 6;
    h = 7;
    i = 8
    j = 9;
    k = 10;
    l = 11;
    m = 12;
    n = 13;
    o = 14;
    p = 15;
    q = 16;
    r = 17;
    s = 18;
    t = 19;
    u = 20;
    v = 21;
    w = 22;
    x = 23;
    y = 24;
    z = 25;
    aa = 0;
    bb = 1;
    cc = 2;
    dd = 3;
    ee = 4;
    ff = 5;
    gg = 6;
    hh = 7;
    ii = 8;
    jj = 9;
    kk = 10;
    ll = 11;
    mm = 12;
    nn = 13;
    oo = 14;
    pp = 15;
    qq = 16;
    rr = 17;
    ss = 18;
    tt = 19;
    uu = 20;
    vv = 21;
    ww = 22;
    xx = 23;
    yy = 24;
    zz = 25;
    aaa = 0;
    bbb = 1;
    ccc = 2;
    ddd = 3;
    eee = 4;
    fff = 5;
    ggg = 6;
    hhh = 7;
    iii = 8;
    jjj = 9;
    kkk = 10;
    lll = 11;
    mmm = 12;
    nnn = 13;
    ooo = 14;
    ppp = 15;
    qqq = 16;
    rrr = 17;
    sss = 18;
    ttt = 19;
    uuu = 20;
    vvv = 21;
    www = 22;
    xxx = 23;
    yyy = 24;
    zzz = 25;
    aaaa = 0;
    bbbb = 1;
    cccc = 2;
    dddd = 3;
    eeee = 4;
    ffff = 5;
    gggg = 6;
    hhhh = 7;
    iiii = 8;
    jjjj = 9;
    kkkk = 10;
    llll = 11;
    mmmm = 12;
    nnnn = 13;
    oooo = 14;
    pppp = 15;
    qqqq = 16;
    rrrr = 17;
    ssss = 18;
    tttt = 19;
    uuuu = 20;
    vvvv = 21;
    wwww = 22;
    xxxx = 23;
    yyyy = 24;
    zzzz = 25;
    return 1;
})();

shouldBe(result.toString(), "1");
