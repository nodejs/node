// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-explicit-tailcalls
// Flags: --harmony-do-expressions
"use strict";

var SyntaxErrorTests = [
  { msg: "Unexpected expression inside tail call",
    tests: [
      { src: `()=>{ return continue  foo ; }`,
        err: `                       ^^^`,
      },
      { src: `()=>{ return  continue 42 ; }`,
        err: `                       ^^`,
      },
      { src: `()=>{ return  continue   new foo ()  ; }`,
        err: `                         ^^^^^^^^^^`,
      },
      { src: `()=>{ loop: return  continue  loop ; }`,
        err: `                              ^^^^`,
      },
      { src: `class A { foo() { return  continue   super.x ; } }`,
        err: `                                     ^^^^^^^`,
      },
      { src: `()=>{ return  continue  this  ; }`,
        err: `                        ^^^^`,
      },
      { src: `()=>{ return  continue class A {} ; }`,
        err: `                       ^^^^^^^^^^`,
      },
      { src: `()=>{ return  continue class A extends B {} ; }`,
        err: `                       ^^^^^^^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return  continue function A() { } ; }`,
        err: `                       ^^^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return  continue { a: b, c: d} ; }`,
        err: `                       ^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return  continue function* Gen() { yield 1; } ; }`,
        err: `                       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^`,
      },
      { src: `function A() { return  continue new.target ; }`,
        err: `                                ^^^^^^^^^^`,
      },
      { src: `()=>{ return  continue () ; }`,
        err: `                       ^^`,
      },
      { src: `()=>{ return  continue ( 42 ) ; }`,
        err: `                       ^^^^^^`,
      },
      { src: "()=>{ return continue `123 ${foo} 34lk` ;  }",
        err: `                      ^^^^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return continue do { x ? foo() : bar() ; } }`,
        err: `                      ^^^^^^^^^^^^^^^^^^^^^^^^^^`,
      },
    ],
  },
  { msg: "Tail call expression is not allowed here",
    tests: [
      { src: `()=>{ return  continue continue continue b()  ; }`,
        err: `                                ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return  continue ( continue b() ) ; }`,
        err: `                         ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return   continue  f()   - a ; }`,
        err: `               ^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return b + continue   f()  ; }`,
        err: `                 ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return 1, 2, 3,   continue  f() , 4  ; }`,
        err: `                        ^^^^^^^^^^^^^`,
      },
      { src: `()=>{ var x =  continue  f ( ) ; }`,
        err: `               ^^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return   continue f () ? 1 : 2 ; }`,
        err: `               ^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return (1, 2, 3, continue f()), 4; }`,
        err: `                       ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return [1, 2, continue f() ] ;  }`,
        err: `                    ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return [1, 2, ... continue f() ] ;  }`,
        err: `                        ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return [1, 2, continue f(), 3 ] ;  }`,
        err: `                    ^^^^^^^^^^^^`,
      },
      { src: "()=>{ return `123 ${a} ${ continue foo ( ) } 34lk` ;  }",
        err: `                          ^^^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return g( 1, 2, continue f() ); }`,
        err: `                      ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return continue f() || a; }`,
        err: `             ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return a || b || c || continue f() || d; }`,
        err: `                            ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return a && b && c && continue f() && d; }`,
        err: `                            ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return a && b || c && continue f() ? d : e; }`,
        err: `                            ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return a ? b : c && continue f() && d || e; }`,
        err: `                          ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return continue foo() instanceof bar ; }`,
        err: `             ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return bar instanceof continue foo() ; }`,
        err: `                            ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return continue foo() in bar ; }`,
        err: `             ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return bar in continue foo() ; }`,
        err: `                    ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ function* G() { yield continue foo(); } }`,
        err: `                            ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ (1, 2, 3, continue f() ) => {} }`,
        err: `                ^^^^^^^^^^^^`,
      },
      { src: `()=>{ (... continue f()) => {}  }`,
        err: `           ^^^^^^^^^^^^`,
      },
      { src: `()=>{ (a, b, c, ... continue f() ) => {}  }`,
        err: `                    ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return a <= continue f(); }`,
        err: `                  ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return b > continue f(); }`,
        err: `                 ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return a << continue f(); }`,
        err: `                  ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return b >> continue f(); }`,
        err: `                  ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return c >>> continue f(); }`,
        err: `                   ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return continue f() = a ; }`,
        err: `             ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return a = continue f() ; }`,
        err: `                 ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return a += continue f(); }`,
        err: `                  ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return a ** continue f() ; }`,
        err: `                  ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return delete continue foo() ; }`,
        err: `                    ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ typeof continue foo()  ; }`,
        err: `             ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return ~ continue foo() ; }`,
        err: `               ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return void  continue foo() ; }`,
        err: `                   ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return !continue foo() ; }`,
        err: `              ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return -continue foo() ; }`,
        err: `              ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return +continue foo() ; }`,
        err: `              ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return ++ continue f( ) ; }`,
        err: `                ^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return continue f()  ++; }`,
        err: `             ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return continue f() --; }`,
        err: `             ^^^^^^^^^^^^`,
      },
      { src: `()=>{ return (continue foo()) () ;  }`,
        err: `              ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ for (var i = continue foo(); i < 10; i++) bar(); }`,
        err: `                   ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ for (var i = 0; i < continue foo(); i++) bar(); }`,
        err: `                          ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ for (var i = 0; i < 10; continue foo()) bar(); }`,
        err: `                              ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ if (continue foo()) bar(); }`,
        err: `          ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ while (continue foo()) bar(); }`,
        err: `             ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ do { smth; } while (continue foo()) ; }`,
        err: `                          ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ throw continue foo() ; }`,
        err: `            ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ switch (continue foo()) { case 1: break; } ; }`,
        err: `              ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ let x = continue foo() }`,
        err: `              ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ const c = continue  foo() }`,
        err: `                ^^^^^^^^^^^^^^^`,
      },
      { src: `class A {}; class B extends A { constructor() { return continue foo () ; } }`,
        err: `                                                       ^^^^^^^^^^^^^^^`,
      },
      { src: `class A extends continue f () {}; }`,
        err: `                ^^^^^^^^^^^^^`,
      },
    ],
  },
  { msg: "Tail call expression in try block",
    tests: [
      { src: `()=>{ try { return  continue   f ( ) ; } catch(e) {} }`,
        err: `                    ^^^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ try { try { smth; } catch(e) { return  continue  f( ) ; } }`,
        err: `                                             ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ try { try { smth; } catch(e) { return  continue  f( ) ; } } finally { bla; } }`,
        err: `                                             ^^^^^^^^^^^^^^`,
      },
    ],
  },
  { msg: "Tail call expression in catch block when finally block is also present",
    tests: [
      { src: `()=>{ try { smth; } catch(e) { return  continue   f ( ) ; } finally { blah; } }`,
        err: `                                       ^^^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ try { smth; } catch(e) { try { smth; } catch (e) { return  continue   f ( ) ; } } finally { blah; } }`,
        err: `                                                                 ^^^^^^^^^^^^^^^^`,
      },
    ],
  },
  { msg: "Tail call expression in for-in/of body",
    tests: [
      { src: `()=>{ for (var v in {a:0}) { return continue  foo () ; } }`,
        err: `                                    ^^^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ for (var v of [1, 2, 3]) { return continue  foo () ; } }`,
        err: `                                        ^^^^^^^^^^^^^^^^`,
      },
    ],
  },
  { msg: "Tail call of a direct eval is not allowed",
    tests: [
      { src: `()=>{ return  continue  eval(" foo () " )  ; }`,
        err: `                        ^^^^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return  a || continue  eval("", 1, 2)  ; }`,
        err: `                             ^^^^^^^^^^^^^^`,
      },
      { src: `()=>{ return  a, continue  eval  ( )  ; }`,
        err: `                           ^^^^^^^^^`,
      },
      { src: `()=> a, continue  eval  ( )  ; `,
        err: `                  ^^^^^^^^^`,
      },
      { src: `()=> a || continue  eval  (' ' )  ; `,
        err: `                    ^^^^^^^^^^^^`,
      },
    ],
  },
  { msg: "Undefined label 'foo'",
    tests: [
      { src: `()=>{ continue  foo () ; }`,
        err: `                ^^^`,
      },
    ],
  },
];


