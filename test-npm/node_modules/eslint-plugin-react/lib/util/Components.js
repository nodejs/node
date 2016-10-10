/**
 * @fileoverview Utility class and functions for React components detection
 * @author Yannick Croissant
 */
'use strict';

var util = require('util');
var variableUtil = require('./variable');
var pragmaUtil = require('./pragma');

/**
 * Components
 * @class
 */
function Components() {
  this._list = {};
  this._getId = function(node) {
    return node && node.range.join(':');
  };
}

/**
 * Add a node to the components list, or update it if it's already in the list
 *
 * @param {ASTNode} node The AST node being added.
 * @param {Number} confidence Confidence in the component detection (0=banned, 1=maybe, 2=yes)
 * @returns {Object} Added component object
 */
Components.prototype.add = function(node, confidence) {
  var id = this._getId(node);
  if (this._list[id]) {
    if (confidence === 0 || this._list[id].confidence === 0) {
      this._list[id].confidence = 0;
    } else {
      this._list[id].confidence = Math.max(this._list[id].confidence, confidence);
    }
    return this._list[id];
  }
  this._list[id] = {
    node: node,
    confidence: confidence
  };
  return this._list[id];
};

/**
 * Find a component in the list using its node
 *
 * @param {ASTNode} node The AST node being searched.
 * @returns {Object} Component object, undefined if the component is not found
 */
Components.prototype.get = function(node) {
  var id = this._getId(node);
  return this._list[id];
};

/**
 * Update a component in the list
 *
 * @param {ASTNode} node The AST node being updated.
 * @param {Object} props Additional properties to add to the component.
 */
Components.prototype.set = function(node, props) {
  while (node && !this._list[this._getId(node)]) {
    node = node.parent;
  }
  if (!node) {
    return;
  }
  var id = this._getId(node);
  this._list[id] = util._extend(this._list[id], props);
};

/**
 * Return the components list
 * Components for which we are not confident are not returned
 *
 * @returns {Object} Components list
 */
Components.prototype.list = function() {
  var list = {};
  var usedPropTypes = {};
  // Find props used in components for which we are not confident
  for (var i in this._list) {
    if (!this._list.hasOwnProperty(i) || this._list[i].confidence >= 2) {
      continue;
    }
    var component = null;
    var node = null;
    node = this._list[i].node;
    while (!component && node.parent) {
      node = node.parent;
      component = this.get(node);
    }
    if (component) {
      usedPropTypes[this._getId(component.node)] = this._list[i].usedPropTypes;
    }
  }
  // Assign used props in not confident components to the parent component
  for (var j in this._list) {
    if (!this._list.hasOwnProperty(j) || this._list[j].confidence < 2) {
      continue;
    }
    var id = this._getId(this._list[j].node);
    list[j] = this._list[j];
    if (usedPropTypes[id]) {
      list[j].usedPropTypes = (list[j].usedPropTypes || []).concat(usedPropTypes[id]);
    }
  }
  return list;
};

/**
 * Return the length of the components list
 * Components for which we are not confident are not counted
 *
 * @returns {Number} Components list length
 */
Components.prototype.length = function() {
  var length = 0;
  for (var i in this._list) {
    if (!this._list.hasOwnProperty(i) || this._list[i].confidence < 2) {
      continue;
    }
    length++;
  }
  return length;
};

