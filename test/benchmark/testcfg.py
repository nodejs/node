import sys, os
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import testpy

def GetConfiguration(context, root):
  # TODO: Isolate fixed TCP/UDP ports before parallelizing benchmark tests.
  return testpy.SerialTestConfiguration(context, root, 'benchmark')
