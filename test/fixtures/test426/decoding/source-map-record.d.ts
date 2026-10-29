/**
 * The global type of a .golden JSON.
 */
interface SourceMapRecord {
  file: string|null;
  sources: SourceRecord[];
  mappings: MappingRecord[];
}

/**
 * Corresponds to a [Decoded Source Record](https://tc39.es/ecma426/#decoded-source-record)
 */
interface SourceRecord {
  url: string|null;
  content: string|null;
  ignored: boolean;
}

/**
 * Corresponds to a [Decoded Mapping Record](https://tc39.es/ecma426/#decoded-mapping-record)
 */
interface MappingRecord {
  generatedPosition: PositionRecord;
  originalPosition: OriginalPositionRecord|null;
  name: string|null;
}

/**
 * Corresponds to a [Position Record](https://tc39.es/ecma426/#sec-position-record-type)
 */
interface PositionRecord {
  line: number;
  column: number;
}

/**
 * Corresponds to a [Original Position Record](https://tc39.es/ecma426/#sec-original-position-record-type)
 */
interface OriginalPositionRecord {
  /** An index into {@link SourceMapRecord.sources}. */
  sourceIndex: number;
  line: number;
  column: number;
}
