// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Script, SourcePosition} from '../../profile.mjs';
import {LogEntry} from '../log/log.mjs';

import {FocusEvent} from './events.mjs';
import {groupBy, LazyTable} from './helper.mjs';
import {DOM, V8CustomElement} from './helper.mjs';

DOM.defineCustomElement('view/list-panel',
                        (templateText) =>
                            class ListPanel extends V8CustomElement {
  _selectedLogEntries = [];
  _displayedLogEntries = [];
  _timeline;

  _detailsClickHandler = this._handleDetailsClick.bind(this);
  _logEntryClickHandler = this._handleLogEntryClick.bind(this);

  constructor() {
    super(templateText);
    this.groupKey.addEventListener('change', e => this.update());
    this.showAllRadio.onclick = _ => this._showEntries(this._timeline);
    this.showTimerangeRadio.onclick = _ =>
        this._showEntries(this._timeline.selectionOrSelf);
    this.showSelectionRadio.onclick = _ =>
        this._showEntries(this._selectedLogEntries);
  }

  static get observedAttributes() {
    return ['title'];
  }

  attributeChangedCallback(name, oldValue, newValue) {
    if (name == 'title') {
      this.$('#title').innerHTML = newValue;
    }
  }

  set timeline(timeline) {
    console.assert(timeline !== undefined, 'timeline undefined!');
    this._timeline = timeline;
    this.$('.panel').style.display = timeline.isEmpty() ? 'none' : 'inherit';
    this._initGroupKeySelect();
  }

  set selectedLogEntries(entries) {
    if (entries === this._timeline) {
      this.showAllRadio.click();
    } else if (entries === this._timeline.selection) {
      this.showTimerangeRadio.click();
    } else {
      this._selectedLogEntries = entries;
      this.showSelectionRadio.click();
    }
  }

  get entryClass() {
    return this._timeline.at(0)?.constructor;
  }

  get groupKey() {
    return this.$('#group-key');
  }

  get table() {
    return this.$('#table');
  }

  get showAllRadio() {
    return this.$('#show-all');
  }
  get showTimerangeRadio() {
    return this.$('#show-timerange');
  }
  get showSelectionRadio() {
    return this.$('#show-selection');
  }

  get _propertyNames() {
    return this.entryClass?.propertyNames ?? [];
  }

  _initGroupKeySelect() {
    const select = this.groupKey;
    select.options.length = 0;
    for (const propertyName of this._propertyNames) {
      const option = DOM.element('option');
      option.text = propertyName;
      select.add(option);
    }
  }

  _showEntries(entries) {
    this._displayedLogEntries = entries;
    this.update();
  }

  _update() {
    if (this._timeline.isEmpty()) return;
    DOM.removeAllChildren(this.table);
    if (this._displayedLogEntries.length == 0) return;
    const propertyName = this.groupKey.selectedOptions[0].text;
    const groups =
        groupBy(this._displayedLogEntries, each => each[propertyName], true);
    this._render(groups, this.table);
  }

  createSubgroups(group) {
    const map = new Map();
    for (let propertyName of this._propertyNames) {
      map.set(
          propertyName,
          groupBy(group.entries, each => each[propertyName], true));
    }
    return map;
  }

  _handleLogEntryClick(e) {
    const group = e.currentTarget.group;
    this.dispatchEvent(new FocusEvent(group.key));
  }

  _handleDetailsClick(event) {
    event.stopPropagation();
    const tr = event.target.parentNode;
    const group = tr.group;
    // Create subgroup in-place if the don't exist yet.
    if (tr.groups === undefined) {
      const groups = tr.groups = this.createSubgroups(group);
      this.renderDrilldown(groups, tr);
    }
    const detailsTr = tr.nextSibling;
    if (tr.classList.contains('open')) {
      tr.classList.remove('open');
      detailsTr.style.display = 'none';
    } else {
      tr.classList.add('open');
      detailsTr.style.display = 'table-row';
    }
  }

  renderDrilldown(groups, previousSibling) {
    const tr = DOM.tr('entry-details');
    tr.style.display = 'none';
    // indent by one td.
    tr.appendChild(DOM.td());
    const td = DOM.td();
    td.colSpan = 3;
    groups.forEach((group, key) => {
      this.renderDrilldownGroup(td, group, key);
    });
    tr.appendChild(td);
    // Append the new TR after previousSibling.
    previousSibling.parentNode.insertBefore(tr, previousSibling.nextSibling);
  }

  renderDrilldownGroup(td, groups, key) {
    const div = DOM.div('drilldown-group-title');
    div.textContent = `Grouped by ${key}: ${groups[0]?.parentTotal ?? 0}#`;
    td.appendChild(div);
    const table = DOM.table();
    this._render(groups, table, false)
    td.appendChild(table);
  }

  _render(groups, table) {
    let last;
    new LazyTable(table, groups, group => {
      if (last && last.count < group.count) {
        console.log(last, group);
      }
      last = group;
      const tr = DOM.tr();
      tr.group = group;
      const details = tr.appendChild(DOM.td('', 'toggle'));
      details.onclick = this._detailsClickHandler;
      tr.appendChild(DOM.td(`${group.percent.toFixed(2)}%`, 'percentage'));
      tr.appendChild(DOM.td(group.count, 'count'));
      const valueTd = tr.appendChild(DOM.td(`${group.key}`, 'key'));
      if (this._isClickable(group.key)) {
        tr.onclick = this._logEntryClickHandler;
        valueTd.classList.add('clickable');
      }
      return tr;
    }, 10);
  }

  _isClickable(object) {
    if (typeof object !== 'object') return false;
    if (object instanceof LogEntry) return true;
    if (object instanceof SourcePosition) return true;
    if (object instanceof Script) return true;
    return false;
  }
});
