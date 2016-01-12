/**
 * @fileoverview HTML reporter
 * @author Julian Laval
 * @copyright 2015 Julian Laval. All rights reserved.
 */
"use strict";

var handlebars = require("handlebars").create();
var fs = require("fs");
var path = require("path");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Given a word and a count, append an s if count is not one.
 * @param {string} word A word in its singular form.
 * @param {int} count A number controlling whether word should be pluralized.
 * @returns {string} The original word with an s on the end if count is not one.
 */
function pluralize(word, count) {
    return (count === 1 ? word : word + "s");
}

/**
 * Renders text along the template of x problems (x errors, x warnings)
 * @param {string} totalErrors Total errors
 * @param {string} totalWarnings Total warnings
 * @returns {string} The formatted string, pluralized where necessary
 */
handlebars.registerHelper("renderText", function(totalErrors, totalWarnings) {
    var totalProblems = totalErrors + totalWarnings;
    var renderedText = totalProblems + " " + pluralize("problem", totalProblems);
    if (totalProblems !== 0) {
        renderedText += " (" + totalErrors + " " + pluralize("error", totalErrors) + ", " + totalWarnings + " " + pluralize("warning", totalWarnings) + ")";
    }
    return renderedText;
});

/**
 * Get the color based on whether there are errors/warnings...
 * @param {string} totalErrors Total errors
 * @param {string} totalWarnings Total warnings
 * @returns {int} The color code (0 = green, 1 = yellow, 2 = red)
 */
handlebars.registerHelper("getColor", function(totalErrors, totalWarnings) {
    if (totalErrors !== 0) {
        return 2;
    } else if (totalWarnings !== 0) {
        return 1;
    }
    return 0;
});

/**
 * Get the HTML row content based on the severity of the message
 * @param {int} severity Severity of the message
 * @returns {string} The generated HTML row
 */
handlebars.registerHelper("getSeverity", function(severity) {
    // Return warning else error
    return new handlebars.SafeString((severity === 1) ? "<td class=\"clr-1\">Warning</td>" : "<td class=\"clr-2\">Error</td>");
});

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = function(results) {

    var template = fs.readFileSync(path.join(__dirname, "html-template.html"), "utf-8");

    var data = {
        date: new Date(),
        totalErrors: 0,
        totalWarnings: 0,
        results: results
    };

    // Iterate over results to get totals
    results.forEach(function(result) {
        data.totalErrors += result.errorCount;
        data.totalWarnings += result.warningCount;
    });

    return handlebars.compile(template)(data);
};
