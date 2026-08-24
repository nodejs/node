declare namespace InternalWatchdogBinding {
  class TraceSigintWatchdog {
    constructor();
    start(): void;
    stop(): void;
    close(callback?: () => void): void;
    hasRef(): boolean;
    ref(): void;
    unref(): void;
  }
}

export interface WatchdogBinding {
  TraceSigintWatchdog: typeof InternalWatchdogBinding.TraceSigintWatchdog;
}
