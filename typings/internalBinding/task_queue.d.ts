export interface TaskQueueBinding {
  enqueueMicrotask(callback: () => void): void;
  setTickCallback(callback: () => void): void;
  runMicrotasks(): void;
  tickInfo: Uint8Array;
  promiseRejectEvents: {
    kPromiseRejectWithNoHandler: number;
    kPromiseHandlerAddedAfterReject: number;
  };
  setPromiseRejectCallback(
    callback: (type: number, promise: Promise<unknown>, value: unknown) => void,
  ): void;
}
