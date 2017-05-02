# Tips, Tricks & Troubleshooting

## Tips

TBD

## Tricks

TDB

## Troubleshooting

1. Python crashes with `LookupError: unknown encoding: cp65001`
   This is a known `Windows`/`python` bug ([_python bug_][1], [_stack overflow_][2]). The simplest solution is to set an
   environment variable which tells `python` how to handle this situation:
   ```cmd
   set PYTHONIOENCODING=UTF-8
   ```
   
   [1]: http://bugs.python.org/issue1602    "python bug"
   [2]: http://stackoverflow.com/questions/878972/windows-cmd-encoding-change-causes-python-crash    "stack overflow"



