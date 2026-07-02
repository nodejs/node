'use strict';

const {
  Array,
  ArrayIsArray,
  ArrayPrototypeFind,
  ArrayPrototypeJoin,
  ArrayPrototypePop,
  ArrayPrototypePush,
  ArrayPrototypeSort,
  FunctionPrototype,
  ObjectAssign,
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
  kDeferPhase,
  kEvaluationPhase,
  kInstantiated,
  kUninstantiated,
} = internalBinding('module_wrap');
const {
  getPromiseDetails,
  privateSymbols: {
    entry_point_module_private_symbol,
    module_source_private_symbol: kModuleSource,
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
  ERR_REQUIRE_ESM_RACE_CONDITION,
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

/**
 * @typedef {object} TopLevelAwaitLocation
 * @property {string} url URL of the module containing the top-level await.
 * @property {number} line 1-based line number of the top-level await.
 * @property {number} column 1-based column number of the top-level await.
 * @property {string} sourceLine The source line containing the top-level await.
 */

/**
 * Locate the top-level awaits in the given module by parsing the source with acorn.
 * @param {string} source Module source code.
 * @returns {object[]} The acorn AST nodes of the top-level awaits, in source order.
 */
function findTopLevelAwait(source) {
  const { Parser } = require('internal/deps/acorn/acorn/dist/acorn');
  const walk = require('internal/deps/acorn/acorn-walk/dist/walk');
  let ast;
  try {
    ast = Parser.parse(source, {
      __proto__: null, ecmaVersion: 'latest', sourceType: 'module', locations: true,
    });
  } catch {
    return [];  // The source is not parsable, skip.
  }
  // We are looking for _top-level_ await, so we don't traverse into function bodies.
  const baseVisitor = ObjectAssign({ __proto__: null }, walk.base, { Function: noop });
  const found = [];
  walk.simple(ast, {
    __proto__: null,
    AwaitExpression(node) { ArrayPrototypePush(found, node); },
    // `for await (...)` is a ForOfStatement with `await: true`, not an AwaitExpression.
    ForOfStatement(node) {
      if (node.await) { ArrayPrototypePush(found, node); }
    },
    // `await using x = ...` is a VariableDeclaration, not an AwaitExpression.
    VariableDeclaration(node) {
      if (node.kind === 'await using') { ArrayPrototypePush(found, node); }
    },
  }, baseVisitor);
  ArrayPrototypeSort(found, (a, b) => a.start - b.start);
  return found;
}

/**
 * Collect the modules that contain top-level await in the linked graph of a job.
 * @param {ModuleJobBase} root The root of the module graph to search.
 * @returns {ModuleWrap[]} Modules that contain top-level await.
 */
function findModulesWithTopLevelAwait(root) {
  const found = [];
  const seen = new SafeSet();
  const stack = [root];
  while (stack.length > 0) {
    const job = ArrayPrototypePop(stack);
    if (seen.has(job)) { continue; }
    seen.add(job);
    if (job.module?.hasTopLevelAwait) {
      ArrayPrototypePush(found, job.module);
    }
    let linked = job.linked;
    if (isPromise(linked)) {
      linked = getPromiseDetails(linked)?.[1];
    }
    // If `require(esm)` comes from the deprecated async loader hook worker thread,
    // linked may be pending at this point. In that case, this branch would be skipped -
    // we just allow lossy reporting of TLA locations in an edge case when a deprecated
    // feature is used in combination with another experimental flag.
    if (ArrayIsArray(linked)) {
      for (let i = 0; i < linked.length; i++) {
        ArrayPrototypePush(stack, linked[i]);
      }
    }
  }
  return found;
}

/**
 * Locate the top-level awaits in the given modules.
 * @param {ModuleJobBase} root The root of the module graph to search.
 * @returns {TopLevelAwaitLocation[]} The locations of the top-level awaits.
 */
function getTopLevelAwaitLocations(root) {
  const modules = findModulesWithTopLevelAwait(root);
  const locations = [];
  for (let i = 0; i < modules.length; i++) {
    const module = modules[i];
    const source = module[kModuleSource];
    if (typeof source !== 'string') { continue; }  // Not retained during compilation. Skip.
    const found = findTopLevelAwait(source);
    if (found.length === 0) { continue; }
    const lines = StringPrototypeSplit(source, '\n');
    for (let j = 0; j < found.length; j++) {
      const { start } = found[j].loc;
      ArrayPrototypePush(locations, {
        __proto__: null,
        url: module.url,
        line: start.line,
        // Acorn reports 0-based columns, convert them to 1-based to match `line`.
        column: start.column + 1,
        sourceLine: lines[start.line - 1],
      });
    }
  }
  return locations;
}

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
    // references in the linking. Track whether we're overwriting an existing entry
    // so we know whether to remove the temporary entry in the finally block.
    const hadPreviousEntry = this.loader.loadCache.get(this.url, this.type) !== undefined;
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
        if (this.shouldRunModule(request.phase)) {
          ArrayPrototypePush(evaluationDepJobs, job);
        }
        modules[idx] = job.module;
        this.commonJsDeps[idx] = job.module.isCommonJS;
      }
      this.module.link(modules);
    } finally {
      if (!hadPreviousEntry) {
        // Remove the temporary entry. On failure this ensures subsequent attempts
        // don't return a broken job. On success the caller
        // (#getOrCreateModuleJobAfterResolve) will re-insert under the correct key.
        this.loader.loadCache.delete(this.url, this.type);
      }
      // If there was a previous entry (ensurePhase() path), leave this in cache -
      // it is the upgraded job and the caller will not re-insert.
    }

    return evaluationDepJobs;
  }

  /**
   * If the require()'d graph contains top-level await, collect the source locations
   * of the top-level awaits using source code retained during compilation and throw
   * ERR_REQUIRE_ASYNC_MODULE. The module must be at least instantiated.
   * @param {Module|undefined} parent CommonJS module that require()'d this, if any.
   */
  throwIfAsyncGraph(parent) {
    if (!this.module.hasAsyncGraph) {
      return;
    }
    const locations = getOptionValue('--experimental-print-required-tla') ? getTopLevelAwaitLocations(this) : [];
    const filename = urlToFilename(this.url);
    throw new ERR_REQUIRE_ASYNC_MODULE(filename, parent, locations);
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

  shouldLinkModule(phase) {
    return phase >= kDeferPhase;
  }
  shouldRunModule(phase) {
    return phase === kEvaluationPhase;
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

    if (this.shouldLinkModule(this.phase)) {
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
        if (this.shouldRunModule(request.phase)) {
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
    assert(this.shouldRunModule(this.phase));
    assert(this.module instanceof ModuleWrap);
    let status = this.module.getStatus();

    debug('ModuleJob.runSync()', status, this.module);
    if (status === kUninstantiated) {
      // TODO(joyeecheung): Reject graphs with top-level await _before_ instantiation, so that
      // the async graph error supersedes instantiation (mismatch export) errors in the graph.
      // FIXME(joyeecheung): this cannot fully handle < kInstantiated. Make the linking
      // fully synchronous instead.
      if (this.module.getModuleRequests().length === 0) {
        this.module.link([]);
      }
      this.module.instantiate();
      status = this.module.getStatus();
    }
    if (status === kInstantiated || status === kErrored) {
      this.throwIfAsyncGraph(parent);
      if (status === kInstantiated) {
        setHasStartedUserESMExecution();
        const namespace = this.module.evaluateSync();
        return { __proto__: null, module: this.module, namespace };
      }
      throw this.module.getError();
    } else if (status === kEvaluating || status === kEvaluated) {
      this.throwIfAsyncGraph(parent);
      // kEvaluating can show up when this is being used to deal with CJS <-> CJS cycles.
      // Allow it for now, since we only need to ban ESM <-> CJS cycles which would be
      // detected earlier during the linking phase, though the CJS handling in the ESM
      // loader won't be able to emit warnings on pending circular exports like what
      // the CJS loader does.
      // TODO(joyeecheung): remove the re-invented require() in the ESM loader and
      // always handle CJS using the CJS loader to eliminate the quirks.
      return { __proto__: null, module: this.module, namespace: this.module.getNamespace() };
    }
    assert(status === kUninstantiated, `Unexpected module status ${status}.`);
    throw new ERR_REQUIRE_ESM_RACE_CONDITION();
  }

  async run(isEntryPoint = false) {
    debug('ModuleJob.run()', this.module);
    assert(this.shouldRunModule(this.phase));
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
    if (this.shouldLinkModule(phase)) {
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
    assert(this.shouldRunModule(this.phase));
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
    } else if (status === kInstantiated || status === kUninstantiated) {
      // If we get here, the module was initially required and is now being imported.
      // The require() module failed either because the graph has TLA (kInstantiated),
      // or instantiation failed (kUninstantiated, e.g. missing named export).
      // Try finishing the instantiation - if it succeeds, proceed to evaluation,
      // otherwise the branch below re-throw any instantiation error.
      if (status === kUninstantiated) {
        this.module.instantiate();
      }
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
    assert(this.shouldRunModule(this.phase));
    // TODO(joyeecheung): Reject graphs with top-level await _before_ instantiation, so that the
    // async graph error supersedes instantiation (mismatch export) errors in the graph.
    // TODO(joyeecheung): add the error decoration logic from the async instantiate.
    this.module.instantiate();
    // On the deprecated async loader hook worker thread, dependencies linked by an
    // earlier import may not be walkable synchronously, so double-check with
    // V8 now that the graph is instantiated.
    this.throwIfAsyncGraph(parent);
    setHasStartedUserESMExecution();
    try {
      const namespace = this.module.evaluateSync();
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
