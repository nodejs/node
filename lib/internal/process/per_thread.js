'use strict';

// This files contains process bootstrappers that can be
// run when setting up each thread, including the main
// thread and the worker threads.

const {
  ArrayPrototypeEvery,
  ArrayPrototypeForEach,
  ArrayPrototypeIncludes,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  ArrayPrototypeSplice,
  BigUint64Array,
  Float64Array,
  NumberMAX_SAFE_INTEGER,
  ObjectFreeze,
  ReflectApply,
  RegExpPrototypeTest,
  SafeArrayIterator,
  Set,
  SetPrototypeEntries,
  SetPrototypeValues,
  StringPrototypeEndsWith,
  StringPrototypeReplace,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  Symbol,
  SymbolIterator,
  Uint32Array,
} = primordials;

const {
  errnoException,
  codes: {
    ERR_ASSERTION,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_OUT_OF_RANGE,
    ERR_UNKNOWN_SIGNAL
  }
} = require('internal/errors');
const format = require('internal/util/inspect').format;
const {
  validateArray,
  validateNumber,
  validateObject,
} = require('internal/validators');
const constants = internalBinding('constants').os.signals;

const {
  handleProcessExit,
} = require('internal/modules/esm/handle_process_exit');

const kInternal = Symbol('internal properties');

function assert(x, msg) {
  if (!x) throw new ERR_ASSERTION(msg || 'assertion error');
}

const binding = internalBinding('process_methods');

let hrValues;
let hrBigintValues;

function refreshHrtimeBuffer() {
  // The 3 entries filled in by the original process.hrtime contains
  // the upper/lower 32 bits of the second part of the value,
  // and the remaining nanoseconds of the value.
  hrValues = new Uint32Array(binding.hrtimeBuffer);
  // Use a BigUint64Array in the closure because this is actually a bit
  // faster than simply returning a BigInt from C++ in V8 7.1.
  hrBigintValues = new BigUint64Array(binding.hrtimeBuffer, 0, 1);
}

// Create the buffers.
refreshHrtimeBuffer();

function hrtime(time) {
  binding.hrtime();

  if (time !== undefined) {
    validateArray(time, 'time');
    if (time.length !== 2) {
      throw new ERR_OUT_OF_RANGE('time', 2, time.length);
    }

    const sec = (hrValues[0] * 0x100000000 + hrValues[1]) - time[0];
    const nsec = hrValues[2] - time[1];
    const needsBorrow = nsec < 0;
    return [needsBorrow ? sec - 1 : sec, needsBorrow ? nsec + 1e9 : nsec];
  }

  return [
    hrValues[0] * 0x100000000 + hrValues[1],
    hrValues[2],
  ];
}

function hrtimeBigInt() {
  binding.hrtimeBigInt();
  return hrBigintValues[0];
}

