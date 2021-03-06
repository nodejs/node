// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// ()=>42 will have the same start and end position as the top-level script.
var foo = eval("()=>{ return 42 }");
assertEquals(42, foo());

%LiveEditPatchScript(foo, "()=>{ return 13 }");

assertEquals(13, foo());
