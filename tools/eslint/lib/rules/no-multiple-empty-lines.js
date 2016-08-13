/**
 * @fileoverview Disallows multiple blank lines.
 * implementation adapted from the no-trailing-spaces rule.
 * @author Greg Cochard
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow multiple empty lines",
            category: "Stylistic Issues",
            recommended: false
        },

        fixable: "whitespace",

        schema: [
            {
                type: "object",
                properties: {
                    max: {
                        type: "integer",
                        minimum: 0
                    },
                    maxEOF: {
                        type: "integer",
                        minimum: 0
                    },
                    maxBOF: {
                        type: "integer",
                        minimum: 0
                    }
                },
                required: ["max"],
                additionalProperties: false
            }
        ]
    },

    create: function(context) {

        // Use options.max or 2 as default
        let max = 2,
            maxEOF,
            maxBOF;

        // store lines that appear empty but really aren't
        const notEmpty = [];

        if (context.options.length) {
            max = context.options[0].max;
            maxEOF = typeof context.options[0].maxEOF !== "undefined" ? context.options[0].maxEOF : max;
            maxBOF = typeof context.options[0].maxBOF !== "undefined" ? context.options[0].maxBOF : max;
        }

        const sourceCode = context.getSourceCode();

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {

            TemplateLiteral: function(node) {
                let start = node.loc.start.line;
                const end = node.loc.end.line;

                while (start <= end) {
                    notEmpty.push(start);
                    start++;
                }
            },

            "Program:exit": function checkBlankLines(node) {
                const lines = sourceCode.lines,
                    fullLines = sourceCode.text.match(/.*(\r\n|\r|\n|\u2028|\u2029)/g) || [],
                    linesRangeStart = [];
                let firstNonBlankLine = -1,
                    trimmedLines = [],
                    blankCounter = 0,
                    currentLocation,
                    lastLocation,
                    firstOfEndingBlankLines,
                    diff,
                    rangeStart,
                    rangeEnd;

                /**
                 * Fix code.
                 * @param {RuleFixer} fixer - The fixer of this context.
                 * @returns {Object} The fixing information.
                 */
                function fix(fixer) {
                    return fixer.removeRange([rangeStart, rangeEnd]);
                }

                linesRangeStart.push(0);
                lines.forEach(function(str, i) {
                    const length = i < fullLines.length ? fullLines[i].length : 0,
                        trimmed = str.trim();

                    if ((firstNonBlankLine === -1) && (trimmed !== "")) {
                        firstNonBlankLine = i;
                    }

                    linesRangeStart.push(linesRangeStart[linesRangeStart.length - 1] + length);
                    trimmedLines.push(trimmed);
                });

                // add the notEmpty lines in there with a placeholder
                notEmpty.forEach(function(x, i) {
                    trimmedLines[i] = x;
                });

                if (typeof maxEOF === "undefined") {

                    /*
                     * Swallow the final newline, as some editors add it
                     * automatically and we don't want it to cause an issue
                     */
                    if (trimmedLines[trimmedLines.length - 1] === "") {
                        trimmedLines = trimmedLines.slice(0, -1);
                    }

                    firstOfEndingBlankLines = trimmedLines.length;
                } else {

                    // save the number of the first of the last blank lines
                    firstOfEndingBlankLines = trimmedLines.length;
                    while (trimmedLines[firstOfEndingBlankLines - 1] === ""
                            && firstOfEndingBlankLines > 0) {
                        firstOfEndingBlankLines--;
                    }
                }

                // Aggregate and count blank lines
                if (firstNonBlankLine > maxBOF) {
                    diff = firstNonBlankLine - maxBOF;
                    rangeStart = linesRangeStart[firstNonBlankLine - diff];
                    rangeEnd = linesRangeStart[firstNonBlankLine];
                    context.report({
                        node: node,
                        loc: node.loc.start,
                        message: "Too many blank lines at the beginning of file. Max of " + maxBOF + " allowed.",
                        fix: fix
                    });
                }
                currentLocation = firstNonBlankLine - 1;

                lastLocation = currentLocation;
                currentLocation = trimmedLines.indexOf("", currentLocation + 1);
                while (currentLocation !== -1) {
                    lastLocation = currentLocation;
                    currentLocation = trimmedLines.indexOf("", currentLocation + 1);
                    if (lastLocation === currentLocation - 1) {
                        blankCounter++;
                    } else {
                        const location = {
                            line: lastLocation + 1,
                            column: 1
                        };

                        if (lastLocation < firstOfEndingBlankLines) {

                            // within the file, not at the end
                            if (blankCounter >= max) {
                                diff = blankCounter - max + 1;
                                rangeStart = linesRangeStart[location.line - diff];
                                rangeEnd = linesRangeStart[location.line];

                                context.report({
                                    node: node,
                                    loc: location,
                                    message: "More than " + max + " blank " + (max === 1 ? "line" : "lines") + " not allowed.",
                                    fix: fix
                                });
                            }
                        } else {

                            // inside the last blank lines
                            if (blankCounter > maxEOF) {
                                diff = blankCounter - maxEOF + 1;
                                rangeStart = linesRangeStart[location.line - diff];
                                rangeEnd = linesRangeStart[location.line - 1];
                                context.report({
                                    node: node,
                                    loc: location,
                                    message: "Too many blank lines at the end of file. Max of " + maxEOF + " allowed.",
                                    fix: fix
                                });
                            }
                        }

                        // Finally, reset the blank counter
                        blankCounter = 0;
                    }
                }
            }
        };

    }
};
