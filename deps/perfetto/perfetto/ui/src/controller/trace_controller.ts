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

import '../tracks/all_controller';

import * as uuidv4 from 'uuid/v4';

import {assertExists, assertTrue} from '../base/logging';
import {
  Actions,
  AddTrackArgs,
  DeferredAction,
} from '../common/actions';
import {Engine} from '../common/engine';
import {HttpRpcEngine} from '../common/http_rpc_engine';
import {NUM, NUM_NULL, rawQueryToRows, STR_NULL} from '../common/protos';
import {
  EngineMode,
  SCROLLING_TRACK_GROUP,
} from '../common/state';
import {toNs, toNsCeil, toNsFloor} from '../common/time';
import {TimeSpan} from '../common/time';
import {
  createWasmEngine,
  destroyWasmEngine,
  WasmEngineProxy
} from '../common/wasm_engine_proxy';
import {QuantizedLoad, ThreadDesc} from '../frontend/globals';
import {ANDROID_LOGS_TRACK_KIND} from '../tracks/android_log/common';
import {SLICE_TRACK_KIND} from '../tracks/chrome_slices/common';
import {CPU_FREQ_TRACK_KIND} from '../tracks/cpu_freq/common';
import {CPU_SLICE_TRACK_KIND} from '../tracks/cpu_slices/common';
import {GPU_FREQ_TRACK_KIND} from '../tracks/gpu_freq/common';
import {HEAP_PROFILE_TRACK_KIND} from '../tracks/heap_profile/common';
import {
  PROCESS_SCHEDULING_TRACK_KIND
} from '../tracks/process_scheduling/common';
import {PROCESS_SUMMARY_TRACK} from '../tracks/process_summary/common';
import {THREAD_STATE_TRACK_KIND} from '../tracks/thread_state/common';

import {
  CpuAggregationController
} from './aggregation/cpu_aggregation_controller';
import {
  ThreadAggregationController
} from './aggregation/thread_aggregation_controller';
import {Child, Children, Controller} from './controller';
import {globals} from './globals';
import {
  HeapProfileController,
  HeapProfileControllerArgs
} from './heap_profile_controller';
import {LoadingManager} from './loading_manager';
import {LogsController} from './logs_controller';
import {QueryController, QueryControllerArgs} from './query_controller';
import {SearchController} from './search_controller';
import {
  SelectionController,
  SelectionControllerArgs
} from './selection_controller';
import {
  TraceBufferStream,
  TraceFileStream,
  TraceHttpStream,
  TraceStream
} from './trace_stream';
import {TrackControllerArgs, trackControllerRegistry} from './track_controller';

type States = 'init'|'loading_trace'|'ready';

interface ThreadSliceTrack {
  maxDepth: number;
  trackId: number;
}

// TraceController handles handshakes with the frontend for everything that
// concerns a single trace. It owns the WASM trace processor engine, handles
// tracks data and SQL queries. There is one TraceController instance for each
// trace opened in the UI (for now only one trace is supported).
export class TraceController extends Controller<States> {
  private readonly engineId: string;
  private engine?: Engine;

  constructor(engineId: string) {
    super('init');
    this.engineId = engineId;
  }

  onDestroy() {
    if (this.engine instanceof WasmEngineProxy) {
      destroyWasmEngine(this.engine.id);
    }
  }

  run() {
    const engineCfg = assertExists(globals.state.engines[this.engineId]);
    switch (this.state) {
      case 'init':
        this.loadTrace()
            .then(mode => {
              globals.dispatch(Actions.setEngineReady({
                engineId: this.engineId,
                ready: true,
                mode,
              }));
            })
            .catch(err => {
              this.updateStatus(`${err}`);
              throw err;
            });
        this.updateStatus('Opening trace');
        this.setState('loading_trace');
        break;

      case 'loading_trace':
        // Stay in this state until loadTrace() returns and marks the engine as
        // ready.
        if (this.engine === undefined || !engineCfg.ready) return;
        this.setState('ready');
        break;

      case 'ready':
        // At this point we are ready to serve queries and handle tracks.
        const engine = assertExists(this.engine);
        assertTrue(engineCfg.ready);
        const childControllers: Children = [];

        // Create a TrackController for each track.
        for (const trackId of Object.keys(globals.state.tracks)) {
          const trackCfg = globals.state.tracks[trackId];
          if (trackCfg.engineId !== this.engineId) continue;
          if (!trackControllerRegistry.has(trackCfg.kind)) continue;
          const trackCtlFactory = trackControllerRegistry.get(trackCfg.kind);
          const trackArgs: TrackControllerArgs = {trackId, engine};
          childControllers.push(Child(trackId, trackCtlFactory, trackArgs));
        }

        // Create a QueryController for each query.
        for (const queryId of Object.keys(globals.state.queries)) {
          const queryArgs: QueryControllerArgs = {queryId, engine};
          childControllers.push(Child(queryId, QueryController, queryArgs));
        }

        const selectionArgs: SelectionControllerArgs = {engine};
        childControllers.push(
          Child('selection', SelectionController, selectionArgs));

        const heapProfileArgs: HeapProfileControllerArgs = {engine};
        childControllers.push(
            Child('heapProfile', HeapProfileController, heapProfileArgs));
        childControllers.push(Child(
            'cpu_aggregation',
            CpuAggregationController,
            {engine, kind: 'cpu_aggregation'}));
        childControllers.push(Child(
            'thread_aggregation',
            ThreadAggregationController,
            {engine, kind: 'thread_state_aggregation'}));
        childControllers.push(Child('search', SearchController, {
          engine,
          app: globals,
        }));

        childControllers.push(Child('logs', LogsController, {
          engine,
          app: globals,
        }));

        return childControllers;

      default:
        throw new Error(`unknown state ${this.state}`);
    }
    return;
  }

