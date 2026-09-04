declare namespace InternalPerformanceBinding {
  interface HistogramBase {
    count(): number;
    countBigInt(): bigint;
    min(): number;
    minBigInt(): bigint;
    max(): number;
    maxBigInt(): bigint;
    mean(): number;
    exceeds(): number;
    exceedsBigInt(): bigint;
    stddev(): number;
    percentile(percentile: number): number;
    percentileBigInt(percentile: number): bigint;
    percentiles(percentiles: Map<number, number>): void;
    percentilesBigInt(percentiles: Map<number, bigint>): void;
    reset(): void;
    skewness(): number;
    kurtosis(): number;
    cdf(value: number): number;
    countAt(value: number): number;
    ksTest(other: HistogramBase): number;
    percentilesAt(
      percentiles: Map<number, number>,
      values: Float64Array,
    ): void;
    linearBuckets(stepSize: number, buckets: Map<number, number>): void;
    logBuckets(
      firstBucket: number,
      base: number,
      buckets: Map<number, number>,
    ): void;
    welchTest(
      other: HistogramBase,
      confidence: number,
    ): [number, number, number, number, number];
    mannWhitneyTest(other: HistogramBase): [number, number, number];
    cohensD(other: HistogramBase): number;
    cliffsD(other: HistogramBase): number;
    percentileCI(percentile: number, confidence: number): [number, number, number];
    ewmaMean(): number;
    ewmaStddev(): number;
    ewmaErrorRate(): number;
  }

  interface ELDHistogram extends HistogramBase {
    start(reset?: boolean): void;
    stop(): void;
    close(callback?: () => void): void;
    hasRef(): boolean;
    ref(): void;
    unref(): void;
    getAsyncId(): number;
    asyncReset(resource: object, executionAsyncId?: number): void;
    getAsyncContextFrameForDebuggingOnly(): unknown;
    getProviderType(): number;
  }

  interface Histogram extends HistogramBase {}

  class Histogram {
    constructor(
      lowest: number | bigint,
      highest: number | bigint,
      figures: number,
      halfLife?: number,
      threshold?: number,
    );
    record(value: number | bigint): void;
    recordDelta(): void;
    recordCorrected(
      value: number | bigint,
      expectedInterval: number | bigint,
    ): void;
    add(other: Histogram): number;
    subtract(other: Histogram): number;
  }

  interface Constants {
    NODE_PERFORMANCE_GC_MAJOR: number;
    NODE_PERFORMANCE_GC_MINOR: number;
    NODE_PERFORMANCE_GC_MINOR_MARK_SWEEP: number;
    NODE_PERFORMANCE_GC_INCREMENTAL: number;
    NODE_PERFORMANCE_GC_WEAKCB: number;
    NODE_PERFORMANCE_GC_FLAGS_NO: number;
    NODE_PERFORMANCE_GC_FLAGS_CONSTRUCT_RETAINED: number;
    NODE_PERFORMANCE_GC_FLAGS_FORCED: number;
    NODE_PERFORMANCE_GC_FLAGS_SYNCHRONOUS_PHANTOM_PROCESSING: number;
    NODE_PERFORMANCE_GC_FLAGS_ALL_AVAILABLE_GARBAGE: number;
    NODE_PERFORMANCE_GC_FLAGS_ALL_EXTERNAL_MEMORY: number;
    NODE_PERFORMANCE_GC_FLAGS_SCHEDULE_IDLE: number;
    NODE_PERFORMANCE_ENTRY_TYPE_GC: number;
    NODE_PERFORMANCE_ENTRY_TYPE_HTTP: number;
    NODE_PERFORMANCE_ENTRY_TYPE_HTTP2: number;
    NODE_PERFORMANCE_ENTRY_TYPE_NET: number;
    NODE_PERFORMANCE_ENTRY_TYPE_DNS: number;
    NODE_PERFORMANCE_ENTRY_TYPE_QUIC: number;
    NODE_PERFORMANCE_MILESTONE_TIME_ORIGIN_TIMESTAMP: number;
    NODE_PERFORMANCE_MILESTONE_TIME_ORIGIN: number;
    NODE_PERFORMANCE_MILESTONE_ENVIRONMENT: number;
    NODE_PERFORMANCE_MILESTONE_NODE_START: number;
    NODE_PERFORMANCE_MILESTONE_V8_START: number;
    NODE_PERFORMANCE_MILESTONE_LOOP_START: number;
    NODE_PERFORMANCE_MILESTONE_LOOP_EXIT: number;
    NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE: number;
  }
}

type PerformanceObserverCallback =
  | ((name: string,
      type: string,
      startTime: number,
      duration: number,
      details: unknown) => void)
  | ((entry: unknown) => void);

export interface PerformanceBinding {
  Histogram: typeof InternalPerformanceBinding.Histogram;
  constants: InternalPerformanceBinding.Constants;
  observerCounts: Uint32Array;
  milestones: Float64Array;
  setupObservers(callback: PerformanceObserverCallback): void;
  installGarbageCollectionTracking(): void;
  removeGarbageCollectionTracking(): void;
  notify(type: string, entry: unknown): void;
  loopIdleTime(): number;
  createELDHistogram(
    interval: number,
    samplePerIteration: boolean,
  ): InternalPerformanceBinding.ELDHistogram;
  markBootstrapComplete(): void;
  uvMetricsInfo(): [number, number, number];
  now(): number;
}
