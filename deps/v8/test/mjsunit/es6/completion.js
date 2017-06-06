// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function assertUndef(x) {
  assertEquals(undefined, x);
}


// ClassDeclaration

assertUndef(eval('class C {}'));
assertUndef(eval('class C {m() {}}'));
assertUndef(eval('class C extends null {}'));
assertEquals(42, eval('42; class C {}'));
assertEquals(42, eval('42; class C {m() {}}'));
assertEquals(42, eval('42; class C extends null {}'));


// IfStatement [13.6.7]

assertUndef(eval('42; if (true) ; else 0;'));  // ES5: 42
assertUndef(eval('42; if (true) ;'));  // ES5: 42
assertUndef(eval('42; if (false) 0;'));  // ES5: 42

assertEquals(1, eval('42; if (true) 1;'));
assertEquals(1, eval('42; if (true) 1; else 0;'));
assertEquals(0, eval('42; if (false) 1; else 0;'));


// IterationStatement [13.7]

assertUndef(eval('42; do ; while (false);'));  // ES5: 42
assertUndef(eval('42; var x = 1; do ; while (x--);'));  // ES5: 42
assertUndef(eval('42; while (false) 0;'));  // ES5: 42
assertUndef(eval('42; while (true) break;'));  // ES5: 42
assertUndef(eval('42; bla: while (true) break bla;'));  // ES5: 42
assertUndef(eval('42; var x = 1; while (x--) ;'));  // ES5: 42
assertUndef(eval('42; for (; false; ) 0;'));  // ES5: 42
assertUndef(eval('42; for (var x = 1; x; x--) ;'));  // ES5: 42
assertUndef(eval('42; for (var x in ["foo", "bar"]) ;'));
assertUndef(eval('42; for (var x of ["foo", "bar"]) ;'));
assertUndef(eval('42; for (let x = 1; x; x--) ;'));
assertUndef(eval('42; for (let x in ["foo", "bar"]) ;'));
assertUndef(eval('42; for (let x of ["foo", "bar"]) ;'));
assertUndef(eval('42; for (const x in ["foo", "bar"]) ;'));
assertUndef(eval('42; for (const x of ["foo", "bar"]) ;'));

assertEquals(1, eval('42; var x = 10; do x--; while (x);'));
assertEquals(1, eval('42; var x = 10; while (x) x--;'));
assertEquals(1, eval('42; for (var x = 10; x; x--) x;'));
assertEquals(1, eval('42; for (var x = 10; x; --x) x;'));
assertEquals(1, eval('42; for (let x = 10; x; --x) x;'));
assertEquals(1, eval('42; var y = 2; for (var x in ["foo", "bar"]) y--;'));
assertEquals(1, eval('42; var y = 2; for (const x in ["foo", "bar"]) y--;'));
assertEquals(1, eval('42; var y = 2; for (let x in ["foo", "bar"]) y--;'));
assertEquals(1, eval('42; var y = 2; for (var x of ["foo", "bar"]) y--;'));
assertEquals(1, eval('42; var y = 2; for (const x of ["foo", "bar"]) y--;'));
assertEquals(1, eval('42; var y = 2; for (let x of ["foo", "bar"]) y--;'));


// WithStatement [13.11.7]

assertUndef(eval('42; with ({}) ;'));  // ES5: 42

assertEquals(1, eval('42; with ({}) 1;'));


// SwitchStatement [13.12.11]

assertUndef(eval('42; switch (0) {};'));  // ES5: 42
assertUndef(eval('42; switch (0) { case 1: 1; };'));  // ES5: 42
assertUndef(eval('42; switch (0) { case 0: ; };'));  // ES5: 42
assertUndef(eval('42; switch (0) { default: ; };'));  // ES5: 42
assertUndef(eval('42; switch (0) { case 0: break; }'));  // ES5: 42

assertEquals(1, eval('42; switch (0) { case 0: 1; }'));
assertEquals(1, eval('42; switch (0) { case 0: 1; break; }'));
assertEquals(1, eval('42; switch (0) { case 0: 1; case 666: break; }'));
assertEquals(2, eval('42; switch (0) { case 0: 1; case 666: 2; break; }'));


// TryStatement [13.15.8]

