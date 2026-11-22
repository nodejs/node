import { Matcher, type Form } from '@isaacs/nonrecursive-glob-match';
export type Platform = 'aix' | 'android' | 'darwin' | 'freebsd' | 'haiku' | 'linux' | 'openbsd' | 'sunos' | 'win32' | 'cygwin' | 'netbsd';
export interface MinimatchOptions {
    /** do not expand `{x,y}` style braces */
    nobrace?: boolean;
    /** do not treat patterns starting with `#` as a comment */
    nocomment?: boolean;
    /** do not treat patterns starting with `!` as a negation */
    nonegate?: boolean;
    /** treat `**` the same as `*` */
    noglobstar?: boolean;
    /** do not expand extglobs like `+(a|b)` */
    noext?: boolean;
    /** return the pattern if nothing matches */
    nonull?: boolean;
    /** treat `\\` as a path separator, not an escape character */
    windowsPathsNoEscape?: boolean;
    /**
     * inverse of {@link MinimatchOptions.windowsPathsNoEscape}
     * @deprecated
     */
    allowWindowsEscape?: boolean;
    /**
     * Compare a partial path to a pattern. As long as the parts
     * of the path that are present are not contradicted by the
     * pattern, it will be treated as a match. This is useful in
     * applications where you're walking through a folder structure,
     * and don't yet have the full path, but want to ensure that you
     * do not walk down paths that can never be a match.
     */
    partial?: boolean;
    /** allow matches that start with `.` even if the pattern does not */
    dot?: boolean;
    /** ignore case */
    nocase?: boolean;
    /** ignore case only in wildcard patterns */
    nocaseMagicOnly?: boolean;
    /**
     * Normalize unicode in the pattern and in each portion of the
     * tested paths, with this form. `true` means `'NFC'`.
     */
    normalize?: boolean | Form;
    /** consider braces to be "magic" for the purpose of `hasMagic` */
    magicalBraces?: boolean;
    /**
     * If set, then patterns without slashes will be matched
     * against the basename of the path if it contains slashes.
     * For example, `a?b` would match the path `/xyz/123/acb`, but
     * not `/xyz/acb/123`.
     */
    matchBase?: boolean;
    /** invert the results of negated matches */
    flipNegate?: boolean;
    /** do not collapse multiple `/` into a single `/` */
    preserveMultipleSlashes?: boolean;
    /**
     * A number indicating the level of optimization that should be done
     * to the pattern prior to parsing and using it for matches.
     * At level 2 or higher, throws RangeError if splitting `**` and `..`
     * would make more than `braceExpandMax` patterns, or add more than
     * 1,000,000 path portions.
     */
    optimizationLevel?: number;
    /** operating system platform */
    platform?: Platform;
    /**
     * When a pattern starts with a UNC path or drive letter, and in
     * `nocase:true` mode, do not convert the root portions of the
     * pattern into a case-insensitive regular expression, and instead
     * leave them as strings.
     *
     * This is the default when the platform is `win32` and
     * `nocase:true` is set.
     */
    windowsNoMagicRoot?: boolean;
    /**
     * max number of `{...}` patterns to expand. Default 100_000.
     *
     * At `optimizationLevel` 2 or higher, this also limits the
     * patterns made when `**` is followed by `..`. Over this limit,
     * construction throws a RangeError.
     */
    braceExpandMax?: number;
    /**
     * Max number of non-adjacent `**` patterns to recursively walk down.
     *
     * The default of 200 is almost certainly high enough for most purposes,
     * and can handle absurdly excessive patterns.
     */
    maxGlobstarRecursion?: number;
}
export declare const minimatch: {
    (p: string, pattern: string, options?: MinimatchOptions): boolean;
    sep: Sep;
    GLOBSTAR: typeof GLOBSTAR;
    filter: (pattern: string, options?: MinimatchOptions) => (p: string) => boolean;
    defaults: (def: MinimatchOptions) => typeof minimatch;
    braceExpand: (pattern: string, options?: MinimatchOptions) => string[];
    match: (list: string[], pattern: string, options?: MinimatchOptions) => string[];
    Minimatch: typeof Minimatch;
    escape: (s: string, { windowsPathsNoEscape, magicalBraces, }?: Pick<MinimatchOptions, "windowsPathsNoEscape" | "magicalBraces">) => string;
    unescape: (s: string, { windowsPathsNoEscape, magicalBraces, }?: Pick<MinimatchOptions, "windowsPathsNoEscape" | "magicalBraces">) => string;
};
export type Sep = '\\' | '/';
export declare const sep: Sep;
export declare const GLOBSTAR: unique symbol;
export declare const filter: (pattern: string, options?: MinimatchOptions) => (p: string) => boolean;
export declare const defaults: (def: MinimatchOptions) => typeof minimatch;
export declare const braceExpand: (pattern: string, options?: MinimatchOptions) => string[];
export declare const match: (list: string[], pattern: string, options?: MinimatchOptions) => string[];
/**
 * A path that Minimatch#match split, with the settings that the split
 * depends on. `resolved` is the parts after optimizationLevel 2 has
 * resolved `.` and `..` in them, which only depends on the same
 * settings, once a level 2 Minimatch has needed them.
 */
