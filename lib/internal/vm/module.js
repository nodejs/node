'use strict';

const {
  Array,
  ArrayIsArray,
  ArrayPrototypeForEach,
  ArrayPrototypeIndexOf,
  ArrayPrototypeMap,
  ArrayPrototypeSome,
  ObjectDefineProperty,
  ObjectFreeze,
  ObjectGetPrototypeOf,
  ObjectPrototypeHasOwnProperty,
  ObjectSetPrototypeOf,
  PromisePrototypeThen,
  PromiseResolve,
  ReflectApply,
  SafePromiseAllReturnArrayLike,
  Symbol,
  SymbolToStringTag,
  TypeError,
} = primordials;

const assert = require('internal/assert');
const {
  isModuleNamespaceObject,
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
  validateBuffer,
  validateFunction,
  validateInt32,
  validateObject,
  validateUint32,
  validateString,
  validateThisInternalField,
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
  kSourcePhase,
  kEvaluationPhase,
} = binding;

const STATUS_MAP = {
  __proto__: null,
  [kUninstantiated]: 'unlinked',
  [kInstantiating]: 'linking',
  [kInstantiated]: 'linked',
  [kEvaluating]: 'evaluating',
  [kEvaluated]: 'evaluated',
  [kErrored]: 'errored',
};

const PHASE_MAP = {
  __proto__: null,
  [kSourcePhase]: 'source',
  [kEvaluationPhase]: 'evaluation',
};

let globalModuleId = 0;
const defaultModuleName = 'vm:module';

const kWrap = Symbol('kWrap');
const kContext = Symbol('kContext');
const kPerContextModuleId = Symbol('kPerContextModuleId');
const kLink = Symbol('kLink');

const { isContext } = require('internal/vm');

function isModule(object) {
  if (typeof object !== 'object' || object === null || !ObjectPrototypeHasOwnProperty(object, kWrap)) {
    return false;
  }
  return true;
}

function phaseEnumToPhaseName(phase) {
  const phaseName = PHASE_MAP[phase];
  assert(phaseName !== undefined, `Invalid phase value: ${phase}`);
  return phaseName;
}

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
    } else {
      assert(syntheticEvaluationSteps);
      this[kWrap] = new ModuleWrap(identifier, context,
                                   syntheticExportNames,
                                   syntheticEvaluationSteps);
    }

    this[kContext] = context;
  }

  get identifier() {
    validateThisInternalField(this, kWrap, 'Module');
    return this[kWrap].url;
  }

  get context() {
    validateThisInternalField(this, kWrap, 'Module');
    return this[kContext];
  }

  get namespace() {
    validateThisInternalField(this, kWrap, 'Module');
    if (this[kWrap].getStatus() < kInstantiated) {
      throw new ERR_VM_MODULE_STATUS('must not be unlinked or linking');
    }
    return this[kWrap].getNamespace();
  }

  get status() {
    validateThisInternalField(this, kWrap, 'Module');
    return STATUS_MAP[this[kWrap].getStatus()];
  }

  get error() {
    validateThisInternalField(this, kWrap, 'Module');
    if (this[kWrap].getStatus() !== kErrored) {
      throw new ERR_VM_MODULE_STATUS('must be errored');
    }
    return this[kWrap].getError();
  }

  async link(linker) {
    validateThisInternalField(this, kWrap, 'Module');
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
    validateThisInternalField(this, kWrap, 'Module');
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
        'must be one of linked, evaluated, or errored',
      );
    }
    await this[kWrap].evaluate(timeout, breakOnSigint);
  }

  [customInspectSymbol](depth, options) {
    validateThisInternalField(this, kWrap, 'Module');
    if (typeof depth === 'number' && depth < 0)
      return this;

    const constructor = getConstructorOf(this) || Module;
    const o = { __proto__: { constructor } };
    o.status = this.status;
    o.identifier = this.identifier;
    o.context = this.context;

    ObjectSetPrototypeOf(o, ObjectGetPrototypeOf(this));
    ObjectDefineProperty(o, SymbolToStringTag, {
      __proto__: null,
      value: constructor.name,
      configurable: true,
    });

    // Lazy to avoid circular dependency
    const { inspect } = require('internal/util/inspect');
    return inspect(o, { ...options, customInspect: false });
  }
}

const kNoError = Symbol('kNoError');

class SourceTextModule extends Module {
  #error = kNoError;
  #statusOverride;

  #moduleRequests;
  #dependencySpecifiers;

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

    if (initializeImportMeta !== undefined) {
      validateFunction(initializeImportMeta, 'options.initializeImportMeta');
    }

    if (importModuleDynamically !== undefined) {
      validateFunction(importModuleDynamically, 'options.importModuleDynamically');
    }

