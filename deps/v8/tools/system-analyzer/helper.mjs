// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const KB = 1024;
export const MB = KB * KB;
export const GB = MB * KB;
export const kMicro2Milli = 1 / 1000;

export function formatBytes(bytes) {
  const units = ['B', 'KiB', 'MiB', 'GiB'];
  const divisor = 1024;
  let index = 0;
  while (index < units.length && bytes >= divisor) {
    index++;
    bytes /= divisor;
  }
  return bytes.toFixed(2) + units[index];
}

export function formatMicroSeconds(millis) {
  return (millis * kMicro2Milli).toFixed(1) + 'ms';
}

export function delay(time) {
  return new Promise(resolver => setTimeout(resolver, time));
}

export class Group {
  constructor(key, id, parentTotal, entries) {
    this.key = key;
    this.id = id;
    this.count = 1;
    this.entries = entries;
    this.parentTotal = parentTotal;
  }

  get percent() {
    return this.count / this.parentTotal * 100;
  }

  add() {
    this.count++;
  }

  addEntry(entry) {
    this.count++;
    this.entries.push(entry);
  }
}

export function groupBy(array, keyFunction, collect = false) {
  if (array.length === 0) return [];
  if (keyFunction === undefined) keyFunction = each => each;
  const keyToGroup = new Map();
  const groups = [];
  let id = 0;
  // This is performance critical, resorting to for-loop
  for (let each of array) {
    const key = keyFunction(each);
    let group = keyToGroup.get(key);
    if (group !== undefined) {
      collect ? group.addEntry(each) : group.add();
      continue;
    }
    let entries = undefined;
    if (collect) entries = [each];
    group = new Group(key, id++, array.length, entries);
    groups.push(group);
    keyToGroup.set(key, group);
  }
  // Sort by count
  return groups.sort((a, b) => b.count - a.count);
}