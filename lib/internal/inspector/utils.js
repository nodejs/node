'use strict';

const EventEmitter = require('events');
const {
  ERR_INSPECTOR_ALREADY_CONNECTED,
  ERR_INSPECTOR_NOT_AVAILABLE
} = require('internal/errors').codes;
const { Connection, open } = process.binding('inspector');

if (!Connection)
  throw new ERR_INSPECTOR_NOT_AVAILABLE();

const connectionSymbol = Symbol('connectionProperty');
const nextIdSymbol = Symbol('nextId');
const onMessageSymbol = Symbol('onMessage');

class BaseSession extends EventEmitter {
  constructor() {
    super();
    this[connectionSymbol] = null;
    this[nextIdSymbol] = 1;
  }

  connect() {
    if (this[connectionSymbol])
      throw new ERR_INSPECTOR_ALREADY_CONNECTED('The inspector session');
    const connection =
      new Connection((message) => this[onMessageSymbol](message));
    if (connection.sessionAttached) {
      throw new ERR_INSPECTOR_ALREADY_CONNECTED('Another inspector session');
    }
    this[connectionSymbol] = connection;
  }
}

module.exports = {
  open: (port, host, wait) => open(port, host, !!wait),
  connectionSymbol,
  onMessageSymbol,
  nextIdSymbol,
  BaseSession
};
