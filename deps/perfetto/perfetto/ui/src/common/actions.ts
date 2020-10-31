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

import {Draft} from 'immer';

import {assertExists} from '../base/logging';
import {
  Area,
  CallsiteInfo,
  HeapProfileFlamegraphViewingOption
} from '../common/state';
import {ConvertTrace, ConvertTraceToPprof} from '../controller/trace_converter';

import {DEFAULT_VIEWING_OPTION} from './flamegraph_util';
import {
  AdbRecordingTarget,
  createEmptyState,
  EngineMode,
  LogsPagination,
  NewEngineMode,
  OmniboxState,
  RecordConfig,
  RecordingTarget,
  SCROLLING_TRACK_GROUP,
  State,
  Status,
  TimestampedAreaSelection,
  TraceSource,
  TraceTime,
  TrackState,
  VisibleState,
} from './state';

type StateDraft = Draft<State>;

export interface AddTrackArgs {
  id?: string;
  engineId: string;
  kind: string;
  name: string;
  trackGroup?: string;
  config: {};
}

function clearTraceState(state: StateDraft) {
  const nextId = state.nextId;
  const recordConfig = state.recordConfig;
  const route = state.route;
  const recordingTarget = state.recordingTarget;
  const extensionInstalled = state.extensionInstalled;
  const availableAdbDevices = state.availableAdbDevices;
  const chromeCategories = state.chromeCategories;
  const newEngineMode = state.newEngineMode;

  Object.assign(state, createEmptyState());
  state.nextId = nextId;
  state.recordConfig = recordConfig;
  state.route = route;
  state.recordingTarget = recordingTarget;
  state.extensionInstalled = extensionInstalled;
  state.availableAdbDevices = availableAdbDevices;
  state.chromeCategories = chromeCategories;
  state.newEngineMode = newEngineMode;
}

