'use strict';

const { isModuleNamespaceObject } = require('util').types;
const { URL } = require('internal/url');
const { isContext } = internalBinding('contextify');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_VM_MODULE_ALREADY_LINKED,
  ERR_VM_MODULE_DIFFERENT_CONTEXT,
  ERR_VM_MODULE_LINKING_ERRORED,
  ERR_VM_MODULE_NOT_LINKED,
  ERR_VM_MODULE_NOT_MODULE,
  ERR_VM_MODULE_STATUS,
} = require('internal/errors').codes;
const {
  getConstructorOf,
  customInspectSymbol,
  emitExperimentalWarning
} = require('internal/util');
const { SafePromise } = primordials;
const {
  validateInt32,
  validateUint32,
  validateString
} = require('internal/validators');

const {
  ModuleWrap,
  callbackMap,
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
// ModuleWrap -> vm.SourceTextModule
const wrapToModuleMap = new WeakMap();
const defaultModuleName = 'vm:module';

// TODO(devsnek): figure out AbstractModule class or protocol
class SourceTextModule {
  constructor(src, options = {}) {
    emitExperimentalWarning('vm.SourceTextModule');

    validateString(src, 'src');
    if (typeof options !== 'object' || options === null)
      throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);

    const {
      context,
      lineOffset = 0,
      columnOffset = 0,
      initializeImportMeta,
      importModuleDynamically,
    } = options;

    if (context !== undefined) {
      if (typeof context !== 'object' || context === null) {
        throw new ERR_INVALID_ARG_TYPE('options.context', 'Object', context);
      }
      if (!isContext(context)) {
        throw new ERR_INVALID_ARG_TYPE('options.context', 'vm.Context',
                                       context);
      }
    }

    let { url } = options;
    if (url !== undefined) {
      validateString(url, 'options.url');
      url = new URL(url).href;
    } else if (context === undefined) {
      url = `${defaultModuleName}(${globalModuleId++})`;
    } else if (perContextModuleId.has(context)) {
      const curId = perContextModuleId.get(context);
      url = `${defaultModuleName}(${curId})`;
      perContextModuleId.set(context, curId + 1);
    } else {
      url = `${defaultModuleName}(0)`;
      perContextModuleId.set(context, 1);
    }

    validateInt32(lineOffset, 'options.lineOffset');
    validateInt32(columnOffset, 'options.columnOffset');

    if (initializeImportMeta !== undefined &&
        typeof initializeImportMeta !== 'function') {
      throw new ERR_INVALID_ARG_TYPE(
        'options.initializeImportMeta', 'function', initializeImportMeta);
    }

    if (importModuleDynamically !== undefined &&
        typeof importModuleDynamically !== 'function') {
      throw new ERR_INVALID_ARG_TYPE(
        'options.importModuleDynamically', 'function', importModuleDynamically);
    }

    const wrap = new ModuleWrap(src, url, context, lineOffset, columnOffset);
    wrapMap.set(this, wrap);
    linkingStatusMap.set(this, 'unlinked');
    wrapToModuleMap.set(wrap, this);

    callbackMap.set(wrap, {
      initializeImportMeta,
      importModuleDynamically: importModuleDynamically ? async (...args) => {
        const m = await importModuleDynamically(...args);
        if (isModuleNamespaceObject(m)) {
          return m;
        }
        if (!m || !wrapMap.has(m))
          throw new ERR_VM_MODULE_NOT_MODULE();
        const childLinkingStatus = linkingStatusMap.get(m);
        if (childLinkingStatus === 'errored')
          throw m.error;
        return m.namespace;
      } : undefined,
    });

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
      throw new ERR_VM_MODULE_STATUS(
        'must not be uninstantiated or instantiating'
      );
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
      throw new ERR_VM_MODULE_STATUS('must be errored');
    return wrap.getError();
  }

  async link(linker) {
    if (typeof linker !== 'function')
      throw new ERR_INVALID_ARG_TYPE('linker', 'function', linker);
    if (linkingStatusMap.get(this) !== 'unlinked')
      throw new ERR_VM_MODULE_ALREADY_LINKED();
    const wrap = wrapMap.get(this);
    if (wrap.getStatus() !== kUninstantiated)
      throw new ERR_VM_MODULE_STATUS('must be uninstantiated');

    linkingStatusMap.set(this, 'linking');

    const promises = wrap.link(async (specifier) => {
      const m = await linker(specifier, this);
      if (!m || !wrapMap.has(m))
        throw new ERR_VM_MODULE_NOT_MODULE();
      if (m.context !== this.context)
        throw new ERR_VM_MODULE_DIFFERENT_CONTEXT();
      const childLinkingStatus = linkingStatusMap.get(m);
      if (childLinkingStatus === 'errored')
        throw new ERR_VM_MODULE_LINKING_ERRORED();
      if (childLinkingStatus === 'unlinked')
        await m.link(linker);
      return wrapMap.get(m);
    });

    try {
      if (promises !== undefined)
        await SafePromise.all(promises);
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
      throw new ERR_VM_MODULE_STATUS('must not be instantiating or evaluating');
    if (linkingStatusMap.get(this) !== 'linked')
      throw new ERR_VM_MODULE_NOT_LINKED();
    wrap.instantiate();
  }

  async evaluate(options = {}) {
    if (typeof options !== 'object' || options === null) {
      throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
    }

    let timeout = options.timeout;
    if (timeout === undefined) {
      timeout = -1;
    } else {
      validateUint32(timeout, 'options.timeout', true);
    }

    const { breakOnSigint = false } = options;
    if (typeof breakOnSigint !== 'boolean') {
      throw new ERR_INVALID_ARG_TYPE('options.breakOnSigint', 'boolean',
                                     breakOnSigint);
    }

    const wrap = wrapMap.get(this);
    const status = wrap.getStatus();
    if (status !== kInstantiated &&
        status !== kEvaluated &&
        status !== kErrored) {
      throw new ERR_VM_MODULE_STATUS(
        'must be one of instantiated, evaluated, and errored'
      );
    }
    const result = wrap.evaluate(timeout, breakOnSigint);
    return { result, __proto__: null };
  }

  [customInspectSymbol](depth, options) {
    let ctor = getConstructorOf(this);
    ctor = ctor === null ? SourceTextModule : ctor;

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
  SourceTextModule,
  wrapToModuleMap,
  wrapMap,
  linkingStatusMap,
};
