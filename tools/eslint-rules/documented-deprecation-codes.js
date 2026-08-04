'use strict';

const fs = require('fs');
const path = require('path');
const { isDefiningDeprecation } = require('./rules-utils.js');

const patternToMatch = /^DEP\d+$/;

const mdFile = 'doc/api/deprecations.md';
const doc = fs.readFileSync(path.resolve(__dirname, '../..', mdFile), 'utf8');

function isInDoc(code) {
  return doc.includes(`### ${code}:`);
}

function getDeprecationCode(node) {
  return node.expression.arguments[2].value;
}

module.exports = {
  create: function(context) {
    return {
      ExpressionStatement: function(node) {
        if (!isDefiningDeprecation(node) || !getDeprecationCode(node)) return;
        const code = getDeprecationCode(node);
        if (!patternToMatch.test(code)) {
          const message = `"${code}" does not match the expected pattern`;
          context.report({ node, message });
        }
        if (!isInDoc(code)) {
          const message = `"${code}" is not documented in ${mdFile}`;
          context.report({ node, message });
        }
      },
    };
  },
};
