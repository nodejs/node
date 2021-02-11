// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {IcLogEntry} from './log/ic.mjs';

// For compatibility with console scripts:
print = console.log;

export class Group {
  constructor(property, key, entry) {
    this.property = property;
    this.key = key;
    this.count = 1;
    this.entries = [entry];
    this.percentage = undefined;
    this.groups = undefined;
  }

  add(entry) {
    this.count++;
    this.entries.push(entry)
  }

  createSubGroups() {
    // TODO: use Map
    this.groups = {};
    for (const propertyName of IcLogEntry.propertyNames) {
      if (this.property == propertyName) continue;
      this.groups[propertyName] = Group.groupBy(this.entries, propertyName);
    }
  }

  static groupBy(entries, property) {
    let accumulator = Object.create(null);
    let length = entries.length;
    for (let i = 0; i < length; i++) {
      let entry = entries[i];
      let key = entry[property];
      if (accumulator[key] == undefined) {
        accumulator[key] = new Group(property, key, entry);
      } else {
        let group = accumulator[key];
        if (group.entries == undefined) console.log([group, entry]);
        group.add(entry)
      }
    }
    let result = [];
    for (let key in accumulator) {
      let group = accumulator[key];
      group.percentage = Math.round(group.count / length * 100 * 100) / 100;
      result.push(group);
    }
    result.sort((a, b) => {return b.count - a.count});
    return result;
  }
}
