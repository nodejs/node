'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  FunctionPrototype,
  ObjectSetPrototypeOf,
  PromiseAll,
  PromiseResolve,
  PromisePrototypeCatch,
  ReflectApply,
  SafeSet,
  StringPrototypeIncludes,
  StringPrototypeMatch,
  StringPrototypeReplace,
  StringPrototypeSplit,
} = primordials;

const { ModuleWrap } = internalBinding('module_wrap');

const { decorateErrorStack } = require('internal/util');
const assert = require('internal/assert');
const resolvedPromise = PromiseResolve();

const noop = FunctionPrototype;

let hasPausedEntry = false;

/* A ModuleJob tracks the loading of a single Module, and the ModuleJobs of
 * its dependencies, over time. */
class ModuleJob {
  // `loader` is the Loader instance used for loading dependencies.
  // `moduleProvider` is a function
  constructor(loader, url, moduleProvider, isMain, inspectBrk) {
    this.loader = loader;
    this.isMain = isMain;
    this.inspectBrk = inspectBrk;

    this.module = undefined;
    // Expose the promise to the ModuleWrap directly for linking below.
    // `this.module` is also filled in below.
    this.modulePromise = ReflectApply(moduleProvider, loader, [url, isMain]);

    // Wait for the ModuleWrap instance being linked with all dependencies.
    const link = async () => {
      this.module = await this.modulePromise;
      assert(this.module instanceof ModuleWrap);

      // Explicitly keeping track of dependency jobs is needed in order
      // to flatten out the dependency graph below in `_instantiate()`,
      // so that circular dependencies can't cause a deadlock by two of
      // these `link` callbacks depending on each other.
      const dependencyJobs = [];
      const promises = this.module.link(async (specifier) => {
        const jobPromise = this.loader.getModuleJob(specifier, url);
        ArrayPrototypePush(dependencyJobs, jobPromise);
        const job = await jobPromise;
        return job.modulePromise;
      });

      if (promises !== undefined)
        await PromiseAll(promises);

      return PromiseAll(dependencyJobs);
    };
    // Promise for the list of all dependencyJobs.
    this.linked = link();
    // This promise is awaited later anyway, so silence
    // 'unhandled rejection' warnings.
    PromisePrototypeCatch(this.linked, noop);

    // instantiated == deep dependency jobs wrappers are instantiated,
    // and module wrapper is instantiated.
    this.instantiated = undefined;
  }

  instantiate() {
    if (this.instantiated === undefined) {
      this.instantiated = this._instantiate();
    }
    return this.instantiated;
  }

  async _instantiate() {
    const jobsInGraph = new SafeSet();
    const addJobsToDependencyGraph = async (moduleJob) => {
      if (jobsInGraph.has(moduleJob)) {
        return;
      }
      jobsInGraph.add(moduleJob);
      const dependencyJobs = await moduleJob.linked;
      return PromiseAll(
        ArrayPrototypeMap(dependencyJobs, addJobsToDependencyGraph));
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
      if (StringPrototypeIncludes(e.message,
                                  ' does not provide an export named')) {
        const splitStack = StringPrototypeSplit(e.stack, '\n');
        const parentFileUrl = splitStack[0];
        const [, childSpecifier, name] = StringPrototypeMatch(e.message,
                                                              /module '(.*)' does not provide an export named '(.+)'/);
        const childFileURL =
            await this.loader.resolve(childSpecifier, parentFileUrl);
        const format = await this.loader.getFormat(childFileURL);
        if (format === 'commonjs') {
          const importStatement = splitStack[1];
          // TODO(@ctavan): The original error stack only provides the single
          // line which causes the error. For multi-line import statements we
          // cannot generate an equivalent object descructuring assignment by
          // just parsing the error stack.
          const oneLineNamedImports = StringPrototypeMatch(importStatement, /{.*}/);
          const destructuringAssignment = oneLineNamedImports &&
              StringPrototypeReplace(oneLineNamedImports, /\s+as\s+/g, ': ');
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

  async run() {
    await this.instantiate();
    const timeout = -1;
    const breakOnSigint = false;
    await this.module.evaluate(timeout, breakOnSigint);
    return { module: this.module };
  }
}
ObjectSetPrototypeOf(ModuleJob.prototype, null);
module.exports = ModuleJob;
