// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LogReader, parseString, parseVarArgs} from '../logreader.mjs';
import {Profile} from '../profile.mjs';

import {ApiLogEntry} from './log/api.mjs';
import {CodeLogEntry, DeoptLogEntry} from './log/code.mjs';
import {IcLogEntry} from './log/ic.mjs';
import {Edge, MapLogEntry} from './log/map.mjs';
import {Timeline} from './timeline.mjs';

// ===========================================================================

export class Processor extends LogReader {
  _profile = new Profile();
  _mapTimeline = new Timeline();
  _icTimeline = new Timeline();
  _deoptTimeline = new Timeline();
  _codeTimeline = new Timeline();
  _apiTimeline = new Timeline();
  _formatPCRegexp = /(.*):[0-9]+:[0-9]+$/;
  _lastTimestamp = 0;
  _lastCodeLogEntry;
  MAJOR_VERSION = 7;
  MINOR_VERSION = 6;
  constructor(logString) {
    super();
    const propertyICParser = [
      parseInt, parseInt, parseInt, parseInt, parseString, parseString,
      parseString, parseString, parseString, parseString
    ];
    this.dispatchTable_ = {
      __proto__: null,
      'v8-version': {
        parsers: [
          parseInt,
          parseInt,
        ],
        processor: this.processV8Version
      },
      'code-creation': {
        parsers: [
          parseString, parseInt, parseInt, parseInt, parseInt, parseString,
          parseVarArgs
        ],
        processor: this.processCodeCreation
      },
      'code-deopt': {
        parsers: [
          parseInt, parseInt, parseInt, parseInt, parseInt, parseString,
          parseString, parseString
        ],
        processor: this.processCodeDeopt
      },
      'code-move':
          {parsers: [parseInt, parseInt], processor: this.processCodeMove},
      'code-delete': {parsers: [parseInt], processor: this.processCodeDelete},
      'code-source-info': {
        parsers: [
          parseInt, parseInt, parseInt, parseInt, parseString, parseString,
          parseString
        ],
        processor: this.processCodeSourceInfo
      },
      'code-disassemble': {
        parsers: [
          parseInt,
          parseString,
          parseString,
        ],
        processor: this.processCodeDisassemble
      },
      'script-source': {
        parsers: [parseInt, parseString, parseString],
        processor: this.processScriptSource
      },
      'sfi-move':
          {parsers: [parseInt, parseInt], processor: this.processFunctionMove},
      'map-create':
          {parsers: [parseInt, parseString], processor: this.processMapCreate},
      'map': {
        parsers: [
          parseString, parseInt, parseString, parseString, parseInt, parseInt,
          parseInt, parseString, parseString
        ],
        processor: this.processMap
      },
      'map-details': {
        parsers: [parseInt, parseString, parseString],
        processor: this.processMapDetails
      },
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
      'api': {
        parsers: [parseString, parseVarArgs],
        processor: this.processApiEvent
      },
    };
    if (logString) this.processString(logString);
  }

  printError(str) {
    console.error(str);
    throw str
  }

  processString(string) {
    let end = string.length;
    let current = 0;
    let next = 0;
    let line;
    let i = 0;
    let entry;
    try {
      while (current < end) {
        next = string.indexOf('\n', current);
        if (next === -1) break;
        i++;
        line = string.substring(current, next);
        current = next + 1;
        this.processLogLine(line);
      }
    } catch (e) {
      console.error(`Error occurred during parsing, trying to continue: ${e}`);
    }
    this.finalize();
  }

  processLogFile(fileName) {
    this.collectEntries = true;
    this.lastLogFileName_ = fileName;
    let i = 1;
    let line;
    try {
      while (line = readline()) {
        this.processLogLine(line);
        i++;
      }
    } catch (e) {
      console.error(
          `Error occurred during parsing line ${i}` +
          ', trying to continue: ' + e);
    }
    this.finalize();
  }

  finalize() {
    // TODO(cbruni): print stats;
    this._mapTimeline.transitions = new Map();
    let id = 0;
    this._mapTimeline.forEach(map => {
      if (map.isRoot()) id = map.finalizeRootMap(id + 1);
      if (map.edge && map.edge.name) {
        const edge = map.edge;
        const list = this._mapTimeline.transitions.get(edge.name);
        if (list === undefined) {
          this._mapTimeline.transitions.set(edge.name, [edge]);
        } else {
          list.push(edge);
        }
      }
    });
  }

