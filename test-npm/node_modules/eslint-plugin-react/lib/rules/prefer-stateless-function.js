/**
 * @fileoverview Enforce stateless components to be written as a pure function
 * @author Yannick Croissant
 * @author Alberto Rodríguez
 * @copyright 2015 Alberto Rodríguez. All rights reserved.
 */
'use strict';

var Components = require('../util/Components');

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

module.exports = Components.detect(function(context, components, utils) {

  var sourceCode = context.getSourceCode();

  // --------------------------------------------------------------------------
  // Public
  // --------------------------------------------------------------------------

  /**
   * Get properties name
   * @param {Object} node - Property.
   * @returns {String} Property name.
   */
  function getPropertyName(node) {
    // Special case for class properties
    // (babel-eslint does not expose property name so we have to rely on tokens)
    if (node.type === 'ClassProperty') {
      var tokens = context.getFirstTokens(node, 2);
      return tokens[1] && tokens[1].type === 'Identifier' ? tokens[1].value : tokens[0].value;
    }

    return node.key.name;
  }

  /**
   * Get properties for a given AST node
   * @param {ASTNode} node The AST node being checked.
   * @returns {Array} Properties array.
   */
  function getComponentProperties(node) {
    switch (node.type) {
      case 'ClassExpression':
      case 'ClassDeclaration':
        return node.body.body;
      case 'ObjectExpression':
        return node.properties;
      default:
        return [];
    }
  }

  /**
   * Checks whether the constructor body is a redundant super call.
   * @see ESLint no-useless-constructor rule
   * @param {Array} body - constructor body content.
   * @param {Array} ctorParams - The params to check against super call.
   * @returns {boolean} true if the construtor body is redundant
   */
  function isRedundantSuperCall(body, ctorParams) {
    if (
      body.length !== 1 ||
      body[0].type !== 'ExpressionStatement' ||
      body[0].expression.callee.type !== 'Super'
    ) {
      return false;
    }

    var superArgs = body[0].expression.arguments;
    var firstSuperArg = superArgs[0];
    var lastSuperArgIndex = superArgs.length - 1;
    var lastSuperArg = superArgs[lastSuperArgIndex];
    var isSimpleParameterList = ctorParams.every(function(param) {
      return param.type === 'Identifier' || param.type === 'RestElement';
    });

    /**
     * Checks if a super argument is the same with constructor argument
     * @param {ASTNode} arg argument node
     * @param {number} index argument index
     * @returns {boolean} true if the arguments are same, false otherwise
     */
    function isSameIdentifier(arg, index) {
      return (
        arg.type === 'Identifier' &&
        arg.name === ctorParams[index].name
      );
    }

    var spreadsArguments =
      superArgs.length === 1 &&
      firstSuperArg.type === 'SpreadElement' &&
      firstSuperArg.argument.name === 'arguments';

    var passesParamsAsArgs =
      superArgs.length === ctorParams.length &&
      superArgs.every(isSameIdentifier) ||
      superArgs.length <= ctorParams.length &&
      superArgs.slice(0, -1).every(isSameIdentifier) &&
      lastSuperArg.type === 'SpreadElement' &&
      ctorParams[lastSuperArgIndex].type === 'RestElement' &&
      lastSuperArg.argument.name === ctorParams[lastSuperArgIndex].argument.name;

    return isSimpleParameterList && (spreadsArguments || passesParamsAsArgs);
  }

  /**
   * Check if a given AST node have any other properties the ones available in stateless components
   * @param {ASTNode} node The AST node being checked.
   * @returns {Boolean} True if the node has at least one other property, false if not.
   */
  function hasOtherProperties(node) {
    var properties = getComponentProperties(node);
    return properties.some(function(property) {
      var name = getPropertyName(property);
      var isDisplayName = name === 'displayName';
      var isPropTypes = name === 'propTypes' || name === 'props' && property.typeAnnotation;
      var contextTypes = name === 'contextTypes';
      var isUselessConstructor =
        property.kind === 'constructor' &&
        isRedundantSuperCall(property.value.body.body, property.value.params)
      ;
      var isRender = name === 'render';
      return !isDisplayName && !isPropTypes && !contextTypes && !isUselessConstructor && !isRender;
    });
  }

  /**
   * Mark a setState as used
   * @param {ASTNode} node The AST node being checked.
   */
  function markThisAsUsed(node) {
    components.set(node, {
      useThis: true
    });
  }

  /**
   * Mark a ref as used
   * @param {ASTNode} node The AST node being checked.
   */
  function markRefAsUsed(node) {
    components.set(node, {
      useRef: true
    });
  }

  /**
   * Mark return as invalid
   * @param {ASTNode} node The AST node being checked.
   */
  function markReturnAsInvalid(node) {
    components.set(node, {
      invalidReturn: true
    });
  }

  return {
    // Mark `this` destructuring as a usage of `this`
    VariableDeclarator: function(node) {
      // Ignore destructuring on other than `this`
      if (!node.id || node.id.type !== 'ObjectPattern' || !node.init || node.init.type !== 'ThisExpression') {
        return;
      }
      // Ignore `props` and `context`
      var useThis = node.id.properties.some(function(property) {
        var name = getPropertyName(property);
        return name !== 'props' && name !== 'context';
      });
      if (!useThis) {
        return;
      }
      markThisAsUsed(node);
    },

    // Mark `this` usage
    MemberExpression: function(node) {
      // Ignore calls to `this.props` and `this.context`
      if (
        node.object.type !== 'ThisExpression' ||
        (node.property.name || node.property.value) === 'props' ||
        (node.property.name || node.property.value) === 'context'
      ) {
        return;
      }
      markThisAsUsed(node);
    },

    // Mark `ref` usage
    JSXAttribute: function(node) {
      var name = sourceCode.getText(node.name);
      if (name !== 'ref') {
        return;
      }
      markRefAsUsed(node);
    },

    // Mark `render` that do not return some JSX
    ReturnStatement: function(node) {
      var blockNode;
      var scope = context.getScope();
      while (scope) {
        blockNode = scope.block && scope.block.parent;
        if (blockNode && (blockNode.type === 'MethodDefinition' || blockNode.type === 'Property')) {
          break;
        }
        scope = scope.upper;
      }
      if (!blockNode || !blockNode.key || blockNode.key.name !== 'render' || utils.isReturningJSX(node, true)) {
        return;
      }
      markReturnAsInvalid(node);
    },

    'Program:exit': function() {
      var list = components.list();
      for (var component in list) {
        if (
          !list.hasOwnProperty(component) ||
          hasOtherProperties(list[component].node) ||
          list[component].useThis ||
          list[component].useRef ||
          list[component].invalidReturn ||
          (!utils.isES5Component(list[component].node) && !utils.isES6Component(list[component].node))
        ) {
          continue;
        }

        context.report({
          node: list[component].node,
          message: 'Component should be written as a pure function'
        });
      }
    }
  };

});

module.exports.schema = [];
