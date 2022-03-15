// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {SelectRelatedEvent} from './events.mjs';
import {CollapsableElement, DOM, formatBytes, formatMicroSeconds} from './helper.mjs';

const kRegisters = ['rsp', 'rbp', 'rax', 'rbx', 'rcx', 'rdx', 'rsi', 'rdi'];
// Add Interpreter and x64 registers
for (let i = 0; i < 14; i++) {
  kRegisters.push(`r${i}`);
}

DOM.defineCustomElement('view/code-panel',
                        (templateText) =>
                            class CodePanel extends CollapsableElement {
  _timeline;
  _selectedEntries;
  _entry;

  constructor() {
    super(templateText);
    this._propertiesNode = this.$('#properties');
    this._codeSelectNode = this.$('#codeSelect');
    this._disassemblyNode = this.$('#disassembly');
    this._feedbackVectorNode = this.$('#feedbackVector');
    this._sourceNode = this.$('#sourceCode');
    this._registerSelector = new RegisterSelector(this._disassemblyNode);

    this._codeSelectNode.onchange = this._handleSelectCode.bind(this);
    this.$('#selectedRelatedButton').onclick =
        this._handleSelectRelated.bind(this)
  }

  set timeline(timeline) {
    this._timeline = timeline;
    this.$('.panel').style.display = timeline.isEmpty() ? 'none' : 'inherit';
    this.requestUpdate();
  }

  set selectedEntries(entries) {
    this._selectedEntries = entries;
    this.entry = entries.first();
  }

  set entry(entry) {
    this._entry = entry;
    if (!entry) {
      this._propertiesNode.propertyDict = {};
    } else {
      this._propertiesNode.propertyDict = {
        '__this__': entry,
        functionName: entry.functionName,
        size: formatBytes(entry.size),
        creationTime: formatMicroSeconds(entry.time / 1000),
        sourcePosition: entry.sourcePosition,
        script: entry.script,
        type: entry.type,
        kind: entry.kindName,
        variants: entry.variants.length > 1 ? entry.variants : undefined,
      };
    }
    this.requestUpdate();
  }

  _update() {
    this._updateSelect();
    this._updateDisassembly();
    this._updateFeedbackVector();
    this._sourceNode.innerText = this._entry?.source ?? '';
  }

  _updateFeedbackVector() {
    if (!this._entry?.feedbackVector) {
      this._feedbackVectorNode.propertyDict = {};
    } else {
      const dict = this._entry.feedbackVector.toolTipDict;
      delete dict.title;
      delete dict.code;
      this._feedbackVectorNode.propertyDict = dict;
    }
  }

  _updateDisassembly() {
    if (!this._entry?.code) {
      this._disassemblyNode.innerText = '';
      return;
    }
    const rawCode = this._entry?.code;
    try {
      this._disassemblyNode.innerText = rawCode;
      let formattedCode = this._disassemblyNode.innerHTML;
      for (let register of kRegisters) {
        const button = `<span class="register ${register}">${register}</span>`
        formattedCode = formattedCode.replaceAll(register, button);
      }
      // Let's replace the base-address since it doesn't add any value.
      // TODO
      this._disassemblyNode.innerHTML = formattedCode;
    } catch (e) {
      console.error(e);
      this._disassemblyNode.innerText = rawCode;
    }
  }

  _updateSelect() {
    const select = this._codeSelectNode;
    if (select.data === this._selectedEntries) return;
    select.data = this._selectedEntries;
    select.options.length = 0;
    const sorted =
        this._selectedEntries.slice().sort((a, b) => a.time - b.time);
    for (const code of this._selectedEntries) {
      const option = DOM.element('option');
      option.text = this._entrySummary(code);
      option.data = code;
      select.add(option);
    }
  }
  _entrySummary(code) {
    if (code.isBuiltinKind) {
      return `${code.functionName}(...) t=${
          formatMicroSeconds(code.time)} size=${formatBytes(code.size)}`;
    }
    return `${code.functionName}(...) t=${formatMicroSeconds(code.time)} size=${
        formatBytes(code.size)} script=${code.script?.toString()}`;
  }

  _handleSelectCode() {
    this.entry = this._codeSelectNode.selectedOptions[0].data;
  }

  _handleSelectRelated(e) {
    if (!this._entry) return;
    this.dispatchEvent(new SelectRelatedEvent(this._entry));
  }
});

class RegisterSelector {
  _currentRegister;
  constructor(node) {
    this._node = node;
    this._node.onmousemove = this._handleDisassemblyMouseMove.bind(this);
  }

  _handleDisassemblyMouseMove(event) {
    const target = event.target;
    if (!target.classList.contains('register')) {
      this._clear();
      return;
    };
    this._select(target.innerText);
  }

  _clear() {
    if (this._currentRegister == undefined) return;
    for (let node of this._node.querySelectorAll('.register')) {
      node.classList.remove('selected');
    }
  }

  _select(register) {
    if (register == this._currentRegister) return;
    this._clear();
    this._currentRegister = register;
    for (let node of this._node.querySelectorAll(`.register.${register}`)) {
      node.classList.add('selected');
    }
  }
}
