/**
 * @fileoverview Rule to flag blocks with no reason to exist
 * @author Brandon Mills
 * @copyright 2015 Roberto Vidal. All rights reserved.
 * @copyright 2014 Brandon Mills. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    // A stack of lone blocks to be checked for block-level bindings
    var loneBlocks = [],
        ruleDef;

    /**
     * Reports a node as invalid.
     * @param {ASTNode} node - The node to be reported.
     * @returns {void}
    */
    function report(node) {
        var parent = context.getAncestors().pop();
        context.report(node, parent.type === "Program" ?
            "Block is redundant." :
            "Nested block is redundant."
        );
    }

    /**
     * Checks for any ocurrence of BlockStatement > BlockStatement or Program > BlockStatement
     * @returns {boolean} True if the current node is a lone block.
    */
    function isLoneBlock() {
        var parent = context.getAncestors().pop();
        return parent.type === "BlockStatement" || parent.type === "Program";
    }

    /**
     * Checks the enclosing block of the current node for block-level bindings,
     * and "marks it" as valid if any.
     * @returns {void}
    */
    function markLoneBlock() {
        if (loneBlocks.length === 0) {
            return;
        }

        var block = context.getAncestors().pop();

        if (loneBlocks[loneBlocks.length - 1] === block) {
            loneBlocks.pop();
        }
    }

    // Default rule definition: report all lone blocks
    ruleDef = {
        BlockStatement: function(node) {
            if (isLoneBlock(node)) {
                report(node);
            }
        }
    };

    // ES6: report blocks without block-level bindings
    if (context.parserOptions.ecmaVersion >= 6) {
        ruleDef = {
            "BlockStatement": function(node) {
                if (isLoneBlock(node)) {
                    loneBlocks.push(node);
                }
            },
            "BlockStatement:exit": function(node) {
                if (loneBlocks.length > 0 && loneBlocks[loneBlocks.length - 1] === node) {
                    loneBlocks.pop();
                    report(node);
                }
            }
        };

        ruleDef.VariableDeclaration = function(node) {
            if (node.kind === "let" || node.kind === "const") {
                markLoneBlock(node);
            }
        };

        ruleDef.FunctionDeclaration = function(node) {
            if (context.getScope().isStrict) {
                markLoneBlock(node);
            }
        };

        ruleDef.ClassDeclaration = markLoneBlock;
    }

    return ruleDef;
};

module.exports.schema = [];
