// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {Engine} from '../common/engine';
import {
  ALLOC_SPACE_MEMORY_ALLOCATED_KEY,
  DEFAULT_VIEWING_OPTION,
  expandCallsites,
  findRootSize,
  mergeCallsites,
  OBJECTS_ALLOCATED_KEY,
  OBJECTS_ALLOCATED_NOT_FREED_KEY,
  SPACE_MEMORY_ALLOCATED_NOT_FREED_KEY
} from '../common/flamegraph_util';
import {CallsiteInfo, HeapProfileFlamegraph} from '../common/state';
import {fromNs} from '../common/time';
import {HeapProfileDetails} from '../frontend/globals';

import {Controller} from './controller';
import {globals} from './globals';

export interface HeapProfileControllerArgs {
  engine: Engine;
}
const MIN_PIXEL_DISPLAYED = 1;

class TablesCache {
  private engine: Engine;
  private cache: Map<string, string>;
  private prefix: string;
  private tableId: number;
  private cacheSizeLimit: number;

  constructor(engine: Engine, prefix: string) {
    this.engine = engine;
    this.cache = new Map<string, string>();
    this.prefix = prefix;
    this.tableId = 0;
    this.cacheSizeLimit = 10;
  }

  async getTableName(query: string): Promise<string> {
    let tableName = this.cache.get(query);
    if (tableName === undefined) {
      // TODO(hjd): This should be LRU.
      if (this.cache.size > this.cacheSizeLimit) {
        for (const name of this.cache.values()) {
          await this.engine.query(`drop table ${name}`);
        }
        this.cache.clear();
      }
      tableName = `${this.prefix}_${this.tableId++}`;
      await this.engine.query(
          `create temp table if not exists ${tableName} as ${query}`);
      this.cache.set(query, tableName);
    }
    return tableName;
  }
}

export class HeapProfileController extends Controller<'main'> {
  private flamegraphDatasets: Map<string, CallsiteInfo[]> = new Map();
  private lastSelectedHeapProfile?: HeapProfileFlamegraph;
  private requestingData = false;
  private queuedRequest = false;
  private heapProfileDetails: HeapProfileDetails = {};
  private cache: TablesCache;

  constructor(private args: HeapProfileControllerArgs) {
    super('main');
    this.cache = new TablesCache(args.engine, 'grouped_callsites');
  }

  run() {
    const selection = globals.state.currentHeapProfileFlamegraph;

    if (!selection) return;

    if (this.shouldRequestData(selection)) {
      if (this.requestingData) {
        this.queuedRequest = true;
      } else {
        this.requestingData = true;
        const selectedHeapProfile: HeapProfileFlamegraph =
            this.copyHeapProfile(selection);

        this.getHeapProfileMetadata(
                selection.type,
                selectedHeapProfile.ts,
                selectedHeapProfile.upid)
            .then(result => {
              if (result !== undefined) {
                Object.assign(this.heapProfileDetails, result);
              }

              // TODO(hjd): Clean this up.
              if (this.lastSelectedHeapProfile &&
                  this.lastSelectedHeapProfile.focusRegex !==
                      selection.focusRegex) {
                this.flamegraphDatasets.clear();
              }

              this.lastSelectedHeapProfile = this.copyHeapProfile(selection);

              const expandedId = selectedHeapProfile.expandedCallsite ?
                  selectedHeapProfile.expandedCallsite.id :
                  -1;
              const rootSize =
                  selectedHeapProfile.expandedCallsite === undefined ?
                  undefined :
                  selectedHeapProfile.expandedCallsite.totalSize;

              const key =
                  `${selectedHeapProfile.upid};${selectedHeapProfile.ts}`;

              this.getFlamegraphData(
                      key,
                      selectedHeapProfile.viewingOption ?
                          selectedHeapProfile.viewingOption :
                          DEFAULT_VIEWING_OPTION,
                      selection.ts,
                      selectedHeapProfile.upid,
                      selectedHeapProfile.type,
                      selectedHeapProfile.focusRegex)
                  .then(flamegraphData => {
                    if (flamegraphData !== undefined && selection &&
                        selection.kind === selectedHeapProfile.kind &&
                        selection.id === selectedHeapProfile.id &&
                        selection.ts === selectedHeapProfile.ts) {
                      const expandedFlamegraphData =
                          expandCallsites(flamegraphData, expandedId);
                      this.prepareAndMergeCallsites(
                          expandedFlamegraphData,
                          this.lastSelectedHeapProfile!.viewingOption,
                          rootSize,
                          this.lastSelectedHeapProfile!.expandedCallsite);
                    }
                  })
                  .finally(() => {
                    this.requestingData = false;
                    if (this.queuedRequest) {
                      this.queuedRequest = false;
                      this.run();
                    }
                  });
            });
      }
    }
  }