export const StateActions = {

  navigate(state: StateDraft, args: {route: string}): void {
    state.route = args.route;
  },

  openTraceFromFile(state: StateDraft, args: {file: File}): void {
    clearTraceState(state);
    const id = `${state.nextId++}`;
    state.engines[id] = {
      id,
      ready: false,
      source: {type: 'FILE', file: args.file},
    };
    state.route = `/viewer`;
  },

  openTraceFromBuffer(state: StateDraft, args: {buffer: ArrayBuffer}): void {
    clearTraceState(state);
    const id = `${state.nextId++}`;
    state.engines[id] = {
      id,
      ready: false,
      source: {type: 'ARRAY_BUFFER', buffer: args.buffer},
    };
    state.route = `/viewer`;
  },

  openTraceFromUrl(state: StateDraft, args: {url: string}): void {
    clearTraceState(state);
    const id = `${state.nextId++}`;
    state.engines[id] = {
      id,
      ready: false,
      source: {type: 'URL', url: args.url},
    };
    state.route = `/viewer`;
  },

  openTraceFromHttpRpc(state: StateDraft, _args: {}): void {
    clearTraceState(state);
    const id = `${state.nextId++}`;
    state.engines[id] = {
      id,
      ready: false,
      source: {type: 'HTTP_RPC'},
    };
    state.route = `/viewer`;
  },

  openVideoFromFile(state: StateDraft, args: {file: File}): void {
    state.video = URL.createObjectURL(args.file);
    state.videoEnabled = true;
  },

  // TODO(b/141359485): Actions should only modify state.
  convertTraceToJson(
      _: StateDraft, args: {file: Blob, truncate?: 'start'|'end'}): void {
    ConvertTrace(args.file, args.truncate);
  },

  convertTraceToPprof(
      _: StateDraft,
      args: {pid: number, src: TraceSource, ts1: number, ts2?: number}): void {
    ConvertTraceToPprof(args.pid, args.src, args.ts1, args.ts2);
  },

  addTracks(state: StateDraft, args: {tracks: AddTrackArgs[]}) {
    args.tracks.forEach(track => {
      const id = track.id === undefined ? `${state.nextId++}` : track.id;
      track.id = id;
      state.tracks[id] = track as TrackState;
      if (track.trackGroup === SCROLLING_TRACK_GROUP) {
        state.scrollingTracks.push(id);
      } else if (track.trackGroup !== undefined) {
        assertExists(state.trackGroups[track.trackGroup]).tracks.push(id);
      }
    });
  },

  addTrack(state: StateDraft, args: {
    id?: string; engineId: string; kind: string; name: string;
    trackGroup?: string; config: {};
  }): void {
    const id = args.id !== undefined ? args.id : `${state.nextId++}`;
    state.tracks[id] = {
      id,
      engineId: args.engineId,
      kind: args.kind,
      name: args.name,
      trackGroup: args.trackGroup,
      config: args.config,
    };
    if (args.trackGroup === SCROLLING_TRACK_GROUP) {
      state.scrollingTracks.push(id);
    } else if (args.trackGroup !== undefined) {
      assertExists(state.trackGroups[args.trackGroup]).tracks.push(id);
    }
  },

  addTrackGroup(
      state: StateDraft,
      // Define ID in action so a track group can be referred to without running
      // the reducer.
      args: {
        engineId: string; name: string; id: string; summaryTrackId: string;
        collapsed: boolean;
      }): void {
    state.trackGroups[args.id] = {
      ...args,
      tracks: [],
    };
  },

  updateAggregateSorting(
      state: StateDraft, args: {id: string, column: string}) {
    let prefs = state.aggregatePreferences[args.id];
    if (!prefs) {
      prefs = {id: args.id};
      state.aggregatePreferences[args.id] = prefs;
    }

    if (!prefs.sorting || prefs.sorting.column !== args.column) {
      // No sorting set for current column.
      state.aggregatePreferences[args.id].sorting = {
        column: args.column,
        direction: 'DESC'
      };
    } else if (prefs.sorting.direction === 'DESC') {
      // Toggle the direction if the column is currently sorted.
      state.aggregatePreferences[args.id].sorting = {
        column: args.column,
        direction: 'ASC'
      };
    } else {
      // If direction is currently 'ASC' toggle to no sorting.
      state.aggregatePreferences[args.id].sorting = undefined;
    }
  },

  setVisibleTracks(state: StateDraft, args: {tracks: string[]}) {
    state.visibleTracks = args.tracks;
  },

  updateTrackConfig(state: StateDraft, args: {id: string, config: {}}) {
    if (state.tracks[args.id] === undefined) return;
    state.tracks[args.id].config = args.config;
  },

  executeQuery(
      state: StateDraft,
      args: {queryId: string; engineId: string; query: string}): void {
    state.queries[args.queryId] = {
      id: args.queryId,
      engineId: args.engineId,
      query: args.query,
    };
  },

  deleteQuery(state: StateDraft, args: {queryId: string}): void {
    delete state.queries[args.queryId];
  },

  moveTrack(
      state: StateDraft,
      args: {srcId: string; op: 'before' | 'after', dstId: string}): void {
    const moveWithinTrackList = (trackList: string[]) => {
      const newList: string[] = [];
      for (let i = 0; i < trackList.length; i++) {
        const curTrackId = trackList[i];
        if (curTrackId === args.dstId && args.op === 'before') {
          newList.push(args.srcId);
        }
        if (curTrackId !== args.srcId) {
          newList.push(curTrackId);
        }
        if (curTrackId === args.dstId && args.op === 'after') {
          newList.push(args.srcId);
        }
      }
      trackList.splice(0);
      newList.forEach(x => {
        trackList.push(x);
      });
    };

    moveWithinTrackList(state.pinnedTracks);
    moveWithinTrackList(state.scrollingTracks);
  },

  toggleTrackPinned(state: StateDraft, args: {trackId: string}): void {
    const id = args.trackId;
    const isPinned = state.pinnedTracks.includes(id);
    const trackGroup = assertExists(state.tracks[id]).trackGroup;

    if (isPinned) {
      state.pinnedTracks.splice(state.pinnedTracks.indexOf(id), 1);
      if (trackGroup === SCROLLING_TRACK_GROUP) {
        state.scrollingTracks.unshift(id);
      }
    } else {
      if (trackGroup === SCROLLING_TRACK_GROUP) {
        state.scrollingTracks.splice(state.scrollingTracks.indexOf(id), 1);
      }
      state.pinnedTracks.push(id);
    }
  },

  toggleTrackGroupCollapsed(state: StateDraft, args: {trackGroupId: string}):
      void {
        const id = args.trackGroupId;
        const trackGroup = assertExists(state.trackGroups[id]);
        trackGroup.collapsed = !trackGroup.collapsed;
      },

  setEngineReady(
      state: StateDraft,
      args: {engineId: string; ready: boolean, mode: EngineMode}): void {
    state.engines[args.engineId].ready = args.ready;
    state.engines[args.engineId].mode = args.mode;
  },

  setNewEngineMode(state: StateDraft, args: {mode: NewEngineMode}): void {
    state.newEngineMode = args.mode;
  },

  // Marks all engines matching the given |mode| as failed.
  setEngineFailed(state: StateDraft, args: {mode: EngineMode; failure: string}):
      void {
        for (const engine of Object.values(state.engines)) {
          if (engine.mode === args.mode) engine.failed = args.failure;
        }
      },

  createPermalink(state: StateDraft, _: {}): void {
    state.permalink = {requestId: `${state.nextId++}`, hash: undefined};
  },

  setPermalink(state: StateDraft, args: {requestId: string; hash: string}):
      void {
        // Drop any links for old requests.
        if (state.permalink.requestId !== args.requestId) return;
        state.permalink = args;
      },

  loadPermalink(state: StateDraft, args: {hash: string}): void {
    state.permalink = {
      requestId: `${state.nextId++}`,
      hash: args.hash,
    };
  },

  clearPermalink(state: StateDraft, _: {}): void {
    state.permalink = {};
  },

  setTraceTime(state: StateDraft, args: TraceTime): void {
    state.traceTime = args;
  },

  updateStatus(state: StateDraft, args: Status): void {
    state.status = args;
  },

  // TODO(hjd): Remove setState - it causes problems due to reuse of ids.
  setState(state: StateDraft, args: {newState: State}): void {
    for (const key of Object.keys(state)) {
      // tslint:disable-next-line no-any
      delete (state as any)[key];
    }
    for (const key of Object.keys(args.newState)) {
      // tslint:disable-next-line no-any
      (state as any)[key] = (args.newState as any)[key];
    }
  },

  setRecordConfig(state: StateDraft, args: {config: RecordConfig;}): void {
    state.recordConfig = args.config;
  },

  selectNote(state: StateDraft, args: {id: string}): void {
    if (args.id) {
      state.currentSelection = {
        kind: 'NOTE',
        id: args.id
      };
    }
  },

  addNote(
      state: StateDraft,
      args: {timestamp: number, color: string, isMovie: boolean}): void {
    const id = `${state.nextId++}`;
    state.notes[id] = {
      noteType: 'DEFAULT',
      id,
      timestamp: args.timestamp,
      color: args.color,
      text: '',
    };
    if (args.isMovie) {
      state.videoNoteIds.push(id);
    }
    this.selectNote(state, {id});
  },

  addAreaNote(
      state: StateDraft, args: {timestamp: number, area: Area, color: string}):
      void {
        const id = `${state.nextId++}`;
        state.notes[id] = {
          noteType: 'AREA',
          id,
          timestamp: args.timestamp,
          area: args.area,
          color: args.color,
          text: '',
        };
        this.selectNote(state, {id});
      },

  toggleVideo(state: StateDraft, _: {}): void {
    state.videoEnabled = !state.videoEnabled;
    if (!state.videoEnabled) {
      state.video = null;
      state.flagPauseEnabled = false;
      state.scrubbingEnabled = false;
      state.videoNoteIds.forEach(id => {
        this.removeNote(state, {id});
      });
    }
  },

  toggleFlagPause(state: StateDraft, _: {}): void {
    if (state.video != null) {
      state.flagPauseEnabled = !state.flagPauseEnabled;
    }
  },

  toggleScrubbing(state: StateDraft, _: {}): void {
    if (state.video != null) {
      state.scrubbingEnabled = !state.scrubbingEnabled;
    }
  },

  setVideoOffset(state: StateDraft, args: {offset: number}): void {
    state.videoOffset = args.offset;
  },

  changeNoteColor(state: StateDraft, args: {id: string, newColor: string}):
      void {
        const note = state.notes[args.id];
        if (note === undefined) return;
        note.color = args.newColor;
      },

  changeNoteText(state: StateDraft, args: {id: string, newText: string}): void {
    const note = state.notes[args.id];
    if (note === undefined) return;
    note.text = args.newText;
  },

  removeNote(state: StateDraft, args: {id: string}): void {
    if (state.notes[args.id].noteType === 'MOVIE') {
      state.videoNoteIds = state.videoNoteIds.filter(id => {
        return id !== args.id;
      });
    }
    delete state.notes[args.id];
    if (state.currentSelection === null) return;
    if (state.currentSelection.kind === 'NOTE' &&
        state.currentSelection.id === args.id) {
      state.currentSelection = null;
    }
  },

  selectSlice(state: StateDraft, args: {id: number, trackId: string}): void {
    state.currentSelection = {
      kind: 'SLICE',
      id: args.id,
      trackId: args.trackId,
    };
  },

  selectCounter(
      state: StateDraft,
      args: {leftTs: number, rightTs: number, id: number, trackId: string}):
      void {
        state.currentSelection = {
          kind: 'COUNTER',
          leftTs: args.leftTs,
          rightTs: args.rightTs,
          id: args.id,
          trackId: args.trackId,
        };
      },

  selectHeapProfile(
      state: StateDraft,
      args: {id: number, upid: number, ts: number, type: string}): void {
    state.currentSelection = {
      kind: 'HEAP_PROFILE',
      id: args.id,
      upid: args.upid,
      ts: args.ts,
      type: args.type,
    };
  },

  showHeapProfileFlamegraph(
      state: StateDraft,
      args: {id: number, upid: number, ts: number, type: string}): void {
    state.currentHeapProfileFlamegraph = {
      kind: 'HEAP_PROFILE_FLAMEGRAPH',
      id: args.id,
      upid: args.upid,
      ts: args.ts,
      type: args.type,
      viewingOption: DEFAULT_VIEWING_OPTION,
      focusRegex: '',
    };
  },

  expandHeapProfileFlamegraph(
      state: StateDraft, args: {expandedCallsite?: CallsiteInfo}): void {
    if (state.currentHeapProfileFlamegraph === null) return;
    state.currentHeapProfileFlamegraph.expandedCallsite = args.expandedCallsite;
  },

  changeViewHeapProfileFlamegraph(
      state: StateDraft,
      args: {viewingOption: HeapProfileFlamegraphViewingOption}): void {
    if (state.currentHeapProfileFlamegraph === null) return;
    state.currentHeapProfileFlamegraph.viewingOption = args.viewingOption;
  },

  changeFocusHeapProfileFlamegraph(
      state: StateDraft, args: {focusRegex: string}): void {
    if (state.currentHeapProfileFlamegraph === null) return;
    state.currentHeapProfileFlamegraph.focusRegex = args.focusRegex;
  },

  selectChromeSlice(state: StateDraft, args: {id: number, trackId: string}):
      void {
        state.currentSelection = {
          kind: 'CHROME_SLICE',
          id: args.id,
          trackId: args.trackId
        };
      },

  selectThreadState(state: StateDraft, args: {
    utid: number,
    ts: number,
    dur: number,
    state: string,
    cpu: number,
    trackId: string
  }): void {
    state.currentSelection = {
      kind: 'THREAD_STATE',
      utid: args.utid,
      ts: args.ts,
      dur: args.dur,
      state: args.state,
      cpu: args.cpu,
      trackId: args.trackId,
    };
  },

  deselect(state: StateDraft, _: {}): void {
    state.currentSelection = null;
  },

  updateLogsPagination(state: StateDraft, args: LogsPagination): void {
    state.logsPagination = args;
  },

  startRecording(state: StateDraft, _: {}): void {
    state.recordingInProgress = true;
    state.lastRecordingError = undefined;
    state.recordingCancelled = false;
  },

  stopRecording(state: StateDraft, _: {}): void {
    state.recordingInProgress = false;
  },

  cancelRecording(state: StateDraft, _: {}): void {
    state.recordingInProgress = false;
    state.recordingCancelled = true;
  },

  setExtensionAvailable(state: StateDraft, args: {available: boolean}): void {
    state.extensionInstalled = args.available;
  },

  updateBufferUsage(state: StateDraft, args: {percentage: number}): void {
    state.bufferUsage = args.percentage;
  },

  setRecordingTarget(state: StateDraft, args: {target: RecordingTarget}): void {
    state.recordingTarget = args.target;
  },

  setAvailableAdbDevices(
      state: StateDraft, args: {devices: AdbRecordingTarget[]}): void {
    state.availableAdbDevices = args.devices;
  },

  setOmnibox(state: StateDraft, args: OmniboxState): void {
    state.frontendLocalState.omniboxState = args;
  },

  selectArea(state: StateDraft, args: TimestampedAreaSelection): void {
    state.frontendLocalState.selectedArea = args;
  },

  setVisibleTraceTime(state: StateDraft, args: VisibleState): void {
    state.frontendLocalState.visibleState = args;
  },

  setChromeCategories(state: StateDraft, args: {categories: string[]}): void {
    state.chromeCategories = args.categories;
  },

  setLastRecordingError(state: StateDraft, args: {error?: string}): void {
    state.lastRecordingError = args.error;
    state.recordingStatus = undefined;
  },

  setRecordingStatus(state: StateDraft, args: {status?: string}): void {
    state.recordingStatus = args.status;
    state.lastRecordingError = undefined;
  },
};

