// Copyright (C) 2019 The Android Open Source Project
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

import {defer, Deferred} from '../base/deferred';
import {assertExists, assertTrue} from '../base/logging';

const SLICE_SIZE = 32 * 1024 * 1024;

// The object returned by TraceStream.readChunk() promise.
export interface TraceChunk {
  data: Uint8Array;
  eof: boolean;
  bytesRead: number;
  bytesTotal: number;
}

// Base interface for loading trace data in chunks.
// The caller has to call readChunk() until TraceChunk.eof == true.
export interface TraceStream {
  readChunk(): Promise<TraceChunk>;
}

// Loads a trace from a File object. For the "open file" use case.
export class TraceFileStream implements TraceStream {
  private traceFile: Blob;
  private reader: FileReader;
  private pendingRead?: Deferred<TraceChunk>;
  private bytesRead = 0;

  constructor(traceFile: Blob) {
    this.traceFile = traceFile;
    this.reader = new FileReader();
    this.reader.onloadend = () => this.onLoad();
  }

  onLoad() {
    const res = assertExists(this.reader.result) as ArrayBuffer;
    const pendingRead = assertExists(this.pendingRead);
    this.pendingRead = undefined;
    if (this.reader.error) {
      pendingRead.reject(this.reader.error);
      return;
    }
    this.bytesRead += res.byteLength;
    pendingRead.resolve({
      data: new Uint8Array(res),
      eof: this.bytesRead >= this.traceFile.size,
      bytesRead: this.bytesRead,
      bytesTotal: this.traceFile.size,
    });
  }

  readChunk(): Promise<TraceChunk> {
    const sliceEnd = Math.min(this.bytesRead + SLICE_SIZE, this.traceFile.size);
    const slice = this.traceFile.slice(this.bytesRead, sliceEnd);
    this.pendingRead = defer<TraceChunk>();
    this.reader.readAsArrayBuffer(slice);
    return this.pendingRead;
  }
}

// Loads a trace from an ArrayBuffer. For the window.open() + postMessage
// use-case, used by other dashboards (see post_message_handler.ts).
export class TraceBufferStream implements TraceStream {
  private traceBuf: ArrayBuffer;
  private bytesRead = 0;

  constructor(traceBuf: ArrayBuffer) {
    this.traceBuf = traceBuf;
  }

  readChunk(): Promise<TraceChunk> {
    assertTrue(this.bytesRead <= this.traceBuf.byteLength);
    const len = Math.min(SLICE_SIZE, this.traceBuf.byteLength - this.bytesRead);
    const data = new Uint8Array(this.traceBuf, this.bytesRead, len);
    this.bytesRead += len;
    return Promise.resolve({
      data,
      eof: this.bytesRead >= this.traceBuf.byteLength,
      bytesRead: this.bytesRead,
      bytesTotal: this.traceBuf.byteLength,
    });
  }
}

// Loads a stream from a URL via fetch(). For the permalink (?s=UUID) and
// open url (?url=http://...) cases.
export class TraceHttpStream implements TraceStream {
  private bytesRead = 0;
  private bytesTotal = 0;
  private uri: string;
  private httpStream?: ReadableStreamReader;

  constructor(uri: string) {
    assertTrue(uri.startsWith('http://') || uri.startsWith('https://'));
    this.uri = uri;
  }

  async readChunk(): Promise<TraceChunk> {
    // Initialize the fetch() job on the first read request.
    if (this.httpStream === undefined) {
      const response = await fetch(this.uri);
      if (response.status !== 200) {
        throw new Error(`HTTP ${response.status} - ${response.statusText}`);
      }
      const len = response.headers.get('Content-Length');
      this.bytesTotal = len ? Number.parseInt(len, 10) : 0;
      // tslint:disable-next-line no-any
      this.httpStream = (response.body as any).getReader();
    }

    const res =
        (await this.httpStream!.read()) as {value?: Uint8Array, done: boolean};
    const data = res.value ? res.value : new Uint8Array();
    this.bytesRead += data.length;
    return {
      data,
      eof: res.done,
      bytesRead: this.bytesRead,
      bytesTotal: this.bytesTotal,
    };
  }
}
