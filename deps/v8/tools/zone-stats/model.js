// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

export class Isolate {
  constructor(address) {
    this.address = address;
    this.start = null;
    this.end = null;
    this.peakUsageTime = null;
    // Maps zone name to per-zone statistics.
    this.zones = new Map();
    // Zone names sorted by memory usage (from low to high).
    this.sorted_zone_names = [];
    // Maps time to total and per-zone memory usages.
    this.samples = new Map();

    this.peakAllocatedMemory = 0;

    // Maps zone name to their max memory consumption.
    this.zonePeakMemory = Object.create(null);
    // Peak memory consumed by a single zone.
    this.singleZonePeakMemory = 0;
  }

  finalize() {
    this.samples.forEach(sample => this.finalizeSample(sample));
    this.start = Math.floor(this.start);
    this.end = Math.ceil(this.end);
    this.sortZoneNamesByPeakMemory();
  }

  getLabel() {
    let label = `${this.address}: `;
    label += ` peak=${formatBytes(this.peakAllocatedMemory)}`;
    label += ` time=[${this.start}, ${this.end}] ms`;
    return label;
  }

  finalizeSample(sample) {
    const time = sample.time;
    if (this.start == null) {
      this.start = time;
      this.end = time;
    } else {
      this.end = Math.max(this.end, time);
    }

    const allocated = sample.allocated;
    if (allocated > this.peakAllocatedMemory) {
      this.peakUsageTime = time;
      this.peakAllocatedMemory = allocated;
    }

    const sample_zones = sample.zones;
    if (sample_zones !== undefined) {
      sample.zones.forEach((zone_sample, zone_name) => {
        let zone_stats = this.zones.get(zone_name);
        if (zone_stats === undefined) {
          zone_stats = {max_allocated: 0, max_used: 0};
          this.zones.set(zone_name, zone_stats);
        }

        zone_stats.max_allocated =
            Math.max(zone_stats.max_allocated, zone_sample.allocated);
        zone_stats.max_used = Math.max(zone_stats.max_used, zone_sample.used);
      });
    }
  }

  sortZoneNamesByPeakMemory() {
    let entries = [...this.zones.keys()];
    entries.sort((a, b) =>
      this.zones.get(a).max_allocated - this.zones.get(b).max_allocated
    );
    this.sorted_zone_names = entries;

    let max = 0;
    for (let [key, value] of entries) {
      this.zonePeakMemory[key] = value;
      max = Math.max(max, value);
    }
    this.singleZonePeakMemory = max;
  }

  getInstanceTypePeakMemory(type) {
    if (!(type in this.zonePeakMemory)) return 0;
    return this.zonePeakMemory[type];
  }
}
