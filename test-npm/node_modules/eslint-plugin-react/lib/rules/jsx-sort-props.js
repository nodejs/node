/**
 * @fileoverview Enforce props alphabetical sorting
 * @author Ilya Volodin, Yannick Croissant
 */
'use strict';

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

function isCallbackPropName(propName) {
  return /^on[A-Z]/.test(propName);
}

module.exports = function(context) {

  var configuration = context.options[0] || {};
  var ignoreCase = configuration.ignoreCase || false;
  var callbacksLast = configuration.callbacksLast || false;
  var shorthandFirst = configuration.shorthandFirst || false;

  return {
    JSXOpeningElement: function(node) {
      node.attributes.reduce(function(memo, decl, idx, attrs) {
        if (decl.type === 'JSXSpreadAttribute') {
          return attrs[idx + 1];
        }

        var previousPropName = memo.name.name;
        var currentPropName = decl.name.name;
        var previousValue = memo.value;
        var currentValue = decl.value;
        var previousIsCallback = isCallbackPropName(previousPropName);
        var currentIsCallback = isCallbackPropName(currentPropName);

        if (ignoreCase) {
          previousPropName = previousPropName.toLowerCase();
          currentPropName = currentPropName.toLowerCase();
        }

        if (callbacksLast) {
          if (!previousIsCallback && currentIsCallback) {
            // Entering the callback prop section
            return decl;
          }
          if (previousIsCallback && !currentIsCallback) {
            // Encountered a non-callback prop after a callback prop
            context.report({
              node: memo,
              message: 'Callbacks must be listed after all other props'
            });
            return memo;
          }
        }

        if (shorthandFirst) {
          if (currentValue && !previousValue) {
            return decl;
          }
          if (!currentValue && previousValue) {
            context.report({
              node: memo,
              message: 'Shorthand props must be listed before all other props'
            });
            return memo;
          }
        }

        if (currentPropName < previousPropName) {
          context.report({
            node: decl,
            message: 'Props should be sorted alphabetically'
          });
          return memo;
        }

        return decl;
      }, node.attributes[0]);
    }
  };
};

module.exports.schema = [{
  type: 'object',
  properties: {
    // Whether callbacks (prefixed with "on") should be listed at the very end,
    // after all other props.
    callbacksLast: {
      type: 'boolean'
    },
    // Whether shorthand properties (without a value) should be listed first
    shorthandFirst: {
      type: 'boolean'
    },
    ignoreCase: {
      type: 'boolean'
    }
  },
  additionalProperties: false
}];
