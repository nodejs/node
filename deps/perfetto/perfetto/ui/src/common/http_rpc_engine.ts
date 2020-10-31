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
import {fetchWithTimeout} from '../base/http_utils';
import {assertExists} from '../base/logging';
import {StatusResult} from '../common/protos';

import {Engine, LoadingTracker} from './engine';

export const RPC_URL = 'http://127.0.0.1:9001/';
const RPC_CONNECT_TIMEOUT_MS = 2000;

export interface HttpRpcState {
  connected: boolean;
  loadedTraceName?: string;
  failure?: string;
}

interface QueuedRequest {
  methodName: string;
  reqData?: Uint8Array;
  resp: Deferred<Uint8Array>;
}

export class HttpRpcEngine extends Engine {
  readonly id: string;
  private requestQueue = new Array<QueuedRequest>();
  errorHandler: (err: string) => void = () => {};

  constructor(id: string, loadingTracker?: LoadingTracker) {
    super(loadingTracker);
    this.id = id;
  }

  async parse(data: Uint8Array): Promise<void> {
    await this.enqueueRequest('parse', data);
  }

  async notifyEof(): Promise<void> {
    await this.enqueueRequest('notify_eof');
  }

  async restoreInitialTables(): Promise<void> {
    await this.enqueueRequest('restore_initial_tables');
  }

  rawQuery(rawQueryArgs: Uint8Array): Promise<Uint8Array> {
    return this.enqueueRequest('raw_query', rawQueryArgs);
  }

  rawComputeMetric(rawComputeMetricArgs: Uint8Array): Promise<Uint8Array> {
    return this.enqueueRequest('compute_metric', rawComputeMetricArgs);
  }

  enqueueRequest(methodName: string, data?: Uint8Array): Promise<Uint8Array> {
    const resp = defer<Uint8Array>();
    this.requestQueue.push({methodName, reqData: data, resp});
    if (this.requestQueue.length === 1) {
      this.submitNextQueuedRequest();
    }
    return resp;
  }

  private submitNextQueuedRequest() {
    if (this.requestQueue.length === 0) {
      return;
    }
    const req = this.requestQueue[0];
    const methodName = req.methodName.toLowerCase();
    // Deliberately not using fetchWithTimeout() here. These queries can be
    // arbitrarily long.
    fetch(RPC_URL + methodName, {
      method: 'post',
      cache: 'no-cache',
      headers: {
        'Content-Type': 'application/x-protobuf',
        'Accept': 'application/x-protobuf',
      },
      body: req.reqData || new Uint8Array(),
    })
        .then(resp => this.onFetchResponse(resp))
        .catch(err => this.errorHandler(err));
  }

  private onFetchResponse(resp: Response) {
    const pendingReq = assertExists(this.requestQueue.shift());
    if (resp.status !== 200) {
      pendingReq.resp.reject(`HTTP ${resp.status} - ${resp.statusText}`);
      return;
    }
    resp.arrayBuffer().then(arrBuf => {
      this.submitNextQueuedRequest();
      pendingReq.resp.resolve(new Uint8Array(arrBuf));
    });
  }

  static async checkConnection(): Promise<HttpRpcState> {
    const httpRpcState: HttpRpcState = {connected: false};
    try {
      const resp = await fetchWithTimeout(
          RPC_URL + 'status',
          {method: 'post', cache: 'no-cache'},
          RPC_CONNECT_TIMEOUT_MS);
      if (resp.status !== 200) {
        httpRpcState.failure = `${resp.status} - ${resp.statusText}`;
      } else {
        const buf = new Uint8Array(await resp.arrayBuffer());
        const status = StatusResult.decode(buf);
        httpRpcState.connected = true;
        if (status.loadedTraceName) {
          httpRpcState.loadedTraceName = status.loadedTraceName;
        }
      }
    } catch (err) {
      httpRpcState.failure = `${err}`;
    }
    return httpRpcState;
  }
}
