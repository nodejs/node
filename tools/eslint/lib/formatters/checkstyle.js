/**
 * @fileoverview CheckStyle XML reporter
 * @author Ian Christian Myers
 */
"use strict";

//------------------------------------------------------------------------------
// Helper Functions
//------------------------------------------------------------------------------

/**
 * Returns the severity of warning or error
 * @param {object} message message object to examine
 * @returns {string} severity level
 * @private
 */
function getMessageType(message) {
    if (message.fatal || message.severity === 2) {
        return "error";
    } else {
        return "warning";
    }
}

/**
 * Returns the escaped value for a character
 * @param {string} s string to examine
 * @returns {string} severity level
 * @private
 */
function xmlEscape(s) {
    return ("" + s).replace(/[<>&"']/g, function(c) {
        switch (c) {
            case "<":
                return "&lt;";
            case ">":
                return "&gt;";
            case "&":
                return "&amp;";
            case "\"":
                return "&quot;";
            case "'":
                return "&apos;";
            default:
                throw new Error("unreachable");
        }
    });
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = function(results) {

    var output = "";

    output += "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
    output += "<checkstyle version=\"4.3\">";

    results.forEach(function(result) {
        var messages = result.messages;

        output += "<file name=\"" + xmlEscape(result.filePath) + "\">";

        messages.forEach(function(message) {
            output += "<error line=\"" + xmlEscape(message.line) + "\" " +
                "column=\"" + xmlEscape(message.column) + "\" " +
                "severity=\"" + xmlEscape(getMessageType(message)) + "\" " +
                "message=\"" + xmlEscape(message.message) +
                (message.ruleId ? " (" + message.ruleId + ")" : "") + "\" " +
                "source=\"" + (message.ruleId ? xmlEscape("eslint.rules." + message.ruleId) : "") + "\" />";
        });

        output += "</file>";

    });

    output += "</checkstyle>";

    return output;
};
