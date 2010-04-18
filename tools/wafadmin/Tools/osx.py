#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy 2008

"""MacOSX related tools

To compile an executable into a Mac application bundle (a .app), set its 'mac_app' attribute
  obj.mac_app = True

To make a bundled shared library (a .bundle), set the 'mac_bundle' attribute:
  obj.mac_bundle = True
"""

import os, shutil, sys, platform
import TaskGen, Task, Build, Options, Utils
from TaskGen import taskgen, feature, after, before
from Logs import error, debug

# plist template
app_info = '''
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist SYSTEM "file://localhost/System/Library/DTDs/PropertyList.dtd">
<plist version="0.9">
<dict>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleGetInfoString</key>
	<string>Created by Waf</string>
	<key>CFBundleSignature</key>
	<string>????</string>
	<key>NOTE</key>
	<string>THIS IS A GENERATED FILE, DO NOT MODIFY</string>
	<key>CFBundleExecutable</key>
	<string>%s</string>
</dict>
</plist>
'''

# see WAF issue 285
# and also http://trac.macports.org/ticket/17059
@feature('cc', 'cxx')
@before('apply_lib_vars')
def set_macosx_deployment_target(self):
	if self.env['MACOSX_DEPLOYMENT_TARGET']:
		os.environ['MACOSX_DEPLOYMENT_TARGET'] = self.env['MACOSX_DEPLOYMENT_TARGET']
	elif 'MACOSX_DEPLOYMENT_TARGET' not in os.environ:
		if sys.platform == 'darwin':
			os.environ['MACOSX_DEPLOYMENT_TARGET'] = '.'.join(platform.mac_ver()[0].split('.')[:2])

@feature('cc', 'cxx')
@after('apply_lib_vars')
def apply_framework(self):
	for x in self.to_list(self.env['FRAMEWORKPATH']):
		frameworkpath_st = '-F%s'
		self.env.append_unique('CXXFLAGS', frameworkpath_st % x)
		self.env.append_unique('CCFLAGS', frameworkpath_st % x)
		self.env.append_unique('LINKFLAGS', frameworkpath_st % x)

	for x in self.to_list(self.env['FRAMEWORK']):
		self.env.append_value('LINKFLAGS', ['-framework', x])

@taskgen
def create_bundle_dirs(self, name, out):
	bld = self.bld
	dir = out.parent.get_dir(name)

	if not dir:
		dir = out.__class__(name, out.parent, 1)
		bld.rescan(dir)
		contents = out.__class__('Contents', dir, 1)
		bld.rescan(contents)
		macos = out.__class__('MacOS', contents, 1)
		bld.rescan(macos)
	return dir

def bundle_name_for_output(out):
	name = out.name
	k = name.rfind('.')
	if k >= 0:
		name = name[:k] + '.app'
	else:
		name = name + '.app'
	return name

@taskgen
@after('apply_link')
@feature('cprogram')
def create_task_macapp(self):
	"""Use env['MACAPP'] to force *all* executables to be transformed into Mac applications
	or use obj.mac_app = True to build specific targets as Mac apps"""
	if self.env['MACAPP'] or getattr(self, 'mac_app', False):
		apptask = self.create_task('macapp')
		apptask.set_inputs(self.link_task.outputs)

		out = self.link_task.outputs[0]

		name = bundle_name_for_output(out)
		dir = self.create_bundle_dirs(name, out)

		n1 = dir.find_or_declare(['Contents', 'MacOS', out.name])

		apptask.set_outputs([n1])
		apptask.chmod = 0755
		apptask.install_path = os.path.join(self.install_path, name, 'Contents', 'MacOS')
		self.apptask = apptask

@after('apply_link')
@feature('cprogram')
def create_task_macplist(self):
	"""Use env['MACAPP'] to force *all* executables to be transformed into Mac applications
	or use obj.mac_app = True to build specific targets as Mac apps"""
	if  self.env['MACAPP'] or getattr(self, 'mac_app', False):
		# check if the user specified a plist before using our template
		if not getattr(self, 'mac_plist', False):
			self.mac_plist = app_info

		plisttask = self.create_task('macplist')
		plisttask.set_inputs(self.link_task.outputs)

		out = self.link_task.outputs[0]
		self.mac_plist = self.mac_plist % (out.name)

		name = bundle_name_for_output(out)
		dir = self.create_bundle_dirs(name, out)

		n1 = dir.find_or_declare(['Contents', 'Info.plist'])

		plisttask.set_outputs([n1])
		plisttask.mac_plist = self.mac_plist
		plisttask.install_path = os.path.join(self.install_path, name, 'Contents')
		self.plisttask = plisttask

@after('apply_link')
@feature('cshlib')
def apply_link_osx(self):
	name = self.link_task.outputs[0].name
	if not self.install_path:
		return
	if getattr(self, 'vnum', None):
		name = name.replace('.dylib', '.%s.dylib' % self.vnum)

	path = os.path.join(Utils.subst_vars(self.install_path, self.env), name)
	if '-dynamiclib' in self.env['LINKFLAGS']:
		self.env.append_value('LINKFLAGS', '-install_name')
		self.env.append_value('LINKFLAGS', path)

@before('apply_link', 'apply_lib_vars')
@feature('cc', 'cxx')
def apply_bundle(self):
	"""use env['MACBUNDLE'] to force all shlibs into mac bundles
	or use obj.mac_bundle = True for specific targets only"""
	if not ('cshlib' in self.features or 'shlib' in self.features): return
	if self.env['MACBUNDLE'] or getattr(self, 'mac_bundle', False):
		self.env['shlib_PATTERN'] = self.env['macbundle_PATTERN']
		uselib = self.uselib = self.to_list(self.uselib)
		if not 'MACBUNDLE' in uselib: uselib.append('MACBUNDLE')

@after('apply_link')
@feature('cshlib')
def apply_bundle_remove_dynamiclib(self):
	if self.env['MACBUNDLE'] or getattr(self, 'mac_bundle', False):
		if not getattr(self, 'vnum', None):
			try:
				self.env['LINKFLAGS'].remove('-dynamiclib')
			except ValueError:
				pass

# TODO REMOVE IN 1.6 (global variable)
app_dirs = ['Contents', 'Contents/MacOS', 'Contents/Resources']

def app_build(task):
	env = task.env
	shutil.copy2(task.inputs[0].srcpath(env), task.outputs[0].abspath(env))

	return 0

def plist_build(task):
	env = task.env
	f = open(task.outputs[0].abspath(env), "w")
	f.write(task.mac_plist)
	f.close()

	return 0

Task.task_type_from_func('macapp', vars=[], func=app_build, after="cxx_link cc_link static_link")
Task.task_type_from_func('macplist', vars=[], func=plist_build, after="cxx_link cc_link static_link")

