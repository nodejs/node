import { InternalMessagingBinding } from './messaging';

declare namespace InternalWorkerBinding {
  class Worker {
    constructor(
      url: string | URL | null,
      env: object | null | undefined,
      execArgv: string[] | null | undefined,
      resourceLimits: Float64Array,
      trackUnmanagedFds: boolean,
    );
    startThread(): void;
    stopThread(): void;
    hasRef(): boolean;
    ref(): void;
    unref(): void;
    getResourceLimits(): Float64Array;
    takeHeapSnapshot(): object;
    getHeapStatistics(): Promise<object>;
    cpuUsage(): Promise<object>;
    startCpuProfile(): Promise<CPUProfileHandle>;
    startHeapProfile(): Promise<HeapProfileHandle>;
    loopIdleTime(): number;
    loopStartTime(): number;
  }
}

export interface CPUProfileHandle {
  stop(): Promise<string>;
  [Symbol.asyncDispose](): Promise<void>;
}

export interface HeapProfileHandle {
  stop(): Promise<string>;
  [Symbol.asyncDispose](): Promise<void>;
}

export interface WorkerBinding {
  Worker: typeof InternalWorkerBinding.Worker;
  getEnvMessagePort(): InternalMessagingBinding.MessagePort;
  threadId: number;
  threadName: string;
  isMainThread: boolean;
  isInternalThread: boolean;
  ownsProcessState: boolean;
  resourceLimits?: Float64Array;
  kMaxYoungGenerationSizeMb: number;
  kMaxOldGenerationSizeMb: number;
  kCodeRangeSizeMb: number;
  kStackSizeMb: number;
  kTotalResourceLimitCount: number;
}
