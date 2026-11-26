# Hourly Fuzzing Report Prompt Template

Copy this prompt into your recurring hourly scheduler call (for example,
`schedule` with `IsDaemon: false`). Replace all `<PLACEHOLDER>` values with
literal strings before you schedule the task.

```text
Collect the hourly V8 CL fuzzing report and present all findings in plain V8
terms.

Run parameters:
- HOST: <HOST>
- RUN_ROOT: <RUN_ROOT>
- CTL: <CTL_OR_NONE>
- SKILL_DIR: <SKILL_DIR>

Steps:
1. Run `report_arms.py`:
   - If HOST is `local`:
     python3 "<SKILL_DIR>/scripts/report_arms.py" "<RUN_ROOT>" --mode hourly
   - If HOST is remote:
     ssh -o BatchMode=yes -o ConnectTimeout=10 -o ControlPath="<CTL>" "<HOST>" \
       "python3 - '<RUN_ROOT>' --mode hourly --host '<HOST>'" \
       < "<SKILL_DIR>/scripts/report_arms.py"
2. If SSH fails with exit code 255 or authentication error, tell the user the
   exact command to renew SSH access:
   `ssh -fN -o ControlMaster=yes -o ControlPersist=24h -o ServerAliveInterval=60 -o ControlPath="<CTL>" "<HOST>"`
   Then stop this turn.
3. Show the status table and report every unique crash signature across all
   classes (`deterministic`, `state-dependent`, `amplified`, `flaky`, and
   `in-flight`), including signatures where `reproduced` is `0`.
4. For every crash, show the exact reproduction command (including
   `--bundle-reprl` for state-dependent crashes) and the `scp` download hint
   when HOST is remote.
5. Highlight any alert from the report (dead arm, startup test failure,
   timeout rate above 15%, or 0 CL target hits after 15 min).
6. If every arm shows `ENDED` or `STOPPED`, show the final summary and cancel
   this recurring schedule (for example, with `manage_task`).
```
