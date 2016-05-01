/**
 * @fileoverview The event generator for AST nodes.
 * @author Toru Nagashima
 */

"use strict";

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * The event generator for AST nodes.
 * This implements below interface.
 *
 * ```ts
 * interface EventGenerator {
 *     emitter: EventEmitter;
 *     enterNode(node: ASTNode): void;
 *     leaveNode(node: ASTNode): void;
 * }
 * ```
 *
 * @param {EventEmitter} emitter - An event emitter which is the destination of events.
 * @returns {NodeEventGenerator} new instance.
 */
function NodeEventGenerator(emitter) {
    this.emitter = emitter;
}

NodeEventGenerator.prototype = {
    constructor: NodeEventGenerator,

    /**
     * Emits an event of entering AST node.
     * @param {ASTNode} node - A node which was entered.
     * @returns {void}
     */
    enterNode: function enterNode(node) {
        this.emitter.emit(node.type, node);
    },

    /**
     * Emits an event of leaving AST node.
     * @param {ASTNode} node - A node which was left.
     * @returns {void}
     */
    leaveNode: function leaveNode(node) {
        this.emitter.emit(node.type + ":exit", node);
    }
};

module.exports = NodeEventGenerator;
