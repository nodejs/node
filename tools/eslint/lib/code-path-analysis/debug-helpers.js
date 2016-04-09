/**
 * @fileoverview Helpers to debug for code path analysis.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 * See LICENSE file in root directory for full license.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var debug = require("debug")("eslint:code-path");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Gets id of a given segment.
 * @param {CodePathSegment} segment - A segment to get.
 * @returns {string} Id of the segment.
 */
/* istanbul ignore next */
function getId(segment) { // eslint-disable-line require-jsdoc
    return segment.id + (segment.reachable ? "" : "!");
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = {

    /**
     * A flag that debug dumping is enabled or not.
     * @type {boolean}
     */
    enabled: debug.enabled,

    /**
     * Dumps given objects.
     *
     * @param {...any} args - objects to dump.
     * @returns {void}
     */
    dump: debug,

    /**
     * Dumps the current analyzing state.
     *
     * @param {ASTNode} node - A node to dump.
     * @param {CodePathState} state - A state to dump.
     * @param {boolean} leaving - A flag whether or not it's leaving
     * @returns {void}
     */
    dumpState: !debug.enabled ? debug : /* istanbul ignore next */ function(node, state, leaving) {
        for (var i = 0; i < state.currentSegments.length; ++i) {
            var segInternal = state.currentSegments[i].internal;

            if (leaving) {
                segInternal.exitNodes.push(node);
            } else {
                segInternal.nodes.push(node);
            }
        }

        debug(
            state.currentSegments.map(getId).join(",") + ") " +
            node.type + (leaving ? ":exit" : "")
        );
    },

    /**
     * Dumps a DOT code of a given code path.
     * The DOT code can be visialized with Graphvis.
     *
     * @param {CodePath} codePath - A code path to dump.
     * @returns {void}
     * @see http://www.graphviz.org
     * @see http://www.webgraphviz.com
     */
    dumpDot: !debug.enabled ? debug : /* istanbul ignore next */ function(codePath) {
        var text =
            "\n" +
            "digraph {\n" +
            "node[shape=box,style=\"rounded,filled\",fillcolor=white];\n" +
            "initial[label=\"\",shape=circle,style=filled,fillcolor=black,width=0.25,height=0.25];\n";

        if (codePath.returnedSegments.length > 0) {
            text += "final[label=\"\",shape=doublecircle,style=filled,fillcolor=black,width=0.25,height=0.25];\n";
        }
        if (codePath.thrownSegments.length > 0) {
            text += "thrown[label=\"✘\",shape=circle,width=0.3,height=0.3,fixedsize];\n";
        }

        var traceMap = Object.create(null);
        var arrows = this.makeDotArrows(codePath, traceMap);

        for (var id in traceMap) { // eslint-disable-line guard-for-in
            var segment = traceMap[id];

            text += id + "[";

            if (segment.reachable) {
                text += "label=\"";
            } else {
                text += "style=\"rounded,dashed,filled\",fillcolor=\"#FF9800\",label=\"<<unreachable>>\\n";
            }

            if (segment.internal.nodes.length > 0) {
                text += segment.internal.nodes.map(function(node) {
                    switch (node.type) {
                        case "Identifier": return node.type + " (" + node.name + ")";
                        case "Literal": return node.type + " (" + node.value + ")";
                        default: return node.type;
                    }
                }).join("\\n");
            } else if (segment.internal.exitNodes.length > 0) {
                text += segment.internal.exitNodes.map(function(node) {
                    switch (node.type) {
                        case "Identifier": return node.type + ":exit (" + node.name + ")";
                        case "Literal": return node.type + ":exit (" + node.value + ")";
                        default: return node.type + ":exit";
                    }
                }).join("\\n");
            } else {
                text += "????";
            }

            text += "\"];\n";
        }

        text += arrows + "\n";
        text += "}";
        debug("DOT", text);
    },

    /**
     * Makes a DOT code of a given code path.
     * The DOT code can be visialized with Graphvis.
     *
     * @param {CodePath} codePath - A code path to make DOT.
     * @param {object} traceMap - Optional. A map to check whether or not segments had been done.
     * @returns {string} A DOT code of the code path.
     */
    makeDotArrows: function(codePath, traceMap) {
        var stack = [[codePath.initialSegment, 0]];
        var done = traceMap || Object.create(null);
        var lastId = codePath.initialSegment.id;
        var text = "initial->" + codePath.initialSegment.id;

        while (stack.length > 0) {
            var item = stack.pop();
            var segment = item[0];
            var index = item[1];

            if (done[segment.id] && index === 0) {
                continue;
            }
            done[segment.id] = segment;

            var nextSegment = segment.allNextSegments[index];

            if (!nextSegment) {
                continue;
            }

            if (lastId === segment.id) {
                text += "->" + nextSegment.id;
            } else {
                text += ";\n" + segment.id + "->" + nextSegment.id;
            }
            lastId = nextSegment.id;

            stack.unshift([segment, 1 + index]);
            stack.push([nextSegment, 0]);
        }

        codePath.returnedSegments.forEach(function(finalSegment) {
            if (lastId === finalSegment.id) {
                text += "->final";
            } else {
                text += ";\n" + finalSegment.id + "->final";
            }
            lastId = null;
        });

        codePath.thrownSegments.forEach(function(finalSegment) {
            if (lastId === finalSegment.id) {
                text += "->thrown";
            } else {
                text += ";\n" + finalSegment.id + "->thrown";
            }
            lastId = null;
        });

        return text + ";";
    }
};
