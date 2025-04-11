// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-staging

// Note that 'await' and 'yield' are allowed as BindingIdentifiers.
const reservedWords =
    'break case catch class const continue debugger default delete do else enum export extends false finally for function if import in instanceof new null return super switch this throw true try typeof var void while with'
        .split(' ');

(async function AsyncLoops() {
  // Keywords disallowed.
  for (const rw of reservedWords) {
    assertThrows(
        () => Function(`async function() { for (await using ${rw} of []) {} }`),
        SyntaxError);
    assertThrows(
        () => Function(
            `async function() { for await (await using ${rw} of []) {} }`),
        SyntaxError);
  }
  // 'await' is a keyword inside async functions.
  assertThrows(
      () => Function(`async function() { for (await using await of []) {} }`),
      SyntaxError);
  assertThrows(
      () => Function(
          `async function() { for await (await using await of []) {} }`),
      SyntaxError);

  // Patterns disallowed.
  assertThrows(
      () => Function('async function() { for (await using {a} of []) {} }'),
      SyntaxError);
  assertThrows(
      () => Function('async function() { for (await using [a] of []) {} }'),
      SyntaxError);
  assertThrows(
      () =>
          Function('async function() { for await (await using {a} of []) {} }'),
      SyntaxError);
  assertThrows(
      () =>
          Function('async function() { for await (await using [a] of []) {} }'),
      SyntaxError);

  // Disallowed in sync contexts.
  assertThrows(() => Function('for (await x of []) {}'), SyntaxError);

  // The first 'of' is an identifier, second 'of' is the for-of.
  for (await using of of []) {}
  for await (await using of of []) {}
  // Other idents.
  for (await using static of[]) {}
  for (await using yield of []) {}
  for (await using get of []) {}
  for (await using set of []) {}
  for (await using using of[]) {}
  for (await using async of []) {}
  for (await using foo of []) {}
  for await (await using static of[]) {}
  for await (await using yield of []) {}
  for await (await using get of []) {}
  for await (await using set of []) {}
  for await (await using using of[]) {}
  for await (await using async of []) {}
  for await (await using foo of []) {}
})();

(function SyncLoops() {
  // Keywords disallowed.
  for (const rw of reservedWords) {
    assertThrows(() => Function(`for (using ${rw} of []) {}`), SyntaxError);
  }

  // Patterns disallowed.
  assertThrows(() => Function('for (using {a} of []) {}'), SyntaxError);

  // 'using[a]' is an member expression here lol.
  for (using[a] of []) {}
  // 'using' is an identifier here.
  for (using of[]) {}
  // 'using of' is a declaration here.
  const of = [[]];
  for (using of of [0]) {}
  // Other idents.
  for (using static of[]) {}
  for (using yield of []) {}
  for (using await of []) {}
  for (using get of []) {}
  for (using set of []) {}
  for (using using of[]) {}
  for (using async of []) {}
  for (using foo of []) {}
})();
