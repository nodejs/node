// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
- Duplicate parameters are allowed for
  - non-arrow functions which are not conscise methods *and*
  - when the parameter list is simple *and*
  - we're in sloppy mode (incl. the function doesn't declare itself strict).
*/

function assertDuplicateParametersError(code) {
  caught = false;
  try {
    eval(code);
  } catch(e) {
    // Assert that it's the duplicate parameters error, and e.g,. not a syntax
    // error because of a typo in the test.
    assertTrue(e.message.startsWith("Duplicate parameter name not allowed"));
    caught = true;
  } finally {
    assertTrue(caught);
  }
}

FunctionType = {
  NORMAL : 0,
  ARROW : 1,
  METHOD : 2,
  CONCISE_METHOD : 3,
};

Laziness = {
  EAGER : 0,
  LAZY_BOUNDARY : 1,
  LAZY : 2
};

Strictness = {
  SLOPPY : 0,
  STRICT : 1,
  STRICT_FUNCTION : 2
};

function testHelper(type, strict, lazy, duplicate_params_string, ok) {
  code = ""
  strict_inside = "";
  if (strict == Strictness.STRICT) {
    code = "'use strict'; ";
  } else if (strict == Strictness.STRICT_FUNCTION) {
    strict_inside = "'use strict'; ";
  } else {
    assertEquals(strict, Strictness.SLOPPY);
  }

  if (type == FunctionType.NORMAL) {
    if (lazy == Laziness.EAGER) {
      code += "(function foo(" + duplicate_params_string + ") { " + strict_inside + "})";
    } else if (lazy == Laziness.LAZY_BOUNDARY) {
      code += "function foo(" + duplicate_params_string + ") { " + strict_inside + "}";
    } else if (lazy == Laziness.LAZY) {
      code += 'function lazy() { function foo(' + duplicate_params_string +
          ') { ' + strict_inside + '} }';
    } else {
      assertUnreachable();
    }
  } else if (type == FunctionType.ARROW) {
    if (lazy == Laziness.EAGER) {
      // Force an arrow function to be eager by making its body trivial.
      assertEquals(strict, Strictness.SLOPPY);
      code += "(" + duplicate_params_string + ") => 1";
    } else if (lazy == Laziness.LAZY_BOUNDARY) {
      // Duplicate parameters in non-simple parameter lists are not recognized
      // at the laziness boundary, when the lazy function is an arrow
      // function. Hack around this by calling the function. See
      // https://bugs.chromium.org/p/v8/issues/detail?id=6108.
      let simple = /^[a-z, ]*$/.test(duplicate_params_string);
      if (simple) {
        code += "(" + duplicate_params_string + ") => { " + strict_inside + "};";
      } else {
        code += "let foo = (" + duplicate_params_string + ") => { " + strict_inside + "}; foo();";
      }
    } else if (lazy == Laziness.LAZY) {
      // PreParser cannot detect duplicates in arrow function parameters. When
      // parsing the parameter list, it doesn't know it's an arrow function
      // parameter list, so it just discards the identifiers, and cannot do the
      // check any more when it sees the arrow. Work around this by calling the
      // function which forces parsing it.
      code += 'function lazy() { (' + duplicate_params_string + ') => { ' +
          strict_inside + '} } lazy();';
    } else {
      assertUnreachable();
    }
  } else if (type == FunctionType.METHOD) {
    code += "var o = {";
    if (lazy == Laziness.EAGER) {
      code += "foo : (function(" + duplicate_params_string + ") { " + strict_inside + "})";
    } else if (lazy == Laziness.LAZY_BOUNDARY) {
      code += "foo : function(" + duplicate_params_string + ") { " + strict_inside + "}";
    } else if (lazy == Laziness.LAZY) {
      code += 'lazy: function() { function foo(' + duplicate_params_string +
          ') { ' + strict_inside + '} }';
    } else {
      assertUnreachable();
    }
    code += "};";
  } else if (type == FunctionType.CONCISE_METHOD) {
    if (lazy == Laziness.LAZY_BOUNDARY) {
      code += "var o = { foo(" + duplicate_params_string + ") { " + strict_inside + "} };";
    } else if (lazy == Laziness.LAZY) {
      code += 'function lazy() { var o = { foo(' + duplicate_params_string +
          ') { ' + strict_inside + '} }; }';
    } else {
      assertUnreachable();
    }
  } else {
    assertUnreachable();
  }

  if (ok) {
    assertDoesNotThrow(code);
  } else {
    assertDuplicateParametersError(code);
  }
}

