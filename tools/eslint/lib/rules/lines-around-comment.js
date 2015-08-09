/**
 * @fileoverview Enforces empty lines around comments.
 * @author Jamund Ferguson
 * @copyright 2015 Jamund Ferguson. All rights reserved.
 */
"use strict";

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
    comments.forEach(function (token) {
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

module.exports = function(context) {

    var options = context.options[0] || {};
    options.beforeLineComment = options.beforeLineComment || false;
    options.afterLineComment = options.afterLineComment || false;
    options.beforeBlockComment = typeof options.beforeBlockComment !== "undefined" ? options.beforeBlockComment : true;
    options.afterBlockComment = options.afterBlockComment || false;
    options.allowBlockStart = options.allowBlockStart || false;
    options.allowBlockEnd = options.allowBlockEnd || false;

    /**
     * Returns whether or not comments are not on lines starting with or ending with code
     * @param {ASTNode} node The comment node to check.
     * @returns {boolean} True if the comment is not alone.
     */
    function codeAroundComment(node) {

        var lines = context.getSourceLines();

        // Get the whole line and cut it off at the start of the comment
        var startLine = lines[node.loc.start.line - 1];
        var endLine = lines[node.loc.end.line - 1];

        var preamble = startLine.slice(0, node.loc.start.column).trim();

        // Also check after the comment
        var postamble = endLine.slice(node.loc.end.column).trim();

        // Should be false if there was only whitespace around the comment
        return !!(preamble || postamble);
    }

    /**
     * Returns whether or not comments are at the block start or not.
     * @param {ASTNode} node The Comment node.
     * @returns {boolean} True if the comment is at block start.
     */
    function isCommentAtBlockStart(node) {
        var ancestors = context.getAncestors();
        var parent;

        if (ancestors.length) {
            parent = ancestors.pop();
        }
        return parent && (parent.type === "BlockStatement" || parent.body.type === "BlockStatement") &&
                node.loc.start.line - parent.loc.start.line === 1;
    }

    /**
     * Returns whether or not comments are at the block end or not.
     * @param {ASTNode} node The Comment node.
     * @returns {boolean} True if the comment is at block end.
     */
    function isCommentAtBlockEnd(node) {
        var ancestors = context.getAncestors();
        var parent;

        if (ancestors.length) {
            parent = ancestors.pop();
        }
        return parent && (parent.type === "BlockStatement" || parent.body.type === "BlockStatement") &&
                parent.loc.end.line - node.loc.end.line === 1;
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
            blockEndAllowed = options.allowBlockEnd && isCommentAtBlockEnd(node);

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
        if (!blockStartAllowed && before && !contains(prevLineNum, commentAndEmptyLines)) {
            context.report(node, "Expected line before comment.");
        }

        // check for newline after
        if (!blockEndAllowed && after && !contains(nextLineNum, commentAndEmptyLines)) {
            context.report(node, "Expected line after comment.");
        }

    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {

        "LineComment": function(node) {
            if (options.beforeLineComment || options.afterLineComment) {
                checkForEmptyLine(node, {
                    after: options.afterLineComment,
                    before: options.beforeLineComment
                });
            }
        },

        "BlockComment": function(node) {
            if (options.beforeBlockComment || options.afterBlockComment) {
                checkForEmptyLine(node, {
                    after: options.afterBlockComment,
                    before: options.beforeBlockComment
                });
            }
        }

    };
};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "beforeBlockComment": {
                "type": "boolean"
            },
            "afterBlockComment": {
                "type": "boolean"
            },
            "beforeLineComment": {
                "type": "boolean"
            },
            "afterLineComment": {
                "type": "boolean"
            },
            "allowBlockStart": {
                "type": "boolean"
            },
            "allowBlockEnd": {
                "type": "boolean"
            }
        },
        "additionalProperties": false
    }
];
