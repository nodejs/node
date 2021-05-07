// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {IcLogEntry} from '../log/ic.mjs';
import {MapLogEntry} from '../log/map.mjs';

import {FocusEvent, SelectionEvent, ToolTipEvent} from './events.mjs';
import {delay, DOM, formatBytes, formatMicroSeconds, V8CustomElement} from './helper.mjs';

DOM.defineCustomElement('view/code-panel',
                        (templateText) =>
                            class CodePanel extends V8CustomElement {
  _timeline;
  _selectedEntries;
  _entry;

  constructor() {
    super(templateText);
    this._codeSelectNode.onchange = this._handleSelectCode.bind(this);
    this.$('#selectedRelatedButton').onclick =
        this._handleSelectRelated.bind(this)
  }

  set timeline(timeline) {
    this._timeline = timeline;
    this.$('.panel').style.display = timeline.isEmpty() ? 'none' : 'inherit';
    this.update();
  }

  set selectedEntries(entries) {
    this._selectedEntries = entries;
    // TODO: add code selection dropdown
    this._updateSelect();
    this.entry = entries.first();
  }

  set entry(entry) {
    this._entry = entry;
    this.update();
  }

  get _disassemblyNode() {
    return this.$('#disassembly');
  }

  get _sourceNode() {
    return this.$('#sourceCode');
  }

  get _codeSelectNode() {
    return this.$('#codeSelect');
  }

  _update() {
    this._disassemblyNode.innerText = this._entry?.disassemble ?? '';
    this._sourceNode.innerText = this._entry?.source ?? '';
  }

  _updateSelect() {
    const select = this._codeSelectNode;
    select.options.length = 0;
    const sorted =
        this._selectedEntries.slice().sort((a, b) => a.time - b.time);
    for (const code of this._selectedEntries) {
      const option = DOM.element('option');
      option.text =
          `${code.name}(...) t=${formatMicroSeconds(code.time)} size=${
              formatBytes(code.size)} script=${code.script?.toString()}`;
      option.data = code;
      select.add(option);
    }
  }

  _handleSelectCode() {
    this.entry = this._codeSelectNode.selectedOptions[0].data;
  }

  _handleSelectRelated(e) {
    if (!this._entry) return;
    this.dispatchEvent(new SelectRelatedEvent(this._entry));
  }
});