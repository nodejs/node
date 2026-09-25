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
}

export interface StreamPipeBinding {
  StreamPipe: typeof InternalStreamPipeBinding.StreamPipe;
}
