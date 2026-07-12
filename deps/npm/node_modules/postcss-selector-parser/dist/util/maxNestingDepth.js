"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MAX_NESTING_DEPTH = void 0;
exports.default = resolveMaxNestingDepth;
/**
 * The default maximum selector nesting depth allowed when parsing or
 * serializing a selector. Going beyond this would otherwise recurse deeply
 * enough to overflow the call stack (CVE-2026-9358 / CWE-674). Real-world
 * selectors never get anywhere near this, so it acts purely as a safety net
 * that turns an uncatchable stack overflow into a catchable error.
 */
exports.MAX_NESTING_DEPTH = 256;
/**
 * Coerce a user-supplied nesting-depth limit into a safe value. Anything that
 * is not a non-negative safe integer (NaN, Infinity, negative numbers, or a
 * non-number) would disable or break the guard, so it falls back to the
 * default.
 *
 * @param {unknown} value the limit provided through the `maxNestingDepth` option
 * @returns {number} a safe, non-negative integer limit
 */
function resolveMaxNestingDepth(value) {
    return Number.isSafeInteger(value) && value >= 0 ? value : exports.MAX_NESTING_DEPTH;
}
//# sourceMappingURL=maxNestingDepth.js.map