#!/usr/bin/env node
'use strict';
const test = require('node:test');

// This file has a shebang line. Without allowHashBang: true,
// acorn would fail to parse it and statement coverage would
// degrade to 0 total statements.

function greet(name) {
  return 'Hello, ' + name;
}

// Uncalled — should show as uncovered statements
function farewell(name) {
  return 'Goodbye, ' + name;
}

test('shebang file coverage', () => {
  greet('world');
});
