// Flags: --expose-internals
'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const { internalBinding } = require('internal/test/binding');
const { filterOwnProperties } = require('internal/util');
const { internalModuleReadJSON } = internalBinding('fs');
const { readFileSync } = require('fs');
const { strictEqual, deepStrictEqual } = require('assert');

{
  strictEqual(internalModuleReadJSON('nosuchfile')[0], undefined);
}
{
  strictEqual(internalModuleReadJSON(fixtures.path('empty.txt'))[0], '');
}
{
  strictEqual(internalModuleReadJSON(fixtures.path('empty-with-bom.txt'))[0], '');
}
{
  const filename = fixtures.path('require-bin/package.json');
  const returnValue = JSON.parse(internalModuleReadJSON(filename)[0]);
  const file = JSON.parse(readFileSync(filename, 'utf-8'));
  const expectedValue = filterOwnProperties(file, ['name', 'main', 'exports', 'imports', 'type']);
  deepStrictEqual({
    __proto__: null,
    ...returnValue,
  }, expectedValue);
}
