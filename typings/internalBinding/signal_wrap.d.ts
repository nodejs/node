declare namespace InternalSignalWrapBinding {
  class Signal {
    constructor();
    onsignal?: (signum: number) => void;
    start(signum: number): number | undefined;
    stop(): number;
    close(callback?: () => void): void;
    hasRef(): boolean;
    ref(): void;
    unref(): void;
  }
}

export interface SignalWrapBinding {
  Signal: typeof InternalSignalWrapBinding.Signal;
}
