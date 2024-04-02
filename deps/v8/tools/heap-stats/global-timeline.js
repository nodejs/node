// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {
  VIEW_BY_INSTANCE_TYPE,
  VIEW_BY_INSTANCE_CATEGORY,
  VIEW_BY_FIELD_TYPE
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
      this.drawChart();
    } else {
      this.hide();
    }
  }

  getFieldData() {
    const labels = [
      {type: 'number', label: 'Time'},
      {type: 'number', label: 'Ptr compression benefit'},
      {type: 'string', role: 'tooltip'},
      {type: 'number', label: 'Embedder fields'},
      {type: 'number', label: 'Tagged fields (excl. in-object Smis)'},
      {type: 'number', label: 'In-object Smi-only fields'},
      {type: 'number', label: 'Other raw fields'},
      {type: 'number', label: 'Unboxed doubles'},
      {type: 'number', label: 'Boxed doubles'},
      {type: 'number', label: 'String data'}
    ];
    const chart_data = [labels];
    const isolate_data = this.data[this.selection.isolate];
    let sum_total = 0;
    let sum_ptr_compr_benefit_perc = 0;
    let count = 0;
    Object.keys(isolate_data.gcs).forEach(gc_key => {
      const gc_data = isolate_data.gcs[gc_key];
      const data_set = gc_data[this.selection.data_set].field_data;
      const data = [];
      data.push(gc_data.time * kMillis2Seconds);
      const total = data_set.tagged_fields +
                    data_set.inobject_smi_fields +
                    data_set.embedder_fields +
                    data_set.other_raw_fields +
                    data_set.unboxed_double_fields +
                    data_set.boxed_double_fields +
                    data_set.string_data;
      const ptr_compr_benefit =
          (data_set.inobject_smi_fields + data_set.tagged_fields) / 2;
      const ptr_compr_benefit_perc = ptr_compr_benefit / total * 100;
      sum_total += total;
      sum_ptr_compr_benefit_perc += ptr_compr_benefit_perc;
      count++;
      const tooltip = "Ptr compression benefit: " +
                      (ptr_compr_benefit / KB).toFixed(2) + "KB " +
                      " (" + ptr_compr_benefit_perc.toFixed(2) + "%)";
      data.push(ptr_compr_benefit / KB);
      data.push(tooltip);
      data.push(data_set.embedder_fields / KB);
      data.push(data_set.tagged_fields / KB);
      data.push(data_set.inobject_smi_fields / KB);
      data.push(data_set.other_raw_fields / KB);
      data.push(data_set.unboxed_double_fields / KB);
      data.push(data_set.boxed_double_fields / KB);
      data.push(data_set.string_data / KB);
      chart_data.push(data);
    });
    const avg_ptr_compr_benefit_perc =
        count ? sum_ptr_compr_benefit_perc / count : 0;
    console.log("==================================================");
    console.log("= Average ptr compression benefit is " +
                avg_ptr_compr_benefit_perc.toFixed(2) + "%");
    console.log("= Average V8 heap size " +
                (sum_total / count / KB).toFixed(2) + " KB");
    console.log("==================================================");
    return chart_data;
  }

  getCategoryData() {
    const categories = Object.keys(this.selection.categories)
                           .map(k => this.selection.category_names.get(k));
    const labels = ['Time', ...categories];
    const chart_data = [labels];
    const isolate_data = this.data[this.selection.isolate];
    Object.keys(isolate_data.gcs).forEach(gc_key => {
      const gc_data = isolate_data.gcs[gc_key];
      const data_set = gc_data[this.selection.data_set].instance_type_data;
      const data = [];
      data.push(gc_data.time * kMillis2Seconds);
      Object.values(this.selection.categories).forEach(instance_types => {
        data.push(
            instance_types
                .map(instance_type => {
                  return data_set[instance_type].overall;
                })
                .reduce((accu, current) => accu + current, 0) /
            KB);
      });
      chart_data.push(data);
    });
    return chart_data;
  }

  getInstanceTypeData() {
    const instance_types =
        Object.values(this.selection.categories)
            .reduce((accu, current) => accu.concat(current), []);
    const labels = ['Time', ...instance_types];
    const chart_data = [labels];
    const isolate_data = this.data[this.selection.isolate];
    Object.keys(isolate_data.gcs).forEach(gc_key => {
      const gc_data = isolate_data.gcs[gc_key];
      const data_set = gc_data[this.selection.data_set].instance_type_data;
      const data = [];
      data.push(gc_data.time * kMillis2Seconds);
      instance_types.forEach(instance_type => {
        data.push(data_set[instance_type].overall / KB);
      });
      chart_data.push(data);
    });
    return chart_data;
  }

  getChartData() {
    switch (this.selection.data_view) {
      case VIEW_BY_FIELD_TYPE:
        return this.getFieldData();
      case VIEW_BY_INSTANCE_CATEGORY:
        return this.getCategoryData();
      case VIEW_BY_INSTANCE_TYPE:
      default:
        return this.getInstanceTypeData();
    }
  }

  getChartOptions() {
    const options = {
      isStacked: true,
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
      pointSize: 5,
      explorer: {},
    };
    switch (this.selection.data_view) {
      case VIEW_BY_FIELD_TYPE:
        // Overlay pointer compression benefit on top of the graph
        return Object.assign(options, {
          series: {0: {type: 'line', lineDashStyle: [13, 13]}},
        });
      case VIEW_BY_INSTANCE_CATEGORY:
      case VIEW_BY_INSTANCE_TYPE:
      default:
        return options;
    }
  }

  drawChart() {
    setTimeout(() => this._drawChart(), 10);
  }

  _drawChart() {
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
