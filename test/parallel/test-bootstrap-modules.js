// Flags: --expose-internals
'use strict';

// This list must be computed before we require any modules to
// to eliminate the noise.
const actualModules = new Set(process.moduleLoadList.slice());

const common = require('../common');
const assert = require('assert');

const expectedModules = new Set([
  'Internal Binding builtins',
  'Internal Binding encoding_binding',
  'Internal Binding errors',
  'Internal Binding util',
  'NativeModule internal/errors',
  'Internal Binding config',
  'Internal Binding timers',
  'Internal Binding async_wrap',
  'Internal Binding task_queue',
  'Internal Binding symbols',
  'NativeModule internal/async_hooks',
  'Internal Binding constants',
  'Internal Binding types',
  'NativeModule internal/util',
  'NativeModule internal/util/types',
  'NativeModule internal/validators',
  'NativeModule internal/linkedlist',
  'NativeModule internal/priority_queue',
  'NativeModule internal/assert',
  'NativeModule internal/util/inspect',
  'NativeModule internal/util/debuglog',
  'NativeModule internal/timers',
  'NativeModule events',
  'Internal Binding buffer',
  'Internal Binding string_decoder',
  'NativeModule internal/buffer',
  'NativeModule buffer',
  'Internal Binding messaging',
  'NativeModule internal/worker/js_transferable',
  'Internal Binding process_methods',
  'NativeModule internal/process/per_thread',
  'Internal Binding credentials',
  'NativeModule internal/process/promises',
  'NativeModule internal/fixed_queue',
  'NativeModule async_hooks',
  'NativeModule internal/process/task_queues',
  'NativeModule timers',
  'Internal Binding trace_events',
  'NativeModule internal/constants',
  'NativeModule path',
  'NativeModule internal/process/execution',
  'NativeModule internal/process/warning',
  'NativeModule internal/console/constructor',
  'NativeModule internal/console/global',
  'NativeModule internal/querystring',
  'NativeModule querystring',
  'Internal Binding url',
  'Internal Binding blob',
  'NativeModule internal/url',
  'NativeModule util',
  'Internal Binding performance',
  'NativeModule internal/perf/utils',
  'NativeModule internal/event_target',
  'Internal Binding mksnapshot',
  'NativeModule internal/v8/startup_snapshot',
  'NativeModule internal/process/signal',
  'Internal Binding fs',
  'NativeModule internal/encoding',
  'NativeModule internal/webstreams/util',
  'NativeModule internal/webstreams/queuingstrategies',
  'NativeModule internal/blob',
  'NativeModule internal/fs/utils',
  'NativeModule fs',
  'NativeModule internal/idna',
  'Internal Binding options',
  'NativeModule internal/options',
  'NativeModule internal/source_map/source_map_cache',
  'Internal Binding contextify',
  'NativeModule internal/vm',
  'NativeModule internal/modules/helpers',
  'NativeModule internal/modules/package_json_reader',
  'Internal Binding module_wrap',
  'NativeModule internal/modules/cjs/loader',
  'NativeModule internal/vm/module',
  'NativeModule internal/modules/esm/utils',
  'Internal Binding wasm_web_api',
  'Internal Binding worker',
  'NativeModule internal/modules/run_main',
  'NativeModule internal/net',
  'NativeModule internal/dns/utils',
  'NativeModule internal/process/pre_execution',
]);

if (!common.isMainThread) {
  [
    'NativeModule diagnostics_channel',
    'NativeModule internal/abort_controller',
    'NativeModule internal/error_serdes',
    'NativeModule internal/perf/event_loop_utilization',
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
    'NativeModule stream',
    'NativeModule stream/promises',
    'NativeModule string_decoder',
    'NativeModule worker_threads',
  ].forEach(expectedModules.add.bind(expectedModules));
}

if (common.isWindows) {
  // On Windows fs needs SideEffectFreeRegExpPrototypeExec which uses vm.
  expectedModules.add('NativeModule vm');
}

if (common.hasIntl) {
  expectedModules.add('Internal Binding icu');
  expectedModules.add('NativeModule url');
} else {
  expectedModules.add('NativeModule url');
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
