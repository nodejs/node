// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FocusEvent, SelectionEvent, SelectTimeEvent} from './events.mjs';
import {delay, DOM, V8CustomElement} from './helper.mjs';
import {Group} from './ic-model.mjs';
import {IcLogEntry} from './log/ic.mjs';
import {MapLogEntry} from './log/map.mjs';

DOM.defineCustomElement(
    'ic-panel', (templateText) => class ICPanel extends V8CustomElement {
      _selectedLogEntries;
      _timeline;
      constructor() {
        super(templateText);
        this.initGroupKeySelect();
        this.groupKey.addEventListener('change', e => this.updateTable(e));
      }
      set timeline(value) {
        console.assert(value !== undefined, 'timeline undefined!');
        this._timeline = value;
        this.selectedLogEntries = this._timeline.all;
        this.update();
      }
      get groupKey() {
        return this.$('#group-key');
      }

      get table() {
        return this.$('#table');
      }

      get tableBody() {
        return this.$('#table-body');
      }

      get count() {
        return this.$('#count');
      }

      get spanSelectAll() {
        return this.querySelectorAll('span');
      }

      set selectedLogEntries(value) {
        this._selectedLogEntries = value;
        this.update();
      }

      _update() {
        this._updateCount();
        this._updateTable();
      }

      _updateCount() {
        this.count.innerHTML = `length=${this._selectedLogEntries.length}`;
      }

      _updateTable(event) {
        let select = this.groupKey;
        let key = select.options[select.selectedIndex].text;
        DOM.removeAllChildren(this.tableBody);
        let groups = Group.groupBy(this._selectedLogEntries, key, true);
        this._render(groups, this.tableBody);
      }

      escapeHtml(unsafe) {
        if (!unsafe) return '';
        return unsafe.toString()
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#039;');
      }

      handleMapClick(e) {
        const group = e.target.parentNode.entry;
        const id = group.key;
        const selectedMapLogEntries =
            this.searchIcLogEntryToMapLogEntry(id, group.entries);
        this.dispatchEvent(new SelectionEvent(selectedMapLogEntries));
      }

      searchIcLogEntryToMapLogEntry(id, icLogEntries) {
        // searches for mapLogEntries using the id, time
        const selectedMapLogEntriesSet = new Set();
        for (const icLogEntry of icLogEntries) {
          const selectedMap = MapLogEntry.get(id, icLogEntry.time);
          selectedMapLogEntriesSet.add(selectedMap);
        }
        return Array.from(selectedMapLogEntriesSet);
      }

      // TODO(zcankara) Handle in the processor for events with source
      // positions.
      handleFilePositionClick(e) {
        const tr = e.target.parentNode;
        const sourcePosition = tr.group.entries[0].sourcePosition;
        this.dispatchEvent(new FocusEvent(sourcePosition));
      }

      _render(groups, parent) {
        const fragment = document.createDocumentFragment();
        const max = Math.min(1000, groups.length)
        const detailsClickHandler = this.handleDetailsClick.bind(this);
        const mapClickHandler = this.handleMapClick.bind(this);
        const fileClickHandler = this.handleFilePositionClick.bind(this);
        for (let i = 0; i < max; i++) {
          const group = groups[i];
          const tr = DOM.tr();
          tr.group = group;
          const details = tr.appendChild(DOM.td('', 'toggle'));
          details.onclick = detailsClickHandler;
          tr.appendChild(DOM.td(group.percentage + '%', 'percentage'));
          tr.appendChild(DOM.td(group.count, 'count'));
          const valueTd = tr.appendChild(DOM.td(group.key, 'key'));
          if (group.property === 'map') {
            valueTd.onclick = mapClickHandler;
            valueTd.classList.add('clickable');
          } else if (group.property == 'filePosition') {
            valueTd.classList.add('clickable');
            valueTd.onclick = fileClickHandler;
          }
          fragment.appendChild(tr);
        }
        const omitted = groups.length - max;
        if (omitted > 0) {
          const tr = DOM.tr();
          const tdNode = tr.appendChild(DOM.td(`Omitted ${omitted} entries.`));
          tdNode.colSpan = 4;
          fragment.appendChild(tr);
        }
        parent.appendChild(fragment);
      }

      handleDetailsClick(event) {
        const tr = event.target.parentNode;
        const group = tr.group;
        // Create subgroup in-place if the don't exist yet.
        if (group.groups === undefined) {
          group.createSubGroups();
          this.renderDrilldown(group, tr);
        }
        let detailsTr = tr.nextSibling;
        if (tr.classList.contains('open')) {
          tr.classList.remove('open');
          detailsTr.style.display = 'none';
        } else {
          tr.classList.add('open');
          detailsTr.style.display = 'table-row';
        }
      }

      renderDrilldown(group, previousSibling) {
        let tr = DOM.tr('entry-details');
        tr.style.display = 'none';
        // indent by one td.
        tr.appendChild(DOM.td());
        let td = DOM.td();
        td.colSpan = 3;
        for (let key in group.groups) {
          this.renderDrilldownGroup(td, group.groups[key], key);
        }
        tr.appendChild(td);
        // Append the new TR after previousSibling.
        previousSibling.parentNode.insertBefore(tr, previousSibling.nextSibling)
      }

      renderDrilldownGroup(td, children, key) {
        const max = 20;
        const div = DOM.div('drilldown-group-title');
        div.textContent =
            `Grouped by ${key} [top ${max} out of ${children.length}]`;
        td.appendChild(div);
        const table = DOM.table();
        this._render(children.slice(0, max), table, false)
        td.appendChild(table);
      }

      initGroupKeySelect() {
        const select = this.groupKey;
        select.options.length = 0;
        for (const propertyName of IcLogEntry.propertyNames) {
          const option = document.createElement('option');
          option.text = propertyName;
          select.add(option);
        }
      }
    });
