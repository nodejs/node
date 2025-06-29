'use strict';

const {
  Array,
  ArrayPrototypeJoin,
  ArrayPrototypeSome,
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
const { decorateErrorStack, kEmptyObject } = require('internal/util');
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
const isCommonJSGlobalLikeNotDefinedError = (errorMessage) =>
  ArrayPrototypeSome(
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
  if (e?.name === 'ReferenceError' &&
      isCommonJSGlobalLikeNotDefinedError(e.message)) {

    if (hasTopLevelAwait) {
      e.message = `Cannot determine intended module format because both require() and top-level await are present. If the code is intended to be CommonJS, wrap await in an async function. If the code is intended to be an ES module, replace require() with import.`;
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
  constructor(url, importAttributes, phase, isMain, inspectBrk) {
    assert(typeof phase === 'number');
    this.importAttributes = importAttributes;
    this.phase = phase;
    this.isMain = isMain;
    this.inspectBrk = inspectBrk;

    this.url = url;
  }
}

/* A ModuleJob tracks the loading of a single Module, and the ModuleJobs of
 * its dependencies, over time. */
class ModuleJob extends ModuleJobBase {
  #loader = null;

  /**
   * @param {ModuleLoader} loader The ESM loader.
   * @param {string} url URL of the module to be wrapped in ModuleJob.
   * @param {ImportAttributes} importAttributes Import attributes from the import statement.
   * @param {ModuleWrap|Promise<ModuleWrap>} moduleOrModulePromise Translated ModuleWrap for the module.
   * @param {number} phase The phase to load the module to.
   * @param {boolean} isMain Whether the module is the entry point.
   * @param {boolean} inspectBrk Whether this module should be evaluated with the
   *                             first line paused in the debugger (because --inspect-brk is passed).
   * @param {boolean} isForRequireInImportedCJS Whether this is created for require() in imported CJS.
   */
  constructor(loader, url, importAttributes = { __proto__: null }, moduleOrModulePromise,
              phase = kEvaluationPhase, isMain, inspectBrk, isForRequireInImportedCJS = false) {
    super(url, importAttributes, phase, isMain, inspectBrk);
    this.#loader = loader;

    // Expose the promise to the ModuleWrap directly for linking below.
    if (isForRequireInImportedCJS) {
      this.module = moduleOrModulePromise;
      assert(this.module instanceof ModuleWrap);
      this.modulePromise = PromiseResolve(this.module);
    } else {
      this.modulePromise = moduleOrModulePromise;
    }

    if (this.phase === kEvaluationPhase) {
      // Promise for the list of all dependencyJobs.
      this.linked = this.#link();
      // This promise is awaited later anyway, so silence
      // 'unhandled rejection' warnings.
      PromisePrototypeThen(this.linked, undefined, noop);
    }

    // instantiated == deep dependency jobs wrappers are instantiated,
    // and module wrapper is instantiated.
    this.instantiated = undefined;
  }

  /**
   * Ensure that this ModuleJob is moving towards the required phase
   * (does not necessarily mean it is ready at that phase - run does that)
   * @param {number} phase
   */
  ensurePhase(phase) {
    if (this.phase < phase) {
      this.phase = phase;
      this.linked = this.#link();
      PromisePrototypeThen(this.linked, undefined, noop);
    }
  }

  /**
   * Iterates the module requests and links with the loader.
   * @returns {Promise<ModuleJob[]>} Dependency module jobs.
   */
  async #link() {
    this.module = await this.modulePromise;
    assert(this.module instanceof ModuleWrap);

    const moduleRequests = this.module.getModuleRequests();
    // Explicitly keeping track of dependency jobs is needed in order
    // to flatten out the dependency graph below in `_instantiate()`,
    // so that circular dependencies can't cause a deadlock by two of
    // these `link` callbacks depending on each other.
    // Create an ArrayLike to avoid calling into userspace with `.then`
    // when returned from the async function.
    const evaluationDepJobs = Array(moduleRequests.length);
    ObjectSetPrototypeOf(evaluationDepJobs, null);

    // Modules should be aligned with the moduleRequests array in order.
    const modulePromises = Array(moduleRequests.length);
    // Track each loop for whether it is an evaluation phase or source phase request.
    let isEvaluation;
    // Iterate with index to avoid calling into userspace with `Symbol.iterator`.
    for (
      let idx = 0, eidx = 0;
      // Use the let-scoped eidx value to update the executionDepJobs length at the end of the loop.
      idx < moduleRequests.length || (evaluationDepJobs.length = eidx, false);
      idx++, eidx += isEvaluation
    ) {
      const { specifier, phase, attributes } = moduleRequests[idx];
      isEvaluation = phase === kEvaluationPhase;
      // TODO(joyeecheung): resolve all requests first, then load them in another
      // loop so that hooks can pre-fetch sources off-thread.
      const dependencyJobPromise = this.#loader.getModuleJobForImport(
        specifier, this.url, attributes, phase,
      );
      const modulePromise = PromisePrototypeThen(dependencyJobPromise, (job) => {
        debug(`async link() ${this.url} -> ${specifier}`, job);
        if (phase === kEvaluationPhase) {
          evaluationDepJobs[eidx] = job;
        }
        return job.modulePromise;
      });
      modulePromises[idx] = modulePromise;
    }

    const modules = await SafePromiseAllReturnArrayLike(modulePromises);
    this.module.link(modules);

    return evaluationDepJobs;
  }

  #instantiate() {
    if (this.instantiated === undefined) {
      this.instantiated = this.#_instantiate();
    }
    return this.instantiated;
  }

  async #_instantiate() {
    const jobsInGraph = new SafeSet();
    const addJobsToDependencyGraph = async (moduleJob) => {
      debug(`async addJobsToDependencyGraph() ${this.url}`, moduleJob);

      if (jobsInGraph.has(moduleJob)) {
        return;
      }
      jobsInGraph.add(moduleJob);
      const dependencyJobs = await moduleJob.linked;
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
        const parentFileUrl = RegExpPrototypeSymbolReplace(
          /:\d+$/,
          splitStack[0],
          '',
        );
        const { 1: childSpecifier, 2: name } = RegExpPrototypeExec(
          /module '(.*)' does not provide an export named '(.+)'/,
          e.message);
        const { url: childFileURL } = await this.#loader.resolve(
          childSpecifier,
          parentFileUrl,
          kEmptyObject,
        );
        let format;
        try {
          // This might throw for non-CommonJS modules because we aren't passing
          // in the import attributes and some formats require them; but we only
          // care about CommonJS for the purposes of this error message.
          ({ format } =
            await this.#loader.load(childFileURL));
        } catch {
          // Continue regardless of error.
        }

        if (format === 'commonjs') {
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

    debug('ModuleJob.runSync', this.module);
    // FIXME(joyeecheung): this cannot fully handle < kInstantiated. Make the linking
    // fully synchronous instead.
    if (status === kUninstantiated) {
      this.module.async = this.module.instantiateSync();
      status = this.module.getStatus();
    }
    if (status === kInstantiated || status === kErrored) {
      const filename = urlToFilename(this.url);
      const parentFilename = urlToFilename(parent?.filename);
      this.module.async ??= this.module.isGraphAsync();

      if (this.module.async && !getOptionValue('--experimental-print-required-tla')) {
        throw new ERR_REQUIRE_ASYNC_MODULE(filename, parentFilename);
      }
      if (status === kInstantiated) {
        setHasStartedUserESMExecution();
        const namespace = this.module.evaluateSync(filename, parentFilename);
        return { __proto__: null, module: this.module, namespace };
      }
      throw this.module.getError();
    } else if (status === kEvaluating || status === kEvaluated) {
      // kEvaluating can show up when this is being used to deal with CJS <-> CJS cycles.
      // Allow it for now, since we only need to ban ESM <-> CJS cycles which would be
      // detected earlier during the linking phase, though the CJS handling in the ESM
      // loader won't be able to emit warnings on pending circular exports like what
      // the CJS loader does.
      // TODO(joyeecheung): remove the re-invented require() in the ESM loader and
      // always handle CJS using the CJS loader to eliminate the quirks.
      return { __proto__: null, module: this.module, namespace: this.module.getNamespaceSync() };
    }
    assert.fail(`Unexpected module status ${status}.`);
  }

  async run(isEntryPoint = false) {
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
      explainCommonJSGlobalLikeNotDefinedError(e, this.module.url, this.module.hasTopLevelAwait());
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
  #loader = null;

  /**
   * @param {ModuleLoader} loader The ESM loader.
   * @param {string} url URL of the module to be wrapped in ModuleJob.
   * @param {ImportAttributes} importAttributes Import attributes from the import statement.
   * @param {ModuleWrap} moduleWrap Translated ModuleWrap for the module.
   * @param {number} phase The phase to load the module to.
   * @param {boolean} isMain Whether the module is the entry point.
   * @param {boolean} inspectBrk Whether this module should be evaluated with the
   *                             first line paused in the debugger (because --inspect-brk is passed).
   */
  constructor(loader, url, importAttributes, moduleWrap, phase = kEvaluationPhase, isMain,
              inspectBrk) {
    super(url, importAttributes, phase, isMain, inspectBrk, true);

    this.#loader = loader;
    this.module = moduleWrap;

    assert(this.module instanceof ModuleWrap);
    this.linked = undefined;
    this.type = importAttributes.type;
    if (phase === kEvaluationPhase) {
      this.#link();
    }
  }

  /**
   * Ensure that this ModuleJob is at the required phase
   * @param {number} phase
   */
  ensurePhase(phase) {
    if (this.phase < phase) {
      this.phase = phase;
      this.#link();
    }
  }

  #link() {
    // Store itself into the cache first before linking in case there are circular
    // references in the linking.
    this.#loader.loadCache.set(this.url, this.type, this);
    try {
      const moduleRequests = this.module.getModuleRequests();
      // Modules should be aligned with the moduleRequests array in order.
      const modules = Array(moduleRequests.length);
      const evaluationDepJobs = Array(moduleRequests.length);
      let j = 0;
      for (let i = 0; i < moduleRequests.length; ++i) {
        const { specifier, attributes, phase } = moduleRequests[i];
        const job = this.#loader.getModuleJobForRequire(specifier, this.url, attributes, phase);
        modules[i] = job.module;
        if (phase === kEvaluationPhase) {
          evaluationDepJobs[j++] = job;
        }
      }
      evaluationDepJobs.length = j;
      this.module.link(modules);
      this.linked = evaluationDepJobs;
    } finally {
      // Restore it - if it succeeds, we'll reset in the caller; Otherwise it's
      // not cached and if the error is caught, subsequent attempt would still fail.
      this.#loader.loadCache.delete(this.url, this.type);
    }
  }

  get modulePromise() {
    return PromiseResolve(this.module);
  }

  async run() {
    assert(this.phase === kEvaluationPhase);
    // This path is hit by a require'd module that is imported again.
    const status = this.module.getStatus();
    if (status > kInstantiated) {
      if (this.evaluationPromise) {
        await this.evaluationPromise;
      }
      return { __proto__: null, module: this.module };
    } else if (status === kInstantiated) {
      // The evaluation may have been canceled because instantiateSync() detected TLA first.
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
    assert(this.phase === kEvaluationPhase);
    // TODO(joyeecheung): add the error decoration logic from the async instantiate.
    this.module.async = this.module.instantiateSync();
    // If --experimental-print-required-tla is true, proceeds to evaluation even
    // if it's async because we want to search for the TLA and help users locate
    // them.
    // TODO(joyeecheung): track the asynchroniticy using v8::Module::HasTopLevelAwait()
    // and we'll be able to throw right after compilation of the modules, using acron
    // to find and print the TLA.
    const parentFilename = urlToFilename(parent?.filename);
    const filename = urlToFilename(this.url);
    if (this.module.async && !getOptionValue('--experimental-print-required-tla')) {
      throw new ERR_REQUIRE_ASYNC_MODULE(filename, parentFilename);
    }
    setHasStartedUserESMExecution();
    try {
      const namespace = this.module.evaluateSync(filename, parentFilename);
      return { __proto__: null, module: this.module, namespace };
    } catch (e) {
      explainCommonJSGlobalLikeNotDefinedError(e, this.module.url, this.module.hasTopLevelAwait());
      throw e;
    }
  }
}

ObjectSetPrototypeOf(ModuleJobBase.prototype, null);
module.exports = {
  ModuleJob, ModuleJobSync, ModuleJobBase,
};
