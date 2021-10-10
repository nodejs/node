/**
 * @fileoverview A module that filters reported problems based on `eslint-disable` and `eslint-enable` comments
 * @author Teddy Katz
 */

"use strict";

const escapeRegExp = require("escape-string-regexp");

/**
 * Compares the locations of two objects in a source file
 * @param {{line: number, column: number}} itemA The first object
 * @param {{line: number, column: number}} itemB The second object
 * @returns {number} A value less than 1 if itemA appears before itemB in the source file, greater than 1 if
 * itemA appears after itemB in the source file, or 0 if itemA and itemB have the same location.
 */
function compareLocations(itemA, itemB) {
    return itemA.line - itemB.line || itemA.column - itemB.column;
}

/**
 * Groups a set of directives into sub-arrays by their parent comment.
 * @param {Directive[]} directives Unused directives to be removed.
 * @returns {Directive[][]} Directives grouped by their parent comment.
 */
function groupByParentComment(directives) {
    const groups = new Map();

    for (const directive of directives) {
        const { unprocessedDirective: { parentComment } } = directive;

        if (groups.has(parentComment)) {
            groups.get(parentComment).push(directive);
        } else {
            groups.set(parentComment, [directive]);
        }
    }

    return [...groups.values()];
}

/**
 * Creates removal details for a set of directives within the same comment.
 * @param {Directive[]} directives Unused directives to be removed.
 * @param {Token} commentToken The backing Comment token.
 * @returns {{ description, fix, position }[]} Details for later creation of output Problems.
 */
function createIndividualDirectivesRemoval(directives, commentToken) {

    /*
     * `commentToken.value` starts right after `//` or `/*`.
     * All calculated offsets will be relative to this index.
     */
    const commentValueStart = commentToken.range[0] + "//".length;

    // Find where the list of rules starts. `\S+` matches with the directive name (e.g. `eslint-disable-line`)
    const listStartOffset = /^\s*\S+\s+/u.exec(commentToken.value)[0].length;

    /*
     * Get the list text without any surrounding whitespace. In order to preserve the original
     * formatting, we don't want to change that whitespace.
     *
     *     // eslint-disable-line rule-one , rule-two , rule-three -- comment
     *                            ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
     */
    const listText = commentToken.value
        .slice(listStartOffset) // remove directive name and all whitespace before the list
        .split(/\s-{2,}\s/u)[0] // remove `-- comment`, if it exists
        .trimRight(); // remove all whitespace after the list

    /*
     * We can assume that `listText` contains multiple elements.
     * Otherwise, this function wouldn't be called - if there is
     * only one rule in the list, then the whole comment must be removed.
     */

    return directives.map(directive => {
        const { ruleId } = directive;

        const regex = new RegExp(String.raw`(?:^|\s*,\s*)${escapeRegExp(ruleId)}(?:\s*,\s*|$)`, "u");
        const match = regex.exec(listText);
        const matchedText = match[0];
        const matchStartOffset = listStartOffset + match.index;
        const matchEndOffset = matchStartOffset + matchedText.length;

        const firstIndexOfComma = matchedText.indexOf(",");
        const lastIndexOfComma = matchedText.lastIndexOf(",");

        let removalStartOffset, removalEndOffset;

        if (firstIndexOfComma !== lastIndexOfComma) {

            /*
             * Since there are two commas, this must one of the elements in the middle of the list.
             * Matched range starts where the previous rule name ends, and ends where the next rule name starts.
             *
             *     // eslint-disable-line rule-one , rule-two , rule-three -- comment
             *                                    ^^^^^^^^^^^^^^
             *
             * We want to remove only the content between the two commas, and also one of the commas.
             *
             *     // eslint-disable-line rule-one , rule-two , rule-three -- comment
             *                                     ^^^^^^^^^^^
             */
            removalStartOffset = matchStartOffset + firstIndexOfComma;
            removalEndOffset = matchStartOffset + lastIndexOfComma;

        } else {

            /*
             * This is either the first element or the last element.
             *
             * If this is the first element, matched range starts where the first rule name starts
             * and ends where the second rule name starts. This is exactly the range we want
             * to remove so that the second rule name will start where the first one was starting
             * and thus preserve the original formatting.
             *
             *     // eslint-disable-line rule-one , rule-two , rule-three -- comment
             *                            ^^^^^^^^^^^
             *
             * Similarly, if this is the last element, we've already matched the range we want to
             * remove. The previous rule name will end where the last one was ending, relative
             * to the content on the right side.
             *
             *     // eslint-disable-line rule-one , rule-two , rule-three -- comment
             *                                               ^^^^^^^^^^^^^
             */
            removalStartOffset = matchStartOffset;
            removalEndOffset = matchEndOffset;
        }

        return {
            description: `'${ruleId}'`,
            fix: {
                range: [
                    commentValueStart + removalStartOffset,
                    commentValueStart + removalEndOffset
                ],
                text: ""
            },
            position: directive.unprocessedDirective
        };
    });
}