  private copyHeapProfile(heapProfile: HeapProfileFlamegraph):
      HeapProfileFlamegraph {
    return {
      kind: heapProfile.kind,
      id: heapProfile.id,
      upid: heapProfile.upid,
      ts: heapProfile.ts,
      type: heapProfile.type,
      expandedCallsite: heapProfile.expandedCallsite,
      viewingOption: heapProfile.viewingOption,
      focusRegex: heapProfile.focusRegex,
    };
  }

  private shouldRequestData(selection: HeapProfileFlamegraph) {
    return selection.kind === 'HEAP_PROFILE_FLAMEGRAPH' &&
        (this.lastSelectedHeapProfile === undefined ||
         (this.lastSelectedHeapProfile !== undefined &&
          (this.lastSelectedHeapProfile.id !== selection.id ||
           this.lastSelectedHeapProfile.ts !== selection.ts ||
           this.lastSelectedHeapProfile.type !== selection.type ||
           this.lastSelectedHeapProfile.upid !== selection.upid ||
           this.lastSelectedHeapProfile.viewingOption !==
               selection.viewingOption ||
           this.lastSelectedHeapProfile.focusRegex !== selection.focusRegex ||
           this.lastSelectedHeapProfile.expandedCallsite !==
               selection.expandedCallsite)));
  }

  private prepareAndMergeCallsites(
      flamegraphData: CallsiteInfo[],
      viewingOption: string|undefined = DEFAULT_VIEWING_OPTION,
      rootSize?: number, expandedCallsite?: CallsiteInfo) {
    const mergedFlamegraphData = mergeCallsites(
        flamegraphData, this.getMinSizeDisplayed(flamegraphData, rootSize));
    this.heapProfileDetails.flamegraph = mergedFlamegraphData;
    this.heapProfileDetails.expandedCallsite = expandedCallsite;
    this.heapProfileDetails.viewingOption = viewingOption;
    globals.publish('HeapProfileDetails', this.heapProfileDetails);
  }


  async getFlamegraphData(
      baseKey: string, viewingOption: string, ts: number, upid: number,
      type: string, focusRegex: string): Promise<CallsiteInfo[]> {
    let currentData: CallsiteInfo[];
    const key = `${baseKey}-${viewingOption}`;
    if (this.flamegraphDatasets.has(key)) {
      currentData = this.flamegraphDatasets.get(key)!;
    } else {
      // TODO(taylori): Show loading state.

      // Collecting data for drawing flamegraph for selected heap profile.
      // Data needs to be in following format:
      // id, name, parent_id, depth, total_size
      const tableName =
          await this.prepareViewsAndTables(ts, upid, type, focusRegex);
      currentData =
          await this.getFlamegraphDataFromTables(tableName, viewingOption);
      this.flamegraphDatasets.set(key, currentData);
    }
    return currentData;
  }

