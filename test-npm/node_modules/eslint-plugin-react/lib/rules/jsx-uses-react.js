/**
 * @fileoverview Prevent React to be marked as unused
 * @author Glen Mailer
 */
'use strict';

var variableUtil = require('../util/variable');
var pragmaUtil = require('../util/pragma');

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

module.exports = function(context) {

  var pragma = pragmaUtil.getFromContext(context);

  // --------------------------------------------------------------------------
  // Public
  // --------------------------------------------------------------------------

  return {

    JSXOpeningElement: function() {
      variableUtil.markVariableAsUsed(context, pragma);
    },

    BlockComment: function(node) {
      pragma = pragmaUtil.getFromNode(node) || pragma;
    }

  };

};

module.exports.schema = [{
  type: 'object',
  properties: {
    pragma: {
      type: 'string'
    }
  },
  additionalProperties: false
}];