/**
 * Creates a description of deleting an entire unused disable comment.
 * @param {Directive[]} directives Unused directives to be removed.
 * @param {Token} commentToken The backing Comment token.
 * @returns {{ description, fix, position }} Details for later creation of an output Problem.
 */
function createCommentRemoval(directives, commentToken) {
    const { range } = commentToken;
    const ruleIds = directives.filter(directive => directive.ruleId).map(directive => `'${directive.ruleId}'`);

    return {
        description: ruleIds.length <= 2
            ? ruleIds.join(" or ")
            : `${ruleIds.slice(0, ruleIds.length - 1).join(", ")}, or ${ruleIds[ruleIds.length - 1]}`,
        fix: {
            range,
            text: " "
        },
        position: directives[0].unprocessedDirective
    };
}

/**
 * Parses details from directives to create output Problems.
 * @param {Directive[]} allDirectives Unused directives to be removed.
 * @returns {{ description, fix, position }[]} Details for later creation of output Problems.
 */
function processUnusedDisableDirectives(allDirectives) {
    const directiveGroups = groupByParentComment(allDirectives);

    return directiveGroups.flatMap(
        directives => {
            const { parentComment } = directives[0].unprocessedDirective;
            const remainingRuleIds = new Set(parentComment.ruleIds);

            for (const directive of directives) {
                remainingRuleIds.delete(directive.ruleId);
            }

            return remainingRuleIds.size
                ? createIndividualDirectivesRemoval(directives, parentComment.commentToken)
                : [createCommentRemoval(directives, parentComment.commentToken)];
        }
    );
}

/**
 * This is the same as the exported function, except that it
 * doesn't handle disable-line and disable-next-line directives, and it always reports unused
 * disable directives.
 * @param {Object} options options for applying directives. This is the same as the options
 * for the exported function, except that `reportUnusedDisableDirectives` is not supported
 * (this function always reports unused disable directives).
 * @returns {{problems: Problem[], unusedDisableDirectives: Problem[]}} An object with a list
 * of filtered problems and unused eslint-disable directives
 */
function applyDirectives(options) {
    const problems = [];
    let nextDirectiveIndex = 0;
    let currentGlobalDisableDirective = null;
    const disabledRuleMap = new Map();

    // enabledRules is only used when there is a current global disable directive.
    const enabledRules = new Set();
    const usedDisableDirectives = new Set();

    for (const problem of options.problems) {
        while (
            nextDirectiveIndex < options.directives.length &&
            compareLocations(options.directives[nextDirectiveIndex], problem) <= 0
        ) {
            const directive = options.directives[nextDirectiveIndex++];

            switch (directive.type) {
                case "disable":
                    if (directive.ruleId === null) {
                        currentGlobalDisableDirective = directive;
                        disabledRuleMap.clear();
                        enabledRules.clear();
                    } else if (currentGlobalDisableDirective) {
                        enabledRules.delete(directive.ruleId);
                        disabledRuleMap.set(directive.ruleId, directive);
                    } else {
                        disabledRuleMap.set(directive.ruleId, directive);
                    }
                    break;

                case "enable":
                    if (directive.ruleId === null) {
                        currentGlobalDisableDirective = null;
                        disabledRuleMap.clear();
                    } else if (currentGlobalDisableDirective) {
                        enabledRules.add(directive.ruleId);
                        disabledRuleMap.delete(directive.ruleId);
                    } else {
                        disabledRuleMap.delete(directive.ruleId);
                    }
                    break;

                // no default
            }
        }

        if (disabledRuleMap.has(problem.ruleId)) {
            usedDisableDirectives.add(disabledRuleMap.get(problem.ruleId));
        } else if (currentGlobalDisableDirective && !enabledRules.has(problem.ruleId)) {
            usedDisableDirectives.add(currentGlobalDisableDirective);
        } else {
            problems.push(problem);
        }
    }

    const unusedDisableDirectivesToReport = options.directives
        .filter(directive => directive.type === "disable" && !usedDisableDirectives.has(directive));

    const processed = processUnusedDisableDirectives(unusedDisableDirectivesToReport);

    const unusedDisableDirectives = processed
        .map(({ description, fix, position }) => ({
            ruleId: null,
            message: description
                ? `Unused eslint-disable directive (no problems were reported from ${description}).`
                : "Unused eslint-disable directive (no problems were reported).",
            line: position.line,
            column: position.column,
            severity: options.reportUnusedDisableDirectives === "warn" ? 1 : 2,
            nodeType: null,
            ...options.disableFixes ? {} : { fix }
        }));

    return { problems, unusedDisableDirectives };
}

