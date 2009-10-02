#!/usr/bin/env python
# encoding: utf-8
# Ali Sabil, 2007

import TaskGen

TaskGen.declare_chain(
	name = 'gob2',
	rule = '${GOB2} -o ${TGT[0].bld_dir(env)} ${GOB2FLAGS} ${SRC}',
	ext_in = '.gob',
	ext_out = '.c'
)

def detect(conf):
	gob2 = conf.find_program('gob2', var='GOB2', mandatory=True)
	conf.env['GOB2'] = gob2
	conf.env['GOB2FLAGS'] = ''