  processV8Version(majorVersion, minorVersion) {
    if ((majorVersion == this.MAJOR_VERSION &&
         minorVersion <= this.MINOR_VERSION) ||
        (majorVersion < this.MAJOR_VERSION)) {
      window.alert(
          `Unsupported version ${majorVersion}.${minorVersion}. \n` +
          `Please use the matching tool for given the V8 version.`);
    }
  }

  processCodeCreation(type, kind, timestamp, start, size, name, maybe_func) {
    this._lastTimestamp = timestamp;
    let entry;
    let stateName = '';
    if (maybe_func.length) {
      const funcAddr = parseInt(maybe_func[0]);
      stateName = maybe_func[1] ?? '';
      const state = Profile.parseState(maybe_func[1]);
      entry = this._profile.addFuncCode(
          type, name, timestamp, start, size, funcAddr, state);
    } else {
      entry = this._profile.addCode(type, name, timestamp, start, size);
    }
    this._lastCodeLogEntry =
        new CodeLogEntry(type + stateName, timestamp, kind, entry);
    this._codeTimeline.push(this._lastCodeLogEntry);
  }

  processCodeDeopt(
      timestamp, codeSize, instructionStart, inliningId, scriptOffset,
      deoptKind, deoptLocation, deoptReason) {
    this._lastTimestamp = timestamp;
    const codeEntry = this._profile.findEntry(instructionStart);
    const logEntry = new DeoptLogEntry(
        deoptKind, timestamp, codeEntry, deoptReason, deoptLocation,
        scriptOffset, instructionStart, codeSize, inliningId);
    this._deoptTimeline.push(logEntry);
    this.addSourcePosition(codeEntry, logEntry);
    logEntry.functionSourcePosition = logEntry.sourcePosition;
    // custom parse deopt location
    if (deoptLocation !== '<unknown>') {
      const colSeparator = deoptLocation.lastIndexOf(':');
      const rowSeparator = deoptLocation.lastIndexOf(':', colSeparator - 1);
      const script = this.getScript(deoptLocation.substring(1, rowSeparator));
      const line =
          parseInt(deoptLocation.substring(rowSeparator + 1, colSeparator));
      const column = parseInt(
          deoptLocation.substring(colSeparator + 1, deoptLocation.length - 1));
      logEntry.sourcePosition =
          script.addSourcePosition(line, column, logEntry);
    }
  }

  processScriptSource(scriptId, url, source) {
    this._profile.addScriptSource(scriptId, url, source);
  }

  processCodeMove(from, to) {
    this._profile.moveCode(from, to);
  }

  processCodeDelete(start) {
    this._profile.deleteCode(start);
  }

  processFunctionMove(from, to) {
    this._profile.moveFunc(from, to);
  }

  processCodeSourceInfo(
      start, scriptId, startPos, endPos, sourcePositions, inliningPositions,
      inlinedFunctions) {
    this._profile.addSourcePositions(
        start, scriptId, startPos, endPos, sourcePositions, inliningPositions,
        inlinedFunctions);
    let profileEntry = this._profile.findEntry(start);
    if (profileEntry !== this._lastCodeLogEntry._entry) return;
    this.addSourcePosition(profileEntry, this._lastCodeLogEntry);
    this._lastCodeLogEntry = undefined;
  }

  addSourcePosition(profileEntry, logEntry) {
    let script = this.getProfileEntryScript(profileEntry);
    const parts = profileEntry.getRawName().split(':');
    if (parts.length < 3) return;
    const line = parseInt(parts[parts.length - 2]);
    const column = parseInt(parts[parts.length - 1]);
    logEntry.sourcePosition = script.addSourcePosition(line, column, logEntry);
  }

  processCodeDisassemble(start, kind, disassemble) {
    this._profile.addDisassemble(start, kind, disassemble);
  }

  processPropertyIC(
      type, pc, time, line, column, old_state, new_state, mapId, key, modifier,
      slow_reason) {
    this._lastTimestamp = time;
    const profileEntry = this._profile.findEntry(pc);
    const fnName = this.formatProfileEntry(profileEntry);
    const script = this.getProfileEntryScript(profileEntry);
    const map = this.getOrCreateMapEntry(mapId, time);
    // TODO: Use SourcePosition here directly
    let entry = new IcLogEntry(
        type, fnName, time, line, column, key, old_state, new_state, map,
        slow_reason, modifier);
    if (script) {
      entry.sourcePosition = script.addSourcePosition(line, column, entry);
    }
    this._icTimeline.push(entry);
  }

