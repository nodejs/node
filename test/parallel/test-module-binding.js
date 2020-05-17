// Flags: --expose-internals
'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const { internalBinding } = require('internal/test/binding');
const { internalModuleReadJSON } = internalBinding('fs');
const { readFileSync } = require('fs');
const { strictEqual } = require('assert');
{
  const result = internalModuleReadJSON('nosuchfile');
  strictEqual(result.string, undefined);
  strictEqual(result.containsKeys, undefined);
}
{
  const result = internalModuleReadJSON(fixtures.path('empty.txt'));
  strictEqual(result.string, '');
  strictEqual(result.containsKeys, false);
}
{
  const result = internalModuleReadJSON(fixtures.path('empty.txt'));
  strictEqual(result.string, '');
  strictEqual(result.containsKeys, false);
}
{
  const result = internalModuleReadJSON(fixtures.path('empty-with-bom.txt'));
  strictEqual(result.string, '');
  strictEqual(result.containsKeys, false);
}
{
  const filename = fixtures.path('require-bin/package.json');
  const result = internalModuleReadJSON(filename);
  strictEqual(result.string, readFileSync(filename, 'utf8'));
  strictEqual(result.containsKeys, true);
}
