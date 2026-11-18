// Flags: --expose-internals
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';

import assert from 'node:assert';
import test from 'node:test';
import { readdirSync, readFileSync } from 'node:fs';
import sourceMap from 'internal/source_map/source_map';

const { SourceMap, kScopes } = sourceMap;

// Runs the decoding tests: each `<name>.map` in a proposal directory is
// decoded and compared with its `<name>.map.golden` Decoded Source Map Record.
// See test/fixtures/test426/decoding/README.md.

const kStatus = JSON.parse(readFileSync(new URL('./status/decoding.json', import.meta.url), 'utf8'));

// Compares the parts of the golden record that each proposal adds, as
// described by its `<proposal>/<proposal>.d.ts`.
const kProposalCheckers = {
  scopes: checkScopes,
};

for (const proposal of readdirSync(fixtures.path('test426/decoding'), { withFileTypes: true })) {
  if (!proposal.isDirectory()) continue;
  const dir = fixtures.path('test426/decoding', proposal.name);
  const files = readdirSync(dir).filter((file) => file.endsWith('.map'));
  const checker = kProposalCheckers[proposal.name];
  for (const file of files) {
    const name = `${proposal.name}/${file}`;
    test(name, {
      skip: kStatus[proposal.name]?.skip ?? kStatus[name]?.skip,
      todo: checker === undefined,
    }, () => {
      if (checker === undefined) return;
      const payload = JSON.parse(readFileSync(`${dir}/${file}`, 'utf8'));
      const golden = JSON.parse(readFileSync(`${dir}/${file}.golden`, 'utf8'));
      checker(new SourceMap(payload), golden);
    });
  }
}

function checkScopes(sourceMap, golden) {
  const { originalScopes, generatedRanges } = sourceMap[kScopes];
  const flattenedScopes = originalScopes.map((roots) => flatten(roots));
  assert.deepStrictEqual(
    originalScopes.map((roots) => ({ rootScopes: roots.map(toGoldenScope) })),
    golden.sources.map(({ rootScopes }) => ({ rootScopes })));
  assert.deepStrictEqual(
    generatedRanges.map((range) => toGoldenRange(range, flattenedScopes)),
    golden.ranges);
}

function toGoldenScope(scope) {
  return {
    start: scope.start,
    end: scope.end,
    name: scope.name,
    kind: scope.kind,
    isStackFrame: scope.isStackFrame,
    variables: scope.variables,
    children: scope.children.map(toGoldenScope),
  };
}

// The golden format refers to a definition by its source index and its index
// in the pre-order list of that source's scopes.
function toGoldenRange(range, flattenedScopes) {
  let definition = null;
  if (range.definition !== null) {
    const { sourceIndex } = range.definition;
    definition = {
      source: sourceIndex,
      scope: flattenedScopes[sourceIndex].indexOf(range.definition),
    };
  }
  return {
    start: range.start,
    end: range.end,
    definition,
    stackFrameType: range.stackFrameType,
    callSite: range.callSite,
    bindings: range.bindings,
    children: range.children.map((child) => toGoldenRange(child, flattenedScopes)),
  };
}

function flatten(scopes, result = []) {
  for (const scope of scopes) {
    result.push(scope);
    flatten(scope.children, result);
  }
  return result;
}
