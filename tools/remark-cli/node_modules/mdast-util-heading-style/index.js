/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module mdast:util:heading-style
 * @fileoverview Utility to get the style of an mdast heading.
 */

'use strict';

/* eslint-env node */

/**
 * Get the probable style of an atx-heading, depending on
 * preferred style.
 *
 * @example
 *   consolidate(1, 'setext') // 'atx'
 *   consolidate(1, 'atx') // 'atx'
 *   consolidate(3, 'setext') // 'setext'
 *   consolidate(3, 'atx') // 'atx'
 *
 * @private
 * @param {number} depth - Depth of heading.
 * @param {string?} relative - Preferred style.
 * @return {string?} - Type.
 */
function consolidate(depth, relative) {
    return depth < 3 ? 'atx' :
        relative === 'atx' || relative === 'setext' ? relative : null;
}

/**
 * Check the style of a heading.
 *
 * @example
 *   style(); // null
 *
 *   style(remark().parse('# foo').children[0]); // 'atx'
 *
 *   style(remark().parse('# foo #').children[0]); // 'atx-closed'
 *
 *   style(remark().parse('foo\n===').children[0]); // 'setext'
 *
 * @param {Node} node - Node to check.
 * @param {string?} relative - Heading type which we'd wish
 *   this to be.
 * @return {string?} - Type, either `'atx-closed'`,
 *   `'atx'`, or `'setext'`.
 */
function style(node, relative) {
    var last = node.children[node.children.length - 1];
    var depth = node.depth;
    var pos = node && node.position && node.position.end;
    var final = last && last.position && last.position.end;

    if (!pos) {
        return null;
    }

    /*
     * This can only occur for atx and `'atx-closed'`
     * headings.  This might incorrectly match `'atx'`
     * headings with lots of trailing white space as an
     * `'atx-closed'` heading.
     */

    if (!last) {
        if (pos.column - 1 <= depth * 2) {
            return consolidate(depth, relative);
        }

        return 'atx-closed';
    }

    if (final.line + 1 === pos.line) {
        return 'setext';
    }

    if (final.column + depth < pos.column) {
        return 'atx-closed';
    }

    return consolidate(depth, relative);
}

/*
 * Expose.
 */

module.exports = style;
