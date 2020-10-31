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

export function uint8ArrayToBase64(buffer: Uint8Array): string {
  return btoa(uint8ArrayToString(buffer));
}

// This function will not handle correctly buffers with a large number of
// elements due to a js limitation on the number of arguments of a function.
// The apply will in fact do a call like
// 'String.fromCharCode(buffer[0],buffer[1],...)'.
export function uint8ArrayToString(buffer: Uint8Array): string {
  return String.fromCharCode.apply(null, Array.from(buffer));
}

export function stringToUint8Array(str: string): Uint8Array {
  const bufView = new Uint8Array(new ArrayBuffer(str.length));
  const strLen = str.length;
  for (let i = 0; i < strLen; i++) {
    bufView[i] = str.charCodeAt(i);
  }
  return bufView;
}