/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_PERFETTO_CMD_PERFETTO_ATOMS_H_
#define SRC_PERFETTO_CMD_PERFETTO_ATOMS_H_

namespace perfetto {

// This must match the values of the PerfettoUploadEvent enum in:
// frameworks/base/cmds/statsd/src/atoms.proto
enum class PerfettoStatsdAtom {
  kUndefined = 0,

  kTraceBegin = 1,
  kBackgroundTraceBegin = 2,

  kOnConnect = 3,
  kOnTracingDisabled = 4,

  kUploadDropboxBegin = 5,
  kUploadDropboxSuccess = 6,
  kUploadDropboxFailure = 7,

  kUploadIncidentBegin = 8,
  kUploadIncidentSuccess = 9,
  kUploadIncidentFailure = 10,

  kFinalizeTraceAndExit = 11,

  kTriggerBegin = 12,
  kTriggerSuccess = 13,
  kTriggerFailure = 14,

  kHitGuardrails = 15,
  kOnTimeout = 16,
  kNotUploadingEmptyTrace = 17,
};

}  // namespace perfetto

#endif  // SRC_PERFETTO_CMD_PERFETTO_ATOMS_H_
