'use strict';

const {
  Array,
  ArrayPrototypeFind,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  FunctionPrototype,
  ObjectSetPrototypeOf,
  PromisePrototypeThen,
  PromiseResolve,
  RegExpPrototypeExec,
  RegExpPrototypeSymbolReplace,
  SafePromiseAllReturnArrayLike,
  SafePromiseAllReturnVoid,
  SafeSet,
  StringPrototypeIncludes,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
  globalThis,
} = primordials;
let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});

const {
  ModuleWrap,
  kErrored,
  kEvaluated,
  kEvaluating,
  kEvaluationPhase,
  kInstantiated,
  kUninstantiated,
} = internalBinding('module_wrap');
const {
  privateSymbols: {
    entry_point_module_private_symbol,
  },
} = internalBinding('util');
/**
 * @typedef {import('./utils.js').ModuleRequestType} ModuleRequestType
 */
const { decorateErrorStack } = require('internal/util');
const { isPromise } = require('internal/util/types');
const {
  getSourceMapsSupport,
} = require('internal/source_map/source_map_cache');
const assert = require('internal/assert');
const resolvedPromise = PromiseResolve();
const {
  setHasStartedUserESMExecution,
  urlToFilename,
} = require('internal/modules/helpers');
const { getOptionValue } = require('internal/options');
const noop = FunctionPrototype;
const {
  ERR_REQUIRE_ASYNC_MODULE,
} = require('internal/errors').codes;
let hasPausedEntry = false;

const CJSGlobalLike = [
  'require',
  'module',
  'exports',
  '__filename',
  '__dirname',
];
const findCommonJSGlobalLikeNotDefinedError = (errorMessage) =>
  ArrayPrototypeFind(
    CJSGlobalLike,
    (globalLike) => errorMessage === `${globalLike} is not defined`,
  );


/**
 *
 * @param {Error} e
 * @param {string} url
 * @returns {void}
 */
