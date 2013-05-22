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
    inactiveObservers: new InternalArray,
    performing: { __proto__: null },
    performingCount: 0,
  };
  objectInfoMap.set(object, info);
  return info;
}

var defaultAcceptTypes = {
  __proto__: null,
  'new': true,
  'updated': true,
  'deleted': true,
  'prototype': true,
  'reconfigured': true
};

function CreateObserver(callback, accept) {
  var observer = {
    __proto__: null,
    callback: callback,
    accept: defaultAcceptTypes
  };

  if (IS_UNDEFINED(accept))
    return observer;

  var acceptMap = { __proto__: null };
  for (var i = 0; i < accept.length; i++)
    acceptMap[accept[i]] = true;

  observer.accept = acceptMap;
  return observer;
}

function ObserverIsActive(observer, objectInfo) {
  if (objectInfo.performingCount === 0)
    return true;

  var performing = objectInfo.performing;
  for (var type in performing) {
    if (performing[type] > 0 && observer.accept[type])
      return false;
  }

  return true;
}

function ObserverIsInactive(observer, objectInfo) {
  return !ObserverIsActive(observer, objectInfo);
}

function RemoveNullElements(from) {
  var i = 0;
  var j = 0;
  for (; i < from.length; i++) {
    if (from[i] === null)
      continue;
    if (j < i)
      from[j] = from[i];
    j++;
  }

  if (i !== j)
    from.length = from.length - (i - j);
}

function RepartitionObservers(conditionFn, from, to, objectInfo) {
  var anyRemoved = false;
  for (var i = 0; i < from.length; i++) {
    var observer = from[i];
    if (conditionFn(observer, objectInfo)) {
      anyRemoved = true;
      from[i] = null;
      to.push(observer);
    }
  }

  if (anyRemoved)
    RemoveNullElements(from);
}

function BeginPerformChange(objectInfo, type) {
  objectInfo.performing[type] = (objectInfo.performing[type] || 0) + 1;
  objectInfo.performingCount++;
  RepartitionObservers(ObserverIsInactive,
                       objectInfo.changeObservers,
                       objectInfo.inactiveObservers,
                       objectInfo);
}

function EndPerformChange(objectInfo, type) {
  objectInfo.performing[type]--;
  objectInfo.performingCount--;
  RepartitionObservers(ObserverIsActive,
                       objectInfo.inactiveObservers,
                       objectInfo.changeObservers,
                       objectInfo);
}

function EnsureObserverRemoved(objectInfo, callback) {
  function remove(observerList) {
    for (var i = 0; i < observerList.length; i++) {
      if (observerList[i].callback === callback) {
        observerList.splice(i, 1);
        return true;
      }
    }
    return false;
  }

  if (!remove(objectInfo.changeObservers))
    remove(objectInfo.inactiveObservers);
}

function AcceptArgIsValid(arg) {
  if (IS_UNDEFINED(arg))
    return true;

  if (!IS_SPEC_OBJECT(arg) ||
      !IS_NUMBER(arg.length) ||
      arg.length < 0)
    return false;

  var length = arg.length;
  for (var i = 0; i < length; i++) {
    if (!IS_STRING(arg[i]))
      return false;
  }
  return true;
}

function ObjectObserve(object, callback, accept) {
  if (!IS_SPEC_OBJECT(object))
    throw MakeTypeError("observe_non_object", ["observe"]);
  if (!IS_SPEC_FUNCTION(callback))
    throw MakeTypeError("observe_non_function", ["observe"]);
  if (ObjectIsFrozen(callback))
    throw MakeTypeError("observe_callback_frozen");
  if (!AcceptArgIsValid(accept))
    throw MakeTypeError("observe_accept_invalid");

  if (!observerInfoMap.has(callback)) {
    observerInfoMap.set(callback, {
      pendingChangeRecords: null,
      priority: observationState.observerPriority++,
    });
  }

  var objectInfo = objectInfoMap.get(object);
  if (IS_UNDEFINED(objectInfo)) objectInfo = CreateObjectInfo(object);
  %SetIsObserved(object, true);

  EnsureObserverRemoved(objectInfo, callback);

  var observer = CreateObserver(callback, accept);
  if (ObserverIsActive(observer, objectInfo))
    objectInfo.changeObservers.push(observer);
  else
    objectInfo.inactiveObservers.push(observer);

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

  EnsureObserverRemoved(objectInfo, callback);

  if (objectInfo.changeObservers.length === 0 &&
      objectInfo.inactiveObservers.length === 0) {
    %SetIsObserved(object, false);
  }

  return object;
}

