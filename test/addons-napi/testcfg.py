import sys, os
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import testpy

def GetConfiguration(context, root):
  return testpy.AddonTestConfiguration(context, root, 'addons-napi', ['--napi-modules'])
