'use strict';

const { assertTypeScript } = require('internal/util');
assertTypeScript();

const {
  stripTypeScriptTypes,
} = require('internal/parser/typescript');

module.exports = {
  stripTypeScriptTypes,
};
