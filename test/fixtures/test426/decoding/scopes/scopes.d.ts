
/// <reference path="../source-map-record.d.ts" />

interface SourceMapRecord {
  ranges: GeneratedRangeRecord[];
}

interface SourceRecord {
  scope: OriginalScopeRecord|null;
}

/**
 * Corresponds to a [Original Scope Record](https://tc39.es/ecma426/branch/proposal-scopes/#sec-original-scope-record-type)
 */
interface OriginalScopeRecord {
  start: PositionRecord;
  end: PositionRecord;
  name: string|null;
  kind: string|null;
  isStackFrame: boolean;
  variables: string[];
  children: OriginalScopeRecord[];
}

/**
 * Corresponds to a [Generated Range Record](https://tc39.es/ecma426/branch/proposal-scopes/#sec-generated-range-record-type)
 */
interface GeneratedRangeRecord {
  start: PositionRecord;
  end: PositionRecord;
  /**
   * An index into a flattened list of {@link OriginalScopeRecord}. The flattened list is obtained by iterating
   * {@link SourceMapRecord.sources} from `0` to `SourceMapRecord.sources.length` and traverse each {@link SourceRecord.scope} in [pre-order](https://en.wikipedia.org/wiki/Tree_traversal#Pre-order,_NLR).
   */
  definitionIndex: number|null;
  stackFrameType: 'none'|'original'|'hidden';
  bindings: BindingRecord[][];
  callSite: OriginalPositionRecord|null;
  children: GeneratedRangeRecord[];
}

/**
 * Corresponds to a [Binding Record](https://tc39.es/ecma426/branch/proposal-scopes/#sec-binding-record-type)
 */
interface BindingRecord {
  from: PositionRecord;
  binding: string|null;
}
