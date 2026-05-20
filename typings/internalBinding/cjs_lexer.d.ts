export type CJSLexerParseResult = [exports: Set<string>, reexports: string[]];

export interface CJSLexerBinding {
  parse(source?: string | null): CJSLexerParseResult;
}
