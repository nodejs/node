import type { ModuleSyntaxTransformOutput, Options, Token, TokenKind, TopLevelAwaitLocation, TransformOutput } from "../lib/wasm";
export type { Token, TokenKind, TopLevelAwaitLocation };
export declare function transformSync(source: string, options?: Options): TransformOutput;
export declare function transformModuleSyntax(source: string): ModuleSyntaxTransformOutput;
export declare function getFirstExpression(source: string, startColumn: number): string;
export declare function isValidSyntax(source: string): boolean;
export declare function isRecoverableError(source: string): boolean;
export declare function tokenize(source: string): Token[];
export declare function findTopLevelAwaits(source: string): TopLevelAwaitLocation[];
