import type { HandleWrap } from './handle_wrap';
import { owner_symbol } from './symbols';

declare namespace InternalFsEventWrapBinding {
  class FSEvent {
    constructor();
    [owner_symbol]?: object;
    readonly initialized: boolean;
    onchange?: (
      status: number,
      eventType: string,
      filename: string | Uint8Array | null,
    ) => void;
    start(
      filename: string | Uint8Array,
      persistent: boolean,
      recursive: boolean,
      encoding: string,
    ): number;
  }

  interface FSEvent extends HandleWrap {}
}

export interface FsEventWrapBinding {
  FSEvent: typeof InternalFsEventWrapBinding.FSEvent;
}
