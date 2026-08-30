'use strict';

const {
  converters: baseConverters,
  convertToInt,
  createDictionaryConverter,
  createEnumConverter,
  createInterfaceConverter,
  createSequenceConverter,
} = require('internal/webidl');
const { AbortSignal } = require('internal/abort_controller');
const { isUint8Array } = require('internal/util/types');

const converters = { __proto__: null };

function unsignedLongLong(value, options) {
  return convertToInt(value, 64, 'unsigned', options);
}

function enforceRangeUnsignedLongLong(value, options = { __proto__: null }) {
  return convertToInt(value, 64, 'unsigned', {
    __proto__: null,
    prefix: options.prefix,
    context: options.context,
    code: options.code,
    enforceRange: true,
  });
}

function allowStreamBufferOptions(options) {
  return {
    __proto__: null,
    prefix: options.prefix,
    context: options.context,
    code: options.code,
    allowShared: true,
    allowResizable: true,
  };
}

converters.AbortSignal = createInterfaceConverter(
  'AbortSignal', AbortSignal.prototype);
converters.BackpressurePolicy = createEnumConverter('BackpressurePolicy', [
  'strict',
  'unbounded',
  'drop-oldest',
  'drop-newest',
]);
converters.unsignedLongLong = unsignedLongLong;
converters.enforceRangeUnsignedLongLong = enforceRangeUnsignedLongLong;
converters.WriterChunk = (value, options = { __proto__: null }) => {
  if (isUint8Array(value)) {
    return baseConverters.Uint8Array(
      value, allowStreamBufferOptions(options));
  }
  return baseConverters.USVString(value, options);
};
converters.WriterChunkSequence = createSequenceConverter(
  converters.WriterChunk);

const signalMember = {
  __proto__: null,
  key: 'signal',
  converter: converters.AbortSignal,
};
const budgetMember = {
  __proto__: null,
  key: 'budget',
  converter: converters.unsignedLongLong,
};
const backpressureMember = {
  __proto__: null,
  key: 'backpressure',
  converter: converters.BackpressurePolicy,
  defaultValue: () => 'strict',
};
const limitMember = {
  __proto__: null,
  key: 'limit',
  converter: converters.enforceRangeUnsignedLongLong,
};

converters.WriteOptions = createDictionaryConverter('WriteOptions', [
  signalMember,
]);
converters.PushStreamOptions = createDictionaryConverter(
  'PushStreamOptions', [budgetMember, backpressureMember, signalMember]);
converters.PullOptions = createDictionaryConverter('PullOptions', [
  signalMember,
]);
converters.PipeToOptions = createDictionaryConverter('PipeToOptions', [
  {
    __proto__: null,
    key: 'preventClose',
    converter: baseConverters.boolean,
    defaultValue: () => false,
  },
  {
    __proto__: null,
    key: 'preventFail',
    converter: baseConverters.boolean,
    defaultValue: () => false,
  },
  signalMember,
]);
converters.PipeToSyncOptions = createDictionaryConverter(
  'PipeToSyncOptions', [
    {
      __proto__: null,
      key: 'preventClose',
      converter: baseConverters.boolean,
      defaultValue: () => false,
    },
    {
      __proto__: null,
      key: 'preventFail',
      converter: baseConverters.boolean,
      defaultValue: () => false,
    },
  ]);
converters.ConsumeOptions = createDictionaryConverter('ConsumeOptions', [
  limitMember,
  signalMember,
]);
converters.ConsumeSyncOptions = createDictionaryConverter(
  'ConsumeSyncOptions', [limitMember]);
const encodingMember = {
  __proto__: null,
  key: 'encoding',
  converter: baseConverters.DOMString,
  defaultValue: () => 'utf-8',
};
converters.TextConsumeOptions = createDictionaryConverter(
  'TextConsumeOptions', [
    [limitMember, signalMember],
    [encodingMember],
  ]);
converters.TextConsumeSyncOptions = createDictionaryConverter(
  'TextConsumeSyncOptions', [
    [limitMember],
    [encodingMember],
  ]);
converters.MergeOptions = createDictionaryConverter('MergeOptions', [
  signalMember,
]);
converters.BroadcastOptions = createDictionaryConverter(
  'BroadcastOptions', [budgetMember, backpressureMember, signalMember]);
converters.ShareOptions = createDictionaryConverter(
  'ShareOptions', [budgetMember, backpressureMember, signalMember]);
converters.ShareSyncOptions = createDictionaryConverter(
  'ShareSyncOptions', [budgetMember, backpressureMember]);
converters.DuplexDirectionOptions = createDictionaryConverter(
  'DuplexDirectionOptions', [
    budgetMember,
    {
      __proto__: null,
      key: 'backpressure',
      converter: converters.BackpressurePolicy,
    },
  ]);
converters.DuplexOptions = createDictionaryConverter('DuplexOptions', [
  {
    __proto__: null,
    key: 'a',
    converter: converters.DuplexDirectionOptions,
  },
  {
    __proto__: null,
    key: 'b',
    converter: converters.DuplexDirectionOptions,
  },
  budgetMember,
  backpressureMember,
  signalMember,
]);

module.exports = { converters };
