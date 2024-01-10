'use strict';

const {
  NumberParseInt,
  ObjectFreeze,
  ObjectDefineProperties,
  SymbolToStringTag,
} = primordials;
const {
  Buffer,
} = require('buffer');
const {
  Transform,
  pipeline,
} = require('stream');
const { clearTimeout, setTimeout } = require('timers');
const {
  AbortController,
} = require('internal/abort_controller');
const { fetch } = require('internal/deps/undici/undici');
const { URL } = require('internal/url');
const {
  codes: {
    ERR_MISSING_OPTION,
  },
} = require('internal/errors');
const {
  Event,
  NodeEventTarget,
  kEvents,
} = require('internal/event_target');
const { kEmptyObject, kEnumerableProperty } = require('internal/util');

/**
 * @type {number[]} BOM
 */
const BOM = [0xEF, 0xBB, 0xBF];
/**
 * @type {10} LF
 */
const LF = 0x0A;
/**
 * @type {13} CR
 */
const CR = 0x0D;
/**
 * @type {58} COLON
 */
const COLON = 0x3A;
/**
 * @type {32} SPACE
 */
const SPACE = 0x20;

/**
 * The event stream format's MIME type is text/event-stream.
 * @see https://html.spec.whatwg.org/multipage/server-sent-events.html#parsing-an-event-stream
 */
const mimeType = 'text/event-stream';

/**
 * A reconnection time, in milliseconds. This must initially be an implementation-defined value,
 * probably in the region of a few seconds.
 *
 * In Comparison:
 * - Chrome uses 3000ms.
 * - Deno uses 5000ms.
 */
const defaultReconnectionTime = 3000;

/**
 * The readyState attribute represents the state of the connection.
 * @enum
 * @readonly
 * @see https://html.spec.whatwg.org/multipage/server-sent-events.html#dom-eventsource-readystate-dev
 */

/**
 * The connection has not yet been established, or it was closed and the user
 * agent is reconnecting.
 * @type {0}
 */
const CONNECTING = 0;

/**
 * The user agent has an open connection and is dispatching events as it
 * receives them.
 * @type {1}
 */
const OPEN = 1;

/**
 * The connection is not open, and the user agent is not trying to reconnect.
 * @type {2}
 */
const CLOSED = 2;

/**
 * @typedef {MessagePort|ServiceWorker|WindowProxy} MessageEventSource
 */

/**
 * @typedef {object} MessageEventInit
 * @property {*} [data] The data of the message.
 * @property {string} [origin] The origin of the message emitter.
 * @property {MessagePort[]} [ports] The ports associated with the channel the
 * message is being sent through (where appropriate, e.g. in channel messaging
 * or when sending a message to a shared worker).
 * @property {MessageEventSource} [source] A MessageEventSource (which can be a
 * WindowProxy, MessagePort, or ServiceWorker object) representing the message
 * emitter.
 * @property {string} [lastEventId] A DOMString representing a unique ID for
 * the event.
 */

/**
 * The MessageEvent interface represents a message received by a target object.
 * @class MessageEvent
 * @extends {Event}
 * @see https://html.spec.whatwg.org/multipage/comms.html#dom-messageevent-initmessageevent
 */
class MessageEvent extends Event {
  #data = null;
  #origin = null;
  #source = null;
  #ports = null;
  #lastEventId = null;

  /**
   * Creates a new MessageEvent object.
   * @param {string} type
   * @param {MessageEventInit} [options]
   */
  constructor(type, options = kEmptyObject) {
    super(type, options);

    this.#data = options.data ?? null;
    this.#origin = options.origin || '';
    this.#ports = options.ports || [];
    this.#source = options.source ?? null;
    this.#lastEventId = options.lastEventId || '';
  }

  /**
   * Returns the event's data. This can be any data type.
   * @readonly
   * @returns {*}
   */
  get data() {
    return this.#data;
  }

  /**
   * Returns the origin of the message, for server-sent events and
   * cross-document messaging.
   * @readonly
   * @returns {string}
   */
  get origin() {
    return this.#origin;
  }

  /**
   * Returns the last event ID string, for server-sent events.
   * @readonly
   * @returns {string}
   */
  get lastEventId() {
    return this.#lastEventId;
  }

  /**
   * Returns a MessageEventSource (which can be a WindowProxy, MessagePort,
   * or ServiceWorker object) representing the message source.
   * @readonly
   * @returns {MessageEventSource}
   */
  get source() {
    return this.#source;
  }