function ArrayObserve(object, callback) {
  return ObjectObserve(object, callback, ['new',
                                          'updated',
                                          'deleted',
                                          'splice']);
}

function ArrayUnobserve(object, callback) {
  return ObjectUnobserve(object, callback);
}

function EnqueueChangeRecord(changeRecord, observers) {
  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (IS_SYMBOL(changeRecord.name)) return;

  for (var i = 0; i < observers.length; i++) {
    var observer = observers[i];
    if (IS_UNDEFINED(observer.accept[changeRecord.type]))
      continue;

    var callback = observer.callback;
    var observerInfo = observerInfoMap.get(callback);
    observationState.pendingObservers[observerInfo.priority] = callback;
    %SetObserverDeliveryPending();
    if (IS_NULL(observerInfo.pendingChangeRecords)) {
      observerInfo.pendingChangeRecords = new InternalArray(changeRecord);
    } else {
      observerInfo.pendingChangeRecords.push(changeRecord);
    }
  }
}

function BeginPerformSplice(array) {
  var objectInfo = objectInfoMap.get(array);
  if (!IS_UNDEFINED(objectInfo))
    BeginPerformChange(objectInfo, 'splice');
}

function EndPerformSplice(array) {
  var objectInfo = objectInfoMap.get(array);
  if (!IS_UNDEFINED(objectInfo))
    EndPerformChange(objectInfo, 'splice');
}

function EnqueueSpliceRecord(array, index, removed, deleteCount, addedCount) {
  var objectInfo = objectInfoMap.get(array);
  if (IS_UNDEFINED(objectInfo) || objectInfo.changeObservers.length === 0)
    return;

  var changeRecord = {
    type: 'splice',
    object: array,
    index: index,
    removed: removed,
    addedCount: addedCount
  };

  changeRecord.removed.length = deleteCount;
  // TODO(rafaelw): This breaks spec-compliance. Re-enable when freezing isn't
  // slow.
  // ObjectFreeze(changeRecord);
  // ObjectFreeze(changeRecord.removed);
  EnqueueChangeRecord(changeRecord, objectInfo.changeObservers);
}

function NotifyChange(type, object, name, oldValue) {
  var objectInfo = objectInfoMap.get(object);
  if (objectInfo.changeObservers.length === 0)
    return;

  var changeRecord = (arguments.length < 4) ?
      { type: type, object: object, name: name } :
      { type: type, object: object, name: name, oldValue: oldValue };
  // TODO(rafaelw): This breaks spec-compliance. Re-enable when freezing isn't
  // slow.
  // ObjectFreeze(changeRecord);
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
  // TODO(rafaelw): This breaks spec-compliance. Re-enable when freezing isn't
  // slow.
  // ObjectFreeze(newRecord);

  EnqueueChangeRecord(newRecord, objectInfo.changeObservers);
}

function ObjectNotifierPerformChange(changeType, changeFn, receiver) {
  if (!IS_SPEC_OBJECT(this))
    throw MakeTypeError("called_on_non_object", ["performChange"]);

  var target = notifierTargetMap.get(this);
  if (IS_UNDEFINED(target))
    throw MakeTypeError("observe_notify_non_notifier");
  if (!IS_STRING(changeType))
    throw MakeTypeError("observe_perform_non_string");
  if (!IS_SPEC_FUNCTION(changeFn))
    throw MakeTypeError("observe_perform_non_function");

  if (IS_NULL_OR_UNDEFINED(receiver)) {
    receiver = %GetDefaultReceiver(changeFn) || receiver;
  } else if (!IS_SPEC_OBJECT(receiver) && %IsClassicModeFunction(changeFn)) {
    receiver = ToObject(receiver);
  }

  var objectInfo = objectInfoMap.get(target);
  if (IS_UNDEFINED(objectInfo))
    return;

  BeginPerformChange(objectInfo, changeType);
  try {
    %_CallFunction(receiver, changeFn);
  } finally {
    EndPerformChange(objectInfo, changeType);
  }
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
  InstallFunctions($Array, DONT_ENUM, $Array(
    "observe", ArrayObserve,
    "unobserve", ArrayUnobserve
  ));
  InstallFunctions(notifierPrototype, DONT_ENUM, $Array(
    "notify", ObjectNotifierNotify,
    "performChange", ObjectNotifierPerformChange
  ));
}

SetupObjectObserve();
