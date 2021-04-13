'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeIncludes,
  Symbol
} = primordials;

const {
  Http3OptionsObject,
  QUIC_STREAM_HEADERS_KIND_INFO,
  QUIC_STREAM_HEADERS_KIND_INITIAL,
  QUIC_STREAM_HEADERS_KIND_TRAILING,
  QUIC_STREAM_HEADERS_FLAGS_NONE,
  QUIC_STREAM_HEADERS_FLAGS_TERMINAL,
} = internalBinding('quic');

if (Http3OptionsObject === undefined)
  return;

const {
  utcDate,
} = require('internal/http');

const {
  validateBigIntOrSafeInteger,
  validateObject,
} = require('internal/validators');

const {
  kType,
} = require('internal/quic/common');

const {
  inspect,
} = require('util');

const {
  customInspectSymbol: kInspect,
} = require('internal/util');

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_THIS,
    ERR_HTTP_INVALID_HEADER_VALUE,
    ERR_HTTP2_CONNECT_AUTHORITY,
    ERR_HTTP2_CONNECT_PATH,
    ERR_HTTP2_CONNECT_SCHEME,
    ERR_HTTP2_INVALID_INFO_STATUS,
    ERR_HTTP2_PAYLOAD_FORBIDDEN,
    ERR_HTTP2_STATUS_101,
    ERR_HTTP2_STATUS_INVALID,
    ERR_HTTP3_HOST_OR_AUTHORITY_REQUIRED,
  },
} = require('internal/errors');

const {
  assertValidPseudoHeaderResponse,
  assertValidPseudoHeaderTrailer,
  mapToHeaders,
} = require('internal/http2/util');

const kSensitiveHeaders = Symbol('nodejs.http2.sensitiveHeaders');
const kHandle = Symbol('kHandle');
const kOptions = Symbol('kOptions');

/**
 * @typedef {{
 *   maxHeaderLength? : bigint | number,
 *   maxHeaderPairs? : bigint | number,
 *   maxFieldSectionSize? : bigint | number,
 *   maxPushes? : bigint | number,
 *   qpackBlockedStreams? : bigint | number,
 *   qpackMaxTableCapacity? : bigint | number,
 * }} Http3OptionsInit
 */

class Http3Options {
  [kType] = 'Http3Options';

  /**
   * @param {*} value
   * @returns {boolean}
   */
  static isHttp3Options(value) {
    return typeof value?.[kHandle] === 'object';
  }

  /**
   * @param {Http3OptionsInit} [options]
   */
  constructor(options = {}) {
    validateObject(options, 'options');
    const {
      maxHeaderLength,
      maxHeaderPairs,
      maxFieldSectionSize,
      maxPushes,
      qpackBlockedStreams,
      qpackMaxTableCapacity,
    } = options;

    if (maxHeaderLength !== undefined) {
      validateBigIntOrSafeInteger(
        maxHeaderLength,
        'options.maxHeaderLength');
    }

    if (maxHeaderPairs !== undefined) {
      validateBigIntOrSafeInteger(
        maxHeaderPairs,
        'options.maxHeaderPairs');
    }

    if (maxFieldSectionSize !== undefined) {
      validateBigIntOrSafeInteger(
        maxFieldSectionSize,
        'options.maxFieldSectionSize');
    }

    if (maxPushes !== undefined)
      validateBigIntOrSafeInteger(maxPushes, 'options.maxPushes');

    if (qpackBlockedStreams !== undefined) {
      validateBigIntOrSafeInteger(
        qpackBlockedStreams,
        'options.qpackBlockedStreams');
    }

    if (qpackMaxTableCapacity !== undefined) {
      validateBigIntOrSafeInteger(
        qpackMaxTableCapacity,
        'options.qpackMaxTableCapacity');
    }

    this[kOptions] = {
      maxHeaderLength,
      maxHeaderPairs,
      maxFieldSectionSize,
      maxPushes,
      qpackBlockedStreams,
      qpackMaxTableCapacity,
    };

    this[kHandle] = new Http3OptionsObject(this[kOptions]);
  }

  [kInspect](depth, options) {
    if (!Http3Options.isHttp3Options(this))
      throw new ERR_INVALID_THIS('Http3Options');
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `${this[kType]} ${inspect(this[kOptions], opts)}`;
  }
}

