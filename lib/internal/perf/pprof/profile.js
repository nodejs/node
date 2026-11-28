// Copyright 2022 Datadog, Inc.
// This product includes software developed at Datadog
// (https://www.datadoghq.com/).
//
// Modified for inclusion in Node.js core: ported from the encoding half of
// https://github.com/DataDog/pprof-format/blob/53548398f38b16eb096cc04a75927316a68a9b54/src/index.ts
//
// SPDX-License-Identifier: MIT

'use strict';

// The messages of pprof's profile.proto:
// https://github.com/google/pprof/blob/084b33a85f63291f0513dc6a6284e5dfa66c8e3a/proto/profile.proto

const {
  ArrayPrototypeMap,
} = primordials;

const { FastBuffer } = require('internal/buffer');
const {
  Message,
  encodeBoolField,
  encodeMessageArrayField,
  encodeMessageField,
  encodeNumberField,
  encodePackedNumbersField,
  kEncodeToBuffer,
  kMeasure,
  measureBoolField,
  measureMessageArrayField,
  measureMessageField,
  measureNumberField,
  measurePackedNumbersField,
  newEncodeContext,
  verifyEncodedLength,
  yieldToEventLoop,
} = require('internal/perf/pprof/protobuf');
const { StringTable } = require('internal/perf/pprof/string_table');

// Field numbers, as declared in profile.proto.
const kValueTypeType = 1;
const kValueTypeUnit = 2;

const kLabelKey = 1;
const kLabelStr = 2;
const kLabelNum = 3;
const kLabelNumUnit = 4;

const kSampleLocationId = 1;
const kSampleValue = 2;
const kSampleLabel = 3;

const kMappingId = 1;
const kMappingMemoryStart = 2;
const kMappingMemoryLimit = 3;
const kMappingFileOffset = 4;
const kMappingFilename = 5;
const kMappingBuildId = 6;
const kMappingHasFunctions = 7;
const kMappingHasFilenames = 8;
const kMappingHasLineNumbers = 9;
const kMappingHasInlineFrames = 10;

const kLineFunctionId = 1;
const kLineLine = 2;
const kLineColumn = 3;

const kLocationId = 1;
const kLocationMappingId = 2;
const kLocationAddress = 3;
const kLocationLine = 4;
const kLocationIsFolded = 5;

const kFunctionId = 1;
const kFunctionName = 2;
const kFunctionSystemName = 3;
const kFunctionFilename = 4;
const kFunctionStartLine = 5;

const kProfileSampleType = 1;
const kProfileSample = 2;
const kProfileMapping = 3;
const kProfileLocation = 4;
const kProfileFunction = 5;
const kProfileStringTable = 6;
const kProfileDropFrames = 7;
const kProfileKeepFrames = 8;
const kProfileTimeNanos = 9;
const kProfileDurationNanos = 10;
const kProfilePeriodType = 11;
const kProfilePeriod = 12;
const kProfileComment = 13;
const kProfileDefaultSampleType = 14;
const kProfileDocUrl = 15;

/**
 * The name and unit of a number in a profile. Each sample of a profile holds
 * a list of numbers, and the profile's `sampleType` holds one `ValueType` per
 * number, in the same order, which says what that number is. For example, a
 * time profile has the sample types `sample` in `count`, `wall` in
 * `nanoseconds` and, with CPU time, `cpu` in `nanoseconds`: a sample with
 * the values `[3, 3000000, 2500000]` was seen 3 times, which accounts for
 * 3 ms of wall time and 2.5 ms of CPU time. The profile's `periodType` is
 * also a `ValueType`, which gives the name and unit of its `period`.
 * `type` and `unit` are indices in the profile's string table.
 */
class ValueType extends Message {
  /**
   * Returns `data` if it is a `ValueType`, or creates one from it, so that
   * messages and plain objects can be mixed in the fields of other messages.
   * @param {ValueType|object} data A `ValueType`, or an object with the fields of one.
   * @returns {ValueType} `data`, or a new `ValueType` with the fields of `data`.
   */
  static create(data) {
    return data instanceof ValueType ? data : new ValueType(data);
  }

