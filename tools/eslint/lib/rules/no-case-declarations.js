/**
 * @fileoverview Rule to flag use of an lexical declarations inside a case clause
 * @author Erik Arvidsson
 * @copyright 2015 Erik Arvidsson. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Checks whether or not a node is a lexical declaration.
     * @param {ASTNode} node A direct child statement of a switch case.
     * @returns {boolean} Whether or not the node is a lexical declaration.
     */
    function isLexicalDeclaration(node) {
        switch (node.type) {
            case "FunctionDeclaration":
            case "ClassDeclaration":
                return true;
            case "VariableDeclaration":
                return node.kind !== "var";
            default:
                return false;
        }
    }

    return {
        "SwitchCase": function(node) {
            for (var i = 0; i < node.consequent.length; i++) {
                var statement = node.consequent[i];
                if (isLexicalDeclaration(statement)) {
                    context.report({
                        node: node,
                        message: "Unexpected lexical declaration in case block."
                    });
                }
            }
        }
    };

};

module.exports.schema = [];
