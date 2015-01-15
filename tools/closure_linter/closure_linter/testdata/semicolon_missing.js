/**
 * @fileoverview This is for regression testing of scenario where semicolon is
 *     missing at EOF. b/10801776.
 */

goog.provide('dummy.foo.DATA');

/**
 * @type {string}
 * @const
 *
 * For repeating the bug blank comment line above this is needed.
 */

// +3: MISSING_SEMICOLON
dummy.foo.DATA =
    'SFSDFSDdfgdfgdftreterterterterterggsdfsrrwerwerwsfwerwerwere55454ss' +
    'SFSDFSDdfgdfgdftretertertertertergg'
