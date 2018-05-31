// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const KB = 1024;
const MB = KB * KB;
const GB = MB * KB;
const kMillis2Seconds = 1 / 1000;

function formatBytes(bytes) {
  const units = ['B', 'KiB', 'MiB', 'GiB'];
  const divisor = 1024;
  let index = 0;
  while (index < units.length && bytes >= divisor) {
    index++;
    bytes /= divisor;
  }
  return bytes.toFixed(2) + units[index];
}

function formatSeconds(millis) {
  return (millis * kMillis2Seconds).toFixed(2) + 's';
}