  private async loadTrace(): Promise<EngineMode> {
    this.updateStatus('Creating trace processor');
    // Check if there is any instance of the trace_processor_shell running in
    // HTTP RPC mode (i.e. trace_processor_shell -D).
    let engineMode: EngineMode;
    let useRpc = false;
    if (globals.state.newEngineMode === 'USE_HTTP_RPC_IF_AVAILABLE') {
      useRpc = (await HttpRpcEngine.checkConnection()).connected;
    }
    if (useRpc) {
      console.log('Opening trace using native accelerator over HTTP+RPC');
      engineMode = 'HTTP_RPC';
      const engine =
          new HttpRpcEngine(this.engineId, LoadingManager.getInstance);
      engine.errorHandler = (err) => {
        globals.dispatch(
            Actions.setEngineFailed({mode: 'HTTP_RPC', failure: `${err}`}));
        throw err;
      };
      this.engine = engine;
    } else {
      console.log('Opening trace using built-in WASM engine');
      engineMode = 'WASM';
      this.engine = new WasmEngineProxy(
          this.engineId,
          createWasmEngine(this.engineId),
          LoadingManager.getInstance);
    }

    globals.dispatch(Actions.setEngineReady({
      engineId: this.engineId,
      ready: false,
      mode: engineMode,
    }));
    const engineCfg = assertExists(globals.state.engines[this.engineId]);
    let traceStream: TraceStream|undefined;
    if (engineCfg.source.type === 'FILE') {
      traceStream = new TraceFileStream(engineCfg.source.file);
    } else if (engineCfg.source.type === 'ARRAY_BUFFER') {
      traceStream = new TraceBufferStream(engineCfg.source.buffer);
    } else if (engineCfg.source.type === 'URL') {
      traceStream = new TraceHttpStream(engineCfg.source.url);
    } else if (engineCfg.source.type === 'HTTP_RPC') {
      traceStream = undefined;
    } else {
      throw new Error(`Unknown source: ${JSON.stringify(engineCfg.source)}`);
    }

    // |traceStream| can be undefined in the case when we are using the external
    // HTTP+RPC endpoint and the trace processor instance has already loaded
    // a trace (because it was passed as a cmdline argument to
    // trace_processor_shell). In this case we don't want the UI to load any
    // file/stream and we just want to jump to the loading phase.
    if (traceStream !== undefined) {
      const tStart = performance.now();
      for (;;) {
        const res = await traceStream.readChunk();
        await this.engine.parse(res.data);
        const elapsed = (performance.now() - tStart) / 1000;
        let status = 'Loading trace ';
        if (res.bytesTotal > 0) {
          const progress = Math.round(res.bytesRead / res.bytesTotal * 100);
          status += `${progress}%`;
        } else {
          status += `${Math.round(res.bytesRead / 1e6)} MB`;
        }
        status += ` - ${Math.ceil(res.bytesRead / elapsed / 1e6)} MB/s`;
        this.updateStatus(status);
        if (res.eof) break;
      }
      await this.engine.notifyEof();
    } else {
      assertTrue(this.engine instanceof HttpRpcEngine);
      await this.engine.restoreInitialTables();
    }

    const traceTime = await this.engine.getTraceTimeBounds();
    const traceTimeState = {
      startSec: traceTime.start,
      endSec: traceTime.end,
    };
    const actions: DeferredAction[] = [
      Actions.setTraceTime(traceTimeState),
      Actions.navigate({route: '/viewer'}),
    ];

    // We don't know the resolution at this point. However this will be
    // replaced in 50ms so a guess is fine.
    const resolution = (traceTime.end - traceTime.start) / 1000;
    actions.push(Actions.setVisibleTraceTime(
        {...traceTimeState, lastUpdate: Date.now() / 1000, resolution}));

    globals.dispatchMultiple(actions);

    // Make sure the helper views are available before we start adding tracks.
    await this.initialiseHelperViews();

    {
      // When we reload from a permalink don't create extra tracks:
      const {pinnedTracks, tracks} = globals.state;
      if (!pinnedTracks.length && !Object.keys(tracks).length) {
        await this.listTracks();
      }
    }

    await this.listThreads();
    await this.loadTimelineOverview(traceTime);
    return engineMode;
  }

