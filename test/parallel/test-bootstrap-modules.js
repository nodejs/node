// Flags: --expose-internals
'use strict';

// This list must be computed before we require any modules to
// to eliminate the noise.
const actualModules = new Set(process.moduleLoadList.slice());

const common = require('../common');
const assert = require('assert');

const expectedModules = new Set([
  'Internal Binding async_wrap',
  'Internal Binding block_list',
  'Internal Binding buffer',
  'Internal Binding config',
  'Internal Binding constants',
  'Internal Binding contextify',
  'Internal Binding credentials',
  'Internal Binding errors',
  'Internal Binding fs_dir',
  'Internal Binding fs_event_wrap',
  'Internal Binding fs',
  'Internal Binding heap_utils',
  'Internal Binding messaging',
  'Internal Binding module_wrap',
  'Internal Binding native_module',
  'Internal Binding options',
  'Internal Binding performance',
  'Internal Binding pipe_wrap',
  'Internal Binding process_methods',
  'Internal Binding report',
  'Internal Binding serdes',
  'Internal Binding stream_wrap',
  'Internal Binding string_decoder',
  'Internal Binding symbols',
  'Internal Binding task_queue',
  'Internal Binding tcp_wrap',
  'Internal Binding timers',
  'Internal Binding trace_events',
  'Internal Binding types',
  'Internal Binding url',
  'Internal Binding util',
  'Internal Binding uv',
  'Internal Binding v8',
  'Internal Binding wasm_web_api',
  'Internal Binding worker',
  'NativeModule buffer',
  'NativeModule events',
  'NativeModule fs',
  'NativeModule internal/abort_controller',
  'NativeModule internal/assert',
  'NativeModule internal/async_hooks',
  'NativeModule internal/blocklist',
  'NativeModule internal/bootstrap/pre_execution',
  'NativeModule internal/buffer',
  'NativeModule internal/console/constructor',
  'NativeModule internal/console/global',
  'NativeModule internal/constants',
  'NativeModule internal/dtrace',
  'NativeModule internal/encoding',
  'NativeModule internal/errors',
  'NativeModule internal/event_target',
  'NativeModule internal/fixed_queue',
  'NativeModule internal/fs/dir',
  'NativeModule internal/fs/promises',
  'NativeModule internal/fs/read_file_context',
  'NativeModule internal/fs/rimraf',
  'NativeModule internal/fs/utils',
  'NativeModule internal/fs/watchers',
  'NativeModule internal/heap_utils',
  'NativeModule internal/histogram',
  'NativeModule internal/idna',
  'NativeModule internal/linkedlist',
  'NativeModule internal/modules/cjs/helpers',
  'NativeModule internal/modules/cjs/loader',
  'NativeModule internal/modules/esm/assert',
  'NativeModule internal/modules/esm/create_dynamic_module',
  'NativeModule internal/modules/esm/fetch_module',
  'NativeModule internal/modules/esm/formats',
  'NativeModule internal/modules/esm/get_format',
  'NativeModule internal/modules/esm/handle_process_exit',
  'NativeModule internal/modules/esm/initialize_import_meta',
  'NativeModule internal/modules/esm/load',
  'NativeModule internal/modules/esm/loader',
  'NativeModule internal/modules/esm/module_job',
  'NativeModule internal/modules/esm/module_map',
  'NativeModule internal/modules/esm/resolve',
  'NativeModule internal/modules/esm/translators',
  'NativeModule internal/modules/package_json_reader',
  'NativeModule internal/modules/run_main',
  'NativeModule internal/net',
  'NativeModule internal/options',
  'NativeModule internal/perf/event_loop_delay',
  'NativeModule internal/perf/event_loop_utilization',
  'NativeModule internal/perf/nodetiming',
  'NativeModule internal/perf/observe',
  'NativeModule internal/perf/performance_entry',
  'NativeModule internal/perf/performance',
  'NativeModule internal/perf/timerify',
  'NativeModule internal/perf/usertiming',
  'NativeModule internal/perf/resource_timing',
  'NativeModule internal/perf/utils',
  'NativeModule internal/priority_queue',
  'NativeModule internal/process/esm_loader',
  'NativeModule internal/process/execution',
  'NativeModule internal/process/per_thread',
  'NativeModule internal/process/promises',
  'NativeModule internal/process/report',
  'NativeModule internal/process/signal',
  'NativeModule internal/process/task_queues',
  'NativeModule internal/process/warning',
  'NativeModule internal/promise_hooks',
  'NativeModule internal/querystring',
  'NativeModule internal/socketaddress',
  'NativeModule internal/source_map/source_map_cache',
  'NativeModule internal/stream_base_commons',
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
  'NativeModule internal/structured_clone',
  'NativeModule internal/timers',
  'NativeModule internal/url',
  'NativeModule internal/util',
  'NativeModule internal/util/debuglog',
  'NativeModule internal/util/inspect',
  'NativeModule internal/util/iterable_weak_map',
  'NativeModule internal/util/parse_args/utils',
  'NativeModule internal/util/parse_args/parse_args',
  'NativeModule internal/util/types',
  'NativeModule internal/validators',
  'NativeModule internal/vm/module',
  'NativeModule internal/wasm_web_api',
  'NativeModule internal/webstreams/adapters',
  'NativeModule internal/webstreams/compression',
  'NativeModule internal/webstreams/encoding',
  'NativeModule internal/webstreams/queuingstrategies',
  'NativeModule internal/webstreams/readablestream',
  'NativeModule internal/webstreams/transformstream',
  'NativeModule internal/webstreams/util',
  'NativeModule internal/webstreams/writablestream',
  'NativeModule internal/worker/io',
  'NativeModule internal/worker/js_transferable',
  'Internal Binding blob',
  'NativeModule internal/blob',
  'NativeModule async_hooks',
  'NativeModule net',
  'NativeModule path',
  'NativeModule perf_hooks',
  'NativeModule querystring',
  'NativeModule stream',
  'NativeModule stream/promises',
  'NativeModule string_decoder',
  'NativeModule timers',
  'NativeModule url',
  'NativeModule util',
  'NativeModule v8',
  'NativeModule vm',
]);

