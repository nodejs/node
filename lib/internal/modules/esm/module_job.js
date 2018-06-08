'use strict';

const { internalBinding } = require('internal/bootstrap/loaders');
const { ModuleWrap } = internalBinding('module_wrap');
const { SafeSet, SafePromise } = require('internal/safe_globals');
const { decorateErrorStack } = require('internal/util');
const assert = require('assert');
const resolvedPromise = SafePromise.resolve();

const req = require('https');
const fs = require('fs');
const crypto = require('crypto');

const fetchModule = (url) => new Promise((resolve, reject) => {
  req.get(url, (res) => {
    let data = '';

    res.on('data', (chunk) => {
      data += chunk;
    });

    res.on('end', () => {
      resolve(data);
    });
  });
});

function url2hash(url) {
  const hash = crypto.createHash('sha256');
  hash.update(url);
  return hash.digest('hex');
}

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
        if (specifier.match(/http?s:/g)) {
          // TODO: Create http_node_modules and keep things inside it

          let dest = process.env.NODE_HTTP_MODULES || '/tmp';

          if (dest[dest.length - 1] === '/') {
            dest = dest.slice(0, dest.length - 1);
          }

          const scriptPath = `${dest}/${url2hash(specifier)}.mjs`;

          if (!fs.existsSync(scriptPath)) {
            const scriptContent = await fetchModule(specifier);
            fs.writeFileSync(scriptPath, scriptContent);
          }

          specifier = scriptPath;
        }

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
      module.evaluate(-1, false);
    } catch (e) {
      this.hadError = true;
      this.error = e;
      throw e;
    }
    return module;
  }
}
Object.setPrototypeOf(ModuleJob.prototype, null);
module.exports = ModuleJob;
