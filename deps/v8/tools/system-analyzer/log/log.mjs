// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


class Event {
  #time;
  #type;
  constructor(type, time) {
    //TODO(zcankara) remove type and add empty getters to override
    this.#time = time;
    this.#type = type;
  }
  get time() {
    return this.#time;
  }
  get type() {
    return this.#type;
  }
}

export { Event };
