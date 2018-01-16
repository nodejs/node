'use strict';

const { emitExperimentalWarning } = require('internal/util');
const { URL } = require('internal/url');
const { kParsingContext, isContext } = process.binding('contextify');
const errors = require('internal/errors');
const {
  getConstructorOf,
  customInspectSymbol,
} = require('internal/util');

const {
  ModuleWrap,
  kUninstantiated,
  kInstantiating,
  kInstantiated,
  kEvaluating,
  kEvaluated,
  kErrored,
} = internalBinding('module_wrap');

const STATUS_MAP = {
  [kUninstantiated]: 'uninstantiated',
  [kInstantiating]: 'instantiating',
  [kInstantiated]: 'instantiated',
  [kEvaluating]: 'evaluating',
  [kEvaluated]: 'evaluated',
  [kErrored]: 'errored',
};

let globalModuleId = 0;
const perContextModuleId = new WeakMap();
const wrapMap = new WeakMap();
const dependencyCacheMap = new WeakMap();
const linkingStatusMap = new WeakMap();

class Module {
  constructor(src, options = {}) {
    emitExperimentalWarning('vm.Module');

    if (typeof src !== 'string')
      throw new errors.TypeError(
        'ERR_INVALID_ARG_TYPE', 'src', 'string', src);
    if (typeof options !== 'object' || options === null)
      throw new errors.TypeError(
        'ERR_INVALID_ARG_TYPE', 'options', 'object', options);

    let context;
    if (options.context !== undefined) {
      if (isContext(options.context)) {
        context = options.context;
      } else {
        throw new errors.TypeError(
          'ERR_INVALID_ARG_TYPE', 'options.context', 'vm.Context');
      }
    }

    let url = options.url;
    if (url !== undefined) {
      if (typeof url !== 'string') {
        throw new errors.TypeError(
          'ERR_INVALID_ARG_TYPE', 'options.url', 'string', url);
      }
      url = new URL(url).href;
    } else if (context === undefined) {
      url = `vm:module(${globalModuleId++})`;
    } else if (perContextModuleId.has(context)) {
      const curId = perContextModuleId.get(context);
      url = `vm:module(${curId})`;
      perContextModuleId.set(context, curId + 1);
    } else {
      url = 'vm:module(0)';
      perContextModuleId.set(context, 1);
    }

    const wrap = new ModuleWrap(src, url, {
      [kParsingContext]: context,
      lineOffset: options.lineOffset,
      columnOffset: options.columnOffset
    });

    wrapMap.set(this, wrap);
    linkingStatusMap.set(this, 'unlinked');

    Object.defineProperties(this, {
      url: { value: url, enumerable: true },
      context: { value: context, enumerable: true },
    });
  }

  get linkingStatus() {
    return linkingStatusMap.get(this);
  }

  get status() {
    return STATUS_MAP[wrapMap.get(this).getStatus()];
  }

  get namespace() {
    const wrap = wrapMap.get(this);
    if (wrap.getStatus() < kInstantiated)
      throw new errors.Error('ERR_VM_MODULE_STATUS',
                             'must not be uninstantiated or instantiating');
    return wrap.namespace();
  }

  get dependencySpecifiers() {
    let deps = dependencyCacheMap.get(this);
    if (deps !== undefined)
      return deps;

    deps = wrapMap.get(this).getStaticDependencySpecifiers();
    Object.freeze(deps);
    dependencyCacheMap.set(this, deps);
    return deps;
  }

  get error() {
    const wrap = wrapMap.get(this);
    if (wrap.getStatus() !== kErrored)
      throw new errors.Error('ERR_VM_MODULE_STATUS', 'must be errored');
    return wrap.getError();
  }

  async link(linker) {
    if (typeof linker !== 'function')
      throw new errors.TypeError(
        'ERR_INVALID_ARG_TYPE', 'linker', 'function', linker);
    if (linkingStatusMap.get(this) !== 'unlinked')
      throw new errors.Error('ERR_VM_MODULE_ALREADY_LINKED');
    const wrap = wrapMap.get(this);
    if (wrap.getStatus() !== kUninstantiated)
      throw new errors.Error('ERR_VM_MODULE_STATUS', 'must be uninstantiated');
    linkingStatusMap.set(this, 'linking');
    const promises = [];
    wrap.link((specifier) => {
      const p = (async () => {
        const m = await linker(this, specifier);
        if (!m || !wrapMap.has(m))
          throw new errors.Error('ERR_VM_MODULE_NOT_MODULE');
        if (m.context !== this.context)
          throw new errors.Error('ERR_VM_MODULE_DIFFERENT_CONTEXT');
        const childLinkingStatus = linkingStatusMap.get(m);
        if (childLinkingStatus === 'errored')
          throw new errors.Error('ERR_VM_MODULE_LINKING_ERRORED');
        if (childLinkingStatus === 'unlinked')
          await m.link(linker);
        return wrapMap.get(m);
      })();
      promises.push(p);
      return p;
    });
    try {
      await Promise.all(promises);
      linkingStatusMap.set(this, 'linked');
    } catch (err) {
      linkingStatusMap.set(this, 'errored');
      throw err;
    }
  }

  instantiate() {
    const wrap = wrapMap.get(this);
    const status = wrap.getStatus();
    if (status === kInstantiating || status === kEvaluating)
      throw new errors.Error(
        'ERR_VM_MODULE_STATUS', 'must not be instantiating or evaluating');
    if (linkingStatusMap.get(this) !== 'linked')
      throw new errors.Error('ERR_VM_MODULE_NOT_LINKED');
    wrap.instantiate();
  }

  async evaluate(options) {
    const wrap = wrapMap.get(this);
    const status = wrap.getStatus();
    if (status !== kInstantiated &&
        status !== kEvaluated &&
        status !== kErrored) {
      throw new errors.Error(
        'ERR_VM_MODULE_STATUS',
        'must be one of instantiated, evaluated, and errored');
    }
    const result = wrap.evaluate(options);
    return { result, __proto__: null };
  }

  [customInspectSymbol](depth, options) {
    let ctor = getConstructorOf(this);
    ctor = ctor === null ? Module : ctor;

    if (typeof depth === 'number' && depth < 0)
      return options.stylize(`[${ctor.name}]`, 'special');

    const o = Object.create({ constructor: ctor });
    o.status = this.status;
    o.linkingStatus = this.linkingStatus;
    o.url = this.url;
    o.context = this.context;
    return require('util').inspect(o, options);
  }
}

module.exports = {
  Module
};
