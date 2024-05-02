'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypePushApply,
  ArrayPrototypeSlice,
  StringPrototypeSlice,
} = primordials;

const Buffer = require('buffer').Buffer;
const console = require('internal/console/global');
const vm = require('vm');
const { SourceTextModule } = require('internal/vm/module');

const { natives } = internalBinding('builtins');

async function linker(specifier, referencingModule) {
  // Transform "./file.mjs" to "file"
  const file = StringPrototypeSlice(specifier, 2, -4);
  const code = natives[`internal/deps/v8/tools/${file}`];
  return new SourceTextModule(code, { context: referencingModule.context });
}

(async () => {
  const tickArguments = [];
  if (process.platform === 'darwin') {
    ArrayPrototypePush(tickArguments, '--mac');
  } else if (process.platform === 'win32') {
    ArrayPrototypePush(tickArguments, '--windows');
  }
  ArrayPrototypePushApply(tickArguments,
                          ArrayPrototypeSlice(process.argv, 1));

  const context = vm.createContext({
    arguments: tickArguments,
    write(s) { process.stdout.write(s); },
    printErr(err) { console.error(err); },
    console,
    process,
    Buffer,
  });

  const polyfill = natives['internal/v8_prof_polyfill'];
  const script = `(function(module, require) {
    ${polyfill}
  })`;

  vm.runInContext(script, context)(module, require);

  const tickProcessor = natives['internal/deps/v8/tools/tickprocessor-driver'];
  const tickprocessorDriver = new SourceTextModule(tickProcessor, { context });
  await tickprocessorDriver.link(linker);
  await tickprocessorDriver.evaluate();
})();
