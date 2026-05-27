'use strict';
// Top-level `using` is valid inside the CJS module wrapper (V8 wraps this
// in a function) but the ECMAScript spec forbids `using` at the top level
// of a Script. Since acorn parses raw source as sourceType:'script',
// this file triggers a parse error -> getStatements() returns null.
// Line, branch, and function coverage still work normally.
using resource = { [Symbol.dispose]() {} };
module.exports = { ok: true };
