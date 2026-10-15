import contextlib
import io
import os
import sys
import unittest
from os import path

ROOT = path.abspath(path.join(path.dirname(__file__), '..', '..'))
sys.path.insert(0, path.join(ROOT, 'tools'))
import generate_sbom as sbom


class TestDepCatalog(unittest.TestCase):
  """Validate that the DEP_CATALOG entries are structurally correct."""

  def test_all_entries_have_required_fields(self):
    for dep in sbom.DEP_CATALOG:
      for field in ('name', 'description', 'purl_base', 'version_fn'):
        self.assertIn(field, dep,
          'DEP_CATALOG entry %r is missing field %r' % (dep.get('name'), field))

  def test_version_fn_is_callable(self):
    for dep in sbom.DEP_CATALOG:
      self.assertTrue(callable(dep['version_fn']),
        'version_fn for %r must be callable' % dep['name'])

  def test_scope_values(self):
    valid_scopes = {'required', 'optional', 'excluded'}
    for dep in sbom.DEP_CATALOG:
      scope = dep.get('scope', 'required')
      self.assertIn(scope, valid_scopes,
        'invalid scope %r for %r' % (scope, dep['name']))

  def test_component_type_values(self):
    valid_types = {'library', 'framework'}
    for dep in sbom.DEP_CATALOG:
      component_type = dep.get('component_type', 'library')
      self.assertIn(component_type, valid_types,
        'invalid component_type %r for %r' % (component_type, dep['name']))

  def test_purl_base_has_no_version(self):
    for dep in sbom.DEP_CATALOG:
      self.assertTrue(dep['purl_base'].startswith('pkg:'),
        'purl_base for %r must start with "pkg:"' % dep['name'])
      self.assertNotIn('@', dep['purl_base'],
        'purl_base for %r must not include a version' % dep['name'])

  def test_no_duplicate_names(self):
    names = [dep['name'] for dep in sbom.DEP_CATALOG]
    self.assertEqual(len(names), len(set(names)),
      'DEP_CATALOG contains duplicate names: %r' %
      [n for n in names if names.count(n) > 1])

  def test_deps_dir_coverage(self):
    """Every first-level subdirectory of deps/ needs a DEP_CATALOG entry."""
    deps_dir = path.join(ROOT, 'deps')
    if not path.isdir(deps_dir):
      self.skipTest('deps/ directory not present')
    # Directories that are not dependencies in their own right.
    known_non_deps = {
      'icu-tmp',    # temporary download target
      'icu-small',  # bundled subset of ICU, covered by the 'icu' entry
    }
    # Directory names that differ from the canonical dependency name.
    dir_to_catalog = {
      'uv': 'libuv',
      'cares': 'c-ares',
      'histogram': 'hdr_histogram',
      'crates': 'v8-crates',
    }
    catalog_names = {dep['name'] for dep in sbom.DEP_CATALOG}
    missing = []
    for entry in sorted(os.listdir(deps_dir)):
      if not path.isdir(path.join(deps_dir, entry)) or entry in known_non_deps:
        continue
      if dir_to_catalog.get(entry, entry) not in catalog_names:
        missing.append(entry)
    self.assertEqual(missing, [],
      'deps/ subdirectories with no DEP_CATALOG entry: %r' % missing)


class TestVersionExtractors(unittest.TestCase):
  """Smoke-test the version extractors against real files in the repo."""

  def _require(self, *parts):
    p = path.join(ROOT, *parts)
    if not path.exists(p):
      self.skipTest('file not present: %s' % p)
    return p

  def test_version_from_package_json(self):
    p = self._require('deps', 'npm', 'package.json')
    self.assertRegex(sbom.version_from_package_json(p), r'^\d+\.\d+\.\d+')

  def test_version_from_c_macros(self):
    p = self._require('deps', 'uv', 'include', 'uv', 'version.h')
    version = sbom.version_from_c_macros(
      p, 'UV_VERSION_MAJOR', 'UV_VERSION_MINOR', 'UV_VERSION_PATCH')
    self.assertRegex(version, r'^\d+\.\d+\.\d+$')

  def test_version_from_string_macro(self):
    p = self._require('deps', 'ada', 'ada.h')
    self.assertRegex(
      sbom.version_from_string_macro(p, 'ADA_VERSION'), r'^\d+\.')

  def test_version_from_cmake(self):
    p = self._require('deps', 'zlib', 'CMakeLists.txt')
    self.assertRegex(sbom.version_from_cmake(p), r'^\d+\.')

  def test_version_from_icu_dep(self):
    p = self._require('tools', 'icu', 'current_ver.dep')
    self.assertRegex(sbom.version_from_icu_dep(p), r'^\d+\.')

  def test_version_from_plain_file(self):
    p = self._require('deps', 'perfetto', 'VERSION')
    self.assertNotEqual(sbom.version_from_plain_file(p), sbom.UNKNOWN)

  def test_version_from_openssl(self):
    self.assertRegex(sbom.version_from_openssl(sbom.REPO_ROOT), r'^\d+\.\d+\.')

  def test_missing_file_is_unknown(self):
    missing = path.join(ROOT, 'deps', 'does-not-exist', 'version.h')
    # The extractors warn on stderr rather than raising.
    with contextlib.redirect_stderr(io.StringIO()) as stderr:
      version = sbom.version_from_string_macro(missing, 'FOO_VERSION')
    self.assertEqual(version, sbom.UNKNOWN)
    self.assertIn('cannot read', stderr.getvalue())

  def test_missing_macro_is_unknown(self):
    p = self._require('deps', 'ada', 'ada.h')
    with contextlib.redirect_stderr(io.StringIO()) as stderr:
      version = sbom.version_from_string_macro(p, 'NOT_A_MACRO')
    self.assertEqual(version, sbom.UNKNOWN)
    self.assertIn('no match', stderr.getvalue())


