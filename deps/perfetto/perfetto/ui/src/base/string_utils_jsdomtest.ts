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

import {
  stringToUint8Array,
  uint8ArrayToBase64,
  uint8ArrayToString
} from './string_utils';

test('uint8ArrayToBase64', () => {
  const bytes = [...'Hello, world'].map(c => c.charCodeAt(0));
  const buffer = new Uint8Array(bytes);
  expect(uint8ArrayToBase64(buffer)).toEqual('SGVsbG8sIHdvcmxk');
});

test('stringToBufferToString', () => {
  const testString = 'Hello world!';
  const buffer = stringToUint8Array(testString);
  const convertedBack = uint8ArrayToString(buffer);
  expect(testString).toEqual(convertedBack);
});

test('bufferToStringToBuffer', () => {
  const bytes = [...'Hello, world'].map(c => c.charCodeAt(0));
  const buffer = new Uint8Array(bytes);
  const toString = uint8ArrayToString(buffer);
  const convertedBack = stringToUint8Array(toString);
  expect(convertedBack).toEqual(buffer);
});