  private async listTracks() {
    this.updateStatus('Loading tracks');

    const engine = assertExists<Engine>(this.engine);
    const numGpus = await engine.getNumberOfGpus();
    const tracksToAdd: AddTrackArgs[] = [];

    // TODO(hjd): Renable Vsync tracks when fixed.
    //// TODO(hjd): Move this code out of TraceController.
    // for (const counterName of ['VSYNC-sf', 'VSYNC-app']) {
    //  const hasVsync =
    //      !!(await engine.query(
    //             `select ts from counters where name like "${
    //                                                         counterName
    //                                                       }" limit 1`))
    //            .numRecords;
    //  if (!hasVsync) continue;
    //  addToTrackActions.push(Actions.addTrack({
    //    engineId: this.engineId,
    //    kind: 'VsyncTrack',
    //    name: `${counterName}`,
    //    config: {
    //      counterName,
    //    }
    //  }));
    //}
    const maxCpuFreq = await engine.query(`
      select max(value)
      from counter c
      inner join cpu_counter_track t on c.track_id = t.id
      where name = 'cpufreq';
    `);

    const cpus = await engine.getCpus();

    for (const cpu of cpus) {
      tracksToAdd.push({
        engineId: this.engineId,
        kind: CPU_SLICE_TRACK_KIND,
        name: `Cpu ${cpu}`,
        trackGroup: SCROLLING_TRACK_GROUP,
        config: {
          cpu,
        }
      });
    }

    for (const cpu of cpus) {
      // Only add a cpu freq track if we have
      // cpu freq data.
      // TODO(taylori): Find a way to display cpu idle
      // events even if there are no cpu freq events.
      const cpuFreqIdle = await engine.query(`
        select
          id as cpu_freq_id,
          (
            select id
            from cpu_counter_track
            where name = 'cpuidle'
            and cpu = ${cpu}
            limit 1
          ) as cpu_idle_id
        from cpu_counter_track
        where name = 'cpufreq' and cpu = ${cpu}
        limit 1;
      `);
      if (cpuFreqIdle.numRecords > 0) {
        const freqTrackId = +cpuFreqIdle.columns[0].longValues![0];

        const idleTrackExists: boolean = !cpuFreqIdle.columns[1].isNulls![0];
        const idleTrackId = idleTrackExists ?
            +cpuFreqIdle.columns[1].longValues![0] :
            undefined;

        tracksToAdd.push({
          engineId: this.engineId,
          kind: CPU_FREQ_TRACK_KIND,
          name: `Cpu ${cpu} Frequency`,
          trackGroup: SCROLLING_TRACK_GROUP,
          config: {
            cpu,
            maximumValue: +maxCpuFreq.columns[0].doubleValues![0],
            freqTrackId,
            idleTrackId,
          }
        });
      }
    }

    const upidToProcessTracks = new Map();
    const rawProcessTracks = await engine.query(`
      SELECT
        pt.upid,
        pt.name,
        pt.track_ids,
        MAX(experimental_slice_layout.layout_depth) as max_depth
      FROM (
        SELECT upid, name, GROUP_CONCAT(process_track.id) AS track_ids
        FROM process_track GROUP BY upid, name
      ) AS pt CROSS JOIN experimental_slice_layout
      WHERE pt.track_ids = experimental_slice_layout.filter_track_ids
      GROUP BY pt.track_ids;
    `);
    for (let i = 0; i < rawProcessTracks.numRecords; i++) {
      const upid = +rawProcessTracks.columns[0].longValues![i];
      const name = rawProcessTracks.columns[1].stringValues![i];
      const rawTrackIds = rawProcessTracks.columns[2].stringValues![i];
      const trackIds = rawTrackIds.split(',').map(v => Number(v));
      const maxDepth = +rawProcessTracks.columns[3].longValues![i];
      const track = {
        engineId: this.engineId,
        kind: 'AsyncSliceTrack',
        name,
        config: {
          maxDepth,
          trackIds,
        },
      };
      const tracks = upidToProcessTracks.get(upid);
      if (tracks) {
        tracks.push(track);
      } else {
        upidToProcessTracks.set(upid, [track]);
      }
    }

    const heapProfiles = await engine.query(`
      select distinct(upid) from heap_profile_allocation
      union
      select distinct(upid) from heap_graph_object`);

    const heapUpids: Set<number> = new Set();
    for (let i = 0; i < heapProfiles.numRecords; i++) {
      const upid = heapProfiles.columns[0].longValues![i];
      heapUpids.add(+upid);
    }

    const maxGpuFreq = await engine.query(`
      select max(value)
      from counter c
      inner join gpu_counter_track t on c.track_id = t.id
      where name = 'gpufreq';
    `);

    for (let gpu = 0; gpu < numGpus; gpu++) {
      // Only add a gpu freq track if we have
      // gpu freq data.
      const freqExists = await engine.query(`
        select id
        from gpu_counter_track
        where name = 'gpufreq' and gpu_id = ${gpu}
        limit 1;
      `);
      if (freqExists.numRecords > 0) {
        tracksToAdd.push({
          engineId: this.engineId,
          kind: GPU_FREQ_TRACK_KIND,
          name: `Gpu ${gpu} Frequency`,
          trackGroup: SCROLLING_TRACK_GROUP,
          config: {
            gpu,
            trackId: +freqExists.columns[0].longValues![0],
            maximumValue: +maxGpuFreq.columns[0].doubleValues![0],
          }
        });
      }
    }

    // Add global or GPU counter tracks that are not bound to any pid/tid.
    const globalCounters = await engine.query(`
      select name, id
      from counter_track
      where type = 'counter_track'
      union
      select name, id
      from gpu_counter_track
      where name != 'gpufreq'
    `);
    for (let i = 0; i < globalCounters.numRecords; i++) {
      const name = globalCounters.columns[0].stringValues![i];
      const trackId = +globalCounters.columns[1].longValues![i];
      tracksToAdd.push({
        engineId: this.engineId,
        kind: 'CounterTrack',
        name,
        trackGroup: SCROLLING_TRACK_GROUP,
        config: {
          name,
          trackId,
        }
      });
    }

    // Add global slice tracks.
    const globalSliceTracks = await engine.query(`
      select
        track.name as track_name,
        track.id as track_id,
        max(depth) as max_depth
      from track
      join slice on track.id = slice.track_id
      where track.type = 'track'
      group by track_id
    `);

    for (let i = 0; i < globalSliceTracks.numRecords; i++) {
      const name = globalSliceTracks.columns[0].stringValues![i];
      const trackId = +globalSliceTracks.columns[1].longValues![i];
      const maxDepth = +globalSliceTracks.columns[2].longValues![i];

      tracksToAdd.push({
        engineId: this.engineId,
        kind: SLICE_TRACK_KIND,
        name: `${name}`,
        trackGroup: SCROLLING_TRACK_GROUP,
        config: {maxDepth, trackId},
      });
    }

    interface CounterTrack {
      name: string;
      trackId: number;
      startTs?: number;
      endTs?: number;
    }

    const counterUtids = new Map<number, CounterTrack[]>();
    const threadCounters = await engine.query(`
      select thread_counter_track.name, utid, thread_counter_track.id,
      start_ts, end_ts from thread_counter_track join thread using(utid)
    `);
    for (let i = 0; i < threadCounters.numRecords; i++) {
      const name = threadCounters.columns[0].stringValues![i];
      const utid = +threadCounters.columns[1].longValues![i];
      const trackId = +threadCounters.columns[2].longValues![i];
      let startTs = undefined;
      let endTs = undefined;
      if (!threadCounters.columns[3].isNulls![i]) {
        startTs = +threadCounters.columns[3].longValues![i] / 1e9;
      }
      if (!threadCounters.columns[4].isNulls![i]) {
        endTs = +threadCounters.columns[4].longValues![i] / 1e9;
      }

      const track: CounterTrack = {name, trackId, startTs, endTs};
      const el = counterUtids.get(utid);
      if (el === undefined) {
        counterUtids.set(utid, [track]);
      } else {
        el.push(track);
      }
    }

    const counterUpids = new Map<number, CounterTrack[]>();
    const processCounters = await engine.query(`
      select process_counter_track.name, upid, process_counter_track.id,
      start_ts, end_ts from process_counter_track join process using(upid)
    `);
    for (let i = 0; i < processCounters.numRecords; i++) {
      const name = processCounters.columns[0].stringValues![i];
      const upid = +processCounters.columns[1].longValues![i];
      const trackId = +processCounters.columns[2].longValues![i];
      let startTs = undefined;
      let endTs = undefined;
      if (!processCounters.columns[3].isNulls![i]) {
        startTs = +processCounters.columns[3].longValues![i] / 1e9;
      }
      if (!processCounters.columns[4].isNulls![i]) {
        endTs = +processCounters.columns[4].longValues![i] / 1e9;
      }

      const track: CounterTrack = {name, trackId, startTs, endTs};
      const el = counterUpids.get(upid);
      if (el === undefined) {
        counterUpids.set(upid, [track]);
      } else {
        el.push(track);
      }
    }

    // Local experiments shows getting maxDepth separately is ~2x faster than
    // joining with threads and processes.
    const maxDepthQuery = await engine.query(`
          select thread_track.utid, thread_track.id, max(depth) as maxDepth
          from slice
          inner join thread_track on slice.track_id = thread_track.id
          group by thread_track.id
        `);

    const utidToThreadTrack = new Map<number, ThreadSliceTrack>();
    for (let i = 0; i < maxDepthQuery.numRecords; i++) {
      const utid = maxDepthQuery.columns[0].longValues![i] as number;
      const trackId = maxDepthQuery.columns[1].longValues![i] as number;
      const maxDepth = maxDepthQuery.columns[2].longValues![i] as number;
      utidToThreadTrack.set(utid, {maxDepth, trackId});
    }

    // Return all threads
    // sorted by:
    //  total cpu time *for the whole parent process*
    //  upid
    //  utid
    const threadQuery = await engine.query(`
        select
          utid,
          tid,
          upid,
          pid,
          thread.name as threadName,
          process.name as processName,
          total_dur as totalDur,
          ifnull(has_sched, false) as hasSched
        from
          thread
          left join (select utid, count(1), true as has_sched
              from sched group by utid
          ) using(utid)
          left join process using(upid)
          left join (select upid, sum(dur) as total_dur
              from sched join thread using(utid)
              group by upid
            ) using(upid)
        where utid != 0
        group by utid, upid
        order by total_dur desc, upid, utid`);

    const upidToUuid = new Map<number, string>();
    const utidToUuid = new Map<number, string>();
    const addTrackGroupActions: DeferredAction[] = [];

    for (const row of rawQueryToRows(threadQuery, {
           utid: NUM,
           upid: NUM_NULL,
           tid: NUM_NULL,
           pid: NUM_NULL,
           threadName: STR_NULL,
           processName: STR_NULL,
           totalDur: NUM_NULL,
           hasSched: NUM,
         })) {
      const utid = row.utid;
      const tid = row.tid;
      const upid = row.upid;
      const pid = row.pid;
      const threadName = row.threadName;
      const processName = row.processName;
      const hasSchedEvents = !!row.totalDur;
      const threadHasSched = !!row.hasSched;

      const threadTrack =
          utid === null ? undefined : utidToThreadTrack.get(utid);
      if (threadTrack === undefined &&
          (upid === null || counterUpids.get(upid) === undefined) &&
          counterUtids.get(utid) === undefined && !threadHasSched &&
          (upid === null || upid !== null && !heapUpids.has(upid))) {
        continue;
      }

      // Group by upid if present else by utid.
      let pUuid = upid === null ? utidToUuid.get(utid) : upidToUuid.get(upid);
      // These should only happen once for each track group.
      if (pUuid === undefined) {
        pUuid = uuidv4();
        const summaryTrackId = uuidv4();
        if (upid === null) {
          utidToUuid.set(utid, pUuid);
        } else {
          upidToUuid.set(upid, pUuid);
        }

        const pidForColor = pid || tid || upid || utid || 0;
        const kind = hasSchedEvents ? PROCESS_SCHEDULING_TRACK_KIND :
                                      PROCESS_SUMMARY_TRACK;

        tracksToAdd.push({
          id: summaryTrackId,
          engineId: this.engineId,
          kind,
          name: `${upid === null ? tid : pid} summary`,
          config: {pidForColor, upid, utid},
        });

        const name =
            getTrackName({utid, processName, pid, threadName, tid, upid});
        const addTrackGroup = Actions.addTrackGroup({
          engineId: this.engineId,
          summaryTrackId,
          name,
          id: pUuid,
          collapsed: !(upid !== null && heapUpids.has(upid)),
        });

        // If the track group contains a heap profile, it should be before all
        // other processes.
        if (upid !== null && heapUpids.has(upid)) {
          addTrackGroupActions.unshift(addTrackGroup);
        } else {
          addTrackGroupActions.push(addTrackGroup);
        }

        if (upid !== null) {
          if (heapUpids.has(upid)) {
            tracksToAdd.push({
              engineId: this.engineId,
              kind: HEAP_PROFILE_TRACK_KIND,
              name: `Heap Profile`,
              trackGroup: pUuid,
              config: {upid}
            });
          }

          const counterNames = counterUpids.get(upid);
          if (counterNames !== undefined) {
            counterNames.forEach(element => {
              tracksToAdd.push({
                engineId: this.engineId,
                kind: 'CounterTrack',
                name: element.name,
                trackGroup: pUuid,
                config: {
                  name: element.name,
                  trackId: element.trackId,
                  startTs: element.startTs,
                  endTs: element.endTs,
                }
              });
            });
          }

          if (upidToProcessTracks.has(upid)) {
            for (const track of upidToProcessTracks.get(upid)) {
              tracksToAdd.push(Object.assign(track, {trackGroup: pUuid}));
            }
          }
        }
      }
      const counterThreadNames = counterUtids.get(utid);
      if (counterThreadNames !== undefined) {
        counterThreadNames.forEach(element => {
          tracksToAdd.push({
            engineId: this.engineId,
            kind: 'CounterTrack',
            name: element.name,
            trackGroup: pUuid,
            config: {
              name: element.name,
              trackId: element.trackId,
              startTs: element.startTs,
              endTs: element.endTs,
            }
          });
        });
      }
      if (threadHasSched) {
        tracksToAdd.push({
          engineId: this.engineId,
          kind: THREAD_STATE_TRACK_KIND,
          name: getTrackName({utid, tid, threadName}),
          trackGroup: pUuid,
          config: {utid}
        });
      }

      if (threadTrack !== undefined) {
        tracksToAdd.push({
          engineId: this.engineId,
          kind: SLICE_TRACK_KIND,
          name: getTrackName({utid, tid, threadName}),
          trackGroup: pUuid,
          config:
              {maxDepth: threadTrack.maxDepth, trackId: threadTrack.trackId},
        });
      }
    }

    const logCount = await engine.query(`select count(1) from android_logs`);
    if (logCount.columns[0].longValues![0] > 0) {
      tracksToAdd.push({
        engineId: this.engineId,
        kind: ANDROID_LOGS_TRACK_KIND,
        name: 'Android logs',
        trackGroup: SCROLLING_TRACK_GROUP,
        config: {}
      });
    }

    const annotationSliceRows = await engine.query(`
      SELECT id, name FROM annotation_slice_track`);
    for (let i = 0; i < annotationSliceRows.numRecords; i++) {
      const id = annotationSliceRows.columns[0].longValues![i];
      const name = annotationSliceRows.columns[1].stringValues![i];
      tracksToAdd.push({
        engineId: this.engineId,
        kind: SLICE_TRACK_KIND,
        name,
        trackGroup: SCROLLING_TRACK_GROUP,
        config: {
          maxDepth: 0,
          namespace: 'annotation',
          trackId: id,
        },
      });
    }

    const annotationCounterRows = await engine.query(`
      SELECT id, name FROM annotation_counter_track`);
    for (let i = 0; i < annotationCounterRows.numRecords; i++) {
      const id = annotationCounterRows.columns[0].longValues![i];
      const name = annotationCounterRows.columns[1].stringValues![i];
      tracksToAdd.push({
        engineId: this.engineId,
        kind: 'CounterTrack',
        name,
        trackGroup: SCROLLING_TRACK_GROUP,
        config: {
          name,
          namespace: 'annotation',
          trackId: id,
        }
      });
    }

    addTrackGroupActions.push(Actions.addTracks({tracks: tracksToAdd}));
    globals.dispatchMultiple(addTrackGroupActions);
  }

