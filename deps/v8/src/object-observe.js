// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

var observationState;

function GetObservationStateJS() {
  if (IS_UNDEFINED(observationState))
    observationState = %GetObservationState();

  if (IS_UNDEFINED(observationState.callbackInfoMap)) {
    observationState.callbackInfoMap = %ObservationWeakMapCreate();
    observationState.objectInfoMap = %ObservationWeakMapCreate();
    observationState.notifierObjectInfoMap = %ObservationWeakMapCreate();
    observationState.pendingObservers = null;
    observationState.nextCallbackPriority = 0;
    observationState.lastMicrotaskId = 0;
  }

  return observationState;
}

function GetWeakMapWrapper() {
  function MapWrapper(map) {
    this.map_ = map;
  };

  MapWrapper.prototype = {
    __proto__: null,
    get: function(key) {
      return %WeakCollectionGet(this.map_, key);
    },
    set: function(key, value) {
      %WeakCollectionSet(this.map_, key, value);
    },
    has: function(key) {
      return !IS_UNDEFINED(this.get(key));
    }
  };

  return MapWrapper;
}

var contextMaps;

function GetContextMaps() {
  if (IS_UNDEFINED(contextMaps)) {
    var map = GetWeakMapWrapper();
    var observationState = GetObservationStateJS();
    contextMaps = {
      callbackInfoMap: new map(observationState.callbackInfoMap),
      objectInfoMap: new map(observationState.objectInfoMap),
      notifierObjectInfoMap: new map(observationState.notifierObjectInfoMap)
    };
  }

  return contextMaps;
}

function GetCallbackInfoMap() {
  return GetContextMaps().callbackInfoMap;
}

function GetObjectInfoMap() {
  return GetContextMaps().objectInfoMap;
}

function GetNotifierObjectInfoMap() {
  return GetContextMaps().notifierObjectInfoMap;
}

function GetPendingObservers() {
  return GetObservationStateJS().pendingObservers;
}

function SetPendingObservers(pendingObservers) {
  GetObservationStateJS().pendingObservers = pendingObservers;
}

function GetNextCallbackPriority() {
  return GetObservationStateJS().nextCallbackPriority++;
}

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

function TypeMapCreateFromList(typeList, length) {
  var typeMap = TypeMapCreate();
  for (var i = 0; i < length; i++) {
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

var defaultAcceptTypes = (function() {
  var defaultTypes = [
    'add',
    'update',
    'delete',
    'setPrototype',
    'reconfigure',
    'preventExtensions'
  ];
  return TypeMapCreateFromList(defaultTypes, defaultTypes.length);
})();

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
  observer.accept = acceptList;
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
    GetObjectInfoMap().set(object, objectInfo);
  }
  return objectInfo;
}

function ObjectInfoGet(object) {
  return GetObjectInfoMap().get(object);
}

function ObjectInfoGetFromNotifier(notifier) {
  return GetNotifierObjectInfoMap().get(notifier);
}