  /**
   * Returns the array containing the MessagePort objects
   * representing the ports associated with the channel the message is being
   * sent through (where appropriate, e.g. in channel messaging or when
   * sending a message to a shared worker).
   * @readonly
   * @returns {MessagePort[]}
   */
  get ports() {
    return ObjectFreeze(this.#ports);
  }

  /**
   * Initializes the value of a MessageEvent created using the
   * MessageEvent() constructor.
   * @param {string} type
   * @param {boolean} [bubbles=false]
   * @param {boolean} [cancelable=false]
   * @param {*} [data=null]
   * @param {string} [origin=""]
   * @param {string} [lastEventId=""]
   * @param {MessageEventSource} [source=null]
   * @param {MessagePort[]} [ports=[]]
   */
  initMessageEvent(
    type,
    bubbles = false,
    cancelable = false,
    data = null,
    origin = '',
    lastEventId = '',
    source = null,
    ports = [],
  ) {
    this.initEvent(type, bubbles, cancelable);

    this.#data = data;
    this.#origin = origin;
    this.#lastEventId = lastEventId;
    this.#source = source;
    this.#ports = ports;
  }
}

ObjectDefineProperties(MessageEvent.prototype, {
  [SymbolToStringTag]: {
    __proto__: null,
    writable: false,
    enumerable: false,
    configurable: true,
    value: 'MessageEvent',
  },
  detail: kEnumerableProperty,
});

/**
 * Checks if the given value is a valid LastEventId.
 * @param {Buffer | string} value
 * @returns {boolean}
 */
function isValidLastEventId(value) {
  // LastEventId should not contain U+0000 NULL
  return (
    typeof value === 'string' && (value.indexOf('\u0000') === -1)
  );
}

/**
 * Checks if the given value is a base 10 digit.
 * @param {Buffer | string} value
 * @returns {boolean}
 */
function isASCIINumber(value) {
  for (let i = 0; i < value.length; i++) {
    if (value.charCodeAt(i) < 0x30 || value.charCodeAt(i) > 0x39) return false;
  }
  return true;
}

/**
 * @typedef {object} EventSourceStreamEvent
 * @type {object}
 * @property {string} [event] The event type.
 * @property {string} [data] The data of the message.
 * @property {string} [id] A unique ID for the event.
 * @property {string} [retry] The reconnection time, in milliseconds.
 */

/**
 * @typedef EventSourceState
 * @type {object}
 * @property {string} lastEventId The last event ID received from the server.
 * @property {string} origin The origin of the event source.
 * @property {number} reconnectionTime The reconnection time, in milliseconds.
 */

class EventSourceStream extends Transform {
  /**
   * @type {EventSourceState}
   */
  state = null;

  /**
   * Leading byte-order-mark check.
   * @type {boolean}
   */
  checkBOM = true;

  /**
   * @type {boolean}
   */
  crlfCheck = false;

  /**
   * @type {boolean}
   */
  eventEndCheck = false;

  /**
   * @type {Buffer}
   */
  buffer = null;

  pos = 0;

  event = {
    data: undefined,
    event: undefined,
    id: undefined,
    retry: undefined,
  };

  /**
   * @param {object} options
   * @param {EventSourceState} options.eventSourceState
   * @param {Function} [options.push]
   */
  constructor(options = {}) {
    options.readableObjectMode = true;
    super(options);
    this.state = options.eventSourceState;
    if (options.push) {
      this.push = options.push;
    }
  }

  /**
   * @param {Buffer} chunk
   * @param {string} _encoding
   * @param {Function} callback
   * @returns {void}
   */
  _transform(chunk, _encoding, callback) {
    if (chunk.length === 0) {
      callback();
      return;
    }
    this.buffer = this.buffer ? Buffer.concat([this.buffer, chunk]) : chunk;

    // Strip leading byte-order-mark if any
    if (this.checkBOM) {
      switch (this.buffer.length) {
        case 1:
          if (this.buffer[0] === BOM[0]) {
            callback();
            return;
          }
          this.checkBOM = false;
          break;
        case 2:
          if (this.buffer[0] === BOM[0] && this.buffer[1] === BOM[1]) {
            callback();
            return;
          }
          this.checkBOM = false;
          break;
        case 3:
          if (this.buffer[0] === BOM[0] && this.buffer[1] === BOM[1] && this.buffer[2] === BOM[2]) {
            this.buffer = this.buffer.slice(3);
            this.checkBOM = false;
            callback();
            return;
          }
          this.checkBOM = false;
          break;
        default:
          if (this.buffer[0] === BOM[0] && this.buffer[1] === BOM[1] && this.buffer[2] === BOM[2]) {
            this.buffer = this.buffer.slice(3);
          }
          this.checkBOM = false;
          break;
      }
    }

    while (this.pos < this.buffer.length) {
      if (this.buffer[this.pos] === LF || this.buffer[this.pos] === CR) {
        if (this.eventEndCheck) {
          this.eventEndCheck = false;
          this.processEvent(this.event);
          this.event = {
            data: undefined,
            event: undefined,
            id: undefined,
            retry: undefined,
          };
          this.buffer = this.buffer.slice(1);
          continue;
        }
        if (this.buffer[0] === COLON) {
          this.buffer = this.buffer.slice(1);
          continue;
        }
        this.parseLine(this.buffer.slice(0, this.pos), this.event);

        // Remove the processed line from the buffer
        this.buffer = this.buffer.slice(this.pos + 1);
        // Reset the position
        this.pos = 0;
        this.eventEndCheck = true;
        continue;
      }
      this.pos++;
    }

    callback();
  }

