# Tips, Tricks & Troubleshooting

## Tips

### Windows

1. If you regularly build on windows, you should check out [`ninja`](./building-node-with-ninja.md).
   You may find it to be much faster than `MSBuild` and less eager to rebuild unchanged sub-targets.  
   P.S. non-windows devs might find it better then `make` as well.

<!--
## Tricks

TDB

-->
## Troubleshooting

1. Python crashes with `LookupError: unknown encoding: cp65001`
   This is a known `Windows`/`python` bug ([_python bug_][1], [_stack overflow_][2]). The simplest solution is to set an
   environment variable which tells `python` how to handle this situation:
   ```cmd
   set PYTHONIOENCODING=UTF-8
   ```
   
   [1]: http://bugs.python.org/issue1602    "python bug"
   [2]: http://stackoverflow.com/questions/878972/windows-cmd-encoding-change-causes-python-crash    "stack overflow"



