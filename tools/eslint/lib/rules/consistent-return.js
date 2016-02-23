/**
 * @fileoverview Rule to flag consistent return values
 * @author Nicholas C. Zakas
 */
"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether or not a given code path segment is unreachable.
 * @param {CodePathSegment} segment - A CodePathSegment to check.
 * @returns {boolean} `true` if the segment is unreachable.
 */
function isUnreachable(segment) {
    return !segment.reachable;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var funcInfo = null;

    /**
     * Checks whether of not the implicit returning is consistent if the last
     * code path segment is reachable.
     *
     * @param {ASTNode} node - A program/function node to check.
     * @returns {void}
     */
    function checkLastSegment(node) {
        var loc, type;

        // Skip if it expected no return value or unreachable.
        // When unreachable, all paths are returned or thrown.
        if (!funcInfo.hasReturnValue ||
            funcInfo.codePath.currentSegments.every(isUnreachable)
        ) {
            return;
        }

        // Adjust a location and a message.
        if (node.type === "Program") {
            // The head of program.
            loc = {line: 1, column: 0};
            type = "program";
        } else if (node.type === "ArrowFunctionExpression") {
            // `=>` token
            loc = context.getSourceCode().getTokenBefore(node.body).loc.start;
            type = "function";
        } else if (
            node.parent.type === "MethodDefinition" ||
            (node.parent.type === "Property" && node.parent.method)
        ) {
            // Method name.
            loc = node.parent.key.loc.start;
            type = "method";
        } else {
            // Function name or `function` keyword.
            loc = (node.id || node).loc.start;
            type = "function";
        }

        // Reports.
        context.report({
            node: node,
            loc: loc,
            message: "Expected to return a value at the end of this {{type}}.",
            data: {type: type}
        });
    }

    return {
        // Initializes/Disposes state of each code path.
        "onCodePathStart": function(codePath) {
            funcInfo = {
                upper: funcInfo,
                codePath: codePath,
                hasReturn: false,
                hasReturnValue: false,
                message: ""
            };
        },
        "onCodePathEnd": function() {
            funcInfo = funcInfo.upper;
        },

        // Reports a given return statement if it's inconsistent.
        "ReturnStatement": function(node) {
            var hasReturnValue = Boolean(node.argument);

            if (!funcInfo.hasReturn) {
                funcInfo.hasReturn = true;
                funcInfo.hasReturnValue = hasReturnValue;
                funcInfo.message = "Expected " + (hasReturnValue ? "a" : "no") + " return value.";
            } else if (funcInfo.hasReturnValue !== hasReturnValue) {
                context.report({node: node, message: funcInfo.message});
            }
        },

        // Reports a given program/function if the implicit returning is not consistent.
        "Program:exit": checkLastSegment,
        "FunctionDeclaration:exit": checkLastSegment,
        "FunctionExpression:exit": checkLastSegment,
        "ArrowFunctionExpression:exit": checkLastSegment
    };
};

module.exports.schema = [];
