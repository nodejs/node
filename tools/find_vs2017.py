import re
import sys
import os
from ctypes import *

root_dir = os.path.dirname(__file__)
sys.path.insert(0, os.path.join(root_dir, 'comtypes'))

from comtypes import IUnknown
from comtypes import GUID
from comtypes import COMMETHOD
from comtypes import BSTR
from comtypes import DWORD
from comtypes.safearray import _midlSAFEARRAY
from comtypes.client import CreateObject


""" Find Visual Studio 2017 C/C++ compiler install location """

class ISetupInstance(IUnknown):
  _iid_ = GUID('{B41463C3-8866-43B5-BC33-2B0676F7F42E}')
  _methods_  = [
    COMMETHOD([], HRESULT, 'GetInstanceId',
              ( ['out'], POINTER(BSTR), 'pbstrInstanceId' ) ),
    COMMETHOD([], HRESULT, 'GetInstallDate',
              ( ['out'], POINTER(c_ulonglong), 'pInstallDate') ),
    COMMETHOD([], HRESULT, 'GetInstallationName',
              ( ['out'], POINTER(BSTR), 'pInstallationName') ),
    COMMETHOD([], HRESULT, 'GetInstallationPath',
              ( ['out'], POINTER(BSTR), 'pInstallationPath') ),
    COMMETHOD([], HRESULT, 'GetInstallationVersion',
              ( ['out'], POINTER(BSTR), 'pInstallationVersion') ),
    COMMETHOD([], HRESULT, 'GetDisplayName',
              ( ['in'], DWORD, 'lcid' ),
              ( ['out'], POINTER(BSTR), 'pDisplayName') ),
    COMMETHOD([], HRESULT, 'GetDescription',
              ( ['in'], DWORD, 'lcid' ),
              ( ['out'], POINTER(BSTR), 'pDescription') ),
    COMMETHOD([], HRESULT, 'ResolvePath',
              ( ['in'], c_wchar_p, 'pRelativePath' ),
              ( ['out'], POINTER(BSTR), 'pAbsolutePath') ),
  ]

class ISetupPackageReference(IUnknown):
  _iid_ = GUID('{da8d8a16-b2b6-4487-a2f1-594ccccd6bf5}')
  _methods_ = [
    COMMETHOD([], HRESULT, 'GetId',
              ( ['out'], POINTER(BSTR), 'pOut' ) ),
    COMMETHOD([], HRESULT, 'GetVersion',
              ( ['out'], POINTER(BSTR), 'pOut' ) ),
    COMMETHOD([], HRESULT, 'GetChip',
              ( ['out'], POINTER(BSTR), 'pOut' ) ),
    COMMETHOD([], HRESULT, 'GetLanguage',
              ( ['out'], POINTER(BSTR), 'pOut' ) ),
    COMMETHOD([], HRESULT, 'GetBranch',
              ( ['out'], POINTER(BSTR), 'pOut' ) ),
    COMMETHOD([], HRESULT, 'GetType',
              ( ['out'], POINTER(BSTR), 'pOut' ) ),
    COMMETHOD([], HRESULT, 'GetUniqueId',
              ( ['out'], POINTER(BSTR), 'pOut' ) )
  ]

class ISetupInstance2(ISetupInstance):
  _iid_ = GUID('{89143C9A-05AF-49B0-B717-72E218A2185C}')
  _methods_ = [
    COMMETHOD([], HRESULT, 'GetState',
              ( ['out'],  POINTER(DWORD), 'pState' ) ),
    COMMETHOD([], HRESULT, 'GetPackages',
              ( ['out'], POINTER(_midlSAFEARRAY(POINTER(ISetupPackageReference))), 'ppPackage' ) )
  ]

