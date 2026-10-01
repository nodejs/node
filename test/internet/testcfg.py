import sys, os
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import testpy

def GetConfiguration(context, root):
  # TODO: Isolate shared listening ports before allowing concurrent tests.
  return testpy.SerialTestConfiguration(context, root, 'internet')
