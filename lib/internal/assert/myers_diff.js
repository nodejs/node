'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  Int32Array,
  MathMax,
  MathMin,
  MathRound,
  RegExpPrototypeExec,
  StringPrototypeEndsWith,
} = primordials;

const {
  codes: {
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');

const colors = require('internal/util/colors');

const kChunkSize = 512;
const kNopLinesToCollapse = 5;
// Lines that are just structural characters make poor alignment anchors
// because they appear many times and don't uniquely identify a position.
const kTrivialLinePattern = /^\s*[{}[\],]+\s*$/;
const kOperations = {
  DELETE: -1,
  NOP: 0,
  INSERT: 1,
};

function areLinesEqual(actual, expected, checkCommaDisparity) {
  if (actual === expected) {
    return true;
  }
  if (checkCommaDisparity) {
    return (actual + ',') === expected || actual === (expected + ',');
  }
  return false;
}

function myersDiffInternal(actual, expected, checkCommaDisparity) {
  const actualLength = actual.length;
  const expectedLength = expected.length;
  const max = actualLength + expectedLength;

  const v = new Int32Array(2 * max + 1);
  const trace = [];

  for (let diffLevel = 0; diffLevel <= max; diffLevel++) {
    ArrayPrototypePush(trace, new Int32Array(v)); // Clone the current state of `v`

    for (let diagonalIndex = -diffLevel; diagonalIndex <= diffLevel; diagonalIndex += 2) {
      const offset = diagonalIndex + max;
      const previousOffset = v[offset - 1];
      const nextOffset = v[offset + 1];
      let x = diagonalIndex === -diffLevel || (diagonalIndex !== diffLevel && previousOffset < nextOffset) ?
        nextOffset :
        previousOffset + 1;
      let y = x - diagonalIndex;

      while (
        x < actualLength &&
        y < expectedLength &&
        areLinesEqual(actual[x], expected[y], checkCommaDisparity)
      ) {
        x++;
        y++;
      }

      v[offset] = x;

      if (x >= actualLength && y >= expectedLength) {
        return backtrack(trace, actual, expected, checkCommaDisparity);
      }
    }
  }
}

function backtrack(trace, actual, expected, checkCommaDisparity) {
  const actualLength = actual.length;
  const expectedLength = expected.length;
  const max = actualLength + expectedLength;

  let x = actualLength;
  let y = expectedLength;
  const result = [];

  for (let diffLevel = trace.length - 1; diffLevel >= 0; diffLevel--) {
    const v = trace[diffLevel];
    const diagonalIndex = x - y;
    const offset = diagonalIndex + max;

    let prevDiagonalIndex;
    if (
      diagonalIndex === -diffLevel ||
      (diagonalIndex !== diffLevel && v[offset - 1] < v[offset + 1])
    ) {
      prevDiagonalIndex = diagonalIndex + 1;
    } else {
      prevDiagonalIndex = diagonalIndex - 1;
    }

    const prevX = v[prevDiagonalIndex + max];
    const prevY = prevX - prevDiagonalIndex;

    while (x > prevX && y > prevY) {
      const actualItem = actual[x - 1];
      const value = checkCommaDisparity && !StringPrototypeEndsWith(actualItem, ',') ? expected[y - 1] : actualItem;
      ArrayPrototypePush(result, [ kOperations.NOP, value ]);
      x--;
      y--;
    }

    if (diffLevel > 0) {
      if (x > prevX) {
        ArrayPrototypePush(result, [ kOperations.INSERT, actual[--x] ]);
      } else {
        ArrayPrototypePush(result, [ kOperations.DELETE, expected[--y] ]);
      }
    }
  }

  return result;
}

function myersDiff(actual, expected, checkCommaDisparity = false) {
  const actualLength = actual.length;
  const expectedLength = expected.length;
  const max = actualLength + expectedLength;

  if (max > 2 ** 31 - 1) {
    throw new ERR_OUT_OF_RANGE(
      'myersDiff input size',
      '< 2^31',
      max,
    );
  }

  // For small inputs, run the algorithm directly
  if (actualLength <= kChunkSize && expectedLength <= kChunkSize) {
    return myersDiffInternal(actual, expected, checkCommaDisparity);
  }

  const boundaries = findAlignedBoundaries(
    actual, expected, checkCommaDisparity,
  );

  // Process chunks and concatenate results (last chunk first for reversed order)
  const result = [];
  for (let i = boundaries.length - 2; i >= 0; i--) {
    const actualStart = boundaries[i].actualIdx;
    const actualEnd = boundaries[i + 1].actualIdx;
    const expectedStart = boundaries[i].expectedIdx;
    const expectedEnd = boundaries[i + 1].expectedIdx;

    const actualChunk = ArrayPrototypeSlice(actual, actualStart, actualEnd);
    const expectedChunk = ArrayPrototypeSlice(expected, expectedStart, expectedEnd);

    if (actualChunk.length === 0 && expectedChunk.length === 0) continue;

    if (actualChunk.length === 0) {
      for (let j = expectedChunk.length - 1; j >= 0; j--) {
        ArrayPrototypePush(result, [kOperations.DELETE, expectedChunk[j]]);
      }
      continue;
    }

    if (expectedChunk.length === 0) {
      for (let j = actualChunk.length - 1; j >= 0; j--) {
        ArrayPrototypePush(result, [kOperations.INSERT, actualChunk[j]]);
      }
      continue;
    }

    const chunkDiff = myersDiffInternal(actualChunk, expectedChunk, checkCommaDisparity);
    for (let j = 0; j < chunkDiff.length; j++) {
      ArrayPrototypePush(result, chunkDiff[j]);
    }
  }

  return result;
}

function findAlignedBoundaries(actual, expected, checkCommaDisparity) {
  const actualLen = actual.length;
  const expectedLen = expected.length;
  const boundaries = [{ actualIdx: 0, expectedIdx: 0 }];
  const searchRadius = kChunkSize / 2;

  // Add boundaries until the remainder of both sides fits in a single chunk.
  // Stepping along the larger remaining side keeps every chunk bounded on
  // both sides, which bounds the O((N + M) * D) memory of each
  // myersDiffInternal() call.
  let prev = boundaries[0];
  while (actualLen - prev.actualIdx > kChunkSize ||
         expectedLen - prev.expectedIdx > kChunkSize) {
    const actualRemaining = actualLen - prev.actualIdx;
    const expectedRemaining = expectedLen - prev.expectedIdx;

    let targetActual;
    let targetExpected;
    if (actualRemaining >= expectedRemaining) {
      targetActual = prev.actualIdx + kChunkSize;
      targetExpected = prev.expectedIdx +
        MathRound(kChunkSize * expectedRemaining / actualRemaining);
    } else {
      targetExpected = prev.expectedIdx + kChunkSize;
      targetActual = prev.actualIdx +
        MathRound(kChunkSize * actualRemaining / expectedRemaining);
    }

    const anchor = findAnchorNear(
      actual, expected, targetActual, targetExpected,
      prev, searchRadius, checkCommaDisparity,
    );

    let next;
    if (anchor !== undefined) {
      next = anchor;
    } else {
      // Fallback: use the proportional position. The chunk boundary may split
      // a matching region, so the diff stays correct but may not be minimal.
      next = { actualIdx: targetActual, expectedIdx: targetExpected };
    }
    ArrayPrototypePush(boundaries, next);
    prev = next;
  }

  ArrayPrototypePush(boundaries, { actualIdx: actualLen, expectedIdx: expectedLen });
  return boundaries;
}

// Search outward from targetActual and targetExpected for a non-trivial
// line that matches in both arrays, with adjacent context verification.
function findAnchorNear(actual, expected, targetActual, targetExpected,
                        prevBoundary, searchRadius, checkCommaDisparity) {
  const actualLen = actual.length;

  const searchStart = MathMax(prevBoundary.expectedIdx + 1, targetExpected - searchRadius);
  const searchEnd = MathMin(expected.length - 1, targetExpected + searchRadius);
  if (searchStart > searchEnd) {
    return undefined;
  }
  const maxExpectedOffset = MathMax(
    targetExpected - searchStart,
    searchEnd - targetExpected,
  );

  for (let offset = 0; offset <= searchRadius; offset++) {
    const candidates = offset === 0 ? [targetActual] : [targetActual + offset, targetActual - offset];

    for (let i = 0; i < candidates.length; i++) {
      const actualIdx = candidates[i];
      if (actualIdx <= prevBoundary.actualIdx || actualIdx >= actualLen) {
        continue;
      }

      const line = actual[actualIdx];
      if (isTrivialLine(line)) {
        continue;
      }

      for (let j = 0; j <= maxExpectedOffset; j++) {
        const offsets = j === 0 ? [0] : [j, -j];
        for (let k = 0; k < offsets.length; k++) {
          const expectedIdx = targetExpected + offsets[k];
          if (expectedIdx < searchStart || expectedIdx > searchEnd) {
            continue;
          }

          if (
            areLinesEqual(line, expected[expectedIdx], checkCommaDisparity) &&
            hasAdjacentMatch(actual, expected, actualIdx, expectedIdx, checkCommaDisparity)
          ) {
            return { actualIdx, expectedIdx };
          }
        }
      }
    }
  }

  return undefined;
}

function hasAdjacentMatch(actual, expected, actualIdx, expectedIdx, checkCommaDisparity) {
  if (actualIdx > 0 && expectedIdx > 0 &&
      areLinesEqual(actual[actualIdx - 1], expected[expectedIdx - 1], checkCommaDisparity)) {
    return true;
  }
  if (actualIdx < actual.length - 1 && expectedIdx < expected.length - 1 &&
      areLinesEqual(actual[actualIdx + 1], expected[expectedIdx + 1], checkCommaDisparity)) {
    return true;
  }
  return false;
}

function isTrivialLine(line) {
  return RegExpPrototypeExec(kTrivialLinePattern, line) !== null;
}

function printSimpleMyersDiff(diff) {
  let message = '';

  for (let diffIdx = diff.length - 1; diffIdx >= 0; diffIdx--) {
    const { 0: operation, 1: value } = diff[diffIdx];
    let color = colors.white;

    if (operation === kOperations.INSERT) {
      color = colors.green;
    } else if (operation === kOperations.DELETE) {
      color = colors.red;
    }

    message += `${color}${value}${colors.white}`;
  }

  return `\n${message}`;
}

function printMyersDiff(diff, operator) {
  let message = '';
  let skipped = false;
  let nopCount = 0;

  for (let diffIdx = diff.length - 1; diffIdx >= 0; diffIdx--) {
    const { 0: operation, 1: value } = diff[diffIdx];
    const previousOperation = diffIdx < diff.length - 1 ? diff[diffIdx + 1][0] : null;

    // Avoid grouping if only one line would have been grouped otherwise
    if (previousOperation === kOperations.NOP && operation !== previousOperation) {
      if (nopCount === kNopLinesToCollapse + 1) {
        message += `${colors.white}  ${diff[diffIdx + 1][1]}\n`;
      } else if (nopCount === kNopLinesToCollapse + 2) {
        message += `${colors.white}  ${diff[diffIdx + 2][1]}\n`;
        message += `${colors.white}  ${diff[diffIdx + 1][1]}\n`;
      } else if (nopCount >= kNopLinesToCollapse + 3) {
        message += `${colors.blue}...${colors.white}\n`;
        message += `${colors.white}  ${diff[diffIdx + 1][1]}\n`;
        skipped = true;
      }
      nopCount = 0;
    }

    if (operation === kOperations.INSERT) {
      if (operator === 'partialDeepStrictEqual') {
        message += `${colors.gray}${colors.hasColors ? ' ' : '+'} ${value}${colors.white}\n`;
      } else {
        message += `${colors.green}+${colors.white} ${value}\n`;
      }
    } else if (operation === kOperations.DELETE) {
      message += `${colors.red}-${colors.white} ${value}\n`;
    } else if (operation === kOperations.NOP) {
      if (nopCount < kNopLinesToCollapse) {
        message += `${colors.white}  ${value}\n`;
      }
      nopCount++;
    }
  }

  message = message.trimEnd();

  return { message: `\n${message}`, skipped };
}

module.exports = { myersDiff, printMyersDiff, printSimpleMyersDiff };
