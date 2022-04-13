// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {LinuxCppEntriesProvider} from '../../tickprocessor.mjs';
import {SelectRelatedEvent} from './events.mjs';
import {CollapsableElement, DOM, formatBytes, formatMicroSeconds} from './helper.mjs';

const kRegisters = ['rsp', 'rbp', 'rax', 'rbx', 'rcx', 'rdx', 'rsi', 'rdi'];
// Make sure we dont match register on bytecode: Star1 or Star2
const kAvoidBytecodeOps = '(.*?[^a-zA-Z])'
// Look for registers in strings like:  movl rbx,[rcx-0x30]
const kRegisterRegexp = `(${kRegisters.join('|')}|r[0-9]+)`
const kRegisterRegexpSplit =
    new RegExp(`${kAvoidBytecodeOps}${kRegisterRegexp}`)
const kIsRegisterRegexp = new RegExp(`^${kRegisterRegexp}$`);

const kFullAddressRegexp = /(0x[0-9a-f]{8,})/;
const kRelativeAddressRegexp = /([+-]0x[0-9a-f]+)/;
const kAnyAddressRegexp = /([+-]?0x[0-9a-f]+)/;

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
    this._selectionHandler = new SelectionHandler(this._disassemblyNode);

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
        variants: entry.variants.length > 1 ? [undefined, ...entry.variants] :
                                              undefined,
      };
    }
    this.requestUpdate();
  }

  _update() {
    this._updateSelect();
    this._updateDisassembly();
    this._updateFeedbackVector();
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
    this._disassemblyNode.innerText = '';
    if (!this._entry?.code) return;
    try {
      this._disassemblyNode.appendChild(
          new AssemblyFormatter(this._entry).fragment);
    } catch (e) {
      console.error(e);
      this._disassemblyNode.innerText = this._entry.code;
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

class AssemblyFormatter {
  constructor(codeLogEntry) {
    this._fragment = new DocumentFragment();
    this._entry = codeLogEntry;
    codeLogEntry.code.split('\n').forEach(line => this._addLine(line));
  }

  get fragment() {
    return this._fragment;
  }

  _addLine(line) {
    const parts = line.split(' ');
    let lineAddress = 0;
    if (kFullAddressRegexp.test(parts[0])) {
      lineAddress = parseInt(parts[0]);
    }
    const content = DOM.span({textContent: parts.join(' ') + '\n'});
    let formattedCode = content.innerHTML.split(kRegisterRegexpSplit)
                            .map(part => this._formatRegisterPart(part))
                            .join('');
    formattedCode = formattedCode.split(kAnyAddressRegexp)
                        .map(
                            (part, index) => this._formatAddressPart(
                                part, index, lineAddress))
                        .join('');
    // Let's replace the base-address since it doesn't add any value.
    // TODO
    content.innerHTML = formattedCode;
    this._fragment.appendChild(content);
  }

  _formatRegisterPart(part) {
    if (!kIsRegisterRegexp.test(part)) return part;
    return `<span class="reg ${part}">${part}</span>`
  }

  _formatAddressPart(part, index, lineAddress) {
    if (kFullAddressRegexp.test(part)) {
      // The first or second address must be the line address
      if (index <= 1) {
        return `<span class="addr line" data-addr="${part}">${part}</span>`;
      }
      return `<span class=addr data-addr="${part}">${part}</span>`;
    } else if (kRelativeAddressRegexp.test(part)) {
      const targetAddress = (lineAddress + parseInt(part)).toString(16);
      return `<span class=addr data-addr="0x${targetAddress}">${part}</span>`;
    } else {
      return part;
    }
  }
}

class SelectionHandler {
  _currentRegisterHovered;
  _currentRegisterClicked;

  constructor(node) {
    this._node = node;
    this._node.onmousemove = this._handleMouseMove.bind(this);
    this._node.onclick = this._handleClick.bind(this);
  }

  $(query) {
    return this._node.querySelectorAll(query);
  }

  _handleClick(event) {
    const target = event.target;
    if (target.classList.contains('addr')) {
      return this._handleClickAddress(target);
    } else if (target.classList.contains('reg')) {
      this._handleClickRegister(target);
    } else {
      this._clearRegisterSelection();
    }
  }

  _handleClickAddress(target) {
    let targetAddress = target.getAttribute('data-addr') ?? target.innerText;
    // Clear any selection
    for (let addrNode of this.$('.addr.selected')) {
      addrNode.classList.remove('selected');
    }
    // Highlight all matching addresses
    let lineAddrNode;
    for (let addrNode of this.$(`.addr[data-addr="${targetAddress}"]`)) {
      addrNode.classList.add('selected');
      if (addrNode.classList.contains('line') && lineAddrNode == undefined) {
        lineAddrNode = addrNode;
      }
    }
    // Jump to potential target address.
    if (lineAddrNode) {
      lineAddrNode.scrollIntoView({behavior: 'smooth', block: 'nearest'});
    }
  }

  _handleClickRegister(target) {
    this._setRegisterSelection(target.innerText);
    this._currentRegisterClicked = this._currentRegisterHovered;
  }

  _handleMouseMove(event) {
    if (this._currentRegisterClicked) return;
    const target = event.target;
    if (!target.classList.contains('reg')) {
      this._clearRegisterSelection();
    } else {
      this._setRegisterSelection(target.innerText);
    }
  }

  _clearRegisterSelection() {
    if (!this._currentRegisterHovered) return;
    for (let node of this.$('.reg.selected')) {
      node.classList.remove('selected');
    }
    this._currentRegisterClicked = undefined;
    this._currentRegisterHovered = undefined;
  }

  _setRegisterSelection(register) {
    if (register == this._currentRegisterHovered) return;
    this._clearRegisterSelection();
    this._currentRegisterHovered = register;
    for (let node of this.$(`.reg.${register}`)) {
      node.classList.add('selected');
    }
  }
}
