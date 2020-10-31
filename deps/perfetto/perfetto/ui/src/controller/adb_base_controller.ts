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

import {extractDurationFromTraceConfig} from '../base/extract_utils';
import {extractTraceConfig} from '../base/extract_utils';
import {isAdbTarget} from '../common/state';

import {Adb} from './adb_interfaces';
import {ReadBuffersResponse} from './consumer_port_types';
import {globals} from './globals';
import {Consumer, RpcConsumerPort} from './record_controller_interfaces';

export enum AdbAuthState {
  DISCONNECTED,
  AUTH_IN_PROGRESS,
  CONNECTED,
}

interface Command {
  method: string;
  params: Uint8Array;
}

export abstract class AdbBaseConsumerPort extends RpcConsumerPort {
  // Contains the commands sent while the authentication is in progress. They
  // will all be executed afterwards. If the device disconnects, they are
  // removed.
  private commandQueue: Command[] = [];

  protected adb: Adb;
  protected state = AdbAuthState.DISCONNECTED;
  protected device?: USBDevice;

  constructor(adb: Adb, consumer: Consumer) {
    super(consumer);
    this.adb = adb;
  }

  async handleCommand(method: string, params: Uint8Array) {
    try {
      this.commandQueue.push({method, params});
      if (this.state === AdbAuthState.DISCONNECTED ||
          this.deviceDisconnected()) {
        this.state = AdbAuthState.AUTH_IN_PROGRESS;
        this.device = await this.findDevice();
        if (!this.device) {
          this.state = AdbAuthState.DISCONNECTED;
          const target = globals.state.recordingTarget;
          throw Error(`Device with serial ${
              isAdbTarget(target) ? target.serial : 'n/a'} not found.`);
        }

        this.sendStatus(`Please allow USB debugging on device.
          If you press cancel, reload the page.`);

        await this.adb.connect(this.device);

        // During the authentication the device may have been disconnected.
        if (!globals.state.recordingInProgress || this.deviceDisconnected()) {
          throw Error('Recording not in progress after adb authorization.');
        }

        this.state = AdbAuthState.CONNECTED;
        this.sendStatus('Device connected.');
      }

      if (this.state === AdbAuthState.AUTH_IN_PROGRESS) return;

      console.assert(this.state === AdbAuthState.CONNECTED);

      for (const cmd of this.commandQueue) this.invoke(cmd.method, cmd.params);

      this.commandQueue = [];
    } catch (e) {
      this.commandQueue = [];
      this.state = AdbAuthState.DISCONNECTED;
      this.sendErrorMessage(e.message);
    }
  }

  private deviceDisconnected() {
    return !this.device || !this.device.opened;
  }

  setDurationStatus(enableTracingProto: Uint8Array) {
    const traceConfigProto = extractTraceConfig(enableTracingProto);
    if (!traceConfigProto) return;
    const duration = extractDurationFromTraceConfig(traceConfigProto);
    this.sendStatus(`Recording in progress${
        duration ? ' for ' + duration.toString() + ' ms' : ''}...`);
  }

  abstract invoke(method: string, argsProto: Uint8Array): void;

  generateChunkReadResponse(data: Uint8Array, last = false):
      ReadBuffersResponse {
    return {
      type: 'ReadBuffersResponse',
      slices: [{data, lastSliceForPacket: last}]
    };
  }

  async findDevice(): Promise<USBDevice|undefined> {
    if (!('usb' in navigator)) return undefined;
    const connectedDevice = globals.state.recordingTarget;
    if (!isAdbTarget(connectedDevice)) return undefined;
    const devices = await navigator.usb.getDevices();
    return devices.find(d => d.serialNumber === connectedDevice.serial);
  }
}