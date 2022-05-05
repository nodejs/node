// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {calcOffsetInVMCage} from '../js/helper.mjs';
import {DOM, FileReader,} from '../js/web-api-helper.mjs';

import {kSpaceNames} from './space-categories.mjs';

class TraceLogParseHelper {
  static re_gc_header = /(Before|After) GC:\d/;
  static re_page_info =
      /\{owner:.+,address:.+,size:.+,allocated_bytes:.+,wasted_memory:.+\}/;
  static re_owner = /(?<=owner:)[a-z_]+_space/;
  static re_address = /(?<=address:)0x[a-f0-9]+(?=,)/;
  static re_size = /(?<=size:)\d+(?=,)/;
  static re_allocated_bytes = /(?<=allocated_bytes:)\d+(?=,)/;
  static re_wasted_memory = /(?<=wasted_memory:)\d+(?=})/;

  static matchGCHeader(content) {
    return this.re_gc_header.test(content);
  }

  static matchPageInfo(content) {
    return this.re_page_info.test(content);
  }

  static parsePageInfo(content) {
    const owner = this.re_owner.exec(content)[0];
    const address =
        calcOffsetInVMCage(BigInt(this.re_address.exec(content)[0], 16));
    const size = parseInt(this.re_size.exec(content)[0]);
    const allocated_bytes = parseInt(this.re_allocated_bytes.exec(content)[0]);
    const wasted_memory = parseInt(this.re_wasted_memory.exec(content)[0]);
    const info = [
      owner,
      address,
      address + size,
      allocated_bytes,
      wasted_memory,
    ];
    return info;
  }

  // Create a empty snapshot.
  static createSnapShotData() {
    let snapshot = {header: null, data: {}};
    for (let space_name of kSpaceNames) {
      snapshot.data[space_name] = [];
    }
    return snapshot;
  }

  static createModelFromV8TraceFile(contents) {
    let snapshots = [];
    let snapshot = this.createSnapShotData();

    // Fill data info a snapshot, then push it into snapshots.
    for (let content of contents) {
      if (this.matchGCHeader(content)) {
        if (snapshot.header != null) {
          snapshots.push(snapshot);
        }
        snapshot = this.createSnapShotData();
        snapshot.header = content;
        continue;
      }

      if (this.matchPageInfo(content)) {
        let pageinfo = this.parsePageInfo(content);
        try {
          snapshot.data[pageinfo[0]].push(pageinfo);
        } catch (e) {
          console.error(e);
        }
      }
    }
    // EOL, push the last.
    if (snapshot.header != null) {
      snapshots.push(snapshot);
    }
    return snapshots;
  }
}

DOM.defineCustomElement('../js/log-file-reader', 'trace-file-reader',
                        (templateText) =>
                            class TraceFileReader extends FileReader {
  constructor() {
    super(templateText);
    this.fullDataFromFile = '';
    this.addEventListener('fileuploadchunk', (e) => this.handleLoadChunk(e));

    this.addEventListener('fileuploadend', (e) => this.handleLoadEnd(e));
  }

  handleLoadChunk(event) {
    this.fullDataFromFile += event.detail;
  }

  handleLoadEnd(event) {
    let contents = this.fullDataFromFile.split('\n');
    let snapshots = TraceLogParseHelper.createModelFromV8TraceFile(contents);
    this.dispatchEvent(new CustomEvent('change', {
      bubbles: true,
      composed: true,
      detail: snapshots,
    }));
  }
});
