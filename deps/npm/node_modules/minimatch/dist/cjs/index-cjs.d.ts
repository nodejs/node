declare const _default: {
    (p: string, pattern: string, options?: import("./index.js").MinimatchOptions): boolean;
    sep: string;
    GLOBSTAR: typeof import("./index.js").GLOBSTAR;
    filter: (pattern: string, options?: import("./index.js").MinimatchOptions) => (p: string) => boolean;
    defaults: (def: import("./index.js").MinimatchOptions) => any;
    braceExpand: (pattern: string, options?: import("./index.js").MinimatchOptions) => string[];
    makeRe: (pattern: string, options?: import("./index.js").MinimatchOptions) => false | import("./index.js").MMRegExp;
    match: (list: string[], pattern: string, options?: import("./index.js").MinimatchOptions) => string[];
    Minimatch: typeof import("./index.js").Minimatch;
} & {
    default: {
        (p: string, pattern: string, options?: import("./index.js").MinimatchOptions): boolean;
        sep: string;
        GLOBSTAR: typeof import("./index.js").GLOBSTAR;
        filter: (pattern: string, options?: import("./index.js").MinimatchOptions) => (p: string) => boolean;
        defaults: (def: import("./index.js").MinimatchOptions) => any;
        braceExpand: (pattern: string, options?: import("./index.js").MinimatchOptions) => string[];
        makeRe: (pattern: string, options?: import("./index.js").MinimatchOptions) => false | import("./index.js").MMRegExp;
        match: (list: string[], pattern: string, options?: import("./index.js").MinimatchOptions) => string[];
        Minimatch: typeof import("./index.js").Minimatch;
    };
    minimatch: {
        (p: string, pattern: string, options?: import("./index.js").MinimatchOptions): boolean;
        sep: string;
        GLOBSTAR: typeof import("./index.js").GLOBSTAR;
        filter: (pattern: string, options?: import("./index.js").MinimatchOptions) => (p: string) => boolean;
        defaults: (def: import("./index.js").MinimatchOptions) => any;
        braceExpand: (pattern: string, options?: import("./index.js").MinimatchOptions) => string[];
        makeRe: (pattern: string, options?: import("./index.js").MinimatchOptions) => false | import("./index.js").MMRegExp;
        match: (list: string[], pattern: string, options?: import("./index.js").MinimatchOptions) => string[];
        Minimatch: typeof import("./index.js").Minimatch;
    };
};
export = _default;
