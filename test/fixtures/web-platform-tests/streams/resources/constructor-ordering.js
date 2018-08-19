'use strict';

// Helpers for tests that constructors perform getting and validation of properties in the standard order.
// See ../readable-streams/constructor.js for an example of how to use them.

// Describes an operation on a property. |type| is "get", "validate" or "tonumber". |name| is the name of the property
// in question. |side| is usually undefined, but is used by TransformStream to distinguish between the readable and
// writable strategies.
class Op {
  constructor(type, name, side) {
    this.type = type;
    this.name = name;
    this.side = side;
  }

  toString() {
    return this.side === undefined ? `${this.type} on ${this.name}` : `${this.type} on ${this.name} (${this.side})`;
  }

  equals(otherOp) {
    return this.type === otherOp.type && this.name === otherOp.name && this.side === otherOp.side;
  }
}

// Provides a concise syntax to create an Op object. |side| is used by TransformStream to distinguish between the two
// strategies.
function op(type, name, side = undefined) {
  return new Op(type, name, side);
}

// Records a sequence of operations. Also checks each operation against |failureOp| to see if it should fail.
class OpRecorder {
  constructor(failureOp) {
    this.ops = [];
    this.failureOp = failureOp;
    this.matched = false;
  }

  // Record an operation. Returns true if this operation should fail.
  recordAndCheck(type, name, side = undefined) {
    const recordedOp = op(type, name, side);
    this.ops.push(recordedOp);
    return this.failureOp.equals(recordedOp);
  }

  // Returns true if validation of this property should fail.
  check(name, side = undefined) {
    return this.failureOp.equals(op('validate', name, side));
  }

  // Returns the sequence of recorded operations as a string.
  actual() {
    return this.ops.toString();
  }
}

// Creates an object with the list of properties named in |properties|. Every property access will be recorded in
// |record|, which will also be used to determine whether a particular property access should fail, or whether it should
// return an invalid value that will fail validation.
function createRecordingObjectWithProperties(record, properties) {
  const recordingObject = {};
  for (const property of properties) {
    defineCheckedProperty(record, recordingObject, property, () => record.check(property) ? 'invalid' : undefined);
  }
  return recordingObject;
}

// Add a getter to |object| named |property| which throws if op('get', property) should fail, and otherwise calls
// getter() to get the return value.
function defineCheckedProperty(record, object, property, getter) {
  Object.defineProperty(object, property, {
    get() {
      if (record.recordAndCheck('get', property)) {
        throw new Error(`intentional failure of get ${property}`);
      }
      return getter();
    }
  });
}

// Similar to createRecordingObjectWithProperties(), but with specific functionality for "highWaterMark" so that numeric
// conversion can be recorded. Permits |side| to be specified so that TransformStream can distinguish between its two
// strategies.
function createRecordingStrategy(record, side = undefined) {
  return {
    get size() {
      if (record.recordAndCheck('get', 'size', side)) {
        throw new Error(`intentional failure of get size`);
      }
      return record.check('size', side) ? 'invalid' : undefined;
    },
    get highWaterMark() {
      if (record.recordAndCheck('get', 'highWaterMark', side)) {
        throw new Error(`intentional failure of get highWaterMark`);
      }
      return createRecordingNumberObject(record, 'highWaterMark', side);
    }
  };
}

// Creates an object which will record when it is converted to a number. It will assert if the conversion is to some
// other type, and will fail if op('tonumber', property, side) is set as the failure step. The object will convert to -1
// if 'validate' is set as the failure step, and 1 otherwise.
function createRecordingNumberObject(record, property, side = undefined) {
  return {
    [Symbol.toPrimitive](hint) {
      assert_equals(hint, 'number', `hint for ${property} should be 'number'`);
      if (record.recordAndCheck('tonumber', property, side)) {
        throw new Error(`intentional failure of ${op('tonumber', property, side)}`);
      }
      return record.check(property, side) ? -1 : 1;
    }
  };
}

// Creates a string from everything in |operations| up to and including |failureOp|. "validate" steps are excluded from
// the output, as we cannot record them except by making them fail.
function expectedAsString(operations, failureOp) {
  const expected = [];
  for (const step of operations) {
    if (step.type !== 'validate') {
      expected.push(step);
    }
    if (step.equals(failureOp)) {
      break;
    }
  }
  return expected.toString();
}