// When we are on the frontend side, we don't really want to execute the
// actions above, we just want to serialize them and marshal their
// arguments, send them over to the controller side and have them being
// executed there. The magic below takes care of turning each action into a
// function that returns the marshaled args.

// A DeferredAction is a bundle of Args and a method name. This is the marshaled
// version of a StateActions method call.
export interface DeferredAction<Args = {}> {
  type: string;
  args: Args;
}

// This type magic creates a type function DeferredActions<T> which takes a type
// T and 'maps' its attributes. For each attribute on T matching the signature:
// (state: StateDraft, args: Args) => void
// DeferredActions<T> has an attribute:
// (args: Args) => DeferredAction<Args>
type ActionFunction<Args> = (state: StateDraft, args: Args) => void;
type DeferredActionFunc<T> = T extends ActionFunction<infer Args>?
    (args: Args) => DeferredAction<Args>:
    never;
type DeferredActions<C> = {
  [P in keyof C]: DeferredActionFunc<C[P]>;
};

// Actions is an implementation of DeferredActions<typeof StateActions>.
// (since StateActions is a variable not a type we have to do
// 'typeof StateActions' to access the (unnamed) type of StateActions).
// It's a Proxy such that any attribute access returns a function:
// (args) => {return {type: ATTRIBUTE_NAME, args};}
export const Actions =
    // tslint:disable-next-line no-any
    new Proxy<DeferredActions<typeof StateActions>>({} as any, {
      // tslint:disable-next-line no-any
      get(_: any, prop: string, _2: any) {
        return (args: {}): DeferredAction<{}> => {
          return {
            type: prop,
            args,
          };
        };
      },
    });
