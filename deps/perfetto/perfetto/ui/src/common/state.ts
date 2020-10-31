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

/**
 * A plain js object, holding objects of type |Class| keyed by string id.
 * We use this instead of using |Map| object since it is simpler and faster to
 * serialize for use in postMessage.
 */
export interface ObjectById<Class extends{id: string}> { [id: string]: Class; }

export type Timestamped<T> = {
  [P in keyof T]: T[P];
}&{lastUpdate: number};

export type OmniboxState =
    Timestamped<{omnibox: string; mode: 'SEARCH' | 'COMMAND'}>;

export type VisibleState =
    Timestamped<{startSec: number; endSec: number; resolution: number;}>;

export type TimestampedAreaSelection = Timestamped<AreaSelection>;
export interface AreaSelection {
  area?: Area;
}
export interface Area {
  startSec: number;
  endSec: number;
  tracks: string[];
}

export const MAX_TIME = 180;

export const SCROLLING_TRACK_GROUP = 'ScrollingTracks';


export type EngineMode = 'WASM'|'HTTP_RPC';

export type NewEngineMode = 'USE_HTTP_RPC_IF_AVAILABLE'|'FORCE_BUILTIN_WASM';

export type HeapProfileFlamegraphViewingOption =
    'SPACE'|'ALLOC_SPACE'|'OBJECTS'|'ALLOC_OBJECTS';

export interface CallsiteInfo {
  id: number;
  parentId: number;
  depth: number;
  name?: string;
  totalSize: number;
  selfSize: number;
  mapping: string;
  merged: boolean;
}

export interface TraceFileSource {
  type: 'FILE';
  file: File;
}

export interface TraceArrayBufferSource {
  type: 'ARRAY_BUFFER';
  buffer: ArrayBuffer;
}

export interface TraceUrlSource {
  type: 'URL';
  url: string;
}

export interface TraceHttpRpcSource {
  type: 'HTTP_RPC';
}

export type TraceSource =
    TraceFileSource|TraceArrayBufferSource|TraceUrlSource|TraceHttpRpcSource;

export interface TrackState {
  id: string;
  engineId: string;
  kind: string;
  name: string;
  trackGroup?: string;
  config: {};
}

export interface TrackGroupState {
  id: string;
  engineId: string;
  name: string;
  collapsed: boolean;
  tracks: string[];  // Child track ids.
  summaryTrackId: string;
}

export interface EngineConfig {
  id: string;
  mode?: EngineMode;  // Is undefined until |ready| is true.
  ready: boolean;
  failed?: string;  // If defined the engine has crashed with the given message.
  source: TraceSource;
}

export interface QueryConfig {
  id: string;
  engineId: string;
  query: string;
}

export interface PermalinkConfig {
  requestId?: string;  // Set by the frontend to request a new permalink.
  hash?: string;       // Set by the controller when the link has been created.
}

export interface TraceTime {
  startSec: number;
  endSec: number;
}

export interface FrontendLocalState {
  omniboxState: OmniboxState;
  visibleState: VisibleState;
  selectedArea: TimestampedAreaSelection;
}

export interface Status {
  msg: string;
  timestamp: number;  // Epoch in seconds (Date.now() / 1000).
}

export interface Note {
  noteType: 'DEFAULT'|'MOVIE';
  id: string;
  timestamp: number;
  color: string;
  text: string;
}

export interface AreaNote {
  noteType: 'AREA';
  id: string;
  timestamp: number;
  area: Area;
  color: string;
  text: string;
}

export interface NoteSelection {
  kind: 'NOTE';
  id: string;
}

export interface SliceSelection {
  kind: 'SLICE';
  id: number;
}

export interface CounterSelection {
  kind: 'COUNTER';
  leftTs: number;
  rightTs: number;
  id: number;
}

export interface HeapProfileSelection {
  kind: 'HEAP_PROFILE';
  id: number;
  upid: number;
  ts: number;
  type: string;
}

