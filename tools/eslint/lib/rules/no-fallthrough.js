/**
 * @fileoverview Rule to flag fall-through cases in switch statements.
 * @author Matt DuVall <http://mattduvall.com/>
 */
"use strict";


var FALLTHROUGH_COMMENT = /falls\sthrough/;

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var switches = [];

    return {

        "SwitchCase": function(node) {

            var consequent = node.consequent,
                switchData = switches[switches.length - 1],
                i,
                comments,
                comment;

            /*
             * Some developers wrap case bodies in blocks, so if there is just one
             * node and it's a block statement, check inside.
             */
            if (consequent.length === 1 && consequent[0].type === "BlockStatement") {
                consequent = consequent[0];
            }

            // checking on previous case
            if (!switchData.lastCaseClosed) {

                // a fall through comment will be the last trailing comment of the last case
                comments = context.getComments(switchData.lastCase).trailing;
                comment = comments[comments.length - 1];

                // unless the user doesn't like semicolons, in which case it's first leading comment of this case
                if (!comment) {
                    comments = context.getComments(node).leading;
                    comment = comments[comments.length - 1];
                }

                // check for comment
                if (!comment || !FALLTHROUGH_COMMENT.test(comment.value)) {

                    context.report(switchData.lastCase,
                        "Expected a \"break\" statement before \"{{code}}\".",
                        { code: node.test ? "case" : "default" });
                }
            }

            // now dealing with the current case
            switchData.lastCaseClosed = false;
            switchData.lastCase = node;

            // try to verify using statements - go backwards as a fast path for the search
            if (consequent.length) {
                for (i = consequent.length - 1; i >= 0; i--) {
                    if (/(?:Break|Continue|Return|Throw)Statement/.test(consequent[i].type)) {
                        switchData.lastCaseClosed = true;
                        break;
                    }
                }
            } else {
                // the case statement has no statements, so it must logically fall through
                switchData.lastCaseClosed = true;
            }

            /*
             * Any warnings are triggered when the next SwitchCase occurs.
             * There is no need to warn on the last SwitchCase, since it can't
             * fall through to anything.
             */
        },

        "SwitchStatement": function(node) {
            switches.push({
                node: node,
                lastCaseClosed: true,
                lastCase: null
            });
        },

        "SwitchStatement:exit": function() {
            switches.pop();
        }
    };

};

module.exports.schema = [];