const explainCommonJSGlobalLikeNotDefinedError = (e, url, hasTopLevelAwait) => {
  const notDefinedGlobalLike = e?.name === 'ReferenceError' && findCommonJSGlobalLikeNotDefinedError(e.message);

  if (notDefinedGlobalLike) {
    if (hasTopLevelAwait) {
      let advice;
      switch (notDefinedGlobalLike) {
        case 'require':
          advice = 'replace require() with import';
          break;
        case 'module':
        case 'exports':
          advice = 'use export instead of module.exports/exports';
          break;
        case '__filename':
          advice = 'use import.meta.filename instead';
          break;
        case '__dirname':
          advice = 'use import.meta.dirname instead';
          break;
      }

      e.message = `Cannot determine intended module format because both '${notDefinedGlobalLike}' and top-level await are present. If the code is intended to be CommonJS, wrap await in an async function. If the code is intended to be an ES module, ${advice}.`;
      e.code = 'ERR_AMBIGUOUS_MODULE_SYNTAX';
      return;
    }

    e.message += ' in ES module scope';

    if (StringPrototypeStartsWith(e.message, 'require ')) {
      e.message += ', you can use import instead';
    }

    const packageConfig =
      StringPrototypeStartsWith(url, 'file://') &&
        RegExpPrototypeExec(/\.js(\?[^#]*)?(#.*)?$/, url) !== null &&
        require('internal/modules/package_json_reader')
          .getPackageScopeConfig(url);
    if (packageConfig.type === 'module') {
      e.message +=
        '\nThis file is being treated as an ES module because it has a ' +
        `'.js' file extension and '${packageConfig.pjsonPath}' contains ` +
        '"type": "module". To treat it as a CommonJS script, rename it ' +
        'to use the \'.cjs\' file extension.';
    }
  }
};

class ModuleJobBase {
  constructor(loader, url, importAttributes, phase, isMain, inspectBrk) {
    assert(typeof phase === 'number');
    this.loader = loader;
    this.importAttributes = importAttributes;
    this.phase = phase;
    this.isMain = isMain;
    this.inspectBrk = inspectBrk;

    this.url = url;
  }

  /**
   * Synchronously link the module and its dependencies.
   * @param {ModuleRequestType} requestType Type of the module request.
   * @returns {ModuleJobBase[]}
   */
  syncLink(requestType) {
    // Store itself into the cache first before linking in case there are circular
    // references in the linking.
    this.loader.loadCache.set(this.url, this.type, this);
    const moduleRequests = this.module.getModuleRequests();
    // Modules should be aligned with the moduleRequests array in order.
    const modules = Array(moduleRequests.length);
    const evaluationDepJobs = [];
    this.commonJsDeps = Array(moduleRequests.length);
    try {
      for (let idx = 0; idx < moduleRequests.length; idx++) {
        const request = moduleRequests[idx];
        // TODO(joyeecheung): split this into two iterators, one for resolving and one for loading so
        // that hooks can pre-fetch sources off-thread.
        const job = this.loader.getOrCreateModuleJob(this.url, request, requestType);
        debug(`ModuleJobBase.syncLink() ${this.url} -> ${request.specifier}`, job);
        assert(!isPromise(job));
        assert(job.module instanceof ModuleWrap);
        if (request.phase === kEvaluationPhase) {
          ArrayPrototypePush(evaluationDepJobs, job);
        }
        modules[idx] = job.module;
        this.commonJsDeps[idx] = job.module.isCommonJS;
      }
      this.module.link(modules);
    } finally {
      // Restore it - if it succeeds, we'll reset in the caller; Otherwise it's
      // not cached and if the error is caught, subsequent attempt would still fail.
      this.loader.loadCache.delete(this.url, this.type);
    }

    return evaluationDepJobs;
  }

  /**
   * Ensure that this ModuleJob is moving towards the required phase
   * (does not necessarily mean it is ready at that phase - run does that)
   * @param {number} phase
   */
  ensurePhase(phase, requestType) {
    if (this.phase < phase) {
      this.phase = phase;
      this.linked = this.link(requestType);
      if (isPromise(this.linked)) {
        PromisePrototypeThen(this.linked, undefined, noop);
      }
    }
  }
}

/* A ModuleJob tracks the loading of a single Module, and the ModuleJobs of
 * its dependencies, over time. */
class ModuleJob extends ModuleJobBase {
  /**
   * @param {ModuleLoader} loader The ESM loader.
   * @param {string} url URL of the module to be wrapped in ModuleJob.
   * @param {ImportAttributes} importAttributes Import attributes from the import statement.
   * @param {ModuleWrap|Promise<ModuleWrap>} moduleOrModulePromise Translated ModuleWrap for the module.
   * @param {number} phase The phase to load the module to.
   * @param {boolean} isMain Whether the module is the entry point.
   * @param {boolean} inspectBrk Whether this module should be evaluated with the
   *   first line paused in the debugger (because --inspect-brk is passed).
   * @param {ModuleRequestType} requestType Type of the module request.
   */
  constructor(loader, url, importAttributes = { __proto__: null }, moduleOrModulePromise,
              phase = kEvaluationPhase, isMain, inspectBrk, requestType) {
    super(loader, url, importAttributes, phase, isMain, inspectBrk);

    // Expose the promise to the ModuleWrap directly for linking below.
    if (isPromise(moduleOrModulePromise)) {
      this.modulePromise = moduleOrModulePromise;
    } else {
      this.module = moduleOrModulePromise;
      this.modulePromise = PromiseResolve(moduleOrModulePromise);
    }

    if (this.phase === kEvaluationPhase) {
      // Promise for the list of all dependencyJobs.
      this.linked = this.link(requestType);
      // This promise is awaited later anyway, so silence
      // 'unhandled rejection' warnings.
      if (isPromise(this.linked)) {
        PromisePrototypeThen(this.linked, undefined, noop);
      }
    }

    // instantiated == deep dependency jobs wrappers are instantiated,
    // and module wrapper is instantiated.
    this.instantiated = undefined;
  }

  /**
   * @param {ModuleRequestType} requestType Type of the module request.
   * @returns {ModuleJobBase[]|Promise<ModuleJobBase[]>}
   */
  link(requestType) {
    if (this.loader.isForAsyncLoaderHookWorker) {
      return this.#asyncLink(requestType);
    }
    return this.syncLink(requestType);
  }

  /**
   * @param {ModuleRequestType} requestType Type of the module request.
   * @returns {Promise<ModuleJobBase[]>}
   */
  async #asyncLink(requestType) {
    assert(this.loader.isForAsyncLoaderHookWorker);
    this.module = await this.modulePromise;
    assert(this.module instanceof ModuleWrap);
    const moduleRequests = this.module.getModuleRequests();
    // Create an ArrayLike to avoid calling into userspace with `.then`
    // when returned from the async function.
    // Modules should be aligned with the moduleRequests array in order.
    const modulePromises = Array(moduleRequests.length);
    const evaluationDepJobs = [];
    this.commonJsDeps = Array(moduleRequests.length);
    for (let idx = 0; idx < moduleRequests.length; idx++) {
      const request = moduleRequests[idx];
      // Explicitly keeping track of dependency jobs is needed in order
      // to flatten out the dependency graph below in `asyncInstantiate()`,
      // so that circular dependencies can't cause a deadlock by two of
      // these `link` callbacks depending on each other.
      // TODO(joyeecheung): split this into two iterators, one for resolving and one for loading so
      // that hooks can pre-fetch sources off-thread.
      const dependencyJobPromise = this.loader.getOrCreateModuleJob(this.url, request, requestType);
      const modulePromise = PromisePrototypeThen(dependencyJobPromise, (job) => {
        debug(`ModuleJob.asyncLink() ${this.url} -> ${request.specifier}`, job);
        if (request.phase === kEvaluationPhase) {
          ArrayPrototypePush(evaluationDepJobs, job);
        }
        return job.modulePromise;
      });
      modulePromises[idx] = modulePromise;
    }
    const modules = await SafePromiseAllReturnArrayLike(modulePromises);
    for (let idx = 0; idx < moduleRequests.length; idx++) {
      this.commonJsDeps[idx] = modules[idx].isCommonJS;
    }

    this.module.link(modules);
    return evaluationDepJobs;
  }

  #instantiate() {
    if (this.instantiated === undefined) {
      this.instantiated = this.#asyncInstantiate();
    }
    return this.instantiated;
  }

  async #asyncInstantiate() {
    const jobsInGraph = new SafeSet();
    // TODO(joyeecheung): if it's not on the async loader thread, consider this already
    // linked.
    const addJobsToDependencyGraph = async (moduleJob) => {
      debug(`async addJobsToDependencyGraph() ${this.url}`, moduleJob);

      if (jobsInGraph.has(moduleJob)) {
        return;
      }
      jobsInGraph.add(moduleJob);
      const dependencyJobs = isPromise(moduleJob.linked) ? await moduleJob.linked : moduleJob.linked;
      return SafePromiseAllReturnVoid(dependencyJobs, addJobsToDependencyGraph);
    };
    await addJobsToDependencyGraph(this);

    try {
      if (!hasPausedEntry && this.inspectBrk) {
        hasPausedEntry = true;
        const initWrapper = internalBinding('inspector').callAndPauseOnStart;
        initWrapper(this.module.instantiate, this.module);
      } else {
        this.module.instantiate();
      }
    } catch (e) {
      decorateErrorStack(e);
      // TODO(@bcoe): Add source map support to exception that occurs as result
      // of missing named export. This is currently not possible because
      // stack trace originates in module_job, not the file itself. A hidden
      // symbol with filename could be set in node_errors.cc to facilitate this.
      if (!getSourceMapsSupport().enabled &&
          StringPrototypeIncludes(e.message,
                                  ' does not provide an export named')) {
        const splitStack = StringPrototypeSplit(e.stack, '\n', 2);
        const { 1: childSpecifier, 2: name } = RegExpPrototypeExec(
          /module '(.*)' does not provide an export named '(.+)'/,
          e.message);
        const moduleRequests = this.module.getModuleRequests();
        let isCommonJS = false;
        for (let i = 0; i < moduleRequests.length; ++i) {
          if (moduleRequests[i].specifier === childSpecifier) {
            isCommonJS = this.commonJsDeps[i];
            break;
          }
        }

        if (isCommonJS) {
          const importStatement = splitStack[1];
          // TODO(@ctavan): The original error stack only provides the single
          // line which causes the error. For multi-line import statements we
          // cannot generate an equivalent object destructuring assignment by
          // just parsing the error stack.
          const oneLineNamedImports = RegExpPrototypeExec(/{.*}/, importStatement);
          const destructuringAssignment = oneLineNamedImports &&
            RegExpPrototypeSymbolReplace(/\s+as\s+/g, oneLineNamedImports, ': ');
          e.message = `Named export '${name}' not found. The requested module` +
            ` '${childSpecifier}' is a CommonJS module, which may not support` +
            ' all module.exports as named exports.\nCommonJS modules can ' +
            'always be imported via the default export, for example using:' +
            `\n\nimport pkg from '${childSpecifier}';\n${
              destructuringAssignment ?
                `const ${destructuringAssignment} = pkg;\n` : ''}`;
          const newStack = StringPrototypeSplit(e.stack, '\n');
          newStack[3] = `SyntaxError: ${e.message}`;
          e.stack = ArrayPrototypeJoin(newStack, '\n');
        }
      }
      throw e;
    }

    for (const dependencyJob of jobsInGraph) {
      // Calling `this.module.instantiate()` instantiates not only the
      // ModuleWrap in this module, but all modules in the graph.
      dependencyJob.instantiated = resolvedPromise;
    }
  }

  runSync(parent) {
    assert(this.phase === kEvaluationPhase);
    assert(this.module instanceof ModuleWrap);
    let status = this.module.getStatus();

    debug('ModuleJob.runSync()', status, this.module);
    if (status === kUninstantiated) {
      // FIXME(joyeecheung): this cannot fully handle < kInstantiated. Make the linking
      // fully synchronous instead.
      if (this.module.getModuleRequests().length === 0) {
        this.module.link([]);
      }
      this.module.instantiate();
      status = this.module.getStatus();
    }
    if (status === kInstantiated || status === kErrored) {
      const filename = urlToFilename(this.url);
      const parentFilename = urlToFilename(parent?.filename);
      if (this.module.hasAsyncGraph && !getOptionValue('--experimental-print-required-tla')) {
        throw new ERR_REQUIRE_ASYNC_MODULE(filename, parentFilename);
      }
      if (status === kInstantiated) {
        setHasStartedUserESMExecution();
        const namespace = this.module.evaluateSync(filename, parentFilename);
        return { __proto__: null, module: this.module, namespace };
      }
      throw this.module.getError();
    } else if (status === kEvaluating || status === kEvaluated) {
      if (this.module.hasAsyncGraph) {
        const filename = urlToFilename(this.url);
        const parentFilename = urlToFilename(parent?.filename);
        throw new ERR_REQUIRE_ASYNC_MODULE(filename, parentFilename);
      }
      // kEvaluating can show up when this is being used to deal with CJS <-> CJS cycles.
      // Allow it for now, since we only need to ban ESM <-> CJS cycles which would be
      // detected earlier during the linking phase, though the CJS handling in the ESM
      // loader won't be able to emit warnings on pending circular exports like what
      // the CJS loader does.
      // TODO(joyeecheung): remove the re-invented require() in the ESM loader and
      // always handle CJS using the CJS loader to eliminate the quirks.
      return { __proto__: null, module: this.module, namespace: this.module.getNamespace() };
    }
    assert.fail(`Unexpected module status ${status}.`);
  }

  async run(isEntryPoint = false) {
    debug('ModuleJob.run()', this.module);
    assert(this.phase === kEvaluationPhase);
    await this.#instantiate();
    if (isEntryPoint) {
      globalThis[entry_point_module_private_symbol] = this.module;
    }
    const timeout = -1;
    const breakOnSigint = false;
    setHasStartedUserESMExecution();
    try {
      await this.module.evaluate(timeout, breakOnSigint);
    } catch (e) {
      explainCommonJSGlobalLikeNotDefinedError(e, this.module.url, this.module.hasTopLevelAwait);
      throw e;
    }
    return { __proto__: null, module: this.module };
  }
}