  /**
   * Creates a `ValueType` message: the type and unit of a value.
   * @param {object} data The fields of the message. Missing fields take their
   *   default value.
   */
  constructor(data) {
    super();
    this.type = data.type || 0;
    this.unit = data.unit || 0;
  }

  /**
   * Measures the message, without its tag and length.
   * @returns {number} The number of bytes that the message takes.
   */
  [kMeasure]() {
    return measureNumberField(kValueTypeType, this.type) +
      measureNumberField(kValueTypeUnit, this.unit);
  }

  /**
   * Encodes the message, without its tag and length.
   * @param {Buffer} buffer The buffer to write the message to, which is at
   *   least as large as measured.
   * @param {number} offset The offset at which to write the message.
   * @returns {number} The offset after the message.
   */
  [kEncodeToBuffer](buffer, offset) {
    offset = encodeNumberField(buffer, offset, kValueTypeType, this.type);
    return encodeNumberField(buffer, offset, kValueTypeUnit, this.unit);
  }
}

/**
 * A key-value pair that annotates a sample with information that is not part
 * of its stack, such as the ID of the thread or of the request that was being
 * handled when the sample was taken. Tools use labels to filter and group
 * samples. `key` is the index of the key in the profile's string table, and
 * the value is either a string, as the index `str` in the string table, or a
 * number, `num`, whose unit is named by the index `numUnit`. The time
 * profiler does not create labels.
 */
class Label extends Message {
  /**
   * Returns `data` if it is a `Label`, or creates one from it, so that
   * messages and plain objects can be mixed in the fields of other messages.
   * @param {Label|object} data A `Label`, or an object with the fields of one.
   * @returns {Label} `data`, or a new `Label` with the fields of `data`.
   */
  static create(data) {
    return data instanceof Label ? data : new Label(data);
  }

  /**
   * Creates a `Label` message: a key and a string or numeric value attached to
   * a sample.
   * @param {object} data The fields of the message. Missing fields take their
   *   default value.
   */
  constructor(data) {
    super();
    this.key = data.key || 0;
    this.str = data.str || 0;
    this.num = data.num || 0;
    this.numUnit = data.numUnit || 0;
  }

  /**
   * Measures the message, without its tag and length.
   * @returns {number} The number of bytes that the message takes.
   */
  [kMeasure]() {
    return measureNumberField(kLabelKey, this.key) +
      measureNumberField(kLabelStr, this.str) +
      measureNumberField(kLabelNum, this.num) +
      measureNumberField(kLabelNumUnit, this.numUnit);
  }

  /**
   * Encodes the message, without its tag and length.
   * @param {Buffer} buffer The buffer to write the message to, which is at
   *   least as large as measured.
   * @param {number} offset The offset at which to write the message.
   * @returns {number} The offset after the message.
   */
  [kEncodeToBuffer](buffer, offset) {
    offset = encodeNumberField(buffer, offset, kLabelKey, this.key);
    offset = encodeNumberField(buffer, offset, kLabelStr, this.str);
    offset = encodeNumberField(buffer, offset, kLabelNum, this.num);
    return encodeNumberField(buffer, offset, kLabelNumUnit, this.numUnit);
  }
}

/**
 * One measurement of a profile: a stack trace, and the values recorded while
 * the program was executing it. For the time profiler, a sample aggregates
 * all the times that the sampler found the thread in the same stack, and its
 * values are the number of times, the wall time that they account for and,
 * optionally, the CPU time. `locationId` lists the IDs of the `Location`s of
 * the stack, leaf (the function that was executing) first. `value` holds one
 * value per `ValueType` of the profile's `sampleType`, in the same order.
 * `label` holds the `Label`s that annotate the sample.
 */
