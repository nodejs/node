/* tslint:disable */
/* eslint-disable */

export declare function transform(src: string | Uint8Array, opts?: Options): Promise<TransformOutput>;
export declare function transformSync(src: string | Uint8Array, opts?: Options): TransformOutput;
export type { Options, TransformOutput };



interface Options {
    module?: boolean;
    filename?: string;
    mode?: Mode;
    transform?: TransformConfig;
    deprecatedTsModuleAsError?: boolean;
    sourceMap?: boolean;
}

interface TransformConfig {
    /**
     * @see https://www.typescriptlang.org/tsconfig#verbatimModuleSyntax
     */
    verbatimModuleSyntax?: boolean;
    /**
     * Native class properties support
     */
    nativeClassProperties?: boolean;
    importNotUsedAsValues?: "remove" | "preserve";
    /**
     * Don't create `export {}`.
     * By default, strip creates `export {}` for modules to preserve module
     * context.
     * 
     * @see https://github.com/swc-project/swc/issues/1698
     */
    noEmptyExport?: boolean;
    importExportAssignConfig?: "Classic" | "Preserve" | "NodeNext" | "EsNext";
    /**
     * Disables an optimization that inlines TS enum member values
     * within the same module that assumes the enum member values
     * are never modified.
     * 
     * Defaults to false.
     */
    tsEnumIsMutable?: boolean;
}



type Mode = "strip-only" | "transform";



interface TransformOutput {
    code: string;
    map?: string;
}


