'use strict';

// The ServerContext and ConnectContext protocols of the Fetch Server API.
// https://fetch-server.proposal.wintertc.org/#servercontext

const {
  ObjectDefineProperties,
  ObjectGetOwnPropertyDescriptors,
  PromiseReject,
  PromiseResolve,
  ReflectOwnKeys,
  RegExpPrototypeExec,
  SafeSet,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeToLowerCase,
  StringPrototypeTrim,
  Symbol,
  SymbolFor,
  SymbolToStringTag,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');

const {
  converters,
} = require('internal/webidl');

const {
  getErrorCode,
  invalidState,
  kDenyConnectSendable,
  kDenySendable,
  kRequestRejected,
  notSupported,
} = require('internal/http/fetch_server/errors');

const {
  convertHeadersInit,
  listGet,
  toByteString,
} = require('internal/http/fetch_server/headers');

const kFallThrough = SymbolFor('wintertc.server.fallthrough');
const kContext = Symbol('kContext');

// Protocols known to use the Capsule Protocol (Section 11.3).
const kCapsuleProtocols = new SafeSet(['connect-udp', 'connect-ip']);

function getState(self) {
  const state = self?.[kContext];
  if (state === undefined) {
    throw new ERR_INVALID_ARG_TYPE('this', 'ServerContext', self);
  }
  return state;
}

function getOwnAccessors(obj) {
  const descriptors = ObjectGetOwnPropertyDescriptors(obj);
  const keys = ReflectOwnKeys(descriptors);
  for (let i = 0; i < keys.length; i++) {
    descriptors[keys[i]].enumerable = true;
  }
  return descriptors;
}

function convertAddress(address) {
  // Returns the SocketAddress converted to a JavaScript value.
  const result = { address: address?.address ?? '', port: address?.port ?? 0 };
  if (address?.family !== undefined) result.family = address.family;
  if (address?.path !== undefined) result.path = address.path;
  return result;
}

// Parses an RFC 9218 Priority field value (a Structured Field Dictionary).
const kKeyRegExp = /^[a-z*][a-z0-9_\-.*]*$/;
function parsePriority(value) {
  const priority = { urgency: 3, incremental: false };
  if (value === null) return priority;
  const members = StringPrototypeSplit(value, ',');
  let urgency = 3;
  let incremental = false;
  for (let i = 0; i < members.length; i++) {
    const member = StringPrototypeTrim(members[i]);
    if (member === '') return priority;
    const eq = StringPrototypeIndexOf(member, '=');
    const key = eq === -1 ? member : StringPrototypeSlice(member, 0, eq);
    // Parameters on members are ignored.
    let val = eq === -1 ? '?1' : StringPrototypeSlice(member, eq + 1);
    const semi = StringPrototypeIndexOf(val, ';');
    if (semi !== -1) val = StringPrototypeSlice(val, 0, semi);
    if (RegExpPrototypeExec(kKeyRegExp, key) === null) return priority;
    if (key === 'u') {
      if (RegExpPrototypeExec(/^-?[0-9]{1,15}$/, val) !== null) {
        const n = +val;
        if (n >= 0 && n <= 7) urgency = n;
      }
    } else if (key === 'i') {
      if (val === '?1') incremental = true;
      else if (val === '?0') incremental = false;
    }
  }
  priority.urgency = urgency;
  priority.incremental = incremental;
  return priority;
}

function convertPriority(value) {
  if (value === null || value === undefined) {
    return { urgency: 3, incremental: false };
  }
  if (typeof value !== 'object' && typeof value !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('serverPriority', 'Object', value);
  }
  const incremental = value.incremental;
  const urgency = value.urgency;
  return {
    urgency: urgency === undefined ? 3 :
      converters['unsigned short'](urgency, {
        __proto__: null,
        enforceRange: true,
        context: 'urgency',
      }),
    incremental: incremental === undefined ? false : !!incremental,
  };
}

// =============================================================================
// ServerContext (Section 5.8, Section 7)
// =============================================================================

function sendInformationalSteps(state, status, headers) {
  const exchange = state.exchange;
  status = converters['unsigned short'](status, {
    __proto__: null,
    enforceRange: true,
    prefix: 'Failed to execute \'sendInformational\'',
    context: 'status',
  });
  const fields = headers === undefined ? [] : convertHeadersInit(headers);
  if (status === 101 || status < 100 || status > 199) {
    throw new ERR_OUT_OF_RANGE('status', '>= 100 and <= 199, not 101', status);
  }
  if (state.inert || exchange.state === 'committed' ||
      exchange.state === 'complete' || exchange.state === 'denied') {
    throw invalidState(
      `Cannot send an informational response in the ${exchange.state} state`);
  }
  if (exchange.state === 'aborted') return;
  if (exchange.backend.httpVersion === '1.0') return;
  if (status === 100) {
    if (!exchange.pending100) {
      throw invalidState('No 100 (Continue) response is pending');
    }
    exchange.pending100 = false;
    exchange.backend.sendContinue(fields);
    exchange.state = 'informational';
    return;
  }
  if (exchange.autoContinue && exchange.pending100) {
    exchange.pending100 = false;
    exchange.backend.sendContinue([]);
  }
  exchange.backend.sendInformational(status, fields);
  exchange.state = 'informational';
}

function denySteps(state, error) {
  const exchange = state.exchange;
  if (exchange.state === 'aborted') return;
  if (state.inert || exchange.state !== 'open') {
    throw invalidState(`Cannot deny in the ${exchange.state} state`);
  }
  if (error === kFallThrough) {
    if (!state.isConnect || exchange.kind !== 'upgrade') {
      throw invalidState(
        'Only an upgrade-originated exchange can fall through');
    }
    exchange.fallThrough = true;
    state.inert = true;
    return;
  }
  const sendable = state.isConnect && exchange.kind === 'connect' ?
    kDenyConnectSendable : kDenySendable;
  const code = getErrorCode(error, sendable) ?? kRequestRejected;
  exchange.deny();
  exchange.backend.deny(code);
}

function waitUntilSteps(state, promise) {
  state.exchange.waitUntil(PromiseResolve(promise));
}

const contextAccessors = {
  __proto__: null,
  get request() {
    return getState(this).exchange.request;
  },
  get remoteAddress() {
    return convertAddress(getState(this).exchange.backend.remoteAddress());
  },
  get localAddress() {
    return convertAddress(getState(this).exchange.backend.localAddress());
  },
  get alpnProtocol() {
    return getState(this).exchange.backend.alpnProtocol;
  },
  get encrypted() {
    return getState(this).exchange.backend.encrypted;
  },
  get serverName() {
    return getState(this).exchange.backend.serverName;
  },
  get clientPriority() {
    const state = getState(this);
    state.clientPriority ??= parsePriority(
      listGet(state.exchange.backend.headerList, 'priority'));
    return { urgency: state.clientPriority.urgency,
             incremental: state.clientPriority.incremental };
  },
  get serverPriority() {
    const state = getState(this);
    const value = state.exchange.serverPriority;
    return value == null ? null :
      { urgency: value.urgency, incremental: value.incremental };
  },
  set serverPriority(value) {
    const state = getState(this);
    if (value === null || value === undefined) {
      if (!state.inert) state.exchange.serverPriority = null;
      return;
    }
    const priority = convertPriority(value);
    if (priority.urgency > 7) {
      throw new ERR_OUT_OF_RANGE('urgency', '>= 0 and <= 7', priority.urgency);
    }
    if (!state.inert) state.exchange.serverPriority = priority;
  },
  get onpriority() {
    return getState(this).exchange.onpriority ?? null;
  },
  set onpriority(value) {
    const state = getState(this);
    const callback = typeof value === 'function' ? value : null;
    if (!state.inert) state.exchange.onpriority = callback;
  },
  get autoContinue() {
    return getState(this).exchange.autoContinue;
  },
  get completed() {
    return getState(this).exchange.completed;
  },
  get sendInformational() {
    const state = getState(this);
    state.sendInformational ??= function sendInformational(status, headers) {
      return sendInformationalSteps(state, status, headers);
    };
    return state.sendInformational;
  },
  get deny() {
    const state = getState(this);
    state.deny ??= function deny(error) {
      return denySteps(state, error);
    };
    return state.deny;
  },
  get waitUntil() {
    const state = getState(this);
    state.waitUntil ??= function waitUntil(promise) {
      if (arguments.length === 0) {
        throw new ERR_INVALID_ARG_TYPE('promise', 'Promise', promise);
      }
      return waitUntilSteps(state, promise);
    };
    return state.waitUntil;
  },
};

const ServerContextPrototype = { __proto__: null };
ObjectDefineProperties(ServerContextPrototype,
                       getOwnAccessors(contextAccessors));
ObjectDefineProperties(ServerContextPrototype, {
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'ServerContext',
  },
});


