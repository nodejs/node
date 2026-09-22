import type { HandleWrap } from './handle_wrap';

declare namespace InternalWatchdogBinding {
  class TraceSigintWatchdog {
    constructor();
    start(): void;
    stop(): void;
  }

  interface TraceSigintWatchdog extends HandleWrap {}
}

export interface WatchdogBinding {
  TraceSigintWatchdog: typeof InternalWatchdogBinding.TraceSigintWatchdog;
}
