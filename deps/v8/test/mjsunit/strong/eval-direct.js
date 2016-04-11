// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

// In strong mode, direct calls to eval are forbidden

assertThrows("'use strong'; eval();", SyntaxError);
assertThrows("'use strong'; (eval)();", SyntaxError);
assertThrows("'use strong'; (((eval)))();", SyntaxError);
assertThrows("'use strong'; eval([]);", SyntaxError);
assertThrows("'use strong'; eval('function f() {}');", SyntaxError);
assertThrows("'use strong'; function f() {eval()}", SyntaxError);

assertDoesNotThrow("'use strong'; eval;");
assertDoesNotThrow("'use strong'; let foo = eval; foo();");
assertDoesNotThrow("'use strong'; (1, eval)();");

// TODO(neis): The tagged template triggers %ObjectFreeze on an array, which
// throws when trying to redefine 'length'.
// assertDoesNotThrow("'use strong'; eval`foo`;");