class Sample extends Message {
  /**
   * Returns `data` if it is a `Sample`, or creates one from it, so that
   * messages and plain objects can be mixed in the fields of other messages.
   * @param {Sample|object} data A `Sample`, or an object with the fields of one.
   * @returns {Sample} `data`, or a new `Sample` with the fields of `data`.
   */
  static create(data) {
    return data instanceof Sample ? data : new Sample(data);
  }

  /**
   * Creates a `Sample` message: a stack and the values recorded for it.
   * @param {object} data The fields of the message. Missing fields take their
   *   default value.
   */
  constructor(data) {
    super();
    this.locationId = data.locationId || [];
    this.value = data.value || [];
    this.label = ArrayPrototypeMap(data.label || [], Label.create);
  }

  /**
   * Measures the message, without its tag and length.
   * @param {EncodeContext} context The context of the encoding, which the
   *   payload lengths of the fields are recorded in.
   * @returns {number} The number of bytes that the message takes.
   */
  [kMeasure](context) {
    return measurePackedNumbersField(kSampleLocationId, this.locationId,
                                     context) +
      measurePackedNumbersField(kSampleValue, this.value, context) +
      measureMessageArrayField(kSampleLabel, this.label, context);
  }

  /**
   * Encodes the message, without its tag and length.
   * @param {Buffer} buffer The buffer to write the message to, which is at
   *   least as large as measured.
   * @param {number} offset The offset at which to write the message.
   * @param {EncodeContext} context The context of the encoding, which the
   *   payload lengths of the fields are read from.
   * @returns {number} The offset after the message.
   */
  [kEncodeToBuffer](buffer, offset, context) {
    offset = encodePackedNumbersField(buffer, offset, kSampleLocationId,
                                      this.locationId, context);
    offset = encodePackedNumbersField(buffer, offset, kSampleValue,
                                      this.value, context);
    return encodeMessageArrayField(buffer, offset, kSampleLabel, this.label,
                                   context);
  }
}

/**
 * Describes a binary, such as an executable or a shared library, that is
 * loaded in the address space of the profiled process, so that tools can
 * find the symbols of the native code addresses in a profile: the address
 * range it is loaded at, its file, its build ID, and which symbol information
 * is available. Profiles of JavaScript do not refer to code by address, so
 * the time profiler creates no mappings.
 */
class Mapping extends Message {
  /**
   * Returns `data` if it is a `Mapping`, or creates one from it, so that
   * messages and plain objects can be mixed in the fields of other messages.
   * @param {Mapping|object} data A `Mapping`, or an object with the fields of one.
   * @returns {Mapping} `data`, or a new `Mapping` with the fields of `data`.
   */
  static create(data) {
    return data instanceof Mapping ? data : new Mapping(data);
  }

  /**
   * Creates a `Mapping` message: a binary mapped into the address space.
   * @param {object} data The fields of the message. Missing fields take their
   *   default value.
   */
  constructor(data) {
    super();
    this.id = data.id || 0;
    this.memoryStart = data.memoryStart || 0;
    this.memoryLimit = data.memoryLimit || 0;
    this.fileOffset = data.fileOffset || 0;
    this.filename = data.filename || 0;
    this.buildId = data.buildId || 0;
    this.hasFunctions = !!data.hasFunctions;
    this.hasFilenames = !!data.hasFilenames;
    this.hasLineNumbers = !!data.hasLineNumbers;
    this.hasInlineFrames = !!data.hasInlineFrames;
  }

  /**
   * Measures the message, without its tag and length.
   * @returns {number} The number of bytes that the message takes.
   */
  [kMeasure]() {
    return measureNumberField(kMappingId, this.id) +
      measureNumberField(kMappingMemoryStart, this.memoryStart) +
      measureNumberField(kMappingMemoryLimit, this.memoryLimit) +
      measureNumberField(kMappingFileOffset, this.fileOffset) +
      measureNumberField(kMappingFilename, this.filename) +
      measureNumberField(kMappingBuildId, this.buildId) +
      measureBoolField(kMappingHasFunctions, this.hasFunctions) +
      measureBoolField(kMappingHasFilenames, this.hasFilenames) +
      measureBoolField(kMappingHasLineNumbers, this.hasLineNumbers) +
      measureBoolField(kMappingHasInlineFrames, this.hasInlineFrames);
  }

