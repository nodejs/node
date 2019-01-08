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

const cryptoModules = ['crypto', 'http2'];
const requireModules = cryptoModules.concat(['tls', 'https']);
const bindingModules = cryptoModules.concat(['tls_wrap']);

module.exports = function(context) {
  const missingCheckNodes = [];
  const requireNodes = [];
  var commonModuleNode = null;
  var hasSkipCall = false;

  function testCryptoUsage(node) {
    if (utils.isRequired(node, requireModules) ||
        utils.isBinding(node, bindingModules)) {
      requireNodes.push(node);
    }

    if (utils.isCommonModule(node)) {
      commonModuleNode = node;
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

  function reportIfMissingCheck() {
    if (hasSkipCall) {
      // There is a skip, which is good, but verify that the require() calls
      // in question come after at least one check.
      if (missingCheckNodes.length > 0) {
        requireNodes.forEach((requireNode) => {
          const beforeAllChecks = missingCheckNodes.every((checkNode) => {
            return requireNode.start < checkNode.start;
          });

          if (beforeAllChecks) {
            context.report({
              node: requireNode,
              message: msg
            });
          }
        });
      }
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
      context.report({
        node,
        message: msg,
        fix: (fixer) => {
          if (commonModuleNode) {
            return fixer.insertTextAfter(
              commonModuleNode,
              '\nif (!common.hasCrypto) {' +
              ' common.skip("missing crypto");' +
              '}'
            );
          }
        }
      });
    });
  }

  return {
    'CallExpression': (node) => testCryptoUsage(node),
    'IfStatement:exit': (node) => testIfStatement(node),
    'MemberExpression:exit': (node) => testMemberExpression(node),
    'Program:exit': (node) => reportIfMissingCheck(node)
  };
};
