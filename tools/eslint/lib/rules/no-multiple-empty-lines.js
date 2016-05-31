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
        var max = 2,
            maxEOF,
            maxBOF;

        // store lines that appear empty but really aren't
        var notEmpty = [];

        if (context.options.length) {
            max = context.options[0].max;
            maxEOF = context.options[0].maxEOF;
            maxBOF = context.options[0].maxBOF;
        }

        var sourceCode = context.getSourceCode();

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {

            TemplateLiteral: function(node) {
                var start = node.loc.start.line;
                var end = node.loc.end.line;

                while (start <= end) {
                    notEmpty.push(start);
                    start++;
                }
            },

            "Program:exit": function checkBlankLines(node) {
                var lines = sourceCode.lines,
                    currentLocation = -1,
                    lastLocation,
                    blankCounter = 0,
                    location,
                    firstOfEndingBlankLines,
                    firstNonBlankLine = -1,
                    trimmedLines = [];

                lines.forEach(function(str, i) {
                    var trimmed = str.trim();

                    if ((firstNonBlankLine === -1) && (trimmed !== "")) {
                        firstNonBlankLine = i;
                    }

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
                    currentLocation = firstNonBlankLine - 1;

                    context.report(node, 0,
                            "Too many blank lines at the beginning of file. Max of " + maxBOF + " allowed.");
                }

                lastLocation = currentLocation;
                currentLocation = trimmedLines.indexOf("", currentLocation + 1);
                while (currentLocation !== -1) {
                    lastLocation = currentLocation;
                    currentLocation = trimmedLines.indexOf("", currentLocation + 1);
                    if (lastLocation === currentLocation - 1) {
                        blankCounter++;
                    } else {
                        location = {
                            line: lastLocation + 1,
                            column: 1
                        };

                        if (lastLocation < firstOfEndingBlankLines) {

                            // within the file, not at the end
                            if (blankCounter >= max) {
                                context.report({
                                    node: node,
                                    loc: location,
                                    message: "More than " + max + " blank " + (max === 1 ? "line" : "lines") + " not allowed."
                                });
                            }
                        } else {

                            // inside the last blank lines
                            if (blankCounter > maxEOF) {
                                context.report({
                                    node: node,
                                    loc: location,
                                    message: "Too many blank lines at the end of file. Max of " + maxEOF + " allowed."
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
