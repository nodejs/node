#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"""Environment representation

There is one gotcha: getitem returns [] if the contents evals to False
This means env['foo'] = {}; print env['foo'] will print [] not {}
"""

import os, copy, re
import Logs, Options, Utils
from Constants import *
re_imp = re.compile('^(#)*?([^#=]*?)\ =\ (.*?)$', re.M)

class Environment(object):
	"""A safe-to-use dictionary, but do not attach functions to it please (break cPickle)
	An environment instance can be stored into a file and loaded easily
	"""
	__slots__ = ("table", "parent")
	def __init__(self, filename=None):
		self.table = {}
		#self.parent = None

		if filename:
			self.load(filename)

	def __contains__(self, key):
		if key in self.table: return True
		try: return self.parent.__contains__(key)
		except AttributeError: return False # parent may not exist

	def __str__(self):
		keys = set()
		cur = self
		while cur:
			keys.update(cur.table.keys())
			cur = getattr(cur, 'parent', None)
		keys = list(keys)
		keys.sort()
		return "\n".join(["%r %r" % (x, self.__getitem__(x)) for x in keys])

	def __getitem__(self, key):
		try:
			while 1:
				x = self.table.get(key, None)
				if not x is None:
					return x
				self = self.parent
		except AttributeError:
			return []

	def __setitem__(self, key, value):
		self.table[key] = value

	def __delitem__(self, key):
		del self.table[key]
	
	def pop(self, key, *args):
		if len(args):
			return self.table.pop(key, *args)
		return self.table.pop(key)

	def set_variant(self, name):
		self.table[VARIANT] = name

	def variant(self):
		try:
			while 1:
				x = self.table.get(VARIANT, None)
				if not x is None:
					return x
				self = self.parent
		except AttributeError:
			return DEFAULT

	def copy(self):
		# TODO waf 1.6 rename this method derive, #368
		newenv = Environment()
		newenv.parent = self
		return newenv

	def detach(self):
		"""TODO try it
		modifying the original env will not change the copy"""
		tbl = self.get_merged_dict()
		try:
			delattr(self, 'parent')
		except AttributeError:
			pass
		else:
			keys = tbl.keys()
			for x in keys:
				tbl[x] = copy.deepcopy(tbl[x])
			self.table = tbl

	def get_flat(self, key):
		s = self[key]
		if isinstance(s, str): return s
		return ' '.join(s)

	def _get_list_value_for_modification(self, key):
		"""Gets a value that must be a list for further modification.  The
		list may be modified inplace and there is no need to
		"self.table[var] = value" afterwards.
		"""
		try:
			value = self.table[key]
		except KeyError:
			try: value = self.parent[key]
			except AttributeError: value = []
			if isinstance(value, list):
				value = value[:]
			else:
				value = [value]
		else:
			if not isinstance(value, list):
				value = [value]
		self.table[key] = value
		return value

	def append_value(self, var, value):
		current_value = self._get_list_value_for_modification(var)

		if isinstance(value, list):
			current_value.extend(value)
		else:
			current_value.append(value)

	def prepend_value(self, var, value):
		current_value = self._get_list_value_for_modification(var)

		if isinstance(value, list):
			current_value = value + current_value
			# a new list: update the dictionary entry
			self.table[var] = current_value
		else:
			current_value.insert(0, value)

	# prepend unique would be ambiguous
	def append_unique(self, var, value):
		current_value = self._get_list_value_for_modification(var)

		if isinstance(value, list):
			for value_item in value:
				if value_item not in current_value:
					current_value.append(value_item)
		else:
			if value not in current_value:
				current_value.append(value)

	def get_merged_dict(self):
		"""compute a merged table"""
		table_list = []
		env = self
		while 1:
			table_list.insert(0, env.table)
			try: env = env.parent
			except AttributeError: break
		merged_table = {}
		for table in table_list:
			merged_table.update(table)
		return merged_table

	def store(self, filename):
		"Write the variables into a file"
		file = open(filename, 'w')
		merged_table = self.get_merged_dict()
		keys = list(merged_table.keys())
		keys.sort()
		for k in keys: file.write('%s = %r\n' % (k, merged_table[k]))
		file.close()

	def load(self, filename):
		"Retrieve the variables from a file"
		tbl = self.table
		code = Utils.readf(filename)
		for m in re_imp.finditer(code):
			g = m.group
			tbl[g(2)] = eval(g(3))
		Logs.debug('env: %s', self.table)

	def get_destdir(self):
		"return the destdir, useful for installing"
		if self.__getitem__('NOINSTALL'): return ''
		return Options.options.destdir

	def update(self, d):
		for k, v in d.iteritems():
			self[k] = v


	def __getattr__(self, name):
		if name in self.__slots__:
			return object.__getattr__(self, name)
		else:
			return self[name]

	def __setattr__(self, name, value):
		if name in self.__slots__:
			object.__setattr__(self, name, value)
		else:
			self[name] = value

	def __delattr__(self, name):
		if name in self.__slots__:
			object.__delattr__(self, name)
		else:
			del self[name]

