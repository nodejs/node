/**
 * @fileoverview Check that common.hasCrypto is used if crypto, tls,
 * https, or http2 modules are required.
 *
 * This rule can be ignored using // eslint-disable-line crypto-check
 *
 * @author Daniel Bevenius <daniel.bevenius@gmail.com>
 */
'use strict';

const utils = require('./rules-utils.js');

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------
const msg = 'Please add a hasCrypto check to allow this test to be skipped ' +
            'when Node is built "--without-ssl".';

module.exports = function(context) {
  const missingCheckNodes = [];
  const requireNodes = [];
  var hasSkipCall = false;

  function testCryptoUsage(node) {
    if (utils.isRequired(node, ['crypto', 'tls', 'https', 'http2'])) {
      requireNodes.push(node);
    }
  }

  function testIfStatement(node) {
    if (node.test.argument === undefined) {
      return;
    }
    if (isCryptoCheck(node.test.argument)) {
      checkCryptoCall(node);
    }
  }

  function isCryptoCheck(node) {
    return utils.usesCommonProperty(node, ['hasCrypto', 'hasFipsCrypto']);
  }

  function checkCryptoCall(node) {
    if (utils.inSkipBlock(node)) {
      hasSkipCall = true;
    } else {
      missingCheckNodes.push(node);
    }
  }

  function testMemberExpression(node) {
    if (isCryptoCheck(node)) {
      checkCryptoCall(node);
    }
  }

  function reportIfMissingCheck(node) {
    if (hasSkipCall) {
      return;
    }

    if (requireNodes.length > 0) {
      if (missingCheckNodes.length > 0) {
        report(missingCheckNodes);
      } else {
        report(requireNodes);
      }
    }
  }

  function report(nodes) {
    nodes.forEach((node) => {
      context.report(node, msg);
    });
  }

  return {
    'CallExpression': (node) => testCryptoUsage(node),
    'IfStatement:exit': (node) => testIfStatement(node),
    'MemberExpression:exit': (node) => testMemberExpression(node),
    'Program:exit': (node) => reportIfMissingCheck(node)
  };
};