  private async listThreads() {
    this.updateStatus('Reading thread list');
    const sqlQuery = `select utid, tid, pid, thread.name,
        ifnull(
          case when length(process.name) > 0 then process.name else null end,
          thread.name)
        from (select * from thread order by upid) as thread
        left join (select * from process order by upid) as process
        using(upid)`;
    const threadRows = await assertExists(this.engine).query(sqlQuery);
    const threads: ThreadDesc[] = [];
    for (let i = 0; i < threadRows.numRecords; i++) {
      const utid = threadRows.columns[0].longValues![i] as number;
      const tid = threadRows.columns[1].longValues![i] as number;
      const pid = threadRows.columns[2].longValues![i] as number;
      const threadName = threadRows.columns[3].stringValues![i];
      const procName = threadRows.columns[4].stringValues![i];
      threads.push({utid, tid, threadName, pid, procName});
    }  // for (record ...)
    globals.publish('Threads', threads);
  }

  private async loadTimelineOverview(traceTime: TimeSpan) {
    const engine = assertExists<Engine>(this.engine);
    const numSteps = 100;
    const stepSec = traceTime.duration / numSteps;
    let hasSchedOverview = false;
    for (let step = 0; step < numSteps; step++) {
      this.updateStatus(
          'Loading overview ' +
          `${Math.round((step + 1) / numSteps * 1000) / 10}%`);
      const startSec = traceTime.start + step * stepSec;
      const startNs = toNsFloor(startSec);
      const endSec = startSec + stepSec;
      const endNs = toNsCeil(endSec);

      // Sched overview.
      const schedRows = await engine.query(
          `select sum(dur)/${stepSec}/1e9, cpu from sched ` +
          `where ts >= ${startNs} and ts < ${endNs} and utid != 0 ` +
          'group by cpu order by cpu');
      const schedData: {[key: string]: QuantizedLoad} = {};
      for (let i = 0; i < schedRows.numRecords; i++) {
        const load = schedRows.columns[0].doubleValues![i];
        const cpu = schedRows.columns[1].longValues![i] as number;
        schedData[cpu] = {startSec, endSec, load};
        hasSchedOverview = true;
      }  // for (record ...)
      globals.publish('OverviewData', schedData);
    }  // for (step ...)

    if (hasSchedOverview) {
      return;
    }

    // Slices overview.
    const traceStartNs = toNs(traceTime.start);
    const stepSecNs = toNs(stepSec);
    const sliceSummaryQuery = await engine.query(`select
           bucket,
           upid,
           sum(utid_sum) / cast(${stepSecNs} as float) as upid_sum
         from thread
         inner join (
           select
             cast((ts - ${traceStartNs})/${stepSecNs} as int) as bucket,
             sum(dur) as utid_sum,
             utid
           from slice
           inner join thread_track on slice.track_id = thread_track.id
           group by bucket, utid
         ) using(utid)
         group by bucket, upid`);

    const slicesData: {[key: string]: QuantizedLoad[]} = {};
    for (let i = 0; i < sliceSummaryQuery.numRecords; i++) {
      const bucket = sliceSummaryQuery.columns[0].longValues![i] as number;
      const upid = sliceSummaryQuery.columns[1].longValues![i] as number;
      const load = sliceSummaryQuery.columns[2].doubleValues![i];

      const startSec = traceTime.start + stepSec * bucket;
      const endSec = startSec + stepSec;

      const upidStr = upid.toString();
      let loadArray = slicesData[upidStr];
      if (loadArray === undefined) {
        loadArray = slicesData[upidStr] = [];
      }
      loadArray.push({startSec, endSec, load});
    }
    globals.publish('OverviewData', slicesData);
  }

