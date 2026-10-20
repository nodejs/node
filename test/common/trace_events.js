'use strict';

// Helpers to deal with both Chrome legacy JSON trace format, and Perfetto
// binary format.
// This depends on perfetto's `trace_processor_shell` converts to pftrace format
// to JSON format. Run `make tools/perfetto/trace_processor_shell` to download
// it, or point `TRACE_PROCESSOR_SHELL_PATH` at an existing build of it.

const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const common = require('./');

const traceProcessor = path.resolve(
  __dirname, '..', '..',
  process.env.TRACE_PROCESSOR_SHELL_PATH || 'tools/perfetto/trace_processor_shell');

// The JSON form of a trace runs about three times the size of the trace it was
// converted from, and the traces these tests produce are a few hundred KiB at
// most. This is an assumed MAX size of a JSON conversion size limit for tests.
const kMaxTraceJsonBytes = 64 * 1024 * 1024;

const traceFileExt = common.hasPerfetto ? 'pftrace' : 'log';
const defaultTraceFileName = `node_trace.1.${traceFileExt}`;

// Only perfetto traces need converting, so a missing `trace_processor_shell`
// does not stop anything on a legacy build.
function checkTraceProcessor() {
  if (common.hasPerfetto && !fs.existsSync(traceProcessor)) {
    assert.fail('trace_processor_shell is missing, ' +
                'run `make tools/perfetto/trace_processor_shell` to download it');
  }
}

function readTraceEvents(file) {
  if (!common.hasPerfetto) {
    return JSON.parse(fs.readFileSync(file, 'utf8')).traceEvents;
  }

  const converted = spawnSync(traceProcessor, ['convert', 'json', file],
                              { maxBuffer: kMaxTraceJsonBytes });
  assert.ifError(converted.error);
  assert.strictEqual(
    converted.status, 0,
    `trace_processor_shell failed: ${converted.stderr}`);
  return JSON.parse(converted.stdout.toString()).traceEvents;
}

// A perfetto trace event carries the single category it was emitted with. The
// legacy backend instead groups it with every ancestor category, so
// `node.net.native` is recorded as `node,node.net,node.net.native`.
function traceCategory(name) {
  if (common.hasPerfetto) {
    return name;
  }
  const parts = name.split('.');
  return parts.map((_, i) => parts.slice(0, i + 1).join('.')).join(',');
}

module.exports = {
  defaultTraceFileName,
  readTraceEvents,
  checkTraceProcessor,
  traceCategory,
};