  /**
   * @param {Buffer} line
   * @param {EventSourceStreamEvent} event
   */
  parseLine(line, event) {
    if (line.length === 0) {
      return;
    }
    const fieldNameEnd = line.indexOf(COLON);
    let fieldValueStart;

    if (fieldNameEnd === -1) {
      return;
      // fieldNameEnd = line.length;
      // fieldValueStart = line.length;
    }
    fieldValueStart = fieldNameEnd + 1;
    if (line[fieldValueStart] === SPACE) {
      fieldValueStart += 1;
    }


    const fieldValueSize = line.length - fieldValueStart;
    const fieldName = line.slice(0, fieldNameEnd).toString('utf8');
    switch (fieldName) {
      case 'data':
        event.data = line.slice(fieldValueStart, fieldValueStart + fieldValueSize).toString('utf8');
        break;
      case 'event':
        event.event = line.slice(fieldValueStart, fieldValueStart + fieldValueSize).toString('utf8');
        break;
      case 'id':
        event.id = line.slice(fieldValueStart, fieldValueStart + fieldValueSize).toString('utf8');
        break;
      case 'retry':
        event.retry = line.slice(fieldValueStart, fieldValueStart + fieldValueSize).toString('utf8');
        break;
    }
  }

  /**
   * @param {EventSourceStreamEvent} event
   */
  processEvent(event) {
    if (event.retry) {
      if (isASCIINumber(event.retry)) {
        this.state.reconnectionTime = NumberParseInt(event.retry, 10);
      }
    }
    const {
      id,
      data = null,
      event: type = 'message',
    } = event;

    if (id && isValidLastEventId(id)) {
      this.state.lastEventId = id;
    }

    this.push(
      new MessageEvent(type, {
        data,
        lastEventId: this.state.lastEventId,
        origin: this.state.origin,
      }),
    );
  }
}

/**
 * @typedef {object} EventSourceInit
 * @property {boolean} [withCredentials] indicates whether the request
 * should include credentials.
 */

/**
 * The EventSource interface is used to receive server-sent events. It
 * connects to a server over HTTP and receives events in text/event-stream
 * format without closing the connection.
 * @extends {NodeEventTarget}
 * @see https://developer.mozilla.org/en-US/docs/Web/API/EventSource
 * @api public
 */
class EventSource extends NodeEventTarget {
  #url = null;
  #withCredentials = false;
  #readyState = CONNECTING;
  #lastEventId = '';
  #connection = null;
  #reconnectionTimer = null;
  #controller = new AbortController();
  /**
   * @type {EventSourceState}
   */
  #state = {
    lastEventId: '',
    origin: '',
    reconnectionTime: defaultReconnectionTime,
  };

  /**
   * Creates a new EventSource object.
   * @param {string} url
   * @param {EventSourceInit} [eventSourceInitDict]
   */
  constructor(url, eventSourceInitDict) {
    super();

    if (arguments.length === 0) {
      throw new ERR_MISSING_OPTION('url');
    }

    this.#url = `${url}`;
    this.#state.origin = new URL(this.#url).origin;

    if (eventSourceInitDict) {
      if (eventSourceInitDict.withCredentials) {
        this.#withCredentials = eventSourceInitDict.withCredentials;
      }
    }

    this.#connect();
  }

  /**
   * Returns the state of this EventSource object's connection. It can have the
   * values described below.
   * @returns {0|1|2}
   * @readonly
   */
  get readyState() {
    return this.#readyState;
  }

  /**
   * Returns the URL providing the event stream.
   * @readonly
   * @returns {string}
   */
  get url() {
    return this.#url;
  }

  /**
   * Returns a boolean indicating whether the EventSource object was
   * instantiated with CORS credentials set (true), or not (false, the default).
   */
  get withCredentials() {
    return this.#withCredentials;
  }

  async #connect() {
    this.#readyState = CONNECTING;
    this.#connection = null;

    /**
     * @type {RequestInit}
     */
    const options = {
      method: 'GET',
      redirect: 'manual',
      keepalive: true,
      headers: {
        'Accept': mimeType,
        'Cache-Control': 'no-cache',
        'Connection': 'keep-alive',
      },
      signal: this.#controller.signal,
    };