if (!common.isMainThread) {
  [
    'Internal Binding messaging',
    'Internal Binding performance',
    'Internal Binding symbols',
    'Internal Binding worker',
    'NativeModule internal/streams/duplex',
    'NativeModule internal/streams/passthrough',
    'NativeModule internal/streams/readable',
    'NativeModule internal/streams/transform',
    'NativeModule internal/streams/writable',
    'NativeModule internal/error_serdes',
    'NativeModule internal/process/worker_thread_only',
    'NativeModule internal/streams/buffer_list',
    'NativeModule internal/streams/destroy',
    'NativeModule internal/streams/end-of-stream',
    'NativeModule internal/streams/legacy',
    'NativeModule internal/streams/pipeline',
    'NativeModule internal/streams/state',
    'NativeModule internal/worker',
    'NativeModule internal/worker/io',
    'NativeModule stream',
    'NativeModule worker_threads',
  ].forEach(expectedModules.add.bind(expectedModules));
}

if (common.hasIntl) {
  expectedModules.add('Internal Binding icu');
} else {
  expectedModules.add('NativeModule url');
}

if (process.features.inspector) {
  expectedModules.add('Internal Binding inspector');
  expectedModules.add('NativeModule internal/inspector_async_hook');
  expectedModules.add('NativeModule internal/util/inspector');
  expectedModules.add('Internal Binding profiler');
}

if (process.env.NODE_V8_COVERAGE) {
  expectedModules.add('Internal Binding profiler');
}

if (common.hasCrypto) {
  expectedModules.add('Internal Binding crypto');
  expectedModules.add('NativeModule crypto');
  expectedModules.add('NativeModule internal/crypto/certificate');
  expectedModules.add('NativeModule internal/crypto/cipher');
  expectedModules.add('NativeModule internal/crypto/diffiehellman');
  expectedModules.add('NativeModule internal/crypto/hash');
  expectedModules.add('NativeModule internal/crypto/hashnames');
  expectedModules.add('NativeModule internal/crypto/hkdf');
  expectedModules.add('NativeModule internal/crypto/keygen');
  expectedModules.add('NativeModule internal/crypto/keys');
  expectedModules.add('NativeModule internal/crypto/pbkdf2');
  expectedModules.add('NativeModule internal/crypto/random');
  expectedModules.add('NativeModule internal/crypto/scrypt');
  expectedModules.add('NativeModule internal/crypto/sig');
  expectedModules.add('NativeModule internal/crypto/util');
  expectedModules.add('NativeModule internal/crypto/x509');
  expectedModules.add('NativeModule internal/streams/lazy_transform');
}

const { internalBinding } = require('internal/test/binding');
if (internalBinding('config').hasDtrace) {
  expectedModules.add('Internal Binding dtrace');
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
