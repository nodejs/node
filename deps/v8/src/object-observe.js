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

// Overview:
//
// This file contains all of the routing and accounting for Object.observe.
// User code will interact with these mechanisms via the Object.observe APIs
// and, as a side effect of mutation objects which are observed. The V8 runtime
// (both C++ and JS) will interact with these mechanisms primarily by enqueuing
// proper change records for objects which were mutated. The Object.observe
// routing and accounting consists primarily of three participants
//
// 1) ObjectInfo. This represents the observed state of a given object. It
//    records what callbacks are observing the object, with what options, and
//    what "change types" are in progress on the object (i.e. via
//    notifier.performChange).
//
// 2) CallbackInfo. This represents a callback used for observation. It holds
//    the records which must be delivered to the callback, as well as the global
//    priority of the callback (which determines delivery order between
//    callbacks).
//
// 3) observationState.pendingObservers. This is the set of observers which
//    have change records which must be delivered. During "normal" delivery
//    (i.e. not Object.deliverChangeRecords), this is the mechanism by which
//    callbacks are invoked in the proper order until there are no more
//    change records pending to a callback.
//
// Note that in order to reduce allocation and processing costs, the
// implementation of (1) and (2) have "optimized" states which represent
// common cases which can be handled more efficiently.

var observationState = %GetObservationState();
if (IS_UNDEFINED(observationState.callbackInfoMap)) {
  observationState.callbackInfoMap = %ObservationWeakMapCreate();
  observationState.objectInfoMap = %ObservationWeakMapCreate();
  observationState.notifierObjectInfoMap = %ObservationWeakMapCreate();
  observationState.pendingObservers = null;
  observationState.nextCallbackPriority = 0;
}

function ObservationWeakMap(map) {
  this.map_ = map;
}

ObservationWeakMap.prototype = {
  get: function(key) {
    key = %UnwrapGlobalProxy(key);
    if (!IS_SPEC_OBJECT(key)) return UNDEFINED;
    return %WeakCollectionGet(this.map_, key);
  },
  set: function(key, value) {
    key = %UnwrapGlobalProxy(key);
    if (!IS_SPEC_OBJECT(key)) return UNDEFINED;
    %WeakCollectionSet(this.map_, key, value);
  },
  has: function(key) {
    return !IS_UNDEFINED(this.get(key));
  }
};

var callbackInfoMap =
    new ObservationWeakMap(observationState.callbackInfoMap);
var objectInfoMap = new ObservationWeakMap(observationState.objectInfoMap);
var notifierObjectInfoMap =
    new ObservationWeakMap(observationState.notifierObjectInfoMap);

function nullProtoObject() {
  return { __proto__: null };
}

function TypeMapCreate() {
  return nullProtoObject();
}

function TypeMapAddType(typeMap, type, ignoreDuplicate) {
  typeMap[type] = ignoreDuplicate ? 1 : (typeMap[type] || 0) + 1;
}

function TypeMapRemoveType(typeMap, type) {
  typeMap[type]--;
}

function TypeMapCreateFromList(typeList) {
  var typeMap = TypeMapCreate();
  for (var i = 0; i < typeList.length; i++) {
    TypeMapAddType(typeMap, typeList[i], true);
  }
  return typeMap;
}

function TypeMapHasType(typeMap, type) {
  return !!typeMap[type];
}

function TypeMapIsDisjointFrom(typeMap1, typeMap2) {
  if (!typeMap1 || !typeMap2)
    return true;

  for (var type in typeMap1) {
    if (TypeMapHasType(typeMap1, type) && TypeMapHasType(typeMap2, type))
      return false;
  }

  return true;
}

var defaultAcceptTypes = TypeMapCreateFromList([
  'add',
  'update',
  'delete',
  'setPrototype',
  'reconfigure',
  'preventExtensions'
]);

// An Observer is a registration to observe an object by a callback with
// a given set of accept types. If the set of accept types is the default
// set for Object.observe, the observer is represented as a direct reference
// to the callback. An observer never changes its accept types and thus never
// needs to "normalize".
function ObserverCreate(callback, acceptList) {
  if (IS_UNDEFINED(acceptList))
    return callback;
  var observer = nullProtoObject();
  observer.callback = callback;
  observer.accept = TypeMapCreateFromList(acceptList);
  return observer;
}

function ObserverGetCallback(observer) {
  return IS_SPEC_FUNCTION(observer) ? observer : observer.callback;
}

function ObserverGetAcceptTypes(observer) {
  return IS_SPEC_FUNCTION(observer) ? defaultAcceptTypes : observer.accept;
}

function ObserverIsActive(observer, objectInfo) {
  return TypeMapIsDisjointFrom(ObjectInfoGetPerformingTypes(objectInfo),
                               ObserverGetAcceptTypes(observer));
}

