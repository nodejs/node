'use strict';

const fs = require('fs');
const path = require('path');
const { isDefiningError } = require('./rules-utils.js');

// Load the errors documentation file once
const docPath = path.resolve(__dirname, '../../doc/api/errors.md');
const doc = fs.readFileSync(docPath, 'utf8');

// Helper function to parse errors documentation and return a Map
function getErrorsInDoc() {
  const lines = doc.split('\n');
  let currentHeader;
  const errors = new Map();
  const codePattern = /^### `([^`]+)`$/;
  const anchorPattern = /^<a id="([^"]+)"><\/a>$/;

  function parse(line, legacy) {
    const error = { legacy };
    let code;

    const codeMatch = line.match(codePattern);
    if (codeMatch) {
      error.header = true;
      code = codeMatch[1];
    }

    const anchorMatch = line.match(anchorPattern);
    if (anchorMatch) {
      error.anchor = true;
      code ??= anchorMatch[1];
    }

    if (!code) return;

    // If the code already exists in the Map, merge the new error data
    errors.set(code, {
      ...errors.get(code),
      ...error,
    });
  }

  for (const line of lines) {
    if (line.startsWith('## ')) currentHeader = line.substring(3);
    if (currentHeader === 'Node.js error codes') parse(line, false);
    if (currentHeader === 'Legacy Node.js error codes') parse(line, true);
  }

  return errors;
}

// Main rule export
module.exports = {
  create(context) {
    const errors = getErrorsInDoc();
    return {
      ExpressionStatement(node) {
        if (!isDefiningError(node)) return;

        const code = node.expression.arguments?.[0]?.value;
        if (!code) return;

        const err = errors.get(code); // Use Map's get method to retrieve the error

        if (!err || !err.header) {
          context.report({
            node,
            message: `"${code}" is not documented in doc/api/errors.md`,
          });
          if (!err) return;
        }

        if (!err.anchor) {
          context.report({
            node,
            message: `doc/api/errors.md does not have an anchor for "${code}"`,
          });
        }

        if (err.legacy) {
          context.report({
            node,
            message: `"${code}" is marked as legacy, yet it is used in lib/.`,
          });
        }
      },
    };
  },
};
