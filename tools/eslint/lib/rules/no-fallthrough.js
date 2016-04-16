/**
 * @fileoverview Rule to flag fall-through cases in switch statements.
 * @author Matt DuVall <http://mattduvall.com/>
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var lodash = require("lodash");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var FALLTHROUGH_COMMENT = /falls?\s?through/i;

/**
 * Checks whether or not a given node has a fallthrough comment.
 * @param {ASTNode} node - A SwitchCase node to get comments.
 * @param {RuleContext} context - A rule context which stores comments.
 * @returns {boolean} `true` if the node has a fallthrough comment.
 */
function hasFallthroughComment(node, context) {
    var sourceCode = context.getSourceCode();
    var comment = lodash.last(sourceCode.getComments(node).leading);

    return Boolean(comment && FALLTHROUGH_COMMENT.test(comment.value));
}

/**
 * Checks whether or not a given code path segment is reachable.
 * @param {CodePathSegment} segment - A CodePathSegment to check.
 * @returns {boolean} `true` if the segment is reachable.
 */
function isReachable(segment) {
    return segment.reachable;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var currentCodePath = null;

    /*
     * We need to use leading comments of the next SwitchCase node because
     * trailing comments is wrong if semicolons are omitted.
     */
    var fallthroughCase = null;

    return {
        "onCodePathStart": function(codePath) {
            currentCodePath = codePath;
        },
        "onCodePathEnd": function() {
            currentCodePath = currentCodePath.upper;
        },

        "SwitchCase": function(node) {

            /*
             * Checks whether or not there is a fallthrough comment.
             * And reports the previous fallthrough node if that does not exist.
             */
            if (fallthroughCase && !hasFallthroughComment(node, context)) {
                context.report({
                    message: "Expected a 'break' statement before '{{type}}'.",
                    data: {type: node.test ? "case" : "default"},
                    node: node
                });
            }
            fallthroughCase = null;
        },

        "SwitchCase:exit": function(node) {

            /*
             * `reachable` meant fall through because statements preceded by
             * `break`, `return`, or `throw` are unreachable.
             * And allows empty cases and the last case.
             */
            if (currentCodePath.currentSegments.some(isReachable) &&
                node.consequent.length > 0 &&
                lodash.last(node.parent.cases) !== node
            ) {
                fallthroughCase = node;
            }
        }
    };
};

module.exports.schema = [];
