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

description("KDE JS Test");
// shouldBe() (base.js) test at very end of this file (Harri)

/*
* md5.jvs 1.0b 27/06/96
*
* Javascript implementation of the RSA Data Security, Inc. MD5
* Message-Digest Algorithm.
*
* Copyright (c) 1996 Henri Torgemane. All Rights Reserved.
*
* Permission to use, copy, modify, and distribute this software
* and its documentation for any purposes and without
* fee is hereby granted provided that this copyright notice
* appears in all copies.
*
* Of course, this soft is provided "as is" without express or implied
* warranty of any kind.

*/

// $Id: md5-1.js 11771 2005-12-26 23:07:31Z mjs $

function array(n) {
    for(i=0;i<n;i++) this[i]=0;
    this.length=n;
}

/* Some basic logical functions had to be rewritten because of a bug in
* Javascript.. Just try to compute 0xffffffff >> 4 with it..
* Of course, these functions are slower than the original would be, but
* at least, they work!
*/

function integer(n) { return n%(0xffffffff+1); }

function shr(a,b) {
    a=integer(a);
    b=integer(b);
    if (a-0x80000000>=0) {
        a=a%0x80000000;
        a>>=b;
        a+=0x40000000>>(b-1);
    } else
        a>>=b;
    return a;
}

function shl1(a) {
    a=a%0x80000000;
    if (a&0x40000000==0x40000000)
        {
            a-=0x40000000;
            a*=2;
            a+=0x80000000;
        } else
            a*=2;
    return a;
}

function shl(a,b) {
    a=integer(a);
    b=integer(b);
    for (var i=0;i<b;i++) a=shl1(a);
    return a;
}

function and(a,b) {
    a=integer(a);
    b=integer(b);
    var t1=(a-0x80000000);
    var t2=(b-0x80000000);
    if (t1>=0)
        if (t2>=0)
            return ((t1&t2)+0x80000000);
        else
            return (t1&b);
    else
        if (t2>=0)
            return (a&t2);
        else
            return (a&b);
}

function or(a,b) {
    a=integer(a);
    b=integer(b);
    var t1=(a-0x80000000);
    var t2=(b-0x80000000);
    if (t1>=0)
        if (t2>=0)
            return ((t1|t2)+0x80000000);
        else
            return ((t1|b)+0x80000000);
    else
        if (t2>=0)
            return ((a|t2)+0x80000000);
        else
            return (a|b);
}

function xor(a,b) {
    a=integer(a);
    b=integer(b);
    var t1=(a-0x80000000);
    var t2=(b-0x80000000);
    if (t1>=0)
        if (t2>=0)
            return (t1^t2);
        else
            return ((t1^b)+0x80000000);
    else
        if (t2>=0)
            return ((a^t2)+0x80000000);
        else
            return (a^b);
}


function not(a) {
    a=integer(a);
    return (0xffffffff-a);
}

/* Here begin the real algorithm */

var state = new array(4);
var count = new array(2);
count[0] = 0;
count[1] = 0;

var buffer = new array(64);
var transformBuffer = new array(16);
var digestBits = new array(16);

var S11 = 7;
var S12 = 12;
var S13 = 17;
var S14 = 22;
var S21 = 5;
var S22 = 9;
var S23 = 14;
var S24 = 20;
var S31 = 4;
var S32 = 11;
var S33 = 16;
var S34 = 23;
var S41 = 6;
var S42 = 10;
var S43 = 15;
var S44 = 21;


function F(x,y,z) {
    return or(and(x,y),and(not(x),z));
}

function G(x,y,z) {
    return or(and(x,z),and(y,not(z)));
}

function H(x,y,z) {
    return xor(xor(x,y),z);
}

function I(x,y,z) {
    return xor(y ,or(x , not(z)));
}

function rotateLeft(a,n) {
    return or(shl(a, n),(shr(a,(32 - n))));
}

function FF(a,b,c,d,x,s,ac) {
    a = a+F(b, c, d) + x + ac;
    a = rotateLeft(a, s);
    a = a+b;
    return a;
}

function GG(a,b,c,d,x,s,ac) {
    a = a+G(b, c, d) +x + ac;
    a = rotateLeft(a, s);
    a = a+b;
    return a;
}

function HH(a,b,c,d,x,s,ac) {
    a = a+H(b, c, d) + x + ac;
    a = rotateLeft(a, s);
    a = a+b;
    return a;
}

function II(a,b,c,d,x,s,ac) {
    a = a+I(b, c, d) + x + ac;
    a = rotateLeft(a, s);
    a = a+b;
    return a;
}

