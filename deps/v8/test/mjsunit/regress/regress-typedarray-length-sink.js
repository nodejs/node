// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class AudioProcessor {
  constructor(size = 512) {
    this.buffer = new Float32Array(size);
    this.position = 0;
  }

  generate(buffer, offset, count) {
    // Fill samples
    for (let i = 0; i < count; i++) {
      buffer[offset + i] = 0.5;
    }
  }

  _onBuffer(buf) {
    // Simulate DOM postMessage by detaching array buffer
    %ArrayBufferDetach(buf.buffer);
  }

  advance(samples) {
    let remaining = samples | 0;
    const bufferLength = this.buffer.length; // Loaded before loop: expected 512
    while (remaining > 0) {
      const todo = Math.min(remaining, bufferLength - this.position);
      this.generate(this.buffer, this.position, todo);
      this.position += todo;
      remaining -= todo;
      if (this.position === bufferLength) {
        this._onBuffer(this.buffer); // Detaches buffer.buffer in-place

        this.buffer = new Float32Array(bufferLength);

        if (this.buffer.length !== 512 || bufferLength !== 512) {
          throw new Error(
              `Bug detected! After detachment: bufferLength evaluated to ${
                  bufferLength}, new Float32Array length is ${
                  this.buffer.length} (expected 512)`);
        }
        this.position = 0;
      }
    }
    return true;
  }
}

function runRegressionTest() {
  const processor = new AudioProcessor(512);

  // Prepare methods for JIT compilation
  %PrepareFunctionForOptimization(AudioProcessor.prototype.advance);
  %PrepareFunctionForOptimization(AudioProcessor.prototype.generate);
  %PrepareFunctionForOptimization(AudioProcessor.prototype._onBuffer);

  // 1. Warm up unoptimized to gather type feedback
  for (let i = 0; i < 20; i++) {
    processor.advance(1024);
  }

  // 2. Trigger JIT optimization on next call
  %OptimizeFunctionOnNextCall(AudioProcessor.prototype.advance);

  // 3. Execute optimized call
  processor.advance(1024);
}

runRegressionTest();
