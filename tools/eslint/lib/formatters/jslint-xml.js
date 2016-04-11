/**
 * @fileoverview JSLint XML reporter
 * @author Ian Christian Myers
 */
"use strict";

var lodash = require("lodash");

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = function(results) {

    var output = "";

    output += "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
    output += "<jslint>";

    results.forEach(function(result) {
        var messages = result.messages;

        output += "<file name=\"" + result.filePath + "\">";

        messages.forEach(function(message) {
            output += "<issue line=\"" + message.line + "\" " +
                "char=\"" + message.column + "\" " +
                "evidence=\"" + lodash.escape(message.source || "") + "\" " +
                "reason=\"" + lodash.escape(message.message || "") +
                (message.ruleId ? " (" + message.ruleId + ")" : "") + "\" />";
        });

        output += "</file>";

    });

    output += "</jslint>";

    return output;
};
