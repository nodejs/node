/**
 * @fileoverview Prevent extra closing tags for components without children
 * @author Yannick Croissant
 */
'use strict';

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

module.exports = function(context) {

  var tagConvention = /^[a-z]|\-/;
  function isTagName(name) {
    return tagConvention.test(name);
  }

  function isComponent(node) {
    return node.name && node.name.type === 'JSXIdentifier' && !isTagName(node.name.name);
  }

  function hasChildren(node) {
    var childrens = node.parent.children;
    if (
      !childrens.length ||
      (childrens.length === 1 && childrens[0].type === 'Literal' && !childrens[0].value.replace(/(?!\xA0)\s/g, ''))
    ) {
      return false;
    }
    return true;
  }

  // --------------------------------------------------------------------------
  // Public
  // --------------------------------------------------------------------------

  return {

    JSXOpeningElement: function(node) {
      if (!isComponent(node) || node.selfClosing || hasChildren(node)) {
        return;
      }
      context.report({
        node: node,
        message: 'Empty components are self-closing'
      });
    }
  };

};

module.exports.schema = [];
