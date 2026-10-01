import sys, os
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import testpy

def GetConfiguration(context, root):
  # TODO: Replace preexec_fn memory limits before parallelizing this suite.
  return testpy.SerialTestConfiguration(context, root, 'wasm-allocation')
