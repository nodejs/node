# -*- coding: utf-8 -*-
import gc
import sys
import unittest
from markupsafe import Markup, escape, escape_silent
from markupsafe._compat import text_type, PY2


class MarkupTestCase(unittest.TestCase):

    def test_adding(self):
        # adding two strings should escape the unsafe one
        unsafe = '<script type="application/x-some-script">alert("foo");</script>'
        safe = Markup('<em>username</em>')
        assert unsafe + safe == text_type(escape(unsafe)) + text_type(safe)

    def test_string_interpolation(self):
        # string interpolations are safe to use too
        assert Markup('<em>%s</em>') % '<bad user>' == \
               '<em>&lt;bad user&gt;</em>'
        assert Markup('<em>%(username)s</em>') % {
            'username': '<bad user>'
        } == '<em>&lt;bad user&gt;</em>'

        assert Markup('%i') % 3.14 == '3'
        assert Markup('%.2f') % 3.14 == '3.14'

    def test_type_behavior(self):
        # an escaped object is markup too
        assert type(Markup('foo') + 'bar') is Markup

        # and it implements __html__ by returning itself
        x = Markup("foo")
        assert x.__html__() is x

    def test_html_interop(self):
        # it also knows how to treat __html__ objects
        class Foo(object):
            def __html__(self):
                return '<em>awesome</em>'
            def __unicode__(self):
                return 'awesome'
            __str__ = __unicode__
        assert Markup(Foo()) == '<em>awesome</em>'
        assert Markup('<strong>%s</strong>') % Foo() == \
            '<strong><em>awesome</em></strong>'

    def test_tuple_interpol(self):
        self.assertEqual(Markup('<em>%s:%s</em>') % (
            '<foo>',
            '<bar>',
        ), Markup(u'<em>&lt;foo&gt;:&lt;bar&gt;</em>'))

    def test_dict_interpol(self):
        self.assertEqual(Markup('<em>%(foo)s</em>') % {
            'foo': '<foo>',
        }, Markup(u'<em>&lt;foo&gt;</em>'))
        self.assertEqual(Markup('<em>%(foo)s:%(bar)s</em>') % {
            'foo': '<foo>',
            'bar': '<bar>',
        }, Markup(u'<em>&lt;foo&gt;:&lt;bar&gt;</em>'))

    def test_escaping(self):
        # escaping
        assert escape('"<>&\'') == '&#34;&lt;&gt;&amp;&#39;'
        assert Markup("<em>Foo &amp; Bar</em>").striptags() == "Foo & Bar"

    def test_unescape(self):
        assert Markup("&lt;test&gt;").unescape() == "<test>"
        assert "jack & tavi are cooler than mike & russ" == \
            Markup("jack & tavi are cooler than mike &amp; russ").unescape(), \
            Markup("jack & tavi are cooler than mike &amp; russ").unescape()

        # Test that unescape is idempotent
        original = '&foo&#x3b;'
        once = Markup(original).unescape()
        twice = Markup(once).unescape()
        expected = "&foo;"
        assert expected == once == twice, (once, twice)

    def test_formatting(self):
        for actual, expected in (
            (Markup('%i') % 3.14, '3'),
            (Markup('%.2f') % 3.14159, '3.14'),
            (Markup('%s %s %s') % ('<', 123, '>'), '&lt; 123 &gt;'),
            (Markup('<em>{awesome}</em>').format(awesome='<awesome>'),
             '<em>&lt;awesome&gt;</em>'),
            (Markup('{0[1][bar]}').format([0, {'bar': '<bar/>'}]),
             '&lt;bar/&gt;'),
            (Markup('{0[1][bar]}').format([0, {'bar': Markup('<bar/>')}]),
             '<bar/>')):
            assert actual == expected, "%r should be %r!" % (actual, expected)

    # This is new in 2.7
    if sys.version_info >= (2, 7):
        def test_formatting_empty(self):
            formatted = Markup('{}').format(0)
            assert formatted == Markup('0')

    def test_custom_formatting(self):
        class HasHTMLOnly(object):
            def __html__(self):
                return Markup('<foo>')

        class HasHTMLAndFormat(object):
            def __html__(self):
                return Markup('<foo>')
            def __html_format__(self, spec):
                return Markup('<FORMAT>')

        assert Markup('{0}').format(HasHTMLOnly()) == Markup('<foo>')
        assert Markup('{0}').format(HasHTMLAndFormat()) == Markup('<FORMAT>')

    def test_complex_custom_formatting(self):
        class User(object):
            def __init__(self, id, username):
                self.id = id
                self.username = username
            def __html_format__(self, format_spec):
                if format_spec == 'link':
                    return Markup('<a href="/user/{0}">{1}</a>').format(
                        self.id,
                        self.__html__(),
                    )
                elif format_spec:
                    raise ValueError('Invalid format spec')
                return self.__html__()
            def __html__(self):
                return Markup('<span class=user>{0}</span>').format(self.username)

        user = User(1, 'foo')
        assert Markup('<p>User: {0:link}').format(user) == \
            Markup('<p>User: <a href="/user/1"><span class=user>foo</span></a>')

    def test_formatting_with_objects(self):
        class Stringable(object):
            def __unicode__(self):
                return u'строка'
            if PY2:
                def __str__(self):
                    return 'some other value'
            else:
                __str__ = __unicode__

        assert Markup('{s}').format(s=Stringable()) == \
            Markup(u'строка')

    def test_all_set(self):
        import markupsafe as markup
        for item in markup.__all__:
            getattr(markup, item)

    def test_escape_silent(self):
        assert escape_silent(None) == Markup()
        assert escape(None) == Markup(None)
        assert escape_silent('<foo>') == Markup(u'&lt;foo&gt;')

    def test_splitting(self):
        self.assertEqual(Markup('a b').split(), [
            Markup('a'),
            Markup('b')
        ])
        self.assertEqual(Markup('a b').rsplit(), [
            Markup('a'),
            Markup('b')
        ])
        self.assertEqual(Markup('a\nb').splitlines(), [
            Markup('a'),
            Markup('b')
        ])

    def test_mul(self):
        self.assertEqual(Markup('a') * 3, Markup('aaa'))


class MarkupLeakTestCase(unittest.TestCase):

    def test_markup_leaks(self):
        counts = set()
        for count in range(20):
            for item in range(1000):
                escape("foo")
                escape("<foo>")
                escape(u"foo")
                escape(u"<foo>")
            if hasattr(sys, 'pypy_version_info'):
                gc.collect()
            counts.add(len(gc.get_objects()))
        assert len(counts) == 1, 'ouch, c extension seems to ' \
            'leak objects, got: ' + str(len(counts))


def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(MarkupTestCase))

    # this test only tests the c extension
    if not hasattr(escape, 'func_code'):
        suite.addTest(unittest.makeSuite(MarkupLeakTestCase))

    return suite


if __name__ == '__main__':
    unittest.main(defaultTest='suite')

# vim:sts=4:sw=4:et:
