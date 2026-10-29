export interface ProfilerBinding {
  setCoverageDirectory(directory: string): void;
  setSourceMapCacheGetter(getter: () => object | undefined): void;
  startCoverage(): void;
  takeCoverage(): void;
  stopCoverage(): void;
  endCoverage(): void;
}
