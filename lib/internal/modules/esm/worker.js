/* eslint-disable */
const {
  connectIncoming,
  RemoteLoaderWorker
} = require('internal/modules/esm/ipc_types');
const publicWorker = require('worker_threads');
const {
  parentPort,
  workerData,
} = publicWorker;
if (!parentPort) return;
parentPort.close();
// Hide parentPort and workerData from loader hooks.
publicWorker.parentPort = null;
publicWorker.workerData = null;
const esmLoader = require('internal/process/esm_loader');
const { defaultResolve } = require('internal/modules/esm/resolve');
const { defaultGetFormat } = require('internal/modules/esm/get_format');
const { defaultGetSource } = require('internal/modules/esm/get_source');
const { defaultTransformSource } = require(
  'internal/modules/esm/transform_source');

const defaultLoaderWorker = {
  resolveImportURL(params) {
    return defaultResolve(params.specifier, {
      parentURL: params.base,
      conditions: params.conditions,
    });
  },

  getFormat({ url }) {
    return defaultGetFormat(url);
  },

  getSource({ url, format }) {
    return defaultGetSource(url, { format });
  },

  transformSource({ source, url, format }) {
    return defaultTransformSource(source, { url, format });
  },
};

{
  const {
    insideBelowPort,
    insideAbovePort
  } = workerData;
  let parentLoaderAPI;
  if (!insideAbovePort) {
    parentLoaderAPI = defaultLoaderWorker;
  } else {
    const aboveDelegates = [];
    let rpcIndex = 0;
    const pendingAbove = [];
    parentLoaderAPI = {
      resolve(params) {
        return new Promise((f, r) => {
          if (rpcIndex >= aboveDelegates.length) {
            pendingAbove.push({
              params,
              return: f,
              throw: r
            });
            return;
          }
          const rpcAbove = aboveDelegates[rpcIndex];
          rpcIndex++;
          sendAbove(params, rpcAbove, f, r);
        });
      }
    };
    function addAbove(port) {
      if (workerData.nothingAbove === true) throw new Error();
      port.on('close', () => {
        const i = aboveDelegates.indexOf(rpcAbove);
        aboveDelegates.splice(i, 1);
        if (i < rpcIndex) rpcIndex--;
      });
      let rpcAbove = new RemoteLoaderWorker(insideAbovePort);

      aboveDelegates.push(rpcAbove);
      const pending = [...pendingAbove];
      pendingAbove.length = 0;
      for (const { params, return: f, throw: r } of pending) {
        sendAbove(params, rpcAbove, f, r);
      }
    }

    async function sendAbove(params, rpcAbove, f, r) {
      try {
        const value = ResolveResponse.fromOrNull(
          await rpcAbove.send(new ResolveRequest(params))
        );
        if (value !== null) return f(value);
        else return r(new Error('unknown resolve response'));
      } catch (e) {
        return r(e);
      }
    }

    addAbove(insideAbovePort);
  }

  const userModule = esmLoader.ESMLoader.importWrapped(workerData.loaderHREF).catch(
    (err) => {
      const { decorateErrorStack } = require('internal/util');
      decorateErrorStack(err);
      internalBinding('errors').triggerUncaughtException(
        err,
        true /* fromPromise */
      );
    }
  );

  async function forwardResolveHook(specifier, options) {
    return parentLoaderAPI.resolveImportURL({
      specifier,
      base: options.parentURL,
      conditions: options.conditions,
    });
  }

  async function forwardGetFormatHook(url) {
    return parentLoaderAPI.getFormat({ url });
  }

  async function forwardGetSourceHook(url, { format }) {
    return parentLoaderAPI.getSource({ url, format });
  }

  async function forwardTransformSourceHook(source, { url, format }) {
    return parentLoaderAPI.transformSource({ source, format, url });
  }

  const userLoaderWorker = {
    async resolveImportURL(params) {
      const { namespace: {resolve: resolveHandler}} = await userModule;
      if (!resolveHandler) {
        return parentLoaderAPI.resolveImportURL(params);
      }
      const options = {
        parentURL: params.base,
        conditions: params.conditions,
      };
      return resolveHandler(params.specifier, options, forwardResolveHook);
    },

    async getFormat({ url }) {
      const { namespace: {getFormat: getFormatHandler}} = await userModule;
      if (!getFormatHandler) {
        return parentLoaderAPI.getFormat({ url });
      }
      return getFormatHandler(url, {}, forwardGetFormatHook);
    },

    async getSource({ url, format }) {
      const { namespace: {getSource: getSourceHandler}} = await userModule;
      if (!getSourceHandler) {
        return parentLoaderAPI.getSource({ url, format });
      }
      return getSourceHandler(url, { format }, forwardGetSourceHook);
    },

    async transformSource({ url, format, source }) {
      const { namespace: {transformSource: transformSourceHandler}} = await userModule;
      if (!transformSourceHandler) {
        return parentLoaderAPI.transformSource({ url, format, source });
      }
      return transformSourceHandler(source, { format, url }, forwardTransformSourceHook);
    },

    async addBelowPort({ port }) {
      connectIncoming(port, this);
      return null;
    }
  };

  userLoaderWorker.addBelowPort({ port: insideBelowPort });
}
