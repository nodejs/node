'use strict';

const {
  BigInt,
  JSONStringify,
  ObjectDefineProperty,
  Promise,
  PromisePrototypeThen,
  ReflectHas,
  StringPrototypeSlice,
  Symbol,
} = primordials;

const {
  codes: {
    ERR_INSPECTOR_NOT_AVAILABLE,
    ERR_SCRIPT_EXECUTION_INTERRUPTED,
  },
} = require('internal/errors');
const { runInterruptible } = internalBinding('contextify');
const { hasInspector } = internalBinding('config');

let Session;

// Object group used for every remote handle we create so the whole batch can be
// released in one go after each evaluation, keeping the inspector heap tidy.
const kObjectGroup = 'node-repl';

// Property used to ferry a live value from the inspector world back to ours.
const kStashKey = '__nodeREPLStash';
const kStashFn = `function() {
  Object.defineProperty(globalThis, ${JSONStringify(kStashKey)}, {
    value: this, configurable: true, enumerable: false, writable: true,
  });
}`;

// Cached channel, attached to the REPL server instance.
const kChannel = Symbol('kReplInspectorChannel');

// Converts the `unserializableValue` field of a remote object back into the
// real primitive it represents.
function fromUnserializable(value) {
  switch (value) {
    case 'NaN':
      return NaN;
    case 'Infinity':
      return Infinity;
    case '-Infinity':
      return -Infinity;
    case '-0':
      return -0;
    default:
      // BigInt literals arrive as e.g. "123n".
      if (typeof value === 'string' && value[value.length - 1] === 'n') {
        try {
          return BigInt(StringPrototypeSlice(value, 0, -1));
        } catch {
          // Fall through and return the raw string.
        }
      }
      return value;
  }
}

class ReplInspectorChannel {
  constructor(repl) {
    this.repl = repl;
    this.session = null;
    // Exposed so callers can scope their own remote handles to the shared group.
    this.objectGroup = kObjectGroup;
  }

  // Lazily connects the shared session and arranges for it to be torn down on
  // REPL exit.
  getSession() {
    if (this.session !== null) {
      return this.session;
    }
    Session ??= require('inspector').Session;
    const session = this.session = new Session();
    session.connect();
    session.post('Runtime.enable');
    this.repl.once('exit', () => {
      this.session = null;
      try {
        session.post('Runtime.disable');
        session.disconnect();
      } catch {
        // The session may already be gone; nothing else to do.
      }
    });
    return session;
  }

  // Issues a single inspector command.
  post(method, params) {
    return new Promise((resolve, reject) => {
      this.getSession().post(method, params, (error, result) => {
        if (error) {
          reject(error);
        } else {
          resolve(result);
        }
      });
    });
  }

  // Issues an inspector command that may be interrupted by SIGINT.
  postInterruptible(method, params, breakOnSigint) {
    if (!breakOnSigint) {
      return this.post(method, params);
    }
    return new Promise((resolve, reject) => {
      let fired = false;
      let cbError;
      let cbResult;
      let onLate = null;
      const handle = (error, result) => {
        fired = true;
        cbError = error;
        cbResult = result;
        if (onLate !== null) {
          onLate();
        }
      };
      const interrupted = runInterruptible(() => {
        this.getSession().post(method, params, handle);
      });
      if (interrupted) {
        // Execution was terminated by SIGINT; ignore any callback that may have
        // fired as part of the termination.
        reject(new ERR_SCRIPT_EXECUTION_INTERRUPTED());
        return;
      }
      if (fired) {
        if (cbError) {
          reject(cbError);
        } else {
          resolve(cbResult);
        }
        return;
      }
      // The evaluation is still pending (e.g. top-level await); resolve once
      // the inspector eventually invokes the callback.
      onLate = () => {
        if (cbError) {
          reject(cbError);
        } else {
          resolve(cbResult);
        }
      };
    });
  }

  // Performs a `Runtime.evaluate`.
  async evaluate(params, breakOnSigint) {
    const response = await this.postInterruptible(
      'Runtime.evaluate', params, breakOnSigint);
    if (response.exceptionDetails || response.result?.subtype !== 'promise') {
      return response;
    }

    // Hold the promise open until it resolves or rejects,
    // so we can return the final value.
    return this.postInterruptible('Runtime.awaitPromise', {
      __proto__: null,
      promiseObjectId: response.result.objectId,
      returnByValue: params.returnByValue,
      generatePreview: params.generatePreview,
    }, breakOnSigint);
  }

  // Turns a remote object returned by the inspector into the real, live value
  // it points at.
  async toLiveValue(remote, context) {
    if (remote.objectId !== undefined) {
      await this.post('Runtime.callFunctionOn', {
        functionDeclaration: kStashFn,
        objectId: remote.objectId,
        objectGroup: kObjectGroup,
      });
      const value = context[kStashKey];
      delete context[kStashKey];
      return { __proto__: null, value };
    }
    if (remote.type === 'undefined') {
      return { __proto__: null, value: undefined };
    }
    if (ReflectHas(remote, 'value')) {
      return { __proto__: null, value: remote.value };
    }
    if (remote.unserializableValue !== undefined) {
      return { __proto__: null, value: fromUnserializable(remote.unserializableValue) };
    }
    return { __proto__: null, value: undefined };
  }

  // Releases every remote handle created in the shared object group.
  releaseObjects() {
    if (this.session !== null) {
      this.session.post('Runtime.releaseObjectGroup', {
        objectGroup: kObjectGroup,
      });
    }
  }

  // Fetches the global lexical scope names (`let`/`const`/`class` declarations)
  // for a context.
  globalLexicalScopeNames(contextId) {
    return PromisePrototypeThen(
      this.post('Runtime.globalLexicalScopeNames', {
        executionContextId: contextId,
      }),
      (result) => result?.names ?? [],
      () => [],
    );
  }

  // Captures the inspector execution-context id of a freshly created context.
  captureExecutionContextId(createContext) {
    const session = this.getSession();
    let contextId;
    session.once('Runtime.executionContextCreated', ({ params }) => {
      contextId = params.context.id;
    });
    const context = createContext();
    return { context, contextId };
  }
}

function getReplInspector(repl) {
  if (!hasInspector) {
    throw new ERR_INSPECTOR_NOT_AVAILABLE();
  }
  if (repl[kChannel] === undefined) {
    ObjectDefineProperty(repl, kChannel, {
      __proto__: null,
      value: new ReplInspectorChannel(repl),
      configurable: true,
      writable: true,
      enumerable: false,
    });
  }
  return repl[kChannel];
}

module.exports = {
  getReplInspector,
};