// The execution of this function itself should not cause any side effects.
function wrapProcessMethods(binding) {
  const {
    cpuUsage: _cpuUsage,
    memoryUsage: _memoryUsage,
    rss,
    resourceUsage: _resourceUsage
  } = binding;

  function _rawDebug(...args) {
    binding._rawDebug(ReflectApply(format, null, args));
  }

  // Create the argument array that will be passed to the native function.
  const cpuValues = new Float64Array(2);

  // Replace the native function with the JS version that calls the native
  // function.
  function cpuUsage(prevValue) {
    // If a previous value was passed in, ensure it has the correct shape.
    if (prevValue) {
      if (!previousValueIsValid(prevValue.user)) {
        validateObject(prevValue, 'prevValue');

        validateNumber(prevValue.user, 'prevValue.user');
        throw new ERR_INVALID_ARG_VALUE.RangeError('prevValue.user',
                                                   prevValue.user);
      }

      if (!previousValueIsValid(prevValue.system)) {
        validateNumber(prevValue.system, 'prevValue.system');
        throw new ERR_INVALID_ARG_VALUE.RangeError('prevValue.system',
                                                   prevValue.system);
      }
    }

    // Call the native function to get the current values.
    _cpuUsage(cpuValues);

    // If a previous value was passed in, return diff of current from previous.
    if (prevValue) {
      return {
        user: cpuValues[0] - prevValue.user,
        system: cpuValues[1] - prevValue.system
      };
    }

    // If no previous value passed in, return current value.
    return {
      user: cpuValues[0],
      system: cpuValues[1]
    };
  }

  // Ensure that a previously passed in value is valid. Currently, the native
  // implementation always returns numbers <= Number.MAX_SAFE_INTEGER.
  function previousValueIsValid(num) {
    return typeof num === 'number' &&
        num <= NumberMAX_SAFE_INTEGER &&
        num >= 0;
  }

  const memValues = new Float64Array(5);
  function memoryUsage() {
    _memoryUsage(memValues);
    return {
      rss: memValues[0],
      heapTotal: memValues[1],
      heapUsed: memValues[2],
      external: memValues[3],
      arrayBuffers: memValues[4]
    };
  }

  memoryUsage.rss = rss;

  function exit(code) {
    process.off('exit', handleProcessExit);

    if (code || code === 0)
      process.exitCode = code;

    if (!process._exiting) {
      process._exiting = true;
      process.emit('exit', process.exitCode || 0);
    }
    // FIXME(joyeecheung): This is an undocumented API that gets monkey-patched
    // in the user land. Either document it, or deprecate it in favor of a
    // better public alternative.
    process.reallyExit(process.exitCode || 0);
  }

  function kill(pid, sig) {
    let err;

    // eslint-disable-next-line eqeqeq
    if (pid != (pid | 0)) {
      throw new ERR_INVALID_ARG_TYPE('pid',
                                     'number', pid);
    }

    // Preserve null signal
    if (sig === (sig | 0)) {
      // XXX(joyeecheung): we have to use process._kill here because
      // it's monkey-patched by tests.
      err = process._kill(pid, sig);
    } else {
      sig = sig || 'SIGTERM';
      if (constants[sig]) {
        err = process._kill(pid, constants[sig]);
      } else {
        throw new ERR_UNKNOWN_SIGNAL(sig);
      }
    }

    if (err)
      throw errnoException(err, 'kill');

    return true;
  }

  const resourceValues = new Float64Array(16);
  function resourceUsage() {
    _resourceUsage(resourceValues);
    return {
      userCPUTime: resourceValues[0],
      systemCPUTime: resourceValues[1],
      maxRSS: resourceValues[2],
      sharedMemorySize: resourceValues[3],
      unsharedDataSize: resourceValues[4],
      unsharedStackSize: resourceValues[5],
      minorPageFault: resourceValues[6],
      majorPageFault: resourceValues[7],
      swappedOut: resourceValues[8],
      fsRead: resourceValues[9],
      fsWrite: resourceValues[10],
      ipcSent: resourceValues[11],
      ipcReceived: resourceValues[12],
      signalsCount: resourceValues[13],
      voluntaryContextSwitches: resourceValues[14],
      involuntaryContextSwitches: resourceValues[15]
    };
  }


  return {
    _rawDebug,
    cpuUsage,
    resourceUsage,
    memoryUsage,
    kill,
    exit
  };
}

const replaceUnderscoresRegex = /_/g;
const leadingDashesRegex = /^--?/;
const trailingValuesRegex = /=.*$/;

class FlagsSet extends Set {
  constructor(array) {
    super();
    const trimLeadingDashes =
      (flag) => StringPrototypeReplace(flag, leadingDashesRegex, '');

    // Save these for comparison against flags provided to
    // process.allowedNodeEnvironmentFlags.has() which lack leading dashes.
    const withoutDashes = ArrayPrototypeMap(array,
                                            trimLeadingDashes);
    this[kInternal] = { array, withoutDashes };
  }

  add() {
    // No-op, `Set` API compatible
    return this;
  }

  delete() {
    // No-op, `Set` API compatible
    return false;
  }

  clear() {
    // No-op, `Set` API compatible
  }

