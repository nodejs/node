// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GB, MB} from '../js/helper.mjs';
import {DOM} from '../js/web-api-helper.mjs';

import {getColorFromSpaceName, kSpaceNames} from './space-categories.mjs';

DOM.defineCustomElement(
    'heap-layout-viewer',
    (templateText) => class HeapLayoutViewer extends HTMLElement {
      constructor() {
        super();
        const shadowRoot = this.attachShadow({mode: 'open'});
        shadowRoot.innerHTML = templateText;
        this.chart = echarts.init(this.$('#chart'), null, {
          renderer: 'canvas',
        });
        window.addEventListener('resize', () => {
          this.chart.resize();
        });
        this.currentIndex = 0;
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
        this.drawChart(0);
      }

      getChartTitle(index) {
        return this.data[index].header;
      }

      getSeriesData(pageinfos) {
        let ret = [];
        for (let pageinfo of pageinfos) {
          ret.push({value: pageinfo});
        }
        return ret;
      }

      getChartSeries(index) {
        const snapshot = this.data[index];
        let series = [];
        for (const [space_name, pageinfos] of Object.entries(snapshot.data)) {
          let space_series = {
            name: space_name,
            type: 'custom',
            renderItem(params, api) {
              const addressBegin = api.value(1);
              const addressEnd = api.value(2);
              const allocated = api.value(3);
              const start = api.coord([addressBegin, 0]);
              const end = api.coord([addressEnd, 0]);

              const allocatedRate = allocated / (addressEnd - addressBegin);
              const unAllocatedRate = 1 - allocatedRate;

              const standardH = api.size([0, 1])[1];
              const standardY = start[1] - standardH / 2;

              const allocatedY = standardY + standardH * unAllocatedRate;
              const allocatedH = standardH * allocatedRate;

              const unAllocatedY = standardY;
              const unAllocatedH = standardH - allocatedH;

              const allocatedShape = echarts.graphic.clipRectByRect(
                  {
                    x: start[0],
                    y: allocatedY,
                    width: end[0] - start[0],
                    height: allocatedH,
                  },
                  {
                    x: params.coordSys.x,
                    y: params.coordSys.y,
                    width: params.coordSys.width,
                    height: params.coordSys.height,
                  });

              const unAllocatedShape = echarts.graphic.clipRectByRect(
                  {
                    x: start[0],
                    y: unAllocatedY,
                    width: end[0] - start[0],
                    height: unAllocatedH,
                  },
                  {
                    x: params.coordSys.x,
                    y: params.coordSys.y,
                    width: params.coordSys.width,
                    height: params.coordSys.height,
                  });

              const ret = {
                type: 'group',
                children: [
                  {
                    type: 'rect',
                    shape: allocatedShape,
                    style: api.style(),
                  },
                  {
                    type: 'rect',
                    shape: unAllocatedShape,
                    style: {
                      fill: '#000000',
                    },
                  },
                ],
              };
              return ret;
            },
            data: this.getSeriesData(pageinfos),
            encode: {
              x: [1, 2],
            },
            itemStyle: {
              color: getColorFromSpaceName(space_name),
            },
          };
          series.push(space_series);
        }
        return series;
      }

      drawChart(index) {
        if (index >= this.data.length || index < 0) {
          console.error('Invalid index:', index);
          return;
        }
        const option = {
          tooltip: {
            formatter(params) {
              const ret = params.marker + params.value[0] + '<br>' +
                  'address:' + (params.value[1] / MB).toFixed(3) + 'MB' +
                  '<br>' +
                  'size:' +
                  ((params.value[2] - params.value[1]) / MB).toFixed(3) + 'MB' +
                  '<br>' +
                  'allocated:' + (params.value[3] / MB).toFixed(3) + 'MB' +
                  '<br>' +
                  'wasted:' + params.value[4] + 'B';
              return ret;
            },
          },
          grid: {
            bottom: 120,
            top: 120,
          },
          dataZoom: [
            {
              type: 'slider',
              filterMode: 'weakFilter',
              showDataShadow: true,
              labelFormatter: '',
            },
            {
              type: 'inside',
              filterMode: 'weakFilter',
            },
          ],
          legend: {
            show: true,
            data: kSpaceNames,
            top: '6%',
            type: 'scroll',
          },
          title: {
            text: this.getChartTitle(index),
            left: 'center',
          },
          xAxis: {
            name: 'Address offset in heap(MB)',
            nameLocation: 'center',
            nameTextStyle: {
              fontSize: 25,
              padding: [30, 0, 50, 0],
            },
            type: 'value',
            min: 0,
            max: 4 * GB,
            axisLabel: {
              rotate: 0,
              formatter(value, index) {
                value = value / MB;
                value = value.toFixed(3);
                return value;
              },
            },
          },
          yAxis: {
            data: ['Page'],
          },
          series: this.getChartSeries(index),
        };

        this.show();
        this.chart.resize();
        this.chart.setOption(option);
        this.currentIndex = index;
      }
    });
