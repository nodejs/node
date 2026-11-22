import { expand } from 'brace-expansion';
import { Matcher } from '@isaacs/nonrecursive-glob-match';
import { assertValidPattern } from './assert-valid-pattern.js';
import { escape } from './escape.js';
import { unescape } from './unescape.js';
const DEFAULT_BRACE_EXPAND_MAX = 100_000;
const DEFAULT_GLOB_STAR_RECURSION_MAX = 200;
// avoid the annoying deprecation flag lol
const AWE_KEY = ('allowWindow' + 'sEscape');
export const minimatch = (p, pattern, options) => {
    assertValidPattern(pattern);
    // shortcut: comments match nothing.
    if (!options?.nocomment && pattern.charAt(0) === '#') {
        return false;
    }
    return cachedMinimatch(pattern, options).match(p);
};
// The Minimatch objects that minimatch() used most recently, by options
// and then by pattern. When the cache is full, it becomes the old one,
// and the objects used from the old one are moved back to a new one.
// So it keeps 256 to 512 objects.
// The patterns can come from users, so the cache is also bounded by
// the total weight of the objects (see cacheWeight()), and an object
// that is too heavy is never kept. Otherwise a few unique patterns
// with large brace expansions would fill the heap.
let cache = new Map();
let oldCache = new Map();
let cacheSize = 0;
let cacheTotalWeight = 0;
const cacheMax = 256;
const cacheMaxWeight = 1 << 15;
const cacheMaxEntryWeight = 1 << 12;
// About the memory that a compiled object keeps: the pattern and the
// expanded patterns, of which each row in set is made.
const cacheWeight = (mm) => {
    let w = mm.pattern.length;
    for (const s of mm.globSet) {
        w += s.length + 1;
        if (w > cacheMaxEntryWeight)
            break;
    }
    return w;
};
// tsc fails here if a new option is in neither type. Add it to
// optionsKey() too.
const allOptionsKeyed = true;
void allOptionsKeyed;
// 0, 1 or 2 for undefined, falsy or truthy.
const tri = (v) => v === undefined ? 0
    : v ? 2
        : 1;
// The weight of each boolean option's base-3 digit in the key. The
// weights below 24 are for optimizationLevel (1), platform (3) and
// windowsPathsNoEscape (12). tsc fails here if a boolean option is
// not in the list.
const booleanWeights = new Map(Object.keys({
    nobrace: 0,
    nocomment: 0,
    nonegate: 0,
    noglobstar: 0,
    noext: 0,
    nonull: 0,
    partial: 0,
    dot: 0,
    nocase: 0,
    nocaseMagicOnly: 0,
    magicalBraces: 0,
    matchBase: 0,
    flipNegate: 0,
    preserveMultipleSlashes: 0,
    windowsNoMagicRoot: 0,
}).map((k, i) => [k, 24 * 3 ** i]));
// The cache key for a set of options. It is a number, unless an
// option is set to a value that the number can not show, such as
// normalize or an uncommon platform. Then it is a JSON string.
// The defaults give 0, the same key as no options.
// It only reads the keys that are set, so that the speed does not
// depend on the shape of the options object.
const optionsKey = (o) => {
    let n = 0;
    let wpe = false;
    // inherited keys too, because Minimatch reads them
    // oxlint-disable-next-line guard-for-in
    for (const k in o) {
        const v = o[k];
        switch (k) {
            // the same test as the Minimatch constructor
            case 'windowsPathsNoEscape':
                if (v)
                    wpe = true;
                break;
            case AWE_KEY:
                if (v === false)
                    wpe = true;
                break;
            case 'platform': {
                // the same test as the Minimatch constructor
                const p = v || defaultPlatform;
                if (p === defaultPlatform)
                    break;
                if (p === 'darwin')
                    n += 3;
                else if (p === 'win32')
                    n += 6;
                else if (p === 'linux')
                    n += 9;
                else
                    return JSON.stringify(o);
                break;
            }
            case 'optimizationLevel':
                // The same tests as Minimatch: 1 when undefined, then >= 2,
                // >= 1, or else 0. The default level 1 adds 0.
                if (v === undefined)
                    break;
                n +=
                    v >= 2 ? 1
                        : v >= 1 ? 0
                            : 2;
                break;
            case 'braceExpandMax':
                if (v !== undefined && v !== DEFAULT_BRACE_EXPAND_MAX) {
                    return JSON.stringify(o);
                }
                break;
            case 'maxGlobstarRecursion':
                if (v !== undefined && v !== DEFAULT_GLOB_STAR_RECURSION_MAX) {
                    return JSON.stringify(o);
                }
                break;
            case 'normalize':
                if (v !== undefined)
                    return JSON.stringify(o);
                break;
            default:
                // unknown keys have no weight
                n += (booleanWeights.get(k) ?? 0) * tri(v);
        }
    }
    return wpe ? n + 12 : n;
};
// A copy, so a later change to the options can not change it. Unlike
// a spread, it keeps the inherited keys, which optionsKey() and the
// Minimatch constructor also read.
const copyOptions = (o) => {
    const copy = {};
    // oxlint-disable-next-line guard-for-in
    for (const k in o)
        copy[k] = o[k];
    return copy;
};
const cachedMinimatch = (pattern, options) => {
    const key = options ? optionsKey(options) : 0;
    let mm = cache.get(key)?.get(pattern);
    if (mm)
        return mm;
    mm =
        oldCache.get(key)?.get(pattern) ??
            new Minimatch(pattern, copyOptions(options));
    const weight = cacheWeight(mm);
    // too big to keep, so it is compiled again each time
    if (weight > cacheMaxEntryWeight)
        return mm;
    if (cacheSize >= cacheMax ||
        cacheTotalWeight + weight > cacheMaxWeight) {
        oldCache = cache;
        cache = new Map();
        cacheSize = 0;
        cacheTotalWeight = 0;
    }
    let byPattern = cache.get(key);
    if (!byPattern)
        cache.set(key, (byPattern = new Map()));
    byPattern.set(pattern, mm);
    cacheSize++;
    cacheTotalWeight += weight;
    return mm;
};
/* c8 ignore start */
const defaultPlatform = (typeof process === 'object' && process ?
    process.env?.__MINIMATCH_TESTING_PLATFORM__ || process.platform
    : 'posix');
