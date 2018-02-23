'use strict';

const { ModuleWrap } = internalBinding('module_wrap');
const { SafeSet, SafePromise } = require('internal/safe_globals');
const { decorateErrorStack } = require('internal/util');
const assert = require('assert');
const resolvedPromise = SafePromise.resolve();

/* A ModuleJob tracks the loading of a single Module, and the ModuleJobs of
 * its dependencies, over time. */
class ModuleJob {
  // `loader` is the Loader instance used for loading dependencies.
  // `moduleProvider` is a function
  constructor(loader, url, moduleProvider, inspectBrk) {
    this.loader = loader;
    this.error = null;
    this.hadError = false;
    this.inspectBrk = inspectBrk;

    // This is a Promise<{ module, reflect }>, whose fields will be copied
    // onto `this` by `link()` below once it has been resolved.
    this.modulePromise = moduleProvider(url);
    this.module = undefined;
    this.reflect = undefined;

    // Wait for the ModuleWrap instance being linked with all dependencies.
    const link = async () => {
      ({ module: this.module,
         reflect: this.reflect } = await this.modulePromise);
      assert(this.module instanceof ModuleWrap);

      const dependencyJobs = [];
      const promises = this.module.link(async (specifier) => {
        const jobPromise = this.loader.getModuleJob(specifier, url);
        dependencyJobs.push(jobPromise);
        return (await (await jobPromise).modulePromise).module;
      });

      if (promises !== undefined)
        await SafePromise.all(promises);

      return SafePromise.all(dependencyJobs);
    };
    // Promise for the list of all dependencyJobs.
    this.linked = link();

    // instantiated == deep dependency jobs wrappers instantiated,
    // module wrapper instantiated
    this.instantiated = undefined;
  }

  async instantiate() {
    if (!this.instantiated) {
      return this.instantiated = this._instantiate();
    }
    await this.instantiated;
    return this.module;
  }

  // This method instantiates the module associated with this job and its
  // entire dependency graph, i.e. creates all the module namespaces and the
  // exported/imported variables.
  async _instantiate() {
    const jobsInGraph = new SafeSet();

    const addJobsToDependencyGraph = async (moduleJob) => {
      if (jobsInGraph.has(moduleJob)) {
        return;
      }
      jobsInGraph.add(moduleJob);
      const dependencyJobs = await moduleJob.linked;
      return Promise.all(dependencyJobs.map(addJobsToDependencyGraph));
    };
    try {
      await addJobsToDependencyGraph(this);
    } catch (e) {
      if (!this.hadError) {
        this.error = e;
        this.hadError = true;
      }
      throw e;
    }
    try {
      if (this.inspectBrk) {
        const initWrapper = process.binding('inspector').callAndPauseOnStart;
        initWrapper(this.module.instantiate, this.module);
      } else {
        this.module.instantiate();
      }
    } catch (e) {
      decorateErrorStack(e);
      throw e;
    }
    for (const dependencyJob of jobsInGraph) {
      // Calling `this.module.instantiate()` instantiates not only the
      // ModuleWrap in this module, but all modules in the graph.
      dependencyJob.instantiated = resolvedPromise;
    }
    return this.module;
  }

  async run() {
    const module = await this.instantiate();
    try {
      module.evaluate();
    } catch (e) {
      e.stack;
      this.hadError = true;
      this.error = e;
      throw e;
    }
    return module;
  }
}
Object.setPrototypeOf(ModuleJob.prototype, null);
module.exports = ModuleJob;
