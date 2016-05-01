/**
 * @fileoverview Checks for unreachable code due to return, throws, break, and continue.
 * @author Joel Feenstra
 */
"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether or not a given variable declarator has the initializer.
 * @param {ASTNode} node - A VariableDeclarator node to check.
 * @returns {boolean} `true` if the node has the initializer.
 */
function isInitialized(node) {
    return Boolean(node.init);
}

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

module.exports = {
    meta: {
        docs: {
            description: "disallow unreachable code after `return`, `throw`, `continue`, and `break` statements",
            category: "Possible Errors",
            recommended: true
        },

        schema: []
    },

    create: function(context) {
        var currentCodePath = null;

        /**
         * Reports a given node if it's unreachable.
         * @param {ASTNode} node - A statement node to report.
         * @returns {void}
         */
        function reportIfUnreachable(node) {
            if (currentCodePath.currentSegments.every(isUnreachable)) {
                context.report({message: "Unreachable code.", node: node});
            }
        }

        return {

            // Manages the current code path.
            onCodePathStart: function(codePath) {
                currentCodePath = codePath;
            },

            onCodePathEnd: function() {
                currentCodePath = currentCodePath.upper;
            },

            // Registers for all statement nodes (excludes FunctionDeclaration).
            BlockStatement: reportIfUnreachable,
            BreakStatement: reportIfUnreachable,
            ClassDeclaration: reportIfUnreachable,
            ContinueStatement: reportIfUnreachable,
            DebuggerStatement: reportIfUnreachable,
            DoWhileStatement: reportIfUnreachable,
            EmptyStatement: reportIfUnreachable,
            ExpressionStatement: reportIfUnreachable,
            ForInStatement: reportIfUnreachable,
            ForOfStatement: reportIfUnreachable,
            ForStatement: reportIfUnreachable,
            IfStatement: reportIfUnreachable,
            ImportDeclaration: reportIfUnreachable,
            LabeledStatement: reportIfUnreachable,
            ReturnStatement: reportIfUnreachable,
            SwitchStatement: reportIfUnreachable,
            ThrowStatement: reportIfUnreachable,
            TryStatement: reportIfUnreachable,

            VariableDeclaration: function(node) {
                if (node.kind !== "var" || node.declarations.some(isInitialized)) {
                    reportIfUnreachable(node);
                }
            },

            WhileStatement: reportIfUnreachable,
            WithStatement: reportIfUnreachable,
            ExportNamedDeclaration: reportIfUnreachable,
            ExportDefaultDeclaration: reportIfUnreachable,
            ExportAllDeclaration: reportIfUnreachable
        };
    }
};
