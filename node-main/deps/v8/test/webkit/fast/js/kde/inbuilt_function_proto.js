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
shouldBe("Object.prototype.toString.__proto__","Function.prototype");
shouldBe("Object.prototype.valueOf.__proto__","Function.prototype");
shouldBe("Array.prototype.toString.__proto__","Function.prototype");
shouldBe("Array.prototype.toLocaleString.__proto__","Function.prototype");
shouldBe("Array.prototype.concat.__proto__","Function.prototype");
shouldBe("Array.prototype.join.__proto__","Function.prototype");
shouldBe("Array.prototype.pop.__proto__","Function.prototype");
shouldBe("Array.prototype.push.__proto__","Function.prototype");
shouldBe("Array.prototype.reverse.__proto__","Function.prototype");
shouldBe("Array.prototype.shift.__proto__","Function.prototype");
shouldBe("Array.prototype.slice.__proto__","Function.prototype");
shouldBe("Array.prototype.sort.__proto__","Function.prototype");
shouldBe("Array.prototype.splice.__proto__","Function.prototype");
shouldBe("Array.prototype.unshift.__proto__","Function.prototype");
shouldBe("String.prototype.toString.__proto__","Function.prototype");
shouldBe("String.prototype.valueOf.__proto__","Function.prototype");
shouldBe("String.prototype.charAt.__proto__","Function.prototype");
shouldBe("String.prototype.charCodeAt.__proto__","Function.prototype");
shouldBe("String.prototype.indexOf.__proto__","Function.prototype");
shouldBe("String.prototype.lastIndexOf.__proto__","Function.prototype");
shouldBe("String.prototype.match.__proto__","Function.prototype");
shouldBe("String.prototype.normalize.__proto__","Function.prototype");
shouldBe("String.prototype.replace.__proto__","Function.prototype");
shouldBe("String.prototype.search.__proto__","Function.prototype");
shouldBe("String.prototype.slice.__proto__","Function.prototype");
shouldBe("String.prototype.split.__proto__","Function.prototype");
shouldBe("String.prototype.substr.__proto__","Function.prototype");
shouldBe("String.prototype.substring.__proto__","Function.prototype");
shouldBe("String.prototype.toLowerCase.__proto__","Function.prototype");
shouldBe("String.prototype.toUpperCase.__proto__","Function.prototype");
shouldBe("String.prototype.big.__proto__","Function.prototype");
shouldBe("String.prototype.small.__proto__","Function.prototype");
shouldBe("String.prototype.blink.__proto__","Function.prototype");
shouldBe("String.prototype.bold.__proto__","Function.prototype");
shouldBe("String.prototype.fixed.__proto__","Function.prototype");
shouldBe("String.prototype.italics.__proto__","Function.prototype");
shouldBe("String.prototype.strike.__proto__","Function.prototype");
shouldBe("String.prototype.sub.__proto__","Function.prototype");
shouldBe("String.prototype.sup.__proto__","Function.prototype");
shouldBe("String.prototype.fontcolor.__proto__","Function.prototype");
shouldBe("String.prototype.fontsize.__proto__","Function.prototype");
shouldBe("String.prototype.anchor.__proto__","Function.prototype");
shouldBe("String.prototype.link.__proto__","Function.prototype");
shouldBe("Boolean.prototype.toString.__proto__","Function.prototype");
shouldBe("Boolean.prototype.valueOf.__proto__","Function.prototype");
shouldBe("Date.prototype.toString.__proto__","Function.prototype");
shouldBe("Date.prototype.toUTCString.__proto__","Function.prototype");
shouldBe("Date.prototype.toDateString.__proto__","Function.prototype");
shouldBe("Date.prototype.toTimeString.__proto__","Function.prototype");
shouldBe("Date.prototype.toLocaleString.__proto__","Function.prototype");
shouldBe("Date.prototype.toLocaleDateString.__proto__","Function.prototype");
shouldBe("Date.prototype.toLocaleTimeString.__proto__","Function.prototype");
shouldBe("Date.prototype.valueOf.__proto__","Function.prototype");
shouldBe("Date.prototype.getTime.__proto__","Function.prototype");
shouldBe("Date.prototype.getFullYear.__proto__","Function.prototype");
shouldBe("Date.prototype.getUTCFullYear.__proto__","Function.prototype");
shouldBe("Date.prototype.toGMTString.__proto__","Function.prototype");
shouldBe("Date.prototype.getMonth.__proto__","Function.prototype");
shouldBe("Date.prototype.getUTCMonth.__proto__","Function.prototype");
shouldBe("Date.prototype.getDate.__proto__","Function.prototype");
shouldBe("Date.prototype.getUTCDate.__proto__","Function.prototype");
shouldBe("Date.prototype.getDay.__proto__","Function.prototype");
shouldBe("Date.prototype.getUTCDay.__proto__","Function.prototype");
shouldBe("Date.prototype.getHours.__proto__","Function.prototype");
shouldBe("Date.prototype.getUTCHours.__proto__","Function.prototype");
shouldBe("Date.prototype.getMinutes.__proto__","Function.prototype");
shouldBe("Date.prototype.getUTCMinutes.__proto__","Function.prototype");
shouldBe("Date.prototype.getSeconds.__proto__","Function.prototype");
shouldBe("Date.prototype.getUTCSeconds.__proto__","Function.prototype");
shouldBe("Date.prototype.getMilliseconds.__proto__","Function.prototype");
shouldBe("Date.prototype.getUTCMilliseconds.__proto__","Function.prototype");
shouldBe("Date.prototype.getTimezoneOffset.__proto__","Function.prototype");
shouldBe("Date.prototype.setTime.__proto__","Function.prototype");
shouldBe("Date.prototype.setMilliseconds.__proto__","Function.prototype");
shouldBe("Date.prototype.setUTCMilliseconds.__proto__","Function.prototype");
shouldBe("Date.prototype.setSeconds.__proto__","Function.prototype");
shouldBe("Date.prototype.setUTCSeconds.__proto__","Function.prototype");
shouldBe("Date.prototype.setMinutes.__proto__","Function.prototype");
shouldBe("Date.prototype.setUTCMinutes.__proto__","Function.prototype");
shouldBe("Date.prototype.setHours.__proto__","Function.prototype");
shouldBe("Date.prototype.setUTCHours.__proto__","Function.prototype");
shouldBe("Date.prototype.setDate.__proto__","Function.prototype");
shouldBe("Date.prototype.setUTCDate.__proto__","Function.prototype");
shouldBe("Date.prototype.setMonth.__proto__","Function.prototype");
shouldBe("Date.prototype.setUTCMonth.__proto__","Function.prototype");
shouldBe("Date.prototype.setFullYear.__proto__","Function.prototype");
shouldBe("Date.prototype.setUTCFullYear.__proto__","Function.prototype");
shouldBe("Date.prototype.setYear.__proto__","Function.prototype");
shouldBe("Date.prototype.getYear.__proto__","Function.prototype");
shouldBe("Date.prototype.toGMTString.__proto__","Function.prototype");
shouldBe("RegExp.prototype.exec.__proto__","Function.prototype");
shouldBe("RegExp.prototype.test.__proto__","Function.prototype");
shouldBe("RegExp.prototype.toString.__proto__","Function.prototype");
shouldBe("Error.prototype.toString.__proto__","Function.prototype");
