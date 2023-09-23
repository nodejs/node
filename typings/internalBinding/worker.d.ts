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
    ref(): void;
    unref(): void;
    getResourceLimits(): Float64Array;
    takeHeapSnapshot(): object;
    loopIdleTime(): number;
    loopStartTime(): number;
  }
}

export interface WorkerBinding {
  Worker: typeof InternalWorkerBinding.Worker;
  getEnvMessagePort(): InternalMessagingBinding.MessagePort;
  threadId: number;
  isMainThread: boolean;
  ownsProcessState: boolean;
  resourceLimits?: Float64Array;
  kMaxYoungGenerationSizeMb: number;
  kMaxOldGenerationSizeMb: number;
  kCodeRangeSizeMb: number;
  kStackSizeMb: number;
  kTotalResourceLimitCount: number;
}