export interface HeapProfileFlamegraph {
  kind: 'HEAP_PROFILE_FLAMEGRAPH';
  id: number;
  upid: number;
  ts: number;
  type: string;
  viewingOption: HeapProfileFlamegraphViewingOption;
  focusRegex: string;
  expandedCallsite?: CallsiteInfo;
}

export interface ChromeSliceSelection {
  kind: 'CHROME_SLICE';
  id: number;
}

export interface ThreadStateSelection {
  kind: 'THREAD_STATE';
  utid: number;
  ts: number;
  dur: number;
  state: string;
  cpu: number;
}

type Selection =
    (NoteSelection|SliceSelection|CounterSelection|HeapProfileSelection|
     ChromeSliceSelection|ThreadStateSelection)&{trackId?: string};

export interface LogsPagination {
  offset: number;
  count: number;
}

export interface RecordingTarget {
  name: string;
  os: TargetOs;
}

export interface AdbRecordingTarget extends RecordingTarget {
  serial: string;
}

export interface Sorting {
  column: string;
  direction: 'DESC'|'ASC';
}

export interface AggregationState {
  id: string;
  sorting?: Sorting;
}

export interface State {
  // tslint:disable-next-line:no-any
  [key: string]: any;
  route: string|null;
  nextId: number;

  /**
   * State of the ConfigEditor.
   */
  recordConfig: RecordConfig;
  displayConfigAsPbtxt: boolean;

  /**
   * Open traces.
   */
  newEngineMode: NewEngineMode;
  engines: ObjectById<EngineConfig>;
  traceTime: TraceTime;
  trackGroups: ObjectById<TrackGroupState>;
  tracks: ObjectById<TrackState>;
  aggregatePreferences: ObjectById<AggregationState>;
  visibleTracks: string[];
  scrollingTracks: string[];
  pinnedTracks: string[];
  queries: ObjectById<QueryConfig>;
  permalink: PermalinkConfig;
  notes: ObjectById<Note|AreaNote>;
  status: Status;
  currentSelection: Selection|null;
  currentHeapProfileFlamegraph: HeapProfileFlamegraph|null;
  logsPagination: LogsPagination;

  /**
   * This state is updated on the frontend at 60Hz and eventually syncronised to
   * the controller at 10Hz. When the controller sends state updates to the
   * frontend the frontend has special logic to pick whichever version of this
   * key is most up to date.
   */
  frontendLocalState: FrontendLocalState;

  video: string | null;
  videoEnabled: boolean;
  videoOffset: number;
  videoNoteIds: string[];
  scrubbingEnabled: boolean;
  flagPauseEnabled: boolean;

  /**
   * Trace recording
   */
  recordingInProgress: boolean;
  recordingCancelled: boolean;
  extensionInstalled: boolean;
  recordingTarget: RecordingTarget;
  availableAdbDevices: AdbRecordingTarget[];
  lastRecordingError?: string;
  recordingStatus?: string;

  chromeCategories: string[]|undefined;
}

export const defaultTraceTime = {
  startSec: 0,
  endSec: 10,
};

export declare type RecordMode =
    'STOP_WHEN_FULL' | 'RING_BUFFER' | 'LONG_TRACE';

// 'Q','P','O' for Android, 'L' for Linux, 'C' for Chrome.
export declare type TargetOs = 'Q' | 'P' | 'O' | 'C' | 'L';

export function isAndroidTarget(target: RecordingTarget) {
  return ['Q', 'P', 'O'].includes(target.os);
}

export function isChromeTarget(target: RecordingTarget) {
  return target.os === 'C';
}

export function isLinuxTarget(target: RecordingTarget) {
  return target.os === 'L';
}

export function isAdbTarget(target: RecordingTarget):
    target is AdbRecordingTarget {
  if ((target as AdbRecordingTarget).serial) return true;
  return false;
}

export interface RecordConfig {
  [key: string]: null|number|boolean|string|string[];

