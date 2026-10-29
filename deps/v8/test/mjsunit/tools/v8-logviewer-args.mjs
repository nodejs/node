// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

globalThis.arguments = undefined;

load('test/mjsunit/mjsunit.js');

const {
  CodeCommand,
  DeoptCommand,
  ScriptCommand,
  COMMANDS,
  COMMANDS_LOOKUP
} = await import('../../../tools/system-analyzer/cli.mjs');



(function testCodeCommandArgs() {
  const cmd = new CodeCommand();
  cmd.parseArgs(['--code-kind=Opt', '--limit=5']);
  assertEquals('Opt', cmd.filter.codeKind, "codeKind should be Opt");
  assertEquals(5, cmd.limit, "limit should be 5");
})();

(function testDeoptCommandArgs() {
  const cmd = new DeoptCommand();
  cmd.parseArgs(['--deopt-type=deopt-eager', '--time=1000..5000']);
  assertEquals('deopt-eager', cmd.filter.deoptType,
    "deoptType should be deopt-eager");
  assertEquals(1000, cmd.filter.timeRange.start,
    "timeRange.start should be 1000");
  assertEquals(5000, cmd.filter.timeRange.end,
    "timeRange.end should be 5000");
})();

(function testCommonFilters() {
  const cmd1 = new ScriptCommand();
  cmd1.parseArgs(['--script=test.js']);
  assertEquals('test.js', cmd1.filter.script,
    "script filter should be test.js");
  const cmd2 = new ScriptCommand();
  cmd2.parseArgs(['--script-id=3']);
  assertEquals(3, cmd2.filter.scriptId, "scriptId should be 3");
})();

(function testAllCommandsInstantiate() {
  const commands = COMMANDS;
  for (const CommandClass of commands) {
    const cmd = new CommandClass();
    cmd.parseArgs(['--limit=5', '--format=json']);
    assertEquals(5, cmd.limit,
      `limit should be 5 for ${CommandClass.name}`);
    assertEquals('json', cmd.format,
      `format should be json for ${CommandClass.name}`);
  }
})();

(function testAliasesWork() {
  for (const cls of COMMANDS) {
    for (const alias of cls.aliases) {
      assertEquals(cls, COMMANDS_LOOKUP[alias],
        `Alias ${alias} should map to ${cls.name}`);
    }
  }
})();

(function testNestedSorting() {
  const cmd = new ScriptCommand();
  cmd.parseArgs(['--sort=stats.code']);
  assertEquals('stats.code', cmd.sortField, "sortField should be stats.code");
  const results = [
    { id: 1, stats: { code: 10 } },
    { id: 2, stats: { code: 5 } },
    { id: 3, stats: { code: 20 } }
  ];
  cmd.applySort(results);
  assertEquals(20, results[0].stats.code, "First element should have code 20");
  assertEquals(10, results[1].stats.code, "Second element should have code 10");
  assertEquals(5, results[2].stats.code, "Third element should have code 5");
})();
