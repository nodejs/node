/**
 * Types for the JSON files in this package.
 *
 * All paths are relative to the package root.
 */

/** The format of `suites.json`. */
export interface SuitesIndex {
  suites: SuiteInfo[];
}

export type SuiteInfo =
  | { kind: "interpreting"; file: string }
  | { kind: "decoding"; file: string };

/** A file from the test suite, with its contents inlined. */
export interface TestFile {
  path: string;
  content: string;
}

/** The format of interpreting suite files (`interpreting/<name>.json`). */
export interface InterpretingSuite {
  tests: InterpretingTest[];
}

export interface InterpretingTest {
  name: string;
  description: string;
  sourceMapIsValid: boolean;
  testActions: TestAction[];
  /**
   * The transpiled source that the source map refers to.
   */
  baseFile: TestFile;
  sourceMapFile: TestFile;
}

export type TestAction =
  | CheckMappingAction
  | CheckMappingTransitiveAction
  | CheckIgnoreListAction;

export interface CheckMappingAction {
  actionType: "checkMapping";
  /** 0-based. */
  generatedLine: number;
  /** 0-based. */
  generatedColumn: number;
  originalSource: string | null;
  /** 0-based. */
  originalLine: number | null;
  /** 0-based. */
  originalColumn: number | null;
  mappedName: string | null;
}

export interface CheckMappingTransitiveAction
  extends Omit<CheckMappingAction, "actionType"> {
  actionType: "checkMappingTransitive";
  /** Source maps to apply after `sourceMapFile`, in order. */
  intermediateMaps: TestFile[];
}

export interface CheckIgnoreListAction {
  actionType: "checkIgnoreList";
  /** Sources that must be in the ignore list. */
  present: string[];
}

/** The format of decoding suite files (`decoding/<name>.json`). */
export interface DecodingSuite {
  tests: DecodingTest[];
}

export interface DecodingTest {
  name: string;
  sourceMapFile: TestFile;
  /**
   * The expected Decoded Source Map Record, as JSON in the format described by
   * `decoding/source-map-record.d.ts` (and extended by the proposal-specific
   * `decoding/<suite>/<suite>.d.ts` files).
   */
  goldenFile: TestFile;
  /**
   * Whether decoding this source map should optionally report an error
   * (https://tc39.es/ecma426/#sec-optional-errors).
   */
  optionalErrors: boolean;
}
