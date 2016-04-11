/**
 * @fileoverview A rule to disallow duplicate name in class members.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var stack = [];

    /**
     * Gets state of a given member name.
     * @param {string} name - A name of a member.
     * @param {boolean} isStatic - A flag which specifies that is a static member.
     * @returns {object} A state of a given member name.
     *   - retv.init {boolean} A flag which shows the name is declared as normal member.
     *   - retv.get {boolean} A flag which shows the name is declared as getter.
     *   - retv.set {boolean} A flag which shows the name is declared as setter.
     */
    function getState(name, isStatic) {
        var stateMap = stack[stack.length - 1];
        var key = "$" + name; // to avoid "__proto__".

        if (!stateMap[key]) {
            stateMap[key] = {
                nonStatic: {init: false, get: false, set: false},
                static: {init: false, get: false, set: false}
            };
        }

        return stateMap[key][isStatic ? "static" : "nonStatic"];
    }

    /**
     * Gets the name text of a given node.
     *
     * @param {ASTNode} node - A node to get the name.
     * @returns {string} The name text of the node.
     */
    function getName(node) {
        switch (node.type) {
            case "Identifier": return node.name;
            case "Literal": return String(node.value);

            /* istanbul ignore next: syntax error */
            default: return "";
        }
    }

    return {

        // Initializes the stack of state of member declarations.
        "Program": function() {
            stack = [];
        },

        // Initializes state of member declarations for the class.
        "ClassBody": function() {
            stack.push(Object.create(null));
        },

        // Disposes the state for the class.
        "ClassBody:exit": function() {
            stack.pop();
        },

        // Reports the node if its name has been declared already.
        "MethodDefinition": function(node) {
            if (node.computed) {
                return;
            }

            var name = getName(node.key);
            var state = getState(name, node.static);
            var isDuplicate = false;

            if (node.kind === "get") {
                isDuplicate = (state.init || state.get);
                state.get = true;
            } else if (node.kind === "set") {
                isDuplicate = (state.init || state.set);
                state.set = true;
            } else {
                isDuplicate = (state.init || state.get || state.set);
                state.init = true;
            }

            if (isDuplicate) {
                context.report(node, "Duplicate name '{{name}}'.", {name: name});
            }
        }
    };
};

module.exports.schema = [];
