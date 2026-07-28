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
      headersTimeout?: number,
    ): void;
    pause(): void;
    resume(): void;
    consume(stream: Stream): void;
    unconsume(): void;
    getCurrentBuffer(): Buffer;
  }

  /**
   * Body encoding for `buildHttpMessage`:
   * - 0 = none / Buffer body
   * - 1 = latin1 string body
   * - 2 = utf8 string body
   */
  type BuildHttpMessageBodyEncoding = 0 | 1 | 2;

  /**
   * Bit flags for `buildHttpMessage` (keep in sync with
   * `BuildHttpMessageFlags` in `src/node_http_parser.cc` and the
   * `kBuild*` constants in `lib/_http_outgoing.js`).
   *
   * | bit | name |
   * |-----|------|
   * | 0 | sendDate |
   * | 1 | shouldKeepAlive |
   * | 2 | maxRequestsReached |
   * | 3 | defaultKeepAlive |
   * | 4 | hasBody |
   * | 5 | useChunkedByDefault |
   * | 6 | removedConnection |
   * | 7 | removedContLen |
   * | 8 | removedTE |
   * | 9 | hasAgent |
   */
  type BuildHttpMessageFlags = number;

  /**
   * Output slots written into the optional `Float64Array` out-param of
   * `buildHttpMessage` (keep in sync with `BuildHttpMessageOut`):
   * - [0] last (connection closes after this message)
   * - [1] chunked (Transfer-Encoding: chunked selected)
   * - [2] contentLength (resolved body length when known)
   * - [3] hasContentLength (1 if contentLength is valid)
   */
  type BuildHttpMessageOut = Float64Array;
}

export interface HttpParserBinding {
  ConnectionsList: typeof InternalHttpParserBinding.ConnectionsList;
  HTTPParser: typeof InternalHttpParserBinding.HTTPParser;
  allMethods: string[];
  methods: string[];

  /**
   * Build a complete HTTP/1.1 message (header block, optionally plus body)
   * into a single contiguous Buffer. Used by the OutgoingMessage fast path
   * in `lib/_http_outgoing.js`.
   *
   * @param firstLine Status/request line, e.g. `"HTTP/1.1 200 OK\r\n"`.
   * @param headers Flat `[name, value, name, value, ...]` list, or null.
   * @param body String/Uint8Array body, empty string for a complete
   *   headers+terminator message, or null/undefined for headers-only
   *   (`_storeHeader`; does not append the chunked `0\r\n\r\n`).
   * @param bodyEncoding 0=buffer/none, 1=latin1, 2=utf8.
   * @param flags Bitmask of BuildHttpMessageFlags.
   * @param date Pre-formatted UTC date string (or empty).
   * @param keepAliveSec Keep-Alive timeout in seconds.
   * @param maxRequests Max requests per socket (0 = omit from Keep-Alive).
   * @param knownLength Content-Length if already known, else -1.
   * @param out Optional Float64Array(4) receiving out-params.
   * @returns The on-wire message Buffer, or `undefined` on failure.
   */
  buildHttpMessage(
    firstLine: string,
    headers: readonly string[] | null | undefined,
    body: string | InternalHttpParserBinding.Buffer | null | undefined,
    bodyEncoding: InternalHttpParserBinding.BuildHttpMessageBodyEncoding | number,
    flags: InternalHttpParserBinding.BuildHttpMessageFlags,
    date: string,
    keepAliveSec: number,
    maxRequests: number,
    knownLength: number,
    out?: InternalHttpParserBinding.BuildHttpMessageOut,
  ): InternalHttpParserBinding.Buffer | undefined;
}
