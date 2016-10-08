/**
 * @fileoverview Validate props indentation in JSX
 * @author Yannick Croissant

 * This rule has been ported and modified from eslint and nodeca.
 * @author Vitaly Puzrin
 * @author Gyandeep Singh
 * @copyright 2015 Vitaly Puzrin. All rights reserved.
 * @copyright 2015 Gyandeep Singh. All rights reserved.
 Copyright (C) 2014 by Vitaly Puzrin

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the 'Software'), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */
'use strict';

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------
module.exports = function(context) {

  var MESSAGE = 'Expected indentation of {{needed}} {{type}} {{characters}} but found {{gotten}}.';

  var extraColumnStart = 0;
  var indentType = 'space';
  var indentSize = 4;

  var sourceCode = context.getSourceCode();

  if (context.options.length) {
    if (context.options[0] === 'tab') {
      indentSize = 1;
      indentType = 'tab';
    } else if (typeof context.options[0] === 'number') {
      indentSize = context.options[0];
      indentType = 'space';
    }
  }

  /**
   * Reports a given indent violation and properly pluralizes the message
   * @param {ASTNode} node Node violating the indent rule
   * @param {Number} needed Expected indentation character count
   * @param {Number} gotten Indentation character count in the actual node/code
   * @param {Object=} loc Error line and column location
   */
  function report(node, needed, gotten, loc) {
    var msgContext = {
      needed: needed,
      type: indentType,
      characters: needed === 1 ? 'character' : 'characters',
      gotten: gotten
    };

    if (loc) {
      context.report({
        node: node,
        loc: loc,
        message: MESSAGE,
        data: msgContext
      });
    } else {
      context.report({
        node: node,
        message: MESSAGE,
        data: msgContext
      });
    }
  }

  /**
   * Get node indent
   * @param {ASTNode} node Node to examine
   * @param {Boolean} byLastLine get indent of node's last line
   * @param {Boolean} excludeCommas skip comma on start of line
   * @return {Number} Indent
   */
  function getNodeIndent(node, byLastLine, excludeCommas) {
    byLastLine = byLastLine || false;
    excludeCommas = excludeCommas || false;

    var src = sourceCode.getText(node, node.loc.start.column + extraColumnStart);
    var lines = src.split('\n');
    if (byLastLine) {
      src = lines[lines.length - 1];
    } else {
      src = lines[0];
    }

    var skip = excludeCommas ? ',' : '';

    var regExp;
    if (indentType === 'space') {
      regExp = new RegExp('^[ ' + skip + ']+');
    } else {
      regExp = new RegExp('^[\t' + skip + ']+');
    }

    var indent = regExp.exec(src);
    return indent ? indent[0].length : 0;
  }

  /**
   * Checks node is the first in its own start line. By default it looks by start line.
   * @param {ASTNode} node The node to check
   * @return {Boolean} true if its the first in the its start line
   */
  function isNodeFirstInLine(node) {
    var token = node;
    do {
      token = sourceCode.getTokenBefore(token);
    } while (token.type === 'JSXText');
    var startLine = node.loc.start.line;
    var endLine = token ? token.loc.end.line : -1;

    return startLine !== endLine;
  }

  /**
   * Check indent for nodes list
   * @param {ASTNode[]} nodes list of node objects
   * @param {Number} indent needed indent
   * @param {Boolean} excludeCommas skip comma on start of line
   */
  function checkNodesIndent(node, indent, excludeCommas) {
    var nodeIndent = getNodeIndent(node, false, excludeCommas);
    if (nodeIndent !== indent && isNodeFirstInLine(node)) {
      report(node, indent, nodeIndent);
    }
  }

  return {
    JSXOpeningElement: function(node) {
      if (!node.parent || !node.parent.parent) {
        return;
      }
      var parentElementIndent = getNodeIndent(node.parent.parent);
      var indent = node.parent.parent.loc.start.line === node.loc.start.line ? 0 : indentSize;
      checkNodesIndent(node, parentElementIndent + indent);
    },
    JSXClosingElement: function(node) {
      if (!node.parent) {
        return;
      }
      var peerElementIndent = getNodeIndent(node.parent.openingElement);
      checkNodesIndent(node, peerElementIndent);
    }
  };

};

module.exports.schema = [{
  oneOf: [{
    enum: ['tab']
  }, {
    type: 'integer'
  }]
}];
