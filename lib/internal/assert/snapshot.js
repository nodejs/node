'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypeSlice,
  RegExp,
  SafeMap,
  SafeSet,
  StringPrototypeSplit,
  StringPrototypeReplace,
  Symbol,
} = primordials;

const { codes: { ERR_ASSERT_SNAPSHOT_NOT_SUPPORTED } } = require('internal/errors');
const AssertionError = require('internal/assert/assertion_error');
const { inspect } = require('internal/util/inspect');
const { getOptionValue } = require('internal/options');
const { validateString } = require('internal/validators');
const { once } = require('events');
const { createReadStream, createWriteStream } = require('fs');
const path = require('path');
const assert = require('assert');

const kUpdateSnapshot = getOptionValue('--update-assert-snapshot');
const kInitialSnapshot = Symbol('kInitialSnapshot');
const kDefaultDelimiter = '\n#*#*#*#*#*#*#*#*#*#*#*#\n';
const kDefaultDelimiterRegex = new RegExp(kDefaultDelimiter.replaceAll('*', '\\*').replaceAll('\n', '\r?\n'), 'g');
const kKeyDelimiter = /:\r?\n/g;

function getSnapshotPath() {
  if (process.mainModule) {
    const { dir, name } = path.parse(process.mainModule.filename);
    return path.join(dir, `${name}.snapshot`);
  }
  if (!process.argv[1]) {
    throw new ERR_ASSERT_SNAPSHOT_NOT_SUPPORTED();
  }
  const { dir, name } = path.parse(process.argv[1]);
  return path.join(dir, `${name}.snapshot`);
}

function getSource() {
  return createReadStream(getSnapshotPath(), { encoding: 'utf8' });
}

let _target;
function getTarget() {
  _target ??= createWriteStream(getSnapshotPath(), { encoding: 'utf8' });
  return _target;
}

function serializeName(name) {
  validateString(name, 'name');
  return StringPrototypeReplace(`${name}`, kKeyDelimiter, '_');
}

let writtenNames;
let snapshotValue;
let counter = 0;

async function writeSnapshot({ name, value }) {
  const target = getTarget();
  if (counter > 1) {
    target.write(kDefaultDelimiter);
  }
  writtenNames = writtenNames || new SafeSet();
  if (writtenNames.has(name)) {
    throw new AssertionError({ message: `Snapshot "${name}" already used` });
  }
  writtenNames.add(name);
  const drained = target.write(`${name}:\n${value}`);
  await drained || once(target, 'drain');
}

async function getSnapshot() {
  if (snapshotValue !== undefined) {
    return snapshotValue;
  }
  if (kUpdateSnapshot) {
    snapshotValue = kInitialSnapshot;
    return kInitialSnapshot;
  }
  try {
    const source = getSource();
    let data = '';
    for await (const line of source) {
      data += line;
    }
    snapshotValue = new SafeMap(
      ArrayPrototypeMap(
        StringPrototypeSplit(data, kDefaultDelimiterRegex),
        (item) => {
          const arr = StringPrototypeSplit(item, kKeyDelimiter);
          return [
            arr[0],
            ArrayPrototypeJoin(ArrayPrototypeSlice(arr, 1), ':\n'),
          ];
        }
      ));
  } catch (e) {
    if (e.code === 'ENOENT') {
      snapshotValue = kInitialSnapshot;
    } else {
      throw e;
    }
  }
  return snapshotValue;
}


async function snapshot(input, name) {
  const snapshot = await getSnapshot();
  counter = counter + 1;
  name = serializeName(name);

  const value = inspect(input);
  if (snapshot === kInitialSnapshot) {
    await writeSnapshot({ name, value });
  } else if (snapshot.has(name)) {
    const expected = snapshot.get(name);
    // eslint-disable-next-line no-restricted-syntax
    assert.strictEqual(value, expected);
  } else {
    throw new AssertionError({ message: `Snapshot "${name}" does not exist`, actual: inspect(snapshot) });
  }
}

module.exports = snapshot;