  /**
   * Encodes the message, without its tag and length.
   * @param {Buffer} buffer The buffer to write the message to, which is at
   *   least as large as measured.
   * @param {number} offset The offset at which to write the message.
   * @returns {number} The offset after the message.
   */
  [kEncodeToBuffer](buffer, offset) {
    offset = encodeNumberField(buffer, offset, kMappingId, this.id);
    offset = encodeNumberField(buffer, offset, kMappingMemoryStart,
                               this.memoryStart);
    offset = encodeNumberField(buffer, offset, kMappingMemoryLimit,
                               this.memoryLimit);
    offset = encodeNumberField(buffer, offset, kMappingFileOffset,
                               this.fileOffset);
    offset = encodeNumberField(buffer, offset, kMappingFilename,
                               this.filename);
    offset = encodeNumberField(buffer, offset, kMappingBuildId, this.buildId);
    offset = encodeBoolField(buffer, offset, kMappingHasFunctions,
                             this.hasFunctions);
    offset = encodeBoolField(buffer, offset, kMappingHasFilenames,
                             this.hasFilenames);
    offset = encodeBoolField(buffer, offset, kMappingHasLineNumbers,
                             this.hasLineNumbers);
    return encodeBoolField(buffer, offset, kMappingHasInlineFrames,
                           this.hasInlineFrames);
  }
}

/**
 * A position in the source code: the ID of the `ProfileFunction` that it is
 * in, and its `line` and `column`. A `Location` is made of one or more
 * `Line`s.
 */
class Line extends Message {
  /**
   * Returns `data` if it is a `Line`, or creates one from it, so that
   * messages and plain objects can be mixed in the fields of other messages.
   * @param {Line|object} data A `Line`, or an object with the fields of one.
   * @returns {Line} `data`, or a new `Line` with the fields of `data`.
   */
  static create(data) {
    return data instanceof Line ? data : new Line(data);
  }

  /**
   * Creates a `Line` message: a function and a position in its source.
   * @param {object} data The fields of the message. Missing fields take their
   *   default value.
   */
  constructor(data) {
    super();
    this.functionId = data.functionId || 0;
    this.line = data.line || 0;
    this.column = data.column || 0;
  }

  /**
   * Measures the message, without its tag and length.
   * @returns {number} The number of bytes that the message takes.
   */
  [kMeasure]() {
    return measureNumberField(kLineFunctionId, this.functionId) +
      measureNumberField(kLineLine, this.line) +
      measureNumberField(kLineColumn, this.column);
  }

  /**
   * Encodes the message, without its tag and length.
   * @param {Buffer} buffer The buffer to write the message to, which is at
   *   least as large as measured.
   * @param {number} offset The offset at which to write the message.
   * @returns {number} The offset after the message.
   */
  [kEncodeToBuffer](buffer, offset) {
    offset = encodeNumberField(buffer, offset, kLineFunctionId,
                               this.functionId);
    offset = encodeNumberField(buffer, offset, kLineLine, this.line);
    return encodeNumberField(buffer, offset, kLineColumn, this.column);
  }
}

/**
 * A frame of the stacks of a profile, which samples refer to by `id`. Its
 * `line` lists the source positions that the frame is made of: one, unless
 * functions were inlined into each other, in which case the inlined functions
 * come first and the function they were inlined into comes last. Native code
 * frames can also have an `address`, in the binary of the `Mapping` that
 * `mappingId` refers to. The time profiler creates one location per distinct
 * function, or per distinct line with the `lineNumbers` option.
 */
class Location extends Message {
  /**
   * Returns `data` if it is a `Location`, or creates one from it, so that
   * messages and plain objects can be mixed in the fields of other messages.
   * @param {Location|object} data A `Location`, or an object with the fields of one.
   * @returns {Location} `data`, or a new `Location` with the fields of `data`.
   */
  static create(data) {
    return data instanceof Location ? data : new Location(data);
  }

