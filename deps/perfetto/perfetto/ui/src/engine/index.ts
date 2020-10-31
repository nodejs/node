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

import * as init_trace_processor from '../gen/trace_processor';

import {WasmBridge, WasmBridgeRequest} from './wasm_bridge';

// tslint:disable no-any
// Proxy all messages to WasmBridge#callWasm.
const anySelf = (self as any);

// Messages can arrive before we are initialized, queue these for later.
const msgQueue: MessageEvent[] = [];
anySelf.onmessage = (msg: MessageEvent) => {
  msgQueue.push(msg);
};

const bridge = new WasmBridge(init_trace_processor);
bridge.whenInitialized.then(() => {
  const handleMsg = (msg: MessageEvent) => {
    const request: WasmBridgeRequest = msg.data;
    anySelf.postMessage(bridge.callWasm(request));
  };

  // Dispatch queued messages.
  let msg;
  while (msg = msgQueue.shift()) {
    handleMsg(msg);
  }

  anySelf.onmessage = handleMsg;
});
