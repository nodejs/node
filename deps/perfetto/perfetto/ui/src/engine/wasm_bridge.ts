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

import {defer} from '../base/deferred';
import {assertExists, assertTrue} from '../base/logging';
import * as init_trace_processor from '../gen/trace_processor';

// The Initialize() call will allocate a buffer of REQ_BUF_SIZE bytes which
// will be used to copy the input request data. This is to avoid passing the
// input data on the stack, which has a limited (~1MB) size.
// The buffer will be allocated by the C++ side and reachable at
// HEAPU8[reqBufferAddr, +REQ_BUFFER_SIZE].
const REQ_BUF_SIZE = 32 * 1024 * 1024;

function writeToUIConsole(line: string) {
  console.log(line);
}

export interface WasmBridgeRequest {
  id: number;
  methodName: string;
  data: Uint8Array;
}

export interface WasmBridgeResponse {
  id: number;
  aborted: boolean;  // If true the WASM module crashed.
  data: Uint8Array;
}

export class WasmBridge {
  // When this promise has resolved it is safe to call callWasm.
  whenInitialized: Promise<void>;

  private aborted: boolean;
  private currentRequestResult: WasmBridgeResponse|null;
  private connection: init_trace_processor.Module;
  private reqBufferAddr = 0;

  constructor(init: init_trace_processor.InitWasm) {
    this.aborted = false;
    this.currentRequestResult = null;

    const deferredRuntimeInitialized = defer<void>();
    this.connection = init({
      locateFile: (s: string) => s,
      print: writeToUIConsole,
      printErr: writeToUIConsole,
      onRuntimeInitialized: () => deferredRuntimeInitialized.resolve(),
      onAbort: () => this.aborted = true,
    });
    this.whenInitialized = deferredRuntimeInitialized.then(() => {
      const fn = this.connection.addFunction(this.onReply.bind(this), 'iii');
      this.reqBufferAddr = this.connection.ccall(
          'Initialize',
          /*return=*/ 'number',
          /*args=*/['number', 'number'],
          [fn, REQ_BUF_SIZE]);
    });
  }

  callWasm(req: WasmBridgeRequest): WasmBridgeResponse {
    if (this.aborted) {
      return {
        id: req.id,
        aborted: true,
        data: new Uint8Array(),
      };
    }
    assertTrue(req.data.length <= REQ_BUF_SIZE);
    const endAddr = this.reqBufferAddr + req.data.length;
    this.connection.HEAPU8.subarray(this.reqBufferAddr, endAddr).set(req.data);
    this.connection.ccall(
        req.methodName,    // C method name.
        'void',            // Return type.
        ['number'],        // Arg types.
        [req.data.length]  // Args.
    );

    const result = assertExists(this.currentRequestResult);
    this.currentRequestResult = null;
    result.id = req.id;
    return result;
  }

  // This is invoked from ccall in the same call stack as callWasm.
  private onReply(heapPtr: number, size: number) {
    const data = this.connection.HEAPU8.slice(heapPtr, heapPtr + size);
    this.currentRequestResult = {
      id: 0,  // Will be set by callWasm()'s epilogue.
      aborted: false,
      data,
    };
  }
}
