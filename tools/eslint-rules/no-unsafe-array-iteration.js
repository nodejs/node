/**
 * @file Rule to prevent unsafe array iteration patterns like for...of loops
 *   which rely on user-mutable global methods (Array.prototype[Symbol.iterator]).
 *   Instead, traditional for loops should be used for safer iteration.
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const USE_FOR_LOOP = 'Use traditional for loop instead of for...of for array iteration to avoid relying on user-mutable Symbol.iterator.';

/**
 * Checks if a node represents an array-like expression
 * @param {Object} node - The AST node to check
 * @returns {boolean} - True if the node appears to be an array
 */
function isArrayLike(node) {
  // Direct array literals
  if (node.type === 'ArrayExpression') {
    return true;
  }
  
  // Variables/identifiers that might be arrays (we'll be conservative and flag all)
  if (node.type === 'Identifier') {
    return true;
  }
  
  // Member expressions like obj.array, obj['array']
  if (node.type === 'MemberExpression') {
    return true;
  }
  
  // Call expressions that might return arrays
  if (node.type === 'CallExpression') {
    return true;
  }
  
  return false;
}

module.exports = {
  meta: {
    type: 'problem',
    docs: {
      description: 'disallow for...of loops on arrays to prevent unsafe iteration',
      category: 'Possible Errors',
    },
    fixable: null, // Not auto-fixable due to complexity of conversion
    schema: [],
  },
  
  create(context) {
    return {
      ForOfStatement(node) {
        // Check if we're iterating over something that looks like an array
        if (isArrayLike(node.right)) {
          context.report({
            node,
            message: USE_FOR_LOOP,
          });
        }
      },
    };
  },
};