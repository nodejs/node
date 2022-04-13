import sys, os
import copy
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import testpy

def GetConfiguration(context, root):
  myContext = copy.copy(context)
  myContext.expect_fail = 1
  return testpy.SimpleTestConfiguration(myContext, root, 'known_issues')
