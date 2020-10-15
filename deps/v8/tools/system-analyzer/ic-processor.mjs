// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Event} from './event.mjs';
import {Timeline} from './timeline.mjs';

/**
 * Parser for dynamic code optimization state.
 */
function parseState(s) {
  switch (s) {
    case '':
      return Profile.CodeState.COMPILED;
    case '~':
      return Profile.CodeState.OPTIMIZABLE;
    case '*':
      return Profile.CodeState.OPTIMIZED;
  }
  throw new Error('unknown code state: ' + s);
}

class IcProcessor extends LogReader {
  #profile;
  MAJOR_VERSION = 8;
  MINOR_VERSION = 5;
  constructor() {
    super();
    let propertyICParser = [
      parseInt, parseInt, parseInt, parseInt, parseString, parseString,
      parseInt, parseString, parseString, parseString
    ];
    LogReader.call(this, {
      'code-creation': {
        parsers: [
          parseString, parseInt, parseInt, parseInt, parseInt, parseString,
          parseVarArgs
        ],
        processor: this.processCodeCreation
      },
      'v8-version': {
        parsers: [
          parseInt, parseInt,
        ],
        processor: this.processV8Version
      },
      'code-move':
          {parsers: [parseInt, parseInt], processor: this.processCodeMove},
      'code-delete': {parsers: [parseInt], processor: this.processCodeDelete},
      'sfi-move':
          {parsers: [parseInt, parseInt], processor: this.processFunctionMove},
      'LoadGlobalIC': {
        parsers: propertyICParser,
        processor: this.processPropertyIC.bind(this, 'LoadGlobalIC')
      },
      'StoreGlobalIC': {
        parsers: propertyICParser,
        processor: this.processPropertyIC.bind(this, 'StoreGlobalIC')
      },
      'LoadIC': {
        parsers: propertyICParser,
        processor: this.processPropertyIC.bind(this, 'LoadIC')
      },
      'StoreIC': {
        parsers: propertyICParser,
        processor: this.processPropertyIC.bind(this, 'StoreIC')
      },
      'KeyedLoadIC': {
        parsers: propertyICParser,
        processor: this.processPropertyIC.bind(this, 'KeyedLoadIC')
      },
      'KeyedStoreIC': {
        parsers: propertyICParser,
        processor: this.processPropertyIC.bind(this, 'KeyedStoreIC')
      },
      'StoreInArrayLiteralIC': {
        parsers: propertyICParser,
        processor: this.processPropertyIC.bind(this, 'StoreInArrayLiteralIC')
      },
    });
    this.#profile = new Profile();

    this.LoadGlobalIC = 0;
    this.StoreGlobalIC = 0;
    this.LoadIC = 0;
    this.StoreIC = 0;
    this.KeyedLoadIC = 0;
    this.KeyedStoreIC = 0;
    this.StoreInArrayLiteralIC = 0;
  }
  get profile(){
    return this.#profile;
  }
  /**
   * @override
   */
  printError(str) {
    print(str);
  }
  processString(string) {
    let end = string.length;
    let current = 0;
    let next = 0;
    let line;
    let i = 0;
    let entry;
    while (current < end) {
      next = string.indexOf('\n', current);
      if (next === -1) break;
      i++;
      line = string.substring(current, next);
      current = next + 1;
      this.processLogLine(line);
    }
  }
  processV8Version(majorVersion, minorVersion){
    if(
      (majorVersion == this.MAJOR_VERSION && minorVersion <= this.MINOR_VERSION)
        || (majorVersion < this.MAJOR_VERSION)){
          window.alert(
            `Unsupported version ${majorVersion}.${minorVersion}. \n` +
              `Please use the matching tool for given the V8 version.`);
    }
  }
  processLogFile(fileName) {
    this.collectEntries = true;
    this.lastLogFileName_ = fileName;
    let line;
    while (line = readline()) {
      this.processLogLine(line);
    }
    print();
    print('=====================');
    print('LoadGlobal: ' + this.LoadGlobalIC);
    print('StoreGlobal: ' + this.StoreGlobalIC);
    print('Load: ' + this.LoadIC);
    print('Store: ' + this.StoreIC);
    print('KeyedLoad: ' + this.KeyedLoadIC);
    print('KeyedStore: ' + this.KeyedStoreIC);
    print('StoreInArrayLiteral: ' + this.StoreInArrayLiteralIC);
  }
  addEntry(entry) {
    this.entries.push(entry);
  }
  processCodeCreation(type, kind, timestamp, start, size, name, maybe_func) {
    if (maybe_func.length) {
      let funcAddr = parseInt(maybe_func[0]);
      let state = parseState(maybe_func[1]);
      this.#profile.addFuncCode(
          type, name, timestamp, start, size, funcAddr, state);
    } else {
      this.#profile.addCode(type, name, timestamp, start, size);
    }
  }
  processCodeMove(from, to) {
    this.#profile.moveCode(from, to);
  }
  processCodeDelete(start) {
    this.#profile.deleteCode(start);
  }
  processFunctionMove(from, to) {
    this.#profile.moveFunc(from, to);
  }
  formatName(entry) {
    if (!entry) return '<unknown>';
    let name = entry.func.getName();
    let re = /(.*):[0-9]+:[0-9]+$/;
    let array = re.exec(name);
    if (!array) return name;
    return entry.getState() + array[1];
  }

  processPropertyIC(
      type, pc, time, line, column, old_state, new_state, map, name, modifier,
      slow_reason) {
    this[type]++;
    let entry = this.#profile.findEntry(pc);
    print(
        type + ' (' + old_state + '->' + new_state + modifier + ') at ' +
        this.formatName(entry) + ':' + line + ':' + column + ' ' + name +
        ' (map 0x' + map.toString(16) + ')' +
        (slow_reason ? ' ' + slow_reason : '') + 'time: ' + time);
  }
}

