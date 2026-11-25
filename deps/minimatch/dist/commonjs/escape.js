"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.escape = void 0;
/**
 * Escape all magic characters in a glob pattern.
 *
 * If the {@link MinimatchOptions.windowsPathsNoEscape}
 * option is used, then characters are escaped by wrapping in `[]`, because
 * a magic character wrapped in a character class can only be satisfied by
 * that exact character.  In this mode, `\` is _not_ escaped, because it is
 * not interpreted as a magic character, but instead as a path separator.
 *
 * If the {@link MinimatchOptions.magicalBraces} option is used,
 * then braces (`{` and `}`) will be escaped.
 *
 * A leading `#` or `!` is escaped as well, so that the result is never a
 * comment or a negated pattern. In `windowsPathsNoEscape` mode, a leading
 * `!` is written as `@(!)`, because `[!` starts a negated class.
 */
const escape = (s, { windowsPathsNoEscape = false, magicalBraces = false, } = {}) => {
    // don't need to escape +@! because we escape the parens
    // that make those magic, and escaping ! as [!] isn't valid,
    // because [!]] is a valid glob class meaning not ']'.
    // However, at the start of the pattern, '!' negates the pattern and
    // '#' makes it a comment, so the first character is handled below.
    const e = magicalBraces ?
        windowsPathsNoEscape ? s.replace(/[?*()[\]{}]/g, '[$&]')
            : s.replace(/[?*()[\]\\{}]/g, '\\$&')
        : windowsPathsNoEscape ? s.replace(/[?*()[\]]/g, '[$&]')
            : s.replace(/[?*()[\]\\]/g, '\\$&');
    const c = e.charAt(0);
    if (c === '#') {
        return (windowsPathsNoEscape ? '[#]' : '\\#') + e.slice(1);
    }
    if (c === '!') {
        // '[!]' can not be used, because '[!' starts a negated class, and
        // '\' is a path separator in windowsPathsNoEscape mode. '@(!)'
        // matches only '!'. If extglobs are disabled with `noext`, it
        // matches nothing, rather than everything.
        return (windowsPathsNoEscape ? '@(!)' : '\\!') + e.slice(1);
    }
    return e;
};
exports.escape = escape;
//# sourceMappingURL=escape.js.map