const kHttp3Application = {
  handleHints(stream, hints) {
    // If there are 1xx headers, send those before doing
    // any of the work on the actual response. Unlike
    // the body, headers, and trailers, the hints must
    // be provided directly and immediately. A promise
    // to provide those is not supported.
    if (hints !== undefined) {
      validateObject(hints, 'response.hints');
      hints[':status'] ??= 100;

      // Using == here instead of === is intentional because :status could
      // be a string
      /* eslint-disable-next-line eqeqeq */
      if (hints[':status'] == 101)
        throw new ERR_HTTP2_STATUS_101();
      if (hints[':status'] < 100 || hints[':status'] >= 200)
        throw new ERR_HTTP2_INVALID_INFO_STATUS(hints[':status']);

      const neverIndex = hints[kSensitiveHeaders];
      if (neverIndex !== undefined && !ArrayIsArray(neverIndex))
        throw new ERR_INVALID_ARG_VALUE('hints[http2.neverIndex]', neverIndex);

      stream.sendHeaders(
        QUIC_STREAM_HEADERS_KIND_INFO,
        mapToHeaders(hints, assertValidPseudoHeaderResponse),
        QUIC_STREAM_HEADERS_FLAGS_NONE);
    }
  },

  async handleRequestHeaders(
    stream,
    headers,
    terminal = false,
    authority = undefined) {
    if (headers != null) {
      const actualHeaders = await headers;
      validateObject(actualHeaders, 'response.headers');

      if (actualHeaders[':method'] === 'CONNECT') {
        if (actualHeaders[':scheme'])
          throw new ERR_HTTP2_CONNECT_SCHEME();
        if (actualHeaders[':path'])
          throw new ERR_HTTP2_CONNECT_PATH();
        if (actualHeaders[':authority'] === undefined)
          throw new ERR_HTTP2_CONNECT_AUTHORITY();
      } else {
        actualHeaders[':scheme'] ??= 'https';
        actualHeaders[':method'] ??= 'GET';

        // HTTP/3 allows *either or both* the :authority and host header fields,
        // but prefers :authority. If neither is given, or if their values do
        // not match, it's an error.
        if (actualHeaders[':authority'] === undefined &&
            actualHeaders.host === undefined) {
          if (authority === undefined)
            throw new ERR_HTTP3_HOST_OR_AUTHORITY_REQUIRED();
          actualHeaders[':authority'] = authority;
        }
        if ((actualHeaders[':authority'] !== undefined &&
            actualHeaders.host !== undefined) &&
            actualHeaders[':authority'] !== actualHeaders.host) {
          throw new ERR_HTTP_INVALID_HEADER_VALUE(actualHeaders.host, 'host');
        }

        if (ArrayPrototypeIncludes(['http', 'https'],
                                   actualHeaders[':scheme'])) {
          actualHeaders[':path'] ??=
            actualHeaders[':method'] === 'OPTIONS' ? '*' : '/';
        }
      }

      const neverIndex = actualHeaders[kSensitiveHeaders];
      if (neverIndex !== undefined && !ArrayIsArray(neverIndex)) {
        throw new ERR_INVALID_ARG_VALUE(
          'headers[http2.neverIndex]',
          neverIndex);
      }

      stream.sendHeaders(
        QUIC_STREAM_HEADERS_KIND_INITIAL,
        mapToHeaders(actualHeaders),
        terminal ?
          QUIC_STREAM_HEADERS_FLAGS_TERMINAL :
          QUIC_STREAM_HEADERS_FLAGS_NONE);
    }
  },

  async handleResponseHeaders(stream, headers, terminal = false) {
    if (headers != null) {
      const actualHeaders = await headers;
      validateObject(actualHeaders, 'response.headers');
      actualHeaders[':status'] ??= 200;

      if (actualHeaders[':status'] < 200 || actualHeaders[':status'] >= 600)
        throw new ERR_HTTP2_STATUS_INVALID(actualHeaders[':status']);

      switch (+actualHeaders[':status']) {
        case 204:  // No Content
        case 205:  // Reset Content
        case 304:  // Not Modified
          if (!terminal)
            throw new ERR_HTTP2_PAYLOAD_FORBIDDEN(actualHeaders[':status']);
          // Otherwise fall through
      }

      actualHeaders.date ??= utcDate();

      const neverIndex = actualHeaders[kSensitiveHeaders];
      if (neverIndex !== undefined && !ArrayIsArray(neverIndex)) {
        throw new ERR_INVALID_ARG_VALUE(
          'headers[http2.neverIndex]',
          neverIndex);
      }

      stream.sendHeaders(
        QUIC_STREAM_HEADERS_KIND_INITIAL,
        mapToHeaders(actualHeaders, assertValidPseudoHeaderResponse),
        terminal ?
          QUIC_STREAM_HEADERS_FLAGS_TERMINAL :
          QUIC_STREAM_HEADERS_FLAGS_NONE);
    }
  },

  handleTrailingHeaders(stream, trailers) {
    if (trailers != null) {
      stream.sendHeaders(
        QUIC_STREAM_HEADERS_KIND_TRAILING,
        mapToHeaders(trailers, assertValidPseudoHeaderTrailer),
        QUIC_STREAM_HEADERS_FLAGS_NONE);
    }
  },
};

module.exports = {
  Http3Options,
  kHandle,
  kHttp3Application,
};
