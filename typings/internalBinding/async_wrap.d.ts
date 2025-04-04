import { owner_symbol } from './symbols';

declare namespace InternalAsyncWrapBinding {
  interface Resource {
    [owner_symbol]?: PublicResource;
  }
  type PublicResource = object;

  type EmitHook = (asyncId: number) => void;

  type PromiseHook = (promise: Promise<unknown>, parent: Promise<unknown>) => void;

  interface Providers {
    NONE: 0;
    DIRHANDLE: 1;
    DNSCHANNEL: 2;
    ELDHISTOGRAM: 3;
    FILEHANDLE: 4;
    FILEHANDLECLOSEREQ: 5;
    FIXEDSIZEBLOBCOPY: 6;
    FSEVENTWRAP: 7;
    FSREQCALLBACK: 8;
    FSREQPROMISE: 9;
    GETADDRINFOREQWRAP: 10;
    GETNAMEINFOREQWRAP: 11;
    HEAPSNAPSHOT: 12;
    HTTP2SESSION: 13;
    HTTP2STREAM: 14;
    HTTP2PING: 15;
    HTTP2SETTINGS: 16;
    JSSTREAM: 17;
    JSUDPWRAP: 18;
    MESSAGEPORT: 19;
    PIPECONNECTWRAP: 20;
    PIPESERVERWRAP: 21;
    PIPEWRAP: 22;
    PROCESSWRAP: 23;
    PROMISE: 24;
    QUERYWRAP: 25;
    SHUTDOWNWRAP: 26;
    SIGNALWRAP: 27;
    STATWATCHER: 28;
    STREAMPIPE: 29;
    TCPCONNECTWRAP: 30;
    TCPSERVERWRAP: 31;
    TCPWRAP: 32;
    TTYWRAP: 33;
    UDPSENDWRAP: 34;
    UDPWRAP: 35;
    SIGINTWATCHDOG: 36;
    WORKER: 37;
    WORKERHEAPSNAPSHOT: 38;
    WRITEWRAP: 39;
    ZLIB: 40;
    CHECKPRIMEREQUEST: 41;
    PBKDF2REQUEST: 42;
    KEYPAIRGENREQUEST: 43;
    KEYGENREQUEST: 44;
    KEYEXPORTREQUEST: 45;
    CIPHERREQUEST: 46;
    DERIVEBITSREQUEST: 47;
    HASHREQUEST: 48;
    RANDOMBYTESREQUEST: 49;
    RANDOMPRIMEREQUEST: 50;
    SCRYPTREQUEST: 51;
    SIGNREQUEST: 52;
    TLSWRAP: 53;
    VERIFYREQUEST: 54;
  }
}

export interface AsyncWrapBinding {
  setupHooks(): {
    init: (
      asyncId: number,
      type: keyof InternalAsyncWrapBinding.Providers,
      triggerAsyncId: number,
      resource: InternalAsyncWrapBinding.Resource,
    ) => void;
    before: InternalAsyncWrapBinding.EmitHook;
    after: InternalAsyncWrapBinding.EmitHook;
    destroy: InternalAsyncWrapBinding.EmitHook;
    promise_resolve: InternalAsyncWrapBinding.EmitHook;
  };
  setCallbackTrampoline(
    callback: (
      asyncId: number,
      resource: InternalAsyncWrapBinding.Resource,
      cb: (
        cb: (...args: any[]) => boolean | undefined,
        ...args: any[]
      ) => boolean | undefined,
      ...args: any[]
    ) => boolean | undefined
  ): void;
  pushAsyncContext(asyncId: number, triggerAsyncId: number): void;
  popAsyncContext(asyncId: number): boolean;
  executionAsyncResource(index: number): InternalAsyncWrapBinding.PublicResource | null;
  clearAsyncIdStack(): void;
  queueDestroyAsyncId(asyncId: number): void;
  setPromiseHooks(
    initHook: ((promise: Promise<unknown>, parent?: Promise<unknown>) => void) | undefined,
    promiseBeforeHook: InternalAsyncWrapBinding.PromiseHook | undefined,
    promiseAfterHook: InternalAsyncWrapBinding.PromiseHook | undefined,
    promiseResolveHook: InternalAsyncWrapBinding.PromiseHook | undefined
  ): void;
  registerDestroyHook(resource: object, asyncId: number, destroyed?: { destroyed: boolean }): void;
  async_hook_fields: Uint32Array;
  async_id_fields: Float64Array;
  async_ids_stack: Float64Array;
  execution_async_resources: InternalAsyncWrapBinding.Resource[];
  constants: {
    kInit: 0;
    kBefore: 1;
    kAfter: 2;
    kDestroy: 3;
    kPromiseResolve: 4;
    kTotals: 5;
    kCheck: 6;
    kStackLength: 7;
    kUsesExecutionAsyncResource: 8;

    kExecutionAsyncId: 0;
    kTriggerAsyncId: 1;
    kAsyncIdCounter: 2;
    kDefaultTriggerAsyncId: 3;
  };
  Providers: InternalAsyncWrapBinding.Providers;
}
