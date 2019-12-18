/* eslint-disable */
let nextId = 1;
function getNewId() {
  const id = nextId;
  nextId++;
  return id;
}

class LoaderWorker {
  resolveImportURL() {
    throw new TypeError('Not implemented');
  }

  getFormat() {
    throw new TypeError('Not implemented');
  }

  getSource() {
    throw new TypeError('Not implemented');
  }

  transformSource() {
    throw new TypeError('Not implemented');
  }

  addBelowPort() {
    throw new TypeError('Not implemented');
  }
}

class RemoteLoaderWorker {
  constructor(port) {
    this._port = port;
    this._pending = new Map();

    this._port.on('message', this._handleMessage.bind(this));
    this._port.unref();
  }

  _handleMessage({ id, error, result }) {
    if (!this._pending.has(id)) return;
    const { resolve, reject } = this._pending.get(id);
    this._pending.delete(id);
    this._port.unref();

    if (error) {
      reject(error);
    } else {
      resolve(result);
    }
  }

  _send(method, params, transferList) {
    const id = getNewId();
    const message = { id, method, params };
    return new Promise((resolve, reject) => {
      this._pending.set(id, { resolve, reject });
      this._port.postMessage(message,  transferList);
      this._port.ref();
    });
  }
}

for (const method of Object.getOwnPropertyNames(LoaderWorker.prototype)) {
  Object.defineProperty(RemoteLoaderWorker.prototype, method, {
    configurable: true,
    enumerable: false,
    value(data, transferList) {
      return this._send(method, data, transferList);
    },
  });
}

function connectIncoming(port, instance) {
  port.on('message', async ({ id, method, params }) => {
    if (!id) return;

    let result = null;
    let error = null;
    try {
      if (typeof instance[method] !== 'function') {
        throw new TypeError(`No such RPC method: ${method}`);
      }
      result = await instance[method](params);
    } catch (e) {
      error = e;
    }
    port.postMessage({ id, error, result });
  });

  return instance;
}

module.exports = {
  connectIncoming,
  getNewId,
  LoaderWorker,
  RemoteLoaderWorker,
};
