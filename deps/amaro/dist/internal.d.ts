import type { ModuleSyntaxTransformOutput, Options, TransformOutput } from "../lib/wasm";
export declare function transformSync(source: string, options?: Options): TransformOutput;
export declare function transformModuleSyntax(source: string): ModuleSyntaxTransformOutput;
export declare function getFirstExpression(source: string, startColumn: number): string;
export declare function isValidSyntax(source: string): boolean;
export declare function isRecoverableError(source: string): boolean;
