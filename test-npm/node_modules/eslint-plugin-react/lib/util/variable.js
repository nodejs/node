/**
 * @fileoverview Utility functions for React components detection
 * @author Yannick Croissant
 */
'use strict';

/**
 * Record that a particular variable has been used in code
 *
 * @param {Object} context The current rule context.
 * @param {String} name The name of the variable to mark as used.
 * @returns {Boolean} True if the variable was found and marked as used, false if not.
 */
function markVariableAsUsed(context, name) {
  var scope = context.getScope();
  var variables;
  var i;
  var len;
  var found = false;

  // Special Node.js scope means we need to start one level deeper
  if (scope.type === 'global') {
    while (scope.childScopes.length) {
      scope = scope.childScopes[0];
    }
  }

  do {
    variables = scope.variables;
    for (i = 0, len = variables.length; i < len; i++) {
      if (variables[i].name === name) {
        variables[i].eslintUsed = true;
        found = true;
      }
    }
    scope = scope.upper;
  } while (scope);

  return found;
}

/**
 * Search a particular variable in a list
 * @param {Array} variables The variables list.
 * @param {Array} name The name of the variable to search.
 * @returns {Boolean} True if the variable was found, false if not.
 */
function findVariable(variables, name) {
  var i;
  var len;

  for (i = 0, len = variables.length; i < len; i++) {
    if (variables[i].name === name) {
      return true;
    }
  }

  return false;
}

/**
 * List all variable in a given scope
 *
 * Contain a patch for babel-eslint to avoid https://github.com/babel/babel-eslint/issues/21
 *
 * @param {Object} context The current rule context.
 * @param {Array} name The name of the variable to search.
 * @returns {Boolean} True if the variable was found, false if not.
 */
function variablesInScope(context) {
  var scope = context.getScope();
  var variables = scope.variables;

  while (scope.type !== 'global') {
    scope = scope.upper;
    variables = scope.variables.concat(variables);
  }
  if (scope.childScopes.length) {
    variables = scope.childScopes[0].variables.concat(variables);
    if (scope.childScopes[0].childScopes.length) {
      variables = scope.childScopes[0].childScopes[0].variables.concat(variables);
    }
  }

  return variables;
}

module.exports = {
  findVariable: findVariable,
  variablesInScope: variablesInScope,
  markVariableAsUsed: markVariableAsUsed
};
