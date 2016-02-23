/**
 * @fileoverview Rule to enforce var declarations are only at the top of a function.
 * @author Danny Fritz
 * @author Gyandeep Singh
 * @copyright 2014 Danny Fritz. All rights reserved.
 * @copyright 2014 Gyandeep Singh. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var errorMessage = "All 'var' declarations must be at the top of the function scope.";

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * @param {ASTNode} node - any node
     * @returns {Boolean} whether the given node structurally represents a directive
     */
    function looksLikeDirective(node) {
        return node.type === "ExpressionStatement" &&
            node.expression.type === "Literal" && typeof node.expression.value === "string";
    }

    /**
     * Check to see if its a ES6 import declaration
     * @param {ASTNode} node - any node
     * @returns {Boolean} whether the given node represents a import declaration
     */
    function looksLikeImport(node) {
        return node.type === "ImportDeclaration" || node.type === "ImportSpecifier" ||
            node.type === "ImportDefaultSpecifier" || node.type === "ImportNamespaceSpecifier";
    }

    /**
     * Checks whether this variable is on top of the block body
     * @param {ASTNode} node - The node to check
     * @param {ASTNode[]} statements - collection of ASTNodes for the parent node block
     * @returns {Boolean} True if var is on top otherwise false
     */
    function isVarOnTop(node, statements) {
        var i = 0, l = statements.length;

        // skip over directives
        for (; i < l; ++i) {
            if (!looksLikeDirective(statements[i]) && !looksLikeImport(statements[i])) {
                break;
            }
        }

        for (; i < l; ++i) {
            if (statements[i].type !== "VariableDeclaration") {
                return false;
            }
            if (statements[i] === node) {
                return true;
            }
        }

        return false;
    }

    /**
     * Checks whether variable is on top at the global level
     * @param {ASTNode} node - The node to check
     * @param {ASTNode} parent - Parent of the node
     * @returns {void}
     */
    function globalVarCheck(node, parent) {
        if (!isVarOnTop(node, parent.body)) {
            context.report(node, errorMessage);
        }
    }

    /**
     * Checks whether variable is on top at functional block scope level
     * @param {ASTNode} node - The node to check
     * @param {ASTNode} parent - Parent of the node
     * @param {ASTNode} grandParent - Parent of the node's parent
     * @returns {void}
     */
    function blockScopeVarCheck(node, parent, grandParent) {
        if (!(/Function/.test(grandParent.type) &&
                parent.type === "BlockStatement" &&
                isVarOnTop(node, parent.body))) {
            context.report(node, errorMessage);
        }
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {
        "VariableDeclaration": function(node) {
            var ancestors = context.getAncestors();
            var parent = ancestors.pop();
            var grandParent = ancestors.pop();

            if (node.kind === "var") { // check variable is `var` type and not `let` or `const`
                if (parent.type === "Program") { // That means its a global variable
                    globalVarCheck(node, parent);
                } else {
                    blockScopeVarCheck(node, parent, grandParent);
                }
            }
        }
    };

};

module.exports.schema = [];
