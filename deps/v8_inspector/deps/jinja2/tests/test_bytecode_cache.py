# -*- coding: utf-8 -*-
"""
    jinja2.testsuite.bytecode_cache
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Test bytecode caching

    :copyright: (c) 2010 by the Jinja Team.
    :license: BSD, see LICENSE for more details.
"""
import pytest
from jinja2 import Environment
from jinja2.bccache import FileSystemBytecodeCache
from jinja2.exceptions import TemplateNotFound


@pytest.fixture
def env(package_loader):
    bytecode_cache = FileSystemBytecodeCache()
    return Environment(
        loader=package_loader,
        bytecode_cache=bytecode_cache,
    )


@pytest.mark.byte_code_cache
class TestByteCodeCache():

    def test_simple(self, env):
        tmpl = env.get_template('test.html')
        assert tmpl.render().strip() == 'BAR'
        pytest.raises(TemplateNotFound, env.get_template, 'missing.html')