  has(key) {
    // This will return `true` based on various possible
    // permutations of a flag, including present/missing leading
    // dash(es) and/or underscores-for-dashes.
    // Strips any values after `=`, inclusive.
    // TODO(addaleax): It might be more flexible to run the option parser
    // on a dummy option set and see whether it rejects the argument or
    // not.
    if (typeof key === 'string') {
      key = StringPrototypeReplace(key, replaceUnderscoresRegex, '-');
      if (RegExpPrototypeTest(leadingDashesRegex, key)) {
        key = StringPrototypeReplace(key, trailingValuesRegex, '');
        return ArrayPrototypeIncludes(this[kInternal].array, key);
      }
      return ArrayPrototypeIncludes(this[kInternal].withoutDashes, key);
    }
    return false;
  }

  entries() {
    this[kInternal].set ??=
      new Set(new SafeArrayIterator(this[kInternal].array));
    return SetPrototypeEntries(this[kInternal].set);
  }

  forEach(callback, thisArg = undefined) {
    ArrayPrototypeForEach(
      this[kInternal].array,
      (v) => ReflectApply(callback, thisArg, [v, v, this])
    );
  }

  get size() {
    return this[kInternal].array.length;
  }

  values() {
    this[kInternal].set ??=
      new Set(new SafeArrayIterator(this[kInternal].array));
    return SetPrototypeValues(this[kInternal].set);
  }
}
FlagsSet.prototype.keys =
FlagsSet.prototype[SymbolIterator] =
  FlagsSet.prototype.values;

ObjectFreeze(FlagsSet.prototype.constructor);
ObjectFreeze(FlagsSet.prototype);

// This builds the initial process.allowedNodeEnvironmentFlags
// from data in the config binding.
function buildAllowedFlags() {
  const {
    envSettings: { kAllowedInEnvironment },
    types: { kBoolean },
  } = internalBinding('options');
  const { options, aliases } = require('internal/options');

  const allowedNodeEnvironmentFlags = [];
  for (const { 0: name, 1: info } of options) {
    if (info.envVarSettings === kAllowedInEnvironment) {
      ArrayPrototypePush(allowedNodeEnvironmentFlags, name);
      if (info.type === kBoolean) {
        const negatedName = `--no-${name.slice(2)}`;
        ArrayPrototypePush(allowedNodeEnvironmentFlags, negatedName);
      }
    }
  }

  function isAccepted(to) {
    if (!StringPrototypeStartsWith(to, '-') || to === '--') return true;
    const recursiveExpansion = aliases.get(to);
    if (recursiveExpansion) {
      if (recursiveExpansion[0] === to)
        ArrayPrototypeSplice(recursiveExpansion, 0, 1);
      return ArrayPrototypeEvery(recursiveExpansion, isAccepted);
    }
    return options.get(to).envVarSettings === kAllowedInEnvironment;
  }
  for (const { 0: from, 1: expansion } of aliases) {
    if (ArrayPrototypeEvery(expansion, isAccepted)) {
      let canonical = from;
      if (StringPrototypeEndsWith(canonical, '='))
        canonical = StringPrototypeSlice(canonical, 0, canonical.length - 1);
      if (StringPrototypeEndsWith(canonical, ' <arg>'))
        canonical = StringPrototypeSlice(canonical, 0, canonical.length - 4);
      ArrayPrototypePush(allowedNodeEnvironmentFlags, canonical);
    }
  }

  return ObjectFreeze(new FlagsSet(
    allowedNodeEnvironmentFlags
  ));
}