    if (this.#lastEventId) {
      options.headers['Last-Event-ID'] = this.#lastEventId;
    }

    options.credentials = this.#withCredentials ? 'include' : 'omit';

    try {
      this.#connection = await fetch(this.#url, options);

      // Handle HTTP redirects
      // https://html.spec.whatwg.org/multipage/server-sent-events.html#server-sent-events-intro
      switch (this.#connection.status) {
        // Redirecting status codes
        case 301: // 301 Moved Permanently
        case 302: // 302 Found
        case 307: // 307 Temporary Redirect
        case 308: // 308 Permanent Redirect
          if (!this.#connection.headers.has('Location')) {
            this.close();
            this.dispatchEvent(new Event('error'));
            return;
          }
          this.#url = new URL(this.#connection.headers.get('Location'), new URL(this.#url).origin).href;
          this.#state.origin = new URL(this.#url).origin;
          this.#connect();
          return;
        case 204: // 204 No Content
          // Clients will reconnect if the connection is closed; a client can be told to stop reconnecting
          // using the HTTP 204 No Content response code.
          this.close();
          this.dispatchEvent(new Event('error'));
          return;
        case 200:
          if (this.#connection.headers.get('Content-Type') !== mimeType) {
            this.close();
            this.dispatchEvent(new Event('error'));
            return;
          }
          break;
        default:
          this.close();
          this.dispatchEvent(new Event('error'));
          return;
      }

      if (this.#connection === null) {
        this.close();
        this.dispatchEvent(new Event('error'));
        return;
      }

      const self = this;

      pipeline(this.#connection.body,
               new EventSourceStream({
                 eventSourceState: this.#state,
                 push: function push(chunk) {
                   self.dispatchEvent(chunk);
                 },
               }),
               (err) => {
                 if (err) {
                   this.dispatchEvent(new Event('error'));
                   this.close();
                 }
               });

      this.dispatchEvent(new Event('open'));
      this.#readyState = OPEN;

    } catch (error) {
      if (error.name === 'AbortError') {
        return;
      }
      this.dispatchEvent(new Event('error'));

      // Always set to CONNECTING as the readyState could be OPEN
      this.#readyState = CONNECTING;
      this.#connection = null;

      this.#reconnectionTimer = setTimeout(() => {
        this.#connect();
      }, this.#state.reconnectionTime);
    }

  }

  /**
   * Closes the connection, if any, and sets the readyState attribute to
   * CLOSED.
   */
  close() {
    if (this.#readyState === CLOSED) return;
    clearTimeout(this.#reconnectionTimer);
    this.#controller.abort();
    if (this.#connection) {
      this.#connection = null;
    }
    this.#readyState = CLOSED;
  }
}

ObjectDefineProperties(EventSource, {
  CONNECTING: {
    __proto__: null,
    configurable: false,
    enumerable: true,
    value: CONNECTING,
    writable: false,
  },
  OPEN: {
    __proto__: null,
    configurable: false,
    enumerable: true,
    value: OPEN,
    writable: false,
  },
  CLOSED: {
    __proto__: null,
    configurable: false,
    enumerable: true,
    value: CLOSED,
    writable: false,
  },
});

EventSource.prototype.CONNECTING = CONNECTING;
EventSource.prototype.OPEN = OPEN;
EventSource.prototype.CLOSED = CLOSED;

ObjectDefineProperties(EventSource.prototype, {
  'onopen': {
    __proto__: null,
    get: function get() {
      const listener = this[kEvents].get('open');
      return listener && listener.size > 0 ? listener.next.listener : undefined;
    },

    set: function set(listener) {
      if (typeof listener !== 'function') return;
      this.removeAllListeners('open');
      this.addEventListener('open', listener);
    },
  },
  'onmessage': {
    __proto__: null,
    get: function get() {
      const listener = this[kEvents].get('message');
      return listener && listener.size > 0 ? listener.next.listener : undefined;
    },

    set: function set(listener) {
      if (typeof listener !== 'function') return;
      this.removeAllListeners('message');
      this.addEventListener('message', listener);
    },
  },
  'onerror': {
    __proto__: null,
    get: function get() {
      const listener = this[kEvents].get('error');
      return listener && listener.size > 0 ? listener.next.listener : undefined;
    },

    set: function set(listener) {
      if (typeof listener !== 'function') return;
      this.removeAllListeners('error');
      this.addEventListener('error', listener);
    },
  },
});

module.exports = {
  EventSource,
  EventSourceStream,
  isValidLastEventId,
  isASCIINumber,
  MessageEvent,
};