  async getFlamegraphDataFromTables(
      tableName: string, viewingOption = DEFAULT_VIEWING_OPTION) {
    let orderBy = '';
    let sizeIndex = 4;
    let selfIndex = 9;
    // TODO(fmayer): Improve performance so this is no longer necessary.
    // Alternatively consider collapsing frames of the same label.
    const maxDepth = 100;
    switch (viewingOption) {
      case SPACE_MEMORY_ALLOCATED_NOT_FREED_KEY:
        orderBy = `where cumulative_size > 0 and depth < ${
            maxDepth} order by depth, parent_id,
            cumulative_size desc, name`;
        sizeIndex = 4;
        selfIndex = 9;
        break;
      case ALLOC_SPACE_MEMORY_ALLOCATED_KEY:
        orderBy = `where cumulative_alloc_size > 0 and depth < ${
            maxDepth} order by depth, parent_id,
            cumulative_alloc_size desc, name`;
        sizeIndex = 5;
        selfIndex = 9;
        break;
      case OBJECTS_ALLOCATED_NOT_FREED_KEY:
        orderBy = `where cumulative_count > 0 and depth < ${
            maxDepth} order by depth, parent_id,
            cumulative_count desc, name`;
        sizeIndex = 6;
        selfIndex = 10;
        break;
      case OBJECTS_ALLOCATED_KEY:
        orderBy = `where cumulative_alloc_count > 0 and depth < ${
            maxDepth} order by depth, parent_id,
            cumulative_alloc_count desc, name`;
        sizeIndex = 7;
        selfIndex = 10;
        break;
      default:
        break;
    }

    const callsites = await this.args.engine.query(
        `SELECT id, IFNULL(DEMANGLE(name), name), IFNULL(parent_id, -1), depth,
        cumulative_size, cumulative_alloc_size, cumulative_count,
        cumulative_alloc_count, map_name, size, count from ${tableName} ${
            orderBy}`);

    const flamegraphData: CallsiteInfo[] = new Array();
    const hashToindex: Map<number, number> = new Map();
    for (let i = 0; i < callsites.numRecords; i++) {
      const hash = callsites.columns[0].longValues![i];
      let name = callsites.columns[1].stringValues![i];
      const parentHash = callsites.columns[2].longValues![i];
      const depth = +callsites.columns[3].longValues![i];
      const totalSize = +callsites.columns[sizeIndex].longValues![i];
      const mapping = callsites.columns[8].stringValues![i];
      const selfSize = +callsites.columns[selfIndex].longValues![i];
      const parentId =
          hashToindex.has(+parentHash) ? hashToindex.get(+parentHash)! : -1;
      if (depth === maxDepth - 1) {
        name += ' [tree truncated]';
      }
      hashToindex.set(+hash, i);
      // Instead of hash, we will store index of callsite in this original array
      // as an id of callsite. That way, we have quicker access to parent and it
      // will stay unique.
      flamegraphData.push({
        id: i,
        totalSize,
        depth,
        parentId,
        name,
        selfSize,
        mapping,
        merged: false
      });
    }
    return flamegraphData;
  }

  private async prepareViewsAndTables(
      ts: number, upid: number, type: string,
      focusRegex: string): Promise<string> {
    // Creating unique names for views so we can reuse and not delete them
    // for each marker.
    let whereClause = '';
    if (focusRegex !== '') {
      whereClause = `where focus_str = '${focusRegex}'`;
    }

    return this.cache.getTableName(
        `select id, name, map_name, parent_id, depth, cumulative_size,
          cumulative_alloc_size, cumulative_count, cumulative_alloc_count,
          size, alloc_size, count, alloc_count
          from experimental_flamegraph(${ts}, ${upid}, '${type}') ${
            whereClause}`);
  }

  getMinSizeDisplayed(flamegraphData: CallsiteInfo[], rootSize?: number):
      number {
    const timeState = globals.state.frontendLocalState.visibleState;
    const width =
        (timeState.endSec - timeState.startSec) / timeState.resolution;
    if (rootSize === undefined) {
      rootSize = findRootSize(flamegraphData);
    }
    return MIN_PIXEL_DISPLAYED * rootSize / width;
  }

  async getHeapProfileMetadata(type: string, ts: number, upid: number) {
    // Don't do anything if selection of the marker stayed the same.
    if ((this.lastSelectedHeapProfile !== undefined &&
         ((this.lastSelectedHeapProfile.ts === ts &&
           this.lastSelectedHeapProfile.upid === upid)))) {
      return undefined;
    }

    // Collecting data for more information about heap profile, such as:
    // total memory allocated, memory that is allocated and not freed.
    const pidValue = await this.args.engine.query(
        `select pid from process where upid = ${upid}`);
    const pid = pidValue.columns[0].longValues![0];
    const startTime = fromNs(ts) - globals.state.traceTime.startSec;
    return {ts: startTime, tsNs: ts, pid, upid, type};
  }
}
