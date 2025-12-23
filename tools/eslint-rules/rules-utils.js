/**
 * Utility functions common to ESLint rules.
 */
'use strict';

/**
 * @returns {boolean}
 */
function isRequireCall(node) {
  return node.callee.type === 'Identifier' && node.callee.name === 'require';
}
module.exports.isRequireCall = isRequireCall;

/**
 * @returns {boolean}
 */
module.exports.isString = function(node) {
  return node && node.type === 'Literal' && typeof node.value === 'string';
};

/**
 * @returns {boolean}
 */
module.exports.isDefiningError = function(node) {
  return node.expression &&
         node.expression.type === 'CallExpression' &&
         node.expression.callee &&
         node.expression.callee.name === 'E' &&
         node.expression.arguments.length !== 0;
};

/**
 * @returns {boolean}
 */
module.exports.isDefiningDeprecation = function(node) {
  return node.expression &&
         node.expression.type === 'CallExpression' &&
         node.expression.callee &&
         node.expression.callee.name.endsWith('deprecate') &&
         node.expression.arguments.length !== 0;
};

/**
 * Returns true if any of the passed in modules are used in
 * require calls.
 * @returns {boolean}
 */
module.exports.isRequired = function(node, modules) {
  return isRequireCall(node) && node.arguments.length !== 0 &&
    modules.includes(node.arguments[0].value);
};

const commonModuleRegExp = new RegExp(/^(\.\.\/)*common(\.js)?$/);
/**
 * Return true if common module is required
 * in AST Node under inspection
 * @returns {boolean}
 */
module.exports.isCommonModule = function(node) {
  return isRequireCall(node) &&
         node.arguments.length !== 0 &&
         commonModuleRegExp.test(node.arguments[0].value);
};

/**
 * Returns true if any of the passed in modules are used in
 * process.binding() or internalBinding() calls.
 * @returns {boolean}
 */
module.exports.isBinding = function(node, modules) {
  const isProcessBinding = node.callee.object &&
                           node.callee.object.name === 'process' &&
                           node.callee.property.name === 'binding';

  return (isProcessBinding || node.callee.name === 'internalBinding') &&
         modules.includes(node.arguments[0].value);
};

/**
 * Returns true is the node accesses any property in the properties
 * array on the 'common' object.
 * @returns {boolean}
 */
module.exports.usesCommonProperty = function(node, properties) {
  if (node.name) {
    return properties.includes(node.name);
  }
  if (node.property) {
    return properties.includes(node.property.name);
  }
  return false;
};

/**
 * Returns true if the passed in node is inside an if statement block,
 * and the block also has a call to skip.
 * @returns {boolean}
 */
module.exports.inSkipBlock = function(node) {
  let hasSkipBlock = false;
  if (node.test &&
      node.test.type === 'UnaryExpression' &&
      node.test.operator === '!') {
    const consequent = node.consequent;
    if (consequent.body) {
      consequent.body.some((expressionStatement) => {
        if (hasSkip(expressionStatement.expression)) {
          return hasSkipBlock = true;
        }
        return false;
      });
    } else if (hasSkip(consequent.expression)) {
      hasSkipBlock = true;
    }
  }
  return hasSkipBlock;
};

/**
 * @returns {boolean}
 */
function hasSkip(expression) {
  return expression?.callee?.name === 'skip' || expression?.callee?.property?.name === 'skip';
}