function ObjectInfoGetOrCreate(object) {
  var objectInfo = ObjectInfoGet(object);
  if (IS_UNDEFINED(objectInfo)) {
    if (!%IsJSProxy(object))
      %SetIsObserved(object);

    objectInfo = {
      object: object,
      changeObservers: null,
      notifier: null,
      performing: null,
      performingCount: 0,
    };
    objectInfoMap.set(object, objectInfo);
  }
  return objectInfo;
}

function ObjectInfoGet(object) {
  return objectInfoMap.get(object);
}

function ObjectInfoGetFromNotifier(notifier) {
  return notifierObjectInfoMap.get(notifier);
}

function ObjectInfoGetNotifier(objectInfo) {
  if (IS_NULL(objectInfo.notifier)) {
    objectInfo.notifier = { __proto__: notifierPrototype };
    notifierObjectInfoMap.set(objectInfo.notifier, objectInfo);
  }

  return objectInfo.notifier;
}

function ObjectInfoGetObject(objectInfo) {
  return objectInfo.object;
}

function ChangeObserversIsOptimized(changeObservers) {
  return typeof changeObservers === 'function' ||
         typeof changeObservers.callback === 'function';
}

// The set of observers on an object is called 'changeObservers'. The first
// observer is referenced directly via objectInfo.changeObservers. When a second
// is added, changeObservers "normalizes" to become a mapping of callback
// priority -> observer and is then stored on objectInfo.changeObservers.
function ObjectInfoNormalizeChangeObservers(objectInfo) {
  if (ChangeObserversIsOptimized(objectInfo.changeObservers)) {
    var observer = objectInfo.changeObservers;
    var callback = ObserverGetCallback(observer);
    var callbackInfo = CallbackInfoGet(callback);
    var priority = CallbackInfoGetPriority(callbackInfo);
    objectInfo.changeObservers = nullProtoObject();
    objectInfo.changeObservers[priority] = observer;
  }
}

function ObjectInfoAddObserver(objectInfo, callback, acceptList) {
  var callbackInfo = CallbackInfoGetOrCreate(callback);
  var observer = ObserverCreate(callback, acceptList);

  if (!objectInfo.changeObservers) {
    objectInfo.changeObservers = observer;
    return;
  }

  ObjectInfoNormalizeChangeObservers(objectInfo);
  var priority = CallbackInfoGetPriority(callbackInfo);
  objectInfo.changeObservers[priority] = observer;
}

function ObjectInfoRemoveObserver(objectInfo, callback) {
  if (!objectInfo.changeObservers)
    return;

  if (ChangeObserversIsOptimized(objectInfo.changeObservers)) {
    if (callback === ObserverGetCallback(objectInfo.changeObservers))
      objectInfo.changeObservers = null;
    return;
  }

  var callbackInfo = CallbackInfoGet(callback);
  var priority = CallbackInfoGetPriority(callbackInfo);
  objectInfo.changeObservers[priority] = null;
}

function ObjectInfoHasActiveObservers(objectInfo) {
  if (IS_UNDEFINED(objectInfo) || !objectInfo.changeObservers)
    return false;

  if (ChangeObserversIsOptimized(objectInfo.changeObservers))
    return ObserverIsActive(objectInfo.changeObservers, objectInfo);

  for (var priority in objectInfo.changeObservers) {
    var observer = objectInfo.changeObservers[priority];
    if (!IS_NULL(observer) && ObserverIsActive(observer, objectInfo))
      return true;
  }

  return false;
}

function ObjectInfoAddPerformingType(objectInfo, type) {
  objectInfo.performing = objectInfo.performing || TypeMapCreate();
  TypeMapAddType(objectInfo.performing, type);
  objectInfo.performingCount++;
}

function ObjectInfoRemovePerformingType(objectInfo, type) {
  objectInfo.performingCount--;
  TypeMapRemoveType(objectInfo.performing, type);
}

function ObjectInfoGetPerformingTypes(objectInfo) {
  return objectInfo.performingCount > 0 ? objectInfo.performing : null;
}

function AcceptArgIsValid(arg) {
  if (IS_UNDEFINED(arg))
    return true;

  if (!IS_SPEC_OBJECT(arg) ||
      !IS_NUMBER(arg.length) ||
      arg.length < 0)
    return false;

  return true;
}

// CallbackInfo's optimized state is just a number which represents its global
// priority. When a change record must be enqueued for the callback, it
// normalizes. When delivery clears any pending change records, it re-optimizes.
function CallbackInfoGet(callback) {
  return callbackInfoMap.get(callback);
}

function CallbackInfoGetOrCreate(callback) {
  var callbackInfo = callbackInfoMap.get(callback);
  if (!IS_UNDEFINED(callbackInfo))
    return callbackInfo;

  var priority = observationState.nextCallbackPriority++
  callbackInfoMap.set(callback, priority);
  return priority;
}

