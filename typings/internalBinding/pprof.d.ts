declare namespace InternalPprofBinding {
  // A time profile, as a table of the nodes of the profile tree in pre-order,
  // root first. Every node comes after its parent.
  interface TimeProfile {
    // V8 timer values, in microseconds.
    startTime: number;
    endTime: number;
    // Microseconds to add to a V8 timer value to get microseconds since the
    // Unix epoch. Undefined on Windows.
    epochOffset: number | undefined;
    hasCpuTime: boolean;
    nonJSThreadsCpuTime: number;
    totalHitCount: number;
    restarted: boolean;
    parent: Int32Array;
    scriptId: Int32Array;
    lineNumber: Int32Array;
    columnNumber: Int32Array;
    hitCount: Uint32Array;
    // Nanoseconds. Only set if hasCpuTime is true.
    cpuTime: Float64Array | undefined;
    name: string[];
    scriptName: string[];
  }

  class WallProfiler {
    constructor(
      intervalMicros: number,
      durationMillis: number,
      lineNumbers: boolean,
      workaroundV8Bug: boolean,
      collectCpuTime: boolean,
    );
    start(): void;
    stop(restart: boolean): TimeProfile;
    dispose(): void;
    v8ProfilerStuckEventLoopDetected(): 0 | 1 | 2;
  }
}

export interface PprofBinding {
  WallProfiler: typeof InternalPprofBinding.WallProfiler;
  signalSamplingSupported: boolean;
}
