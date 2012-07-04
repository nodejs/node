# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
SCons generator.

This contains class definitions and supporting functions for generating
pieces of SCons files for the different types of GYP targets.
"""

import os


def WriteList(fp, list, prefix='',
                        separator=',\n    ',
                        preamble=None,
                        postamble=None):
  fp.write(preamble or '')
  fp.write((separator or ' ').join([prefix + l for l in list]))
  fp.write(postamble or '')


class TargetBase(object):
  """
  Base class for a SCons representation of a GYP target.
  """
  is_ignored = False
  target_prefix = ''
  target_suffix = ''
  def __init__(self, spec):
    self.spec = spec
  def full_product_name(self):
    """
    Returns the full name of the product being built:

      * Uses 'product_name' if it's set, else prefix + 'target_name'.
      * Prepends 'product_dir' if set.
      * Appends SCons suffix variables for the target type (or
        product_extension).
    """
    suffix = self.target_suffix
    product_extension = self.spec.get('product_extension')
    if product_extension:
      suffix = '.' + product_extension
    prefix = self.spec.get('product_prefix', self.target_prefix)
    name = self.spec['target_name']
    name = prefix + self.spec.get('product_name', name) + suffix
    product_dir = self.spec.get('product_dir')
    if product_dir:
      name = os.path.join(product_dir, name)
    else:
      name = os.path.join(self.out_dir, name)
    return name

  def write_input_files(self, fp):
    """
    Writes the definition of the input files (sources).
    """
    sources = self.spec.get('sources')
    if not sources:
      fp.write('\ninput_files = []\n')
      return
    preamble = '\ninput_files = [\n    '
    postamble = ',\n]\n'
    WriteList(fp, map(repr, sources), preamble=preamble, postamble=postamble)

  def builder_call(self):
    """
    Returns the actual SCons builder call to build this target.
    """
    name = self.full_product_name()
    return 'env.%s(env.File(%r), input_files)' % (self.builder_name, name)
  def write_target(self, fp, src_dir='', pre=''):
    """
    Writes the lines necessary to build this target.
    """
    fp.write('\n' + pre)
    fp.write('_outputs = %s\n' % self.builder_call())
    fp.write('target_files.extend(_outputs)\n')


class NoneTarget(TargetBase):
  """
  A GYP target type of 'none', implicitly or explicitly.
  """
  def write_target(self, fp, src_dir='', pre=''):
    fp.write('\ntarget_files.extend(input_files)\n')


class SettingsTarget(TargetBase):
  """
  A GYP target type of 'settings'.
  """
  is_ignored = True


compilable_sources_template = """
_result = []
for infile in input_files:
  if env.compilable(infile):
    if (type(infile) == type('')
        and (infile.startswith(%(src_dir)r)
             or not os.path.isabs(env.subst(infile)))):
      # Force files below the build directory by replacing all '..'
      # elements in the path with '__':
      base, ext = os.path.splitext(os.path.normpath(infile))
      base = [d == '..' and '__' or d for d in base.split('/')]
      base = os.path.join(*base)
      object = '${OBJ_DIR}/${COMPONENT_NAME}/${TARGET_NAME}/' + base
      if not infile.startswith(%(src_dir)r):
        infile = %(src_dir)r + infile
      infile = env.%(name)s(object, infile)[0]
    else:
      infile = env.%(name)s(infile)[0]
  _result.append(infile)
input_files = _result
"""

class CompilableSourcesTargetBase(TargetBase):
  """
  An abstract base class for targets that compile their source files.

  We explicitly transform compilable files into object files,
  even though SCons could infer that for us, because we want
  to control where the object file ends up.  (The implicit rules
  in SCons always put the object file next to the source file.)
  """
  intermediate_builder_name = None
  def write_target(self, fp, src_dir='', pre=''):
    if self.intermediate_builder_name is None:
      raise NotImplementedError
    if src_dir and not src_dir.endswith('/'):
      src_dir += '/'
    variables = {
        'src_dir': src_dir,
        'name': self.intermediate_builder_name,
    }
    fp.write(compilable_sources_template % variables)
    super(CompilableSourcesTargetBase, self).write_target(fp)


class ProgramTarget(CompilableSourcesTargetBase):
  """
  A GYP target type of 'executable'.
  """
  builder_name = 'GypProgram'
  intermediate_builder_name = 'StaticObject'
  target_prefix = '${PROGPREFIX}'
  target_suffix = '${PROGSUFFIX}'
  out_dir = '${TOP_BUILDDIR}'


class StaticLibraryTarget(CompilableSourcesTargetBase):
  """
  A GYP target type of 'static_library'.
  """
  builder_name = 'GypStaticLibrary'
  intermediate_builder_name = 'StaticObject'
  target_prefix = '${LIBPREFIX}'
  target_suffix = '${LIBSUFFIX}'
  out_dir = '${LIB_DIR}'


class SharedLibraryTarget(CompilableSourcesTargetBase):
  """
  A GYP target type of 'shared_library'.
  """
  builder_name = 'GypSharedLibrary'
  intermediate_builder_name = 'SharedObject'
  target_prefix = '${SHLIBPREFIX}'
  target_suffix = '${SHLIBSUFFIX}'
  out_dir = '${LIB_DIR}'


class LoadableModuleTarget(CompilableSourcesTargetBase):
  """
  A GYP target type of 'loadable_module'.
  """
  builder_name = 'GypLoadableModule'
  intermediate_builder_name = 'SharedObject'
  target_prefix = '${SHLIBPREFIX}'
  target_suffix = '${SHLIBSUFFIX}'
  out_dir = '${TOP_BUILDDIR}'


TargetMap = {
  None : NoneTarget,
  'none' : NoneTarget,
  'settings' : SettingsTarget,
  'executable' : ProgramTarget,
  'static_library' : StaticLibraryTarget,
  'shared_library' : SharedLibraryTarget,
  'loadable_module' : LoadableModuleTarget,
}


def Target(spec):
  return TargetMap[spec.get('type')](spec)
