// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class Group {
  constructor(key, id, parentTotal, entries) {
    this.key = key;
    this.id = id;
    this.entries = entries;
    this.length = entries.length;
    this.parentTotal = parentTotal;
  }

  get percent() {
    return this.length / this.parentTotal * 100;
  }

  add() {
    this.length++;
  }

  addEntry(entry) {
    this.length++;
    this.entries.push(entry);
  }
}

export function groupBy(array, keyFunction, collect = false) {
  if (array.length === 0) return [];
  if (keyFunction === undefined) keyFunction = each => each;
  const keyToGroup = new Map();
  const groups = [];
  const sharedEmptyArray = [];
  let id = 0;
  // This is performance critical, resorting to for-loop
  for (let each of array) {
    const key = keyFunction(each);
    let group = keyToGroup.get(key);
    if (group !== undefined) {
      collect ? group.addEntry(each) : group.add();
      continue;
    }
    let entries = collect ? [each] : sharedEmptyArray;
    group = new Group(key, id++, array.length, entries);
    groups.push(group);
    keyToGroup.set(key, group);
  }
  // Sort by length
  return groups.sort((a, b) => b.length - a.length);
}

export function arrayEquals(left, right) {
  if (left == right) return true;
  if (left.length != right.length) return false;
  for (let i = 0; i < left.length; i++) {
    if (left[i] != right[i]) return false;
  }
  return true;
}

export function entriesEquals(left, right) {
  if (left == right) return true;
  if (left == undefined) return right == undefined;
  const leftEntries = Object.entries(left);
  const rightEntries = Object.entries(right);
  if (leftEntries.length !== rightEntries.length) return false;
  for (let i = 0; i < leftEntries.length; i++) {
    const l = leftEntries[i];
    const r = rightEntries[i];
    if (l[0] != r[0]) return false;
    if (l[1] != r[1]) return false;
  }
  return true;
}

export function keysEquals(left, right) {
  if (left == right) return true;
  if (left == undefined) return right == undefined;
  return arrayEquals(Object.keys(left), Object.keys(right));
}

export * from '../js/helper.mjs'