  /**
   * Creates a `Location` message: a stack frame and its lines.
   * @param {object} data The fields of the message. Missing fields take their
   *   default value.
   */
  constructor(data) {
    super();
    this.id = data.id || 0;
    this.mappingId = data.mappingId || 0;
    this.address = data.address || 0;
    this.line = ArrayPrototypeMap(data.line || [], Line.create);
    this.isFolded = !!data.isFolded;
  }

  /**
   * Measures the message, without its tag and length.
   * @param {EncodeContext} context The context of the encoding, which the
   *   payload lengths of the fields are recorded in.
   * @returns {number} The number of bytes that the message takes.
   */
  [kMeasure](context) {
    return measureNumberField(kLocationId, this.id) +
      measureNumberField(kLocationMappingId, this.mappingId) +
      measureNumberField(kLocationAddress, this.address) +
      measureMessageArrayField(kLocationLine, this.line, context) +
      measureBoolField(kLocationIsFolded, this.isFolded);
  }

  /**
   * Encodes the message, without its tag and length.
   * @param {Buffer} buffer The buffer to write the message to, which is at
   *   least as large as measured.
   * @param {number} offset The offset at which to write the message.
   * @param {EncodeContext} context The context of the encoding, which the
   *   payload lengths of the fields are read from.
   * @returns {number} The offset after the message.
   */
  [kEncodeToBuffer](buffer, offset, context) {
    offset = encodeNumberField(buffer, offset, kLocationId, this.id);
    offset = encodeNumberField(buffer, offset, kLocationMappingId,
                               this.mappingId);
    offset = encodeNumberField(buffer, offset, kLocationAddress,
                               this.address);
    offset = encodeMessageArrayField(buffer, offset, kLocationLine, this.line,
                                     context);
    return encodeBoolField(buffer, offset, kLocationIsFolded, this.isFolded);
  }
}

/**
 * A function that the frames of a profile are in, which `Line`s refer to by
 * `id`: its name, as `name`, and as `systemName` for the name used by the
 * system, such as a mangled C++ name, its source file, as `filename`, and its
 * first line, as `startLine`. Strings are indices in the profile's string
 * table. Several locations can refer to the same function, such as the
 * locations of the lines of one function. Not named `Function`, so that it
 * does not shadow the global.
 */
class ProfileFunction extends Message {
  /**
   * Returns `data` if it is a `ProfileFunction`, or creates one from it, so that
   * messages and plain objects can be mixed in the fields of other messages.
   * @param {ProfileFunction|object} data A `ProfileFunction`, or an object with the fields of one.
   * @returns {ProfileFunction} `data`, or a new `ProfileFunction` with the fields of `data`.
   */
  static create(data) {
    return data instanceof ProfileFunction ? data : new ProfileFunction(data);
  }

  /**
   * Creates a `Function` message: the name and source file of a function.
   * @param {object} data The fields of the message. Missing fields take their
   *   default value.
   */
  constructor(data) {
    super();
    this.id = data.id || 0;
    this.name = data.name || 0;
    this.systemName = data.systemName || 0;
    this.filename = data.filename || 0;
    this.startLine = data.startLine || 0;
  }

  /**
   * Measures the message, without its tag and length.
   * @returns {number} The number of bytes that the message takes.
   */
  [kMeasure]() {
    return measureNumberField(kFunctionId, this.id) +
      measureNumberField(kFunctionName, this.name) +
      measureNumberField(kFunctionSystemName, this.systemName) +
      measureNumberField(kFunctionFilename, this.filename) +
      measureNumberField(kFunctionStartLine, this.startLine);
  }

