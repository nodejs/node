// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MB} from '../js/helper.mjs';
import {DOM} from '../js/web-api-helper.mjs';

import {getColorFromSpaceName, kSpaceNames} from './space-categories.mjs';

class TrendLineHelper {
  static re_gc_count = /(?<=(Before|After) GC:)\d+(?=,)/;
  static re_allocated = /allocated/;
  static re_space_name = /^[a-z_]+_space/;

  static snapshotHeaderToXLabel(header) {
    const gc_count = this.re_gc_count.exec(header)[0];
    const alpha = header[0];
    return alpha + gc_count;
  }

  static getLineSymbolFromTrendLineName(trend_line_name) {
    const is_allocated_line = this.re_allocated.test(trend_line_name);
    if (is_allocated_line) {
      return 'emptyTriangle';
    }
    return 'emptyCircle';
  }

  static getSizeTrendLineName(space_name) {
    return space_name + ' size';
  }

  static getAllocatedTrendSizeName(space_name) {
    return space_name + ' allocated';
  }

  static getSpaceNameFromTrendLineName(trend_line_name) {
    const space_name = this.re_space_name.exec(trend_line_name)[0];
    return space_name;
  }
}

DOM.defineCustomElement('heap-size-trend-viewer',
                        (templateText) =>
                            class HeapSizeTrendViewer extends HTMLElement {
  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.innerHTML = templateText;
    this.chart = echarts.init(this.$('#chart'), null, {
      renderer: 'canvas',
    });
    this.chart.getZr().on('click', 'series.line', (params) => {
      const pointInPixel = [params.offsetX, params.offsetY];
      const pointInGrid =
          this.chart.convertFromPixel({seriesIndex: 0}, pointInPixel);
      const xIndex = pointInGrid[0];
      this.dispatchEvent(new CustomEvent('change', {
        bubbles: true,
        composed: true,
        detail: xIndex,
      }));
      this.setXMarkLine(xIndex);
    });
    this.chartXAxisData = null;
    this.chartSeriesData = null;
    this.currentIndex = 0;
    window.addEventListener('resize', () => {
      this.chart.resize();
    });
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

  hide() {
    this.$('#container').style.display = 'none';
  }

  show() {
    this.$('#container').style.display = 'block';
  }

  stateChanged() {
    this.initTrendLineNames();
    this.initXAxisDataAndSeries();
    this.drawChart();
  }

  initTrendLineNames() {
    this.trend_line_names = [];
    for (const space_name of kSpaceNames) {
      this.trend_line_names.push(
          TrendLineHelper.getSizeTrendLineName(space_name));
      this.trend_line_names.push(
          TrendLineHelper.getAllocatedTrendSizeName(space_name));
    }
  }

  // X axis represent the moment before or after nth GC : [B1,A1,...Bn,An].
  initXAxisDataAndSeries() {
    this.chartXAxisData = [];
    this.chartSeriesData = [];
    let trend_line_name_data_dict = {};

    for (const trend_line_name of this.trend_line_names) {
      trend_line_name_data_dict[trend_line_name] = [];
    }

    // Init x axis data and trend line series.
    for (const snapshot of this.data) {
      this.chartXAxisData.push(
          TrendLineHelper.snapshotHeaderToXLabel(snapshot.header));
      for (const [space_name, pageinfos] of Object.entries(snapshot.data)) {
        const size_trend_line_name =
            TrendLineHelper.getSizeTrendLineName(space_name);
        const allocated_trend_line_name =
            TrendLineHelper.getAllocatedTrendSizeName(space_name);
        let size_sum = 0;
        let allocated_sum = 0;
        for (const pageinfo of pageinfos) {
          size_sum += pageinfo[2] - pageinfo[1];
          allocated_sum += pageinfo[3];
        }
        trend_line_name_data_dict[size_trend_line_name].push(size_sum);
        trend_line_name_data_dict[allocated_trend_line_name].push(
            allocated_sum);
      }
    }

    // Init mark line series as the first series
    const markline_series = {
      name: 'mark-line',
      type: 'line',

      markLine: {
        silent: true,
        symbol: 'none',
        label: {
          show: false,
        },
        lineStyle: {
          color: '#333',
        },
        data: [
          {
            xAxis: 0,
          },
        ],
      },
    };
    this.chartSeriesData.push(markline_series);

    for (const [trend_line_name, trend_line_data] of Object.entries(
             trend_line_name_data_dict)) {
      const color = getColorFromSpaceName(
          TrendLineHelper.getSpaceNameFromTrendLineName(trend_line_name));
      const trend_line_series = {
        name: trend_line_name,
        type: 'line',
        data: trend_line_data,
        lineStyle: {
          color: color,
        },
        itemStyle: {
          color: color,
        },
        symbol: TrendLineHelper.getLineSymbolFromTrendLineName(trend_line_name),
        symbolSize: 8,
      };
      this.chartSeriesData.push(trend_line_series);
    }
  }

  setXMarkLine(index) {
    if (index < 0 || index >= this.data.length) {
      console.error('Invalid index:', index);
      return;
    }
    // Set the mark-line series
    this.chartSeriesData[0].markLine.data[0].xAxis = index;
    this.chart.setOption({
      series: this.chartSeriesData,
    });
    this.currentIndex = index;
  }

  drawChart() {
    const option = {
      dataZoom: [
        {
          type: 'inside',
          filterMode: 'weakFilter',
        },
        {
          type: 'slider',
          filterMode: 'weakFilter',
          labelFormatter: '',
        },
      ],
      title: {
        text: 'Size Trend',
        left: 'center',
      },
      tooltip: {
        trigger: 'axis',
        position(point, params, dom, rect, size) {
          let ret_x = point[0] + 10;
          if (point[0] > size.viewSize[0] * 0.7) {
            ret_x = point[0] - dom.clientWidth - 10;
          }
          return [ret_x, '85%'];
        },
        formatter(params) {
          const colorSpan = (color) =>
              '<span style="display:inline-block;margin-right:1px;border-radius:5px;width:9px;height:9px;background-color:' +
              color + '"></span>';
          let result = '<p>' + params[0].axisValue + '</p>';
          params.forEach((item) => {
            const xx = '<p style="margin:0;">' + colorSpan(item.color) + ' ' +
                item.seriesName + ': ' + (item.data / MB).toFixed(2) + 'MB' +
                '</p>';
            result += xx;
          });

          return result;
        },
      },
      legend: {
        data: this.trend_line_names,
        top: '6%',
        type: 'scroll',
      },

      xAxis: {
        minInterval: 1,
        type: 'category',
        boundaryGap: false,
        data: this.chartXAxisData,
      },
      yAxis: {
        type: 'value',
        axisLabel: {
          formatter(value, index) {
            return (value / MB).toFixed(3) + 'MB';
          },
        },
      },

      series: this.chartSeriesData,
    };
    this.show();
    this.chart.resize();
    this.chart.setOption(option);
  }
});
