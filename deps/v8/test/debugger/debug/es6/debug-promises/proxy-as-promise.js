// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Test debug events when we listen to all exceptions and
// there is a catch handler for the exception thrown in a Promise.
// We expect a normal Exception debug event to be triggered.

Debug = debug.Debug;

var expected_events = 1;
var log = [];


class P extends Promise {
    constructor(...args) {
        super(...args);
        return new Proxy(this, {
            get(target, property, receiver) {
                if (property in target) {
                    return Reflect.get(target, property, receiver);
                } else {
                    return (...args) =>
                        new Promise((resolve, reject) =>
                            target.then(v => resolve(v[property](...args)))
                                .catch(reject)
                        );
                }
            }
        });
    }
}

P.resolve({doStuff(){log.push(1)}}).doStuff()

function listener(event, exec_state, event_data, data) {}

Debug.setBreakOnUncaughtException();
Debug.setListener(listener);

%PerformMicrotaskCheckpoint();