function CallbackInfoGetPriority(callbackInfo) {
  if (IS_NUMBER(callbackInfo))
    return callbackInfo;
  else
    return callbackInfo.priority;
}

function CallbackInfoNormalize(callback) {
  var callbackInfo = callbackInfoMap.get(callback);
  if (IS_NUMBER(callbackInfo)) {
    var priority = callbackInfo;
    callbackInfo = new InternalArray;
    callbackInfo.priority = priority;
    callbackInfoMap.set(callback, callbackInfo);
  }
  return callbackInfo;
}

function ObjectObserve(object, callback, acceptList) {
  if (!IS_SPEC_OBJECT(object))
    throw MakeTypeError("observe_non_object", ["observe"]);
  if (!IS_SPEC_FUNCTION(callback))
    throw MakeTypeError("observe_non_function", ["observe"]);
  if (ObjectIsFrozen(callback))
    throw MakeTypeError("observe_callback_frozen");
  if (!AcceptArgIsValid(acceptList))
    throw MakeTypeError("observe_accept_invalid");

  var objectInfo = ObjectInfoGetOrCreate(object);
  ObjectInfoAddObserver(objectInfo, callback, acceptList);
  return object;
}

function ObjectUnobserve(object, callback) {
  if (!IS_SPEC_OBJECT(object))
    throw MakeTypeError("observe_non_object", ["unobserve"]);
  if (!IS_SPEC_FUNCTION(callback))
    throw MakeTypeError("observe_non_function", ["unobserve"]);

  var objectInfo = ObjectInfoGet(object);
  if (IS_UNDEFINED(objectInfo))
    return object;

  ObjectInfoRemoveObserver(objectInfo, callback);
  return object;
}

function ArrayObserve(object, callback) {
  return ObjectObserve(object, callback, ['add',
                                          'update',
                                          'delete',
                                          'splice']);
}

function ArrayUnobserve(object, callback) {
  return ObjectUnobserve(object, callback);
}

function ObserverEnqueueIfActive(observer, objectInfo, changeRecord,
                                 needsAccessCheck) {
  if (!ObserverIsActive(observer, objectInfo) ||
      !TypeMapHasType(ObserverGetAcceptTypes(observer), changeRecord.type)) {
    return;
  }

  var callback = ObserverGetCallback(observer);
  if (needsAccessCheck &&
      // Drop all splice records on the floor for access-checked objects
      (changeRecord.type == 'splice' ||
       !%IsAccessAllowedForObserver(
           callback, changeRecord.object, changeRecord.name))) {
    return;
  }

  var callbackInfo = CallbackInfoNormalize(callback);
  if (IS_NULL(observationState.pendingObservers)) {
    observationState.pendingObservers = nullProtoObject();
    GetMicrotaskQueue().push(ObserveMicrotaskRunner);
    %SetMicrotaskPending(true);
  }
  observationState.pendingObservers[callbackInfo.priority] = callback;
  callbackInfo.push(changeRecord);
}

function ObjectInfoEnqueueExternalChangeRecord(objectInfo, changeRecord, type) {
  if (!ObjectInfoHasActiveObservers(objectInfo))
    return;

  var hasType = !IS_UNDEFINED(type);
  var newRecord = hasType ?
      { object: ObjectInfoGetObject(objectInfo), type: type } :
      { object: ObjectInfoGetObject(objectInfo) };

  for (var prop in changeRecord) {
    if (prop === 'object' || (hasType && prop === 'type')) continue;
    %DefineOrRedefineDataProperty(newRecord, prop, changeRecord[prop],
        READ_ONLY + DONT_DELETE);
  }
  ObjectFreeze(newRecord);

  ObjectInfoEnqueueInternalChangeRecord(objectInfo, newRecord,
                                        true /* skip access check */);
}

function ObjectInfoEnqueueInternalChangeRecord(objectInfo, changeRecord,
                                               skipAccessCheck) {
  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (IS_SYMBOL(changeRecord.name)) return;

  var needsAccessCheck = !skipAccessCheck &&
      %IsAccessCheckNeeded(changeRecord.object);

  if (ChangeObserversIsOptimized(objectInfo.changeObservers)) {
    var observer = objectInfo.changeObservers;
    ObserverEnqueueIfActive(observer, objectInfo, changeRecord,
                            needsAccessCheck);
    return;
  }

  for (var priority in objectInfo.changeObservers) {
    var observer = objectInfo.changeObservers[priority];
    if (IS_NULL(observer))
      continue;
    ObserverEnqueueIfActive(observer, objectInfo, changeRecord,
                            needsAccessCheck);
  }
}

