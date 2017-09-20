'use strict';

const { SafeSet, SafePromise } = require('internal/safe_globals');
const resolvedPromise = SafePromise.resolve();
const resolvedArrayPromise = SafePromise.resolve([]);
const { ModuleWrap } = require('internal/loader/ModuleWrap');

const NOOP = () => { /* No-op */ };
class ModuleJob {
  /**
   * @param {module: ModuleWrap?, compiled: Promise} moduleProvider
   */
  constructor(loader, moduleProvider, url) {
    this.url = `${moduleProvider.url}`;
    this.moduleProvider = moduleProvider;
    this.loader = loader;
    this.error = null;
    this.hadError = false;

    if (moduleProvider instanceof ModuleWrap !== true) {
      // linked == promise for dependency jobs, with module populated,
      // module wrapper linked
      this.modulePromise = this.moduleProvider.createModule();
      this.module = undefined;
      const linked = async () => {
        const dependencyJobs = [];
        this.module = await this.modulePromise;
        this.module.link(async (dependencySpecifier) => {
          const dependencyJobPromise =
            this.loader.getModuleJob(this, dependencySpecifier);
          dependencyJobs.push(dependencyJobPromise);
          const dependencyJob = await dependencyJobPromise;
          return dependencyJob.modulePromise;
        });
        return SafePromise.all(dependencyJobs);
      };
      this.linked = linked();

      // instantiated == deep dependency jobs wrappers instantiated,
      //module wrapper instantiated
      this.instantiated = undefined;
    } else {
      const getModuleProvider = async () => moduleProvider;
      this.modulePromise = getModuleProvider();
      this.moduleProvider = { finish: NOOP };
      this.module = moduleProvider;
      this.linked = resolvedArrayPromise;
      this.instantiated = this.modulePromise;
    }
  }

  instantiate() {
    if (this.instantiated) {
      return this.instantiated;
    }
    return this.instantiated = new Promise(async (resolve, reject) => {
      const jobsInGraph = new SafeSet();
      let jobsReadyToInstantiate = 0;
      // (this must be sync for counter to work)
      const queueJob = (moduleJob) => {
        if (jobsInGraph.has(moduleJob)) {
          return;
        }
        jobsInGraph.add(moduleJob);
        moduleJob.linked.then((dependencyJobs) => {
          for (const dependencyJob of dependencyJobs) {
            queueJob(dependencyJob);
          }
          checkComplete();
        }, (e) => {
          if (!this.hadError) {
            this.error = e;
            this.hadError = true;
          }
          checkComplete();
        });
      };
      const checkComplete = () => {
        if (++jobsReadyToInstantiate === jobsInGraph.size) {
          // I believe we only throw once the whole tree is finished loading?
          // or should the error bail early, leaving entire tree to still load?
          if (this.hadError) {
            reject(this.error);
          } else {
            try {
              this.module.instantiate();
              for (const dependencyJob of jobsInGraph) {
                dependencyJob.instantiated = resolvedPromise;
              }
              resolve(this.module);
            } catch (e) {
              e.stack;
              reject(e);
            }
          }
        }
      };
      queueJob(this);
    });
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
