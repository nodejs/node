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

import {defer, Deferred} from './deferred';

interface RemoteResponse {
  id: number;
  result: {};
}

/**
 * A proxy for an object that lives on another thread.
 */
export class Remote {
  private nextRequestId: number;
  private port: MessagePort;
  // tslint:disable-next-line no-any
  private deferredRequests: Map<number, Deferred<any>>;

  constructor(port: MessagePort) {
    this.nextRequestId = 0;
    this.deferredRequests = new Map();
    this.port = port;
    this.port.onmessage = (event: MessageEvent) => {
      this.receive(event.data);
    };
  }

  /**
   * Invoke method with name |method| with |args| on the remote object.
   * Optionally set |transferList| to transfer those objects.
   */
  // tslint:disable-next-line no-any
  send<T extends any>(
      method: string,
      args: Array<{}>, transferList?: Transferable[]): Promise<T> {
    const d = defer<T>();
    this.deferredRequests.set(this.nextRequestId, d);
    this.port.postMessage(
        {
          responseId: this.nextRequestId,
          method,
          args,
        },
        transferList);
    this.nextRequestId += 1;
    return d;
  }

  private receive(response: RemoteResponse): void {
    const d = this.deferredRequests.get(response.id);
    if (!d) throw new Error(`No deferred response with ID ${response.id}`);
    this.deferredRequests.delete(response.id);
    d.resolve(response.result);
  }
}

/**
 * Given a MessagePort |port| where the other end is owned by a Remote
 * (see above) turn each incoming MessageEvent into a call on |handler|
 * and post the result back to the calling thread.
 */
export function forwardRemoteCalls(
    port: MessagePort,
    // tslint:disable-next-line no-any
    handler: {[key: string]: any}) {
  port.onmessage = (msg: MessageEvent) => {
    const method = msg.data.method;
    const id = msg.data.responseId;
    const args = msg.data.args || [];
    if (method === undefined || id === undefined) {
      throw new Error(`Invalid call method: ${method} id: ${id}`);
    }

    if (!(handler[method] instanceof Function)) {
      throw new Error(`Method not known: ${method}(${args})`);
    }

    const result = handler[method].apply(handler, args);
    const transferList = [];

    if (result !== undefined && result.port instanceof MessagePort) {
      transferList.push(result.port);
    }

    port.postMessage(
        {
          id,
          result,
        },
        transferList);
  };
}