/**
 * This is a fully synchronous job and does not spawn additional threads in any way.
 * All the steps are ensured to be synchronous and it throws on instantiating
 * an asynchronous graph. It also disallows CJS <-> ESM cycles.
 *
 * This is used for ES modules loaded via require(esm). Modules loaded by require() in
 * imported CJS are handled by ModuleJob with the isForRequireInImportedCJS set to true instead.
 * The two currently have different caching behaviors.
 * TODO(joyeecheung): consolidate this with the isForRequireInImportedCJS variant of ModuleJob.
 */
class ModuleJobSync extends ModuleJobBase {
  /**
   * @param {ModuleLoader} loader The ESM loader.
   * @param {string} url URL of the module to be wrapped in ModuleJob.
   * @param {ImportAttributes} importAttributes Import attributes from the import statement.
   * @param {ModuleWrap} moduleWrap Translated ModuleWrap for the module.
   * @param {number} phase The phase to load the module to.
   * @param {boolean} isMain Whether the module is the entry point.
   * @param {boolean} inspectBrk Whether this module should be evaluated with the
   *   first line paused in the debugger (because --inspect-brk is passed).
   */
  constructor(loader, url, importAttributes, moduleWrap, phase = kEvaluationPhase, isMain,
              inspectBrk, requestType) {
    super(loader, url, importAttributes, phase, isMain, inspectBrk);

    this.module = moduleWrap;

    assert(this.module instanceof ModuleWrap);
    this.linked = undefined;
    this.type = importAttributes.type;
    if (phase === kEvaluationPhase) {
      this.linked = this.link(requestType);
    }
  }

