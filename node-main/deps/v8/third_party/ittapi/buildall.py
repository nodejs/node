#!/usr/bin/env python
#
# Copyright (C) 2005-2019 Intel Corporation
#
# SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause
#

from __future__ import print_function
import os
import sys
import shutil
import fnmatch
import subprocess


def run_shell(cmd):
    print("\n>>", cmd)
    os.system(cmd)


if sys.platform == 'win32':
    def read_registry(path, depth=0xFFFFFFFF, statics={}):
        try:
            import _winreg
        except ImportError:
            import winreg as _winreg
        parts = path.split('\\')
        hub = parts[0]
        path = '\\'.join(parts[1:])
        if not statics:
            statics['hubs'] = {'HKLM': _winreg.HKEY_LOCAL_MACHINE, 'HKCL': _winreg.HKEY_CLASSES_ROOT}

        def enum_nodes(curpath, level):
            if level < 1:
                return {}
            res = {}
            try:
                aKey = _winreg.OpenKey(statics['hubs'][hub], curpath, 0, _winreg.KEY_READ | _winreg.KEY_WOW64_64KEY)
            except WindowsError:
                return res

            try:
                i = 0
                while True:
                    name, value, _ = _winreg.EnumValue(aKey, i)
                    i += 1
                    res[name] = value
            except WindowsError:
                pass

            keys = []
            try:
                i = 0
                while True:
                    key = _winreg.EnumKey(aKey, i)
                    i += 1
                    keys.append(key)
            except WindowsError:
                pass

            _winreg.CloseKey(aKey)

            for key in keys:
                res[key] = enum_nodes(curpath + '\\' + key, level - 1)

            return res

        return enum_nodes(path, depth)


def get_vs_versions():  # https://www.mztools.com/articles/2008/MZ2008003.aspx
    if sys.platform != 'win32':
        return []
    versions = []

    hkcl = read_registry(r'HKCL', 1)
    for key in hkcl:
        if 'VisualStudio.DTE.' in key:
            version = key.split('.')[2]
            if int(version) >= 12:
                versions.append(version)

    if not versions:
        print("No Visual Studio version found")
    return sorted(versions)


def detect_cmake():
    if sys.platform == 'darwin':
        path, err = subprocess.Popen("which cmake", shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()
        if not path.strip():
            path, err = subprocess.Popen("which xcrun", shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()
            if not path.strip():
                print("No cmake and no XCode found...")
                return None
            return 'xcrun cmake'
    return 'cmake'


def main():
    import argparse
    parser = argparse.ArgumentParser()
    vs_versions = get_vs_versions()
    parser.add_argument("-d", "--debug", help="specify debug build configuration (release by default)", action="store_true")
    parser.add_argument("-c", "--clean", help="delete any intermediate and output files", action="store_true")
    parser.add_argument("-v", "--verbose", help="enable verbose output from build process", action="store_true")
    parser.add_argument("-pt", "--ptmark", help="enable anomaly detection support", action="store_true")
    parser.add_argument("--force_bits", choices=["32", "64"], help="specify bit version for the target")
    if sys.platform == 'win32' and vs_versions:
        parser.add_argument("--vs", help="specify visual studio version {default}", choices=vs_versions, default=vs_versions[0])
    args = parser.parse_args()

    if args.force_bits:
        target_bits = [args.force_bits]
    else:
        target_bits = ['64']
        if (sys.platform != 'darwin'):  # on MAC OSX we produce FAT library including both 32 and 64 bits
            target_bits.append('32')

    print("target_bits", target_bits)
    work_dir = os.getcwd()
    if args.clean:
        bin_dir = os.path.join(work_dir, 'bin')
        if os.path.exists(bin_dir):
            shutil.rmtree(bin_dir)
    for bits in target_bits:
        work_folder = os.path.join(work_dir, "build_" + (sys.platform.replace('32', "")), bits)
        already_there = os.path.exists(work_folder)
        if already_there and args.clean:
            shutil.rmtree(work_folder)
            already_there = False
        if not already_there:
            os.makedirs(work_folder)
        print("work_folder: ", work_folder)
        os.chdir(work_folder)
        if args.clean:
            continue

        cmake = detect_cmake()
        if not cmake:
            print("Error: cmake is not found")
            return

        if sys.platform == 'win32':
            if vs_versions:
                generator = ('Visual Studio %s' % args.vs) + (' Win64' if bits == '64' else '')
            else:
                generator = 'Ninja'
        else:
            generator = 'Unix Makefiles'
        run_shell('%s "%s" -G"%s" %s' % (cmake, work_dir, generator, " ".join([
            ("-DFORCE_32=ON" if bits == '32' else ""),
            ("-DCMAKE_BUILD_TYPE=Debug" if args.debug else ""),
            ('-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON' if args.verbose else ''),
            ("-DITT_API_IPT_SUPPORT=1" if args.ptmark else "")
        ])))
        if sys.platform == 'win32':
            target_project = 'ALL_BUILD'
            run_shell('%s --build . --config %s --target %s' % (cmake, ('Debug' if args.debug else 'Release'), target_project))
        else:
            import glob
            run_shell('%s --build . --config %s' % (cmake, ('Debug' if args.debug else 'Release')))
            if ('linux' in sys.platform and bits == '64'):
                continue
            run_shell('%s --build . --config %s --target' % (cmake, ('Debug' if args.debug else 'Release')))

if __name__== "__main__":
    main()

