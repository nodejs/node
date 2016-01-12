/**
 * @fileoverview RuleContext utility for rules
 * @author Nicholas C. Zakas
 * @copyright 2013 Nicholas C. Zakas. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var RuleFixer = require("./util/rule-fixer");

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

var PASSTHROUGHS = [
    "getAllComments",
    "getAncestors",
    "getComments",
    "getDeclaredVariables",
    "getFilename",
    "getFirstToken",
    "getFirstTokens",
    "getJSDocComment",
    "getLastToken",
    "getLastTokens",
    "getNodeByRangeIndex",
    "getScope",
    "getSource",
    "getSourceLines",
    "getTokenAfter",
    "getTokenBefore",
    "getTokenByRangeStart",
    "getTokens",
    "getTokensAfter",
    "getTokensBefore",
    "getTokensBetween",
    "markVariableAsUsed",
    "isMarkedAsUsed"
];

//------------------------------------------------------------------------------
// Typedefs
//------------------------------------------------------------------------------

/**
 * An error message description
 * @typedef {Object} MessageDescriptor
 * @property {string} nodeType The type of node.
 * @property {Location} loc The location of the problem.
 * @property {string} message The problem message.
 * @property {Object} [data] Optional data to use to fill in placeholders in the
 *      message.
 * @property {Function} fix The function to call that creates a fix command.
 */

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

/**
 * Acts as an abstraction layer between rules and the main eslint object.
 * @constructor
 * @param {string} ruleId The ID of the rule using this object.
 * @param {eslint} eslint The eslint object.
 * @param {number} severity The configured severity level of the rule.
 * @param {array} options The configuration information to be added to the rule.
 * @param {object} settings The configuration settings passed from the config file.
 * @param {object} ecmaFeatures The ecmaFeatures settings passed from the config file.
 */
function RuleContext(ruleId, eslint, severity, options, settings, ecmaFeatures) {

    /**
     * The read-only ID of the rule.
     */
    Object.defineProperty(this, "id", {
        value: ruleId
    });

    /**
     * The read-only options of the rule
     */
    Object.defineProperty(this, "options", {
        value: options
    });

    /**
     * The read-only settings shared between all rules
     */
    Object.defineProperty(this, "settings", {
        value: settings
    });

    /**
     * The read-only ecmaFeatures shared across all rules
     */
    Object.defineProperty(this, "ecmaFeatures", {
        value: Object.create(ecmaFeatures)
    });
    Object.freeze(this.ecmaFeatures);

    // copy over passthrough methods
    PASSTHROUGHS.forEach(function(name) {
        this[name] = function() {
            return eslint[name].apply(eslint, arguments);
        };
    }, this);

    /**
     * Passthrough to eslint.report() that automatically assigns the rule ID and severity.
     * @param {ASTNode|MessageDescriptor} nodeOrDescriptor The AST node related to the message or a message
     *      descriptor.
     * @param {Object=} location The location of the error.
     * @param {string} message The message to display to the user.
     * @param {Object} opts Optional template data which produces a formatted message
     *     with symbols being replaced by this object's values.
     * @returns {void}
     */
    this.report = function(nodeOrDescriptor, location, message, opts) {

        var descriptor,
            fix = null;

        // check to see if it's a new style call
        if (arguments.length === 1) {
            descriptor = nodeOrDescriptor;

            // if there's a fix specified, get it
            if (typeof descriptor.fix === "function") {
                fix = descriptor.fix(new RuleFixer());
            }

            eslint.report(
                ruleId, severity, descriptor.node,
                descriptor.loc || descriptor.node.loc.start,
                descriptor.message, descriptor.data, fix
            );

            return;
        }

        // old style call
        eslint.report(ruleId, severity, nodeOrDescriptor, location, message, opts);
    };

    /**
     * Passthrough to eslint.getSourceCode().
     * @returns {SourceCode} The SourceCode object for the code.
     */
    this.getSourceCode = function() {
        return eslint.getSourceCode();
    };

}

RuleContext.prototype = {
    constructor: RuleContext
};

module.exports = RuleContext;
