declare const convertPathToPattern: (path: string) => string;
declare const escapePath: (path: string) => string;
declare function isDynamicPattern(pattern: string, options?: {
    caseSensitiveMatch: boolean;
}): boolean;

interface GlobOptions {
    absolute?: boolean;
    cwd?: string;
    patterns?: string | string[];
    ignore?: string | string[];
    dot?: boolean;
    deep?: number;
    followSymbolicLinks?: boolean;
    caseSensitiveMatch?: boolean;
    expandDirectories?: boolean;
    onlyDirectories?: boolean;
    onlyFiles?: boolean;
    debug?: boolean;
}
declare function glob(patterns: string | string[], options?: Omit<GlobOptions, 'patterns'>): Promise<string[]>;
declare function glob(options: GlobOptions): Promise<string[]>;
declare function globSync(patterns: string | string[], options?: Omit<GlobOptions, 'patterns'>): string[];
declare function globSync(options: GlobOptions): string[];

export { type GlobOptions, convertPathToPattern, escapePath, glob, globSync, isDynamicPattern };