export type SplitPath = {
    f: string;
    isWindows: boolean;
    preserveMultipleSlashes: boolean;
    normalize: Form | undefined;
    parts: string[];
    resolved?: string[];
};
export type ParseReturnFiltered = string | Matcher | typeof GLOBSTAR;
/**
 * The rows that Minimatch#match can match without splitting the path:
 * `**` and one more portion (`tail`), a portion with `**` on each side
 * of it (`mid`), a portion and then `**` (`head`), or one portion that
 * matches the basename of the path, with matchBase (`base`).
 */
export type RowShape = 'tail' | 'mid' | 'head' | 'base';
/**
 * The separators that a path has: only `/`, only `\` (a Windows path
 * with no `/`), or both. Scanning for the ones it has is faster.
 */
export type PathSeps = '/' | '\\' | 'both';
/**
 * A pattern row split on its first and last `**`, from a start
 * index. Each body segment has the number of path portions that it
 * and the ones after it need, to find the last index where it can
 * start in a path.
 */
export type GlobstarParts = {
    head: ParseReturn[];
    body: ParseReturn[];
    tail: ParseReturn[];
    segments: [segment: ParseReturn[], need: number][];
};
export type ParseReturn = ParseReturnFiltered | false;
export declare class Minimatch {
    #private;
    options: MinimatchOptions;
    set: ParseReturnFiltered[][];
    pattern: string;
    windowsPathsNoEscape: boolean;
    nonegate: boolean;
    negate: boolean;
    comment: boolean;
    empty: boolean;
    preserveMultipleSlashes: boolean;
    partial: boolean;
    globSet: string[];
    globParts: string[][];
    nocase: boolean;
    isWindows: boolean;
    platform: Platform;
    windowsNoMagicRoot: boolean;
    maxGlobstarRecursion: number;
    constructor(pattern: string, options?: MinimatchOptions);
    hasMagic(): boolean;
    make(): void;
    mergeRows(): void;
    preprocess(globParts: string[][]): string[][];
    adjascentGlobstarOptimize(globParts: string[][]): string[][];
    levelOneOptimize(globParts: string[][]): string[][];
    firstPhasePreProcess(globParts: string[][]): string[][];
    secondPhasePreProcess(globParts: string[][]): string[][];
    parseNegate(): void;
    braceExpand(): string[];
    parse(pattern: string): ParseReturn;
    match(f: string, partial?: boolean): boolean;
    static defaults(def: MinimatchOptions): typeof Minimatch;
}
export { escape } from './escape.js';
export { unescape } from './unescape.js';
export type { Form };
//# sourceMappingURL=index.d.ts.map