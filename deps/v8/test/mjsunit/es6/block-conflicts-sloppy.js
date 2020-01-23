// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test for conflicting variable bindings.
// Stress-testing this test is very slow and provides no useful coverage.
// Flags: --nostress-opt --noalways-opt

function CheckException(e) {
  var string = e.toString();
  assertTrue(string.indexOf("has already been declared") >= 0 ||
             string.indexOf("redeclaration") >= 0);
  return 'Conflict';
}


function TestGlobal(s,e) {
  try {
    return eval(s + e);
  } catch (x) {
    return CheckException(x);
  }
}


function TestFunction(s,e) {
  try {
    return eval("(function(){" + s + " return " + e + "})")();
  } catch (x) {
    return CheckException(x);
  }
}


function TestBlock(s,e) {
  try {
    return eval("(function(){ {" + s + "} return " + e + "})")();
  } catch (x) {
    return CheckException(x);
  }
}

function TestAll(expected,s,opt_e) {
  var e = "";
  var msg = s;
  if (opt_e) { e = opt_e; msg += opt_e; }
  // TODO(littledan): Add tests using Realm.eval to ensure that global eval
  // works as expected.
  assertEquals(expected === 'LocalConflict' ? 'NoConflict' : expected,
      TestGlobal(s,e), "global:'" + msg + "'");
  assertEquals(expected === 'LocalConflict' ? 'NoConflict' : expected,
      TestFunction(s,e), "function:'" + msg + "'");
  assertEquals(expected === 'LocalConflict' ? 'Conflict' : expected,
      TestBlock(s,e), "block:'" + msg + "'");
}


function TestConflict(s) {
  TestAll('Conflict', s);
  TestAll('Conflict', 'eval("' + s + '");');
}

function TestNoConflict(s) {
  TestAll('NoConflict', s, "'NoConflict'");
  TestAll('NoConflict', 'eval("' + s + '");', "'NoConflict'");
}

function TestLocalConflict(s) {
  TestAll('LocalConflict', s, "'NoConflict'");
  TestAll('NoConflict', 'eval("' + s + '");', "'NoConflict'");
}

var letbinds = [ "let x;",
                 "let x = 0;",
                 "let x = undefined;",
                 "let x = function() {};",
                 "let x, y;",
                 "let y, x;",
                 "const x = 0;",
                 "const x = undefined;",
                 "const x = function() {};",
                 "const x = 2, y = 3;",
                 "const y = 4, x = 5;",
                 "class x { }",
                 ];
function forCompatible(bind) {
  return !bind.startsWith('class');
}
var varbinds = [ "var x;",
                 "var x = 0;",
                 "var x = undefined;",
                 "var x = function() {};",
                 "var x, y;",
                 "var y, x;",
                 ];
var funbind = "function x() {}";

for (var l = 0; l < letbinds.length; ++l) {
  // Test conflicting let/var bindings.
  for (var v = 0; v < varbinds.length; ++v) {
    // Same level.
    TestConflict(letbinds[l] + varbinds[v]);
    TestConflict(varbinds[v] + letbinds[l]);
    // Different level.
    TestConflict(letbinds[l] + '{' + varbinds[v] + '}');
    TestConflict('{' + varbinds[v] +'}' + letbinds[l]);
    TestNoConflict(varbinds[v] + '{' + letbinds[l] + '}');
    TestNoConflict('{' + letbinds[l] + '}' + varbinds[v]);
    // For loop.
    if (forCompatible(letbinds[l])) {
      TestConflict('for (' + letbinds[l] + '0;) {' + varbinds[v] + '}');
    }
    TestNoConflict('for (' + varbinds[v] + '0;) {' + letbinds[l] + '}');
  }

  // Test conflicting let/let bindings.
  for (var k = 0; k < letbinds.length; ++k) {
    // Same level.
    TestConflict(letbinds[l] + letbinds[k]);
    TestConflict(letbinds[k] + letbinds[l]);
    // Different level.
    TestNoConflict(letbinds[l] + '{ ' + letbinds[k] + '}');
    TestNoConflict('{' + letbinds[k] +'} ' + letbinds[l]);
    // For loop.
    if (forCompatible(letbinds[l])) {
      TestNoConflict('for (' + letbinds[l] + '0;) {' + letbinds[k] + '}');
    }
    if (forCompatible(letbinds[k])) {
      TestNoConflict('for (' + letbinds[k] + '0;) {' + letbinds[l] + '}');
    }
  }

  // Test conflicting function/let bindings.
  // Same level.
  TestConflict(letbinds[l] + funbind);
  TestConflict(funbind + letbinds[l]);
  // Different level.
  TestNoConflict(letbinds[l] + '{' + funbind + '}');
  TestNoConflict('{' + funbind + '}' + letbinds[l]);
  TestNoConflict(funbind + '{' + letbinds[l] + '}');
  TestNoConflict('{' + letbinds[l] + '}' + funbind);
  // For loop.
  if (forCompatible(letbinds[l])) {
    TestNoConflict('for (' + letbinds[l] + '0;) {' + funbind + '}');
  }

  // Test conflicting parameter/let bindings.
  TestConflict('(function(x) {' + letbinds[l] + '})();');
}

// Test conflicting function/var bindings.
for (var v = 0; v < varbinds.length; ++v) {
  // Same level.
  TestLocalConflict(varbinds[v] + funbind);
  TestLocalConflict(funbind + varbinds[v]);
  // Different level.
  TestLocalConflict(funbind + '{' + varbinds[v] + '}');
  TestLocalConflict('{' + varbinds[v] +'}' + funbind);
  TestNoConflict(varbinds[v] + '{' + funbind + '}');
  TestNoConflict('{' + funbind + '}' + varbinds[v]);
  // For loop.
  TestNoConflict('for (' + varbinds[v] + '0;) {' + funbind + '}');
}

// Test conflicting catch/var bindings.
for (var v = 0; v < varbinds.length; ++v) {
  TestNoConflict('try {} catch(x) {' + varbinds[v] + '}');
}

// Test conflicting parameter/var bindings.
for (var v = 0; v < varbinds.length; ++v) {
  TestNoConflict('(function (x) {' + varbinds[v] + '})();');
}

// Test conflicting parameter/function bindings.
TestNoConflict('(function (x) {' + funbind + '})();');
