import type { HandleWrap } from './handle_wrap';

declare namespace InternalSignalWrapBinding {
  class Signal {
    constructor();
    onsignal?: (signum: number) => void;
    start(signum: number): number | undefined;
    stop(): number;
  }

  interface Signal extends HandleWrap {}
}

export interface SignalWrapBinding {
  Signal: typeof InternalSignalWrapBinding.Signal;
}