    if (cachedData !== undefined) {
      validateBuffer(cachedData, 'options.cachedData');
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

    const registry = {
      __proto__: null,
      initializeImportMeta: options.initializeImportMeta,
      importModuleDynamically: options.importModuleDynamically ?
        importModuleDynamicallyWrap(options.importModuleDynamically) :
        undefined,
    };
    // This will take precedence over the referrer as the object being
    // passed into the callbacks.
    registry.callbackReferrer = this;
    const { registerModule } = require('internal/modules/esm/utils');
    registerModule(this[kWrap], registry);

    this.#moduleRequests = ObjectFreeze(ArrayPrototypeMap(this[kWrap].getModuleRequests(), (request) => {
      return ObjectFreeze({
        __proto__: null,
        specifier: request.specifier,
        attributes: request.attributes,
        phase: phaseEnumToPhaseName(request.phase),
      });
    }));
  }

  async [kLink](linker) {
    this.#statusOverride = 'linking';

    // Iterates the module requests and links with the linker.
    // Modules should be aligned with the moduleRequests array in order.
    const modulePromises = Array(this.#moduleRequests.length);
    // Iterates with index to avoid calling into userspace with `Symbol.iterator`.
    for (let idx = 0; idx < this.#moduleRequests.length; idx++) {
      const { specifier, attributes } = this.#moduleRequests[idx];

      const linkerResult = linker(specifier, this, {
        attributes,
        assert: attributes,
      });
      const modulePromise = PromisePrototypeThen(
        PromiseResolve(linkerResult), async (module) => {
          if (!isModule(module)) {
            throw new ERR_VM_MODULE_NOT_MODULE();
          }
          if (module.context !== this.context) {
            throw new ERR_VM_MODULE_DIFFERENT_CONTEXT();
          }
          if (module.status === 'errored') {
            throw new ERR_VM_MODULE_LINK_FAILURE(`request for '${specifier}' resolved to an errored module`, module.error);
          }
          if (module.status === 'unlinked') {
            await module[kLink](linker);
          }
          return module[kWrap];
        });
      modulePromises[idx] = modulePromise;
    }

    try {
      const modules = await SafePromiseAllReturnArrayLike(modulePromises);
      this[kWrap].link(modules);
    } catch (e) {
      this.#error = e;
      throw e;
    } finally {
      this.#statusOverride = undefined;
    }
  }

  get dependencySpecifiers() {
    this.#dependencySpecifiers ??= ObjectFreeze(
      ArrayPrototypeMap(this.#moduleRequests, (request) => request.specifier));
    return this.#dependencySpecifiers;
  }

  get moduleRequests() {
    return this.#moduleRequests;
  }

  get status() {
    if (this.#error !== kNoError) {
      return 'errored';
    }
    if (this.#statusOverride) {
      return this.#statusOverride;
    }
    return super.status;
  }

  get error() {
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
  }

  [kLink]() {
    /** nothing to do for synthetic modules */
  }

  setExport(name, value) {
    validateThisInternalField(this, kWrap, 'SyntheticModule');
    validateString(name, 'name');
    if (this[kWrap].getStatus() < kInstantiated) {
      throw new ERR_VM_MODULE_STATUS('must be linked');
    }
    this[kWrap].setExport(name, value);
  }
}

/**
 * @callback ImportModuleDynamicallyCallback
 * @param {string} specifier
 * @param {ModuleWrap|ContextifyScript|Function|vm.Module} callbackReferrer
 * @param {Record<string, string>} attributes
 * @param {number} phase
 * @returns { Promise<void> }
 */

/**
 * @param {import('internal/vm').VmImportModuleDynamicallyCallback} importModuleDynamically
 * @returns {ImportModuleDynamicallyCallback}
 */
function importModuleDynamicallyWrap(importModuleDynamically) {
  const importModuleDynamicallyWrapper = async (specifier, referrer, attributes, phase) => {
    const phaseName = phaseEnumToPhaseName(phase);
    const m = await ReflectApply(
      importModuleDynamically,
      this,
      [specifier, referrer, attributes, phaseName],
    );
    if (isModuleNamespaceObject(m)) {
      if (phase === kSourcePhase) throw new ERR_VM_MODULE_NOT_MODULE();
      return m;
    }
    if (!isModule(m)) {
      throw new ERR_VM_MODULE_NOT_MODULE();
    }
    if (m.status === 'errored') {
      throw m.error;
    }
    if (phase === kSourcePhase)
      return m[kWrap].getModuleSourceObject();
    return m.namespace;
  };
  return importModuleDynamicallyWrapper;
}

module.exports = {
  Module,
  SourceTextModule,
  SyntheticModule,
  importModuleDynamicallyWrap,
};