class TestResolveScope(unittest.TestCase):
  """Verify that resolve_scope() interprets config.gypi variables correctly."""

  def test_dependency_without_flags_is_required(self):
    self.assertEqual(sbom.resolve_scope({}, {}), 'required')

  def test_disabled_use_flag_is_excluded(self):
    dep = {'use_flag': 'node_use_sqlite'}
    self.assertEqual(
      sbom.resolve_scope(dep, {'node_use_sqlite': 'false'}), 'excluded')

  def test_enabled_use_flag_keeps_scope(self):
    dep = {'use_flag': 'node_use_sqlite', 'scope': 'optional'}
    self.assertEqual(
      sbom.resolve_scope(dep, {'node_use_sqlite': 'true'}), 'optional')

  def test_shared_flag_is_excluded(self):
    dep = {'shared_flag': 'node_shared_zlib'}
    self.assertEqual(
      sbom.resolve_scope(dep, {'node_shared_zlib': 'true'}), 'excluded')

  def test_bundled_shared_flag_keeps_scope(self):
    dep = {'shared_flag': 'node_shared_zlib'}
    self.assertEqual(
      sbom.resolve_scope(dep, {'node_shared_zlib': 'false'}), 'required')

  def test_excluded_scope_stays_excluded(self):
    self.assertEqual(sbom.resolve_scope({'scope': 'excluded'}, {}), 'excluded')

  def test_excluded_when_matches(self):
    dep = {
      'scope': 'optional',
      'use_flag': 'node_use_quic',
      'excluded_when': [('node_shared_openssl', 'true')],
    }
    config_vars = {'node_use_quic': 'true', 'node_shared_openssl': 'true'}
    self.assertEqual(sbom.resolve_scope(dep, config_vars), 'excluded')

  def test_excluded_when_does_not_match(self):
    dep = {
      'scope': 'optional',
      'use_flag': 'node_use_quic',
      'excluded_when': [('node_shared_openssl', 'true')],
    }
    config_vars = {'node_use_quic': 'true', 'node_shared_openssl': 'false'}
    self.assertEqual(sbom.resolve_scope(dep, config_vars), 'optional')


class TestBuildComponent(unittest.TestCase):

  def test_fields(self):
    dep = {
      'name': 'zlib',
      'description': 'Compression library',
      'purl_base': 'pkg:github/madler/zlib',
    }
    component = sbom.build_component(dep, '1.3.0', 'required')
    self.assertEqual(component['bom-ref'], 'zlib@1.3.0')
    self.assertEqual(component['type'], 'library')
    self.assertEqual(component['version'], '1.3.0')
    self.assertEqual(component['scope'], 'required')
    self.assertEqual(component['purl'], 'pkg:github/madler/zlib@1.3.0')

  def test_component_type_override(self):
    dep = {
      'name': 'v8',
      'description': 'JavaScript engine',
      'purl_base': 'pkg:github/v8/v8',
      'component_type': 'framework',
    }
    self.assertEqual(
      sbom.build_component(dep, '13.0.0', 'required')['type'], 'framework')


class TestBuildSbom(unittest.TestCase):

  def _component(self, name, version, scope='required'):
    return {
      'bom-ref': '%s@%s' % (name, version),
      'type': 'library',
      'name': name,
      'version': version,
      'description': '',
      'scope': scope,
      'purl': 'pkg:generic/%s@%s' % (name, version),
    }

  def test_structure(self):
    doc = sbom.build_sbom('27.0.0', [self._component('zlib', '1.3.0')], [])
    self.assertEqual(doc['bomFormat'], 'CycloneDX')
    self.assertEqual(doc['specVersion'], '1.5')
    self.assertTrue(doc['serialNumber'].startswith('urn:uuid:'))
    self.assertEqual(doc['metadata']['component']['name'], 'nodejs')
    self.assertEqual(doc['metadata']['component']['version'], '27.0.0')
    self.assertEqual(len(doc['components']), 1)

  def test_excluded_components_are_not_root_dependencies(self):
    components = [
      self._component('zlib', '1.3.0'),
      self._component('googletest', 'unknown', scope='excluded'),
    ]
    doc = sbom.build_sbom('27.0.0', components, [])
    root = doc['dependencies'][0]
    self.assertEqual(root['ref'], 'nodejs@27.0.0')
    self.assertEqual(root['dependsOn'], ['zlib@1.3.0'])

  def test_serial_number_is_unique(self):
    self.assertNotEqual(sbom.build_sbom('27.0.0', [], [])['serialNumber'],
                        sbom.build_sbom('27.0.0', [], [])['serialNumber'])


if __name__ == '__main__':
  unittest.main()
