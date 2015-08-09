/**
 * @fileoverview Source code for spaced-comments rule
 * @author Gyandeep Singh
 * @copyright 2015 Gyandeep Singh. All rights reserved.
 * @copyright 2014 Greg Cochard. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    // Unless the first option is never, require a space
    var requireSpace = context.options[0] !== "never";

    // Default to match anything, so all will fail if there are no exceptions
    var exceptionMatcher = new RegExp(" ");
    var markerMatcher = new RegExp(" ");
    var jsDocMatcher = new RegExp("((^(\\*)))[ \\n]");

    // Fetch the options dict
    var hasOptions = context.options.length === 2;
    var optionsDict = hasOptions ? context.options[1] : {};

    // Grab the exceptions array and build a RegExp matcher for it
    var hasExceptions = hasOptions && optionsDict.exceptions && optionsDict.exceptions.length;
    var unescapedExceptions = hasExceptions ? optionsDict.exceptions : [];
    var exceptions;

    // Now do the same for markers
    var hasMarkers = hasOptions && optionsDict.markers && optionsDict.markers.length;
    var unescapedMarkers = hasMarkers ? optionsDict.markers : [];
    var markers;

    function escaper(s) {
        return s.replace(/([.*+?${}()|\^\[\]\/\\])/g, "\\$1");
    }

    if (hasExceptions) {
        exceptions = unescapedExceptions.map(escaper);
        exceptionMatcher = new RegExp("(^(" + exceptions.join(")+$)|(^(") + ")+$)");
    }

    if (hasMarkers) {
        markers = unescapedMarkers.map(escaper);

        // the markerMatcher includes any markers in the list, followed by space/tab
        markerMatcher = new RegExp("((^(" + markers.join("))|(^(") + ")))[ \\t\\n]");
    }

    /**
     * Check to see if the block comment is jsDoc comment
     * @param {ASTNode} node comment node
     * @returns {boolean} True if its jsdoc comment
     * @private
     */
    function isJsdoc(node) {
        // make sure comment type is block and it start with /**\n
        return node.type === "Block" && jsDocMatcher.test(node.value);
    }


    function checkCommentForSpace(node) {
        var commentIdentifier = node.type === "Block" ? "/*" : "//";

        if (requireSpace) {

            // If length is zero, ignore it
            if (node.value.length === 0) {
                return;
            }

            // if comment is jsdoc style then ignore it
            if (isJsdoc(node)) {
                return;
            }

            // Check for markers now, and short-circuit if found
            if (hasMarkers && markerMatcher.test(node.value)) {
                return;
            }

            // Space expected and not found
            if (node.value.indexOf(" ") !== 0 && node.value.indexOf("\t") !== 0 && node.value.indexOf("\n") !== 0) {

                /*
                 * Do two tests; one for space starting the line,
                 * and one for a comment comprised only of exceptions
                 */
                if (hasExceptions && !exceptionMatcher.test(node.value)) {
                    context.report(node, "Expected exception block, space or tab after " + commentIdentifier + " in comment.");
                } else if (!hasExceptions) {
                    context.report(node, "Expected space or tab after " + commentIdentifier + " in comment.");
                }
            }

        } else {

            if (node.value.indexOf(" ") === 0 || node.value.indexOf("\t") === 0) {
                context.report(node, "Unexpected space or tab after " + commentIdentifier + " in comment.");
            }
            // there won't be a space or tab after commentIdentifier here, but check for the markers and whitespace
            if (hasMarkers && markerMatcher.test(node.value)) {
                var matches = node.value.match(markerMatcher), match = matches.length ? matches[0] : "";

                context.report(node, "Unexpected space or tab after marker (" + match + ") in comment.");
            }
        }
    }

    return {

        "LineComment": checkCommentForSpace,
        "BlockComment": checkCommentForSpace

    };
};

module.exports.schema = [
    {
        "enum": ["always", "never"]
    },
    {
        "type": "object",
        "properties": {
            "exceptions": {
                "type": "array",
                "items": {
                    "type": "string"
                }
            },
            "markers": {
                "type": "array",
                "items": {
                    "type": "string"
                }
            }
        },
        "additionalProperties": false
    }
];
