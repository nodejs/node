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

// tslint:disable:no-any

/**
 * Promise wrapper with exposed resolve and reject callbacks.
 */
export interface Deferred<T> extends Promise<T> {
  readonly resolve: (value?: T|PromiseLike<T>) => void;
  readonly reject: (reason?: any) => void;
}

/**
 * Create a promise with exposed resolve and reject callbacks.
 */
export function defer<T>(): Deferred<T> {
  let resolve = null as any;
  let reject = null as any;
  const p = new Promise((res, rej) => [resolve, reject] = [res, rej]);
  return Object.assign(p, {resolve, reject}) as any;
}
