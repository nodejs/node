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

import {_TextDecoder} from 'custom_utils';

import {extractTraceConfig} from '../base/extract_utils';
import {uint8ArrayToBase64} from '../base/string_utils';

import {AdbAuthState, AdbBaseConsumerPort} from './adb_base_controller';
import {Adb, AdbStream} from './adb_interfaces';
import {ReadBuffersResponse} from './consumer_port_types';
import {Consumer} from './record_controller_interfaces';

enum AdbShellState {
  READY,
  RECORDING,
  FETCHING
}
const DEFAULT_DESTINATION_FILE = '/data/misc/perfetto-traces/trace-by-ui';
const textDecoder = new _TextDecoder();

export class AdbConsumerPort extends AdbBaseConsumerPort {
  traceDestFile = DEFAULT_DESTINATION_FILE;
  shellState: AdbShellState = AdbShellState.READY;
  private recordShell?: AdbStream;

  constructor(adb: Adb, consumer: Consumer) {
    super(adb, consumer);
    this.adb = adb;
  }

  async invoke(method: string, params: Uint8Array) {
    // ADB connection & authentication is handled by the superclass.
    console.assert(this.state === AdbAuthState.CONNECTED);

    switch (method) {
      case 'EnableTracing':
        this.enableTracing(params);
        break;
      case 'ReadBuffers':
        this.readBuffers();
        break;
      case 'DisableTracing':
        this.disableTracing();
        break;
      case 'FreeBuffers':
        this.freeBuffers();
        break;
      case 'GetTraceStats':
        break;
      default:
        this.sendErrorMessage(`Method not recognized: ${method}`);
        break;
    }
  }

  async enableTracing(enableTracingProto: Uint8Array) {
    try {
      const traceConfigProto = extractTraceConfig(enableTracingProto);
      if (!traceConfigProto) {
        this.sendErrorMessage('Invalid config.');
        return;
      }

      await this.startRecording(traceConfigProto);
      this.setDurationStatus(enableTracingProto);
    } catch (e) {
      this.sendErrorMessage(e.message);
    }
  }

  async startRecording(configProto: Uint8Array) {
    this.shellState = AdbShellState.RECORDING;
    const recordCommand = this.generateStartTracingCommand(configProto);
    this.recordShell = await this.adb.shell(recordCommand);
    const output: string[] = [];
    this.recordShell.onData = raw => output.push(textDecoder.decode(raw));
    this.recordShell.onClose = () => {
      const response = output.join();
      if (!this.tracingEndedSuccessfully(response)) {
        this.sendErrorMessage(response);
        this.shellState = AdbShellState.READY;
        return;
      }
      this.sendStatus('Recording ended successfully. Fetching the trace..');
      this.sendMessage({type: 'EnableTracingResponse'});
      this.recordShell = undefined;
    };
  }

  tracingEndedSuccessfully(response: string): boolean {
    return !response.includes(' 0 ms') && response.includes('Wrote ');
  }

  async readBuffers() {
    console.assert(this.shellState === AdbShellState.RECORDING);
    this.shellState = AdbShellState.FETCHING;

    const readTraceShell =
        await this.adb.shell(this.generateReadTraceCommand());
    readTraceShell.onData = raw =>
        this.sendMessage(this.generateChunkReadResponse(raw));

    readTraceShell.onClose = () => {
      this.sendMessage(
          this.generateChunkReadResponse(new Uint8Array(), /* last */ true));
    };
  }

  async getPidFromShellAsString() {
    const pidStr =
        await this.adb.shellOutputAsString(`ps -u shell | grep perfetto`);
    // We used to use awk '{print $2}' but older phones/Go phones don't have
    // awk installed. Instead we implement similar functionality here.
    const awk = pidStr.split(' ').filter(str => str !== '');
    if (awk.length < 1) {
      throw Error(`Unabled to find perfetto pid in string "${pidStr}"`);
    }
    return awk[1];
  }

  async disableTracing() {
    if (!this.recordShell) return;
    try {
      // We are not using 'pidof perfetto' so that we can use more filters. 'ps
      // -u shell' is meant to catch processes started from shell, so if there
      // are other ongoing tracing sessions started by others, we are not
      // killing them.
      const pid = await this.getPidFromShellAsString();

      if (pid.length === 0 || isNaN(Number(pid))) {
        throw Error(`Perfetto pid not found. Impossible to stop/cancel the
     recording. Command output: ${pid}`);
      }
      // Perfetto stops and finalizes the tracing session on SIGINT.
      const killOutput =
          await this.adb.shellOutputAsString(`kill -SIGINT ${pid}`);

      if (killOutput.length !== 0) {
        throw Error(`Unable to kill perfetto: ${killOutput}`);
      }
    } catch (e) {
      this.sendErrorMessage(e.message);
    }
  }

  freeBuffers() {
    this.shellState = AdbShellState.READY;
    if (this.recordShell) {
      this.recordShell.close();
      this.recordShell = undefined;
    }
  }

  generateChunkReadResponse(data: Uint8Array, last = false):
      ReadBuffersResponse {
    return {
      type: 'ReadBuffersResponse',
      slices: [{data, lastSliceForPacket: last}]
    };
  }

  generateReadTraceCommand(): string {
    // We attempt to delete the trace file after tracing. On a non-root shell,
    // this will fail (due to selinux denial), but perfetto cmd will be able to
    // override the file later. However, on a root shell, we need to clean up
    // the file since perfetto cmd might otherwise fail to override it in a
    // future session.
    return `gzip -c ${this.traceDestFile} && rm -f ${this.traceDestFile}`;
  }

  generateStartTracingCommand(tracingConfig: Uint8Array) {
    const configBase64 = uint8ArrayToBase64(tracingConfig);
    const perfettoCmd = `perfetto -c - -o ${this.traceDestFile}`;
    return `echo '${configBase64}' | base64 -d | ${perfettoCmd}`;
  }
}
