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

import {ConsumerPortResponse} from './consumer_port_types';

export type ConsumerPortCallback = (_: ConsumerPortResponse) => void;
export type ErrorCallback = (_: string) => void;
export type StatusCallback = (_: string) => void;

export abstract class RpcConsumerPort {
  // The responses of the call invocations should be sent through this listener.
  // This is done by the 3 "send" methods in this abstract class.
  private consumerPortListener: Consumer;

  constructor(consumerPortListener: Consumer) {
    this.consumerPortListener = consumerPortListener;
  }

  // RequestData is the proto representing the arguments of the function call.
  abstract handleCommand(methodName: string, requestData: Uint8Array): void;

  sendMessage(data: ConsumerPortResponse) {
    this.consumerPortListener.onConsumerPortResponse(data);
  }

  sendErrorMessage(message: string) {
    this.consumerPortListener.onError(message);
  }

  sendStatus(status: string) {
    this.consumerPortListener.onStatus(status);
  }
}

export interface Consumer {
  onConsumerPortResponse(data: ConsumerPortResponse): void;
  onError: ErrorCallback;
  onStatus: StatusCallback;
}