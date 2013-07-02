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

description('This test verifies the result returned by the eval function when exceptions are thrown and caught whithin the contents of the evaluated string.');

function throwFunc() {
  throw "";
}

function throwOnReturn(){
  1;
  return throwFunc();
}

function twoFunc() {
  2;
}

shouldBe('eval("1;")', "1");
shouldBe('eval("1; try { foo = [2,3,throwFunc(), 4]; } catch (e){}")', "1");
shouldBe('eval("1; try { 2; throw \\"\\"; } catch (e){}")', "2");
shouldBe('eval("1; try { 2; throwFunc(); } catch (e){}")', "2");
shouldBe('eval("1; try { 2; throwFunc(); } catch (e){3;} finally {}")', "3");
shouldBe('eval("1; try { 2; throwFunc(); } catch (e){3;} finally {4;}")', "4");
shouldBe('eval("function blah() { 1; }\\n blah();")', "undefined");
shouldBe('eval("var x = 1;")', "undefined");
shouldBe('eval("if (true) { 1; } else { 2; }")', "1");
shouldBe('eval("if (false) { 1; } else { 2; }")', "2");
shouldBe('eval("try{1; if (true) { 2; throw \\"\\"; } else { 2; }} catch(e){}")', "2");
shouldBe('eval("1; var i = 0; do { ++i; 2; } while(i!=1);")', "2");
shouldBe('eval("try{1; var i = 0; do { ++i; 2; throw \\"\\"; } while(i!=1);} catch(e){}")', "2");
shouldBe('eval("1; try{2; throwOnReturn();} catch(e){}")', "2");
shouldBe('eval("1; twoFunc();")', "undefined");
shouldBe('eval("1; with ( { a: 0 } ) { 2; }")', "2");