/**
 * Given a list of directive comments (i.e. metadata about eslint-disable and eslint-enable comments) and a list
 * of reported problems, determines which problems should be reported.
 * @param {Object} options Information about directives and problems
 * @param {{
 *      type: ("disable"|"enable"|"disable-line"|"disable-next-line"),
 *      ruleId: (string|null),
 *      line: number,
 *      column: number
 * }} options.directives Directive comments found in the file, with one-based columns.
 * Two directive comments can only have the same location if they also have the same type (e.g. a single eslint-disable
 * comment for two different rules is represented as two directives).
 * @param {{ruleId: (string|null), line: number, column: number}[]} options.problems
 * A list of problems reported by rules, sorted by increasing location in the file, with one-based columns.
 * @param {"off" | "warn" | "error"} options.reportUnusedDisableDirectives If `"warn"` or `"error"`, adds additional problems for unused directives
 * @param {boolean} options.disableFixes If true, it doesn't make `fix` properties.
 * @returns {{ruleId: (string|null), line: number, column: number}[]}
 * A list of reported problems that were not disabled by the directive comments.
 */
module.exports = ({ directives, disableFixes, problems, reportUnusedDisableDirectives = "off" }) => {
    const blockDirectives = directives
        .filter(directive => directive.type === "disable" || directive.type === "enable")
        .map(directive => Object.assign({}, directive, { unprocessedDirective: directive }))
        .sort(compareLocations);

    const lineDirectives = directives.flatMap(directive => {
        switch (directive.type) {
            case "disable":
            case "enable":
                return [];

            case "disable-line":
                return [
                    { type: "disable", line: directive.line, column: 1, ruleId: directive.ruleId, unprocessedDirective: directive },
                    { type: "enable", line: directive.line + 1, column: 0, ruleId: directive.ruleId, unprocessedDirective: directive }
                ];

            case "disable-next-line":
                return [
                    { type: "disable", line: directive.line + 1, column: 1, ruleId: directive.ruleId, unprocessedDirective: directive },
                    { type: "enable", line: directive.line + 2, column: 0, ruleId: directive.ruleId, unprocessedDirective: directive }
                ];

            default:
                throw new TypeError(`Unrecognized directive type '${directive.type}'`);
        }
    }).sort(compareLocations);

    const blockDirectivesResult = applyDirectives({
        problems,
        directives: blockDirectives,
        disableFixes,
        reportUnusedDisableDirectives
    });
    const lineDirectivesResult = applyDirectives({
        problems: blockDirectivesResult.problems,
        directives: lineDirectives,
        disableFixes,
        reportUnusedDisableDirectives
    });

    return reportUnusedDisableDirectives !== "off"
        ? lineDirectivesResult.problems
            .concat(blockDirectivesResult.unusedDisableDirectives)
            .concat(lineDirectivesResult.unusedDisableDirectives)
            .sort(compareLocations)
        : lineDirectivesResult.problems;
};
