/**
 * @fileoverview enforce "for" loop update clause moving the counter in the right direction.(for-direction)
 * @author Aladdin-ADD<hh_2013@foxmail.com>
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce \"for\" loop update clause moving the counter in the right direction.",
            category: "Possible Errors",
            recommended: true,
            url: "https://eslint.org/docs/rules/for-direction"
        },
        fixable: null,
        schema: [],
        messages: {
            incorrectDirection: "The update clause in this loop moves the variable in the wrong direction."
        }
    },

    create(context) {

        /**
         * report an error.
         * @param {ASTNode} node the node to report.
         * @returns {void}
         */
        function report(node) {
            context.report({
                node,
                messageId: "incorrectDirection"
            });
        }

        /**
         * check UpdateExpression add/sub the counter
         * @param {ASTNode} update UpdateExpression to check
         * @param {string} counter variable name to check
         * @returns {int} if add return 1, if sub return -1, if nochange, return 0
         */
        function getUpdateDirection(update, counter) {
            if (update.argument.type === "Identifier" && update.argument.name === counter) {
                if (update.operator === "++") {
                    return 1;
                }
                if (update.operator === "--") {
                    return -1;
                }
            }
            return 0;
        }

        /**
         * check AssignmentExpression add/sub the counter
         * @param {ASTNode} update AssignmentExpression to check
         * @param {string} counter variable name to check
         * @returns {int} if add return 1, if sub return -1, if nochange, return 0
         */
        function getAssignmentDirection(update, counter) {
            if (update.left.name === counter) {
                if (update.operator === "+=") {
                    return 1;
                }
                if (update.operator === "-=") {
                    return -1;
                }
            }
            return 0;
        }
        return {
            ForStatement(node) {

                if (node.test && node.test.type === "BinaryExpression" && node.test.left.type === "Identifier" && node.update) {
                    const counter = node.test.left.name;
                    const operator = node.test.operator;
                    const update = node.update;

                    if (operator === "<" || operator === "<=") {

                        // report error if update sub the counter (--, -=)
                        if (update.type === "UpdateExpression" && getUpdateDirection(update, counter) < 0) {
                            report(node);
                        }

                        if (update.type === "AssignmentExpression" && getAssignmentDirection(update, counter) < 0) {
                            report(node);
                        }
                    } else if (operator === ">" || operator === ">=") {

                        // report error if update add the counter (++, +=)
                        if (update.type === "UpdateExpression" && getUpdateDirection(update, counter) > 0) {
                            report(node);
                        }

                        if (update.type === "AssignmentExpression" && getAssignmentDirection(update, counter) > 0) {
                            report(node);
                        }
                    }
                }
            }
        };
    }
};
