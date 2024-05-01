'use strict';

const {
  Symbol,
  SymbolFor,
} = primordials;

const kFirstEventParam = Symbol('nodejs.kFirstEventParam');
const kInitialEvents = Symbol('kInitialEvents');

const kCapture = Symbol('kCapture');
const kErrorMonitor = Symbol('events.errorMonitor');
const kShapeMode = Symbol('shapeMode');
const kMaxEventTargetListeners = Symbol('events.maxEventTargetListeners');
const kMaxEventTargetListenersWarned =
  Symbol('events.maxEventTargetListenersWarned');
const kWatermarkData = SymbolFor('nodejs.watermarkData');

const kImpl = Symbol('kImpl');
const kIsFastPath = Symbol('kIsFastPath');
const kSwitchToSlowPath = Symbol('kSwitchToSlowPath');
const kRejection = SymbolFor('nodejs.rejection');

module.exports = {
  kFirstEventParam,
  kInitialEvents,
  kCapture,
  kErrorMonitor,
  kShapeMode,
  kMaxEventTargetListeners,
  kMaxEventTargetListenersWarned,
  kWatermarkData,
  kImpl,
  kIsFastPath,
  kSwitchToSlowPath,
  kRejection,
};