const path = {
    win32: { sep: '\\' },
    posix: { sep: '/' },
};
/* c8 ignore stop */
export const sep = defaultPlatform === 'win32' ? path.win32.sep : path.posix.sep;
minimatch.sep = sep;
export const GLOBSTAR = Symbol('globstar **');
minimatch.GLOBSTAR = GLOBSTAR;
export const filter = (pattern, options) => {
    let mm;
    return (p) => (mm ??= cachedMinimatch(pattern, options)).match(p);
};
minimatch.filter = filter;
const ext = (a, b = {}) => Object.assign({}, a, b);
export const defaults = (def) => {
    if (!def || typeof def !== 'object' || !Object.keys(def).length) {
        return minimatch;
    }
    const orig = minimatch;
    const m = (p, pattern, options = {}) => orig(p, pattern, ext(def, options));
    return Object.assign(m, {
        Minimatch: class Minimatch extends orig.Minimatch {
            constructor(pattern, options = {}) {
                super(pattern, ext(def, options));
            }
            static defaults(options) {
                return orig.defaults(ext(def, options)).Minimatch;
            }
        },
        unescape: (s, options = {}) => orig.unescape(s, ext(def, options)),
        escape: (s, options = {}) => orig.escape(s, ext(def, options)),
        filter: (pattern, options = {}) => orig.filter(pattern, ext(def, options)),
        defaults: (options) => orig.defaults(ext(def, options)),
        braceExpand: (pattern, options = {}) => orig.braceExpand(pattern, ext(def, options)),
        match: (list, pattern, options = {}) => orig.match(list, pattern, ext(def, options)),
        sep: orig.sep,
        GLOBSTAR: GLOBSTAR,
    });
};
minimatch.defaults = defaults;
// Brace expansion:
// a{b,c}d -> abd acd
// a{b,}c -> abc ac
// a{0..3}d -> a0d a1d a2d a3d
// a{b,c{d,e}f}g -> abg acdfg acefg
// a{b,c}d{e,f}g -> abdeg acdeg abdeg abdfg
//
// Invalid sets are not expanded.
// a{2..}b -> a{2..}b
// a{b}c -> a{b}c
export const braceExpand = (pattern, options = {}) => {
    assertValidPattern(pattern);
    // Thanks to Yeting Li <https://github.com/yetingli> for
    // improving this regexp to avoid a ReDOS vulnerability.
    if (options.nobrace || !/\{(?:(?!\{).)*\}/.test(pattern)) {
        // shortcut. no need to expand.
        return [pattern];
    }
    // brace-expansion turns each escaped backslash '\\' into a single '\',
    // which then escapes the next character instead, and can turn an
    // escaped magic character (for example '\\\*') into a live one.
    // Double every pair, so an escaped backslash comes back out still
    // escaped. The parity of each run of backslashes is unchanged.
    let p = pattern.replace(/\\\\/g, '\\\\\\\\');
    // brace-expansion does not know about [...] classes. In
    // windowsPathsNoEscape mode, escape() writes a literal brace as '[{]'
    // or '[}]'. Escape the brace for brace-expansion, which removes that
    // backslash again, leaving '[{]' or '[}]'.
    if (options.windowsPathsNoEscape || options[AWE_KEY] === false) {
        p = p.replace(/\[([{}])\]/g, '[\\$1]');
    }
    return expand(p, { max: options.braceExpandMax });
};
minimatch.braceExpand = braceExpand;
// A numeric range with at least this many values is matched as one
// pattern portion, rather than expanded. For a glob walk, that is
// one directory read, in place of a stat for each value.
const RANGE_MIN = 100;
// a {lo..hi} range that is not escaped, and not a ${...} variable
const rangeRe = /(?<![\\$])\{(-?\d+)\.\.(-?\d+)\}/g;
// true if a range mark in this portion is inside a `!(...)`,
// `*(...)` or `+(...)` extglob. Those repeat or negate their
// contents, so one Matcher for the whole range does not match the
// same strings as the union of one pattern per value. For example,
// `*({1..100})` would match `510` as `5` then `10`, and `!({1..100})`
// would not match `5`, but every expanded pattern except `!(5)` does.
// `@(...)` and `?(...)` are fine: they pick one value at most.
const inRepeatOrNegExt = (portion, mark) => {
    const stack = [];
    for (let i = 0; i < portion.length; i++) {
        const c = portion.charAt(i);
        if (c === '\\') {
            i++;
        }
        else if (c === mark) {
            if (stack.some(t => t !== '@' && t !== '?'))
                return true;
        }
        else if (portion.charAt(i + 1) === '(' && '!?+*@'.includes(c)) {
            stack.push(c);
            i++;
        }
        else if (c === ')') {
            stack.pop();
        }
    }
    return false;
};
export const match = (list, pattern, options = {}) => {
    const mm = new Minimatch(pattern, options);
    list = list.filter(f => mm.match(f));
    if (mm.options.nonull && !list.length) {
        list.push(pattern);
    }
    return list;
};
minimatch.match = match;
const globMagic = /[?*]|[+@!]\(.*?\)|\[|\]/;
// Index of the `]` that closes the class that starts at `start`,
// or -1 if it is not closed. Posix classes such as `[:alpha:]` are
// not skipped, so this can find a `]` before the real end. That
// only makes a class look less like a literal.
const classEnd = (pattern, start) => {
    let i = start + 1;
    if (pattern.charAt(i) === '!' || pattern.charAt(i) === '^')
        i++;
    let sawStart = false;
    for (; i < pattern.length; i++) {
        const c = pattern.charAt(i);
        if (c === ']' && sawStart)
            return i;
        sawStart = true;
        if (c === '\\')
            i++;
    }
    return -1;
};
// If a path portion pattern has no magic, return the string it
// matches. Otherwise, return undefined. This can report magic for
// a pattern that is in fact literal (eg, `[]`), but never the
// reverse, so a string result is always safe to compare directly.
const literal = (pattern, noext) => {
    if (!noext && (pattern === '+()' || pattern === '@()'))
        return pattern;
    let lit = '';
    for (let i = 0; i < pattern.length; i++) {
        const c = pattern.charAt(i);
        if (c === '\\') {
            // a trailing \ is a literal \
            lit += pattern.charAt(++i) || c;
        }
        else if (c === '*' || c === '?') {
            return undefined;
        }
        else if (c === '[') {
            const close = classEnd(pattern, i);
            if (close === -1) {
                // not closed, so the [ is a literal
                lit += c;
                continue;
            }
            // a class of one char, such as `[*]` or `[\b]`, is a literal
            let body = pattern.slice(i + 1, close);
            if (body.startsWith('\\'))
                body = body.slice(1);
            if ([...body].length !== 1 || '!^'.includes(pattern.charAt(i + 1))) {
                return undefined;
            }
            lit += body;
            i = close;
        }
        else if (!noext &&
            '+@!'.includes(c) &&
            pattern.charAt(i + 1) === '(' &&
            pattern.includes(')', i + 2)) {
            return undefined;
        }
        else {
            lit += c;
        }
    }
    return lit;
};
// The matching engine compiles and walks extglobs recursively, so a
// path portion with very many extglobs (nested, such as `*(*(*(a)))`,
// or in a row, such as `?(a)?(a)?(a)`) can exhaust the call stack.
// Portions with more than this many extglobs are rejected.
const MAX_EXTGLOBS = 256;
// The number of extglob openers, such as `*(`, in a path portion
// pattern. Chars in a class are counted too, so this can be more than
// the number of extglobs, but never less.
const extglobCount = (pattern) => {
    let n = 0;
    for (let i = 0; i < pattern.length - 1; i++) {
        const c = pattern.charAt(i);
        if (c === '\\') {
            i++;
        }
        else if ('+@!?*'.includes(c) && pattern.charAt(i + 1) === '(') {
            n++;
            i++;
        }
    }
    return n;
};
const assertValidExtglobs = (pattern) => {
    if (extglobCount(pattern) > MAX_EXTGLOBS) {
        throw new TypeError('pattern has too many extglobs');
    }
};
// True if the portion pattern has the same meaning as an alternative
// inside `@(...)`. It must not have a `|` or `)` that is not in a
// closed extglob (those are literal at the top level), an extglob
// that is not closed (the `)` of the `@(...)` would close it), or a
// trailing `\` (it would escape the next `|`). A pattern that is
// only an empty extglob, such as `?()`, is literal on its own, but
// not in `@(...)`. Posix classes such as `[:alpha:]` can hide a `]`
// from classEnd, so patterns with them are not merged.
const extSafe = (pattern) => {
    if (/^[?*+@]\(\)$/.test(pattern) || pattern.includes('[:'))
        return false;
    let depth = 0;
    for (let i = 0; i < pattern.length; i++) {
        const c = pattern.charAt(i);
        if (c === '\\') {
            if (++i === pattern.length)
                return false;
        }
        else if (c === '[') {
            const close = classEnd(pattern, i);
            if (close !== -1)
                i = close;
        }
        else if ('+@!?*'.includes(c) && pattern.charAt(i + 1) === '(') {
            depth++;
            i++;
        }
        else if (c === ')') {
            if (!depth--)
                return false;
        }
        else if (c === '|' && !depth) {
            return false;
        }
    }
    return !depth;
};
// The last path that Minimatch#match split, and its parts. A path is
// often tested against many patterns in a row (such as a list of
// ignore patterns), so this saves splitting it again. The parts are
// never changed, so they can be shared.
let lastSplit;
// The last path that Minimatch#match checked before splitting it. If
// the same path is checked again, it is being tested against many
// patterns, so it is split and the parts are kept for the others.
let lastChecked;
// true if a pattern portion that is not ** matches a path portion.
// An empty path portion only matches an empty pattern portion. A path
// portion never has a `/`, so the Matcher does not need to check it.
const portionMatch = (p, f) => typeof p === 'string' ? f === p : f !== '' && p.testUnchecked(f);
// Where the last portion of a row matches in a path: the start of the
// last portion of the path, or of the one before a trailing `/`. -1 if
// it matches neither.
const endIndex = (p, last, lastStart, prev, prevStart) => portionMatch(p, last) ? lastStart
    : prev !== undefined && portionMatch(p, prev) ? prevStart
        : -1;
const isDots = (p) => p === '.' || p === '..';
// a Windows drive letter portion, like `c:`
const isDrive = (p) => typeof p === 'string' && /^[a-z]:$/i.test(p);
// true for the `UNC` portion of a long UNC root, //?/UNC/host/share
const isLongUNC = (p) => typeof p === 'string' && /^unc$/i.test(p);
// The index of the host in a row or split path that starts with a
// Windows UNC root: 2 for `//host/share`, and 4 for
// `//?/UNC/host/share`. Otherwise -1. `//?/c:` is not one.
const uncHost = (p) => {
    if (p[0] !== '' || p[1] !== '')
        return -1;
    const h = p[2] !== '?' ? 2
        : isLongUNC(p[3]) ? 4
            : -1;
    const host = p[h];
    const share = p[h + 1];
    return (h !== -1 &&
        host !== undefined &&
        host !== '' &&
        host !== GLOBSTAR &&
        share !== undefined &&
        share !== '' &&
        share !== GLOBSTAR) ?
        h
        : -1;
};
// true if ** can walk the path portion: not `.` or `..`, and not a
// dotfile unless `dot` is set
const walkable = (p, dot) => !isDots(p) && (dot || !p.startsWith('.'));
// True if a portion of the path from `start` to `end` can not be
// walked by **: one that starts with `.`, or with `dot`, one that is
// `.` or `..`. `start` is the start of a portion, and `end` is the end
// of the path, or just after a separator.
const dotPortion = (f, start, end, dot, sep) => {
    for (let k = start; k < end;) {
        if (f.charCodeAt(k) === 46) {
            if (!dot)
                return true;
            const n = f.charCodeAt(k + 1) === 46 ? k + 2 : k + 1;
            if (n >= end || isSep(f.charCodeAt(n), sep))
                return true;
        }
        // the start of the next portion, after any run of separators
        const s = nextSep(f, k, sep);
        if (s === -1)
            return false;
        k = s + 1;
        while (k < end && isSep(f.charCodeAt(k), sep))
            k++;
    }
    return false;
};
const isSep = (c, sep) => sep === '/' ? c === 47
    : sep === '\\' ? c === 92
        : c === 47 || c === 92;
// the index of the first separator at or after `from`, or -1
const nextSep = (f, from, sep) => {
    if (sep !== 'both')
        return f.indexOf(sep, from);
    const s = f.indexOf('/', from);
    const b = f.indexOf('\\', from);
    return (s === -1 ? b
        : b === -1 ? s
            : Math.min(s, b));
};
// the index of the last separator at or before `from`, or -1
const lastSep = (f, from, sep) => sep !== 'both' ?
    f.lastIndexOf(sep, from)
    : Math.max(f.lastIndexOf('/', from), f.lastIndexOf('\\', from));
// What parse() returns in place of a Matcher while make() builds and
// merges the rows. Most of the Matchers in rows that are merged would
// never be used, so they are only made for the rows that are left.
const PENDING = {};
export class Minimatch {
    options;
    set;
    pattern;
    windowsPathsNoEscape;
    nonegate;
    negate;
    comment;
    empty;
    preserveMultipleSlashes;
    partial;
    globSet;
    globParts;
    nocase;
    isWindows;
    platform;
    windowsNoMagicRoot;
    maxGlobstarRecursion;
    // the path can be checked against the last portion, and the string
    // portions, of each row before it is split. See #canMatch.
    #endCheck;
    #literalCheck;
    // level 2 resolves `.` and `..` portions in the path
    #level2;
    // the unicode normalization form, if any
    #form;
    // the string portions of each row in the set
    #literals = [];
    // the rows that #canMatch can match without splitting the path
    #shapes = [];
    // the rows whose last portion can be checked against the path's
    #endRows = [];
    // brace expansion can make the same portion in many rows, so they
    // share one Matcher, and the states that it caches
    #matchers = new Map();
    // parse() returns PENDING in place of a Matcher
    #pending = false;
    // the char that marks numeric ranges in globSet, if any
    #rangeMark;
    // the GlobstarParts of each pattern row, by start index and partial
    #globstarParts = new WeakMap();
    constructor(pattern, options = {}) {
        assertValidPattern(pattern);
        options = options || {};
        this.options = options;
        this.maxGlobstarRecursion =
            options.maxGlobstarRecursion ?? DEFAULT_GLOB_STAR_RECURSION_MAX;
        this.pattern = pattern;
        this.platform = options.platform || defaultPlatform;
        this.isWindows = this.platform === 'win32';
        this.windowsPathsNoEscape =
            !!options.windowsPathsNoEscape || options[AWE_KEY] === false;
        if (this.windowsPathsNoEscape) {
            this.pattern = this.pattern.replace(/\\/g, '/');
        }
        this.preserveMultipleSlashes = !!options.preserveMultipleSlashes;
        this.negate = false;
        this.nonegate = !!options.nonegate;
        this.comment = false;
        this.empty = false;
        this.partial = !!options.partial;
        this.nocase = !!this.options.nocase;
        this.windowsNoMagicRoot =
            options.windowsNoMagicRoot !== undefined ?
                options.windowsNoMagicRoot
                : !!(this.isWindows && this.nocase);
        this.globSet = [];
        this.globParts = [];
        this.set = [];
        this.#endCheck = !this.preserveMultipleSlashes;
        this.#level2 = (options.optimizationLevel ?? 1) >= 2;
        const n = options.normalize;
        this.#form = n === true ? 'NFC' : n || undefined;
        // make the set of Matchers etc.
        this.make();
        // On Windows, the portions of a row up to its drive letter or UNC
        // share are skipped or compared without case, if the path has one
        // too. So the checks only use the portions after it.
        const drives = this.set.map(row => this.#rootIndex(row));
        this.#endRows = this.set.map((row, r) => this.#endCheck && row.length - 1 > drives[r]);
        // with the end check, the last portion is already checked
        this.#literals = this.set.map((row, r) => {
            const end = this.#endRows[r] ? row.length - 1 : row.length;
            const lits = row
                .slice(drives[r] + 1, end)
                .filter((p) => typeof p === 'string' && !!p);
            return [...new Set(lits)];
        });
        this.#literalCheck = this.#literals.some(l => l.length);
        this.#shapes = this.set.map((row, r) => {
            const [p0, p1, p2] = row;
            if (this.#endRows[r] && row.length === 2) {
                if (p0 === GLOBSTAR && p1 !== GLOBSTAR)
                    return 'tail';
            }
            // the basename of a path that is split on / is its last portion,
            // or the one before a trailing /, the same as the end check
            if (this.#endRows[r] &&
                options.matchBase &&
                row.length === 1 &&
                p0 !== GLOBSTAR &&
                p0 !== '') {
                return 'base';
            }
            // the portion must not be a root, or '', which can be a
            // different portion on Windows (where //x paths get an extra '')
            if (!this.#endCheck || drives[r] !== -1)
                return undefined;
            if (row.length === 3 &&
                p0 === GLOBSTAR &&
                p2 === GLOBSTAR &&
                p1 !== GLOBSTAR &&
                p1 !== '' &&
                // the portion between is only matched below this limit
                this.maxGlobstarRecursion > 0) {
                return 'mid';
            }
            if (row.length === 2 &&
                p1 === GLOBSTAR &&
                p0 !== GLOBSTAR &&
                p0 !== '') {
                return 'head';
            }
            return undefined;
        });
    }
    // a string portion of a pattern, normalized if the option is set
    #norm(p) {
        return this.#form ? p.normalize(this.#form) : p;
    }
    // On Windows, the index of the last root portion of a row, as
    // #matchRow finds it: 0 for `c:/...`, 3 for `//?/c:/...` and
    // `//host/share/...`, and 5 for `//?/UNC/host/share/...`. Otherwise
    // -1.
    #rootIndex(row) {
        if (!this.isWindows)
            return -1;
        if (isDrive(row[0]))
            return 0;
        const uncDrive = row[0] === '' && row[1] === '' && row[2] === '?' && isDrive(row[3]);
        if (uncDrive)
            return 3;
        const h = uncHost(row);
        return h === -1 ? -1 : h + 1;
    }
    hasMagic() {
        if (this.options.magicalBraces && this.set.length > 1) {
            return true;
        }
        for (const pattern of this.set) {
            for (const part of pattern) {
                if (typeof part !== 'string')
                    return true;
            }
        }
        return false;
    }
    make() {
        const pattern = this.pattern;
        const options = this.options;
        // empty patterns and comments match nothing.
        if (!options.nocomment && pattern.charAt(0) === '#') {
            this.comment = true;
            return;
        }
        if (!pattern) {
            this.empty = true;
            return;
        }
        // step 1: figure out negation, etc.
        this.parseNegate();
        // step 2: expand braces
        this.globSet = [...new Set(this.#expandBraces())];
        // step 3: now we have a set, so turn each one into a series of
        // path-portion matching patterns.
        // These will be Matchers, except in the case of "**", which is
        // set to the GLOBSTAR object for globstar behavior,
        // and will not contain any / characters
        //
        // First, we preprocess to make the glob pattern sets a bit simpler
        // and deduped.  There are some perf-killing patterns that can cause
        // problems with a glob walk, but we can simplify them down a bit.
        const rawGlobParts = this.globSet.map(s => this.#slashSplit(s));
        this.globParts = this.preprocess(rawGlobParts);
        // glob --> Matchers
        this.#pending = true;
        let set = this.globParts.map((s, _, __) => {
            if (this.isWindows && this.windowsNoMagicRoot) {
                // check if it's a drive or unc path. The root of a long UNC
                // path, //?/UNC/host/share, is 6 portions.
                const n = s[2] === '?' && isLongUNC(s[3]) ? 6 : 4;
                const isUNC = s[0] === '' &&
                    s[1] === '' &&
                    (s[2] === '?' || !globMagic.test(s[2])) &&
                    s.slice(3, n).every(p => !globMagic.test(p));
                const isDrive = /^[a-z]:/i.test(s[0]);
                if (isUNC) {
                    return [
                        ...s.slice(0, n).map(p => this.#norm(p)),
                        ...s.slice(n).map(ss => this.parse(ss)),
                    ];
                }
                else if (isDrive) {
                    return [
                        this.#norm(s[0]),
                        ...s.slice(1).map(ss => this.parse(ss)),
                    ];
                }
            }
            return s.map(ss => this.parse(ss));
        });
        // filter out everything that didn't compile properly.
        this.set = set.filter(s => s.indexOf(false) === -1);
        // do not treat the ? in UNC paths as magic
        if (this.isWindows) {
            for (let i = 0; i < this.set.length; i++) {
                const p = this.set[i];
                if (p[0] === '' &&
                    p[1] === '' &&
                    this.globParts[i][2] === '?' &&
                    (isDrive(p[3]) || isLongUNC(this.globParts[i][3]))) {
                    if (isLongUNC(this.globParts[i][3]))
                        p[3] = 'UNC';
                    p[2] = '?';
                }
            }
        }
        // with noext, `@(...)` would be literal
        if (!options.noext)
            this.mergeRows();
        this.#pending = false;
        this.set.forEach((row, i) => {
            for (let k = 0; k < row.length; k++) {
                if (row[k] === PENDING) {
                    row[k] = this.parse(this.globParts[i][k]);
                }
            }
        });
    }
    // Merge rows that only differ in one portion, where each of them
    // is a Matcher, into one row that matches any of them with
    // `@(a|b|...)`. Then a path is only walked once for all of them,
    // for example with `**/*.{js,ts}`.
    mergeRows() {
        let merged = true;
        while (merged) {
            merged = false;
            // a single row cannot be merged with anything
            if (this.set.length < 2)
                return;
            let keys = this.#mergeKeys();
            const len = Math.max(0, ...this.set.map(r => r.length));
            for (let k = 0; k < len; k++) {
                // the rows that are the same other than at portion k, keyed
                // by the ids of the parts before k and the parts after k,
                // which are at most the number of rows
                const groups = new Map();
                const R = this.set.length + 1;
                this.set.forEach((row, i) => {
                    if (!(row[k] instanceof Matcher || row[k] === PENDING) ||
                        !extSafe(this.globParts[i][k])) {
                        return;
                    }
                    const { pre, suf } = keys[i];
                    const key = pre[k] * R + suf[k + 1];
                    const g = groups.get(key);
                    if (g)
                        g.push(i);
                    else
                        groups.set(key, [i]);
                });
                const drop = new Set();
                for (const g of groups.values()) {
                    if (g.length < 2)
                        continue;
                    const [first, ...rest] = g;
                    const alts = g.map(i => this.globParts[i][k]);
                    const glob = `@(${alts.join('|')})`;
                    // merging is only an optimization, so leave the rows as they
                    // are if the merged portion would have too many extglobs
                    if (extglobCount(glob) > MAX_EXTGLOBS)
                        continue;
                    this.globParts[first][k] = glob;
                    this.set[first][k] = this.parse(glob);
                    for (const i of rest)
                        drop.add(i);
                }
                if (drop.size) {
                    merged = true;
                    this.set = this.set.filter((_, i) => !drop.has(i));
                    this.globParts = this.globParts.filter((_, i) => !drop.has(i));
                    // every merge drops a row, so this happens at most once per row
                    if (this.set.length < 2)
                        return;
                    keys = this.#mergeKeys();
                }
            }
        }
    }
    // For each row, number ids for its prefixes and suffixes, such that
    // pre[k] identifies parts[0..k) and suf[k] identifies parts[k..n).
    // Two rows are the same other than at portion k exactly when they
    // have the same pre[k] and suf[k + 1]. This is linear in the size of
    // the rows, where joining the whole row for every k was quadratic.
    #mergeKeys() {
        const partIds = new Map();
        const ids = this.globParts.map(parts => parts.map(s => {
            let id = partIds.get(s);
            if (id === undefined)
                partIds.set(s, (id = partIds.size));
            return id;
        }));
        // ids are only compared at the same k, so they are numbered per
        // portion, with 0 for the empty prefix or suffix. Then they are
        // at most the number of rows, and id * P + part id is a safe
        // integer key for any set that fits in memory.
        const P = partIds.size;
        const len = Math.max(0, ...ids.map(r => r.length));
        const keys = ids.map(r => ({
            pre: [0],
            suf: new Array(r.length + 1).fill(0),
        }));
        for (let k = 0; k < len; k++) {
            const m = new Map();
            ids.forEach((r, i) => {
                if (k >= r.length)
                    return;
                const { pre } = keys[i];
                const key = pre[k] * P + r[k];
                let id = m.get(key);
                if (id === undefined)
                    m.set(key, (id = m.size + 1));
                pre.push(id);
            });
        }
        for (let k = len - 1; k >= 0; k--) {
            const m = new Map();
            ids.forEach((r, i) => {
                if (k >= r.length)
                    return;
                const { suf } = keys[i];
                const key = suf[k + 1] * P + r[k];
                let id = m.get(key);
                if (id === undefined)
                    m.set(key, (id = m.size + 1));
                suf[k] = id;
            });
        }
        return keys;
    }
    // various transforms to equivalent pattern sets that are
    // faster to process in a filesystem walk.  The goal is to
    // eliminate what we can, and push all ** patterns as far
    // to the right as possible, even if it increases the number
    // of patterns that we have to process.
    preprocess(globParts) {
        // if we're not in globstar mode, then turn ** into *
        if (this.options.noglobstar) {
            for (const partset of globParts) {
                for (let j = 0; j < partset.length; j++) {
                    if (partset[j] === '**') {
                        partset[j] = '*';
                    }
                }
            }
        }
        const { optimizationLevel = 1 } = this.options;
        if (optimizationLevel >= 2) {
            // aggressive optimization for the purpose of fs walking
            globParts = this.firstPhasePreProcess(globParts);
            globParts = this.secondPhasePreProcess(globParts);
        }
        else if (optimizationLevel >= 1) {
            // just basic optimizations to remove some .. parts
            globParts = this.levelOneOptimize(globParts);
        }
        else {
            // just collapse multiple ** portions into one
            globParts = this.adjascentGlobstarOptimize(globParts);
        }
        return globParts;
    }
    // just get rid of adjascent ** portions
    adjascentGlobstarOptimize(globParts) {
        return globParts.map(parts => {
            let gs = -1;
            while (-1 !== (gs = parts.indexOf('**', gs + 1))) {
                let i = gs;
                while (parts[i + 1] === '**') {
                    i++;
                }
                if (i !== gs) {
                    parts.splice(gs, i - gs);
                }
            }
            return parts;
        });
    }
    // get rid of adjascent ** and resolve .. portions
    levelOneOptimize(globParts) {
        return globParts.map(parts => {
            // .. can not remove the root of a Windows path
            const root = this.#rootIndex(parts) + 1;
            parts = parts.reduce((set, part) => {
                const prev = set[set.length - 1];
                if (part === '**' && prev === '**') {
                    return set;
                }
                if (part === '..') {
                    if (set.length > root &&
                        prev &&
                        prev !== '..' &&
                        prev !== '.' &&
                        prev !== '**') {
                        set.pop();
                        return set;
                    }
                }
                set.push(part);
                return set;
            }, []);
            return parts.length === 0 ? [''] : parts;
        });
    }
    // Check the path against each row before it is split. 'no' if no
    // row can match, so it does not need to be split. 'yes' if a row of
    // `**` and one more portion matches. 'maybe' if it must be split to
    // know. None of this applies with partial, where the path can end
    // before the row does.
    //
    // On Windows, `\` is a separator in the path too, and the portions of
    // a row up to its drive letter or UNC share are not checked (see
    // #rootIndex).
    //
    // Each string portion of a row must be a portion of the path, so the
    // path must contain it. Level 2 can only take portions out of the
    // path, so this is still true then.
    //
    // The last portion of a row must match the last portion of the path,
    // or the one before a trailing `/` (as a/* and a/**/* match a/b/).
    // This does not apply to rows that end in **, rows that end in a
    // root, or preserveMultipleSlashes (which can leave more
    // than one '' at the end). At level 2, it does not apply if that
    // portion is `.` or `..`, which can be
    // resolved away. Level 2 only takes other portions out of the
    // middle of the path.
    //
    // A row of `**/x` then matches if the ** can walk the portions
    // before the one that x matches, the same as #matchGlobstar. At
    // level 2, this is only known if none of them are `.` or `..`, so
    // that the path does not change when it is resolved.
    #canMatch(f) {
        const sep = !this.isWindows || !f.includes('\\') ? '/'
            : f.includes('/') ? 'both'
                : '\\';
        // the portions that f.split(/\/+/) would end with, and where they
        // start in f, found when a row needs them. `ends` is 0 until they
        // are found, then 1, or -1 if they can not be used.
        let ends = this.#endCheck ? 0 : -1;
        let last = '';
        let lastStart = 0;
        let prev;
        let prevStart = 0;
        const dot = !!this.options.dot;
        // A string compare is cheaper than includes(), and a Matcher test
        // costs more, so the cheaper check goes first.
        ROWS: for (let r = 0; r < this.set.length; r++) {
            const row = this.set[r];
            const p = row[row.length - 1];
            let end = ends !== -1 && this.#endRows[r] && p !== GLOBSTAR;
            if (end && ends === 0) {
                const i = lastSep(f, f.length, sep);
                lastStart = i + 1;
                last = f.slice(lastStart);
                if (last === '' && i >= 0) {
                    let j = i;
                    while (j > 0 && isSep(f.charCodeAt(j - 1), sep))
                        j--;
                    prevStart = j === 0 ? 0 : lastSep(f, j - 1, sep) + 1;
                    prev = f.slice(prevStart, j);
                }
                const resolves = this.#level2 &&
                    (isDots(last) ||
                        (last === '' && prev !== undefined && isDots(prev)));
                ends = resolves ? -1 : 1;
                end = !resolves;
            }
            let tailStart = -1;
            if (end && typeof p === 'string') {
                tailStart = endIndex(p, last, lastStart, prev, prevStart);
                if (tailStart === -1)
                    continue;
            }
            if (this.#literalCheck) {
                for (const l of this.#literals[r]) {
                    if (!f.includes(l))
                        continue ROWS;
                }
            }
            if (end && typeof p === 'object') {
                tailStart = endIndex(p, last, lastStart, prev, prevStart);
                if (tailStart === -1)
                    continue;
            }
            const shape = this.#shapes[r];
            if (shape === 'base' && end)
                return 'yes';
            if (shape === 'tail' && end) {
                if (this.#level2 && dotPortion(f, 0, tailStart, true, sep)) {
                    return 'maybe';
                }
                if (!dotPortion(f, 0, tailStart, dot, sep))
                    return 'yes';
                continue;
            }
            if (shape === 'mid' || shape === 'head') {
                const m = this.#walk(f, row, shape === 'head', dot, sep);
                if (m !== 'no')
                    return m;
                continue;
            }
            return 'maybe';
        }
        return 'no';
    }
    // Match a row of a known shape against the split path, the same as
    // #matchGlobstar and #matchGlobStarBodySections do. None of these
    // rows have a drive letter, so they start at the first portion.
    #matchShape(file, row, shape) {
        const dot = !!this.options.dot;
        const n = file.length;
        if (shape === 'head') {
            // x, then at least one portion that ** can walk
            if (n < 2 || !portionMatch(row[0], file[0])) {
                return false;
            }
            for (let i = 1; i < n; i++)
                if (!walkable(file[i], dot))
                    return false;
            return true;
        }
        const x = row[1];
        if (shape === 'tail') {
            // x is the last portion, or the one before a trailing ''
            let t = n - 1;
            if (!portionMatch(x, file[t])) {
                if (file[t] !== '' || n === 1 || !portionMatch(x, file[--t])) {
                    return false;
                }
            }
            for (let i = 0; i < t; i++)
                if (!walkable(file[i], dot))
                    return false;
            return true;
        }
        // mid: the first x that ** can walk to, with a portion after it,
        // and ** can walk all of those
        for (let k = 0; k < n; k++) {
            if (portionMatch(x, file[k]) && k + 1 < n) {
                let i = k + 1;
                while (i < n && walkable(file[i], dot))
                    i++;
                if (i === n)
                    return true;
            }
            if (!walkable(file[k], dot))
                return false;
        }
        return false;
    }
    // Match a row of `**/x/**` or `x/**` against the path, the same as
    // #matchGlobstar and #matchGlobStarBodySections do once it is split.
    // x must match a portion (the first one, for `x/**`), the ** before
    // it must be able to walk the portions before that one, and there
    // must be a portion after it, which the ** after it can walk. At
    // level 2, this is only known if no portion is `.` or `..`.
    #walk(f, row, head, dot, sep) {
        if (this.#level2 && dotPortion(f, 0, f.length, true, sep))
            return 'maybe';
        const x = row[head ? 0 : 1];
        // the last portion has none after it, so it can not be x
        let s = 0;
        for (let e = nextSep(f, s, sep); e !== -1; e = nextSep(f, s, sep)) {
            // the start of the next portion
            let next = e + 1;
            while (next < f.length && isSep(f.charCodeAt(next), sep))
                next++;
            const hit = typeof x === 'string' ?
                e - s === x.length && f.startsWith(x, s)
                : e > s && x.testUnchecked(f.slice(s, e));
            if (hit && !dotPortion(f, next, f.length, dot, sep))
                return 'yes';
            if (head || dotPortion(f, s, next, dot, sep))
                return 'no';
            s = next;
        }
        return 'no';
    }
    // the last split, if it was the same path with the same split
    // settings
    #cachedSplit(f) {
        const last = lastSplit;
        return (last?.f === f &&
            last.isWindows === this.isWindows &&
            last.preserveMultipleSlashes === this.preserveMultipleSlashes &&
            last.normalize === this.#form) ?
            last
            : undefined;
    }
    // split a path for match(), and keep the parts for the next call
    #split(f) {
        const { isWindows, preserveMultipleSlashes } = this;
        // windows: need to use /, not \
        let parts = this.#slashSplit(isWindows ? f.split('\\').join('/') : f);
        // normalize each portion, so that a char that normalizes to `/`
        // does not make a new portion
        const normalize = this.#form;
        if (normalize)
            parts = parts.map(p => p.normalize(normalize));
        lastSplit = {
            f,
            isWindows,
            preserveMultipleSlashes,
            normalize,
            parts,
        };
        return lastSplit;
    }
    // resolve `.` and `..` portions of a path, for level 2
    #levelTwoFileOptimize(file) {
        // work on a copy, so that the cached parts do not change
        const parts = [...file];
        let didSomething = false;
        do {
            didSomething = false;
            // <pre>/<e>/<rest> -> <pre>/<rest>
            if (!this.preserveMultipleSlashes) {
                for (let i = 1; i < parts.length - 1; i++) {
                    const p = parts[i];
                    // don't squeeze out UNC patterns
                    if (i === 1 && p === '' && parts[0] === '')
                        continue;
                    if (p === '.' || p === '') {
                        didSomething = true;
                        parts.splice(i, 1);
                        i--;
                    }
                }
                if (parts[0] === '.' &&
                    parts.length === 2 &&
                    (parts[1] === '.' || parts[1] === '')) {
                    didSomething = true;
                    parts.pop();
                }
            }
            // <pre>/<p>/../<rest> -> <pre>/<rest>
            // .. can not remove the root of a Windows path
            const root = this.#rootIndex(parts) + 1;
            let dd = root;
            while (-1 !== (dd = parts.indexOf('..', dd + 1))) {
                const p = parts[dd - 1];
                if (p && p !== '.' && p !== '..' && p !== '**') {
                    didSomething = true;
                    parts.splice(dd - 1, 2);
                    dd = Math.max(dd - 2, root);
                }
            }
        } while (didSomething);
        return parts.length === 0 ? [''] : parts;
    }
    // First phase: single-pattern processing
    // <pre> is 1 or more portions
    // <rest> is 1 or more portions
    // <p> is any portion other than ., .., '', or **
    // <e> is . or ''
    //
    // **/.. is *brutal* for filesystem walking performance, because
    // it effectively resets the recursive walk each time it occurs,
    // and ** cannot be reduced out by a .. pattern part like a regexp
    // or most strings (other than .., ., and '') can be.
    //
    // <pre>/**/../<p>/<p>/<rest> -> {<pre>/../<p>/<p>/<rest>,<pre>/**/<p>/<p>/<rest>}
    // <pre>/<e>/<rest> -> <pre>/<rest>
    // <pre>/<p>/../<rest> -> <pre>/<rest>
    // **/**/<rest> -> **/<rest>
    //
    // **/*/<rest> -> */**/<rest> <== not valid because ** doesn't follow
    // this WOULD be allowed if ** did follow symlinks, or * didn't
    firstPhasePreProcess(globParts) {
        // each **/.. split doubles the patterns, and adds a copy of the
        // whole pattern. Throw if either gets too large. A partial split
        // would match some paths and not others, depending on which rows
        // the pass reached first.
        const { braceExpandMax = DEFAULT_BRACE_EXPAND_MAX } = this.options;
        let splitCells = 0;
        const maxSplitCells = 1_000_000;
        let didSomething = false;
        do {
            didSomething = false;
            // <pre>/**/../<p>/<p>/<rest> -> {<pre>/../<p>/<p>/<rest>,<pre>/**/<p>/<p>/<rest>}
            for (let parts of globParts) {
                let gs = -1;
                while (-1 !== (gs = parts.indexOf('**', gs + 1))) {
                    let gss = gs;
                    while (parts[gss + 1] === '**') {
                        // <pre>/**/**/<rest> -> <pre>/**/<rest>
                        gss++;
                    }
                    // eg, if gs is 2 and gss is 4, that means we have 3 **
                    // parts, and can remove 2 of them.
                    if (gss > gs) {
                        parts.splice(gs + 1, gss - gs);
                    }
                    let next = parts[gs + 1];
                    const p = parts[gs + 2];
                    const p2 = parts[gs + 3];
                    if (next !== '..')
                        continue;
                    if (!p ||
                        p === '.' ||
                        p === '..' ||
                        !p2 ||
                        p2 === '.' ||
                        p2 === '..') {
                        continue;
                    }
                    // Check before changing parts or allocating the new row.
                    const addedCells = parts.length - 1;
                    if (globParts.length >= braceExpandMax ||
                        splitCells + addedCells > maxSplitCells) {
                        throw new RangeError(`optimizationLevel 2: too many patterns from '**/..'`);
                    }
                    didSomething = true;
                    // edit parts in place, and push the new one
                    parts.splice(gs, 1);
                    const other = parts.slice(0);
                    other[gs] = '**';
                    splitCells += addedCells;
                    globParts.push(other);
                    gs--;
                }
                // <pre>/<e>/<rest> -> <pre>/<rest>
                if (!this.preserveMultipleSlashes) {
                    for (let i = 1; i < parts.length - 1; i++) {
                        const p = parts[i];
                        // don't squeeze out UNC patterns
                        if (i === 1 && p === '' && parts[0] === '')
                            continue;
                        if (p === '.' || p === '') {
                            didSomething = true;
                            parts.splice(i, 1);
                            i--;
                        }
                    }
                    if (parts[0] === '.' &&
                        parts.length === 2 &&
                        (parts[1] === '.' || parts[1] === '')) {
                        didSomething = true;
                        parts.pop();
                    }
                }
                // <pre>/<p>/../<rest> -> <pre>/<rest>
                // .. can not remove the root of a Windows path
                const root = this.#rootIndex(parts) + 1;
                let dd = root;
                while (-1 !== (dd = parts.indexOf('..', dd + 1))) {
                    const p = parts[dd - 1];
                    if (p && p !== '.' && p !== '..' && p !== '**') {
                        didSomething = true;
                        const needDot = dd === 1 && parts[dd + 1] === '**';
                        const splin = needDot ? ['.'] : [];
                        parts.splice(dd - 1, 2, ...splin);
                        if (parts.length === 0)
                            parts.push('');
                        dd = Math.max(dd - 2, root);
                    }
                }
            }
        } while (didSomething);
        return globParts;
    }
    // second phase: multi-pattern dedupes
    // {<pre>/<rest>,<pre>/<rest>} -> <pre>/<rest>
    // {<pre>/*/<rest>,<pre>/<p>/<rest>} -> <pre>/*/<rest>
    secondPhasePreProcess(globParts) {
        const keys = new Set();
        const rows = globParts.filter(parts => {
            const key = parts.join('/');
            if (keys.has(key))
                return false;
            keys.add(key);
            return true;
        });
        // A row is covered by another row that has `*` at some positions,
        // and is the same at all others. Collect each distinct set of `*`
        // positions, then look up each row with `*` put at those positions.
        const stars = new Map();
        for (const parts of rows) {
            const s = [];
            for (let i = 0; i < parts.length; i++) {
                if (parts[i] === '*')
                    s.push(i);
            }
            if (s.length)
                stars.set(s.join(','), s);
        }
        // Each row is checked against each set of `*` positions, so skip
        // this if there are too many. The patterns match the same paths
        // either way.
        if (!stars.size || rows.length * stars.size > 100_000)
            return rows;
        const { dot } = this.options;
        return rows.filter(parts => {
            for (const s of stars.values()) {
                if (s[s.length - 1] >= parts.length)
                    continue;
                const c = [...parts];
                let ok = true;
                let changed = false;
                for (const i of s) {
                    const p = parts[i];
                    if (p === '*')
                        continue;
                    if (!p || p === '**' || (!dot && p.startsWith('.'))) {
                        ok = false;
                        break;
                    }
                    c[i] = '*';
                    changed = true;
                }
                if (ok && changed && keys.has(c.join('/')))
                    return false;
            }
            return true;
        });
    }
    parseNegate() {
        if (this.nonegate)
            return;
        const pattern = this.pattern;
        let negate = false;
        let negateOffset = 0;
        for (let i = 0; i < pattern.length && pattern.charAt(i) === '!'; i++) {
            negate = !negate;
            negateOffset++;
        }
        if (negateOffset)
            this.pattern = pattern.slice(negateOffset);
        this.negate = negate;
    }
    // Match the parts of a path against one row of the set. `resolved`
    // is the parts after level 2 resolves `.` and `..` in them, or the
    // same parts below level 2. Set partial to true to test if, for
    // example, "/a/b" matches the start of "/*/b/*/d". Partial means, if
    // you run out of file before you run out of pattern, then that's
    // fine, as long as all the parts match.
    #matchRow(file, resolved, pattern, partial) {
        let fileStartIndex = 0;
        let patternStartIndex = 0;
        // UNC paths like //?/X:/... can match X:/... and vice versa, and
        // //?/UNC/host/share/... can match //host/share/... and vice versa.
        // Drive letters in absolute drive or unc paths, and the host and
        // share of UNC paths, are always compared case-insensitively.
        const fh = this.isWindows ? uncHost(resolved) : -1;
        const ph = fh === -1 ? -1 : uncHost(pattern);
        if (ph !== -1) {
            const host = pattern[ph];
            const share = pattern[ph + 1];
            if (typeof host !== 'string' || typeof share !== 'string') {
                // a host or share with magic is matched as usual
                fileStartIndex = fh;
                patternStartIndex = ph;
            }
            else if (resolved[fh].toLowerCase() === host.toLowerCase() &&
                resolved[fh + 1].toLowerCase() === share.toLowerCase()) {
                fileStartIndex = fh + 2;
                patternStartIndex = ph + 2;
            }
        }
        else if (this.isWindows) {
            const fileDrive = typeof file[0] === 'string' && /^[a-z]:$/i.test(file[0]);
            const fileUNC = !fileDrive &&
                file[0] === '' &&
                file[1] === '' &&
                file[2] === '?' &&
                /^[a-z]:$/i.test(file[3]);
            const patternDrive = typeof pattern[0] === 'string' && /^[a-z]:$/i.test(pattern[0]);
            const patternUNC = !patternDrive &&
                pattern[0] === '' &&
                pattern[1] === '' &&
                pattern[2] === '?' &&
                typeof pattern[3] === 'string' &&
                /^[a-z]:$/i.test(pattern[3]);
            const fdi = fileUNC ? 3
                : fileDrive ? 0
                    : undefined;
            const pdi = patternUNC ? 3
                : patternDrive ? 0
                    : undefined;
            if (typeof fdi === 'number' && typeof pdi === 'number') {
                const [fd, pd] = [
                    file[fdi],
                    pattern[pdi],
                ];
                // the drives match, so start matching after them
                if (fd.toLowerCase() === pd.toLowerCase()) {
                    patternStartIndex = pdi + 1;
                    fileStartIndex = fdi + 1;
                }
            }
        }
        // the drive letters are found in the parts before level 2 resolves
        // them, and the match uses the resolved parts
        file = resolved;
        if (pattern.includes(GLOBSTAR)) {
            return this.#matchGlobstar(file, pattern, partial, fileStartIndex, patternStartIndex);
        }
        return this.#matchOne(file, pattern, partial, fileStartIndex, patternStartIndex);
    }
    #matchGlobstar(file, pattern, partial, fileIndex, patternIndex) {
        const { head, body, tail, segments } = this.#getGlobstarParts(pattern, partial, patternIndex);
        // check the head, from the current file/pattern index.
        if (head.length) {
            const headEnd = fileIndex + head.length;
            if (!this.#matchOne(file, head, partial, fileIndex, 0, headEnd)) {
                return false;
            }
            fileIndex += head.length;
            patternIndex += head.length;
        }
        // now we know the head matches!
        // if the last portion is not empty, it MUST match the end
        // check the tail
        let fileTailMatch = 0;
        if (tail.length) {
            // if head + tail > file, then we cannot possibly match
            if (tail.length + fileIndex > file.length)
                return false;
            // try to match the tail
            let tailStart = file.length - tail.length;
            if (this.#matchOne(file, tail, partial, tailStart, 0)) {
                fileTailMatch = tail.length;
            }
            else {
                // affordance for stuff like a/**/* matching a/b/
                // if the last file portion is '', and there's more to the pattern
                // then try without the '' bit.
                if (file[file.length - 1] !== '' ||
                    fileIndex + tail.length === file.length) {
                    return false;
                }
                tailStart--;
                if (!this.#matchOne(file, tail, partial, tailStart, 0)) {
                    return false;
                }
                fileTailMatch = tail.length + 1;
            }
        }
        // now we know the tail matches!
        // the middle is zero or more portions wrapped in **, possibly
        // containing more ** sections.
        // so a/**/b/**/c/**/d has become **/b/**/c/**
        // if it's empty, it means a/**/b, just verify we have no bad dots
        // if there's no tail, so it ends on /**, then we must have *something*
        // after the head, or it's not a matc
        if (!body.length) {
            let sawSome = !!fileTailMatch;
            for (let i = fileIndex; i < file.length - fileTailMatch; i++) {
                const f = String(file[i]);
                sawSome = true;
                if (f === '.' ||
                    f === '..' ||
                    (!this.options.dot && f.startsWith('.'))) {
                    return false;
                }
            }
            // in partial mode, we just need to get past all file parts
            return partial || sawSome;
        }
        // now we know that there's one or more body sections, which can
        // be matched anywhere from the 0 index (because the head was pruned)
        // through to the length-fileTailMatch index.
        return !!this.#matchGlobStarBodySections(file, segments, file.length - fileTailMatch, fileIndex, 0, partial, 0, !!fileTailMatch, new Set());
    }
    // Split the pattern into head, tail, and middle of ** delimited
    // parts. The tail has to be at the end, and the others just have to
    // be found in order from the head. These only depend on the pattern
    // row, so they are kept for the next path.
    #getGlobstarParts(pattern, partial, patternIndex) {
        let byStart = this.#globstarParts.get(pattern);
        if (!byStart) {
            byStart = [];
            this.#globstarParts.set(pattern, byStart);
        }
        const key = patternIndex * 2 + (partial ? 1 : 0);
        const found = byStart[key];
        if (found)
            return found;
        const firstgs = pattern.indexOf(GLOBSTAR, patternIndex);
        const lastgs = pattern.lastIndexOf(GLOBSTAR);
        const head = pattern.slice(patternIndex, firstgs);
        const body = pattern.slice(firstgs + 1, partial ? undefined : lastgs);
        const tail = partial ? [] : pattern.slice(lastgs + 1);
        // split the body up into sections, and note the number of
        // portions that must come after the start of each one
        const segments = [[[], 0]];
        let current = segments[0];
        let nonGsParts = 0;
        const nonGsPartsSums = [0];
        for (const b of body) {
            if (b === GLOBSTAR) {
                nonGsPartsSums.push(nonGsParts);
                current = [[], 0];
                segments.push(current);
            }
            else {
                current[0].push(b);
                nonGsParts++;
            }
        }
        // the portions in this section and the ones after it
        segments.forEach((b, i) => {
            b[1] = nonGsParts - nonGsPartsSums[i];
        });
        const parts = { head, body, tail, segments };
        byStart[key] = parts;
        return parts;
    }
    // return false for "nope, not matching"
    // return null for "not matching, cannot keep trying"
    #matchGlobStarBodySections(file, 
    // pattern section, and the portions that it and the rest need
    bodySegments, 
    // the end of the file portions that the body can match
    fileLength, fileIndex, bodyIndex, partial, globStarDepth, sawTail, 
    // the places that a walk has already tried, as
    // bodyIndex * (file.length + 1) + fileIndex. A walk from a place
    // does the same no matter how it got there, and it only comes back
    // if it found no match, so a walk that gets to one stops.
    tried) {
        // take the first body segment, and walk from fileIndex to its "after"
        // value at the end
        // If it doesn't match at that position, we increment, until we hit
        // that final possible position, and give up.
        // If it does match, then advance and try to rest.
        // If any of them fail we keep walking forward.
        // this is still a bit recursively painful, but it's more constrained
        // than previous implementations, because we never test something that
        // can't possibly be a valid matching condition.
        const bs = bodySegments[bodyIndex];
        if (!bs) {
            // with partial, the body is the rest of the pattern, so it
            // matches if no portions are left, or only the '' of a trailing
            // / (as a/* matches a/b/). A ** at the end walks them as its own
            // empty section.
            if (partial) {
                return (fileIndex >= fileLength ||
                    (fileIndex === fileLength - 1 && file[fileIndex] === ''));
            }
            // just make sure that there's no bad dots before the tail
            for (let i = fileIndex; i < fileLength; i++) {
                sawTail = true;
                const f = file[i];
                if (f === '.' ||
                    f === '..' ||
                    (!this.options.dot && f.startsWith('.'))) {
                    return false;
                }
            }
            return sawTail;
        }
        // have a non-globstar body section to test, up to the last index
        // where it can start
        const [body, need] = bs;
        // with partial, the path can end before the body does
        const after = partial ? fileLength : fileLength - need;
        while (fileIndex <= after) {
            const key = bodyIndex * (file.length + 1) + fileIndex;
            if (tried.has(key))
                return false;
            tried.add(key);
            const m = this.#matchOne(file, body, partial, fileIndex, 0, fileIndex + body.length);
            // if limit exceeded, no match. intentional false negative,
            // acceptable break in correctness for security.
            if (m && globStarDepth < this.maxGlobstarRecursion) {
                // match! see if the rest match. if so, we're done!
                const sub = this.#matchGlobStarBodySections(file, bodySegments, fileLength, fileIndex + body.length, bodyIndex + 1, partial, globStarDepth + 1, sawTail, tried);
                if (sub !== false) {
                    return sub;
                }
            }
            // the ** does not walk past the last place the body can start
            if (fileIndex === after)
                break;
            const f = file[fileIndex];
            if (f === '.' ||
                f === '..' ||
                (!this.options.dot && f.startsWith('.'))) {
                return false;
            }
            fileIndex++;
        }
        // walked off. no point continuing
        return partial || null;
    }
    #matchOne(file, pattern, partial, fileIndex, patternIndex, 
    // only match the file portions before this index
    fileEnd = file.length) {
        let fi;
        let pi;
        let pl;
        let fl;
        for (fi = fileIndex,
            pi = patternIndex,
            fl = Math.min(fileEnd, file.length),
            pl = pattern.length; fi < fl && pi < pl; fi++, pi++) {
            let p = pattern[pi];
            let f = file[fi];
            // should be impossible.
            /* c8 ignore start */
            if (p === false || p === GLOBSTAR) {
                return false;
            }
            /* c8 ignore stop */
            // something other than **
            // non-magic patterns just have to match exactly
            // patterns with magic have been turned into Matchers.
            if (!portionMatch(p, f))
                return false;
        }
        // Note: ending in / means that we'll get a final ""
        // at the end of the pattern.  This can only match a
        // corresponding "" at the end of the file.
        // If the file ends in /, then it can only match a
        // a pattern that ends in /, unless the pattern just
        // doesn't have any more for it. But, a/b/ should *not*
        // match "a/b/*", even though * can match "", except in
        // partial mode, where it might simply not be reached yet.
        // However, a/b/ should still satisfy a/*
        // now either we fell off the end of the pattern, or we're done.
        if (fi === fl && pi === pl) {
            // ran out of pattern and filename at the same time.
            // an exact hit!
            return true;
        }
        else if (fi === fl) {
            // ran out of file, but still had pattern left.
            // this is ok if we're doing the match as part of
            // a glob fs traversal.
            return partial;
        }
        else if (pi === pl) {
            // ran out of pattern, still have file left.
            // this is only acceptable if we're on the very last
            // empty segment of a file with a trailing slash.
            // a/* should match a/b/
            return fi === fl - 1 && file[fi] === '';
            /* c8 ignore start */
        }
        else {
            // should be unreachable.
            throw new Error('wtf?');
        }
        /* c8 ignore stop */
    }
    braceExpand() {
        return braceExpand(this.pattern, this.options);
    }
    // Expand braces, except numeric ranges such as {0..999} with at
    // least RANGE_MIN values. Each of those stays in its pattern as
    // `<mark>0..999<mark>`, for the Matcher to test. If a range might
    // be inside a [...] class, expand all of them instead.
    #expandBraces() {
        const { pattern } = this;
        if (this.options.nobrace)
            return this.braceExpand();
        let code = 0xe000;
        while (pattern.includes(String.fromCharCode(code))) {
            if (++code > 0xf8ff)
                return this.braceExpand();
        }
        const mark = String.fromCharCode(code);
        let found = false;
        const marked = pattern.replace(rangeRe, (m, lo, hi) => {
            const a = Number(lo);
            const b = Number(hi);
            if (!Number.isSafeInteger(a) ||
                !Number.isSafeInteger(b) ||
                Math.abs(a - b) + 1 < RANGE_MIN) {
                return m;
            }
            found = true;
            return `${mark}${lo}..${hi}${mark}`;
        });
        if (!found)
            return this.braceExpand();
        const set = braceExpand(marked, this.options);
        for (const p of set) {
            for (const portion of p.split('/')) {
                const c = portion.indexOf('[');
                if (c !== -1 && c < portion.lastIndexOf(mark)) {
                    return this.braceExpand();
                }
                if (!this.options.noext && inRepeatOrNegExt(portion, mark)) {
                    return this.braceExpand();
                }
            }
        }
        this.#rangeMark = mark;
        return set;
    }
    parse(pattern) {
        assertValidPattern(pattern);
        const options = this.options;
        // shortcuts
        if (pattern === '**')
            return GLOBSTAR;
        if (pattern === '')
            return '';
        const { nocase, nocaseMagicOnly, noext = false, dot, normalize, } = options;
        const rangeMark = this.#rangeMark;
        const lit = rangeMark && pattern.includes(rangeMark) ?
            undefined
            : literal(pattern, noext);
        // in nocase mode, a literal with cased chars still needs a
        // Matcher, unless nocaseMagicOnly is set.
        if (lit !== undefined &&
            !(nocase &&
                !nocaseMagicOnly &&
                pattern.toUpperCase() !== pattern.toLowerCase())) {
            return this.#norm(lit);
        }
        if (this.#pending)
            return PENDING;
        let m = this.#matchers.get(pattern);
        if (!m) {
            if (!noext)
                assertValidExtglobs(pattern);
            m = new Matcher(pattern, {
                nocase,
                noext,
                dot,
                normalize,
                ...(rangeMark && { rangeMark }),
            });
            this.#matchers.set(pattern, m);
        }
        return m;
    }
    #slashSplit(p) {
        // if p starts with // on windows, we preserve that
        // so that UNC paths aren't broken.  Otherwise, any number of
        // / characters are coalesced into one, unless
        // preserveMultipleSlashes is set to true.
        if (this.preserveMultipleSlashes) {
            return p.split('/');
        }
        else if (this.isWindows && /^\/\/[^/]+/.test(p)) {
            // add an extra '' for the one we lose
            return ['', ...p.split(/\/+/)];
        }
        else {
            return p.split(/\/+/);
        }
    }
    match(f, partial = this.partial) {
        // short-circuit in the case of busted things.
        // comments, etc.
        if (this.comment) {
            return false;
        }
        if (this.empty) {
            return f === '';
        }
        if (f === '/' && partial) {
            return true;
        }
        const options = this.options;
        // treat the test path as a set of pathparts. If it was not just
        // split or checked, check that it can match before splitting it.
        let split = this.#cachedSplit(f);
        if (!split) {
            // the checks look at the path as given, so not if it is
            // normalized
            if (!partial &&
                !this.#form &&
                (this.#endCheck || this.#literalCheck) &&
                lastChecked !== f) {
                lastChecked = f;
                const can = this.#canMatch(f);
                if (can === 'no')
                    return options.flipNegate ? false : this.negate;
                if (can === 'yes')
                    return options.flipNegate ? true : !this.negate;
            }
            split = this.#split(f);
        }
        const ff = split.parts;
        // the parts after level 2 resolves them, kept for the next pattern
        // that tests the same path
        const resolved = this.#level2 ?
            (split.resolved ??= this.#levelTwoFileOptimize(ff))
            : ff;
        // just ONE of the pattern sets in this.set needs to match
        // in order for it to be valid.  If negating, then just one
        // match means that we have failed.
        // Either way, return on the first hit.
        const set = this.set;
        // Find the basename of the path by looking for the last non-empty segment
        let filename = ff[ff.length - 1];
        if (!filename) {
            for (let i = ff.length - 2; !filename && i >= 0; i--) {
                filename = ff[i];
            }
        }
        for (let r = 0; r < set.length; r++) {
            const pattern = set[r];
            const shape = partial ? undefined : this.#shapes[r];
            // level 2 does not change a path of one portion
            const hit = shape === 'base' ?
                portionMatch(pattern[0], filename)
                : shape ? this.#matchShape(resolved, pattern, shape)
                    : options.matchBase && pattern.length === 1 ?
                        this.#matchRow([filename], [filename], pattern, partial)
                        : this.#matchRow(ff, resolved, pattern, partial);
            if (hit) {
                if (options.flipNegate) {
                    return true;
                }
                return !this.negate;
            }
        }
        // didn't get any hits.  this is success if it's a negative
        // pattern, failure otherwise.
        if (options.flipNegate) {
            return false;
        }
        return this.negate;
    }
    static defaults(def) {
        return minimatch.defaults(def).Minimatch;
    }
}
/* c8 ignore start */
export { escape } from './escape.js';
export { unescape } from './unescape.js';
/* c8 ignore stop */
minimatch.Minimatch = Minimatch;
minimatch.escape = escape;
minimatch.unescape = unescape;
//# sourceMappingURL=index.js.map