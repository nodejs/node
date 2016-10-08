/**
 * @fileoverview Prevents usage of Function.prototype.bind and arrow functions
 *               in React component definition.
 * @author Daniel Lo Nigro <dan.cx>
 */
'use strict';

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

module.exports = function(context) {
  var configuration = context.options[0] || {};

  return {
    JSXAttribute: function(node) {
      var isRef = configuration.ignoreRefs && node.name.name === 'ref';
      if (isRef || !node.value || !node.value.expression) {
        return;
      }
      var valueNode = node.value.expression;
      if (
        !configuration.allowBind &&
        valueNode.type === 'CallExpression' &&
        valueNode.callee.type === 'MemberExpression' &&
        valueNode.callee.property.name === 'bind'
      ) {
        context.report({
          node: node,
          message: 'JSX props should not use .bind()'
        });
      } else if (
        !configuration.allowArrowFunctions &&
        valueNode.type === 'ArrowFunctionExpression'
      ) {
        context.report({
          node: node,
          message: 'JSX props should not use arrow functions'
        });
      }
    }
  };
};

module.exports.schema = [{
  type: 'object',
  properties: {
    allowArrowFunctions: {
      default: false,
      type: 'boolean'
    },
    allowBind: {
      default: false,
      type: 'boolean'
    },
    ignoreRefs: {
      default: false,
      type: 'boolean'
    }
  },
  additionalProperties: false
}];
