/**
 * @fileoverview enforce a particular style for multiline comments
 * @author Teddy Katz
 */
"use strict";

const astUtils = require("../util/ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce a particular style for multiline comments",
            category: "Stylistic Issues",
            recommended: false,
            url: "https://eslint.org/docs/rules/multiline-comment-style"
        },
        fixable: "whitespace",
        schema: [{ enum: ["starred-block", "separate-lines", "bare-block"] }]
    },

    create(context) {
        const sourceCode = context.getSourceCode();
        const option = context.options[0] || "starred-block";

        const EXPECTED_BLOCK_ERROR = "Expected a block comment instead of consecutive line comments.";
        const START_NEWLINE_ERROR = "Expected a linebreak after '/*'.";
        const END_NEWLINE_ERROR = "Expected a linebreak before '*/'.";
        const MISSING_STAR_ERROR = "Expected a '*' at the start of this line.";
        const ALIGNMENT_ERROR = "Expected this line to be aligned with the start of the comment.";
        const EXPECTED_LINES_ERROR = "Expected multiple line comments instead of a block comment.";

        //----------------------------------------------------------------------
        // Helpers
        //----------------------------------------------------------------------

        /**
         * Gets a list of comment lines in a group
         * @param {Token[]} commentGroup A group of comments, containing either multiple line comments or a single block comment
         * @returns {string[]} A list of comment lines
         */
        function getCommentLines(commentGroup) {
            if (commentGroup[0].type === "Line") {
                return commentGroup.map(comment => comment.value);
            }
            return commentGroup[0].value
                .split(astUtils.LINEBREAK_MATCHER)
                .map(line => line.replace(/^\s*\*?/, ""));
        }

        /**
         * Converts a comment into starred-block form
         * @param {Token} firstComment The first comment of the group being converted
         * @param {string[]} commentLinesList A list of lines to appear in the new starred-block comment
         * @returns {string} A representation of the comment value in starred-block form, excluding start and end markers
         */
        function convertToStarredBlock(firstComment, commentLinesList) {
            const initialOffset = sourceCode.text.slice(firstComment.range[0] - firstComment.loc.start.column, firstComment.range[0]);
            const starredLines = commentLinesList.map(line => `${initialOffset} *${line}`);

            return `\n${starredLines.join("\n")}\n${initialOffset} `;
        }

        /**
         * Converts a comment into separate-line form
         * @param {Token} firstComment The first comment of the group being converted
         * @param {string[]} commentLinesList A list of lines to appear in the new starred-block comment
         * @returns {string} A representation of the comment value in separate-line form
         */
        function convertToSeparateLines(firstComment, commentLinesList) {
            const initialOffset = sourceCode.text.slice(firstComment.range[0] - firstComment.loc.start.column, firstComment.range[0]);
            const separateLines = commentLinesList.map(line => `// ${line.trim()}`);

            return separateLines.join(`\n${initialOffset}`);
        }

        /**
         * Converts a comment into bare-block form
         * @param {Token} firstComment The first comment of the group being converted
         * @param {string[]} commentLinesList A list of lines to appear in the new starred-block comment
         * @returns {string} A representation of the comment value in bare-block form
         */
        function convertToBlock(firstComment, commentLinesList) {
            const initialOffset = sourceCode.text.slice(firstComment.range[0] - firstComment.loc.start.column, firstComment.range[0]);
            const blockLines = commentLinesList.map(line => line.trim());

            return `/* ${blockLines.join(`\n${initialOffset}   `)} */`;
        }

        /**
         * Check a comment is JSDoc form
         * @param {Token[]} commentGroup A group of comments, containing either multiple line comments or a single block comment
         * @returns {boolean} if commentGroup is JSDoc form, return true
         */
        function isJSDoc(commentGroup) {
            const lines = commentGroup[0].value.split(astUtils.LINEBREAK_MATCHER);

            return commentGroup[0].type === "Block" &&
                /^\*\s*$/.test(lines[0]) &&
                lines.slice(1, -1).every(line => /^\s* /.test(line)) &&
                /^\s*$/.test(lines[lines.length - 1]);
        }

        /**
         * Each method checks a group of comments to see if it's valid according to the given option.
         * @param {Token[]} commentGroup A list of comments that appear together. This will either contain a single
         * block comment or multiple line comments.
         * @returns {void}
         */
        const commentGroupCheckers = {
            "starred-block"(commentGroup) {
                const commentLines = getCommentLines(commentGroup);

                if (commentLines.some(value => value.includes("*/"))) {
                    return;
                }

                if (commentGroup.length > 1) {
                    context.report({
                        loc: {
                            start: commentGroup[0].loc.start,
                            end: commentGroup[commentGroup.length - 1].loc.end
                        },
                        message: EXPECTED_BLOCK_ERROR,
                        fix(fixer) {
                            const range = [commentGroup[0].range[0], commentGroup[commentGroup.length - 1].range[1]];
                            const starredBlock = `/*${convertToStarredBlock(commentGroup[0], commentLines)}*/`;

                            return commentLines.some(value => value.startsWith("/"))
                                ? null
                                : fixer.replaceTextRange(range, starredBlock);
                        }
                    });
                } else {
                    const block = commentGroup[0];
                    const lines = block.value.split(astUtils.LINEBREAK_MATCHER);
                    const expectedLinePrefix = `${sourceCode.text.slice(block.range[0] - block.loc.start.column, block.range[0])} *`;

                    if (!/^\*?\s*$/.test(lines[0])) {
                        const start = block.value.startsWith("*") ? block.range[0] + 1 : block.range[0];

                        context.report({
                            loc: {
                                start: block.loc.start,
                                end: { line: block.loc.start.line, column: block.loc.start.column + 2 }
                            },
                            message: START_NEWLINE_ERROR,
                            fix: fixer => fixer.insertTextAfterRange([start, start + 2], `\n${expectedLinePrefix}`)
                        });
                    }

                    if (!/^\s*$/.test(lines[lines.length - 1])) {
                        context.report({
                            loc: {
                                start: { line: block.loc.end.line, column: block.loc.end.column - 2 },
                                end: block.loc.end
                            },
                            message: END_NEWLINE_ERROR,
                            fix: fixer => fixer.replaceTextRange([block.range[1] - 2, block.range[1]], `\n${expectedLinePrefix}/`)
                        });
                    }

                    for (let lineNumber = block.loc.start.line + 1; lineNumber <= block.loc.end.line; lineNumber++) {
                        const lineText = sourceCode.lines[lineNumber - 1];

                        if (!lineText.startsWith(expectedLinePrefix)) {
                            context.report({
                                loc: {
                                    start: { line: lineNumber, column: 0 },
                                    end: { line: lineNumber, column: sourceCode.lines[lineNumber - 1].length }
                                },
                                message: /^\s*\*/.test(lineText)
                                    ? ALIGNMENT_ERROR
                                    : MISSING_STAR_ERROR,
                                fix(fixer) {
                                    const lineStartIndex = sourceCode.getIndexFromLoc({ line: lineNumber, column: 0 });
                                    const linePrefixLength = lineText.match(/^\s*\*? ?/)[0].length;
                                    const commentStartIndex = lineStartIndex + linePrefixLength;

                                    const replacementText = lineNumber === block.loc.end.line || lineText.length === linePrefixLength
                                        ? expectedLinePrefix
                                        : `${expectedLinePrefix} `;

                                    return fixer.replaceTextRange([lineStartIndex, commentStartIndex], replacementText);
                                }
                            });
                        }
                    }
                }
            },
            "separate-lines"(commentGroup) {
                if (!isJSDoc(commentGroup) && commentGroup[0].type === "Block") {
                    const commentLines = getCommentLines(commentGroup);
                    const block = commentGroup[0];
                    const tokenAfter = sourceCode.getTokenAfter(block, { includeComments: true });

                    if (tokenAfter && block.loc.end.line === tokenAfter.loc.start.line) {
                        return;
                    }

                    context.report({
                        loc: {
                            start: block.loc.start,
                            end: { line: block.loc.start.line, column: block.loc.start.column + 2 }
                        },
                        message: EXPECTED_LINES_ERROR,
                        fix(fixer) {
                            return fixer.replaceText(block, convertToSeparateLines(block, commentLines.filter(line => line)));
                        }
                    });
                }
            },
            "bare-block"(commentGroup) {
                if (!isJSDoc(commentGroup)) {
                    const commentLines = getCommentLines(commentGroup);

                    // disallows consecutive line comments in favor of using a block comment.
                    if (commentGroup[0].type === "Line" && commentLines.length > 1 &&
                        !commentLines.some(value => value.includes("*/"))) {
                        context.report({
                            loc: {
                                start: commentGroup[0].loc.start,
                                end: commentGroup[commentGroup.length - 1].loc.end
                            },
                            message: EXPECTED_BLOCK_ERROR,
                            fix(fixer) {
                                const range = [commentGroup[0].range[0], commentGroup[commentGroup.length - 1].range[1]];
                                const block = convertToBlock(commentGroup[0], commentLines.filter(line => line));

                                return fixer.replaceTextRange(range, block);
                            }
                        });
                    }

                    // prohibits block comments from having a * at the beginning of each line.
                    if (commentGroup[0].type === "Block") {
                        const block = commentGroup[0];
                        const lines = block.value.split(astUtils.LINEBREAK_MATCHER).filter(line => line.trim());

                        if (lines.length > 0 && lines.every(line => /^\s*\*/.test(line))) {
                            context.report({
                                loc: {
                                    start: block.loc.start,
                                    end: { line: block.loc.start.line, column: block.loc.start.column + 2 }
                                },
                                message: EXPECTED_BLOCK_ERROR,
                                fix(fixer) {
                                    return fixer.replaceText(block, convertToBlock(block, commentLines.filter(line => line)));
                                }
                            });
                        }
                    }
                }
            }
        };

        //----------------------------------------------------------------------
        // Public
        //----------------------------------------------------------------------

        return {
            Program() {
                return sourceCode.getAllComments()
                    .filter(comment => comment.type !== "Shebang")
                    .filter(comment => !astUtils.COMMENTS_IGNORE_PATTERN.test(comment.value))
                    .filter(comment => {
                        const tokenBefore = sourceCode.getTokenBefore(comment, { includeComments: true });

                        return !tokenBefore || tokenBefore.loc.end.line < comment.loc.start.line;
                    })
                    .reduce((commentGroups, comment, index, commentList) => {
                        const tokenBefore = sourceCode.getTokenBefore(comment, { includeComments: true });

                        if (
                            comment.type === "Line" &&
                            index && commentList[index - 1].type === "Line" &&
                            tokenBefore && tokenBefore.loc.end.line === comment.loc.start.line - 1 &&
                            tokenBefore === commentList[index - 1]
                        ) {
                            commentGroups[commentGroups.length - 1].push(comment);
                        } else {
                            commentGroups.push([comment]);
                        }

                        return commentGroups;
                    }, [])
                    .filter(commentGroup => !(commentGroup.length === 1 && commentGroup[0].loc.start.line === commentGroup[0].loc.end.line))
                    .forEach(commentGroupCheckers[option]);
            }
        };
    }
};
