/**
 * @fileoverview enforce the location of arrow function bodies
 * @author Sharmila Jesupaul
 */
"use strict";

const {
    isArrowToken,
    isParenthesised
} = require("../util/ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------
module.exports = {
    meta: {
        type: "layout",

        docs: {
            description: "enforce the location of arrow function bodies",
            category: "Stylistic Issues",
            recommended: false,
            url: "https://eslint.org/docs/rules/implicit-arrow-linebreak"
        },

        fixable: "whitespace",

        schema: [
            {
                enum: ["beside", "below"]
            }
        ],
        messages: {
            expected: "Expected a linebreak before this expression.",
            unexpected: "Expected no linebreak before this expression."
        }
    },

    create(context) {
        const sourceCode = context.getSourceCode();

        //----------------------------------------------------------------------
        // Helpers
        //----------------------------------------------------------------------
        /**
         * Gets the applicable preference for a particular keyword
         * @returns {string} The applicable option for the keyword, e.g. 'beside'
         */
        function getOption() {
            return context.options[0] || "beside";
        }

        /**
         * Formats the comments depending on whether it's a line or block comment.
         * @param {Comment[]} comments The array of comments between the arrow and body
         * @param {Integer} column The column number of the first token
         * @returns {string} A string of comment text joined by line breaks
         */
        function formatComments(comments) {

            return `${comments.map(comment => {

                if (comment.type === "Line") {
                    return `//${comment.value}`;
                }

                return `/*${comment.value}*/`;
            }).join("\n")}\n`;
        }

        /**
         * Finds the first token to prepend comments to depending on the parent type
         * @param {ASTNode} node The validated node
         * @returns {Token|Node} The node to prepend comments to
         */
        function findFirstToken(node) {
            switch (node.parent.type) {
                case "VariableDeclarator":

                    // If the parent is first or only declarator, return the declaration, else, declarator
                    return sourceCode.getFirstToken(
                        node.parent.parent.declarations.length === 1 ||
                        node.parent.parent.declarations[0].id.name === node.parent.id.name
                            ? node.parent.parent : node.parent
                    );
                case "CallExpression":
                case "Property":

                    // find the object key
                    return sourceCode.getFirstToken(node.parent);
                default:
                    return node;
            }
        }

        /**
         * Helper function for adding parentheses fixes for nodes containing nested arrow functions
         * @param {Fixer} fixer Fixer
         * @param {Token} arrow - The arrow token
         * @param {ASTNode} arrowBody - The arrow function body
         * @returns {Function[]} autofixer -- wraps function bodies with parentheses
         */
        function addParentheses(fixer, arrow, arrowBody) {
            const parenthesesFixes = [];
            let closingParentheses = "";

            let followingBody = arrowBody;
            let currentArrow = arrow;

            while (currentArrow && followingBody.type !== "BlockStatement") {
                if (!isParenthesised(sourceCode, followingBody)) {
                    parenthesesFixes.push(
                        fixer.insertTextAfter(currentArrow, " (")
                    );

                    closingParentheses = `\n)${closingParentheses}`;
                }

                currentArrow = sourceCode.getTokenAfter(currentArrow, isArrowToken);

                if (currentArrow) {
                    followingBody = followingBody.body;
                }
            }

            return [...parenthesesFixes,
                fixer.insertTextAfter(arrowBody, closingParentheses)
            ];
        }

        /**
         * Autofixes the function body to collapse onto the same line as the arrow.
         * If comments exist, checks if the function body contains arrow functions, and appends the body with parentheses.
         * Otherwise, prepends the comments before the arrow function.
         * @param {Token} arrowToken The arrow token.
         * @param {ASTNode|Token} arrowBody the function body
         * @param {ASTNode} node The evaluated node
         * @returns {Function} autofixer -- validates the node to adhere to besides
         */
        function autoFixBesides(arrowToken, arrowBody, node) {
            return fixer => {
                const placeBesides = fixer.replaceTextRange([arrowToken.range[1], arrowBody.range[0]], " ");

                const comments = sourceCode.getCommentsInside(node).filter(comment =>
                    comment.loc.start.line < arrowBody.loc.start.line);

                if (comments.length) {

                    // If the grandparent is not a variable declarator
                    if (
                        arrowBody.parent &&
                        arrowBody.parent.parent &&
                        arrowBody.parent.parent.type !== "VariableDeclarator"
                    ) {

                        // If any arrow functions follow, return the necessary parens fixes.
                        if (node.body.type === "ArrowFunctionExpression" &&
                            arrowBody.parent.parent.type !== "VariableDeclarator"
                        ) {
                            return addParentheses(fixer, arrowToken, arrowBody);
                        }
                    }

                    const firstToken = findFirstToken(node);

                    const commentBeforeExpression = fixer.insertTextBeforeRange(
                        firstToken.range,
                        formatComments(comments)
                    );

                    return [placeBesides, commentBeforeExpression];
                }

                return placeBesides;
            };
        }

        /**
         * Validates the location of an arrow function body
         * @param {ASTNode} node The arrow function body
         * @returns {void}
         */
        function validateExpression(node) {
            const option = getOption();

            let tokenBefore = sourceCode.getTokenBefore(node.body);
            const hasParens = tokenBefore.value === "(";

            if (node.type === "BlockStatement") {
                return;
            }

            let fixerTarget = node.body;

            if (hasParens) {

                // Gets the first token before the function body that is not an open paren
                tokenBefore = sourceCode.getTokenBefore(node.body, token => token.value !== "(");
                fixerTarget = sourceCode.getTokenAfter(tokenBefore);
            }

            if (tokenBefore.loc.end.line === fixerTarget.loc.start.line && option === "below") {
                context.report({
                    node: fixerTarget,
                    messageId: "expected",
                    fix: fixer => fixer.insertTextBefore(fixerTarget, "\n")
                });
            } else if (tokenBefore.loc.end.line !== fixerTarget.loc.start.line && option === "beside") {
                context.report({
                    node: fixerTarget,
                    messageId: "unexpected",
                    fix: autoFixBesides(tokenBefore, fixerTarget, node)
                });
            }
        }

        //----------------------------------------------------------------------
        // Public
        //----------------------------------------------------------------------
        return {
            ArrowFunctionExpression: node => validateExpression(node)
        };
    }
};
