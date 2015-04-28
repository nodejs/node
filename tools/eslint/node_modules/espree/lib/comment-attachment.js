/**
 * @fileoverview Attaches comments to the AST.
 * @author Nicholas C. Zakas
 * @copyright 2015 Nicholas C. Zakas. All rights reserved.
 * @copyright 2011-2013 Ariya Hidayat <ariya.hidayat@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var astNodeTypes = require("./ast-node-types");

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------

var extra = {
        trailingComments: [],
        leadingComments: [],
        bottomRightStack: []
    };

//------------------------------------------------------------------------------
// Public
//------------------------------------------------------------------------------

module.exports = {

    reset: function() {
        extra.trailingComments = [];
        extra.leadingComments = [];
        extra.bottomRightStack = [];
    },

    addComment: function(comment) {
        extra.trailingComments.push(comment);
        extra.leadingComments.push(comment);
    },

    processComment: function(node) {
        var lastChild,
            trailingComments,
            i;

        if (node.type === astNodeTypes.Program) {
            if (node.body.length > 0) {
                return;
            }
        }

        if (extra.trailingComments.length > 0) {

            /*
             * If the first comment in trailingComments comes after the
             * current node, then we're good - all comments in the array will
             * come after the node and so it's safe to add then as official
             * trailingComments.
             */
            if (extra.trailingComments[0].range[0] >= node.range[1]) {
                trailingComments = extra.trailingComments;
                extra.trailingComments = [];
            } else {

                /*
                 * Otherwise, if the first comment doesn't come after the
                 * current node, that means we have a mix of leading and trailing
                 * comments in the array and that leadingComments contains the
                 * same items as trailingComments. Reset trailingComments to
                 * zero items and we'll handle this by evaluating leadingComments
                 * later.
                 */
                extra.trailingComments.length = 0;
            }
        } else {
            if (extra.bottomRightStack.length > 0 &&
                    extra.bottomRightStack[extra.bottomRightStack.length - 1].trailingComments &&
                    extra.bottomRightStack[extra.bottomRightStack.length - 1].trailingComments[0].range[0] >= node.range[1]) {
                trailingComments = extra.bottomRightStack[extra.bottomRightStack.length - 1].trailingComments;
                delete extra.bottomRightStack[extra.bottomRightStack.length - 1].trailingComments;
            }
        }

        // Eating the stack.
        while (extra.bottomRightStack.length > 0 && extra.bottomRightStack[extra.bottomRightStack.length - 1].range[0] >= node.range[0]) {
            lastChild = extra.bottomRightStack.pop();
        }

        if (lastChild) {
            if (lastChild.leadingComments && lastChild.leadingComments[lastChild.leadingComments.length - 1].range[1] <= node.range[0]) {
                node.leadingComments = lastChild.leadingComments;
                delete lastChild.leadingComments;
            }
        } else if (extra.leadingComments.length > 0) {

            if (extra.leadingComments[extra.leadingComments.length - 1].range[1] <= node.range[0]) {
                node.leadingComments = extra.leadingComments;
                extra.leadingComments = [];
            } else {

                // https://github.com/eslint/espree/issues/2

                /*
                 * In special cases, such as return (without a value) and
                 * debugger, all comments will end up as leadingComments and
                 * will otherwise be eliminated. This extra step runs when the
                 * bottomRightStack is empty and there are comments left
                 * in leadingComments.
                 *
                 * This loop figures out the stopping point between the actual
                 * leading and trailing comments by finding the location of the
                 * first comment that comes after the given node.
                 */
                for (i = 0; i < extra.leadingComments.length; i++) {
                    if (extra.leadingComments[i].range[1] > node.range[0]) {
                        break;
                    }
                }

                /*
                 * Split the array based on the location of the first comment
                 * that comes after the node. Keep in mind that this could
                 * result in an empty array, and if so, the array must be
                 * deleted.
                 */
                node.leadingComments = extra.leadingComments.slice(0, i);
                if (node.leadingComments.length === 0) {
                    delete node.leadingComments;
                }

                /*
                 * Similarly, trailing comments are attached later. The variable
                 * must be reset to null if there are no trailing comments.
                 */
                trailingComments = extra.leadingComments.slice(i);
                if (trailingComments.length === 0) {
                    trailingComments = null;
                }
            }
        }

        if (trailingComments) {
            node.trailingComments = trailingComments;
        }

        extra.bottomRightStack.push(node);
    }

};
