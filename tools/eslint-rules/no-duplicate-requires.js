/**
 * @fileoverview Ensure modules are not required twice at top level of a module
 * @author devsnek
 */
'use strict';

const { isTopLevelRequireCall, isString } = require('./rules-utils.js');

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = (context) => {
  if (context.parserOptions.sourceType === 'module') {
    return {};
  }

  function getRequiredModuleNameFromCall(node) {
    // Node has arguments and first argument is string
    if (node.arguments.length && isString(node.arguments[0])) {
      return node.arguments[0].value.trim();
    }

    return undefined;
  }

  const required = new Set();

  const rules = {
    CallExpression: (node) => {
      if (isTopLevelRequireCall(node)) {
        const moduleName = getRequiredModuleNameFromCall(node);
        if (moduleName === undefined) {
          return;
        }
        if (required.has(moduleName)) {
          context.report(
            node,
            '\'{{moduleName}}\' require is duplicated.',
            { moduleName }
          );
        } else {
          required.add(moduleName);
        }
      }
    },
  };

  return rules;
};
