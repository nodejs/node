// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// spec:
// https://tc39.es/ecma262/multipage/ordinary-and-exotic-objects-behaviours.html
// #sec-functiondeclarationinstantiation

// This regession focus on checking [var declared "arguments" should bind to
// "arguments exotic object" when has_simple_parameters_ is false and
// inner_scope has var declared "arguments"]


// according to ES#sec-functiondeclarationinstantiation step 8,
// hasParameterExpressions is false, and according to step 15-18,
// argumentsObjectNeeded is true, according to step 27,
// var declared arguments should bind to arguments exotic object
function no_parameters_and_non_lexical_arguments() {
  assertEquals(typeof arguments, 'object');
  var arguments;
}
no_parameters_and_non_lexical_arguments()

// according to ES#sec-functiondeclarationinstantiation step 8,
// hasParameterExpressions is false, and according to step 15-18,
// argumentsObjectNeeded is true, according to step 28,
// var declared arguments should bind to arguments exotic object
function destructuring_parameters_and_non_lexical_arguments([_]) {
  assertEquals(typeof arguments, 'object');
  var arguments;
}
destructuring_parameters_and_non_lexical_arguments([])

// according to ES#sec-functiondeclarationinstantiation step 8,
// hasParameterExpressions is false, and according to step 15-18,
// argumentsObjectNeeded is true, according to step 28,
// var declared arguments should bind to arguments exotic object
function rest_parameters_and_non_lexical_arguments(..._) {
  assertEquals(typeof arguments, 'object');
  var arguments;
}
rest_parameters_and_non_lexical_arguments()

// according to ES#sec-functiondeclarationinstantiation step 8,
// hasParameterExpressions is true, and according to step 15-18,
// argumentsObjectNeeded is true, according to step 28,
// var declared arguments should bind to arguments exotic object
function initializer_parameters_and_non_lexical_arguments(_ = 0) {
  assertEquals(typeof arguments, 'object');
  var arguments;
}
initializer_parameters_and_non_lexical_arguments()

// according to ES#sec-functiondeclarationinstantiation step 8,
// hasParameterExpressions is true, and according to step 15-18,
// and argumentsObjectNeeded is true, according to step 34, should
// throw because access to let declared arguments
function initializer_parameters_and_lexical_arguments(_ = 0) {
  return typeof arguments;
  let arguments;
}

assertThrows(initializer_parameters_and_lexical_arguments);

// according to ES#sec-functiondeclarationinstantiation step 8,
// hasParameterExpressions is false, and according to step 15-18,
// argumentsObjectNeeded is false, according to step 34,
// should throw because access to let declared arguments
function simple_parameters_and_lexical_arguments(_) {
  return typeof arguments;
  let arguments;
}

assertThrows(simple_parameters_and_lexical_arguments);