function transform(buf,offset) {
    var a=0, b=0, c=0, d=0;
    var x = transformBuffer;

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];

    for (i = 0; i < 16; i++) {
        x[i] = and(buf[i*4+offset],0xff);
        for (j = 1; j < 4; j++) {
            x[i]+=shl(and(buf[i*4+j+offset] ,0xff), j * 8);
        }
    }

    /* Round 1 */
    a = FF ( a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
    d = FF ( d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
    c = FF ( c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
    b = FF ( b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
    a = FF ( a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
    d = FF ( d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
    c = FF ( c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
    b = FF ( b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
    a = FF ( a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
    d = FF ( d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
    c = FF ( c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
    b = FF ( b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
    a = FF ( a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
    d = FF ( d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
    c = FF ( c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
    b = FF ( b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

    /* Round 2 */
    a = GG ( a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
    d = GG ( d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
    c = GG ( c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
    b = GG ( b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
    a = GG ( a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
    d = GG ( d, a, b, c, x[10], S22, 0x2441453); /* 22 */
    c = GG ( c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
    b = GG ( b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
    a = GG ( a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
    d = GG ( d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
    c = GG ( c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
    b = GG ( b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
    a = GG ( a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
    d = GG ( d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
    c = GG ( c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
    b = GG ( b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    a = HH ( a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
    d = HH ( d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
    c = HH ( c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
    b = HH ( b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
    a = HH ( a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
    d = HH ( d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
    c = HH ( c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
    b = HH ( b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
    a = HH ( a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
    d = HH ( d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
    c = HH ( c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
    b = HH ( b, c, d, a, x[ 6], S34, 0x4881d05); /* 44 */
    a = HH ( a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
    d = HH ( d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
    c = HH ( c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
    b = HH ( b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    a = II ( a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
    d = II ( d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
    c = II ( c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
    b = II ( b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
    a = II ( a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
    d = II ( d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
    c = II ( c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
    b = II ( b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
    a = II ( a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
    d = II ( d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
    c = II ( c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
    b = II ( b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
    a = II ( a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
    d = II ( d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
    c = II ( c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
    b = II ( b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

    state[0] +=a;
    state[1] +=b;
    state[2] +=c;
    state[3] +=d;
}

function init() {
    count[0]=count[1] = 0;
    state[0] = 0x67452301;
    state[1] = 0xefcdab89;
    state[2] = 0x98badcfe;
    state[3] = 0x10325476;
    for (i = 0; i < digestBits.length; i++)
        digestBits[i] = 0;
}

function update(b) {
    var index,i;

    index = and(shr(count[0],3) , 0x3f);
    if (count[0]<0xffffffff-7)
        count[0] += 8;
    else {
        count[1]++;
        count[0]-=0xffffffff+1;
        count[0]+=8;
    }

    buffer[index] = and(b,0xff);
    if (index >= 63) {
        transform(buffer, 0);
    }
}

function finish() {
    var bits = new array(8);
    var padding;
    var i=0, index=0, padLen=0;

    for (i = 0; i < 4; i++) {
        bits[i] = and(shr(count[0],(i * 8)), 0xff);
    }

    for (i = 0; i < 4; i++) {
        bits[i+4]=and(shr(count[1],(i * 8)), 0xff);
    }

    index = and(shr(count[0], 3) ,0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    padding = new array(64);
    padding[0] = 0x80;
    for (i=0;i<padLen;i++)
        update(padding[i]);

    for (i=0;i<8;i++)
        update(bits[i]);

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            digestBits[i*4+j] = and(shr(state[i], (j * 8)) , 0xff);
        }
    }
}

/* End of the MD5 algorithm */

function hexa(n) {
    var hexa_h = "0123456789abcdef";
    var hexa_c="";
    var hexa_m=n;
    for (hexa_i=0;hexa_i<8;hexa_i++) {
        hexa_c=hexa_h.charAt(Math.abs(hexa_m)%16)+hexa_c;
        hexa_m=Math.floor(hexa_m/16);
    }
    return hexa_c;
}

var ascii="01234567890123456789012345678901" +
" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ"+
"[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

function MD5(entree)
{
    var l,s,k,ka,kb,kc,kd;

    init();
    for (k=0;k<entree.length;k++) {
        l=entree.charAt(k);
        update(ascii.lastIndexOf(l));
    }
    finish();
    ka=kb=kc=kd=0;
    for (i=0;i<4;i++) ka+=shl(digestBits[15-i], (i*8));
    for (i=4;i<8;i++) kb+=shl(digestBits[15-i], ((i-4)*8));
    for (i=8;i<12;i++) kc+=shl(digestBits[15-i], ((i-8)*8));
    for (i=12;i<16;i++) kd+=shl(digestBits[15-i], ((i-12)*8));
    s=hexa(kd)+hexa(kc)+hexa(kb)+hexa(ka);
    return s;
}

shouldBe("MD5('kde')", "'186cf28b76f2264e9fea8fcf91cb4f5d'");
