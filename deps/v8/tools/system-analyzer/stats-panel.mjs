// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {SelectionEvent} from './events.mjs';
import {DOM, V8CustomElement} from './helper.mjs';
import {delay, LazyTable} from './helper.mjs';

DOM.defineCustomElement(
    'stats-panel', (templateText) => class StatsPanel extends V8CustomElement {
      _timeline;
      _transitions;
      _selectedLogEntries;
      constructor() {
        super(templateText);
      }

      get stats() {
        return this.$('#stats');
      }

      set timeline(timeline) {
        this._timeline = timeline;
        this.selectedLogEntries = timeline.all
      }

      set selectedLogEntries(entries) {
        this._selectedLogEntries = entries;
        this.update();
      }

      set transitions(value) {
        this._transitions = value;
      }

      _filterUniqueTransitions(filter) {
        // Returns a list of Maps whose parent is not in the list.
        return this._selectedLogEntries.filter((map) => {
          if (filter(map) === false) return false;
          let parent = map.parent();
          if (parent === undefined) return true;
          return filter(parent) === false;
        });
      }

      _update() {
        this._updateGeneralStats();
        this._updateNamedTransitionsStats();
      }

      _updateGeneralStats() {
        console.assert(this._timeline !== undefined, 'Timeline not set yet!');
        let pairs = [
          ['Transitions', 'primary', (e) => e.edge && e.edge.isTransition()],
          ['Fast to Slow', 'violet', (e) => e.edge && e.edge.isFastToSlow()],
          ['Slow to Fast', 'orange', (e) => e.edge && e.edge.isSlowToFast()],
          ['Initial Map', 'yellow', (e) => e.edge && e.edge.isInitial()],
          [
            'Replace Descriptors',
            'red',
            (e) => e.edge && e.edge.isReplaceDescriptors(),
          ],
          [
            'Copy as Prototype',
            'red',
            (e) => e.edge && e.edge.isCopyAsPrototype(),
          ],
          [
            'Optimize as Prototype',
            null,
            (e) => e.edge && e.edge.isOptimizeAsPrototype(),
          ],
          ['Deprecated', null, (e) => e.isDeprecated()],
          ['Bootstrapped', 'green', (e) => e.isBootstrapped()],
          ['Total', null, (e) => true],
        ];

        let tbody = document.createElement('tbody');
        let total = this._selectedLogEntries.length;
        pairs.forEach(([name, color, filter]) => {
          let row = DOM.tr();
          if (color !== null) {
            row.appendChild(DOM.td(DOM.div(['colorbox', color])));
          } else {
            row.appendChild(DOM.td(''));
          }
          row.classList.add('clickable');
          row.onclick = (e) => {
            // lazily compute the stats
            let node = e.target.parentNode;
            if (node.maps == undefined) {
              node.maps = this._filterUniqueTransitions(filter);
            }
            this.dispatchEvent(new SelectionEvent(node.maps));
          };
          row.appendChild(DOM.td(name));
          let count = this._count(filter);
          row.appendChild(DOM.td(count));
          let percent = Math.round((count / total) * 1000) / 10;
          row.appendChild(DOM.td(percent.toFixed(1) + '%'));
          tbody.appendChild(row);
        });
        this.$('#typeTable').replaceChild(tbody, this.$('#typeTable tbody'));
      }

      _count(filter) {
        let count = 0;
        for (const map of this._selectedLogEntries) {
          if (filter(map)) count++;
        }
        return count;
      }

      _updateNamedTransitionsStats() {
        let rowData = Array.from(this._transitions.entries());
        rowData.sort((a, b) => b[1].length - a[1].length);
        new LazyTable(this.$('#nameTable'), rowData, ([name, maps]) => {
          let row = DOM.tr();
          row.maps = maps;
          row.classList.add('clickable');
          row.addEventListener(
              'click',
              (e) => this.dispatchEvent(new SelectionEvent(
                  e.target.parentNode.maps.map((map) => map.to))));
          row.appendChild(DOM.td(maps.length));
          row.appendChild(DOM.td(name));
          return row;
        });
      }
    });
