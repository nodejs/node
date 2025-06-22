interface InspectorConnectionInstance {
  dispatch(message: string): void;
  disconnect(): void;
}

interface InspectorConnectionConstructor {
  new(onMessageHandler: (message: string) => void): InspectorConnectionInstance;
}

export interface InspectorBinding {
  consoleCall(
    inspectorMethod: (...args: any[]) => any,
    nodeMethod: (...args: any[]) => any,
    ...args: any[]
  ): void;
  setConsoleExtensionInstaller(installer: Function): void;
  callAndPauseOnStart(
    fn: (...args: any[]) => any,
    thisArg: any,
    ...args: any[]
  ): any;
  open(port: number, host: string): void;
  url(): string | undefined;
  waitForDebugger(): boolean;
  asyncTaskScheduled(taskName: string, taskId: number, recurring: boolean): void;
  asyncTaskCanceled(taskId: number): void;
  asyncTaskStarted(taskId: number): void;
  asyncTaskFinished(taskId: number): void;
  registerAsyncHook(enable: () => void, disable: () => void): void;
  isEnabled(): boolean;
  emitProtocolEvent(eventName: string, params: object): void;
  setupNetworkTracking(enable: () => void, disable: () => void): void;
  console: Console;
  Connection: InspectorConnectionConstructor;
  MainThreadConnection: InspectorConnectionConstructor;
}