  // Global settings
  mode: RecordMode;
  durationMs: number;
  bufferSizeMb: number;
  maxFileSizeMb: number;      // Only for mode == 'LONG_TRACE'.
  fileWritePeriodMs: number;  // Only for mode == 'LONG_TRACE'.

  cpuSched: boolean;
  cpuLatency: boolean;
  cpuFreq: boolean;
  cpuCoarse: boolean;
  cpuCoarsePollMs: number;
  cpuSyscall: boolean;

  screenRecord: boolean;

  gpuFreq: boolean;

  ftrace: boolean;
  atrace: boolean;
  ftraceEvents: string[];
  ftraceExtraEvents: string;
  atraceCats: string[];
  atraceApps: string;
  ftraceBufferSizeKb: number;
  ftraceDrainPeriodMs: number;
  androidLogs: boolean;
  androidLogBuffers: string[];

  batteryDrain: boolean;
  batteryDrainPollMs: number;

  boardSensors: boolean;

  memHiFreq: boolean;
  memLmk: boolean;
  meminfo: boolean;
  meminfoPeriodMs: number;
  meminfoCounters: string[];
  vmstat: boolean;
  vmstatPeriodMs: number;
  vmstatCounters: string[];

  heapProfiling: boolean;
  hpSamplingIntervalBytes: number;
  hpProcesses: string;
  hpContinuousDumpsPhase: number;
  hpContinuousDumpsInterval: number;
  hpSharedMemoryBuffer: number;

  javaHeapDump: boolean;
  jpProcesses: string;
  jpContinuousDumpsPhase: number;
  jpContinuousDumpsInterval: number;

  procStats: boolean;
  procStatsPeriodMs: number;

  chromeCategoriesSelected: string[];
}

export function createEmptyRecordConfig(): RecordConfig {
  return {
    mode: 'STOP_WHEN_FULL',
    durationMs: 10000.0,
    maxFileSizeMb: 100,
    fileWritePeriodMs: 2500,
    bufferSizeMb: 10.0,

    cpuSched: false,
    cpuLatency: false,
    cpuFreq: false,
    cpuSyscall: false,

    screenRecord: false,

    gpuFreq: false,

    ftrace: false,
    atrace: false,
    ftraceEvents: [],
    ftraceExtraEvents: '',
    atraceCats: [],
    atraceApps: '',
    ftraceBufferSizeKb: 2 * 1024,
    ftraceDrainPeriodMs: 250,
    androidLogs: false,
    androidLogBuffers: [],

    cpuCoarse: false,
    cpuCoarsePollMs: 1000,

    batteryDrain: false,
    batteryDrainPollMs: 1000,

    boardSensors: false,

    memHiFreq: false,
    meminfo: false,
    meminfoPeriodMs: 1000,
    meminfoCounters: [],

    vmstat: false,
    vmstatPeriodMs: 1000,
    vmstatCounters: [],

    heapProfiling: false,
    hpSamplingIntervalBytes: 4096,
    hpProcesses: '',
    hpContinuousDumpsPhase: 0,
    hpContinuousDumpsInterval: 0,
    hpSharedMemoryBuffer: 8 * 1048576,

    javaHeapDump: false,
    jpProcesses: '',
    jpContinuousDumpsPhase: 0,
    jpContinuousDumpsInterval: 0,

    memLmk: false,
    procStats: false,
    procStatsPeriodMs: 1000,

    chromeCategoriesSelected: [],
  };
}

export function getDefaultRecordingTargets(): RecordingTarget[] {
  return [
    {os: 'Q', name: 'Android Q+'},
    {os: 'P', name: 'Android P'},
    {os: 'O', name: 'Android O-'},
    {os: 'C', name: 'Chrome'},
    {os: 'L', name: 'Linux desktop'}
  ];
}

