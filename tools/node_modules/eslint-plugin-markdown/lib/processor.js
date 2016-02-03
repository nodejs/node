/**
 * @fileoverview Processes Markdown files for consumption by ESLint.
 * @author Brandon Mills
 * @copyright 2015 Brandon Mills. All rights reserved.
 * @copyright 2015 Ian VanSchooten. All rights reserved.
 * See LICENSE in root directory for full license.
 */

"use strict";

var assign = require("object-assign");
var remark = require("remark");

var SUPPORTED_SYNTAXES = ["js", "javascript", "node", "jsx"];

var blocks = [];

/**
 * Performs a depth-first traversal of the Markdown AST.
 * @param {ASTNode} node A Markdown AST node.
 * @param {object} callbacks A map of node types to callbacks.
 * @returns {void}
 */
function traverse(node, callbacks) {
    var i;

    if (callbacks[node.type]) {
        callbacks[node.type](node);
    }

    if (typeof node.children !== "undefined") {
        for (i = 0; i < node.children.length; i++) {
            traverse(node.children[i], callbacks);
        }
    }
}

/**
 * Extracts lintable JavaScript code blocks from Markdown text.
 * @param {string} text The text of the file.
 * @returns {[string]} Code blocks to lint.
 */
function preprocess(text) {
    var ast = remark.parse(text);

    blocks = [];
    traverse(ast, {
        "code": function(node) {
            if (node.lang && SUPPORTED_SYNTAXES.indexOf(node.lang.toLowerCase()) >= 0) {
                blocks.push(node);
            }
        }
    });

    return blocks.map(function(block) {
        return block.value;
    });
}

/**
 * Transforms generated messages for output.
 * @param {Array<Message[]>} messages An array containing one array of messages
 *     for each code block returned from `preprocess`.
 * @returns {Message[]} A flattened array of messages with mapped locations.
 */
function postprocess(messages) {
    return [].concat.apply([], messages.map(function(group, i) {
        return group.map(function(message) {
            return assign({}, message, {
                line: message.line + blocks[i].position.start.line,
                column: message.column + blocks[i].position.indent[message.line - 1] - 1
            });
        });
    }));
}

module.exports = {
    preprocess: preprocess,
    postprocess: postprocess
};
