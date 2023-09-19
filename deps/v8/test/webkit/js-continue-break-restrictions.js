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

description("Verify that invalid continue and break statements are handled correctly");

shouldBeTrue("L:{true;break L;false}");
shouldThrow("if (0) { L:{ break; } }");
shouldThrow("if (0) { L:{ continue L; } }");
shouldThrow("if (0) { L:{ continue; } }");
shouldThrow("if (0) { switch (1) { case 1: continue; } }");
shouldBeTrue("A:L:{true;break L;false}");
shouldThrow("if (0) { A:L:{ break; } }");
shouldThrow("if (0) { A:L:{ continue L; } }");
shouldThrow("if (0) { A:L:{ continue; } }");

shouldBeTrue("L:A:{true;break L;false}");
shouldThrow("if (0) { L:A:{ break; } }");
shouldThrow("if (0) { L:A:{ continue L; } }");
shouldThrow("if (0) { L:A:{ continue; } }");

shouldBeUndefined("if(0){ L:for(;;) continue L; }")
shouldBeUndefined("if(0){ L:A:for(;;) continue L; }")
shouldBeUndefined("if(0){ A:L:for(;;) continue L; }")
shouldThrow("if(0){ A:for(;;) L:continue L; }")
shouldBeUndefined("if(0){ L:for(;;) A:continue L; }")

shouldBeUndefined("if(0){ L:do continue L; while(0); }")
shouldBeUndefined("if(0){ L:A:do continue L; while(0); }")
shouldBeUndefined("if(0){ A:L:do continue L; while(0);}")
shouldThrow("if(0){ A:do L:continue L; while(0); }")
shouldBeUndefined("if(0){ L:do A:continue L; while(0); }")


shouldBeUndefined("if(0){ L:while(0) continue L; }")
shouldBeUndefined("if(0){ L:A:while(0) continue L; }")
shouldBeUndefined("if(0){ A:L:while(0) continue L; }")
shouldThrow("if(0){ A:while(0) L:continue L; }")
shouldBeUndefined("if(0){ L:while(0) A:continue L; }")
