#!/usr/bin/python
# encoding: utf-8
# Grygoriy Fuchedzhy 2009

"""
Compile fluid files (fltk graphic library). Use the 'fluid' feature in conjuction with the 'cxx' feature.
"""

import Task
from TaskGen import extension

Task.simple_task_type('fluid', '${FLUID} -c -o ${TGT[0].abspath(env)} -h ${TGT[1].abspath(env)} ${SRC}', 'BLUE', shell=False, ext_out='.cxx')

@extension('.fl')
def fluid(self, node):
	"""add the .fl to the source list; the cxx file generated will be compiled when possible"""
	cpp = node.change_ext('.cpp')
	hpp = node.change_ext('.hpp')
	self.create_task('fluid', node, [cpp, hpp])

	if 'cxx' in self.features:
		self.allnodes.append(cpp)

def detect(conf):
    fluid = conf.find_program('fluid', var='FLUID', mandatory=True)
    conf.check_cfg(path='fltk-config', package='', args='--cxxflags --ldflags', uselib_store='FLTK', mandatory=True)

