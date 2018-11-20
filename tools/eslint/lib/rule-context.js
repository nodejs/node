/**
 * @fileoverview RuleContext utility for rules
 * @author Nicholas C. Zakas
 */
"use strict";

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

var PASSTHROUGHS = [
        "getAllComments",
        "getAncestors",
        "getComments",
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
     * @param {ASTNode} node The AST node related to the message.
     * @param {Object=} location The location of the error.
     * @param {string} message The message to display to the user.
     * @param {Object} opts Optional template data which produces a formatted message
     *     with symbols being replaced by this object's values.
     * @returns {void}
     */
    this.report = function(node, location, message, opts) {
        eslint.report(ruleId, severity, node, location, message, opts);
    };

}

RuleContext.prototype = {
    constructor: RuleContext
};

module.exports = RuleContext;
