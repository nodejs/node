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

import {stringToUint8Array} from '../base/string_utils';
import {ChromeTracingController} from './chrome_tracing_controller';

let chromeTraceController: ChromeTracingController|undefined = undefined;

enableOnlyOnPerfettoHost();

// Listen for messages from the perfetto ui.
if (window.chrome) {
  chrome.runtime.onConnectExternal.addListener(port => {
    chromeTraceController = new ChromeTracingController(port);
    port.onMessage.addListener(onUIMessage);
  });
}

function onUIMessage(
    message: {method: string, requestData: string}, port: chrome.runtime.Port) {
  if (message.method === 'ExtensionVersion') {
    port.postMessage({version: chrome.runtime.getManifest().version});
    return;
  }
  console.assert(chromeTraceController !== undefined);
  if (!chromeTraceController) return;
  // ChromeExtensionConsumerPort sends the request data as string because
  // chrome.runtime.port doesn't support ArrayBuffers.
  const requestDataArray: Uint8Array = message.requestData ?
      stringToUint8Array(message.requestData) :
      new Uint8Array();
  chromeTraceController.handleCommand(message.method, requestDataArray);
}

function enableOnlyOnPerfettoHost() {
  function enableOnHostWithSuffix(suffix: string) {
    return {
      conditions: [new chrome.declarativeContent.PageStateMatcher({
        pageUrl: {hostSuffix: suffix},
      })],
      actions: [new chrome.declarativeContent.ShowPageAction()]
    };
  }
  chrome.declarativeContent.onPageChanged.removeRules(undefined, () => {
    chrome.declarativeContent.onPageChanged.addRules([
      enableOnHostWithSuffix('localhost'),
      enableOnHostWithSuffix('.perfetto.dev'),
    ]);
  });
}