  async initialiseHelperViews() {
    const engine = assertExists<Engine>(this.engine);
    this.updateStatus('Creating helper views');
    let event = 'sched_waking';
    const waking = await engine.query(
        `select * from instants where name = 'sched_waking' limit 1`);
    if (waking.numRecords === 0) {
      // Only use sched_wakeup if sched_waking is not in the trace.
      event = 'sched_wakeup';
    }
    await engine.query(`create view runnable AS
      select
        ts,
        lead(ts, 1, (select end_ts from trace_bounds))
          OVER(partition by ref order by ts) - ts as dur,
        ref as utid
      from instants
      where name = '${event}'`);

    // Get the first ts for each utid - whether a sched wakeup/waking
    // or sched event.
    await engine.query(`create view first_thread as
      select min(ts) as ts, utid from
      (select min(ts) as ts, utid from runnable group by utid
       UNION
      select min(ts) as ts,utid from sched group by utid)
      group by utid`);

    // Create an entry from first ts to either the first sched_wakeup/waking
    // or to the end if there are no sched wakeup/ings. This means we will
    // show all information we have even with no sched_wakeup/waking events.
    await engine.query(`create view fill as
      select first_thread.ts as ts,
      coalesce(min(runnable.ts), (select end_ts from trace_bounds)) -
      first_thread.ts as dur,
      first_thread.utid as utid
      from first_thread
      LEFT JOIN runnable using(utid) group by utid`);

    await engine.query(`create view full_runnable as
        select * from runnable UNION
        select * from fill`);

    await engine.query(`create virtual table thread_span
        using span_left_join(
          full_runnable partitioned utid,
          sched partitioned utid)`);

    // For performance reasons we need to create a table here.
    // Once b/145350531 is fixed this should be able to revert to a
    // view and we can recover the extra memory use.
    await engine.query(`create table thread_state as
      select ts, dur, utid, cpu,
      case when end_state is not null then 'Running'
      when lag(end_state) over ordered is not null
      then lag(end_state) over ordered else 'R'
      end as state
      from thread_span window ordered as
      (partition by utid order by ts)`);

    await engine.query(`create index utid_index on thread_state(utid)`);

    // Create the helper tables for all the annotations related data.
    await engine.query(`
      CREATE TABLE annotation_counter_track(
        id INTEGER PRIMARY KEY,
        name STRING,
        __metric_name STRING
      );
    `);
    await engine.query(`
      CREATE TABLE annotation_slice_track(
        id INTEGER PRIMARY KEY,
        name STRING,
        __metric_name STRING
      );
    `);

    await engine.query(`
      CREATE TABLE annotation_counter(
        id BIG INT,
        track_id INT,
        ts BIG INT,
        value DOUBLE,
        PRIMARY KEY (track_id, ts)
      ) WITHOUT ROWID;
    `);
    await engine.query(`
      CREATE TABLE annotation_slice(
        id BIG INT,
        track_id INT,
        ts BIG INT,
        dur BIG INT,
        depth INT,
        name STRING,
        PRIMARY KEY (track_id, ts)
      ) WITHOUT ROWID;
    `);

    for (const metric of ['android_startup', 'android_ion']) {
      // We don't care about the actual result of metric here as we are just
      // interested in the annotation tracks.
      const metricResult = await engine.computeMetric([metric]);
      assertTrue(metricResult.error.length === 0);

      const result = await engine.query(`
        SELECT * FROM ${metric}_annotations LIMIT 1`);

      const hasSliceName =
          result.columnDescriptors.some(x => x.name === 'slice_name');
      const hasDur = result.columnDescriptors.some(x => x.name === 'dur');

      if (hasSliceName && hasDur) {
        await engine.query(`
          INSERT INTO annotation_slice_track(name, __metric_name)
          SELECT DISTINCT track_name, '${metric}' as metric_name
          FROM ${metric}_annotations
          WHERE track_type = 'slice'
        `);
        await engine.query(`
          INSERT INTO annotation_slice(id, track_id, ts, dur, depth, name)
          SELECT
            -1 as id,
            t.id AS track_id,
            ts,
            dur,
            0 AS depth,
            slice_name AS name
          FROM ${metric}_annotations a
          JOIN annotation_slice_track t
          ON a.track_name = t.name AND t.__metric_name = '${metric}'
          ORDER BY t.id, ts
        `);
      }

      const hasValue = result.columnDescriptors.some(x => x.name === 'value');
      if (hasValue) {
        await engine.query(`
          INSERT INTO annotation_counter_track(name, __metric_name)
          SELECT DISTINCT track_name, '${metric}' as metric_name
          FROM ${metric}_annotations
          WHERE track_type = 'counter'
        `);
        await engine.query(`
          INSERT INTO annotation_counter(id, track_id, ts, value)
          SELECT
            -1 as id,
            t.id AS track_id,
            ts,
            value
          FROM ${metric}_annotations a
          JOIN annotation_counter_track t
          ON a.track_name = t.name AND t.__metric_name = '${metric}'
          ORDER BY t.id, ts
        `);
      }
    }
  }

  private updateStatus(msg: string): void {
    globals.dispatch(Actions.updateStatus({
      msg,
      timestamp: Date.now() / 1000,
    }));
  }
}

function getTrackName(args: Partial<{
  utid: number,
  processName: string | null,
  pid: number | null,
  threadName: string | null,
  tid: number | null,
  upid: number | null
}>) {
  const {upid, utid, processName, threadName, pid, tid} = args;

  const hasUpid = upid !== undefined && upid !== null;
  const hasUtid = utid !== undefined && utid !== null;
  const hasProcessName = processName !== undefined && processName !== null;
  const hasThreadName = threadName !== undefined && threadName !== null;
  const hasTid = tid !== undefined && tid !== null;
  const hasPid = pid !== undefined && pid !== null;

  if (hasUpid && hasPid && hasProcessName) {
    return `${processName} ${pid}`;
  } else if (hasUpid && hasPid) {
    return `Process ${pid}`;
  } else if (hasThreadName && hasTid) {
    return `${threadName} ${tid}`;
  } else if (hasTid) {
    return `Thread ${tid}`;
  } else if (hasUpid) {
    return `upid: ${upid}`;
  } else if (hasUtid) {
    return `utid: ${utid}`;
  }
  return 'Unknown';
}
