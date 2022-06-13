'use strict';

const assert = require('internal/assert');
const {
  ArrayIsArray,
  ArrayPrototypeForEach,
  ArrayPrototypeIndexOf,
  ArrayPrototypeSome,
  ObjectCreate,
  ObjectDefineProperty,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  PromiseAll,
  ReflectApply,
  SafeWeakMap,
  Symbol,
  SymbolToStringTag,
  TypeError,
} = primordials;

const { isContext } = internalBinding('contextify');
const {
  isModuleNamespaceObject,
  isArrayBufferView,
} = require('internal/util/types');
const {
  customInspectSymbol,
  emitExperimentalWarning,
  getConstructorOf,
  kEmptyObject,
} = require('internal/util');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_VM_MODULE_ALREADY_LINKED,
  ERR_VM_MODULE_DIFFERENT_CONTEXT,
  ERR_VM_MODULE_CANNOT_CREATE_CACHED_DATA,
  ERR_VM_MODULE_LINK_FAILURE,
  ERR_VM_MODULE_NOT_MODULE,
  ERR_VM_MODULE_STATUS,
} = require('internal/errors').codes;
const {
  validateBoolean,
  validateFunction,
  validateInt32,
  validateObject,
  validateUint32,
  validateString,
} = require('internal/validators');

const binding = internalBinding('module_wrap');
const {
  ModuleWrap,
  kUninstantiated,
  kInstantiating,
  kInstantiated,
  kEvaluating,
  kEvaluated,
  kErrored,
} = binding;

const STATUS_MAP = {
  [kUninstantiated]: 'unlinked',
  [kInstantiating]: 'linking',
  [kInstantiated]: 'linked',
  [kEvaluating]: 'evaluating',
  [kEvaluated]: 'evaluated',
  [kErrored]: 'errored',
};

let globalModuleId = 0;
const defaultModuleName = 'vm:module';
const wrapToModuleMap = new SafeWeakMap();

const kWrap = Symbol('kWrap');
const kContext = Symbol('kContext');
const kPerContextModuleId = Symbol('kPerContextModuleId');
const kLink = Symbol('kLink');

class Module {
  constructor(options) {
    emitExperimentalWarning('VM Modules');

    if (new.target === Module) {
      // eslint-disable-next-line no-restricted-syntax
      throw new TypeError('Module is not a constructor');
    }

    const {
      context,
      sourceText,
      syntheticExportNames,
      syntheticEvaluationSteps,
    } = options;

    if (context !== undefined) {
      validateObject(context, 'context');
      if (!isContext(context)) {
        throw new ERR_INVALID_ARG_TYPE('options.context', 'vm.Context',
                                       context);
      }
    }

    let { identifier } = options;
    if (identifier !== undefined) {
      validateString(identifier, 'options.identifier');
    } else if (context === undefined) {
      identifier = `${defaultModuleName}(${globalModuleId++})`;
    } else if (context[kPerContextModuleId] !== undefined) {
      const curId = context[kPerContextModuleId];
      identifier = `${defaultModuleName}(${curId})`;
      context[kPerContextModuleId] += 1;
    } else {
      identifier = `${defaultModuleName}(0)`;
      ObjectDefineProperty(context, kPerContextModuleId, {
        __proto__: null,
        value: 1,
        writable: true,
        enumerable: false,
        configurable: true,
      });
    }

    if (sourceText !== undefined) {
      this[kWrap] = new ModuleWrap(identifier, context, sourceText,
                                   options.lineOffset, options.columnOffset,
                                   options.cachedData);

      binding.callbackMap.set(this[kWrap], {
        initializeImportMeta: options.initializeImportMeta,
        importModuleDynamically: options.importModuleDynamically ?
          importModuleDynamicallyWrap(options.importModuleDynamically) :
          undefined,
      });
    } else {
      assert(syntheticEvaluationSteps);
      this[kWrap] = new ModuleWrap(identifier, context,
                                   syntheticExportNames,
                                   syntheticEvaluationSteps);
    }

    wrapToModuleMap.set(this[kWrap], this);

    this[kContext] = context;
  }

  get identifier() {
    if (this[kWrap] === undefined) {
      throw new ERR_VM_MODULE_NOT_MODULE();
    }
    return this[kWrap].url;
  }