  /**
   * Encodes the message, without its tag and length.
   * @param {Buffer} buffer The buffer to write the message to, which is at
   *   least as large as measured.
   * @param {number} offset The offset at which to write the message.
   * @returns {number} The offset after the message.
   */
  [kEncodeToBuffer](buffer, offset) {
    offset = encodeNumberField(buffer, offset, kFunctionId, this.id);
    offset = encodeNumberField(buffer, offset, kFunctionName, this.name);
    offset = encodeNumberField(buffer, offset, kFunctionSystemName,
                               this.systemName);
    offset = encodeNumberField(buffer, offset, kFunctionFilename,
                               this.filename);
    return encodeNumberField(buffer, offset, kFunctionStartLine,
                             this.startLine);
  }
}

/**
 * A complete profile, as the pprof tools read it. It holds:
 * - `sampleType`: the `ValueType` of each value of the samples.
 * - `sample`: the `Sample`s, which refer to locations by ID.
 * - `location` and `function`: the `Location`s and `ProfileFunction`s that
 *   the stacks of the samples are made of.
 * - `stringTable`: the strings that the other fields refer to by index.
 * - `timeNanos` and `durationNanos`: when the profile started, in
 *   nanoseconds since the epoch, and how long it lasted.
 * - `periodType` and `period`: what separates two samples, such as 1000000
 *   nanoseconds of wall time.
 * This is the class of the `pprof.Profile` objects that the time profiler
 * returns, and that `encode()` and `encodeSync()` accept.
 */
class Profile extends Message {
  /**
   * Creates a `Profile` message: the samples of a profile, and the locations,
   * functions and strings that they refer to.
   * @param {object} [data] The fields of the message. Missing fields take their
   *   default value.
   */
  constructor(data = { __proto__: null }) {
    super();
    this.sampleType = ArrayPrototypeMap(data.sampleType || [],
                                        ValueType.create);
    this.sample = ArrayPrototypeMap(data.sample || [], Sample.create);
    this.mapping = ArrayPrototypeMap(data.mapping || [], Mapping.create);
    this.location = ArrayPrototypeMap(data.location || [], Location.create);
    this.function = ArrayPrototypeMap(data.function || [],
                                      ProfileFunction.create);
    this.stringTable = data.stringTable || new StringTable();
    this.dropFrames = data.dropFrames || 0;
    this.keepFrames = data.keepFrames || 0;
    this.timeNanos = data.timeNanos || 0;
    this.durationNanos = data.durationNanos || 0;
    this.periodType = data.periodType ?
      ValueType.create(data.periodType) : undefined;
    this.period = data.period || 0;
    this.comment = data.comment || [];
    this.defaultSampleType = data.defaultSampleType || 0;
    this.docUrl = data.docUrl || 0;
  }

  /**
   * Measures the message, without its tag and length.
   * @param {EncodeContext} context The context of the encoding, which the
   *   payload lengths of the fields are recorded in.
   * @returns {number} The number of bytes that the message takes.
   */
  [kMeasure](context) {
    return measureMessageArrayField(kProfileSampleType, this.sampleType,
                                    context) +
      measureMessageArrayField(kProfileSample, this.sample, context) +
      measureMessageArrayField(kProfileMapping, this.mapping, context) +
      measureMessageArrayField(kProfileLocation, this.location, context) +
      measureMessageArrayField(kProfileFunction, this.function, context) +
      this.stringTable[kMeasure](kProfileStringTable) +
      measureNumberField(kProfileDropFrames, this.dropFrames) +
      measureNumberField(kProfileKeepFrames, this.keepFrames) +
      measureNumberField(kProfileTimeNanos, this.timeNanos) +
      measureNumberField(kProfileDurationNanos, this.durationNanos) +
      measureMessageField(kProfilePeriodType, this.periodType, context) +
      measureNumberField(kProfilePeriod, this.period) +
      measurePackedNumbersField(kProfileComment, this.comment, context) +
      measureNumberField(kProfileDefaultSampleType, this.defaultSampleType) +
      measureNumberField(kProfileDocUrl, this.docUrl);
  }

