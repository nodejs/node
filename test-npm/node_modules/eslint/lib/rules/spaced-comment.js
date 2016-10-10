/**
 * @fileoverview Source code for spaced-comments rule
 * @author Gyandeep Singh
 * @copyright 2015 Toru Nagashima. All rights reserved.
 * @copyright 2015 Gyandeep Singh. All rights reserved.
 * @copyright 2014 Greg Cochard. All rights reserved.
 */
"use strict";

var lodash = require("lodash");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Escapes the control characters of a given string.
 * @param {string} s - A string to escape.
 * @returns {string} An escaped string.
 */
function escape(s) {
    var isOneChar = s.length === 1;
    s = lodash.escapeRegExp(s);
    return isOneChar ? s : "(?:" + s + ")";
}

/**
 * Escapes the control characters of a given string.
 * And adds a repeat flag.
 * @param {string} s - A string to escape.
 * @returns {string} An escaped string.
 */
function escapeAndRepeat(s) {
    return escape(s) + "+";
}

/**
 * Parses `markers` option.
 * If markers don't include `"*"`, this adds `"*"` to allow JSDoc comments.
 * @param {string[]} [markers] - A marker list.
 * @returns {string[]} A marker list.
 */
function parseMarkersOption(markers) {
    markers = markers ? markers.slice(0) : [];

    // `*` is a marker for JSDoc comments.
    if (markers.indexOf("*") === -1) {
        markers.push("*");
    }

    return markers;
}

/**
 * Creates RegExp object for `always` mode.
 * Generated pattern is below:
 *
 * 1. First, a marker or nothing.
 * 2. Next, a space or an exception pattern sequence.
 *
 * @param {string[]} markers - A marker list.
 * @param {string[]} exceptions - A exception pattern list.
 * @returns {RegExp} A RegExp object for `always` mode.
 */
function createAlwaysStylePattern(markers, exceptions) {
    var pattern = "^";

    // A marker or nothing.
    //   ["*"]            ==> "\*?"
    //   ["*", "!"]       ==> "(?:\*|!)?"
    //   ["*", "/", "!<"] ==> "(?:\*|\/|(?:!<))?" ==> https://jex.im/regulex/#!embed=false&flags=&re=(%3F%3A%5C*%7C%5C%2F%7C(%3F%3A!%3C))%3F
    if (markers.length === 1) {
        // the marker.
        pattern += escape(markers[0]);
    } else {
        // one of markers.
        pattern += "(?:";
        pattern += markers.map(escape).join("|");
        pattern += ")";
    }
    pattern += "?"; // or nothing.

    // A space or an exception pattern sequence.
    //   []                 ==> "\s"
    //   ["-"]              ==> "(?:\s|\-+$)"
    //   ["-", "="]         ==> "(?:\s|(?:\-+|=+)$)"
    //   ["-", "=", "--=="] ==> "(?:\s|(?:\-+|=+|(?:\-\-==)+)$)" ==> https://jex.im/regulex/#!embed=false&flags=&re=(%3F%3A%5Cs%7C(%3F%3A%5C-%2B%7C%3D%2B%7C(%3F%3A%5C-%5C-%3D%3D)%2B)%24)
    if (exceptions.length === 0) {
        // a space.
        pattern += "\\s";
    } else {
        // a space or...
        pattern += "(?:\\s|";
        if (exceptions.length === 1) {
            // a sequence of the exception pattern.
            pattern += escapeAndRepeat(exceptions[0]);
        } else {
            // a sequence of one of exception patterns.
            pattern += "(?:";
            pattern += exceptions.map(escapeAndRepeat).join("|");
            pattern += ")";
        }
        pattern += "(?:$|[\n\r]))"; // the sequence continues until the end.
    }
    return new RegExp(pattern);
}

/**
 * Creates RegExp object for `never` mode.
 * Generated pattern is below:
 *
 * 1. First, a marker or nothing (captured).
 * 2. Next, a space or a tab.
 *
 * @param {string[]} markers - A marker list.
 * @returns {RegExp} A RegExp object for `never` mode.
 */
function createNeverStylePattern(markers) {
    var pattern = "^(" + markers.map(escape).join("|") + ")?[ \t]+";
    return new RegExp(pattern);
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    // Unless the first option is never, require a space
    var requireSpace = context.options[0] !== "never";

    // Parse the second options.
    // If markers don't include `"*"`, it's added automatically for JSDoc comments.
    var config = context.options[1] || {};
    var styleRules = ["block", "line"].reduce(function(rule, type) {
        var markers = parseMarkersOption(config[type] && config[type].markers || config.markers);
        var exceptions = config[type] && config[type].exceptions || config.exceptions || [];

        // Create RegExp object for valid patterns.
        rule[type] = {
            regex: requireSpace ? createAlwaysStylePattern(markers, exceptions) : createNeverStylePattern(markers),
            hasExceptions: exceptions.length > 0,
            markers: new RegExp("^(" + markers.map(escape).join("|") + ")")
        };

        return rule;
    }, {});

    /**
     * Reports a spacing error with an appropriate message.
     * @param {ASTNode} node - A comment node to check.
     * @param {string} message - An error message to report
     * @param {Array} match - An array of match results for markers.
     * @returns {void}
     */
    function report(node, message, match) {
        var type = node.type.toLowerCase(),
            commentIdentifier = type === "block" ? "/*" : "//";

        context.report({
            node: node,
            fix: function(fixer) {
                var start = node.range[0],
                    end = start + 2;

                if (requireSpace) {
                    if (match) {
                        end += match[0].length;
                    }
                    return fixer.insertTextAfterRange([start, end], " ");
                } else {
                    end += match[0].length;
                    return fixer.replaceTextRange([start, end], commentIdentifier + (match[1] ? match[1] : ""));
                }
            },
            message: message
        });
    }

    /**
     * Reports a given comment if it's invalid.
     * @param {ASTNode} node - a comment node to check.
     * @returns {void}
     */
    function checkCommentForSpace(node) {
        var type = node.type.toLowerCase(),
            rule = styleRules[type],
            commentIdentifier = type === "block" ? "/*" : "//";

        // Ignores empty comments.
        if (node.value.length === 0) {
            return;
        }

        // Checks.
        if (requireSpace) {
            if (!rule.regex.test(node.value)) {
                var hasMarker = rule.markers.exec(node.value);
                var marker = hasMarker ? commentIdentifier + hasMarker[0] : commentIdentifier;
                if (rule.hasExceptions) {
                    report(node, "Expected exception block, space or tab after '" + marker + "' in comment.", hasMarker);
                } else {
                    report(node, "Expected space or tab after '" + marker + "' in comment.", hasMarker);
                }
            }
        } else {
            var matched = rule.regex.exec(node.value);
            if (matched) {
                if (!matched[1]) {
                    report(node, "Unexpected space or tab after '" + commentIdentifier + "' in comment.", matched);
                } else {
                    report(node, "Unexpected space or tab after marker (" + matched[1] + ") in comment.", matched);
                }
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
            },
            "line": {
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
            },
            "block": {
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
        },
        "additionalProperties": false
    }
];