// This builds the initial process.availableV8Flags.
function buildAvailableV8Flags() {

  const v8Flags = [
    '--abort-on-contradictory-flags',
    '--abort-on-uncaught-exception',
    '--adjust-os-scheduling-parameters',
    '--allocation-buffer-parking',
    '--allocation-site-pretenuring',
    '--allow-natives-for-differential-fuzzing',
    '--allow-natives-syntax',
    '--allow-overwriting-for-next-flag',
    '--allow-unsafe-function-constructor',
    '--always-compact',
    '--always-opt',
    '--always-osr',
    '--always-sparkplug',
    '--analyze-environment-liveness',
    '--arm-arch',
    '--asm-wasm-lazy-compilation',
    '--assert-types',
    '--async-stack-traces',
    '--baseline-batch-compilation',
    '--baseline-batch-compilation-threshold',
    '--budget-for-feedback-vector-allocation',
    '--builtin-subclassing',
    '--builtins-in-stack-traces',
    '--bytecode-size-allowance-per-tick',
    '--bytecode-size-allowance-per-tick',
    '--cache-prototype-transitions',
    '--clear-exceptions-on-js-entry',
    '--clear-free-memory',
    '--compact-code-space',
    '--compilation-cache',
    '--concurrent-array-buffer-sweeping',
    '--concurrent-cache-deserialization',
    '--concurrent-cache-deserialization',
    '--concurrent-inlining',
    '--concurrent-marking',
    '--concurrent-recompilation',
    '--concurrent-recompilation-delay',
    '--concurrent-recompilation-queue-length',
    '--concurrent-sweeping',
    '--correctness-fuzzer-suppressions',
    '--cpu-profiler-sampling-interval',
    '--crash-on-aborted-evacuation',
    '--csa-trap-on-node',
    '--default-to-experimental-regexp-engine',
    '--deopt-every-n-times',
    '--detailed-error-stack-trace',
    '--detailed-line-info',
    '--detect-ineffective-gcs-near-heap-limit',
    '--disable-abortjs',
    '--disable-old-api-accessors',
    '--disallow-code-generation-from-strings',
    '--dump-counters',
    '--dump-counters-nvp',
    '--dump-wasm-module-path',
    '--embedded-src',
    '--embedded-variant',
    '--embedder-instance-types',
    '--enable-32dregs',
    '--enable-armv7',
    '--enable-armv8',
    '--enable-avx',
    '--enable-avx2',
    '--enable-bmi1',
    '--enable-bmi2',
    '--enable-experimental-regexp-engine',
    '--enable-experimental-regexp-engine-on-excessive-backtracks',
    '--enable-fma3',
    '--enable-lazy-source-positions',
    '--enable-lzcnt',
    '--enable-mega-dom-ic',
    '--enable-neon',
    '--enable-popcnt',
    '--enable-regexp-unaligned-accesses',
    '--enable-sahf',
    '--enable-sharedarraybuffer-per-context',
    '--enable-source-at-csa-bind',
    '--enable-sse3',
    '--enable-sse4-1',
    '--enable-sse4-2',
    '--enable-ssse3',
    '--enable-sudiv',
    '--enable-system-instrumentation',
    '--enable-vfp3',
    '--ephemeron-fixpoint-iterations',
    '--experimental-flush-embedded-blob-icache',
    '--experimental-stack-trace-frames',
    '--experimental-wasm-allow-huge-modules',
    '--experimental-wasm-branch-hinting',
    '--experimental-wasm-compilation-hints',
    '--experimental-wasm-eh',
    '--experimental-wasm-gc',
    '--experimental-wasm-memory64',
    '--experimental-wasm-nn-locals',
    '--experimental-wasm-reftypes',
    '--experimental-wasm-relaxed-simd',
    '--experimental-wasm-return-call',
    '--experimental-wasm-simd',
    '--experimental-wasm-stack-switching',
    '--experimental-wasm-threads',
    '--experimental-wasm-type-reflection',
    '--experimental-wasm-typed-funcref',
    '--experimental-wasm-unsafe-nn-locals',
    '--expose-async-hooks',
    '--expose-cputracemark-as',
    '--expose-externalize-string',
    '--expose-gc',
    '--expose-gc-as',
    '--expose-ignition-statistics',
    '--expose-inspector-scripts',
    '--expose-trigger-failure',
    '--expose-wasm',
    '--fast-promotion-new-space',
    '--feedback-allocation-on-bytecode-size',
    '--feedback-normalization',
    '--finalize-streaming-on-background',
    '--flush-baseline-code',
    '--flush-bytecode',
    '--force-long-branches',
    '--force-marking-deque-overflows',
    '--force-slow-path',
    '--function-context-specialization',
    '--future',
    '--fuzzer-gc-analysis',
    '--fuzzer-random-seed',
    '--fuzzing',
    '--gc-experiment-less-compaction',
    '--gc-fake-mmap',
    '--gc-global',
    '--gc-interval',
    '--gc-stats',
    '--global-gc-scheduling',
    '--hard-abort',
    '--harmony',
    '--harmony-array-find-last',
    '--harmony-atomics',
    '--harmony-class-static-blocks',
    '--harmony-error-cause',
    '--harmony-import-assertions',
    '--harmony-intl-best-fit-matcher',
    '--harmony-intl-dateformat-day-period',
    '--harmony-intl-displaynames-v2',
    '--harmony-intl-enumeration',
    '--harmony-intl-locale-info',
    '--harmony-intl-more-timezone',
    '--harmony-object-has-own',
    '--harmony-private-brand-checks',
    '--harmony-rab-gsab',
    '--harmony-regexp-sequence',
    '--harmony-relative-indexing-methods',
    '--harmony-sharedarraybuffer',
    '--harmony-shipping',
    '--harmony-top-level-await',
    '--harmony-weak-refs-with-cleanup-some',
    '--hash-seed',
    '--heap-growing-percent',
    '--heap-profiler-show-hidden-objects',
    '--heap-profiler-trace-objects',
    '--heap-profiler-use-embedder-graph',
    '--heap-snapshot-string-limit',
    '--help',
    '--histogram-interval',
    '--huge-max-old-generation-size',
    '--icu-timezone-data',
    '--ignition-elide-noneffectful-bytecodes',
    '--ignition-filter-expression-positions',
    '--ignition-reo',
    '--ignition-share-named-property-feedback',
    '--incremental-marking',
    '--incremental-marking-hard-trigger',
    '--incremental-marking-soft-trigger',
    '--incremental-marking-task',
    '--incremental-marking-wrappers',
    '--initial-heap-size',
    '--initial-old-space-size',
    '--inline-new',
    '--interpreted-frames-native-stack',
    '--interrupt-budget',
    '--interrupt-budget-scale-factor-for-top-tier',
    '--isolate-script-cache-ageing',
    '--jitless',
    '--lazy',
    '--lazy-compile-dispatcher',
    '--lazy-eval',
    '--lazy-feedback-allocation',
    '--lazy-new-space-shrinking',
    '--lazy-streaming',
    '--liftoff',
    '--liftoff-only',
    '--lite-mode',
    '--ll-prof',
    '--log',
    '--log-all',
    '--log-api',
    '--log-code',
    '--log-code-disassemble',
    '--log-colour',
    '--log-deopt',
    '--log-function-events',
    '--log-handles',
    '--log-ic',
    '--log-internal-timer-events',
    '--log-maps',
    '--log-maps-details',
    '--log-source-code',
    '--log-suspect',
    '--logfile',
    '--logfile-per-isolate',
    '--manual-evacuation-candidates-selection',
    '--map-counters',
    '--max-bytecode-size-for-early-opt',
    '--max-heap-size',
    '--max-inlined-bytecode-size',
    '--max-inlined-bytecode-size-absolute',
    '--max-inlined-bytecode-size-cumulative',
    '--max-inlined-bytecode-size-small',
    '--max-lazy',
    '--max-old-space-size',
    '--max-optimized-bytecode-size',
    '--max-semi-space-size',
    '--max-serializer-nesting',
    '--max-stack-trace-source-length',
    '--max-valid-polymorphic-map-count',
    '--mcpu',
    '--memory-reducer',
    '--memory-reducer-for-small-heaps',
    '--min-inlining-frequency',
    '--min-semi-space-size',
    '--minor-mc',
    '--minor-mc-trace-fragmentation',
    '--mock-arraybuffer-allocator',
    '--mock-arraybuffer-allocator-limit',
    '--move-object-start',
    '--multi-mapped-mock-allocator',
    '--native-code-counters',
    '--never-compact',
    '--opt',
    '--optimize-for-size',
    '--page-promotion',
    '--page-promotion-threshold',
    '--parallel-compaction',
    '--parallel-compile-tasks',
    '--parallel-marking',
    '--parallel-pointer-update',
    '--parallel-scavenge',
    '--parse-only',
    '--partial-constant-pool',
    '--perf-basic-prof',
    '--perf-basic-prof-only-functions',
    '--perf-prof',
    '--perf-prof-annotate-wasm',
    '--perf-prof-delete-file',
    '--perf-prof-unwinding-info',
    '--polymorphic-inlining',
    '--predictable',
    '--predictable-gc-schedule',
    '--prepare-always-opt',
    '--print-all-code',
    '--print-all-exceptions',
    '--print-builtin-code',
    '--print-builtin-code-filter',
    '--print-builtin-size',
    '--print-bytecode',
    '--print-bytecode-filter',
    '--print-code',
    '--print-code-verbose',
    '--print-deopt-stress',
    '--print-opt-code',
    '--print-opt-code-filter',
    '--print-opt-source',
    '--print-regexp-bytecode',
    '--print-regexp-code',
    '--print-wasm-code',
    '--print-wasm-code-function-index',
    '--print-wasm-stub-code',
    '--prof',
    '--prof-browser-mode',
    '--prof-cpp',
    '--prof-sampling-interval',
    '--profile-deserialization',
    '--random-gc-interval',
    '--random-seed',
    '--randomize-all-allocations',
    '--randomize-hashes',
    '--rcs',
    '--rcs-cpu-time',
    '--reclaim-unmodified-wrappers',
    '--redirect-code-traces',
    '--redirect-code-traces-to',
    '--regexp-backtracks-before-fallback',
    '--regexp-interpret-all',
    '--regexp-optimization',
    '--regexp-peephole-optimization',
    '--regexp-tier-up',
    '--regexp-tier-up-ticks',
    '--rehash-snapshot',
    '--reserve-inline-budget-scale-factor',
    '--retain-maps-for-n-gc',
    '--reuse-opt-code-count',
    '--runtime-call-stats',
    '--sampling-heap-profiler-suppress-randomness',
    '--scale-factor-for-feedback-allocation',
    '--scavenge-separate-stack-scanning',
    '--scavenge-task',
    '--scavenge-task-trigger',
    '--script-delay',
    '--script-delay-fraction',
    '--script-delay-once',
    '--script-streaming',
    '--semi-space-growth-factor',
    '--serialization-statistics',
    '--sim-arm64-optional-features',
    '--single-threaded',
    '--single-threaded-gc',
    '--skip-snapshot-checksum',
    '--slow-histograms',
    '--sparkplug',
    '--sparkplug-filter',
    '--sparkplug-needs-short-builtins',
    '--sparkplug-on-heap',
    '--stack-size',
    '--stack-trace-limit',
    '--stack-trace-on-illegal',
    '--startup-blob',
    '--startup-src',
    '--stress-background-compile',
    '--stress-compaction',
    '--stress-compaction-random',
    '--stress-concurrent-allocation',
    '--stress-concurrent-inlining',
    '--stress-concurrent-inlining-attach-code',
    '--stress-flush-code',
    '--stress-gc-during-compilation',
    '--stress-incremental-marking',
    '--stress-inline',
    '--stress-lazy-source-positions',
    '--stress-marking',
    '--stress-per-context-marking-worklist',
    '--stress-runs',
    '--stress-sampling-allocation-profiler',
    '--stress-scavenge',
    '--stress-snapshot',
    '--stress-turbo-late-spilling',
    '--stress-validate-asm',
    '--stress-wasm-code-gc',
    '--super-ic',
    '--suppress-asm-messages',
    '--switch-table-min-cases',
    '--switch-table-spread-threshold',
    '--target-arch',
    '--target-is-simulator',
    '--target-os',
    '--test-small-max-function-context-stub-size',
    '--testing-bool-flag',
    '--testing-d8-test-runner',
    '--testing-float-flag',
    '--testing-int-flag',
    '--testing-maybe-bool-flag',
    '--testing-prng-seed',
    '--testing-string-flag',
    '--text-is-readable',
    '--ticks-before-optimization',
    '--trace',
    '--trace-all-uses',
    '--trace-allocation-stack-interval',
    '--trace-allocations-origins',
    '--trace-asm-parser',
    '--trace-asm-scanner',
    '--trace-asm-time',
    '--trace-baseline',
    '--trace-baseline-batch-compilation',
    '--trace-block-coverage',
    '--trace-compilation-dependencies',
    '--trace-compiler-dispatcher',
    '--trace-concurrent-marking',
    '--trace-concurrent-recompilation',
    '--trace-creation-allocation-sites',
    '--trace-deopt',
    '--trace-deopt-verbose',
    '--trace-detached-contexts',
    '--trace-duplicate-threshold-kb',
    '--trace-elements-transitions',
    '--trace-environment-liveness',
    '--trace-evacuation',
    '--trace-evacuation-candidates',
    '--trace-experimental-regexp-engine',
    '--trace-file-names',
    '--trace-flush-bytecode',
    '--trace-for-in-enumerate',
    '--trace-fragmentation',
    '--trace-fragmentation-verbose',
    '--trace-gc',
    '--trace-gc-freelists',
    '--trace-gc-freelists-verbose',
    '--trace-gc-ignore-scavenger',
    '--trace-gc-nvp',
    '--trace-gc-object-stats',
    '--trace-gc-verbose',
    '--trace-generalization',
    '--trace-heap-broker',
    '--trace-heap-broker-memory',
    '--trace-heap-broker-verbose',
    '--trace-idle-notification',
    '--trace-idle-notification-verbose',
    '--trace-ignition-codegen',
    '--trace-ignition-dispatches-output-file',
    '--trace-incremental-marking',
    '--trace-migration',
    '--trace-minor-mc-parallel-marking',
    '--trace-mutator-utilization',
    '--trace-opt',
    '--trace-opt-stats',
    '--trace-opt-verbose',
    '--trace-osr',
    '--trace-parallel-scavenge',
    '--trace-pending-allocations',
    '--trace-pretenuring',
    '--trace-pretenuring-statistics',
    '--trace-protector-invalidation',
    '--trace-prototype-users',
    '--trace-rail',
    '--trace-regexp-assembler',
    '--trace-regexp-bytecodes',
    '--trace-regexp-graph',
    '--trace-regexp-parser',
    '--trace-regexp-peephole-optimization',
    '--trace-regexp-tier-up',
    '--trace-representation',
    '--trace-serializer',
    '--trace-side-effect-free-debug-evaluate',
    '--trace-store-elimination',
    '--trace-stress-marking',
    '--trace-stress-scavenge',
    '--trace-track-allocation-sites',
    '--trace-turbo',
    '--trace-turbo-alloc',
    '--trace-turbo-ceq',
    '--trace-turbo-cfg-file',
    '--trace-turbo-filter',
    '--trace-turbo-graph',
    '--trace-turbo-inlining',
    '--trace-turbo-jt',
    '--trace-turbo-load-elimination',
    '--trace-turbo-loop',
    '--trace-turbo-path',
    '--trace-turbo-reduction',
    '--trace-turbo-scheduled',
    '--trace-turbo-scheduler',
    '--trace-turbo-stack-accesses',
    '--trace-turbo-trimming',
    '--trace-turbo-types',
    '--trace-unmapper',
    '--trace-verify-csa',
    '--trace-wasm',
    '--trace-wasm-code-gc',
    '--trace-wasm-memory',
    '--trace-web-snapshot',
    '--trace-zone-stats',
    '--trace-zone-type-stats',
    '--track-detached-contexts',
    '--track-field-types',
    '--track-gc-object-stats',
    '--track-retaining-path',
    '--turbo-allocation-folding',
    '--turbo-cf-optimization',
    '--turbo-collect-feedback-in-generic-lowering',
    '--turbo-compress-translation-arrays',
    '--turbo-dynamic-map-checks',
    '--turbo-escape',
    '--turbo-fast-api-calls',
    '--turbo-filter',
    '--turbo-inline-array-builtins',
    '--turbo-inline-js-wasm-calls',
    '--turbo-inlining',
    '--turbo-instruction-scheduling',
    '--turbo-jt',
    '--turbo-load-elimination',
    '--turbo-loop-peeling',
    '--turbo-loop-rotation',
    '--turbo-loop-variable',
    '--turbo-move-optimization',
    '--turbo-optimize-apply',
    '--turbo-profiling',
    '--turbo-profiling-log-builtins',
    '--turbo-profiling-log-file',
    '--turbo-profiling-verbose',
    '--turbo-rewrite-far-jumps',
    '--turbo-sp-frame-access',
    '--turbo-splitting',
    '--turbo-stats',
    '--turbo-stats-nvp',
    '--turbo-stats-wasm',
    '--turbo-store-elimination',
    '--turbo-stress-instruction-scheduling',
    '--turbo-verify',
    '--turbo-verify-allocation',
    '--turbo-verify-machine-graph',
    '--turboprop',
    '--turboprop-as-toptier',
    '--turboprop-inline-scaling-factor',
    '--turboprop-mid-tier-reg-alloc',
    '--unbox-double-arrays',
    '--use-external-strings',
    '--use-ic',
    '--use-idle-notification',
    '--use-marking-progress-bar',
    '--use-osr',
    '--use-strict',
    '--v8-os-page-size',
    '--validate-asm',
    '--vtune-prof-annotate-wasm',
    '--wasm-async-compilation',
    '--wasm-bounds-checks',
    '--wasm-code-gc',
    '--wasm-debug-mask-for-testing',
    '--wasm-dynamic-tiering',
    '--wasm-enforce-bounds-checks',
    '--wasm-fuzzer-gen-test',
    '--wasm-gc-js-interop',
    '--wasm-generic-wrapper',
    '--wasm-inlining',
    '--wasm-lazy-compilation',
    '--wasm-lazy-validation',
    '--wasm-loop-unrolling',
    '--wasm-math-intrinsics',
    '--wasm-max-code-space',
    '--wasm-max-initial-code-space-reservation',
    '--wasm-max-mem-pages',
    '--wasm-max-table-size',
    '--wasm-memory-protection-keys',
    '--wasm-num-compilation-tasks',
    '--wasm-opt',
    '--wasm-simd-ssse3-codegen',
    '--wasm-stack-checks',
    '--wasm-staging',
    '--wasm-test-streaming',
    '--wasm-tier-mask-for-testing',
    '--wasm-tier-up',
    '--wasm-write-protect-code-memory',
    '--win64-unwinding-info',
    '--write-code-using-rwx',
    '--write-protect-code-memory',
    '--zone-stats-tolerance',
  ];

  return ObjectFreeze(new FlagsSet(v8Flags));
}

// Lazy load internal/trace_events_async_hooks only if the async_hooks
// trace event category is enabled.
let traceEventsAsyncHook;
// Dynamically enable/disable the traceEventsAsyncHook
function toggleTraceCategoryState(asyncHooksEnabled) {
  if (asyncHooksEnabled) {
    if (!traceEventsAsyncHook) {
      traceEventsAsyncHook =
        require('internal/trace_events_async_hooks').createHook();
    }
    traceEventsAsyncHook.enable();
  } else if (traceEventsAsyncHook) {
    traceEventsAsyncHook.disable();
  }
}

module.exports = {
  assert,
  buildAllowedFlags,
  buildAvailableV8Flags,
  hrtime,
  hrtimeBigInt,
  refreshHrtimeBuffer,
  toggleTraceCategoryState,
  wrapProcessMethods,
};
