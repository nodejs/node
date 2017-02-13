/**
 * @fileoverview Verify copyright header
 * @author James M Snell
 */
'use strict';

const copyright = /Copyright Node\.js contributors\. All rights reserved\./;
const spdx = /SPDX-License-Identifier: MIT/;

module.exports = function(context) {

  const state = {
    copyright: 0,
    spdx: 0
  };

  function process(node) {
    if (copyright.test(node.value))
      state.copyright++;
    if (spdx.test(node.value))
      state.spdx++;
  }

  return {
    'BlockComment': process,
    'LineComment': process,
    'Program:exit': function(node) {
      if (state.copyright === 0) {
        context.report(node,
                       'The Node.js Copyright comment is required');
      }
      if (state.copyright > 1) {
        context.report(node,
                       'There should be only one Node.js Copyright comment');
      }
      if (state.spdx === 0) {
        context.report(node,
                       'The Node.js SPDX License comment is required');
      }
      if (state.spdx > 1) {
        context.report(node,
                       'There should be only one Node.js SPDX License comment');
      }
    }
  };
};
