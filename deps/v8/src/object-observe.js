// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"use strict";

var observationState = %GetObservationState();
if (IS_UNDEFINED(observationState.observerInfoMap)) {
  observationState.observerInfoMap = %ObservationWeakMapCreate();
  observationState.objectInfoMap = %ObservationWeakMapCreate();
  observationState.notifierTargetMap = %ObservationWeakMapCreate();
  observationState.pendingObservers = new InternalArray;
  observationState.observerPriority = 0;
}

function ObservationWeakMap(map) {
  this.map_ = map;
}

ObservationWeakMap.prototype = {
  get: function(key) {
    key = %UnwrapGlobalProxy(key);
    if (!IS_SPEC_OBJECT(key)) return void 0;
    return %WeakMapGet(this.map_, key);
  },
  set: function(key, value) {
    key = %UnwrapGlobalProxy(key);
    if (!IS_SPEC_OBJECT(key)) return void 0;
    %WeakMapSet(this.map_, key, value);
  },
  has: function(key) {
    return !IS_UNDEFINED(this.get(key));
  }
};

var observerInfoMap =
    new ObservationWeakMap(observationState.observerInfoMap);
var objectInfoMap = new ObservationWeakMap(observationState.objectInfoMap);
var notifierTargetMap =
    new ObservationWeakMap(observationState.notifierTargetMap);

function CreateObjectInfo(object) {
  var info = {
    changeObservers: new InternalArray,
    notifier: null,
  };
  objectInfoMap.set(object, info);
  return info;
}

function ObjectObserve(object, callback) {
  if (!IS_SPEC_OBJECT(object))
    throw MakeTypeError("observe_non_object", ["observe"]);
  if (!IS_SPEC_FUNCTION(callback))
    throw MakeTypeError("observe_non_function", ["observe"]);
  if (ObjectIsFrozen(callback))
    throw MakeTypeError("observe_callback_frozen");

  if (!observerInfoMap.has(callback)) {
    observerInfoMap.set(callback, {
      pendingChangeRecords: null,
      priority: observationState.observerPriority++,
    });
  }

  var objectInfo = objectInfoMap.get(object);
  if (IS_UNDEFINED(objectInfo)) objectInfo = CreateObjectInfo(object);
  %SetIsObserved(object, true);

  var changeObservers = objectInfo.changeObservers;
  if (changeObservers.indexOf(callback) < 0) changeObservers.push(callback);

  return object;
}

function ObjectUnobserve(object, callback) {
  if (!IS_SPEC_OBJECT(object))
    throw MakeTypeError("observe_non_object", ["unobserve"]);
  if (!IS_SPEC_FUNCTION(callback))
    throw MakeTypeError("observe_non_function", ["unobserve"]);

  var objectInfo = objectInfoMap.get(object);
  if (IS_UNDEFINED(objectInfo))
    return object;

  var changeObservers = objectInfo.changeObservers;
  var index = changeObservers.indexOf(callback);
  if (index >= 0) {
    changeObservers.splice(index, 1);
    if (changeObservers.length === 0) %SetIsObserved(object, false);
  }

  return object;
}

function EnqueueChangeRecord(changeRecord, observers) {
  for (var i = 0; i < observers.length; i++) {
    var observer = observers[i];
    var observerInfo = observerInfoMap.get(observer);
    observationState.pendingObservers[observerInfo.priority] = observer;
    %SetObserverDeliveryPending();
    if (IS_NULL(observerInfo.pendingChangeRecords)) {
      observerInfo.pendingChangeRecords = new InternalArray(changeRecord);
    } else {
      observerInfo.pendingChangeRecords.push(changeRecord);
    }
  }
}

function NotifyChange(type, object, name, oldValue) {
  var objectInfo = objectInfoMap.get(object);
  var changeRecord = (arguments.length < 4) ?
      { type: type, object: object, name: name } :
      { type: type, object: object, name: name, oldValue: oldValue };
  ObjectFreeze(changeRecord);
  EnqueueChangeRecord(changeRecord, objectInfo.changeObservers);
}

var notifierPrototype = {};

function ObjectNotifierNotify(changeRecord) {
  if (!IS_SPEC_OBJECT(this))
    throw MakeTypeError("called_on_non_object", ["notify"]);

  var target = notifierTargetMap.get(this);
  if (IS_UNDEFINED(target))
    throw MakeTypeError("observe_notify_non_notifier");
  if (!IS_STRING(changeRecord.type))
    throw MakeTypeError("observe_type_non_string");

  var objectInfo = objectInfoMap.get(target);
  if (IS_UNDEFINED(objectInfo) || objectInfo.changeObservers.length === 0)
    return;

  var newRecord = { object: target };
  for (var prop in changeRecord) {
    if (prop === 'object') continue;
    %DefineOrRedefineDataProperty(newRecord, prop, changeRecord[prop],
        READ_ONLY + DONT_DELETE);
  }
  ObjectFreeze(newRecord);

  EnqueueChangeRecord(newRecord, objectInfo.changeObservers);
}

function ObjectGetNotifier(object) {
  if (!IS_SPEC_OBJECT(object))
    throw MakeTypeError("observe_non_object", ["getNotifier"]);

  if (ObjectIsFrozen(object)) return null;

  var objectInfo = objectInfoMap.get(object);
  if (IS_UNDEFINED(objectInfo)) objectInfo = CreateObjectInfo(object);

  if (IS_NULL(objectInfo.notifier)) {
    objectInfo.notifier = { __proto__: notifierPrototype };
    notifierTargetMap.set(objectInfo.notifier, object);
  }

  return objectInfo.notifier;
}

function DeliverChangeRecordsForObserver(observer) {
  var observerInfo = observerInfoMap.get(observer);
  if (IS_UNDEFINED(observerInfo))
    return false;

  var pendingChangeRecords = observerInfo.pendingChangeRecords;
  if (IS_NULL(pendingChangeRecords))
    return false;

  observerInfo.pendingChangeRecords = null;
  delete observationState.pendingObservers[observerInfo.priority];
  var delivered = [];
  %MoveArrayContents(pendingChangeRecords, delivered);
  try {
    %Call(void 0, delivered, observer);
  } catch (ex) {}
  return true;
}

function ObjectDeliverChangeRecords(callback) {
  if (!IS_SPEC_FUNCTION(callback))
    throw MakeTypeError("observe_non_function", ["deliverChangeRecords"]);

  while (DeliverChangeRecordsForObserver(callback)) {}
}

function DeliverChangeRecords() {
  while (observationState.pendingObservers.length) {
    var pendingObservers = observationState.pendingObservers;
    observationState.pendingObservers = new InternalArray;
    for (var i in pendingObservers) {
      DeliverChangeRecordsForObserver(pendingObservers[i]);
    }
  }
}

function SetupObjectObserve() {
  %CheckIsBootstrapping();
  InstallFunctions($Object, DONT_ENUM, $Array(
    "deliverChangeRecords", ObjectDeliverChangeRecords,
    "getNotifier", ObjectGetNotifier,
    "observe", ObjectObserve,
    "unobserve", ObjectUnobserve
  ));
  InstallFunctions(notifierPrototype, DONT_ENUM, $Array(
    "notify", ObjectNotifierNotify
  ));
}

SetupObjectObserve();
