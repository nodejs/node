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

import {defer, Deferred} from '../base/deferred';
import {assertTrue} from '../base/logging';
import {WasmBridgeRequest, WasmBridgeResponse} from '../engine/wasm_bridge';

import {Engine, LoadingTracker} from './engine';

const activeWorkers = new Map<string, Worker>();
let warmWorker: null|Worker = null;

function createWorker(): Worker {
  return new Worker('engine_bundle.js');
}

// Take the warm engine and start creating a new WASM engine in the background
// for the next call.
export function createWasmEngine(id: string): Worker {
  if (warmWorker === null) {
    throw new Error('warmupWasmEngine() not called');
  }
  if (activeWorkers.has(id)) {
    throw new Error(`Duplicate worker ID ${id}`);
  }
  const activeWorker = warmWorker;
  warmWorker = createWorker();
  activeWorkers.set(id, activeWorker);
  return activeWorker;
}

export function destroyWasmEngine(id: string) {
  if (!activeWorkers.has(id)) {
    throw new Error(`Cannot find worker ID ${id}`);
  }
  activeWorkers.get(id)!.terminate();
  activeWorkers.delete(id);
}

/**
 * It's quite slow to compile WASM and (in Chrome) this happens every time
 * a worker thread attempts to load a WASM module since there is no way to
 * cache the compiled code currently. To mitigate this we can always keep a
 * WASM backend 'ready to go' just waiting to be provided with a trace file.
 * warmupWasmEngineWorker (together with getWasmEngineWorker)
 * implement this behaviour.
 */
export function warmupWasmEngine(): void {
  if (warmWorker !== null) {
    throw new Error('warmupWasmEngine() already called');
  }
  warmWorker = createWorker();
}

interface PendingRequest {
  id: number;
  respHandler: Deferred<Uint8Array>;
}

/**
 * This implementation of Engine uses a WASM backend hosted in a separate
 * worker thread.
 */
export class WasmEngineProxy extends Engine {
  readonly id: string;
  private readonly worker: Worker;
  private pendingRequests = new Array<PendingRequest>();
  private nextRequestId = 0;

  constructor(id: string, worker: Worker, loadingTracker?: LoadingTracker) {
    super(loadingTracker);
    this.id = id;
    this.worker = worker;
    this.worker.onmessage = this.onMessage.bind(this);
  }

  async parse(reqData: Uint8Array): Promise<void> {
    // We don't care about the response data (the method is actually a void). We
    // just want to linearize and wait for the call to have been completed on
    // the worker.
    await this.queueRequest('trace_processor_parse', reqData);
  }

  async notifyEof(): Promise<void> {
    // We don't care about the response data (the method is actually a void). We
    // just want to linearize and wait for the call to have been completed on
    // the worker.
    await this.queueRequest('trace_processor_notify_eof', new Uint8Array());
  }

  restoreInitialTables(): Promise<void> {
    // We should never get here, restoreInitialTables() should be called only
    // when using the HttpRpcEngine.
    throw new Error('restoreInitialTables() not supported by the WASM engine');
  }

  rawQuery(rawQueryArgs: Uint8Array): Promise<Uint8Array> {
    return this.queueRequest('trace_processor_raw_query', rawQueryArgs);
  }

  rawComputeMetric(rawComputeMetric: Uint8Array): Promise<Uint8Array> {
    return this.queueRequest(
        'trace_processor_compute_metric', rawComputeMetric);
  }

  // Enqueues a request to the worker queue via postMessage(). The returned
  // promised will be resolved once the worker replies to the postMessage()
  // with the paylad of the response, a proto-encoded object which wraps the
  // method return value (e.g., RawQueryResult for SQL query results).
  private queueRequest(methodName: string, reqData: Uint8Array):
      Deferred<Uint8Array> {
    const respHandler = defer<Uint8Array>();
    const id = this.nextRequestId++;
    const request: WasmBridgeRequest = {id, methodName, data: reqData};
    this.pendingRequests.push({id, respHandler});
    this.worker.postMessage(request);
    return respHandler;
  }

  onMessage(m: MessageEvent) {
    const response = m.data as WasmBridgeResponse;
    assertTrue(this.pendingRequests.length > 0);
    const request = this.pendingRequests.shift()!;

    // Requests should be executed and ACKed by the worker in the same order
    // they came in.
    assertTrue(request.id === response.id);
    if (response.aborted) {
      request.respHandler.reject('WASM module crashed');
    } else {
      request.respHandler.resolve(response.data);
    }
  }
}
