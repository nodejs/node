// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {categoryByZoneName} from './categories.js';

import {
  VIEW_TOTALS,
  VIEW_BY_ZONE_NAME,
  VIEW_BY_ZONE_CATEGORY,

  KIND_ALLOCATED_MEMORY,
  KIND_USED_MEMORY,
  KIND_FREED_MEMORY,
} from './details-selection.js';

defineCustomElement('global-timeline', (templateText) =>
 class GlobalTimeline extends HTMLElement {
  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.innerHTML = templateText;
  }

  $(id) {
    return this.shadowRoot.querySelector(id);
  }

  set data(value) {
    this._data = value;
    this.stateChanged();
  }

  get data() {
    return this._data;
  }

  set selection(value) {
    this._selection = value;
    this.stateChanged();
  }

  get selection() {
    return this._selection;
  }

  isValid() {
    return this.data && this.selection;
  }

  hide() {
    this.$('#container').style.display = 'none';
  }

  show() {
    this.$('#container').style.display = 'block';
  }

  stateChanged() {
    if (this.isValid()) {
      const isolate_data = this.data[this.selection.isolate];
      const peakAllocatedMemory = isolate_data.peakAllocatedMemory;
      this.$('#peak-memory-label').innerText = formatBytes(peakAllocatedMemory);
      this.drawChart();
    } else {
      this.hide();
    }
  }

  getZoneLabels(zone_names) {
    switch (this.selection.data_kind) {
      case KIND_ALLOCATED_MEMORY:
        return zone_names.map(name => {
          return {label: name + " (allocated)", type: 'number'};
        });

      case KIND_USED_MEMORY:
        return zone_names.map(name => {
          return {label: name + " (used)", type: 'number'};
        });

        case KIND_FREED_MEMORY:
          return zone_names.map(name => {
            return {label: name + " (freed)", type: 'number'};
          });

        default:
        // Don't show detailed per-zone information.
        return [];
    }
  }

  getTotalsData() {
    const isolate_data = this.data[this.selection.isolate];
    const labels = [
      { label: "Time", type: "number" },
      { label: "Total allocated", type: "number" },
      { label: "Total used", type: "number" },
      { label: "Total freed", type: "number" },
    ];
    const chart_data = [labels];

    const timeStart = this.selection.timeStart;
    const timeEnd = this.selection.timeEnd;
    const filter_entries = timeStart > 0 || timeEnd > 0;

    for (const [time, zone_data] of isolate_data.samples) {
      if (filter_entries && (time < timeStart || time > timeEnd)) continue;
      const data = [];
      data.push(time * kMillis2Seconds);
      data.push(zone_data.allocated / KB);
      data.push(zone_data.used / KB);
      data.push(zone_data.freed / KB);
      chart_data.push(data);
    }
    return chart_data;
  }

  getZoneData() {
    const isolate_data = this.data[this.selection.isolate];
    const selected_zones = this.selection.zones;
    const zone_names = isolate_data.sorted_zone_names.filter(
        zone_name => selected_zones.has(zone_name));
    const data_kind = this.selection.data_kind;
    const show_totals = this.selection.show_totals;
    const zones_labels = this.getZoneLabels(zone_names);

    const totals_labels = show_totals
        ? [
            { label: "Total allocated", type: "number" },
            { label: "Total used", type: "number" },
            { label: "Total freed", type: "number" },
          ]
        : [];

    const labels = [
      { label: "Time", type: "number" },
      ...totals_labels,
      ...zones_labels,
    ];
    const chart_data = [labels];

    const timeStart = this.selection.timeStart;
    const timeEnd = this.selection.timeEnd;
    const filter_entries = timeStart > 0 || timeEnd > 0;

    for (const [time, zone_data] of isolate_data.samples) {
      if (filter_entries && (time < timeStart || time > timeEnd)) continue;
      const active_zone_stats = Object.create(null);
      if (zone_data.zones !== undefined) {
        for (const [zone_name, zone_stats] of zone_data.zones) {
          if (!selected_zones.has(zone_name)) continue;  // Not selected, skip.

          const current_stats = active_zone_stats[zone_name];
          if (current_stats === undefined) {
            active_zone_stats[zone_name] =
                { allocated: zone_stats.allocated,
                  used: zone_stats.used,
                  freed: zone_stats.freed,
                };
          } else {
            // We've got two zones with the same name.
            console.log("=== Duplicate zone names: " + zone_name);
            // Sum stats.
            current_stats.allocated += zone_stats.allocated;
            current_stats.used += zone_stats.used;
            current_stats.freed += zone_stats.freed;
          }
        }
      }

      const data = [];
      data.push(time * kMillis2Seconds);
      if (show_totals) {
        data.push(zone_data.allocated / KB);
        data.push(zone_data.used / KB);
        data.push(zone_data.freed / KB);
      }

      zone_names.forEach(zone => {
        const sample = active_zone_stats[zone];
        let value = null;
        if (sample !== undefined) {
          if (data_kind == KIND_ALLOCATED_MEMORY) {
            value = sample.allocated / KB;
          } else if (data_kind == KIND_FREED_MEMORY) {
            value = sample.freed / KB;
          } else {
            // KIND_USED_MEMORY
            value = sample.used / KB;
          }
        }
        data.push(value);
      });
      chart_data.push(data);
    }
    return chart_data;
  }

  getCategoryData() {
    const isolate_data = this.data[this.selection.isolate];
    const categories = Object.keys(this.selection.categories);
    const categories_names =
        categories.map(k => this.selection.category_names.get(k));
    const selected_zones = this.selection.zones;
    const data_kind = this.selection.data_kind;
    const show_totals = this.selection.show_totals;

    const categories_labels = this.getZoneLabels(categories_names);

    const totals_labels = show_totals
        ? [
            { label: "Total allocated", type: "number" },
            { label: "Total used", type: "number" },
            { label: "Total freed", type: "number" },
          ]
        : [];

    const labels = [
      { label: "Time", type: "number" },
      ...totals_labels,
      ...categories_labels,
    ];
    const chart_data = [labels];

    const timeStart = this.selection.timeStart;
    const timeEnd = this.selection.timeEnd;
    const filter_entries = timeStart > 0 || timeEnd > 0;

    for (const [time, zone_data] of isolate_data.samples) {
      if (filter_entries && (time < timeStart || time > timeEnd)) continue;
      const active_category_stats = Object.create(null);
      if (zone_data.zones !== undefined) {
        for (const [zone_name, zone_stats] of zone_data.zones) {
          const category = selected_zones.get(zone_name);
          if (category === undefined) continue;  // Zone was not selected.

          const current_stats = active_category_stats[category];
          if (current_stats === undefined) {
            active_category_stats[category] =
                { allocated: zone_stats.allocated,
                  used: zone_stats.used,
                  freed: zone_stats.freed,
                };
          } else {
            // Sum stats.
            current_stats.allocated += zone_stats.allocated;
            current_stats.used += zone_stats.used;
            current_stats.freed += zone_stats.freed;
          }
        }
      }

      const data = [];
      data.push(time * kMillis2Seconds);
      if (show_totals) {
        data.push(zone_data.allocated / KB);
        data.push(zone_data.used / KB);
        data.push(zone_data.freed / KB);
      }

      categories.forEach(category => {
        const sample = active_category_stats[category];
        let value = null;
        if (sample !== undefined) {
          if (data_kind == KIND_ALLOCATED_MEMORY) {
            value = sample.allocated / KB;
          } else if (data_kind == KIND_FREED_MEMORY) {
            value = sample.freed / KB;
          } else {
            // KIND_USED_MEMORY
            value = sample.used / KB;
          }
        }
        data.push(value);
      });
      chart_data.push(data);
    }
    return chart_data;
  }

  getChartData() {
    switch (this.selection.data_view) {
      case VIEW_BY_ZONE_NAME:
        return this.getZoneData();
      case VIEW_BY_ZONE_CATEGORY:
        return this.getCategoryData();
      case VIEW_TOTALS:
      default:
        return this.getTotalsData();
      }
  }

  getChartOptions() {
    const options = {
      isStacked: true,
      interpolateNulls: true,
      hAxis: {
        format: '###.##s',
        title: 'Time [s]',
      },
      vAxis: {
        format: '#,###KB',
        title: 'Memory consumption [KBytes]'
      },
      chartArea: {left:100, width: '85%', height: '70%'},
      legend: {position: 'top', maxLines: '1'},
      pointsVisible: true,
      pointSize: 3,
      explorer: {},
    };

    // Overlay total allocated/used points on top of the graph.
    const series = {}
    if (this.selection.data_view == VIEW_TOTALS) {
      series[0] = {type: 'line', color: "red"};
      series[1] = {type: 'line', color: "blue"};
      series[2] = {type: 'line', color: "orange"};
    } else if (this.selection.show_totals) {
      series[0] = {type: 'line', color: "red", lineDashStyle: [13, 13]};
      series[1] = {type: 'line', color: "blue", lineDashStyle: [13, 13]};
      series[2] = {type: 'line', color: "orange", lineDashStyle: [13, 13]};
    }
    return Object.assign(options, {series: series});
  }

  drawChart() {
    console.assert(this.data, 'invalid data');
    console.assert(this.selection, 'invalid selection');

    const chart_data = this.getChartData();

    const data = google.visualization.arrayToDataTable(chart_data);
    const options = this.getChartOptions();
    const chart = new google.visualization.AreaChart(this.$('#chart'));
    this.show();
    chart.draw(data, google.charts.Line.convertOptions(options));
  }
});
