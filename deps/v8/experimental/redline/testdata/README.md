# Test data

The offline checks of `redline selftest` compare the current code with the golden traces in `selection/`.
Each trace records what the default components and strategies do for a fixed random seed.
A check fails if the code gives a different trace, and it prints the first line that differs.
Thus a change to a default pick or to the output of a component shows as a failing check.

Each line of a trace is one record. Tabs separate the fields.

| File | Each line records | Fields |
|---|---|---|
| `mutators.trace` | A pick of the default mutator strategy. | Index, partner flag (`1` if a partner program is available, else `0`), mutator. |
| `generators.trace` | A pick of the default generator strategy, for a random insertion point in a fixed program. | Index, generator. |
| `codegen.trace` | A `CodeGen` mutation of the fixed program. | Index, the generators that the mutator inserted. |
| `generator_outputs.trace` | An output of a generator for a fixed seed. | Generator, sample number, hash of the program, start of the program. |
| `mutator_outputs.trace` | An output of a mutator for a fixed seed. | Mutator, sample number, hash of the program, change, start of the program. |
| `scheduler_<name>.trace` | A pick from a fixed corpus with the seed strategy `<name>`. | Index, id of the seed, id of the partner. |

The scheduler traces exist for the seed strategies `uniform`, `recency`, `tournament` and `productive`.
They use the partner strategy `uniform-partner`. Some picks report new coverage, as in a session.

`experimental/redline/commands/selftest_selection.cc` makes the traces.
Redline finds this directory from the location of its binary. `--testdata=DIR` sets another directory.

## Update the traces

Update the traces only for an intended change of the default picks or outputs:

```shell
out/redline/redline selftest --offline --write-golden
git diff --stat experimental/redline/testdata
```

Examine the diff, and explain the change in the CL description.
[CONTRIBUTE.md](../CONTRIBUTE.md) tells which traces change when you add a generator or a mutator.