function ObjectInfoGetNotifier(objectInfo) {
  if (IS_NULL(objectInfo.notifier)) {
    objectInfo.notifier = { __proto__: notifierPrototype };
    GetNotifierObjectInfoMap().set(objectInfo.notifier, objectInfo);
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

function ConvertAcceptListToTypeMap(arg) {
  // We use undefined as a sentinel for the default accept list.
  if (IS_UNDEFINED(arg))
    return arg;

  if (!IS_SPEC_OBJECT(arg))
    throw MakeTypeError("observe_accept_invalid");

  var len = ToInteger(arg.length);
  if (len < 0) len = 0;

  return TypeMapCreateFromList(arg, len);
}

// CallbackInfo's optimized state is just a number which represents its global
// priority. When a change record must be enqueued for the callback, it
// normalizes. When delivery clears any pending change records, it re-optimizes.
function CallbackInfoGet(callback) {
  return GetCallbackInfoMap().get(callback);
}

function CallbackInfoGetOrCreate(callback) {
  var callbackInfo = GetCallbackInfoMap().get(callback);
  if (!IS_UNDEFINED(callbackInfo))
    return callbackInfo;

  var priority =  GetNextCallbackPriority();
  GetCallbackInfoMap().set(callback, priority);
  return priority;
}

function CallbackInfoGetPriority(callbackInfo) {
  if (IS_NUMBER(callbackInfo))
    return callbackInfo;
  else
    return callbackInfo.priority;
}

function CallbackInfoNormalize(callback) {
  var callbackInfo = GetCallbackInfoMap().get(callback);
  if (IS_NUMBER(callbackInfo)) {
    var priority = callbackInfo;
    callbackInfo = new InternalArray;
    callbackInfo.priority = priority;
    GetCallbackInfoMap().set(callback, callbackInfo);
  }
  return callbackInfo;
}

function ObjectObserve(object, callback, acceptList) {
  if (!IS_SPEC_OBJECT(object))
    throw MakeTypeError("observe_non_object", ["observe"]);
  if (%IsJSGlobalProxy(object))
    throw MakeTypeError("observe_global_proxy", ["observe"]);
  if (!IS_SPEC_FUNCTION(callback))
    throw MakeTypeError("observe_non_function", ["observe"]);
  if (ObjectIsFrozen(callback))
    throw MakeTypeError("observe_callback_frozen");

  var objectObserveFn = %GetObjectContextObjectObserve(object);
  return objectObserveFn(object, callback, acceptList);
}

function NativeObjectObserve(object, callback, acceptList) {
  var objectInfo = ObjectInfoGetOrCreate(object);
  var typeList = ConvertAcceptListToTypeMap(acceptList);
  ObjectInfoAddObserver(objectInfo, callback, typeList);
  return object;
}

function ObjectUnobserve(object, callback) {
  if (!IS_SPEC_OBJECT(object))
    throw MakeTypeError("observe_non_object", ["unobserve"]);
  if (%IsJSGlobalProxy(object))
    throw MakeTypeError("observe_global_proxy", ["unobserve"]);
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

function ObserverEnqueueIfActive(observer, objectInfo, changeRecord) {
  if (!ObserverIsActive(observer, objectInfo) ||
      !TypeMapHasType(ObserverGetAcceptTypes(observer), changeRecord.type)) {
    return;
  }

  var callback = ObserverGetCallback(observer);
  if (!%ObserverObjectAndRecordHaveSameOrigin(callback, changeRecord.object,
                                              changeRecord)) {
    return;
  }

  var callbackInfo = CallbackInfoNormalize(callback);
  if (IS_NULL(GetPendingObservers())) {
    SetPendingObservers(nullProtoObject());
    if (DEBUG_IS_ACTIVE) {
      var id = ++GetObservationStateJS().lastMicrotaskId;
      var name = "Object.observe";
      %EnqueueMicrotask(function() {
        %DebugAsyncTaskEvent({ type: "willHandle", id: id, name: name });
        ObserveMicrotaskRunner();
        %DebugAsyncTaskEvent({ type: "didHandle", id: id, name: name });
      });
      %DebugAsyncTaskEvent({ type: "enqueue", id: id, name: name });
    } else {
      %EnqueueMicrotask(ObserveMicrotaskRunner);
    }
  }
  GetPendingObservers()[callbackInfo.priority] = callback;
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
    %DefineDataPropertyUnchecked(
        newRecord, prop, changeRecord[prop], READ_ONLY + DONT_DELETE);
  }
  ObjectFreezeJS(newRecord);

  ObjectInfoEnqueueInternalChangeRecord(objectInfo, newRecord);
}

function ObjectInfoEnqueueInternalChangeRecord(objectInfo, changeRecord) {
  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (IS_SYMBOL(changeRecord.name)) return;

  if (ChangeObserversIsOptimized(objectInfo.changeObservers)) {
    var observer = objectInfo.changeObservers;
    ObserverEnqueueIfActive(observer, objectInfo, changeRecord);
    return;
  }

  for (var priority in objectInfo.changeObservers) {
    var observer = objectInfo.changeObservers[priority];
    if (IS_NULL(observer))
      continue;
    ObserverEnqueueIfActive(observer, objectInfo, changeRecord);
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

  ObjectFreezeJS(changeRecord);
  ObjectFreezeJS(changeRecord.removed);
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

  ObjectFreezeJS(changeRecord);
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

  var performChangeFn = %GetObjectContextNotifierPerformChange(objectInfo);
  performChangeFn(objectInfo, changeType, changeFn);
}

function NativeObjectNotifierPerformChange(objectInfo, changeType, changeFn) {
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
  if (%IsJSGlobalProxy(object))
    throw MakeTypeError("observe_global_proxy", ["getNotifier"]);

  if (ObjectIsFrozen(object)) return null;

  if (!%ObjectWasCreatedInCurrentOrigin(object)) return null;

  var getNotifierFn = %GetObjectContextObjectGetNotifier(object);
  return getNotifierFn(object);
}

function NativeObjectGetNotifier(object) {
  var objectInfo = ObjectInfoGetOrCreate(object);
  return ObjectInfoGetNotifier(objectInfo);
}

function CallbackDeliverPending(callback) {
  var callbackInfo = GetCallbackInfoMap().get(callback);
  if (IS_UNDEFINED(callbackInfo) || IS_NUMBER(callbackInfo))
    return false;

  // Clear the pending change records from callback and return it to its
  // "optimized" state.
  var priority = callbackInfo.priority;
  GetCallbackInfoMap().set(callback, priority);

  if (GetPendingObservers())
    delete GetPendingObservers()[priority];

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
  var pendingObservers = GetPendingObservers();
  if (pendingObservers) {
    SetPendingObservers(null);
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
