// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use size file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import * as m from 'mithril';

import {Actions} from '../common/actions';
import {AggregateData, Column} from '../common/aggregation_data';
import {translateState} from '../common/thread_state';

import {globals} from './globals';
import {Panel} from './panel';

export interface AggregationPanelAttrs {
  data: AggregateData;
  kind: string;
}

export class AggregationPanel extends Panel<AggregationPanelAttrs> {
  view({attrs}: m.CVnode<AggregationPanelAttrs>) {
    return m(
        '.details-panel',
        m('.details-panel-heading.aggregation',
          m('table',
            m('tr',
              attrs.data.columns.map(
                  col => this.formatColumnHeading(col, attrs.kind))))),
        m(
            '.details-table.aggregation',
            m('table', this.getRows(attrs.data)),
            ));
  }

  formatColumnHeading(col: Column, id: string) {
    const pref = globals.state.aggregatePreferences[id];
    let sortIcon = '';
    if (pref && pref.sorting && pref.sorting.column === col.columnId) {
      sortIcon = pref.sorting.direction === 'DESC' ? 'arrow_drop_down' :
                                                     'arrow_drop_up';
    }
    return m(
        'th',
        {
          onclick: () => {
            globals.dispatch(
                Actions.updateAggregateSorting({id, column: col.columnId}));
          }
        },
        col.title,
        m('i.material-icons', sortIcon));
  }

  getRows(data: AggregateData) {
    if (data.columns.length === 0) return;
    const rows = [];
    for (let i = 0; i < data.columns[0].data.length; i++) {
      const row = [];
      for (let j = 0; j < data.columns.length; j++) {
        row.push(m('td', this.getFormattedData(data, i, j)));
      }
      rows.push(m('tr', row));
    }
    return rows;
  }

  getFormattedData(data: AggregateData, rowIndex: number, columnIndex: number) {
    switch (data.columns[columnIndex].kind) {
      case 'STRING':
        return `${data.strings[data.columns[columnIndex].data[rowIndex]]}`;
      case 'TIMESTAMP_NS':
        return `${data.columns[columnIndex].data[rowIndex] / 1000000}`;
      case 'STATE':
        return translateState(
            `${data.strings[data.columns[columnIndex].data[rowIndex]]}`);
      case 'NUMBER':
      default:
        return `${data.columns[columnIndex].data[rowIndex]}`;
    }
  }

  renderCanvas() {}
}