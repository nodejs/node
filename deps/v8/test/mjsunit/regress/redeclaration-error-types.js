// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function doTest(scripts, expectedError) {
  var realm = Realm.create();

  for (var i = 0; i < scripts.length - 1; i++) {
    Realm.eval(realm, scripts[i]);
  }
  assertThrows(function() {
    Realm.eval(realm, scripts[scripts.length - 1]);
  }, Realm.eval(realm, expectedError));

  Realm.dispose(realm);
}

var tests = [
  {
    // ES#sec-globaldeclarationinstantiation 5.a:
    // If envRec.HasVarDeclaration(name) is true, throw a SyntaxError
    // exception.
    scripts: [
      "var a;",
      "let a;",
    ],
    expectedError: "SyntaxError",
  },
  {
    // ES#sec-globaldeclarationinstantiation 6.a:
    // If envRec.HasLexicalDeclaration(name) is true, throw a SyntaxError
    // exception.
    scripts: [
      "let a;",
      "var a;",
    ],
    expectedError: "SyntaxError",
  },
  {
    // ES#sec-globaldeclarationinstantiation 5.b:
    // If envRec.HasLexicalDeclaration(name) is true, throw a SyntaxError
    // exception.
    scripts: [
      "let a;",
      "let a;",
    ],
    expectedError: "SyntaxError",
  },
  {
    // ES#sec-evaldeclarationinstantiation 5.a.i.1:
    // If varEnvRec.HasLexicalDeclaration(name) is true, throw a SyntaxError
    // exception.
    scripts: [
      'let a; eval("var a;");',
    ],
    expectedError: "SyntaxError",
  },
  {
    // ES#sec-evaldeclarationinstantiation 5.a.i.1:
    // If varEnvRec.HasLexicalDeclaration(name) is true, throw a SyntaxError
    // exception.
    scripts: [
      'let a; eval("function a() {}");',
    ],
    expectedError: "SyntaxError",
  },
  {
    // ES#sec-evaldeclarationinstantiation 5.d.ii.2.a.i:
    // Throw a SyntaxError exception.
    scripts: [
      '(function() { let a; eval("var a;"); })();',
    ],
    expectedError: "SyntaxError",
  },
  {
    // ES#sec-evaldeclarationinstantiation 5.d.ii.2.a.i:
    // Throw a SyntaxError exception.
    scripts: [
      '(function() { let a; eval("function a() {}"); })();',
    ],
    expectedError: "SyntaxError",
  },
  {
    // ES#sec-globaldeclarationinstantiation 5.d:
    // If hasRestrictedGlobal is true, throw a SyntaxError exception.
    scripts: [
      'let NaN;',
    ],
    expectedError: "SyntaxError",
  },
  {
    // ES#sec-globaldeclarationinstantiation 5.d:
    // If hasRestrictedGlobal is true, throw a SyntaxError exception.
    scripts: [
      'function NaN() {}',
    ],
    expectedError: "SyntaxError",
  },

  {
    // ES#sec-evaldeclarationinstantiation 8.a.iv.1.b:
    // If fnDefinable is false, throw a TypeError exception.
    scripts: [
      'eval("function NaN() {}");',
    ],
    expectedError: "TypeError",
  },
  {
    // ES#sec-evaldeclarationinstantiation 8.a.iv.1.b:
    // If fnDefinable is false, throw a TypeError exception.
    scripts: [
      `
        let a;
        try {
          eval("function a() {}");
        } catch (e) {}
        eval("function NaN() {}");
      `,
    ],
    expectedError: "TypeError",
  },
  {
    // ES#sec-evaldeclarationinstantiation 8.a.iv.1.b:
    // If fnDefinable is false, throw a TypeError exception.
    scripts: [
      `
        eval("
          function f() {
            function b() {
              (0, eval)('function NaN() {}');
            }
            b();
          }
          f();
        ");
      `.replace(/"/g, '`'),
    ],
    expectedError: "TypeError",
  },
];

tests.forEach(function(test) {
  doTest(test.scripts, test.expectedError);
});
