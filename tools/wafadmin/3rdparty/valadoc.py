#! /usr/bin/env python
# encoding: UTF-8
# Nicolas Joseph 2009

from fnmatch import fnmatchcase
import os, os.path, re, stat
import Task, Utils, Node, Constants
from TaskGen import feature, extension, after
from Logs import debug, warn, error

VALADOC_STR = '${VALADOC}'

class valadoc_task(Task.Task):

  vars = ['VALADOC', 'VALADOCFLAGS']
  color = 'BLUE'
  after = 'cxx_link cc_link'
  quiet = True

  output_dir = ''
  doclet = ''
  package_name = ''
  package_version = ''
  files = []
  protected = False
  private = False
  inherit = False
  deps = False
  enable_non_null_experimental = False
  force = False

  def runnable_status(self):
    return True

  def run(self):
    if self.env['VALADOC']:
      if not self.env['VALADOCFLAGS']:
        self.env['VALADOCFLAGS'] = ''
      cmd = [Utils.subst_vars(VALADOC_STR, self.env)]
      cmd.append ('-o %s' % self.output_dir)
      if getattr(self, 'doclet', None):
        cmd.append ('--doclet %s' % self.doclet)
      cmd.append ('--package-name %s' % self.package_name)
      if getattr(self, 'version', None):
        cmd.append ('--package-version %s' % self.package_version)
      if getattr(self, 'packages', None):
        for package in self.packages:
          cmd.append ('--pkg %s' % package)
      if getattr(self, 'vapi_dirs', None):
        for vapi_dir in self.vapi_dirs:
          cmd.append ('--vapidir %s' % vapi_dir)
      if getattr(self, 'protected', None):
        cmd.append ('--protected')
      if getattr(self, 'private', None):
        cmd.append ('--private')
      if getattr(self, 'inherit', None):
        cmd.append ('--inherit')
      if getattr(self, 'deps', None):
        cmd.append ('--deps')
      if getattr(self, 'enable_non_null_experimental', None):
        cmd.append ('--enable-non-null-experimental')
      if getattr(self, 'force', None):
        cmd.append ('--force')
      cmd.append (' '.join ([x.relpath_gen (self.generator.bld.bldnode) for x in self.files]))
      return self.generator.bld.exec_command(' '.join(cmd))
    else:
      error ('You must install valadoc <http://live.gnome.org/Valadoc> for generate the API documentation')
      return -1

@feature('valadoc')
def process_valadoc(self):
  task = getattr(self, 'task', None)
  if not task:
    task = self.create_task('valadoc')
    self.task = task
    if getattr(self, 'output_dir', None):
      task.output_dir = self.output_dir
    else:
      Utils.WafError('no output directory')
    if getattr(self, 'doclet', None):
      task.doclet = self.doclet
    else:
      Utils.WafError('no doclet directory')
    if getattr(self, 'package_name', None):
      task.package_name = self.package_name
    else:
      Utils.WafError('no package name')
    if getattr(self, 'package_version', None):
      task.package_version = self.package_version
    if getattr(self, 'packages', None):
      task.packages = Utils.to_list(self.packages)
    if getattr(self, 'vapi_dirs', None):
      task.vapi_dirs = Utils.to_list(self.vapi_dirs)
    if getattr(self, 'files', None):
      task.files = self.files
    else:
      Utils.WafError('no input file')
    if getattr(self, 'protected', None):
      task.protected = self.protected
    if getattr(self, 'private', None):
      task.private = self.private
    if getattr(self, 'inherit', None):
      task.inherit = self.inherit
    if getattr(self, 'deps', None):
      task.deps = self.deps
    if getattr(self, 'enable_non_null_experimental', None):
      task.enable_non_null_experimental = self.enable_non_null_experimental
    if getattr(self, 'force', None):
      task.force = self.force

def detect(conf):
  conf.find_program('valadoc', var='VALADOC', mandatory=False)

