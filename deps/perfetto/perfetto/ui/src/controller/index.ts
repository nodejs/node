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

import '../tracks/all_controller';

import {reportError, setErrorHandler} from '../base/logging';
import {Remote} from '../base/remote';
import {warmupWasmEngine} from '../common/wasm_engine_proxy';

import {AppController} from './app_controller';
import {globals} from './globals';

function main() {
  self.addEventListener('error', e => reportError(e));
  self.addEventListener('unhandledrejection', e => reportError(e));
  warmupWasmEngine();
  let initialized = false;
  self.onmessage = ({data}) => {
    if (initialized) {
      console.error('Already initialized');
      return;
    }
    initialized = true;
    const frontendPort = data.frontendPort as MessagePort;
    const controllerPort = data.controllerPort as MessagePort;
    const extensionPort = data.extensionPort as MessagePort;
    const errorReportingPort = data.errorReportingPort as MessagePort;
    setErrorHandler((err: string) => errorReportingPort.postMessage(err));
    const frontend = new Remote(frontendPort);
    controllerPort.onmessage = ({data}) => globals.dispatch(data);

    globals.initialize(new AppController(extensionPort), frontend);
  };
}

main();

// For devtools-based debugging.
(self as {} as {globals: {}}).globals = globals;