function test(type, strict, lazy, ok_if_param_list_simple) {
  // Simple duplicate params.
  testHelper(type, strict, lazy, "a, dup, dup, b", ok_if_param_list_simple)

  if (strict != Strictness.STRICT_FUNCTION) {
    // Generate test cases where the duplicate parameter occurs because of
    // destructuring or the rest parameter. That is always an error: duplicate
    // parameters are only allowed in simple parameter lists. These tests are
    // not possible if a function declares itself strict, since non-simple
    // parameters are not allowed then.
    testHelper(type, strict, lazy, "a, [dup], dup, b", false);
    testHelper(type, strict, lazy, "a, dup, {b: dup}, c", false);
    testHelper(type, strict, lazy, "a, {dup}, [dup], b", false);
    testHelper(type, strict, lazy, "a, dup, ...dup", false);
    testHelper(type, strict, lazy, "a, dup, dup, ...rest", false);
    testHelper(type, strict, lazy, "a, dup, dup, b = 1", false);
  }
}

// No duplicate parameters allowed for arrow functions even in sloppy mode.
test(FunctionType.ARROW, Strictness.SLOPPY, Laziness.EAGER, false);
test(FunctionType.ARROW, Strictness.SLOPPY, Laziness.LAZY_BOUNDARY, false);
test(FunctionType.ARROW, Strictness.SLOPPY, Laziness.LAZY, false);

// Duplicate parameters allowed for normal functions in sloppy mode.
test(FunctionType.NORMAL, Strictness.SLOPPY, Laziness.EAGER, true);
test(FunctionType.NORMAL, Strictness.SLOPPY, Laziness.LAZY_BOUNDARY, true);
test(FunctionType.NORMAL, Strictness.SLOPPY, Laziness.LAZY, true);

test(FunctionType.NORMAL, Strictness.STRICT, Laziness.EAGER, false);
test(FunctionType.NORMAL, Strictness.STRICT, Laziness.LAZY_BOUNDARY, false);
test(FunctionType.NORMAL, Strictness.STRICT, Laziness.LAZY, false);

test(FunctionType.NORMAL, Strictness.STRICT_FUNCTION, Laziness.EAGER, false);
test(FunctionType.NORMAL, Strictness.STRICT_FUNCTION, Laziness.LAZY_BOUNDARY, false);
test(FunctionType.NORMAL, Strictness.STRICT_FUNCTION, Laziness.LAZY, false);

// No duplicate parameters allowed for conscise methods even in sloppy mode.
test(FunctionType.CONCISE_METHOD, Strictness.SLOPPY, Laziness.LAZY_BOUNDARY, false);
test(FunctionType.CONCISE_METHOD, Strictness.SLOPPY, Laziness.LAZY, false);

// But non-concise methods follow the rules for normal funcs.
test(FunctionType.METHOD, Strictness.SLOPPY, Laziness.EAGER, true);
test(FunctionType.METHOD, Strictness.SLOPPY, Laziness.LAZY_BOUNDARY, true);
test(FunctionType.METHOD, Strictness.SLOPPY, Laziness.LAZY, true);

test(FunctionType.METHOD, Strictness.STRICT, Laziness.EAGER, false);
test(FunctionType.METHOD, Strictness.STRICT, Laziness.LAZY_BOUNDARY, false);
test(FunctionType.METHOD, Strictness.STRICT, Laziness.LAZY, false);

test(FunctionType.METHOD, Strictness.STRICT_FUNCTION, Laziness.EAGER, false);
test(FunctionType.METHOD, Strictness.STRICT_FUNCTION, Laziness.LAZY_BOUNDARY, false);
test(FunctionType.METHOD, Strictness.STRICT_FUNCTION, Laziness.LAZY, false);