export function getBuiltinChromeCategoryList(): string[] {
  // List of static Chrome categories, last updated at Chromium 81.0.4021.0 from
  // Chromium's //base/trace_event/builtin_categories.h.
  return [
    'accessibility',
    'AccountFetcherService',
    'android_webview',
    'audio',
    'base',
    'benchmark',
    'blink',
    'blink.animations',
    'blink.console',
    'blink_gc',
    'blink.net',
    'blink_style',
    'blink.user_timing',
    'blink.worker',
    'Blob',
    'browser',
    'browsing_data',
    'CacheStorage',
    'camera',
    'cast_perf_test',
    'cast.stream',
    'cc',
    'cc.debug',
    'cdp.perf',
    'chromeos',
    'cma',
    'compositor',
    'content',
    'content_capture',
    'devtools',
    'devtools.timeline',
    'devtools.timeline.async',
    'download',
    'download_service',
    'drm',
    'drmcursor',
    'dwrite',
    'DXVA Decoding',
    'EarlyJava',
    'evdev',
    'event',
    'exo',
    'explore_sites',
    'FileSystem',
    'file_system_provider',
    'fonts',
    'GAMEPAD',
    'gpu',
    'gpu.capture',
    'headless',
    'hwoverlays',
    'identity',
    'IndexedDB',
    'input',
    'io',
    'ipc',
    'Java',
    'jni',
    'jpeg',
    'latency',
    'latencyInfo',
    'leveldb',
    'loading',
    'log',
    'login',
    'media',
    'media_router',
    'memory',
    'midi',
    'mojom',
    'mus',
    'native',
    'navigation',
    'net',
    'netlog',
    'offline_pages',
    'omnibox',
    'oobe',
    'ozone',
    'passwords',
    'p2p',
    'page-serialization',
    'pepper',
    'ppapi',
    'ppapi proxy',
    'rail',
    'renderer',
    'renderer_host',
    'renderer.scheduler',
    'RLZ',
    'safe_browsing',
    'screenlock_monitor',
    'sequence_manager',
    'service_manager',
    'ServiceWorker',
    'shell',
    'shortcut_viewer',
    'shutdown',
    'SiteEngagement',
    'skia',
    'startup',
    'sync',
    'sync_lock_contention',
    'thread_pool',
    'test_gpu',
    'test_tracing',
    'toplevel',
    'ui',
    'v8',
    'v8.execute',
    'ValueStoreFrontend::Backend',
    'views',
    'views.frame',
    'viz',
    'vk',
    'wayland',
    'webaudio',
    'weblayer',
    'WebCore',
    'webrtc',
    'xr',
    'disabled-by-default-animation-worklet',
    'disabled-by-default-audio-worklet',
    'disabled-by-default-blink.debug',
    'disabled-by-default-blink.debug.display_lock',
    'disabled-by-default-blink.debug.layout',
    'disabled-by-default-blink.debug.layout.trees',
    'disabled-by-default-blink.feature_usage',
    'disabled-by-default-blink_gc',
    'disabled-by-default-blink.image_decoding',
    'disabled-by-default-blink.invalidation',
    'disabled-by-default-cc',
    'disabled-by-default-cc.debug',
    'disabled-by-default-cc.debug.cdp-perf',
    'disabled-by-default-cc.debug.display_items',
    'disabled-by-default-cc.debug.picture',
    'disabled-by-default-cc.debug.scheduler',
    'disabled-by-default-cc.debug.scheduler.frames',
    'disabled-by-default-cc.debug.scheduler.now',
    'disabled-by-default-cpu_profiler',
    'disabled-by-default-cpu_profiler.debug',
    'disabled-by-default-devtools.screenshot',
    'disabled-by-default-devtools.timeline',
    'disabled-by-default-devtools.timeline.frame',
    'disabled-by-default-devtools.timeline.inputs',
    'disabled-by-default-devtools.timeline.invalidationTracking',
    'disabled-by-default-devtools.timeline.layers',
    'disabled-by-default-devtools.timeline.picture',
    'disabled-by-default-file',
    'disabled-by-default-fonts',
    'disabled-by-default-gpu_cmd_queue',
    'disabled-by-default-gpu.dawn',
    'disabled-by-default-gpu.debug',
    'disabled-by-default-gpu_decoder',
    'disabled-by-default-gpu.device',
    'disabled-by-default-gpu.service',
    'disabled-by-default-histogram_samples',
    'disabled-by-default-ipc.flow',
    'disabled-by-default-java-heap-profiler',
    'disabled-by-default-layer-element',
    'disabled-by-default-lifecycles',
    'disabled-by-default-loading',
    'disabled-by-default-memory-infra',
    'disabled-by-default-memory-infra.v8.code_stats',
    'disabled-by-default-net',
    'disabled-by-default-network',
    'disabled-by-default-paint-worklet',
    'disabled-by-default-power',
    'disabled-by-default-renderer.scheduler',
    'disabled-by-default-renderer.scheduler.debug',
    'disabled-by-default-sequence_manager',
    'disabled-by-default-sequence_manager.debug',
    'disabled-by-default-sequence_manager.verbose_snapshots',
    'disabled-by-default-skia',
    'disabled-by-default-skia.gpu',
    'disabled-by-default-skia.gpu.cache',
    'disabled-by-default-SyncFileSystem',
    'disabled-by-default-system_stats',
    'disabled-by-default-thread_pool_diagnostics',
    'disabled-by-default-toplevel.flow',
    'disabled-by-default-toplevel.ipc',
    'disabled-by-default-v8.compile',
    'disabled-by-default-v8.cpu_profiler',
    'disabled-by-default-v8.cpu_profiler.hires',
    'disabled-by-default-v8.gc',
    'disabled-by-default-v8.gc_stats',
    'disabled-by-default-v8.ic_stats',
    'disabled-by-default-v8.runtime',
    'disabled-by-default-v8.runtime_stats',
    'disabled-by-default-v8.runtime_stats_sampling',
    'disabled-by-default-v8.turbofan',
    'disabled-by-default-v8.wasm',
    'disabled-by-default-video_and_image_capture',
    'disabled-by-default-viz.debug.overlay_planes',
    'disabled-by-default-viz.hit_testing_flow',
    'disabled-by-default-viz.overdraw',
    'disabled-by-default-viz.quads',
    'disabled-by-default-viz.surface_id_flow',
    'disabled-by-default-viz.surface_lifetime',
    'disabled-by-default-viz.triangles',
    'disabled-by-default-worker.scheduler',
  ];
}

