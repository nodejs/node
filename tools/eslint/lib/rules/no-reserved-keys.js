/**
 * @fileoverview Rule to disallow reserved words being used as keys
 * @author Emil Bay
 * @copyright 2014 Emil Bay. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var MESSAGE = "Reserved word '{{key}}' used as key.";

    var reservedWords = [
        "abstract",
        "boolean", "break", "byte",
        "case", "catch", "char", "class", "const", "continue",
        "debugger", "default", "delete", "do", "double",
        "else", "enum", "export", "extends",
        "final", "finally", "float", "for", "function",
        "goto",
        "if", "implements", "import", "in", "instanceof", "int", "interface",
        "long",
        "native", "new",
        "package", "private", "protected", "public",
        "return",
        "short", "static", "super", "switch", "synchronized",
        "this", "throw", "throws", "transient", "try", "typeof",
        "var", "void", "volatile",
        "while", "with"
    ];

    return {

        "ObjectExpression": function(node) {
            node.properties.forEach(function(property) {

                if (property.key.type === "Identifier") {
                    var keyName = property.key.name;

                    if (reservedWords.indexOf("" + keyName) !== -1) {
                        context.report(node, MESSAGE, { key: keyName });
                    }
                }

            });

        }
    };

};
