/**
 * @fileoverview Processes Markdown files for consumption by ESLint.
 * @author Brandon Mills
 */

"use strict";

var assign = require("object-assign");
var unified = require("unified");
var remarkParse = require("remark-parse");

var SUPPORTED_SYNTAXES = ["js", "javascript", "node", "jsx"];
var UNSATISFIABLE_RULES = [
    "eol-last" // The Markdown parser strips trailing newlines in code fences
];

var markdown = unified().use(remarkParse);

var blocks = [];

/**
 * Performs a depth-first traversal of the Markdown AST.
 * @param {ASTNode} node A Markdown AST node.
 * @param {object} callbacks A map of node types to callbacks.
 * @param {object} [parent] The node's parent AST node.
 * @returns {void}
 */
function traverse(node, callbacks, parent) {
    var i;

    if (callbacks[node.type]) {
        callbacks[node.type](node, parent);
    }

    if (typeof node.children !== "undefined") {
        for (i = 0; i < node.children.length; i++) {
            traverse(node.children[i], callbacks, node);
        }
    }
}

/**
 * Converts leading HTML comments to JS block comments.
 * @param {string} html The text content of an HTML AST node.
 * @returns {string[]} An array of JS block comments.
 */
function getComment(html) {
    var commentStart = "<!--";
    var commentEnd = "-->";
    var prefix = "eslint";

    if (
        html.slice(0, commentStart.length) !== commentStart ||
        html.slice(-commentEnd.length) !== commentEnd
    ) {
        return "";
    }

    html = html.slice(commentStart.length, -commentEnd.length);

    if (html.trim().slice(0, prefix.length) !== prefix) {
        return "";
    }

    return html;
}

/**
 * Extracts lintable JavaScript code blocks from Markdown text.
 * @param {string} text The text of the file.
 * @returns {string[]} Source code strings to lint.
 */
function preprocess(text) {
    var ast = markdown.parse(text);

    blocks = [];
    traverse(ast, {
        "code": function(node, parent) {
            var comments = [];
            var index, previousNode, comment;

            if (node.lang && SUPPORTED_SYNTAXES.indexOf(node.lang.toLowerCase()) >= 0) {
                index = parent.children.indexOf(node) - 1;
                previousNode = parent.children[index];
                while (previousNode && previousNode.type === "html") {
                    comment = getComment(previousNode.value);

                    if (!comment) {
                        break;
                    }

                    if (comment.trim() === "eslint-skip") {
                        return;
                    }

                    comments.unshift("/*" + comment + "*/");
                    index--;
                    previousNode = parent.children[index];
                }

                blocks.push(assign({}, node, { comments: comments }));
            }
        }
    });

    return blocks.map(function(block) {
        return block.comments.concat(block.value).join("\n");
    });
}

/**
 * Creates a map function that adjusts messages in a code block.
 * @param {Block} block A code block.
 * @returns {function} A function that adjusts messages in a code block.
 */
function adjustBlock(block) {
    var leadingCommentLines = block.comments.reduce(function(count, comment) {
        return count + comment.split("\n").length;
    }, 0);

    /**
     * Adjusts ESLint messages to point to the correct location in the Markdown.
     * @param {Message} message A message from ESLint.
     * @returns {Message} The same message, but adjusted ot the correct location.
     */
    return function adjustMessage(message) {
        var lineInCode = message.line - leadingCommentLines;
        if (lineInCode < 1) {
            return null;
        }

        return assign({}, message, {
            line: lineInCode + block.position.start.line,
            column: message.column + block.position.indent[lineInCode - 1] - 1
        });
    };
}

/**
 * Excludes unsatisfiable rules from the list of messages.
 * @param {Message} message A message from the linter.
 * @returns {boolean} True if the message should be included in output.
 */
function excludeUnsatisfiableRules(message) {
    return message && UNSATISFIABLE_RULES.indexOf(message.ruleId) < 0;
}

/**
 * Transforms generated messages for output.
 * @param {Array<Message[]>} messages An array containing one array of messages
 *     for each code block returned from `preprocess`.
 * @returns {Message[]} A flattened array of messages with mapped locations.
 */
function postprocess(messages) {
    return [].concat.apply([], messages.map(function(group, i) {
        var adjust = adjustBlock(blocks[i]);
        return group.map(adjust).filter(excludeUnsatisfiableRules);
    }));
}

module.exports = {
    preprocess: preprocess,
    postprocess: postprocess
};
