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

import {perfetto} from '../gen/protos';

// In this file are contained a few functions to simplify the proto parsing.

export function extractTraceConfig(enableTracingRequest: Uint8Array):
    Uint8Array|undefined {
  try {
    const enableTracingObject =
        perfetto.protos.EnableTracingRequest.decode(enableTracingRequest);
    if (!enableTracingObject.traceConfig) return undefined;
    return perfetto.protos.TraceConfig.encode(enableTracingObject.traceConfig)
        .finish();
  } catch (e) {  // This catch is for possible proto encoding/decoding issues.
    console.error('Error extracting the config: ', e.message);
    return undefined;
  }
}

export function extractDurationFromTraceConfig(traceConfigProto: Uint8Array) {
  try {
    return perfetto.protos.TraceConfig.decode(traceConfigProto).durationMs;
  } catch (e) {  // This catch is for possible proto encoding/decoding issues.
    return undefined;
  }
}