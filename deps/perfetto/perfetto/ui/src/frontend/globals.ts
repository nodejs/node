// Copyright (C) 2018 The Android Open Source Project
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

import {assertExists} from '../base/logging';
import {DeferredAction} from '../common/actions';
import {AggregateData} from '../common/aggregation_data';
import {CurrentSearchResults, SearchSummary} from '../common/search_data';
import {CallsiteInfo, createEmptyState, State} from '../common/state';

import {FrontendLocalState} from './frontend_local_state';
import {RafScheduler} from './raf_scheduler';
import {ServiceWorkerController} from './service_worker_controller';

type Dispatch = (action: DeferredAction) => void;
type TrackDataStore = Map<string, {}>;
type QueryResultsStore = Map<string, {}>;
type AggregateDataStore = Map<string, AggregateData>;
type Description = Map<string, string>;
type Args = Map<string, string>;
export interface SliceDetails {
  ts?: number;
  dur?: number;
  priority?: number;
  endState?: string;
  cpu?: number;
  id?: number;
  utid?: number;
  wakeupTs?: number;
  wakerUtid?: number;
  wakerCpu?: number;
  category?: string;
  name?: string;
  args?: Args;
  description?: Description;
}

export interface CounterDetails {
  startTime?: number;
  value?: number;
  delta?: number;
  duration?: number;
}

export interface HeapProfileDetails {
  type?: string;
  id?: number;
  ts?: number;
  tsNs?: number;
  pid?: number;
  upid?: number;
  flamegraph?: CallsiteInfo[];
  expandedCallsite?: CallsiteInfo;
  viewingOption?: string;
  expandedId?: number;
}

export interface QuantizedLoad {
  startSec: number;
  endSec: number;
  load: number;
}
type OverviewStore = Map<string, QuantizedLoad[]>;

export interface ThreadDesc {
  utid: number;
  tid: number;
  threadName: string;
  pid?: number;
  procName?: string;
}
type ThreadMap = Map<number, ThreadDesc>;

/**
 * Global accessors for state/dispatch in the frontend.
 */
class Globals {
  private _dispatch?: Dispatch = undefined;
  private _controllerWorker?: Worker = undefined;
  private _state?: State = undefined;
  private _frontendLocalState?: FrontendLocalState = undefined;
  private _rafScheduler?: RafScheduler = undefined;
  private _serviceWorkerController?: ServiceWorkerController = undefined;

  // TODO(hjd): Unify trackDataStore, queryResults, overviewStore, threads.
  private _trackDataStore?: TrackDataStore = undefined;
  private _queryResults?: QueryResultsStore = undefined;
  private _overviewStore?: OverviewStore = undefined;
  private _aggregateDataStore?: AggregateDataStore = undefined;
  private _threadMap?: ThreadMap = undefined;
  private _sliceDetails?: SliceDetails = undefined;
  private _counterDetails?: CounterDetails = undefined;
  private _heapProfileDetails?: HeapProfileDetails = undefined;
  private _numQueriesQueued = 0;
  private _bufferUsage?: number = undefined;
  private _recordingLog?: string = undefined;

  private _currentSearchResults: CurrentSearchResults = {
    sliceIds: new Float64Array(0),
    tsStarts: new Float64Array(0),
    utids: new Float64Array(0),
    trackIds: [],
    sources: [],
    totalResults: 0,
  };
  searchSummary: SearchSummary = {
    tsStarts: new Float64Array(0),
    tsEnds: new Float64Array(0),
    count: new Uint8Array(0),
  };

  // This variable is set by the is_internal_user.js script if the user is a
  // googler. This is used to avoid exposing features that are not ready yet
  // for public consumption. The gated features themselves are not secret.
  isInternalUser = false;

  initialize(dispatch: Dispatch, controllerWorker: Worker) {
    this._dispatch = dispatch;
    this._controllerWorker = controllerWorker;
    this._state = createEmptyState();
    this._frontendLocalState = new FrontendLocalState();
    this._rafScheduler = new RafScheduler();
    this._serviceWorkerController = new ServiceWorkerController();

    // TODO(hjd): Unify trackDataStore, queryResults, overviewStore, threads.
    this._trackDataStore = new Map<string, {}>();
    this._queryResults = new Map<string, {}>();
    this._overviewStore = new Map<string, QuantizedLoad[]>();
    this._aggregateDataStore = new Map<string, AggregateData>();
    this._threadMap = new Map<number, ThreadDesc>();
    this._sliceDetails = {};
    this._counterDetails = {};
    this._heapProfileDetails = {};
  }

