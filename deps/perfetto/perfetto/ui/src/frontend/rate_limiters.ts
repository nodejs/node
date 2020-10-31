// Copyright (C) 2020 The Android Open Source Project
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

// Returns a wrapper around |f| which calls f at most once every |ms|ms.
export function ratelimit(f: Function, ms: number): Function {
  let inProgess = false;
  return () => {
    if (inProgess) {
      return;
    }
    inProgess = true;
    window.setTimeout(() => {
      f();
      inProgess = false;
    }, ms);
  };
}

// Returns a wrapper around |f| which waits for a |ms|ms pause in calls
// before calling |f|.
export function debounce(f: Function, ms: number): Function {
  let timerId: undefined|number;
  return () => {
    if (timerId) {
      window.clearTimeout(timerId);
    }
    timerId = window.setTimeout(() => {
      f();
      timerId = undefined;
    }, ms);
  };
}