  get context() {
    if (this[kWrap] === undefined) {
      throw new ERR_VM_MODULE_NOT_MODULE();
    }
    return this[kContext];
  }

  get namespace() {
    if (this[kWrap] === undefined) {
      throw new ERR_VM_MODULE_NOT_MODULE();
    }
    if (this[kWrap].getStatus() < kInstantiated) {
      throw new ERR_VM_MODULE_STATUS('must not be unlinked or linking');
    }
    return this[kWrap].getNamespace();
  }

  get status() {
    if (this[kWrap] === undefined) {
      throw new ERR_VM_MODULE_NOT_MODULE();
    }
    return STATUS_MAP[this[kWrap].getStatus()];
  }

  get error() {
    if (this[kWrap] === undefined) {
      throw new ERR_VM_MODULE_NOT_MODULE();
    }
    if (this[kWrap].getStatus() !== kErrored) {
      throw new ERR_VM_MODULE_STATUS('must be errored');
    }
    return this[kWrap].getError();
  }

  async link(linker) {
    if (this[kWrap] === undefined) {
      throw new ERR_VM_MODULE_NOT_MODULE();
    }
    validateFunction(linker, 'linker');
    if (this.status === 'linked') {
      throw new ERR_VM_MODULE_ALREADY_LINKED();
    }
    if (this.status !== 'unlinked') {
      throw new ERR_VM_MODULE_STATUS('must be unlinked');
    }
    await this[kLink](linker);
    this[kWrap].instantiate();
  }

  async evaluate(options = kEmptyObject) {
    if (this[kWrap] === undefined) {
      throw new ERR_VM_MODULE_NOT_MODULE();
    }

    validateObject(options, 'options');

    let timeout = options.timeout;
    if (timeout === undefined) {
      timeout = -1;
    } else {
      validateUint32(timeout, 'options.timeout', true);
    }
    const { breakOnSigint = false } = options;
    validateBoolean(breakOnSigint, 'options.breakOnSigint');
    const status = this[kWrap].getStatus();
    if (status !== kInstantiated &&
        status !== kEvaluated &&
        status !== kErrored) {
      throw new ERR_VM_MODULE_STATUS(
        'must be one of linked, evaluated, or errored'
      );
    }
    await this[kWrap].evaluate(timeout, breakOnSigint);
  }

  [customInspectSymbol](depth, options) {
    if (this[kWrap] === undefined) {
      throw new ERR_VM_MODULE_NOT_MODULE();
    }
    if (typeof depth === 'number' && depth < 0)
      return this;

    const constructor = getConstructorOf(this) || Module;
    const o = ObjectCreate({ constructor });
    o.status = this.status;
    o.identifier = this.identifier;
    o.context = this.context;

    ObjectSetPrototypeOf(o, ObjectGetPrototypeOf(this));
    ObjectDefineProperty(o, SymbolToStringTag, {
      __proto__: null,
      value: constructor.name,
      configurable: true
    });

    // Lazy to avoid circular dependency
    const { inspect } = require('internal/util/inspect');
    return inspect(o, { ...options, customInspect: false });
  }
}

const kDependencySpecifiers = Symbol('kDependencySpecifiers');
const kNoError = Symbol('kNoError');

class SourceTextModule extends Module {
  #error = kNoError;
  #statusOverride;