function componentRule(rule, context) {

  var pragma = pragmaUtil.getFromContext(context);
  var sourceCode = context.getSourceCode();
  var components = new Components();

  // Utilities for component detection
  var utils = {

    /**
     * Check if the node is a React ES5 component
     *
     * @param {ASTNode} node The AST node being checked.
     * @returns {Boolean} True if the node is a React ES5 component, false if not
     */
    isES5Component: function(node) {
      if (!node.parent) {
        return false;
      }
      return new RegExp('^(' + pragma + '\\.)?createClass$').test(sourceCode.getText(node.parent.callee));
    },

    /**
     * Check if the node is a React ES6 component
     *
     * @param {ASTNode} node The AST node being checked.
     * @returns {Boolean} True if the node is a React ES6 component, false if not
     */
    isES6Component: function(node) {
      if (!node.superClass) {
        return false;
      }
      return new RegExp('^(' + pragma + '\\.)?Component$').test(sourceCode.getText(node.superClass));
    },

    /**
     * Check if the node is returning JSX
     *
     * @param {ASTNode} node The AST node being checked (can be a ReturnStatement or an ArrowFunctionExpression).
     * @param {Boolean} strict If true, in a ternary condition the node must return JSX in both cases
     * @returns {Boolean} True if the node is returning JSX, false if not
     */
    isReturningJSX: function(node, strict) {
      var property;
      switch (node.type) {
        case 'ReturnStatement':
          property = 'argument';
          break;
        case 'ArrowFunctionExpression':
          property = 'body';
          break;
        default:
          return false;
      }

      var returnsConditionalJSXConsequent =
        node[property] &&
        node[property].type === 'ConditionalExpression' &&
        node[property].consequent.type === 'JSXElement'
      ;
      var returnsConditionalJSXAlternate =
        node[property] &&
        node[property].type === 'ConditionalExpression' &&
        node[property].alternate.type === 'JSXElement'
      ;
      var returnsConditionalJSX =
        strict ? (returnsConditionalJSXConsequent && returnsConditionalJSXAlternate) :
        (returnsConditionalJSXConsequent || returnsConditionalJSXAlternate);

      var returnsJSX =
        node[property] &&
        node[property].type === 'JSXElement'
      ;
      var returnsReactCreateElement =
        node[property] &&
        node[property].callee &&
        node[property].callee.property &&
        node[property].callee.property.name === 'createElement'
      ;

      return Boolean(
        returnsConditionalJSX ||
        returnsJSX ||
        returnsReactCreateElement
      );
    },

    /**
     * Get the parent component node from the current scope
     *
     * @returns {ASTNode} component node, null if we are not in a component
     */
    getParentComponent: function() {
      return (
        utils.getParentES6Component() ||
        utils.getParentES5Component() ||
        utils.getParentStatelessComponent()
      );
    },

    /**
     * Get the parent ES5 component node from the current scope
     *
     * @returns {ASTNode} component node, null if we are not in a component
     */
    getParentES5Component: function() {
      var scope = context.getScope();
      while (scope) {
        var node = scope.block && scope.block.parent && scope.block.parent.parent;
        if (node && utils.isES5Component(node)) {
          return node;
        }
        scope = scope.upper;
      }
      return null;
    },

    /**
     * Get the parent ES6 component node from the current scope
     *
     * @returns {ASTNode} component node, null if we are not in a component
     */
    getParentES6Component: function() {
      var scope = context.getScope();
      while (scope && scope.type !== 'class') {
        scope = scope.upper;
      }
      var node = scope && scope.block;
      if (!node || !utils.isES6Component(node)) {
        return null;
      }
      return node;
    },

    /**
     * Get the parent stateless component node from the current scope
     *
     * @returns {ASTNode} component node, null if we are not in a component
     */
    getParentStatelessComponent: function() {
      var scope = context.getScope();
      while (scope) {
        var node = scope.block;
        var isFunction = /Function/.test(node.type); // Ignore non functions
        var isNotMethod = !node.parent || node.parent.type !== 'MethodDefinition'; // Ignore classes methods
        var isNotArgument = !node.parent || node.parent.type !== 'CallExpression'; // Ignore arguments (callback, etc.)
        if (isFunction && isNotMethod && isNotArgument) {
          return node;
        }
        scope = scope.upper;
      }
      return null;
    },

    /**
     * Get the related component from a node
     *
     * @param {ASTNode} node The AST node being checked (must be a MemberExpression).
     * @returns {ASTNode} component node, null if we cannot find the component
     */
    getRelatedComponent: function(node) {
      var i;
      var j;
      var k;
      var l;
      // Get the component path
      var componentPath = [];
      while (node) {
        if (node.property && node.property.type === 'Identifier') {
          componentPath.push(node.property.name);
        }
        if (node.object && node.object.type === 'Identifier') {
          componentPath.push(node.object.name);
        }
        node = node.object;
      }
      componentPath.reverse();

      // Find the variable in the current scope
      var variableName = componentPath.shift();
      if (!variableName) {
        return null;
      }
      var variableInScope;
      var variables = variableUtil.variablesInScope(context);
      for (i = 0, j = variables.length; i < j; i++) {
        if (variables[i].name === variableName) {
          variableInScope = variables[i];
          break;
        }
      }
      if (!variableInScope) {
        return null;
      }

      // Find the variable declaration
      var defInScope;
      var defs = variableInScope.defs;
      for (i = 0, j = defs.length; i < j; i++) {
        if (defs[i].type === 'ClassName' || defs[i].type === 'FunctionName' || defs[i].type === 'Variable') {
          defInScope = defs[i];
          break;
        }
      }
      if (!defInScope || !defInScope.node) {
        return null;
      }
      node = defInScope.node.init || defInScope.node;

      // Traverse the node properties to the component declaration
      for (i = 0, j = componentPath.length; i < j; i++) {
        if (!node.properties) {
          continue;
        }
        for (k = 0, l = node.properties.length; k < l; k++) {
          if (node.properties[k].key.name === componentPath[i]) {
            node = node.properties[k];
            break;
          }
        }
        if (!node || !node.value) {
          return null;
        }
        node = node.value;
      }

      // Return the component
      return components.add(node, 1);
    }
  };

  // Component detection instructions
  var detectionInstructions = {
    ClassExpression: function(node) {
      if (!utils.isES6Component(node)) {
        return;
      }
      components.add(node, 2);
    },

    ClassDeclaration: function(node) {
      if (!utils.isES6Component(node)) {
        return;
      }
      components.add(node, 2);
    },

    ClassProperty: function(node) {
      node = utils.getParentComponent();
      if (!node) {
        return;
      }
      components.add(node, 2);
    },

    ObjectExpression: function(node) {
      if (!utils.isES5Component(node)) {
        return;
      }
      components.add(node, 2);
    },

    FunctionExpression: function(node) {
      node = utils.getParentComponent();
      if (!node) {
        return;
      }
      components.add(node, 1);
    },

    FunctionDeclaration: function(node) {
      node = utils.getParentComponent();
      if (!node) {
        return;
      }
      components.add(node, 1);
    },

    ArrowFunctionExpression: function(node) {
      node = utils.getParentComponent();
      if (!node) {
        return;
      }
      if (node.expression && utils.isReturningJSX(node)) {
        components.add(node, 2);
      } else {
        components.add(node, 1);
      }
    },

    ThisExpression: function(node) {
      node = utils.getParentComponent();
      if (!node || !/Function/.test(node.type)) {
        return;
      }
      // Ban functions with a ThisExpression
      components.add(node, 0);
    },

    BlockComment: function(node) {
      pragma = pragmaUtil.getFromNode(node) || pragma;
    },

    ReturnStatement: function(node) {
      if (!utils.isReturningJSX(node)) {
        return;
      }
      node = utils.getParentComponent();
      if (!node) {
        return;
      }
      components.add(node, 2);
    }
  };

  // Update the provided rule instructions to add the component detection
  var ruleInstructions = rule(context, components, utils);
  var updatedRuleInstructions = util._extend({}, ruleInstructions);
  Object.keys(detectionInstructions).forEach(function(instruction) {
    updatedRuleInstructions[instruction] = function(node) {
      detectionInstructions[instruction](node);
      return ruleInstructions[instruction] ? ruleInstructions[instruction](node) : void 0;
    };
  });
  // Return the updated rule instructions
  return updatedRuleInstructions;
}

Components.detect = function(rule) {
  return componentRule.bind(this, rule);
};

module.exports = Components;
