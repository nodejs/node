'use strict';

const fs = require('fs');
const path = require('path');
const { isDefiningError } = require('./rules-utils.js');

const doc = fs.readFileSync(path.resolve(__dirname, '../../doc/api/errors.md'),
                            'utf8');

function isInDoc(code) {
  return doc.match(`### ${code}`) != null;
}

function includesAnchor(code) {
  return doc.match(`<a id="${code}"></a>`) != null;
}

function errorForNode(node) {
  return node.expression.arguments[0].value;
}

module.exports = {
  create: function(context) {
    return {
      ExpressionStatement: function(node) {
        if (!isDefiningError(node) || !errorForNode(node)) return;
        const code = errorForNode(node);
        if (!isInDoc(code)) {
          const message = `"${code}" is not documented in doc/api/errors.md`;
          context.report({ node, message });
        }
        if (!includesAnchor(code)) {
          const message =
            `doc/api/errors.md does not have an anchor for "${code}"`;
          context.report({ node, message });
        }
      }
    };
  }
};
