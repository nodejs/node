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
import {Engine} from '../common/engine';
import {Registry} from '../common/registry';
import {TraceTime, TrackState} from '../common/state';
import {LIMIT, TrackData} from '../common/track_data';

import {Controller} from './controller';
import {ControllerFactory} from './controller';
import {globals} from './globals';

interface TrackConfig {}

type TrackConfigWithNamespace = TrackConfig&{namespace: string};


// TrackController is a base class overridden by track implementations (e.g.,
// sched slices, nestable slices, counters).
export abstract class TrackController<
    Config, Data extends TrackData = TrackData> extends Controller<'main'> {
  readonly trackId: string;
  readonly engine: Engine;
  private data?: TrackData;
  private requestingData = false;
  private queuedRequest = false;

  constructor(args: TrackControllerArgs) {
    super('main');
    this.trackId = args.trackId;
    this.engine = args.engine;
  }

  // Must be overridden by the track implementation. Is invoked when the track
  // frontend runs out of cached data. The derived track controller is expected
  // to publish new track data in response to this call.
  abstract async onBoundsChange(start: number, end: number, resolution: number):
      Promise<Data>;

  get trackState(): TrackState {
    return assertExists(globals.state.tracks[this.trackId]);
  }

  get config(): Config {
    return this.trackState.config as Config;
  }

  configHasNamespace(config: TrackConfig): config is TrackConfigWithNamespace {
    return 'namespace' in config;
  }

  namespaceTable(tableName: string): string {
    if (this.configHasNamespace(this.config)) {
      return this.config.namespace + '_' + tableName;
    } else {
      return tableName;
    }
  }

  publish(data: Data): void {
    this.data = data;
    globals.publish('TrackData', {id: this.trackId, data});
  }

  /**
   * Returns a valid SQL table name with the given prefix that should be unique
   * for each track.
   */
  tableName(prefix: string) {
    // Derive table name from, since that is unique for each track.
    // Track ID can be UUID but '-' is not valid for sql table name.
    const idSuffix = this.trackId.split('-').join('_');
    return `${prefix}_${idSuffix}`;
  }

  shouldSummarize(resolution: number): boolean {
    // |resolution| is in s/px (to nearest power of 10) assuming a display
    // of ~1000px 0.0008 is 0.8s.
    return resolution >= 0.0008;
  }

  protected async query(query: string) {
    const result = await this.engine.query(query);
    return result;
  }

  shouldRequestData(traceTime: TraceTime): boolean {
    if (this.data === undefined) return true;

    // If at the limit only request more data if the view has moved.
    const atLimit = this.data.length === LIMIT;
    if (atLimit) {
      // We request more data than the window, so add window duration to find
      // the previous window.
      const prevWindowStart =
          this.data.start + (traceTime.startSec - traceTime.endSec);
      return traceTime.startSec !== prevWindowStart;
    }

    // Otherwise request more data only when out of range of current data or
    // resolution has changed.
    const inRange = traceTime.startSec >= this.data.start &&
        traceTime.endSec <= this.data.end;
    return !inRange ||
        this.data.resolution !==
        globals.state.frontendLocalState.visibleState.resolution;
  }

  run() {
    const visibleState = globals.state.frontendLocalState.visibleState;
    if (visibleState === undefined || visibleState.resolution === undefined ||
        visibleState.resolution === Infinity) {
      return;
    }
    const dur = visibleState.endSec - visibleState.startSec;
    if (globals.state.visibleTracks.includes(this.trackId) &&
        this.shouldRequestData(visibleState)) {
      if (this.requestingData) {
        this.queuedRequest = true;
      } else {
        this.requestingData = true;
        this.onBoundsChange(
                visibleState.startSec - dur,
                visibleState.endSec + dur,
                visibleState.resolution)
            .then(data => {
              this.publish(data);
            })
            .catch(error => {
              console.error(error);
            })
            .finally(() => {
              this.requestingData = false;
              if (this.queuedRequest) {
                this.queuedRequest = false;
                this.run();
              }
            });
      }
    }
  }
}

export interface TrackControllerArgs {
  trackId: string;
  engine: Engine;
}

export interface TrackControllerFactory extends
    ControllerFactory<TrackControllerArgs> {
  kind: string;
}

export const trackControllerRegistry = new Registry<TrackControllerFactory>();
