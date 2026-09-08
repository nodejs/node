export interface TaskQueueBinding {
  enqueueMicrotask(callback: () => void): void;
  setTickCallback(callback: () => void): void;
  runMicrotasks(): void;
  tickInfo: Uint8Array;
  promiseRejectEvents: {
    kPromiseRejectWithNoHandler: 0;
    kPromiseHandlerAddedAfterReject: 1;
  };
  setPromiseRejectCallback(
    callback: (type: 0 | 1, promise: Promise<unknown>, value: unknown) => void,
  ): void;
}