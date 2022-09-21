'use strict';

const { internalBinding } = require('internal/test/binding');
const {
  builtinCategories: { canBeRequired }
} = internalBinding('builtins');

for (const key of canBeRequired) {
  require(`node:${key}`);
}
