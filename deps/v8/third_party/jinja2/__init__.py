# -*- coding: utf-8 -*-
"""Jinja is a template engine written in pure Python. It provides a
non-XML syntax that supports inline expressions and an optional
sandboxed environment.
"""
from markupsafe import escape
from markupsafe import Markup

from .bccache import BytecodeCache
from .bccache import FileSystemBytecodeCache
from .bccache import MemcachedBytecodeCache
from .environment import Environment
from .environment import Template
from .exceptions import TemplateAssertionError
from .exceptions import TemplateError
from .exceptions import TemplateNotFound
from .exceptions import TemplateRuntimeError
from .exceptions import TemplatesNotFound
from .exceptions import TemplateSyntaxError
from .exceptions import UndefinedError
from .filters import contextfilter
from .filters import environmentfilter
from .filters import evalcontextfilter
from .loaders import BaseLoader
from .loaders import ChoiceLoader
from .loaders import DictLoader
from .loaders import FileSystemLoader
from .loaders import FunctionLoader
from .loaders import ModuleLoader
from .loaders import PackageLoader
from .loaders import PrefixLoader
from .runtime import ChainableUndefined
from .runtime import DebugUndefined
from .runtime import make_logging_undefined
from .runtime import StrictUndefined
from .runtime import Undefined
from .utils import clear_caches
from .utils import contextfunction
from .utils import environmentfunction
from .utils import evalcontextfunction
from .utils import is_undefined
from .utils import select_autoescape

__version__ = "2.11.3"
