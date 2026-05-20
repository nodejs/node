// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SelectRelatedEvent} from './events.mjs';
import {CollapsableElement, DOM, formatBytes, formatMicroSeconds} from './helper.mjs';

DOM.defineCustomElement(
    'view/code-panel',
    (templateText) => class CodePanel extends CollapsableElement {
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
        this.$('.panel').style.display =
            timeline.isEmpty() ? 'none' : 'inherit';
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
            variants: entry.variants.length > 1 ?
                [undefined, ...entry.variants] :
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
        return `${code.functionName}(...) t=${
            formatMicroSeconds(code.time)} size=${
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

const kRegisters = ['rsp', 'rbp', 'rax', 'rbx', 'rcx', 'rdx', 'rsi', 'rdi'];
// Make sure we dont match register on bytecode: Star1 or Star2
const kAvoidBytecodeOpsRegexpSource = '(.*?[^a-zA-Z])'
// Look for registers in strings like:  movl rbx,[rcx-0x30]
const kRegisterRegexpSource = `(?<register>${kRegisters.join('|')}|r[0-9]+)`
const kRegisterSplitRegexp =
    new RegExp(`${kAvoidBytecodeOpsRegexpSource}${kRegisterRegexpSource}`)
const kIsRegisterRegexp = new RegExp(`^${kRegisterRegexpSource}$`);

const kFullAddressRegexp = /(0x[0-9a-f]{8,})/;
const kRelativeAddressRegexp = /([+-]0x[0-9a-f]+)/;
const kAnyAddressRegexp = /(?<address>[+-]?0x[0-9a-f]+)/;

const kJmpRegexp = new RegExp(`jmp ${kRegisterRegexpSource}`);
const kMovRegexp =
    new RegExp(`mov. ${kRegisterRegexpSource},${kAnyAddressRegexp.source}`);

class AssemblyFormatter {
  constructor(codeLogEntry) {
    this._fragment = new DocumentFragment();
    this._entry = codeLogEntry;
    this._lines = new Map();
    this._previousLine = undefined;
    this._parseLines();
    this._format();
  }

  get fragment() {
    return this._fragment;
  }

  _format() {
    let block = DOM.div(['basicBlock', 'header']);
    this._lines.forEach(line => {
      if (!block || line.isBlockStart) {
        this._fragment.appendChild(block);
        block = DOM.div('basicBlock');
      }
      block.appendChild(line.format())
    });
    this._fragment.appendChild(block);
  }

  _parseLines() {
    this._entry.code.split('\n').forEach(each => this._parseLine(each));
    this._findBasicBlocks();
  }

  _parseLine(line) {
    const parts = line.split(' ');
    // Use unique placeholder for address:
    let lineAddress = -this._lines.size;
    for (let part of parts) {
      if (kFullAddressRegexp.test(part)) {
        lineAddress = parseInt(part);
        break;
      }
    }
    const newLine = new AssemblyLine(lineAddress, parts);
    // special hack for: mov reg 0x...; jmp reg;
    if (lineAddress <= 0 && this._previousLine) {
      const jmpMatch = line.match(kJmpRegexp);
      if (jmpMatch) {
        const register = jmpMatch.groups.register;
        const movMatch = this._previousLine.line.match(kMovRegexp);
        if (movMatch.groups.register === register) {
          newLine.outgoing.push(movMatch.groups.address);
        }
      }
    }
    this._lines.set(lineAddress, newLine);
    this._previousLine = newLine;
  }

  _findBasicBlocks() {
    const lines = Array.from(this._lines.values());
    for (let i = 0; i < lines.length; i++) {
      const line = lines[i];
      let forceBasicBlock = i == 0;
      if (i > 0 && i < lines.length - 1) {
        const prevHasAddress = lines[i - 1].address > 0;
        const currentHasAddress = lines[i].address > 0;
        const nextHasAddress = lines[i + 1].address > 0;
        if (prevHasAddress !== currentHasAddress &&
            currentHasAddress == nextHasAddress) {
          forceBasicBlock = true;
        }
      }
      if (forceBasicBlock) {
        // Add fake-incoming address to mark a block start.
        line.addIncoming(0);
      }
      line.outgoing.forEach(address => {
        const outgoing = this._lines.get(address);
        if (outgoing) outgoing.addIncoming(line.address);
      })
    }
  }
}

class AssemblyLine {
  constructor(address, parts) {
    this.address = address;
    this.outgoing = [];
    this.incoming = [];
    parts.forEach(part => {
      const fullMatch = part.match(kFullAddressRegexp);
      if (fullMatch) {
        let inlineAddress = parseInt(fullMatch[0]);
        if (inlineAddress != this.address) this.outgoing.push(inlineAddress);
        if (Number.isNaN(inlineAddress)) throw 'invalid address';
      } else if (kRelativeAddressRegexp.test(part)) {
        this.outgoing.push(this._toAbsoluteAddress(part));
      }
    });
    this.line = parts.join(' ');
  }

  get isBlockStart() {
    return this.incoming.length > 0;
  }

  addIncoming(address) {
    this.incoming.push(address);
  }

  format() {
    const content = DOM.span({textContent: this.line + '\n'});
    let formattedCode = content.innerHTML.split(kRegisterSplitRegexp)
                            .map(part => this._formatRegisterPart(part))
                            .join('');
    formattedCode =
        formattedCode.split(kAnyAddressRegexp)
            .map((part, index) => this._formatAddressPart(part, index))
            .join('');
    // Let's replace the base-address since it doesn't add any value.
    // TODO
    content.innerHTML = formattedCode;
    return content;
  }

  _formatRegisterPart(part) {
    if (!kIsRegisterRegexp.test(part)) return part;
    return `<span class="reg ${part}">${part}</span>`
  }

  _formatAddressPart(part, index) {
    if (kFullAddressRegexp.test(part)) {
      // The first or second address must be the line address
      if (index <= 1) {
        return `<span class="addr line" data-addr="${part}">${part}</span>`;
      }
      return `<span class=addr data-addr="${part}">${part}</span>`;
    } else if (kRelativeAddressRegexp.test(part)) {
      return `<span class=addr data-addr="0x${
          this._toAbsoluteAddress(part).toString(16)}">${part}</span>`;
    } else {
      return part;
    }
  }

  _toAbsoluteAddress(part) {
    return this.address + parseInt(part);
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