export function createEmptyState(): State {
  return {
    route: null,
    nextId: 0,
    newEngineMode: 'USE_HTTP_RPC_IF_AVAILABLE',
    engines: {},
    traceTime: {...defaultTraceTime},
    tracks: {},
    aggregatePreferences: {},
    trackGroups: {},
    visibleTracks: [],
    pinnedTracks: [],
    scrollingTracks: [],
    queries: {},
    permalink: {},
    notes: {},

    recordConfig: createEmptyRecordConfig(),
    displayConfigAsPbtxt: false,

    frontendLocalState: {
      omniboxState: {
        lastUpdate: 0,
        omnibox: '',
        mode: 'SEARCH',
      },

      visibleState: {
        ...defaultTraceTime,
        lastUpdate: 0,
        resolution: 0,
      },
      selectedArea: {
        lastUpdate: 0,
      }
    },

    logsPagination: {
      offset: 0,
      count: 0,
    },

    status: {msg: '', timestamp: 0},
    currentSelection: null,
    currentHeapProfileFlamegraph: null,

    video: null,
    videoEnabled: false,
    videoOffset: 0,
    videoNoteIds: [],
    scrubbingEnabled: false,
    flagPauseEnabled: false,
    recordingInProgress: false,
    recordingCancelled: false,
    extensionInstalled: false,
    recordingTarget: getDefaultRecordingTargets()[0],
    availableAdbDevices: [],

    chromeCategories: undefined,
  };
}

export function getContainingTrackId(state: State, trackId: string): null|
    string {
  const track = state.tracks[trackId];
  if (!track) {
    return null;
  }
  const parentId = track.trackGroup;
  if (!parentId) {
    return null;
  }
  return parentId;
}