  constructor(sourceText, options = kEmptyObject) {
    validateString(sourceText, 'sourceText');
    validateObject(options, 'options');

    const {
      lineOffset = 0,
      columnOffset = 0,
      initializeImportMeta,
      importModuleDynamically,
      context,
      identifier,
      cachedData,
    } = options;

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
        'options.importModuleDynamically', 'function',
        importModuleDynamically);
    }

    if (cachedData !== undefined && !isArrayBufferView(cachedData)) {
      throw new ERR_INVALID_ARG_TYPE(
        'options.cachedData',
        ['Buffer', 'TypedArray', 'DataView'],
        cachedData
      );
    }

    super({
      sourceText,
      context,
      identifier,
      lineOffset,
      columnOffset,
      cachedData,
      initializeImportMeta,
      importModuleDynamically,
    });

    this[kLink] = async (linker) => {
      this.#statusOverride = 'linking';

      const promises = this[kWrap].link(async (identifier, assert) => {
        const module = await linker(identifier, this, { assert });
        if (module[kWrap] === undefined) {
          throw new ERR_VM_MODULE_NOT_MODULE();
        }
        if (module.context !== this.context) {
          throw new ERR_VM_MODULE_DIFFERENT_CONTEXT();
        }
        if (module.status === 'errored') {
          throw new ERR_VM_MODULE_LINK_FAILURE(`request for '${identifier}' resolved to an errored module`, module.error);
        }
        if (module.status === 'unlinked') {
          await module[kLink](linker);
        }
        return module[kWrap];
      });

      try {
        if (promises !== undefined) {
          await PromiseAll(promises);
        }
      } catch (e) {
        this.#error = e;
        throw e;
      } finally {
        this.#statusOverride = undefined;
      }
    };

    this[kDependencySpecifiers] = undefined;
  }

  get dependencySpecifiers() {
    if (this[kWrap] === undefined) {
      throw new ERR_VM_MODULE_NOT_MODULE();
    }
    if (this[kDependencySpecifiers] === undefined) {
      this[kDependencySpecifiers] = this[kWrap].getStaticDependencySpecifiers();
    }
    return this[kDependencySpecifiers];
  }

  get status() {
    if (this[kWrap] === undefined) {
      throw new ERR_VM_MODULE_NOT_MODULE();
    }
    if (this.#error !== kNoError) {
      return 'errored';
    }
    if (this.#statusOverride) {
      return this.#statusOverride;
    }
    return super.status;
  }

  get error() {
    if (this[kWrap] === undefined) {
      throw new ERR_VM_MODULE_NOT_MODULE();
    }
    if (this.#error !== kNoError) {
      return this.#error;
    }
    return super.error;
  }

  createCachedData() {
    const { status } = this;
    if (status === 'evaluating' ||
        status === 'evaluated' ||
        status === 'errored') {
      throw new ERR_VM_MODULE_CANNOT_CREATE_CACHED_DATA();
    }
    return this[kWrap].createCachedData();
  }
}

class SyntheticModule extends Module {
  constructor(exportNames, evaluateCallback, options = kEmptyObject) {
    if (!ArrayIsArray(exportNames) ||
      ArrayPrototypeSome(exportNames, (e) => typeof e !== 'string')) {
      throw new ERR_INVALID_ARG_TYPE('exportNames',
                                     'Array of unique strings',
                                     exportNames);
    } else {
      ArrayPrototypeForEach(exportNames, (name, i) => {
        if (ArrayPrototypeIndexOf(exportNames, name, i + 1) !== -1) {
          throw new ERR_INVALID_ARG_VALUE(`exportNames.${name}`,
                                          name,
                                          'is duplicated');
        }
      });
    }
    validateFunction(evaluateCallback, 'evaluateCallback');

    validateObject(options, 'options');

    const { context, identifier } = options;

    super({
      syntheticExportNames: exportNames,
      syntheticEvaluationSteps: evaluateCallback,
      context,
      identifier,
    });

    this[kLink] = () => this[kWrap].link(() => {
      assert.fail('link callback should not be called');
    });
  }

  setExport(name, value) {
    if (this[kWrap] === undefined) {
      throw new ERR_VM_MODULE_NOT_MODULE();
    }
    validateString(name, 'name');
    if (this[kWrap].getStatus() < kInstantiated) {
      throw new ERR_VM_MODULE_STATUS('must be linked');
    }
    this[kWrap].setExport(name, value);
  }
}

function importModuleDynamicallyWrap(importModuleDynamically) {
  const importModuleDynamicallyWrapper = async (...args) => {
    const m = await ReflectApply(importModuleDynamically, this, args);
    if (isModuleNamespaceObject(m)) {
      return m;
    }
    if (!m || m[kWrap] === undefined) {
      throw new ERR_VM_MODULE_NOT_MODULE();
    }
    if (m.status === 'errored') {
      throw m.error;
    }
    return m.namespace;
  };
  return importModuleDynamicallyWrapper;
}

module.exports = {
  Module,
  SourceTextModule,
  SyntheticModule,
  importModuleDynamicallyWrap,
  getModuleFromWrap: (wrap) => wrapToModuleMap.get(wrap),
};
