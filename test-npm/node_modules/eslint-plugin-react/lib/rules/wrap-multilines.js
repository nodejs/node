/**
 * @fileoverview Prevent missing parentheses around multilines JSX
 * @author Yannick Croissant
 */
'use strict';

// ------------------------------------------------------------------------------
// Constants
// ------------------------------------------------------------------------------

var DEFAULTS = {
  declaration: true,
  assignment: true,
  return: true
};

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

module.exports = function(context) {

  var sourceCode = context.getSourceCode();

  function isParenthesised(node) {
    var previousToken = sourceCode.getTokenBefore(node);
    var nextToken = sourceCode.getTokenAfter(node);

    return previousToken && nextToken &&
      previousToken.value === '(' && previousToken.range[1] <= node.range[0] &&
      nextToken.value === ')' && nextToken.range[0] >= node.range[1];
  }

  function isMultilines(node) {
    return node.loc.start.line !== node.loc.end.line;
  }

  function check(node) {
    if (!node || node.type !== 'JSXElement') {
      return;
    }

    if (!isParenthesised(node) && isMultilines(node)) {
      context.report({
        node: node,
        message: 'Missing parentheses around multilines JSX',
        fix: function(fixer) {
          return fixer.replaceText(node, '(' + sourceCode.getText(node) + ')');
        }
      });
    }
  }

  function isEnabled(type) {
    var userOptions = context.options[0] || {};
    if (({}).hasOwnProperty.call(userOptions, type)) {
      return userOptions[type];
    }
    return DEFAULTS[type];
  }

  // --------------------------------------------------------------------------
  // Public
  // --------------------------------------------------------------------------

  return {

    VariableDeclarator: function(node) {
      if (isEnabled('declaration')) {
        check(node.init);
      }
    },

    AssignmentExpression: function(node) {
      if (isEnabled('assignment')) {
        check(node.right);
      }
    },

    ReturnStatement: function(node) {
      if (isEnabled('return')) {
        check(node.argument);
      }
    }
  };

};

module.exports.schema = [{
  type: 'object',
  properties: {
    declaration: {
      type: 'boolean'
    },
    assignment: {
      type: 'boolean'
    },
    return: {
      type: 'boolean'
    }
  },
  additionalProperties: false
}];