// =============================================================================
// ConnectContext (Section 11)
// =============================================================================

function checkEstablish(state) {
  const exchange = state.exchange;
  if (state.inert || exchange.state === 'committed' ||
      exchange.state === 'complete' || exchange.state === 'denied') {
    throw invalidState(
      `Cannot establish a tunnel in the ${exchange.state} state`);
  }
}

function getOfferedProtocols(exchange) {
  const upgrade = listGet(exchange.backend.headerList, 'upgrade');
  if (upgrade === null) return [];
  const offered = StringPrototypeSplit(upgrade, ',');
  const result = [];
  for (let i = 0; i < offered.length; i++) {
    const token = StringPrototypeTrim(offered[i]);
    if (token !== '') result[result.length] = token;
  }
  return result;
}

function getConnectProtocol(exchange) {
  if (exchange.kind === 'upgrade') {
    const offered = getOfferedProtocols(exchange);
    if (offered.length === 0) return null;
    const first = offered[0];
    const slash = StringPrototypeIndexOf(first, '/');
    return StringPrototypeToLowerCase(slash === -1 ? first : StringPrototypeSlice(first, 0, slash));
  }
  return exchange.backend.connectProtocol ?? null;
}

async function acceptSteps(state, init) {
  const exchange = state.exchange;
  checkEstablish(state);
  let fields = [];
  let protocol;
  if (init !== undefined && init !== null) {
    if (typeof init !== 'object' && typeof init !== 'function') {
      throw new ERR_INVALID_ARG_TYPE('init', 'Object', init);
    }
    if (init.headers !== undefined) fields = convertHeadersInit(init.headers);
    if (init.protocol !== undefined) {
      protocol = toByteString(init.protocol, 'init.protocol');
    }
  }
  const offered = getOfferedProtocols(exchange);
  if (protocol !== undefined) {
    if (exchange.kind !== 'upgrade') {
      throw new ERR_INVALID_ARG_VALUE(
        'init.protocol', protocol,
        'can only be given on an upgrade-originated exchange');
    }
    const lower = StringPrototypeToLowerCase(protocol);
    let matched = false;
    for (let i = 0; i < offered.length; i++) {
      const candidate = StringPrototypeToLowerCase(offered[i]);
      const slash = StringPrototypeIndexOf(candidate, '/');
      if (candidate === lower ||
          (slash !== -1 && StringPrototypeSlice(candidate, 0, slash) === lower)) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      throw new ERR_INVALID_ARG_VALUE('init.protocol', protocol,
                                      'was not offered by the client');
    }
  }
  if (exchange.state === 'aborted') throw exchange.abortReason;
  const connectProtocol = getConnectProtocol(exchange);
  if (kCapsuleProtocols.has(connectProtocol)) {
    let hasCapsuleProtocol = false;
    for (let i = 0; i < fields.length; i++) {
      if (StringPrototypeToLowerCase(fields[i][0]) === 'capsule-protocol') {
        hasCapsuleProtocol = true;
      }
    }
    if (!hasCapsuleProtocol) fields[fields.length] = ['Capsule-Protocol', '?1'];
  }
  exchange.commit();
  const { createTunnel } = require('internal/http/fetch_server/tunnel');
  const selected = protocol ?? offered[0];
  const transport = await exchange.backend.acceptTunnel(exchange, fields,
                                                        selected);
  if (exchange.state === 'aborted') throw exchange.abortReason;
  return createTunnel(exchange, transport, connectProtocol);
}

