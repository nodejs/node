/**
 * @fileoverview Restrict file extensions that may be required
 * @author Scott Andrews
 */
'use strict';

var path = require('path');

// ------------------------------------------------------------------------------
// Constants
// ------------------------------------------------------------------------------

var DEFAULTS = {
  extentions: ['.jsx']
};

var PKG_REGEX = /^[^\.]((?!\/).)*$/;

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

module.exports = function(context) {

  function isPackage(id) {
    return PKG_REGEX.test(id);
  }

  function isRequire(expression) {
    return expression.callee.name === 'require';
  }

  function getId(expression) {
    return expression.arguments[0] && expression.arguments[0].value;
  }

  function getExtension(id) {
    return path.extname(id || '');
  }

  function getExtentionsConfig() {
    return context.options[0] && context.options[0].extensions || DEFAULTS.extentions;
  }

  var forbiddenExtensions = getExtentionsConfig().reduce(function (extensions, extension) {
    extensions[extension] = true;
    return extensions;
  }, Object.create(null));

  function isForbiddenExtension(ext) {
    return ext in forbiddenExtensions;
  }

  // --------------------------------------------------------------------------
  // Public
  // --------------------------------------------------------------------------

  return {

    CallExpression: function(node) {
      if (isRequire(node)) {
        var id = getId(node);
        var ext = getExtension(id);
        if (!isPackage(id) && isForbiddenExtension(ext)) {
          context.report({
            node: node,
            message: 'Unable to require module with extension \'' + ext + '\''
          });
        }
      }
    }

  };

};

module.exports.schema = [{
  type: 'object',
  properties: {
    extensions: {
      type: 'array',
      items: {
        type: 'string'
      }
    }
  },
  additionalProperties: false
}];
