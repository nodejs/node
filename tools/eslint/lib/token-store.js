/**
 * @fileoverview Object to handle access and retrieval of tokens.
 * @author Brandon Mills
 */
"use strict";

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------

module.exports = function(tokens) {
    var api = {},
        starts = Object.create(null),
        ends = Object.create(null),
        index, length, range;

    /**
     * Gets tokens in a given interval.
     * @param {int} start Inclusive index of the first token. 0 if negative.
     * @param {int} end Exclusive index of the last token.
     * @returns {Token[]} Tokens in the interval.
     */
    function get(start, end) {
        var result = [],
            i;

        for (i = Math.max(0, start); i < end && i < length; i++) {
            result.push(tokens[i]);
        }

        return result;
    }

    /**
     * Gets the index in the tokens array of the last token belonging to a node.
     * Usually a node ends exactly at a token, but due to ASI, sometimes a
     * node's range extends beyond its last token.
     * @param {ASTNode} node The node for which to find the last token's index.
     * @returns {int} Index in the tokens array of the node's last token.
     */
    function lastTokenIndex(node) {
        var end = node.range[1],
            cursor = ends[end];

        // If the node extends beyond its last token, get the token before the
        // next token
        if (typeof cursor === "undefined") {
            cursor = starts[end] - 1;
        }

        // If there isn't a next token, the desired token is the last one in the
        // array
        if (isNaN(cursor)) {
            cursor = length - 1;
        }

        return cursor;
    }

    // Map tokens' start and end range to the index in the tokens array
    for (index = 0, length = tokens.length; index < length; index++) {
        range = tokens[index].range;
        starts[range[0]] = index;
        ends[range[1]] = index;
    }

    /**
     * Gets a number of tokens that precede a given node or token in the token
     * stream.
     * @param {(ASTNode|Token)} node The AST node or token.
     * @param {int} [beforeCount=0] The number of tokens before the node or
     *     token to retrieve.
     * @returns {Token[]} Array of objects representing tokens.
     */
    api.getTokensBefore = function(node, beforeCount) {
        var first = starts[node.range[0]];

        return get(first - (beforeCount || 0), first);
    };

    /**
     * Gets the token that precedes a given node or token in the token stream.
     * @param {(ASTNode|Token)} node The AST node or token.
     * @param {int} [skip=0] A number of tokens to skip before the given node or
     *     token.
     * @returns {Token} An object representing the token.
     */
    api.getTokenBefore = function(node, skip) {
        return tokens[starts[node.range[0]] - (skip || 0) - 1];
    };

    /**
     * Gets a number of tokens that follow a given node or token in the token
     * stream.
     * @param {(ASTNode|Token)} node The AST node or token.
     * @param {int} [afterCount=0] The number of tokens after the node or token
     *     to retrieve.
     * @returns {Token[]} Array of objects representing tokens.
     */
    api.getTokensAfter = function(node, afterCount) {
        var start = lastTokenIndex(node) + 1;

        return get(start, start + (afterCount || 0));
    };

    /**
     * Gets the token that follows a given node or token in the token stream.
     * @param {(ASTNode|Token)} node The AST node or token.
     * @param {int} [skip=0] A number of tokens to skip after the given node or
     *     token.
     * @returns {Token} An object representing the token.
     */
    api.getTokenAfter = function(node, skip) {
        return tokens[lastTokenIndex(node) + (skip || 0) + 1];
    };

    /**
     * Gets all tokens that are related to the given node.
     * @param {ASTNode} node The AST node.
     * @param {int} [beforeCount=0] The number of tokens before the node to retrieve.
     * @param {int} [afterCount=0] The number of tokens after the node to retrieve.
     * @returns {Token[]} Array of objects representing tokens.
     */
    api.getTokens = function(node, beforeCount, afterCount) {
        return get(
            starts[node.range[0]] - (beforeCount || 0),
            lastTokenIndex(node) + (afterCount || 0) + 1
        );
    };

    /**
     * Gets the first `count` tokens of the given node's token stream.
     * @param {ASTNode} node The AST node.
     * @param {int} [count=0] The number of tokens of the node to retrieve.
     * @returns {Token[]} Array of objects representing tokens.
     */
    api.getFirstTokens = function(node, count) {
        var first = starts[node.range[0]];

        return get(
            first,
            Math.min(lastTokenIndex(node) + 1, first + (count || 0))
        );
    };

    /**
     * Gets the first token of the given node's token stream.
     * @param {ASTNode} node The AST node.
     * @param {int} [skip=0] A number of tokens to skip.
     * @returns {Token} An object representing the token.
     */
    api.getFirstToken = function(node, skip) {
        return tokens[starts[node.range[0]] + (skip || 0)];
    };

    /**
     * Gets the last `count` tokens of the given node.
     * @param {ASTNode} node The AST node.
     * @param {int} [count=0] The number of tokens of the node to retrieve.
     * @returns {Token[]} Array of objects representing tokens.
     */
    api.getLastTokens = function(node, count) {
        var last = lastTokenIndex(node) + 1;

        return get(Math.max(starts[node.range[0]], last - (count || 0)), last);
    };

    /**
     * Gets the last token of the given node's token stream.
     * @param {ASTNode} node The AST node.
     * @param {int} [skip=0] A number of tokens to skip.
     * @returns {Token} An object representing the token.
     */
    api.getLastToken = function(node, skip) {
        return tokens[lastTokenIndex(node) - (skip || 0)];
    };

    /**
     * Gets all of the tokens between two non-overlapping nodes.
     * @param {ASTNode} left Node before the desired token range.
     * @param {ASTNode} right Node after the desired token range.
     * @param {int} [padding=0] Number of extra tokens on either side of center.
     * @returns {Token[]} Tokens between left and right plus padding.
     */
    api.getTokensBetween = function(left, right, padding) {
        padding = padding || 0;
        return get(
            lastTokenIndex(left) + 1 - padding,
            starts[right.range[0]] + padding
        );
    };

    /**
     * Gets the token starting at the specified index.
     * @param {int} startIndex Index of the start of the token's range.
     * @returns {Token} The token starting at index, or null if no such token.
     */
    api.getTokenByRangeStart = function(startIndex) {
        return (tokens[starts[startIndex]] || null);
    };

    return api;
};
