# -*- coding: utf-8 -*-
"""The code for async support. Importing this patches Jinja on supported
Python versions.
"""
import asyncio
import inspect
from functools import update_wrapper

from markupsafe import Markup

from .environment import TemplateModule
from .runtime import LoopContext
from .utils import concat
from .utils import internalcode
from .utils import missing


async def concat_async(async_gen):
    rv = []

    async def collect():
        async for event in async_gen:
            rv.append(event)

    await collect()
    return concat(rv)


async def generate_async(self, *args, **kwargs):
    vars = dict(*args, **kwargs)
    try:
        async for event in self.root_render_func(self.new_context(vars)):
            yield event
    except Exception:
        yield self.environment.handle_exception()


def wrap_generate_func(original_generate):
    def _convert_generator(self, loop, args, kwargs):
        async_gen = self.generate_async(*args, **kwargs)
        try:
            while 1:
                yield loop.run_until_complete(async_gen.__anext__())
        except StopAsyncIteration:
            pass

    def generate(self, *args, **kwargs):
        if not self.environment.is_async:
            return original_generate(self, *args, **kwargs)
        return _convert_generator(self, asyncio.get_event_loop(), args, kwargs)

    return update_wrapper(generate, original_generate)


async def render_async(self, *args, **kwargs):
    if not self.environment.is_async:
        raise RuntimeError("The environment was not created with async mode enabled.")

    vars = dict(*args, **kwargs)
    ctx = self.new_context(vars)

    try:
        return await concat_async(self.root_render_func(ctx))
    except Exception:
        return self.environment.handle_exception()


def wrap_render_func(original_render):
    def render(self, *args, **kwargs):
        if not self.environment.is_async:
            return original_render(self, *args, **kwargs)
        loop = asyncio.get_event_loop()
        return loop.run_until_complete(self.render_async(*args, **kwargs))

    return update_wrapper(render, original_render)


def wrap_block_reference_call(original_call):
    @internalcode
    async def async_call(self):
        rv = await concat_async(self._stack[self._depth](self._context))
        if self._context.eval_ctx.autoescape:
            rv = Markup(rv)
        return rv

    @internalcode
    def __call__(self):
        if not self._context.environment.is_async:
            return original_call(self)
        return async_call(self)

    return update_wrapper(__call__, original_call)


def wrap_macro_invoke(original_invoke):
    @internalcode
    async def async_invoke(self, arguments, autoescape):
        rv = await self._func(*arguments)
        if autoescape:
            rv = Markup(rv)
        return rv

    @internalcode
    def _invoke(self, arguments, autoescape):
        if not self._environment.is_async:
            return original_invoke(self, arguments, autoescape)
        return async_invoke(self, arguments, autoescape)

    return update_wrapper(_invoke, original_invoke)


@internalcode
async def get_default_module_async(self):
    if self._module is not None:
        return self._module
    self._module = rv = await self.make_module_async()
    return rv


def wrap_default_module(original_default_module):
    @internalcode
    def _get_default_module(self):
        if self.environment.is_async:
            raise RuntimeError("Template module attribute is unavailable in async mode")
        return original_default_module(self)

    return _get_default_module


async def make_module_async(self, vars=None, shared=False, locals=None):
    context = self.new_context(vars, shared, locals)
    body_stream = []
    async for item in self.root_render_func(context):
        body_stream.append(item)
    return TemplateModule(self, context, body_stream)


def patch_template():
    from . import Template

    Template.generate = wrap_generate_func(Template.generate)
    Template.generate_async = update_wrapper(generate_async, Template.generate_async)
    Template.render_async = update_wrapper(render_async, Template.render_async)
    Template.render = wrap_render_func(Template.render)
    Template._get_default_module = wrap_default_module(Template._get_default_module)
    Template._get_default_module_async = get_default_module_async
    Template.make_module_async = update_wrapper(
        make_module_async, Template.make_module_async
    )


def patch_runtime():
    from .runtime import BlockReference, Macro

    BlockReference.__call__ = wrap_block_reference_call(BlockReference.__call__)
    Macro._invoke = wrap_macro_invoke(Macro._invoke)


def patch_filters():
    from .filters import FILTERS
    from .asyncfilters import ASYNC_FILTERS

    FILTERS.update(ASYNC_FILTERS)


def patch_all():
    patch_template()
    patch_runtime()
    patch_filters()


async def auto_await(value):
    if inspect.isawaitable(value):
        return await value
    return value


async def auto_aiter(iterable):
    if hasattr(iterable, "__aiter__"):
        async for item in iterable:
            yield item
        return
    for item in iterable:
        yield item


class AsyncLoopContext(LoopContext):
    _to_iterator = staticmethod(auto_aiter)

    @property
    async def length(self):
        if self._length is not None:
            return self._length

        try:
            self._length = len(self._iterable)
        except TypeError:
            iterable = [x async for x in self._iterator]
            self._iterator = self._to_iterator(iterable)
            self._length = len(iterable) + self.index + (self._after is not missing)

        return self._length

    @property
    async def revindex0(self):
        return await self.length - self.index

    @property
    async def revindex(self):
        return await self.length - self.index0

    async def _peek_next(self):
        if self._after is not missing:
            return self._after

        try:
            self._after = await self._iterator.__anext__()
        except StopAsyncIteration:
            self._after = missing

        return self._after

    @property
    async def last(self):
        return await self._peek_next() is missing

    @property
    async def nextitem(self):
        rv = await self._peek_next()

        if rv is missing:
            return self._undefined("there is no next item")

        return rv

    def __aiter__(self):
        return self

    async def __anext__(self):
        if self._after is not missing:
            rv = self._after
            self._after = missing
        else:
            rv = await self._iterator.__anext__()

        self.index0 += 1
        self._before = self._current
        self._current = rv
        return rv, self


async def make_async_loop_context(iterable, undefined, recurse=None, depth0=0):
    import warnings

    warnings.warn(
        "This template must be recompiled with at least Jinja 2.11, or"
        " it will fail in 3.0.",
        DeprecationWarning,
        stacklevel=2,
    )
    return AsyncLoopContext(iterable, undefined, recurse, depth0)


patch_all()