  get state(): State {
    return assertExists(this._state);
  }

  set state(state: State) {
    this._state = assertExists(state);
  }

  get dispatch(): Dispatch {
    return assertExists(this._dispatch);
  }

  get frontendLocalState() {
    return assertExists(this._frontendLocalState);
  }

  get rafScheduler() {
    return assertExists(this._rafScheduler);
  }

  get serviceWorkerController() {
    return assertExists(this._serviceWorkerController);
  }

  // TODO(hjd): Unify trackDataStore, queryResults, overviewStore, threads.
  get overviewStore(): OverviewStore {
    return assertExists(this._overviewStore);
  }

  get trackDataStore(): TrackDataStore {
    return assertExists(this._trackDataStore);
  }

  get queryResults(): QueryResultsStore {
    return assertExists(this._queryResults);
  }

  get threads() {
    return assertExists(this._threadMap);
  }

  get sliceDetails() {
    return assertExists(this._sliceDetails);
  }

  set sliceDetails(click: SliceDetails) {
    this._sliceDetails = assertExists(click);
  }

  get counterDetails() {
    return assertExists(this._counterDetails);
  }

  set counterDetails(click: CounterDetails) {
    this._counterDetails = assertExists(click);
  }

  get aggregateDataStore(): AggregateDataStore {
    return assertExists(this._aggregateDataStore);
  }

  get heapProfileDetails() {
    return assertExists(this._heapProfileDetails);
  }

  set heapProfileDetails(click: HeapProfileDetails) {
    this._heapProfileDetails = assertExists(click);
  }

  set numQueuedQueries(value: number) {
    this._numQueriesQueued = value;
  }

  get numQueuedQueries() {
    return this._numQueriesQueued;
  }

  get bufferUsage() {
    return this._bufferUsage;
  }

  get recordingLog() {
    return this._recordingLog;
  }

  get currentSearchResults() {
    return this._currentSearchResults;
  }

  set currentSearchResults(results: CurrentSearchResults) {
    this._currentSearchResults = results;
  }

  setBufferUsage(bufferUsage: number) {
    this._bufferUsage = bufferUsage;
  }

  setTrackData(id: string, data: {}) {
    this.trackDataStore.set(id, data);
  }

  setRecordingLog(recordingLog: string) {
    this._recordingLog = recordingLog;
  }

  setAggregateData(kind: string, data: AggregateData) {
    this.aggregateDataStore.set(kind, data);
  }

  getCurResolution() {
    // Truncate the resolution to the closest power of 2.
    // This effectively means the resolution changes every 6 zoom levels.
    const resolution = this.frontendLocalState.timeScale.deltaPxToDuration(1);
    return Math.pow(2, Math.floor(Math.log2(resolution)));
  }

  makeSelection(action: DeferredAction<{}>) {
    // A new selection should cancel the current search selection.
    globals.frontendLocalState.searchIndex = -1;
    globals.frontendLocalState.currentTab =
        action.type === 'deselect' ? undefined : 'current_selection';
    globals.dispatch(action);
  }

  resetForTesting() {
    this._dispatch = undefined;
    this._state = undefined;
    this._frontendLocalState = undefined;
    this._rafScheduler = undefined;
    this._serviceWorkerController = undefined;

    // TODO(hjd): Unify trackDataStore, queryResults, overviewStore, threads.
    this._trackDataStore = undefined;
    this._queryResults = undefined;
    this._overviewStore = undefined;
    this._threadMap = undefined;
    this._sliceDetails = undefined;
    this._aggregateDataStore = undefined;
    this._numQueriesQueued = 0;
    this._currentSearchResults = {
      sliceIds: new Float64Array(0),
      tsStarts: new Float64Array(0),
      utids: new Float64Array(0),
      trackIds: [],
      sources: [],
      totalResults: 0,
    };
  }

  // Used when switching to the legacy TraceViewer UI.
  // Most resources are cleaned up by replacing the current |window| object,
  // however pending RAFs and workers seem to outlive the |window| and need to
  // be cleaned up explicitly.
  shutdown() {
    this._controllerWorker!.terminate();
    this._rafScheduler!.shutdown();
  }
}

export const globals = new Globals();