  /**
   * @param {ModuleRequestType} requestType Type of the module request.
   * @returns {ModuleJobBase[]}
   */
  link(requestType) {
    // Synchronous linking is always used for ModuleJobSync.
    return this.syncLink(requestType);
  }

  get modulePromise() {
    return PromiseResolve(this.module);
  }

  async run() {
    assert(this.phase === kEvaluationPhase);
    // This path is hit by a require'd module that is imported again.
    const status = this.module.getStatus();
    debug('ModuleJobSync.run()', status, this.module);
    // If the module was previously required and errored, reject from import() again.
    if (status === kErrored) {
      throw this.module.getError();
    } else if (status > kInstantiated) {
      if (this.evaluationPromise) {
        await this.evaluationPromise;
      }
      return { __proto__: null, module: this.module };
    } else if (status === kInstantiated) {
      // The evaluation may have been canceled because instantiate() detected TLA first.
      // But when it is imported again, it's fine to re-evaluate it asynchronously.
      const timeout = -1;
      const breakOnSigint = false;
      this.evaluationPromise = this.module.evaluate(timeout, breakOnSigint);
      await this.evaluationPromise;
      this.evaluationPromise = undefined;
      return { __proto__: null, module: this.module };
    }

    assert.fail('Unexpected status of a module that is imported again after being required. ' +
                `Status = ${status}`);
  }