// Should parse successfully.
var NoErrorTests = [
  `()=>{ return continue  a.b.c.foo () ; }`,
  `()=>{ return continue  a().b.c().d.foo () ; }`,
  `()=>{ return continue  foo (1)(2)(3, 4) ; }`,
  `()=>{ return continue (0, eval)(); }`,
  `()=>{ return ( continue b() ) ; }`,
  "()=>{ return continue bar`ab cd ef` ; }",
  "()=>{ return continue bar`ab ${cd} ef` ; }",
  `()=>{ return a || continue f() ; }`,
  `()=>{ return a && continue f() ; }`,
  `()=>{ return a , continue f() ; }`,
  `()=>{ function* G() { return continue foo(); } }`,
  `()=>{ class A { foo() { return continue super.f() ; } } }`,
  `()=>{ function B() { return continue new.target() ; } }`,
  `()=>{ return continue do { x ? foo() : bar() ; }() }`,
  `()=>{ return continue (do { x ? foo() : bar() ; })() }`,
  `()=>{ return do { 1, continue foo() } }`,
  `()=>{ return do { x ? continue foo() : y } }`,
  `()=>{ return a || (b && continue c()); }`,
  `()=>{ return a && (b || continue c()); }`,
  `()=>{ return a || (b ? c : continue d()); }`,
  `()=>{ return 1, 2, 3, a || (b ? c : continue d()); }`,
  `()=> continue  (foo ()) ;`,
  `()=> a || continue  foo () ;`,
  `()=> a && continue  foo () ;`,
  `()=> a ? continue  foo () : b;`,
];


(function() {
  for (var test_set of SyntaxErrorTests) {
    var expected_message = "SyntaxError: " + test_set.msg;
    for (var test of test_set.tests) {
      var passed = true;
      var e = null;
      try {
        eval(test.src);
      } catch (ee) {
        e = ee;
      }
      print("=======================================");
      print("Expected | " + expected_message);
      print("Source   | " + test.src);
      print("         | " + test.err);

      if (e === null) {
        print("FAILED");
        throw new Error("SyntaxError was not thrown");
      }

      var details = %GetExceptionDetails(e);
      if (details.start_pos == undefined ||
          details.end_pos == undefined) {
        throw new Error("Bad message object returned");
      }
      var underline = " ".repeat(details.start_pos) +
                      "^".repeat(details.end_pos - details.start_pos);
      var passed = expected_message === e.toString() &&
                   test.err === underline;

      if (passed) {
        print("PASSED");
        print();
      } else {
        print("---------------------------------------");
        print("Actual   | " + e);
        print("Source   | " + test.src);
        print("         | " + underline);
        print("FAILED");
        throw new Error("Test failed");
      }
    }
  }
})();


(function() {
  for (var src of NoErrorTests) {
    print("=======================================");
    print("Source   | " + src);
    src = `"use strict"; ` + src;
    Realm.eval(0, src);
    print("PASSED");
    print();
  }
})();