  formatProfileEntry(profileEntry, line, column) {
    if (!profileEntry) return '<unknown>';
    if (profileEntry.type === 'Builtin') return profileEntry.name;
    const name = profileEntry.func.getName();
    const array = this._formatPCRegexp.exec(name);
    const formatted =
        (array === null) ? name : profileEntry.getState() + array[1];
    if (line === undefined || column === undefined) return formatted;
    return `${formatted}:${line}:${column}`;
  }

  getProfileEntryScript(profileEntry) {
    if (!profileEntry) return undefined;
    if (profileEntry.type === 'Builtin') return undefined;
    const script = profileEntry.source?.script;
    if (script !== undefined) return script;
    // Slow path, try to get the script from the url:
    const fnName = this.formatProfileEntry(profileEntry);
    let parts = fnName.split(' ');
    let fileName = parts[parts.length - 1];
    return this.getScript(fileName);
  }

  processMap(type, time, from, to, pc, line, column, reason, name) {
    this._lastTimestamp = time;
    const time_ = parseInt(time);
    if (type === 'Deprecate') return this.deprecateMap(type, time_, from);
    // Skip normalized maps that were cached so we don't introduce multiple
    // edges with the same source and target map.
    if (type === 'NormalizeCached') return;
    const from_ = this.getOrCreateMapEntry(from, time_);
    const to_ = this.getOrCreateMapEntry(to, time_);
    if (type === 'Normalize') {
      // Fix a bug where we log "Normalize" transitions for maps created from
      // the NormalizedMapCache.
      if (to_.parent()?.id === from || to_.time < from_.time || to_.depth > 0) {
        console.log(`Skipping transition to cached normalized map`);
        return;
      }
    }
    // TODO: use SourcePosition directly.
    let edge = new Edge(type, name, reason, time, from_, to_);
    const profileEntry = this._profile.findEntry(pc)
    to_.entry = profileEntry;
    let script = this.getProfileEntryScript(profileEntry);
    if (script) {
      to_.sourcePosition = script.addSourcePosition(line, column, to_)
    }
    if (to_.parent() !== undefined && to_.parent() === from_) {
      // Fix bug where we double log transitions.
      console.warn('Fixing up double transition');
      to_.edge.updateFrom(edge);
    } else {
      edge.finishSetup();
    }
  }

  deprecateMap(type, time, id) {
    this._lastTimestamp = time;
    this.getOrCreateMapEntry(id, time).deprecate();
  }

  processMapCreate(time, id) {
    // map-create events might override existing maps if the addresses get
    // recycled. Hence we do not check for existing maps.
    this._lastTimestamp = time;
    this.createMapEntry(id, time);
  }

  processMapDetails(time, id, string) {
    // TODO(cbruni): fix initial map logging.
    const map = this.getOrCreateMapEntry(id, time);
    map.description = string;
  }

  createMapEntry(id, time) {
    this._lastTimestamp = time;
    const map = new MapLogEntry(id, time);
    this._mapTimeline.push(map);
    return map;
  }

  getOrCreateMapEntry(id, time) {
    if (id === '0x000000000000') return undefined;
    const map = MapLogEntry.get(id, time);
    if (map !== undefined) return map;
    console.warn(`No map details provided: id=${id}`);
    // Manually patch in a map to continue running.
    return this.createMapEntry(id, time);
  }

  getScript(url) {
    const script = this._profile.getScript(url);
    // TODO create placeholder script for empty urls.
    if (script === undefined) {
      console.error(`Could not find script for url: '${url}'`)
    }
    return script;
  }

  processApiEvent(type, varArgs) {
    let name, arg1;
    if (varArgs.length == 0) {
      const index = type.indexOf(':');
      if (index > 0) {
        name = type;
        type = type.substr(0, index);
      }
    } else {
      name = varArgs[0];
      arg1 = varArgs[1];
    }
    this._apiTimeline.push(
        new ApiLogEntry(type, this._lastTimestamp, name, arg1));
  }

  get icTimeline() {
    return this._icTimeline;
  }

  get mapTimeline() {
    return this._mapTimeline;
  }

  get deoptTimeline() {
    return this._deoptTimeline;
  }

  get codeTimeline() {
    return this._codeTimeline;
  }

  get apiTimeline() {
    return this._apiTimeline;
  }

  get scripts() {
    return this._profile.scripts_.filter(script => script !== undefined);
  }

  get profile() {
    return this._profile;
  }
}