  runSync(parent) {
    debug('ModuleJobSync.runSync()', this.module);
    assert(this.phase === kEvaluationPhase);
    // TODO(joyeecheung): add the error decoration logic from the async instantiate.
    this.module.instantiate();
    // If --experimental-print-required-tla is true, proceeds to evaluation even
    // if it's async because we want to search for the TLA and help users locate
    // them.
    // TODO(joyeecheung): track the asynchroniticy using v8::Module::HasTopLevelAwait()
    // and we'll be able to throw right after compilation of the modules, using acron
    // to find and print the TLA. This requires the linking to be synchronous in case
    // it runs into cached asynchronous modules that are not yet fetched.
    const parentFilename = urlToFilename(parent?.filename);
    const filename = urlToFilename(this.url);
    if (this.module.hasAsyncGraph && !getOptionValue('--experimental-print-required-tla')) {
      throw new ERR_REQUIRE_ASYNC_MODULE(filename, parentFilename);
    }
    setHasStartedUserESMExecution();
    try {
      const namespace = this.module.evaluateSync(filename, parentFilename);
      return { __proto__: null, module: this.module, namespace };
    } catch (e) {
      explainCommonJSGlobalLikeNotDefinedError(e, this.module.url, this.module.hasTopLevelAwait);
      throw e;
    }
  }
}

ObjectSetPrototypeOf(ModuleJobBase.prototype, null);
module.exports = {
  ModuleJob, ModuleJobSync, ModuleJobBase,
};