assertUndef(eval('42; try { } catch(e) { };'));  // ES5: 42
assertUndef(eval('42; try { } catch(e) { 0; };'));  // ES5: 42
assertUndef(eval('42; try { throw "" } catch(e) { };'));  // ES5: 42
assertUndef(eval('42; try { throw "" } catch(e) { } finally { };'));  // ES5: 42
assertUndef(eval('42; try { } finally { 666 };'));  // ES5: 42


// Some combinations

assertUndef(eval('42; switch (0) { case 0: if (true) break; }'));  // ES5: 42
assertUndef(eval('42; switch (0) { case 0: 1; if (true) ; }'));  // ES5: 1
assertUndef(eval('42; switch (0) { case 0: 1; try { break } catch(e) { }; }'));  // ES5: 1

assertEquals(0, eval('42; switch (0) { case 0: 0; case 1: break; }'));
assertEquals(0, eval('42; while (1) { 0; break; }'))
assertEquals(0, eval('42; bla: while (1) { 0; break bla; }'))
assertEquals(0, eval('42; while (1) { with ({}) { 0; break; } }'))
assertEquals(0, eval('42; while (1) { try { 0; break } catch(e) {666} }'))
assertEquals(0, eval(
    '42; while (1) { try { 0; break } catch(e) {666} finally {666} }'))
assertEquals(0, eval(
    '42; while (1) { try { throw "" } catch(e) {666} finally {0; break} }'))
assertEquals(0, eval(
    '42; while (1) { try { throw "" } catch(e) {0; break} finally {666} }'))
assertEquals(0, eval(
    '42; while (1) { try { 666 } finally {0; break} }'));
assertEquals(0, eval(
    '42; while (1) { try { 666; break } finally {0; break} }'));
assertEquals(0, eval(
    '42; lab: try { 666; break lab } finally {0; break lab}'));
assertEquals(undefined, eval(
  'var b = 1; ' +
  'outer: while (1) { while (1) { if (b--) 42; else break outer; }; 666 }'));

assertUndef(eval('42; switch (0) { case 0: 1; if (true) break; }'));  // ES5: 1

assertUndef(eval('a: while(true) { do { 0 } while(false); switch(1) { case 0: 1; case 1: break a; }; 0 }'));
assertUndef(eval('a: while(true) { do { 0 } while(false); try {} finally { break a }; 0 }'));
assertUndef(eval('a: while(true) { b: while(true) { 0; break b; }; switch(1) { case 1: break a; }; 2 }'));
assertUndef(eval('a: while(true) { b: while(true) { 0; break b; }; while (true) { break a; }; 2 }'));
assertUndef(eval('while (true) { 20; a:{ break a; }  with ({}) break; 30; }'));
assertEquals(42, eval('a: while(true) { switch(0) { case 0: 42; case 1: break a; }; 33 }'));

assertUndef(eval(
  'for (var i = 0; i < 2; ++i) { if (i) { try {} finally { break; } } 0; }'
));
assertUndef(eval(
  'for (var i = 0; i < 2; ++i) { if (i) { try {} finally { continue; } } 0; }'
));



////////////////////////////////////////////////////////////////////////////////
//
// The following are copied from webkit/eval-throw-return and adapted.

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

assertEquals(1, eval("1;"));
assertUndef(eval("1; try { foo = [2,3,throwFunc(), 4]; } catch (e){}"));
assertUndef(eval("1; try { 2; throw ''; } catch (e){}"));
assertUndef(eval("1; try { 2; throwFunc(); } catch (e){}"));
assertEquals(3, eval("1; try { 2; throwFunc(); } catch (e){3;} finally {}"));
assertEquals(3, eval("1; try { 2; throwFunc(); } catch (e){3;} finally {4;}"));
assertUndef(eval("function blah() { 1; }; blah();"));
assertUndef(eval("var x = 1;"));
assertEquals(1, eval("if (true) { 1; } else { 2; }"));
assertEquals(2, eval("if (false) { 1; } else { 2; }"));
assertUndef(eval("try{1; if (true) { 2; throw ''; } else { 2; }} catch(e){}"));
assertEquals(2, eval("1; var i = 0; do { ++i; 2; } while(i!=1);"));
assertUndef(eval(
    "try{1; var i = 0; do { ++i; 2; throw ''; } while (i!=1);} catch(e){}"));
assertUndef(eval("1; try{2; throwOnReturn();} catch(e){}"));
assertUndef(eval("1; twoFunc();"));
assertEquals(2, eval("1; with ( { a: 0 } ) { 2; }"));
