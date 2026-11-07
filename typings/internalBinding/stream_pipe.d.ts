import type { AsyncWrap } from './async_wrap';

declare namespace InternalStreamPipeBinding {
  class StreamPipe {
    constructor(source: object, sink: object);
    readonly source: object | null;
    readonly sink: object | null;
    onunpipe?: () => void;
    oncomplete?: () => void;
    unpipe(): void;
    start(): void;
    isClosed(): boolean;
    pendingWrites(): number;
  }

  interface StreamPipe extends AsyncWrap {}
}

export interface StreamPipeBinding {
  StreamPipe: typeof InternalStreamPipeBinding.StreamPipe;
}
