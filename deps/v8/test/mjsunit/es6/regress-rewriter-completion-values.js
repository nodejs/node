// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Sloppy block function hoisting in scripts must not contribute to the
// completion value.
const r = Realm.create();
assertEquals(undefined, Realm.eval(r, "{ function f() {} }"));
assertEquals(1, Realm.eval(r, "1; { function f() {} }"));

// An empty catch block (or one without a value-producing statement) must
// produce undefined even if the try block wrote to .result before throwing.
assertEquals(undefined, eval("try { do { 42; } while (null.x); } catch (e) {}"));
assertEquals(
    undefined, eval("try { try { 42; } finally { null.x; } } catch (e) {}"));
assertEquals(
    undefined,
    eval("try { using x = { [Symbol.dispose]() { throw 1; } }; 42; } catch (e) {}"));
assertEquals(
    undefined,
    eval("L: { try { 42; L2: { break L2; } null.x; } catch (e) {} }"));

// A labeled break whose target is inside a finally block completes the finally
// block normally and must preserve the try block's completion value.
assertEquals(1, eval("L: { try { 1; } finally { L2: { break L2; } } }"));
assertEquals(
    1, eval("while (true) { try { 1; break; } finally { L2: { break L2; } } }"));
assertEquals(
    1,
    eval("while (true) { try { 1; break; } finally { L2: { break L2; } 2; } }"));
assertEquals(undefined, eval("while (true) { try { 1; } finally { break; } }"));
