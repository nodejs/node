/**
 * @file Ensure WPTRunner paths do not overlap across test files.
 *   Prevents running the same WPT tests multiple times by detecting
 *   when one WPTRunner path is a parent or subset of another.
 */
'use strict';

const { isString } = require('./rules-utils.js');

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

// Module-level state to track paths across all files in a lint run.
// Map<path, { filename, loc }>
const registeredPaths = new Map();

/**
 * Check if pathA contains pathB (i.e., pathB is a subdirectory of pathA)
 * @param {string} pathA - The potential parent path
 * @param {string} pathB - The potential child path
 * @returns {boolean}
 */
function isSubdirectory(pathA, pathB) {
  // Normalize: remove trailing slashes
  const normalizedA = pathA.replace(/\/+$/, '');
  const normalizedB = pathB.replace(/\/+$/, '');

  if (normalizedA === normalizedB) {
    return true;
  }

  // pathB is a subdirectory of pathA if pathB starts with pathA/
  return normalizedB.startsWith(normalizedA + '/');
}

module.exports = {
  meta: {
    type: 'problem',
    docs: {
      description: 'Disallow overlapping WPTRunner paths across test files',
    },
    schema: [],
    messages: {
      overlappingPath:
        "WPTRunner path '{{ currentPath }}' overlaps with '{{ existingPath }}' in {{ existingFile }}. " +
        'One path is a subset of the other, which would run the same tests multiple times.',
    },
  },

  create(context) {
    return {
      NewExpression(node) {
        // Check if this is a `new WPTRunner(...)` call
        if (node.callee.type !== 'Identifier' || node.callee.name !== 'WPTRunner') {
          return;
        }

        // Get the first argument (the path)
        const [firstArg] = node.arguments;
        if (!isString(firstArg)) {
          return;
        }

        const currentPath = firstArg.value;
        const currentFilename = context.filename;

        // Check against all registered paths for overlaps
        for (const [existingPath, existingInfo] of registeredPaths) {
          // Skip if same file (could be legitimate multiple runners in same file)
          if (existingInfo.filename === currentFilename) {
            continue;
          }

          // Check if either path is a subdirectory of the other
          const currentIsSubOfExisting = isSubdirectory(existingPath, currentPath);
          const existingIsSubOfCurrent = isSubdirectory(currentPath, existingPath);

          if (currentIsSubOfExisting || existingIsSubOfCurrent) {
            context.report({
              node: firstArg,
              messageId: 'overlappingPath',
              data: {
                currentPath,
                existingPath,
                existingFile: existingInfo.filename,
              },
            });
          }
        }

        // Register this path
        registeredPaths.set(currentPath, {
          filename: currentFilename,
          loc: firstArg.loc,
        });
      },
    };
  },
};
