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

import '../tracks/all_frontend';
import * as m from 'mithril';

import {Actions} from '../common/actions';
import {HttpRpcEngine, RPC_URL} from '../common/http_rpc_engine';

import {globals} from './globals';
import {showModal} from './modal';

const PROMPT = `Trace Processor Native Accelerator detected on ${RPC_URL} with:
$loadedTraceName

YES, use loaded trace:
Will load from the current state of Trace Processor. If you did run
trace_processor_shell --http file.pftrace this is likely what you want.

YES, but reset state:
Use this if you want to open another trace but still use the
accelerator. This is the equivalent of killing and restarting
trace_processor_shell --http.

NO, Use builtin WASM:
Will not use the accelerator in this tab.

Using the native accelerator has some minor caveats:
- Only one tab can be using the accelerator.
- Sharing, downloading and conversion-to-legacy aren't supported.
`;

// Try to connect to the external Trace Processor HTTP RPC accelerator (if
// available, often it isn't). If connected it will populate the
// |httpRpcState| in the frontend local state. In turn that will show the UI
// chip in the sidebar. trace_controller.ts will repeat this check before
// trying to load a new trace. We do this ahead of time just to have a
// consistent UX (i.e. so that the user can tell if the RPC is working without
// having to open a trace).
export async function CheckHttpRpcConnection(): Promise<void> {
  const state = await HttpRpcEngine.checkConnection();
  globals.frontendLocalState.setHttpRpcState(state);

  // If a trace is already loaded in the trace processor (e.g., the user
  // launched trace_processor_shell -D trace_file.pftrace), prompt the user to
  // initialize the UI with the already-loaded trace.
  if (state.connected && state.loadedTraceName) {
    showModal({
      title: 'Use Trace Processor Native Acceleration?',
      content:
          m('.modal-pre',
            PROMPT.replace('$loadedTraceName', state.loadedTraceName)),
      buttons: [
        {
          text: 'YES, use loaded trace',
          primary: true,
          id: 'rpc_load',
          action: () => {
            globals.dispatch(Actions.openTraceFromHttpRpc({}));
          }
        },
        {
          text: 'YES, but reset state',
          primary: false,
          id: 'rpc_reset',
          action: () => {}
        },
        {
          text: 'NO, Use builtin WASM',
          primary: false,
          id: 'rpc_force_wasm',
          action: () => {
            globals.dispatch(
                Actions.setNewEngineMode({mode: 'FORCE_BUILTIN_WASM'}));
          }
        },
      ],
    });
  }
}
