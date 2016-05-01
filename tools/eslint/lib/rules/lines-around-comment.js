/**
 * @fileoverview Enforces empty lines around comments.
 * @author Jamund Ferguson
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var lodash = require("lodash");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Return an array with with any line numbers that are empty.
 * @param {Array} lines An array of each line of the file.
 * @returns {Array} An array of line numbers.
 */
function getEmptyLineNums(lines) {
    var emptyLines = lines.map(function(line, i) {
        return {
            code: line.trim(),
            num: i + 1
        };
    }).filter(function(line) {
        return !line.code;
    }).map(function(line) {
        return line.num;
    });

    return emptyLines;
}

/**
 * Return an array with with any line numbers that contain comments.
 * @param {Array} comments An array of comment nodes.
 * @returns {Array} An array of line numbers.
 */
function getCommentLineNums(comments) {
    var lines = [];

    comments.forEach(function(token) {
        var start = token.loc.start.line;
        var end = token.loc.end.line;

        lines.push(start, end);
    });
    return lines;
}

/**
 * Determines if a value is an array.
 * @param {number} val The value we wish to check for in the array..
 * @param {Array} array An array.
 * @returns {boolean} True if the value is in the array..
 */
function contains(val, array) {
    return array.indexOf(val) > -1;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require empty lines around comments",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                type: "object",
                properties: {
                    beforeBlockComment: {
                        type: "boolean"
                    },
                    afterBlockComment: {
                        type: "boolean"
                    },
                    beforeLineComment: {
                        type: "boolean"
                    },
                    afterLineComment: {
                        type: "boolean"
                    },
                    allowBlockStart: {
                        type: "boolean"
                    },
                    allowBlockEnd: {
                        type: "boolean"
                    },
                    allowObjectStart: {
                        type: "boolean"
                    },
                    allowObjectEnd: {
                        type: "boolean"
                    },
                    allowArrayStart: {
                        type: "boolean"
                    },
                    allowArrayEnd: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {

        var options = context.options[0] ? lodash.assign({}, context.options[0]) : {};

        options.beforeLineComment = options.beforeLineComment || false;
        options.afterLineComment = options.afterLineComment || false;
        options.beforeBlockComment = typeof options.beforeBlockComment !== "undefined" ? options.beforeBlockComment : true;
        options.afterBlockComment = options.afterBlockComment || false;
        options.allowBlockStart = options.allowBlockStart || false;
        options.allowBlockEnd = options.allowBlockEnd || false;

        var sourceCode = context.getSourceCode();

        /**
         * Returns whether or not comments are on lines starting with or ending with code
         * @param {ASTNode} node The comment node to check.
         * @returns {boolean} True if the comment is not alone.
         */
        function codeAroundComment(node) {
            var token;

            token = node;
            do {
                token = sourceCode.getTokenOrCommentBefore(token);
            } while (token && (token.type === "Block" || token.type === "Line"));

            if (token && token.loc.end.line === node.loc.start.line) {
                return true;
            }

            token = node;
            do {
                token = sourceCode.getTokenOrCommentAfter(token);
            } while (token && (token.type === "Block" || token.type === "Line"));

            if (token && token.loc.start.line === node.loc.end.line) {
                return true;
            }

            return false;
        }

        /**
         * Returns whether or not comments are inside a node type or not.
         * @param {ASTNode} node The Comment node.
         * @param {ASTNode} parent The Comment parent node.
         * @param {string} nodeType The parent type to check against.
         * @returns {boolean} True if the comment is inside nodeType.
         */
        function isCommentInsideNodeType(node, parent, nodeType) {
            return parent.type === nodeType ||
                (parent.body && parent.body.type === nodeType) ||
                (parent.consequent && parent.consequent.type === nodeType);
        }

        /**
         * Returns whether or not comments are at the parent start or not.
         * @param {ASTNode} node The Comment node.
         * @param {string} nodeType The parent type to check against.
         * @returns {boolean} True if the comment is at parent start.
         */
        function isCommentAtParentStart(node, nodeType) {
            var ancestors = context.getAncestors();
            var parent;

            if (ancestors.length) {
                parent = ancestors.pop();
            }

            return parent && isCommentInsideNodeType(node, parent, nodeType) &&
                    node.loc.start.line - parent.loc.start.line === 1;
        }

        /**
         * Returns whether or not comments are at the parent end or not.
         * @param {ASTNode} node The Comment node.
         * @param {string} nodeType The parent type to check against.
         * @returns {boolean} True if the comment is at parent end.
         */
        function isCommentAtParentEnd(node, nodeType) {
            var ancestors = context.getAncestors();
            var parent;

            if (ancestors.length) {
                parent = ancestors.pop();
            }

            return parent && isCommentInsideNodeType(node, parent, nodeType) &&
                    parent.loc.end.line - node.loc.end.line === 1;
        }

        /**
         * Returns whether or not comments are at the block start or not.
         * @param {ASTNode} node The Comment node.
         * @returns {boolean} True if the comment is at block start.
         */
        function isCommentAtBlockStart(node) {
            return isCommentAtParentStart(node, "ClassBody") || isCommentAtParentStart(node, "BlockStatement") || isCommentAtParentStart(node, "SwitchCase");
        }

        /**
         * Returns whether or not comments are at the block end or not.
         * @param {ASTNode} node The Comment node.
         * @returns {boolean} True if the comment is at block end.
         */
        function isCommentAtBlockEnd(node) {
            return isCommentAtParentEnd(node, "ClassBody") || isCommentAtParentEnd(node, "BlockStatement") || isCommentAtParentEnd(node, "SwitchCase") || isCommentAtParentEnd(node, "SwitchStatement");
        }

        /**
         * Returns whether or not comments are at the object start or not.
         * @param {ASTNode} node The Comment node.
         * @returns {boolean} True if the comment is at object start.
         */
        function isCommentAtObjectStart(node) {
            return isCommentAtParentStart(node, "ObjectExpression") || isCommentAtParentStart(node, "ObjectPattern");
        }

        /**
         * Returns whether or not comments are at the object end or not.
         * @param {ASTNode} node The Comment node.
         * @returns {boolean} True if the comment is at object end.
         */
        function isCommentAtObjectEnd(node) {
            return isCommentAtParentEnd(node, "ObjectExpression") || isCommentAtParentEnd(node, "ObjectPattern");
        }

        /**
         * Returns whether or not comments are at the array start or not.
         * @param {ASTNode} node The Comment node.
         * @returns {boolean} True if the comment is at array start.
         */
        function isCommentAtArrayStart(node) {
            return isCommentAtParentStart(node, "ArrayExpression") || isCommentAtParentStart(node, "ArrayPattern");
        }

        /**
         * Returns whether or not comments are at the array end or not.
         * @param {ASTNode} node The Comment node.
         * @returns {boolean} True if the comment is at array end.
         */
        function isCommentAtArrayEnd(node) {
            return isCommentAtParentEnd(node, "ArrayExpression") || isCommentAtParentEnd(node, "ArrayPattern");
        }

        /**
         * Checks if a comment node has lines around it (ignores inline comments)
         * @param {ASTNode} node The Comment node.
         * @param {Object} opts Options to determine the newline.
         * @param {Boolean} opts.after Should have a newline after this line.
         * @param {Boolean} opts.before Should have a newline before this line.
         * @returns {void}
         */
        function checkForEmptyLine(node, opts) {

            var lines = context.getSourceLines(),
                numLines = lines.length + 1,
                comments = context.getAllComments(),
                commentLines = getCommentLineNums(comments),
                emptyLines = getEmptyLineNums(lines),
                commentAndEmptyLines = commentLines.concat(emptyLines);

            var after = opts.after,
                before = opts.before;

            var prevLineNum = node.loc.start.line - 1,
                nextLineNum = node.loc.end.line + 1,
                commentIsNotAlone = codeAroundComment(node);

            var blockStartAllowed = options.allowBlockStart && isCommentAtBlockStart(node),
                blockEndAllowed = options.allowBlockEnd && isCommentAtBlockEnd(node),
                objectStartAllowed = options.allowObjectStart && isCommentAtObjectStart(node),
                objectEndAllowed = options.allowObjectEnd && isCommentAtObjectEnd(node),
                arrayStartAllowed = options.allowArrayStart && isCommentAtArrayStart(node),
                arrayEndAllowed = options.allowArrayEnd && isCommentAtArrayEnd(node);

            var exceptionStartAllowed = blockStartAllowed || objectStartAllowed || arrayStartAllowed;
            var exceptionEndAllowed = blockEndAllowed || objectEndAllowed || arrayEndAllowed;

            // ignore top of the file and bottom of the file
            if (prevLineNum < 1) {
                before = false;
            }
            if (nextLineNum >= numLines) {
                after = false;
            }

            // we ignore all inline comments
            if (commentIsNotAlone) {
                return;
            }

            // check for newline before
            if (!exceptionStartAllowed && before && !contains(prevLineNum, commentAndEmptyLines)) {
                context.report(node, "Expected line before comment.");
            }

            // check for newline after
            if (!exceptionEndAllowed && after && !contains(nextLineNum, commentAndEmptyLines)) {
                context.report(node, "Expected line after comment.");
            }

        }

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {

            LineComment: function(node) {
                if (options.beforeLineComment || options.afterLineComment) {
                    checkForEmptyLine(node, {
                        after: options.afterLineComment,
                        before: options.beforeLineComment
                    });
                }
            },

            BlockComment: function(node) {
                if (options.beforeBlockComment || options.afterBlockComment) {
                    checkForEmptyLine(node, {
                        after: options.afterBlockComment,
                        before: options.beforeBlockComment
                    });
                }
            }

        };
    }
};
