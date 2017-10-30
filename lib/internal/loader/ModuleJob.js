'use strict';

const { SafeSet, SafePromise } = require('internal/safe_globals');
const resolvedPromise = SafePromise.resolve();

class ModuleJob {
  /**
   * @param {module: ModuleWrap?, compiled: Promise} moduleProvider
   */
  constructor(loader, url, moduleProvider) {
    this.loader = loader;
    this.error = null;
    this.hadError = false;

    // linked == promise for dependency jobs, with module populated,
    // module wrapper linked
    this.moduleProvider = moduleProvider;
    this.modulePromise = this.moduleProvider(url);
    this.module = undefined;
    this.reflect = undefined;
    const linked = async () => {
      const dependencyJobs = [];
      ({ module: this.module,
         reflect: this.reflect } = await this.modulePromise);
      this.module.link(async (dependencySpecifier) => {
        const dependencyJobPromise =
            this.loader.getModuleJob(dependencySpecifier, url);
        dependencyJobs.push(dependencyJobPromise);
        const dependencyJob = await dependencyJobPromise;
        return (await dependencyJob.modulePromise).module;
      });
      return SafePromise.all(dependencyJobs);
    };
    this.linked = linked();

    // instantiated == deep dependency jobs wrappers instantiated,
    // module wrapper instantiated
    this.instantiated = undefined;
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