function BeginPerformSplice(array) {
  var objectInfo = ObjectInfoGet(array);
  if (!IS_UNDEFINED(objectInfo))
    ObjectInfoAddPerformingType(objectInfo, 'splice');
}

function EndPerformSplice(array) {
  var objectInfo = ObjectInfoGet(array);
  if (!IS_UNDEFINED(objectInfo))
    ObjectInfoRemovePerformingType(objectInfo, 'splice');
}

function EnqueueSpliceRecord(array, index, removed, addedCount) {
  var objectInfo = ObjectInfoGet(array);
  if (!ObjectInfoHasActiveObservers(objectInfo))
    return;

  var changeRecord = {
    type: 'splice',
    object: array,
    index: index,
    removed: removed,
    addedCount: addedCount
  };

  ObjectFreeze(changeRecord);
  ObjectFreeze(changeRecord.removed);
  ObjectInfoEnqueueInternalChangeRecord(objectInfo, changeRecord);
}

function NotifyChange(type, object, name, oldValue) {
  var objectInfo = ObjectInfoGet(object);
  if (!ObjectInfoHasActiveObservers(objectInfo))
    return;

  var changeRecord;
  if (arguments.length == 2) {
    changeRecord = { type: type, object: object };
  } else if (arguments.length == 3) {
    changeRecord = { type: type, object: object, name: name };
  } else {
    changeRecord = {
      type: type,
      object: object,
      name: name,
      oldValue: oldValue
    };
  }

  ObjectFreeze(changeRecord);
  ObjectInfoEnqueueInternalChangeRecord(objectInfo, changeRecord);
}

var notifierPrototype = {};

function ObjectNotifierNotify(changeRecord) {
  if (!IS_SPEC_OBJECT(this))
    throw MakeTypeError("called_on_non_object", ["notify"]);

  var objectInfo = ObjectInfoGetFromNotifier(this);
  if (IS_UNDEFINED(objectInfo))
    throw MakeTypeError("observe_notify_non_notifier");
  if (!IS_STRING(changeRecord.type))
    throw MakeTypeError("observe_type_non_string");

  ObjectInfoEnqueueExternalChangeRecord(objectInfo, changeRecord);
}

function ObjectNotifierPerformChange(changeType, changeFn) {
  if (!IS_SPEC_OBJECT(this))
    throw MakeTypeError("called_on_non_object", ["performChange"]);

  var objectInfo = ObjectInfoGetFromNotifier(this);

  if (IS_UNDEFINED(objectInfo))
    throw MakeTypeError("observe_notify_non_notifier");
  if (!IS_STRING(changeType))
    throw MakeTypeError("observe_perform_non_string");
  if (!IS_SPEC_FUNCTION(changeFn))
    throw MakeTypeError("observe_perform_non_function");

  ObjectInfoAddPerformingType(objectInfo, changeType);

  var changeRecord;
  try {
    changeRecord = %_CallFunction(UNDEFINED, changeFn);
  } finally {
    ObjectInfoRemovePerformingType(objectInfo, changeType);
  }

  if (IS_SPEC_OBJECT(changeRecord))
    ObjectInfoEnqueueExternalChangeRecord(objectInfo, changeRecord, changeType);
}

function ObjectGetNotifier(object) {
  if (!IS_SPEC_OBJECT(object))
    throw MakeTypeError("observe_non_object", ["getNotifier"]);

  if (ObjectIsFrozen(object)) return null;

  var objectInfo = ObjectInfoGetOrCreate(object);
  return ObjectInfoGetNotifier(objectInfo);
}

function CallbackDeliverPending(callback) {
  var callbackInfo = callbackInfoMap.get(callback);
  if (IS_UNDEFINED(callbackInfo) || IS_NUMBER(callbackInfo))
    return false;

  // Clear the pending change records from callback and return it to its
  // "optimized" state.
  var priority = callbackInfo.priority;
  callbackInfoMap.set(callback, priority);

  if (observationState.pendingObservers)
    delete observationState.pendingObservers[priority];

  var delivered = [];
  %MoveArrayContents(callbackInfo, delivered);

  try {
    %_CallFunction(UNDEFINED, delivered, callback);
  } catch (ex) {}  // TODO(rossberg): perhaps log uncaught exceptions.
  return true;
}

function ObjectDeliverChangeRecords(callback) {
  if (!IS_SPEC_FUNCTION(callback))
    throw MakeTypeError("observe_non_function", ["deliverChangeRecords"]);

  while (CallbackDeliverPending(callback)) {}
}

function ObserveMicrotaskRunner() {
  var pendingObservers = observationState.pendingObservers;
  if (pendingObservers) {
    observationState.pendingObservers = null;
    for (var i in pendingObservers) {
      CallbackDeliverPending(pendingObservers[i]);
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
