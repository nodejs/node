# Tips & Troubleshooting for working with the `node` code 

## Tips

1. If you build the code often, you should check out [`ninja`]. You may
   find it to be faster than `make`, much much faster than `MSBuild` (Windows), and less eager to rebuild unchanged
   sub-targets.

   [`ninja`]: ./building-node-with-ninja.md


## Troubleshooting

1. __Python crashes with `LookupError: unknown encoding: cp65001`__:  
   This is a known `Windows`/`python` bug ([_python bug_][1],
   [_stack overflow_][2]). The simplest solution is to set an
   environment variable which tells `python` how to handle this situation:
   ```cmd
   set PYTHONIOENCODING=UTF-8
   ```

   [1]: http://bugs.python.org/issue1602    "python bug"
   [2]: http://stackoverflow.com/questions/878972/windows-cmd-encoding-change-causes-python-crash    "stack overflow"



