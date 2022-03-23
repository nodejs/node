'use strict';

const { internalBinding } = require('internal/test/binding');
const {
  moduleCategories: { canBeRequired }
} = internalBinding('native_module');
const modulesNeedingPrefix = [
  'test',
];

for (let key of canBeRequired) {
  if (modulesNeedingPrefix.includes(key))
    key = `node:${key}`;
  require(key);
}