// ================

IcProcessor.kProperties = [
  'type',
  'category',
  'functionName',
  'filePosition',
  'state',
  'key',
  'map',
  'reason',
  'file'
];

class CustomIcProcessor extends IcProcessor {
  #timeline = new Timeline();

  functionName(pc) {
    let entry = this.profile.findEntry(pc);
    return this.formatName(entry);
  }

  processPropertyIC(
      type, pc, time, line, column, old_state, new_state, map, key, modifier,
      slow_reason) {
    let fnName = this.functionName(pc);
    let entry = new IcLogEvent(
      type, fnName, time, line, column, key, old_state, new_state, map,
      slow_reason);
    this.#timeline.push(entry);
  }


  get timeline(){
    return this.#timeline;
  }

  processString(string) {
    super.processString(string);
    return this.timeline;
  }
};

class IcLogEvent extends Event {
  constructor(
      type, fn_file, time, line, column, key, oldState, newState, map, reason,
      additional) {
    super(type, time);
    this.category = 'other';
    if (this.type.indexOf('Store') !== -1) {
      this.category = 'Store';
    } else if (this.type.indexOf('Load') !== -1) {
      this.category = 'Load';
    }
    let parts = fn_file.split(' ');
    this.functionName = parts[0];
    this.file = parts[1];
    let position = line + ':' + column;
    this.filePosition = this.file + ':' + position;
    this.oldState = oldState;
    this.newState = newState;
    this.state = this.oldState + ' → ' + this.newState;
    this.key = key;
    this.map = map.toString(16);
    this.reason = reason;
    this.additional = additional;
  }


  parseMapProperties(parts, offset) {
    let next = parts[++offset];
    if (!next.startsWith('dict')) return offset;
    this.propertiesMode = next.substr(5) == '0' ? 'fast' : 'slow';
    this.numberOfOwnProperties = parts[++offset].substr(4);
    next = parts[++offset];
    this.instanceType = next.substr(5, next.length - 6);
    return offset;
  }

  parsePositionAndFile(parts, start) {
    // find the position of 'at' in the parts array.
    let offset = start;
    for (let i = start + 1; i < parts.length; i++) {
      offset++;
      if (parts[i] == 'at') break;
    }
    if (parts[offset] !== 'at') return -1;
    this.position = parts.slice(start, offset).join(' ');
    offset += 1;
    this.isNative = parts[offset] == 'native'
    offset += this.isNative ? 1 : 0;
    this.file = parts[offset];
    return offset;
  }
}

export { CustomIcProcessor as default, IcLogEvent};
