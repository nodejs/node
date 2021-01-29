'use strict';

const {
  ArrayPrototypeFilter,
  ArrayPrototypeFind,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  FunctionPrototype,
  Number,
  ObjectSetPrototypeOf,
  PromiseAll,
  PromiseResolve,
  PromisePrototypeCatch,
  ReflectApply,
  SafeArrayIterator,
  SafeSet,
  StringPrototypeIncludes,
  StringPrototypeMatch,
  StringPrototypeSplit,
} = primordials;

const { ModuleWrap } = internalBinding('module_wrap');

const { decorateErrorStack } = require('internal/util');
const { fileURLToPath } = require('internal/url');
const assert = require('internal/assert');
const resolvedPromise = PromiseResolve();

const noop = FunctionPrototype;

let hasPausedEntry = false;

function extractExample(file, lineNumber) {
  const { readFileSync } = require('fs');
  const { parse } = require('internal/deps/acorn/acorn/dist/acorn');
  const { findNodeAt } = require('internal/deps/acorn/acorn-walk/dist/walk');

  const code = readFileSync(file, { encoding: 'utf8' });
  const parsed = parse(code, {
    sourceType: 'module',
    locations: true,
  });
  let start = 0;
  let node;
  do {
    node = findNodeAt(parsed, start);
    start = node.node.end + 1;

    if (node.node.loc.end.line < lineNumber) {
      continue;
    }

    if (node.node.type !== 'ImportDeclaration') {
      continue;
    }

    const defaultSpecifier = ArrayPrototypeFind(
      node.node.specifiers,
      (specifier) => specifier.type === 'ImportDefaultSpecifier'
    );
    const defaultImport = defaultSpecifier ?
      defaultSpecifier.local.name :
      'pkg';

    const joinString = ', ';
    let totalLength = 0;
    const imports = ArrayPrototypeMap(
      ArrayPrototypeFilter(
        node.node.specifiers,
        (specifier) => (specifier.type === 'ImportSpecifier'),
      ),
      (specifier) => {
        const statement =
          specifier.local.name === specifier.imported.name ?
            `${specifier.imported.name}` :
            `${specifier.imported.name}: ${specifier.local.name}`;
        totalLength += statement.length + joinString.length;
        return statement;
      });

    const boilerplate = `const {  } = ${defaultImport};`;
    const destructuringAssignment = totalLength > 80 - boilerplate.length ?
      `\n${ArrayPrototypeJoin(
        ArrayPrototypeMap(imports, (i) => `  ${i}`),
        ',\n',
      )}\n` :
      ` ${ArrayPrototypeJoin(imports, joinString)} `;

    return `\n\nimport ${defaultImport} from '${node.node.source.value}';\n` +
      `const {${destructuringAssignment}} = ${defaultImport};\n`;
  } while (node === undefined || node.node.loc.start.line <= lineNumber);
  assert.fail('Could not find erroneous import statement');
}

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
        await PromiseAll(new SafeArrayIterator(promises));

      return PromiseAll(new SafeArrayIterator(dependencyJobs));
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
        new SafeArrayIterator(
          ArrayPrototypeMap(dependencyJobs, addJobsToDependencyGraph)
        )
      );
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
      if (
        StringPrototypeIncludes(e.message, ' does not provide an export named')
      ) {
        const splitStack = StringPrototypeSplit(e.stack, '\n');
        const parentFileUrl = splitStack[0];
        const { 1: childSpecifier, 2: name } = StringPrototypeMatch(
          e.message,
          /module '(.*)' does not provide an export named '(.+)'/
        );
        const childFileURL = await this.loader.resolve(
          childSpecifier,
          parentFileUrl
        );
        const format = await this.loader.getFormat(childFileURL);
        if (format === 'commonjs') {
          const [, fileUrl, lineNumber] = StringPrototypeMatch(
            parentFileUrl,
            /^(.*):([0-9]+)$/
          );
          const example = extractExample(
            fileURLToPath(fileUrl),
            Number(lineNumber)
          );
          e.message =
            `Named export '${name}' not found. The requested module` +
            ` '${childSpecifier}' is a CommonJS module, which may not support` +
            ' all module.exports as named exports.\nCommonJS modules can ' +
            'always be imported via the default export, for example using:' +
            example;
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
