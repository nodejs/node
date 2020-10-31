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

import * as rpc from 'noice-json-rpc';

// To really understand how this works it is useful to see the implementation
// of noice-json-rpc.
export class DevToolsSocket implements rpc.LikeSocket {
  private messageCallback: Function = (_: string) => {};
  private openCallback: Function = () => {};
  private closeCallback: Function = () => {};
  private target: chrome.debugger.Debuggee|undefined;

  constructor() {
    chrome.debugger.onDetach.addListener(this.onDetach.bind(this));
    chrome.debugger.onEvent.addListener((_source, method, params) => {
      if (this.messageCallback) {
        const msg: rpc.JsonRpc2.Notification = {method, params};
        this.messageCallback(JSON.stringify(msg));
      }
    });
  }

  send(message: string): void {
    if (this.target === undefined) return;

    const msg: rpc.JsonRpc2.Request = JSON.parse(message);
    chrome.debugger.sendCommand(
        this.target, msg.method, msg.params, (result) => {
          if (result === undefined) result = {};
          const response: rpc.JsonRpc2.Response = {id: msg.id, result};
          this.messageCallback(JSON.stringify(response));
        });
  }

  // This method will be called once for each event soon after the creation of
  // this object. To understand better what happens, checking the implementation
  // of noice-json-rpc is very useful.
  // While the events "message" and "open" are for implementing the LikeSocket,
  // "close" is a callback set from ChromeTracingController, to reset the state
  // after a detach.
  on(event: string, cb: Function) {
    if (event === 'message') {
      this.messageCallback = cb;
    } else if (event === 'open') {
      this.openCallback = cb;
    } else if (event === 'close') {
      this.closeCallback = cb;
    }
  }

  removeListener(_event: string, _cb: Function) {
    throw new Error('Call unexpected');
  }

  attachToBrowser(then: (error?: string) => void) {
    this.attachToTarget({targetId: 'browser'}, then);
  }

  private attachToTarget(
      target: chrome.debugger.Debuggee, then: (error?: string) => void) {
    chrome.debugger.attach(target, /*requiredVersion=*/ '1.3', () => {
      if (chrome.runtime.lastError) {
        then(chrome.runtime.lastError.message);
        return;
      }
      this.target = target;
      this.openCallback();
      then();
    });
  }

  detach() {
    if (this.target === undefined) return;

    chrome.debugger.detach(this.target, () => {
      this.target = undefined;
    });
  }

  onDetach(_source: chrome.debugger.Debuggee, _reason: string) {
    if (_source === this.target) {
      this.target = undefined;
      this.closeCallback();
    }
  }

  isAttached(): boolean {
    return this.target !== undefined;
  }
}