function upgradeWebSocket(state, options, stream) {
  const exchange = state.exchange;
  if (getConnectProtocol(exchange) !== 'websocket') {
    throw invalidState('The connect protocol is not "websocket"');
  }
  checkEstablish(state);
  const ws = require('internal/http/fetch_server/websocket');
  return stream ?
    ws.upgradeWebSocketStream(exchange, options) :
    ws.upgradeWebSocket(exchange, options);
}

const connectAccessors = {
  __proto__: null,
  get connectProtocol() {
    return getConnectProtocol(getState(this).exchange);
  },
  get accept() {
    const state = getState(this);
    state.accept ??= function accept(init = undefined) {
      try {
        return acceptSteps(state, init);
      } catch (err) {
        return PromiseReject(err);
      }
    };
    return state.accept;
  },
  get upgradeWebSocket() {
    const state = getState(this);
    state.upgradeWebSocket ??= function upgradeWebSocket(options = undefined) {
      return upgradeWebSocket(state, options, false);
    };
    return state.upgradeWebSocket;
  },
  get upgradeWebSocketStream() {
    const state = getState(this);
    state.upgradeWebSocketStream ??=
      function upgradeWebSocketStream(options = undefined) {
        return upgradeWebSocket(state, options, true);
      };
    return state.upgradeWebSocketStream;
  },
  get upgradeWebTransport() {
    const state = getState(this);
    state.upgradeWebTransport ??= function upgradeWebTransport() {
      const exchange = state.exchange;
      if (getConnectProtocol(exchange) !== 'webtransport') {
        return PromiseReject(invalidState(
          'The connect protocol is not "webtransport"'));
      }
      try {
        checkEstablish(state);
      } catch (err) {
        return PromiseReject(err);
      }
      return PromiseReject(notSupported('WebTransport is not supported'));
    };
    return state.upgradeWebTransport;
  },
};

const ConnectContextPrototype = { __proto__: ServerContextPrototype };
ObjectDefineProperties(ConnectContextPrototype,
                       getOwnAccessors(connectAccessors));
ObjectDefineProperties(ConnectContextPrototype, {
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'ConnectContext',
  },
});

/**
 * Creates a ServerContext or ConnectContext for an exchange.
 * @param {import('./exchange').Exchange} exchange
 * @param {boolean} isConnect
 * @returns {object}
 */
function createContext(exchange, isConnect) {
  const ctx = {
    __proto__: isConnect ? ConnectContextPrototype : ServerContextPrototype,
  };
  ObjectDefineProperties(ctx, {
    [kContext]: {
      __proto__: null,
      value: {
        __proto__: null,
        exchange,
        isConnect,
        inert: false,
      },
    },
  });
  return ctx;
}

function isContextInert(ctx) {
  return ctx[kContext].inert;
}

module.exports = {
  createContext,
  getConnectProtocol,
  isContextInert,
  kFallThrough,
};
