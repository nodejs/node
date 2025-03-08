'use strict';

const {
  Array,
  ArrayPrototypeJoin,
  ArrayPrototypeReverse,
  StringPrototypeCharCodeAt,
  StringPrototypeTrimEnd,
} = primordials;
const os = require('os');

const kBrackets = { '{': 1, '[': 1, '(': 1, '}': -1, ']': -1, ')': -1 };

function parseHistoryFromFile(historyText, historySize) {
  const lines = historyText.trimEnd().split(os.EOL);
  let linesLength = lines.length;
  if (linesLength > historySize) linesLength = historySize;

  const commands = new Array(linesLength);
  let commandsIndex = 0;
  const currentCommand = new Array(linesLength);
  let currentCommandIndex = 0;
  let bracketCount = 0;
  let inString = false;
  let stringDelimiter = '';

  for (let lineIdx = linesLength - 1; lineIdx >= 0; lineIdx--) {
    const line = lines[lineIdx];
    currentCommand[currentCommandIndex++] = line;

    let isConcatenation = false;
    let isArrowFunction = false;
    let lastChar = '';

    for (let charIdx = 0, len = line.length; charIdx < len; charIdx++) {
      const char = line[charIdx];

      if ((char === "'" || char === '"' || char === '`') &&
          (charIdx === 0 || StringPrototypeCharCodeAt(line, charIdx - 1) !== 92)) { // 92 is '\\'
        if (!inString) {
          inString = true;
          stringDelimiter = char;
        } else if (char === stringDelimiter) {
          inString = false;
        }
      }

      if (!inString) {
        const bracketValue = kBrackets[char];
        if (bracketValue) bracketCount += bracketValue;
      }

      lastChar = char;
    }

    if (!inString) {
      const trimmedLine = StringPrototypeTrimEnd(line);
      isConcatenation = lastChar === '+';
      isArrowFunction = lastChar === '>' && trimmedLine[trimmedLine.length - 2] === '=';
    }

    if (!inString && bracketCount <= 0 && !isConcatenation && !isArrowFunction) {
      commands[commandsIndex++] = ArrayPrototypeJoin(currentCommand.slice(0, currentCommandIndex), '\n');
      currentCommandIndex = 0;
      bracketCount = 0;
    }
  }

  if (currentCommandIndex > 0) {
    commands[commandsIndex++] = ArrayPrototypeJoin(currentCommand.slice(0, currentCommandIndex), '\n');
  }

  commands.length = commandsIndex;
  return ArrayPrototypeReverse(commands);
}

module.exports = { parseHistoryFromFile };
