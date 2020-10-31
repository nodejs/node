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

import {globals} from './globals';

export class Animation {
  private startMs = 0;
  private endMs = 0;
  private boundOnAnimationFrame = this.onAnimationFrame.bind(this);

  constructor(private onAnimationStep: (timeSinceStartMs: number) => void) {}

  start(durationMs: number) {
    const nowMs = performance.now();

    // If the animation is already happening, just update its end time.
    if (nowMs <= this.endMs) {
      this.endMs = nowMs + durationMs;
      return;
    }
    this.startMs = nowMs;
    this.endMs = nowMs + durationMs;
    globals.rafScheduler.start(this.boundOnAnimationFrame);
  }

  stop() {
    this.endMs = 0;
    globals.rafScheduler.stop(this.boundOnAnimationFrame);
  }

  get startTimeMs(): number {
    return this.startMs;
  }

  private onAnimationFrame(nowMs: number) {
    if (nowMs >= this.endMs) {
      globals.rafScheduler.stop(this.boundOnAnimationFrame);
      return;
    }
    this.onAnimationStep(Math.max(Math.round(nowMs - this.startMs), 0));
  }
}
