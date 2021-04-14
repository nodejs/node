declare namespace InternalHttpParserBinding {
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
    static kLenientAll: number;

    close(): void;
    free(): void;
    execute(buffer: Buffer): Error | Buffer;
    finish(): Error | Buffer;
    initialize(
      type: number,
      resource: object,
      maxHeaderSize?: number,
      lenient?: number,
      headersTimeout?: number,
    ): void;
    pause(): void;
    resume(): void;
    consume(stream: object): void;
    unconsume(): void;
    getCurrentBuffer(): Buffer;
  }
}

declare function InternalBinding(binding: 'http_parser'): {
  methods: string[];
  HTTPParser: typeof InternalHttpParserBinding.HTTPParser;
};
