// Flags: --expose-internals
'use strict';

// This list must be computed before we require any modules to
// to eliminate the noise.
const actualModules = new Set(process.moduleLoadList.slice());

const common = require('../common');
const assert = require('assert');

const expectedModules = new Set([
  'Internal Binding async_wrap',
  'Internal Binding buffer',
  'Internal Binding builtins',
  'Internal Binding config',
  'Internal Binding constants',
  'Internal Binding contextify',
  'Internal Binding credentials',
  'Internal Binding errors',
  'Internal Binding fs',
  'Internal Binding mksnapshot',
  'Internal Binding messaging',
  'Internal Binding module_wrap',
  'Internal Binding options',
  'Internal Binding performance',
  'Internal Binding process_methods',
  'Internal Binding string_decoder',
  'Internal Binding symbols',
  'Internal Binding task_queue',
  'Internal Binding timers',
  'Internal Binding trace_events',
  'Internal Binding types',
  'Internal Binding url',
  'Internal Binding util',
  'Internal Binding wasm_web_api',
  'Internal Binding worker',
  'NativeModule buffer',
  'NativeModule events',
  'NativeModule fs',
  'NativeModule internal/assert',
  'NativeModule internal/async_hooks',
  'NativeModule internal/buffer',
  'NativeModule internal/console/constructor',
  'NativeModule internal/console/global',
  'NativeModule internal/constants',
  'NativeModule internal/dns/utils',
  'NativeModule internal/errors',
  'NativeModule internal/event_target',
  'NativeModule internal/fixed_queue',
  'NativeModule internal/fs/utils',
  'NativeModule internal/linkedlist',
  'NativeModule internal/modules/cjs/loader',
  'NativeModule internal/modules/esm/utils',
  'NativeModule internal/modules/helpers',
  'NativeModule internal/modules/package_json_reader',
  'NativeModule internal/modules/run_main',
  'NativeModule internal/net',
  'NativeModule internal/options',
  'NativeModule internal/perf/utils',
  'NativeModule internal/priority_queue',
  'NativeModule internal/process/execution',
  'NativeModule internal/process/per_thread',
  'NativeModule internal/process/pre_execution',
  'NativeModule internal/process/promises',
  'NativeModule internal/process/signal',
  'NativeModule internal/process/task_queues',
  'NativeModule internal/process/warning',
  'NativeModule internal/querystring',
  'NativeModule internal/source_map/source_map_cache',
  'NativeModule internal/timers',
  'NativeModule internal/url',
  'NativeModule internal/util',
  'NativeModule internal/util/debuglog',
  'NativeModule internal/util/inspect',
  'NativeModule internal/util/types',
  'NativeModule internal/validators',
  'NativeModule internal/vm',
  'NativeModule internal/vm/module',
  'NativeModule internal/webidl',
  'NativeModule internal/worker/js_transferable',
  'Internal Binding blob',
  'NativeModule async_hooks',
  'NativeModule path',
  'NativeModule querystring',
  'NativeModule timers',
  'NativeModule internal/v8/startup_snapshot',
  'NativeModule util',
]);

if (common.isMainThread) {
  [
    'NativeModule internal/idna',
    'NativeModule url',
  ].forEach(expectedModules.add.bind(expectedModules));
} else {
  [
    'Internal Binding messaging',
    'Internal Binding performance',
    'Internal Binding symbols',
    'Internal Binding worker',
    'NativeModule internal/abort_controller',
    'NativeModule internal/error_serdes',
    'NativeModule internal/event_target',
    'NativeModule internal/process/worker_thread_only',
    'NativeModule internal/streams/add-abort-signal',
    'NativeModule internal/streams/buffer_list',
    'NativeModule internal/streams/compose',
    'NativeModule internal/streams/destroy',
    'NativeModule internal/streams/duplex',
    'NativeModule internal/streams/end-of-stream',
    'NativeModule internal/streams/from',
    'NativeModule internal/streams/legacy',
    'NativeModule internal/streams/operators',
    'NativeModule internal/streams/passthrough',
    'NativeModule internal/streams/pipeline',
    'NativeModule internal/streams/readable',
    'NativeModule internal/streams/state',
    'NativeModule internal/streams/transform',
    'NativeModule internal/streams/utils',
    'NativeModule internal/streams/writable',
    'NativeModule internal/worker',
    'NativeModule internal/worker/io',
    'NativeModule worker_threads',
    'NativeModule stream',
    'NativeModule stream/promises',
    'NativeModule string_decoder',
    'NativeModule util',
  ].forEach(expectedModules.add.bind(expectedModules));
}

if (common.isWindows) {
  // On Windows fs needs SideEffectFreeRegExpPrototypeExec which uses vm.
  expectedModules.add('NativeModule vm');
}

if (common.hasIntl) {
  expectedModules.add('Internal Binding icu');
}

if (process.features.inspector) {
  expectedModules.add('Internal Binding inspector');
  expectedModules.add('NativeModule internal/inspector_async_hook');
  expectedModules.add('NativeModule internal/util/inspector');
}

const difference = (setA, setB) => {
  return new Set([...setA].filter((x) => !setB.has(x)));
};
const missingModules = difference(expectedModules, actualModules);
const extraModules = difference(actualModules, expectedModules);
const printSet = (s) => { return `${[...s].sort().join(',\n  ')}\n`; };

assert.deepStrictEqual(actualModules, expectedModules,
                       (missingModules.size > 0 ?
                         'These modules were not loaded:\n  ' +
                         printSet(missingModules) : '') +
                       (extraModules.size > 0 ?
                         'These modules were unexpectedly loaded:\n  ' +
                         printSet(extraModules) : ''));
