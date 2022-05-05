// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const KB = 1024;
export const MB = KB * KB;
export const GB = MB * KB;
export const kMillis2Seconds = 1 / 1000;
export const kMicro2Milli = 1 / 1000;

export function formatBytes(bytes, digits = 2) {
  const units = ['B', 'KiB', 'MiB', 'GiB'];
  const divisor = 1024;
  let index = 0;
  while (index < units.length && bytes >= divisor) {
    index++;
    bytes /= divisor;
  }
  return bytes.toFixed(digits) + units[index];
}

export function formatMicroSeconds(micro) {
  return (micro * kMicro2Milli).toFixed(1) + 'ms';
}

export function formatDurationMicros(micros, secondsDigits = 3) {
  return formatDurationMillis(micros * kMicro2Milli, secondsDigits);
}

export function formatDurationMillis(millis, secondsDigits = 3) {
  if (millis < 1000) {
    if (millis < 1) {
      return (millis / kMicro2Milli).toFixed(1) + 'ns';
    }
    return millis.toFixed(2) + 'ms';
  }
  let seconds = millis / 1000;
  const hours = Math.floor(seconds / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  seconds = seconds % 60;
  let buffer = '';
  if (hours > 0) buffer += hours + 'h ';
  if (hours > 0 || minutes > 0) buffer += minutes + 'm ';
  buffer += seconds.toFixed(secondsDigits) + 's';
  return buffer;
}

// Get the offset in the 4GB virtual memory cage.
export function calcOffsetInVMCage(address) {
  let mask = (1n << 32n) - 1n;
  let ret = Number(address & mask);
  return ret;
}

export function delay(time) {
  return new Promise(resolver => setTimeout(resolver, time));
}

export function defer() {
  let resolve_func, reject_func;
  const p = new Promise((resolve, reject) => {
    resolve_func = resolve;
    reject_func = resolve;
  });
  p.resolve = resolve_func;
  p.reject = reject_func;
  return p;
}
