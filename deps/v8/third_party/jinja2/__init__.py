"""Jinja is a template engine written in pure Python. It provides a
non-XML syntax that supports inline expressions and an optional
sandboxed environment.
"""
from .bccache import BytecodeCache as BytecodeCache
from .bccache import FileSystemBytecodeCache as FileSystemBytecodeCache
from .bccache import MemcachedBytecodeCache as MemcachedBytecodeCache
from .environment import Environment as Environment
from .environment import Template as Template
from .exceptions import TemplateAssertionError as TemplateAssertionError
from .exceptions import TemplateError as TemplateError
from .exceptions import TemplateNotFound as TemplateNotFound
from .exceptions import TemplateRuntimeError as TemplateRuntimeError
from .exceptions import TemplatesNotFound as TemplatesNotFound
from .exceptions import TemplateSyntaxError as TemplateSyntaxError
from .exceptions import UndefinedError as UndefinedError
from .loaders import BaseLoader as BaseLoader
from .loaders import ChoiceLoader as ChoiceLoader
from .loaders import DictLoader as DictLoader
from .loaders import FileSystemLoader as FileSystemLoader
from .loaders import FunctionLoader as FunctionLoader
from .loaders import ModuleLoader as ModuleLoader
from .loaders import PackageLoader as PackageLoader
from .loaders import PrefixLoader as PrefixLoader
from .runtime import ChainableUndefined as ChainableUndefined
from .runtime import DebugUndefined as DebugUndefined
from .runtime import make_logging_undefined as make_logging_undefined
from .runtime import StrictUndefined as StrictUndefined
from .runtime import Undefined as Undefined
from .utils import clear_caches as clear_caches
from .utils import is_undefined as is_undefined
from .utils import pass_context as pass_context
from .utils import pass_environment as pass_environment
from .utils import pass_eval_context as pass_eval_context
from .utils import select_autoescape as select_autoescape

__version__ = "3.1.2"
