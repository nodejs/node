#/* <copyright>
#  This file is provided under a dual BSD/GPLv2 license.  When using or
#  redistributing this file, you may do so under either license.
#
#  GPL LICENSE SUMMARY
#
#  Copyright (c) 2005-2017 Intel Corporation. All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of version 2 of the GNU General Public License as
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#  The full GNU General Public License is included in this distribution
#  in the file called LICENSE.GPL.
#
#  Contact Information:
#  http://software.intel.com/en-us/articles/intel-vtune-amplifier-xe/
#
#  BSD LICENSE
#
#  Copyright (c) 2005-2017 Intel Corporation. All rights reserved.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in
#      the documentation and/or other materials provided with the
#      distribution.
#    * Neither the name of Intel Corporation nor the names of its
#      contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#</copyright> */
#********************************************************************************************************************************************************************************************************************************************************************************************

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
    parser.add_argument("-d", "--debug", action="store_true")
    parser.add_argument("-v", "--verbose", action="store_true")
    if sys.platform == 'win32' and vs_versions:
        parser.add_argument("--vs", choices=vs_versions, default=vs_versions[0])
    args = parser.parse_args()

    target_bits = ['64']
    if (sys.platform != 'darwin'):  # on MAC OSX we produce FAT library including both 32 and 64 bits
        target_bits.append('32')

    print("target_bits", target_bits)
    work_dir = os.getcwd()
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
            ('-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON' if args.verbose else '')
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

