/**
 * @fileoverview Check that common.skipIfInspectorDisabled is used if
 *               the inspector module is required.
 * @author Daniel Bevenius <daniel.bevenius@gmail.com>
 */
'use strict';

const utils = require('./rules-utils.js');

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------
const msg = 'Please add a skipIfInspectorDisabled() call to allow this ' +
            'test to be skippped when Node is built \'--without-inspector\'.';

module.exports = function(context) {
  var usesInspector = false;
  var hasInspectorCheck = false;

  function testInspectorUsage(context, node) {
    if (utils.isRequired(node, ['inspector'])) {
      usesInspector = true;
    }
  }

  function checkMemberExpression(context, node) {
    if (utils.usesCommonProperty(node, ['skipIfInspectorDisabled'])) {
      hasInspectorCheck = true;
    }
  }

  function reportIfMissing(context, node) {
    if (usesInspector && !hasInspectorCheck) {
      context.report(node, msg);
    }
  }

  return {
    'CallExpression': (node) => testInspectorUsage(context, node),
    'MemberExpression': (node) => checkMemberExpression(context, node),
    'Program:exit': (node) => reportIfMissing(context, node)
  };
};
