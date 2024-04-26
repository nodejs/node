'use strict';

require('../common');
const assert = require('assert');

const jsonFiles = [
  // [file, [throwWithTypeJSON, throwWithTypeJSONC]]
  ['../fixtures/es-modules/json-with-comments/regular.json', [false, false]],
  ['../fixtures/es-modules/json-with-comments/comments.json', [true, false]],
  ['../fixtures/es-modules/json-with-comments/comments.jsonc', [true, false]],
];

for (const [file, [throwWithTypeJSON, throwWithTypeJSONC]] of jsonFiles) {

  assert[throwWithTypeJSON ? 'rejects' : 'doesNotReject'](async () => {
    await import(file, { with: { type: 'json' } });
  });

  assert[throwWithTypeJSONC ? 'rejects' : 'doesNotReject'](async () => {
    await import(file, { with: { type: 'jsonc' } });
  });
}
