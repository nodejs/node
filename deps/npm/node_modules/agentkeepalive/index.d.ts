declare module "agentkeepalive" {
  import * as http from 'http';
  import * as https from 'https';

  interface AgentStatus {
    createSocketCount: number,
    createSocketErrorCount: number,
    closeSocketCount: number,
    errorSocketCount: number,
    timeoutSocketCount: number,
    requestCount: number,
    freeSockets: object,
    sockets: object,
    requests: object,
  }

  interface HttpOptions extends http.AgentOptions {
    freeSocketKeepAliveTimeout?: number;
    timeout?: number;
    socketActiveTTL?: number;
  }

  interface HttpsOptions extends https.AgentOptions {
    freeSocketKeepAliveTimeout?: number;
    timeout?: number;
    socketActiveTTL?: number;
  }

  class internal extends http.Agent {
    constructor(opts?: HttpOptions);
    readonly statusChanged: boolean;
    createSocket(req: http.IncomingMessage, options: http.RequestOptions, cb: Function): void;
    getCurrentStatus(): AgentStatus;
  }

  namespace internal {
    export class HttpsAgent extends internal {
      constructor(opts?: HttpsOptions);
    }
  }

  export = internal;
}
