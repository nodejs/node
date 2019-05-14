'use strict';

const { internalBinding } = require('internal/test/binding');
const {
  moduleCategories: { canBeRequired }
} = internalBinding('native_module');

for (const key of canBeRequired) {
  require(key);
}
