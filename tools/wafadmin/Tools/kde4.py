#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

import os, sys, re
import Options, TaskGen, Task, Utils
from TaskGen import taskgen, feature, after

class msgfmt_taskgen(TaskGen.task_gen):
	def __init__(self, *k, **kw):
		TaskGen.task_gen.__init__(self, *k, **kw)

@feature('msgfmt')
def init_msgfmt(self):
	#langs = '' # for example "foo/fr foo/br"
	self.default_install_path = '${KDE4_LOCALE_INSTALL_DIR}'

@feature('msgfmt')
@after('init_msgfmt')
def apply_msgfmt(self):
	for lang in self.to_list(self.langs):
		node = self.path.find_resource(lang+'.po')
		task = self.create_task('msgfmt', node, node.change_ext('.mo'))

		if not self.bld.is_install: continue
		langname = lang.split('/')
		langname = langname[-1]
		task.install_path = self.install_path + os.sep + langname + os.sep + 'LC_MESSAGES'
		task.filename = getattr(self, 'appname', 'set_your_appname') + '.mo'
		task.chmod = self.chmod

def detect(conf):
	kdeconfig = conf.find_program('kde4-config')
	if not kdeconfig:
		conf.fatal('we need kde4-config')
	prefix = Utils.cmd_output('%s --prefix' % kdeconfig, silent=True).strip()
	file = '%s/share/apps/cmake/modules/KDELibsDependencies.cmake' % prefix
	try: os.stat(file)
	except OSError:
		file = '%s/share/kde4/apps/cmake/modules/KDELibsDependencies.cmake' % prefix
		try: os.stat(file)
		except OSError: conf.fatal('could not open %s' % file)

	try:
		txt = Utils.readf(file)
	except (OSError, IOError):
		conf.fatal('could not read %s' % file)

	txt = txt.replace('\\\n', '\n')
	fu = re.compile('#(.*)\n')
	txt = fu.sub('', txt)

	setregexp = re.compile('([sS][eE][tT]\s*\()\s*([^\s]+)\s+\"([^"]+)\"\)')
	found = setregexp.findall(txt)

	for (_, key, val) in found:
		#print key, val
		conf.env[key] = val

	# well well, i could just write an interpreter for cmake files
	conf.env['LIB_KDECORE']='kdecore'
	conf.env['LIB_KDEUI']  ='kdeui'
	conf.env['LIB_KIO']    ='kio'
	conf.env['LIB_KHTML']  ='khtml'
	conf.env['LIB_KPARTS'] ='kparts'

	conf.env['LIBPATH_KDECORE'] = conf.env['KDE4_LIB_INSTALL_DIR']
	conf.env['CPPPATH_KDECORE'] = conf.env['KDE4_INCLUDE_INSTALL_DIR']
	conf.env.append_value('CPPPATH_KDECORE', conf.env['KDE4_INCLUDE_INSTALL_DIR']+"/KDE")

	conf.env['MSGFMT'] = conf.find_program('msgfmt')

Task.simple_task_type('msgfmt', '${MSGFMT} ${SRC} -o ${TGT}', color='BLUE', shell=False)

