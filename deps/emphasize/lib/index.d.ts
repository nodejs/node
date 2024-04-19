/// <reference types="highlight.js" />
/**
 * Create an `emphasize` instance.
 *
 * @param {Readonly<Record<string, LanguageFn>> | null | undefined} [grammars]
 *   Grammars to add (optional).
 * @returns
 *   Emphasize.
 */
export function createEmphasize(grammars?: Readonly<Record<string, LanguageFn>> | null | undefined): {
    highlight: (language: string, value: string, sheet?: Readonly<Sheet> | null | undefined) => Result;
    highlightAuto: (value: string, options?: Readonly<AutoOptions> | Readonly<Sheet> | null | undefined) => Result;
    listLanguages: () => string[];
    register: {
        (grammars: Readonly<Record<string, import("highlight.js").LanguageFn>>): undefined;
        (name: string, grammar: import("highlight.js").LanguageFn): undefined;
    };
    registerAlias: {
        (aliases: Readonly<Record<string, string | readonly string[]>>): undefined;
        (language: string, alias: string | readonly string[]): undefined;
    };
    registered: (aliasOrName: string) => boolean;
};
export type Element = import('hast').Element;
export type Root = import('hast').Root;
export type RootData = import('hast').RootData;
export type Text = import('hast').Text;
export type LowlightAutoOptions = import('lowlight').AutoOptions;
export type LanguageFn = import('lowlight').LanguageFn;
/**
 * Extra fields.
 */
export type AutoFieldsExtra = {
    /**
     * Sheet (optional).
     */
    sheet?: Sheet | null | undefined;
};
/**
 * Picked fields.
 */
export type AutoFieldsPicked = Pick<LowlightAutoOptions, 'subset'>;
/**
 * Configuration for `highlightAuto`.
 */
export type AutoOptions = AutoFieldsExtra & AutoFieldsPicked;
/**
 * Result.
 */
export type Result = {
    /**
     *   Detected programming language.
     */
    language: string | undefined;
    /**
     *   How sure `lowlight` is that the given code is in the language.
     */
    relevance: number | undefined;
    /**
     *   Highlighted code.
     */
    value: string;
};
/**
 * Map `highlight.js` classes to styles functions.
 *
 * The `hljs-` prefix must not be used in those classes.
 * The “descendant selector” (a space) is supported.
 *
 * For convenience [chalk’s chaining of styles][styles] is suggested.
 * An abbreviated example is as follows:
 *
 * ```js
 * {
 * 'comment': chalk.gray,
 * 'meta meta-string': chalk.cyan,
 * 'meta keyword': chalk.magenta,
 * 'emphasis': chalk.italic,
 * 'strong': chalk.bold,
 * 'formula': chalk.inverse
 * }
 * ```
 */
export type Sheet = Record<string, Style>;
/**
 * Color something.
 */
export type Style = (value: string) => string;
//# sourceMappingURL=index.d.ts.map