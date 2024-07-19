export interface ContextifyBinding {
  constants: {
    measureMemory: Record<string, unknown>;
  }
  measureMemory(mode: string, execution: string): number | undefined;
  makeContext(contextObject: unknown, name: string, origin: string, strings: boolean, wasm: boolean, microtaskQueue: boolean, hostDefinedOptionId: string): void;
  ContextifyContext: new (code: string, filename?: string, lineOffset?: number, columnOffset?: number, cachedData?: string, produceCachedData?: boolean, parsingContext?: unknown, hostDefinedOptionId?: string) => unknown;
}
