declare namespace InternalHttpParserBinding {
  type Buffer = Uint8Array;
  type Stream = object;

  class ConnectionsList {
    constructor();

    all(): HTTPParser[];
    idle(): HTTPParser[];
    active(): HTTPParser[];
    expired(): HTTPParser[];
  }

  class HTTPParser {
    static REQUEST: 1;
    static RESPONSE: 2;

    static kOnMessageBegin: 0;
    static kOnHeaders: 1;
    static kOnHeadersComplete: 2;
    static kOnBody: 3;
    static kOnMessageComplete: 4;
    static kOnExecute: 5;
    static kOnTimeout: 6;

    static kLenientNone: number;
    static kLenientHeaders: number;
    static kLenientChunkedLength: number;
    static kLenientKeepAlive: number;
    static kLenientTransferEncoding: number;
    static kLenientVersion: number;
    static kLenientDataAfterClose: number;
    static kLenientOptionalLFAfterCR: number;
    static kLenientOptionalCRLFAfterChunk: number;
    static kLenientOptionalCRBeforeLF: number;
    static kLenientSpacesAfterChunkSize: number;
    static kLenientAll: number;

    static kNativeHeadersMagic: number;
    static kNativeHeaderFlagHost: number;
    static kNativeHeaderFlagExpect: number;
    static kNativeHeaderFlagContentLength: number;
    static kNativeHeaderFlagTransferEncoding: number;
    static kNativeHeaderFlagTE: number;
    static kNativeHeadersCountOffset: number;
    static kNativeHeadersFlagsOffset: number;
    static kNativeHeadersPrefix: number;

    close(): void;
    free(): void;
    remove(): void;
    execute(buffer: Buffer): Error | Buffer;
    finish(): Error | Buffer;
    initialize(
      type: number,
      resource: object,
      maxHeaderSize?: number,
      lenient?: number,
      connectionsList?: object,
      maxHeaderPairs?: number,
    ): void;
    pause(): void;
    resume(): void;
    consume(stream: Stream): void;
    unconsume(): void;
    getCurrentBuffer(): Buffer;
    setSkipBody(skip?: boolean): void;
  }
}

export interface HttpParserBinding {
  ConnectionsList: typeof InternalHttpParserBinding.ConnectionsList;
  HTTPParser: typeof InternalHttpParserBinding.HTTPParser;
  allMethods: string[];
  methods: string[];
  nativeHeadersHas(headers: Buffer, name: string): boolean;
  nativeHeadersGet(headers: Buffer, name: string): string | undefined;
  nativeHeadersToArray(headers: Buffer, maxElements?: number): string[];
  nativeHeadersByteLength(headers: Buffer): number;
}