  /**
   * Encodes the fields of the message that follow the string table.
   * @param {Buffer} buffer The buffer to write the fields to, which is at
   *   least as large as measured.
   * @param {number} offset The offset at which to write the fields.
   * @param {EncodeContext} context The context of the encoding, which the
   *   payload lengths of the fields are read from.
   * @returns {number} The offset after the fields.
   */
  #encodeBasicValuesToBuffer(buffer, offset, context) {
    offset = encodeNumberField(buffer, offset, kProfileDropFrames,
                               this.dropFrames);
    offset = encodeNumberField(buffer, offset, kProfileKeepFrames,
                               this.keepFrames);
    offset = encodeNumberField(buffer, offset, kProfileTimeNanos,
                               this.timeNanos);
    offset = encodeNumberField(buffer, offset, kProfileDurationNanos,
                               this.durationNanos);
    offset = encodeMessageField(buffer, offset, kProfilePeriodType,
                                this.periodType, context);
    offset = encodeNumberField(buffer, offset, kProfilePeriod, this.period);
    offset = encodePackedNumbersField(buffer, offset, kProfileComment,
                                      this.comment, context);
    offset = encodeNumberField(buffer, offset, kProfileDefaultSampleType,
                               this.defaultSampleType);
    return encodeNumberField(buffer, offset, kProfileDocUrl, this.docUrl);
  }

  /**
   * Encodes the message, without its tag and length.
   * @param {Buffer} buffer The buffer to write the message to, which is at
   *   least as large as measured.
   * @param {number} offset The offset at which to write the message.
   * @param {EncodeContext} context The context of the encoding, which the
   *   payload lengths of the fields are read from.
   * @returns {number} The offset after the message.
   */
  [kEncodeToBuffer](buffer, offset, context) {
    offset = encodeMessageArrayField(buffer, offset, kProfileSampleType,
                                     this.sampleType, context);
    offset = encodeMessageArrayField(buffer, offset, kProfileSample,
                                     this.sample, context);
    offset = encodeMessageArrayField(buffer, offset, kProfileMapping,
                                     this.mapping, context);
    offset = encodeMessageArrayField(buffer, offset, kProfileLocation,
                                     this.location, context);
    offset = encodeMessageArrayField(buffer, offset, kProfileFunction,
                                     this.function, context);
    offset = this.stringTable[kEncodeToBuffer](buffer, offset,
                                               kProfileStringTable);
    return this.#encodeBasicValuesToBuffer(buffer, offset, context);
  }

  /**
   * Like `encode()`, but yields to the event loop between the encoding of the
   * repeated fields. The profile must not be mutated until the returned promise
   * settles.
   * @returns {Promise<Buffer>} The uncompressed protobuf encoding of the
   *   message.
   * @throws {Error} Rejects with an `ERR_INTERNAL_ASSERTION` error if the
   *   profile was mutated while it was being encoded.
   */
  async encodeAsync() {
    const context = newEncodeContext();
    const length = this[kMeasure](context);
    const buffer = new FastBuffer(length);
    let offset = 0;
    offset = encodeMessageArrayField(buffer, offset, kProfileSampleType,
                                     this.sampleType, context);
    await yieldToEventLoop();
    offset = encodeMessageArrayField(buffer, offset, kProfileSample,
                                     this.sample, context);
    await yieldToEventLoop();
    offset = encodeMessageArrayField(buffer, offset, kProfileMapping,
                                     this.mapping, context);
    await yieldToEventLoop();
    offset = encodeMessageArrayField(buffer, offset, kProfileLocation,
                                     this.location, context);
    await yieldToEventLoop();
    offset = encodeMessageArrayField(buffer, offset, kProfileFunction,
                                     this.function, context);
    await yieldToEventLoop();
    offset = this.stringTable[kEncodeToBuffer](buffer, offset,
                                               kProfileStringTable);
    await yieldToEventLoop();
    offset = this.#encodeBasicValuesToBuffer(buffer, offset, context);
    verifyEncodedLength(offset, length);
    return buffer;
  }
}

module.exports = {
  Label,
  Line,
  Location,
  Mapping,
  Profile,
  ProfileFunction,
  Sample,
  StringTable,
  ValueType,
};
