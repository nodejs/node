// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {dingus} from 'dingusjs';

import {assertExists} from '../base/logging';
import {TraceConfig} from '../common/protos';
import {createEmptyRecordConfig, RecordConfig} from '../common/state';

import {App} from './globals';
import {genConfigProto, RecordController, toPbtxt} from './record_controller';

test('encodeConfig', () => {
  const config = createEmptyRecordConfig();
  config.durationSeconds = 10;
  const result = TraceConfig.decode(genConfigProto(config));
  expect(result.durationMs).toBe(10000);
});

test('SysConfig', () => {
  const config = createEmptyRecordConfig();
  config.cpuSyscall = true;
  const result = TraceConfig.decode(genConfigProto(config));
  const sources = assertExists(result.dataSources);
  const srcConfig = assertExists(sources[0].config);
  const ftraceConfig = assertExists(srcConfig.ftraceConfig);
  const ftraceEvents = assertExists(ftraceConfig.ftraceEvents);
  expect(ftraceEvents.includes('raw_syscalls/sys_enter')).toBe(true);
  expect(ftraceEvents.includes('raw_syscalls/sys_exit')).toBe(true);
});

test('toPbtxt', () => {
  const config = {
    durationMs: 1000,
    maxFileSizeBytes: 43,
    buffers: [
      {
        sizeKb: 42,
      },
    ],
    dataSources: [{
      config: {
        name: 'linux.ftrace',
        targetBuffer: 1,
        ftraceConfig: {
          ftraceEvents: ['sched_switch', 'print'],
        },
      },
    }],
    producers: [
      {
        producerName: 'perfetto.traced_probes',
      },
    ],
  };

  const text = toPbtxt(TraceConfig.encode(config).finish());

  expect(text).toEqual(`buffers: {
    size_kb: 42
}
data_sources: {
    config {
        name: "linux.ftrace"
        target_buffer: 1
        ftrace_config {
            ftrace_events: "sched_switch"
            ftrace_events: "print"
        }
    }
}
duration_ms: 1000
producers: {
    producer_name: "perfetto.traced_probes"
}
max_file_size_bytes: 43
`);
});

test('RecordController', () => {
  const app = dingus<App>('globals');
  (app.state.recordConfig as RecordConfig) = createEmptyRecordConfig();
  const m: MessageChannel = dingus<MessageChannel>('extensionPort');
  const controller = new RecordController({app, extensionPort: m.port1});
  controller.run();
  controller.run();
  controller.run();
  // tslint:disable-next-line no-any
  const calls = app.calls.filter((call: any) => call[0] === 'publish()');
  expect(calls.length).toBe(1);
  // TODO(hjd): Fix up dingus to have a more sensible API.
  expect(calls[0][1][0]).toEqual('TrackData');
});

test('ChromeConfig', () => {
  const config = createEmptyRecordConfig();
  config.ipcFlows = true;
  config.jsExecution = true;
  config.mode = 'STOP_WHEN_FULL';
  const result = TraceConfig.decode(genConfigProto(config));
  const sources = assertExists(result.dataSources);

  const traceConfigSource = assertExists(sources[0].config);
  expect(traceConfigSource.name).toBe('org.chromium.trace_event');
  const chromeConfig = assertExists(traceConfigSource.chromeConfig);
  const traceConfig = assertExists(chromeConfig.traceConfig);

  const metadataConfigSource = assertExists(sources[1].config);
  expect(metadataConfigSource.name).toBe('org.chromium.trace_metadata');
  const chromeConfigM = assertExists(metadataConfigSource.chromeConfig);
  const traceConfigM = assertExists(chromeConfigM.traceConfig);

  const expectedTraceConfig = '{"record_mode":"record-until-full",' +
      '"included_categories":' +
      '["toplevel","disabled-by-default-ipc.flow","mojom","v8"]}';
  expect(traceConfigM).toEqual(expectedTraceConfig);
  expect(traceConfig).toEqual(expectedTraceConfig);
});

test('ChromeConfigRingBuffer', () => {
  const config = createEmptyRecordConfig();
  config.ipcFlows = true;
  config.jsExecution = true;
  config.mode = 'RING_BUFFER';
  const result = TraceConfig.decode(genConfigProto(config));
  const sources = assertExists(result.dataSources);

  const traceConfigSource = assertExists(sources[0].config);
  expect(traceConfigSource.name).toBe('org.chromium.trace_event');
  const chromeConfig = assertExists(traceConfigSource.chromeConfig);
  const traceConfig = assertExists(chromeConfig.traceConfig);

  const metadataConfigSource = assertExists(sources[1].config);
  expect(metadataConfigSource.name).toBe('org.chromium.trace_metadata');
  const chromeConfigM = assertExists(metadataConfigSource.chromeConfig);
  const traceConfigM = assertExists(chromeConfigM.traceConfig);

  const expectedTraceConfig = '{"record_mode":"record-continuously",' +
      '"included_categories":' +
      '["toplevel","disabled-by-default-ipc.flow","mojom","v8"]}';
  expect(traceConfigM).toEqual(expectedTraceConfig);
  expect(traceConfig).toEqual(expectedTraceConfig);
});


test('ChromeConfigLongTrace', () => {
  const config = createEmptyRecordConfig();
  config.ipcFlows = true;
  config.jsExecution = true;
  config.mode = 'RING_BUFFER';
  const result = TraceConfig.decode(genConfigProto(config));
  const sources = assertExists(result.dataSources);

  const traceConfigSource = assertExists(sources[0].config);
  expect(traceConfigSource.name).toBe('org.chromium.trace_event');
  const chromeConfig = assertExists(traceConfigSource.chromeConfig);
  const traceConfig = assertExists(chromeConfig.traceConfig);

  const metadataConfigSource = assertExists(sources[1].config);
  expect(metadataConfigSource.name).toBe('org.chromium.trace_metadata');
  const chromeConfigM = assertExists(metadataConfigSource.chromeConfig);
  const traceConfigM = assertExists(chromeConfigM.traceConfig);

  const expectedTraceConfig = '{"record_mode":"record-continuously",' +
      '"included_categories":' +
      '["toplevel","disabled-by-default-ipc.flow","mojom","v8"]}';
  expect(traceConfigM).toEqual(expectedTraceConfig);
  expect(traceConfig).toEqual(expectedTraceConfig);
});

test('ChromeConfigToPbtxt', () => {
  const config = {
    dataSources: [{
      config: {
        name: 'org.chromium.trace_event',
        chromeConfig:
            {traceConfig: JSON.stringify({included_categories: ['v8']})},
      },
    }],
  };
  const text = toPbtxt(TraceConfig.encode(config).finish());

  expect(text).toEqual(`data_sources: {
    config {
        name: "org.chromium.trace_event"
        chrome_config {
            trace_config: "{\\"included_categories\\":[\\"v8\\"]}"
        }
    }
}
`);
});