class IEnumSetupInstances(IUnknown):
  _iid_ = GUID('{6380BCFF-41D3-4B2E-8B2E-BF8A6810C848}')
  _methods_ = [
    COMMETHOD([], HRESULT, 'Next',
              ( ['in'], c_ulong, 'celt'),
              ( ['out'], POINTER(POINTER(ISetupInstance)), 'rgelt' ),
              ( ['out'], POINTER(c_ulong), 'pceltFetched' ) ),
    COMMETHOD([], HRESULT, 'Skip',
              ( ['in'], c_ulong, 'celt' ) ),
    COMMETHOD([], HRESULT, 'Reset'),
  ]

class ISetupConfiguration(IUnknown):
  _iid_ = GUID('{42843719-DB4C-46C2-8E7C-64F1816EFD5B}')
  _methods_ = [
    COMMETHOD([], HRESULT, 'EnumInstances',
              ( ['out'],  POINTER(POINTER(IEnumSetupInstances)), 'ppIESI' ) ),
    COMMETHOD([], HRESULT, 'GetInstanceForCurrentProcess',
              ( ['out'],  POINTER(POINTER(ISetupInstance)), 'ppISI' ) ),
    COMMETHOD([], HRESULT, 'GetInstanceForPath',
              ( ['in'], c_wchar_p, 'wzPath'),
              ( ['out'],  POINTER(POINTER(ISetupInstance)), 'ppISI' ) )
  ]

class ISetupConfiguration2(ISetupConfiguration) :
  _iid_ = GUID('{26AAB78C-4A60-49D6-AF3B-3C35BC93365D}')
  _methods_ = [
    COMMETHOD([], HRESULT, 'EnumAllInstances',
              ( ['out'],  POINTER(POINTER(IEnumSetupInstances)), 'ppIEnumSetupInstances' ) )
  ]


def GetVS2017CPPBasePath():
  installs = []
  iface = CreateObject(GUID('{177F0C4A-1CD3-4DE7-A32C-71DBBB9FA36D}'))
  setupConfiguration = iface.QueryInterface(ISetupConfiguration2)
  allInstances = setupConfiguration.EnumAllInstances()
  while True:
    result = allInstances.Next(1)
    instance = result[0]
    if not instance:
      break
    path = instance.GetInstallationPath()
    version = instance.GetInstallationVersion()
    instance2 = instance.QueryInterface(ISetupInstance2)
    packages = instance2.GetPackages()        
    for package in packages:
      packageId = package.GetId()
      if packageId == 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64':
        installs.append(path)
  return installs

def GetInstalledVS2017WinSDKs(vs_path):
  sdks = []
  has81sdk = False
  win8preg = re.compile(r"Microsoft.VisualStudio.Component.Windows81SDK")
  win10preg = re.compile(r"Microsoft.VisualStudio.Component.Windows10SDK.(\d+)")
  iface = CreateObject(GUID('{177F0C4A-1CD3-4DE7-A32C-71DBBB9FA36D}'))
  setupConfiguration = iface.QueryInterface(ISetupConfiguration2)
  allInstances = setupConfiguration.EnumAllInstances()
  while True:
    result = allInstances.Next(1)
    instance = result[0]
    if not instance:
      break
    path = instance.GetInstallationPath()
    if path != vs_path:
      continue
    instance2 = instance.QueryInterface(ISetupInstance2)
    packages = instance2.GetPackages()        
    for package in packages:
      packageId = package.GetId()
      if win8preg.match(packageId):
        has81sdk = True
      else:
        win10match = win10preg.match(packageId)
        if win10match:
          sdks.append('10.0.' + str(win10match.group(1)) + '.0')
    
    sdks.sort(reverse = True)
    if has81sdk:
      sdks.append('8.1')
    return sdks

def main():
  if len(sys.argv) == 1:
    installs = GetVS2017CPPBasePath()
    if len(installs) == 0:
      return
    for install in installs:
      sdks = GetInstalledVS2017WinSDKs(install)
      if len(sdks) > 0:
        print install
        return
    print installs[0]
  else:
    sdks = GetInstalledVS2017WinSDKs(sys.argv[1])
    if len(sdks) > 0:
      print sdks[0]

if __name__ == '__main__':
  main()
