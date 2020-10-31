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

import {Actions} from '../common/actions';
import {TraceSource} from '../common/state';
import * as trace_to_text from '../gen/trace_to_text';

import {globals} from './globals';

export function ConvertTrace(trace: Blob, truncate?: 'start'|'end') {
  const mod = trace_to_text({
    noInitialRun: true,
    locateFile: (s: string) => s,
    print: updateStatus,
    printErr: updateStatus,
    onRuntimeInitialized: () => {
      updateStatus('Converting trace');
      const outPath = '/trace.json';
      if (truncate === undefined) {
        mod.callMain(['json', '/fs/trace.proto', outPath]);
      } else {
        mod.callMain(
            ['json', '--truncate', truncate, '/fs/trace.proto', outPath]);
      }
      updateStatus('Trace conversion completed');
      const fsNode = mod.FS.lookupPath(outPath).node;
      const data = fsNode.contents.buffer;
      const size = fsNode.usedBytes;
      globals.publish('LegacyTrace', {data, size}, /*transfer=*/[data]);
      mod.FS.unlink(outPath);
    },
    onAbort: () => {
      console.log('ABORT');
    },
  });
  mod.FS.mkdir('/fs');
  mod.FS.mount(
      mod.FS.filesystems.WORKERFS,
      {blobs: [{name: 'trace.proto', data: trace}]},
      '/fs');

  // TODO removeme.
  (self as {} as {mod: {}}).mod = mod;
}

export async function ConvertTraceToPprof(
    pid: number, src: TraceSource, ts1: number, ts2?: number) {
  generateBlob(src).then(result => {
    const mod = trace_to_text({
      noInitialRun: true,
      locateFile: (s: string) => s,
      print: updateStatus,
      printErr: updateStatus,
      onRuntimeInitialized: () => {
        updateStatus('Converting trace');
        const timestamps = `${ts1}${ts2 === undefined ? '' : `,${ts2}`}`;
        mod.callMain([
          'profile',
          `--pid`,
          `${pid}`,
          `--timestamps`,
          timestamps,
          '/fs/trace.proto'
        ]);
        updateStatus('Trace conversion completed');
        const heapDirName =
            Object.keys(mod.FS.lookupPath('/tmp/').node.contents)[0];
        const heapDirContents =
            mod.FS.lookupPath(`/tmp/${heapDirName}`).node.contents;
        const heapDumpFiles = Object.keys(heapDirContents);
        let fileNum = 0;
        heapDumpFiles.forEach(heapDump => {
          const fileContents =
              mod.FS.lookupPath(`/tmp/${heapDirName}/${heapDump}`)
                  .node.contents;
          fileNum++;
          const fileName = `/heap_dump.${fileNum}.${pid}.pb`;
          downloadFile(new Blob([fileContents]), fileName);
        });
        updateStatus('Profile(s) downloaded');
      },
      onAbort: () => {
        console.log('ABORT');
      },
    });
    mod.FS.mkdir('/fs');
    mod.FS.mount(
        mod.FS.filesystems.WORKERFS,
        {blobs: [{name: 'trace.proto', data: result}]},
        '/fs');
  });
}

async function generateBlob(src: TraceSource) {
  let blob: Blob = new Blob();
  if (src.type === 'URL') {
    const resp = await fetch(src.url);
    if (resp.status !== 200) {
      throw new Error(`fetch() failed with HTTP error ${resp.status}`);
    }
    blob = await resp.blob();
  } else if (src.type === 'ARRAY_BUFFER') {
    blob = new Blob([new Uint8Array(src.buffer, 0, src.buffer.byteLength)]);
  } else if (src.type === 'FILE') {
    blob = src.file;
  } else {
    throw new Error(`Conversion not supported for ${JSON.stringify(src)}`);
  }
  return blob;
}

function downloadFile(file: Blob, name: string) {
  globals.publish('FileDownload', {file, name});
}

function updateStatus(msg: {}) {
  console.log(msg);
  globals.dispatch(Actions.updateStatus({
    msg: msg.toString(),
    timestamp: Date.now() / 1000,
  }));
}
