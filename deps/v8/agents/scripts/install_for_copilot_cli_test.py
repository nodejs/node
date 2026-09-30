#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile
import unittest
from unittest import mock

from install_for_copilot_cli import CopilotInstaller
from install_for_copilot_cli import COPILOT_INCOMPATIBLE_RULES
from install_for_copilot_cli import COPILOT_INCOMPATIBLE_SKILLS
from install_for_copilot_cli import GENERATED_MARKER

REPO_ROOT = Path(__file__).resolve().parents[2]


class CopilotInstallerTest(unittest.TestCase):

  def setUp(self):
    self.temp_dir = tempfile.TemporaryDirectory()
    self.repo_root = Path(self.temp_dir.name)
    self.skills_dir = self.repo_root / "agents" / "skills"
    self.rules_dir = self.repo_root / "agents" / "rules"
    self.skills_dir.mkdir(parents=True)
    self.rules_dir.mkdir()
    self.installer = CopilotInstaller(self.repo_root)

  def tearDown(self):
    self.temp_dir.cleanup()

  def _write(self, path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")

  def _add_skill(self, name: str, body: str = "Original instructions.\n"):
    skill_dir = self.skills_dir / name
    self._write(skill_dir / "SKILL.md",
                f"---\nname: {name}\ndescription: Test skill.\n---\n\n{body}")
    self._write(skill_dir / "scripts" / "helper.py", "print('helper')\n")
    return skill_dir

  def _add_rule(self,
                name: str,
                trigger: str = "",
                globs: str = "",
                description: str = "",
                body: str = "Rule body.\n") -> Path:
    frontmatter = [f"name: {name}"]
    if trigger:
      frontmatter.append(f"trigger: {trigger}")
    if globs:
      frontmatter.append(f"globs: {globs}")
    if description:
      frontmatter.append(f"description: {description}")
    rule_file = self.rules_dir / f"{name}.md"
    self._write(rule_file, "---\n{}\n---\n\n{}".format("\n".join(frontmatter),
                                                       body))
    return rule_file

  def _rule_adapter(self, name: str) -> Path:
    return self.installer.github_instructions_dir / f"{name}.instructions.md"

  # Junctions need neither elevation nor developer mode, so failures are real.
  def _create_junction(self, junction: Path, target: Path) -> None:
    subprocess.run(
        ["cmd.exe", "/d", "/c", "mklink", "/J",
         str(junction),
         str(target)],
        check=True,
        capture_output=True,
        text=True)

  def _is_junction(self, junction: Path) -> bool:
    return bool(junction.lstat().st_file_attributes
                & stat.FILE_ATTRIBUTE_REPARSE_POINT)

  def _remove_junction(self, junction: Path) -> None:
    # Tolerate a missing junction so cleanup cannot mask an assertion failure.
    try:
      is_junction = self._is_junction(junction)
    except FileNotFoundError:
      return
    if is_junction:
      os.rmdir(junction)

  def _symlink_or_skip(self,
                       link: Path,
                       target: Path,
                       is_directory: bool = True) -> None:
    try:
      link.symlink_to(target, target_is_directory=is_directory)
    except OSError as exc:
      self.skipTest(f"Symlinks are unavailable: {exc}")

  def _require_symlinks(self) -> None:
    probe = self.repo_root / "symlink-probe"
    self._symlink_or_skip(probe, self.repo_root)
    probe.unlink()

  def _without_symlink_support(self):
    return mock.patch.object(
        Path, "symlink_to", side_effect=OSError("unsupported"))

  def _file_snapshot(self, root: Path) -> dict[Path, bytes]:
    return {
        path.relative_to(root): path.read_bytes()
        for path in root.rglob("*")
        if path.is_file()
    }

  def test_wrapper_fallback_points_to_canonical_skill(self):
    self._add_skill("test-skill",
                    "Run [the helper](../../scripts/create_worktree.sh).\n")
    self._add_rule("always", trigger="always_on")
    for rule_name in COPILOT_INCOMPATIBLE_RULES:
      self._add_rule(Path(rule_name).stem, trigger="always_on")

    with self._without_symlink_support():
      self.assertEqual(0, self.installer.install(force=False))

    instructions = self.installer.copilot_instructions.read_text(
        encoding="utf-8")
    self.assertIn(GENERATED_MARKER, instructions)
    self.assertIn(".github/instructions/", instructions)
    self.assertIn("Load compatible V8 skills on demand", instructions)
    # The generated file points at canonical knowledge instead of restating
    # it, and does not rely on `@` imports being expanded, so neither an
    # import line nor duplicated build guidance may appear here.
    self.assertNotIn("@agents/rules/", instructions)
    self.assertNotIn("tools/dev/gm.py", instructions)
    for rule_name in COPILOT_INCOMPATIBLE_RULES:
      self.assertNotIn(f"agents/rules/{rule_name}", instructions)
      self.assertFalse(self._rule_adapter(Path(rule_name).stem).exists())

    installed = self.installer.github_skills_dir / "test-skill"
    wrapper = (installed / "SKILL.md").read_text(encoding="utf-8")
    self.assertIn(GENERATED_MARKER, wrapper)
    self.assertIn("agents/skills/test-skill/SKILL.md", wrapper)
    self.assertIn(
        "Treat `agents/skills/test-skill/` as the skill's base directory",
        wrapper)
    self.assertFalse((installed / "scripts" / "helper.py").exists())

  def test_symlinked_skill_resolves_to_canonical_resources(self):
    self._require_symlinks()
    self._add_skill("test-skill")

    self.assertEqual(0, self.installer.install(force=False))

    installed = self.installer.github_skills_dir / "test-skill"
    self.assertTrue(installed.is_symlink())
    self.assertEqual(
        os.path.join("..", "..", "agents", "skills", "test-skill"),
        os.readlink(installed))
    self.assertIn("Original instructions.",
                  (installed / "SKILL.md").read_text(encoding="utf-8"))
    self.assertTrue((installed / "scripts" / "helper.py").is_file())

  def test_stale_symlinked_adapter_is_removed(self):
    self._require_symlinks()
    stale_skill = self._add_skill("stale-skill")
    self.assertEqual(0, self.installer.install(force=False))
    installed = self.installer.github_skills_dir / "stale-skill"
    self.assertTrue(installed.is_symlink())

    shutil.rmtree(stale_skill)
    self.assertEqual(0, self.installer.install(force=False))

    self.assertFalse(installed.is_symlink())
    self.assertFalse(installed.exists())

  def test_force_upgrades_wrapper_to_symlink(self):
    self._require_symlinks()
    self._add_skill("test-skill")
    with self._without_symlink_support():
      self.assertEqual(0, self.installer.install(force=False))
    installed = self.installer.github_skills_dir / "test-skill"
    self.assertFalse(installed.is_symlink())

    self.assertEqual(0, self.installer.install(force=True))

    self.assertTrue(installed.is_symlink())

  def test_always_on_rule_applies_to_every_file(self):
    self._add_rule("always", trigger="always_on", body="Always body.\n")

    self.assertEqual(0, self.installer.install(force=False))

    adapter = self._rule_adapter("always")
    self.assertFalse(adapter.is_symlink())
    wrapper = adapter.read_text(encoding="utf-8")
    self.assertTrue(wrapper.startswith('---\napplyTo: "**"\n---\n'))
    self.assertIn(GENERATED_MARKER, wrapper)
    self.assertIn("Always body.", wrapper)

  def test_rule_adapters_drop_the_canonical_frontmatter(self):
    self._add_rule("always", trigger="always_on")
    self._add_rule("conditional", trigger="glob", globs="src/**/*.tq")

    self.assertEqual(0, self.installer.install(force=False))

    for name in ("always", "conditional"):
      with self.subTest(rule=name):
        wrapper = self._rule_adapter(name).read_text(encoding="utf-8")
        # Scope reaches Copilot through the generated `applyTo` header, so
        # V8's own frontmatter is dropped rather than passed through.
        self.assertNotIn("trigger:", wrapper)
        self.assertNotIn("always_on", wrapper)
        self.assertNotIn("globs:", wrapper)
        self.assertNotIn(f"name: {name}", wrapper)

  def test_glob_rule_wrapper_carries_apply_to(self):
    self._add_rule(
        "conditional",
        trigger="glob",
        globs="src/**/*.tq",
        body="Torque body.\n")

    self.assertEqual(0, self.installer.install(force=False))

    adapter = self._rule_adapter("conditional")
    self.assertFalse(adapter.is_symlink())
    wrapper = adapter.read_text(encoding="utf-8")
    self.assertTrue(wrapper.startswith('---\napplyTo: "src/**/*.tq"\n---\n'))
    self.assertIn(GENERATED_MARKER, wrapper)
    self.assertIn("Torque body.", wrapper)

  def test_glob_rule_wrapper_refreshes_without_force(self):
    rule_file = self._add_rule(
        "conditional", trigger="glob", globs="src/**/*.tq", body="First.\n")
    self.assertEqual(0, self.installer.install(force=False))
    self._write(
        rule_file, "---\nname: conditional\ntrigger: glob\n"
        "globs: src/**/*.tq\n---\n\nSecond.\n")

    self.assertEqual(0, self.installer.install(force=False))

    wrapper = self._rule_adapter("conditional").read_text(encoding="utf-8")
    self.assertIn("Second.", wrapper)
    self.assertNotIn("First.", wrapper)

  def test_glob_written_in_the_trigger_field_is_used_as_apply_to(self):
    self._add_rule("writing-skills", trigger="agents/**/*")

    self.assertEqual(0, self.installer.install(force=False))

    self.assertIn(
        'applyTo: "agents/**/*"',
        self._rule_adapter("writing-skills").read_text(encoding="utf-8"))

  def test_rules_without_a_copilot_trigger_are_listed_only(self):
    self._add_rule(
        "chooseable", trigger="model_decision", description="Use for CLs.")
    self._add_rule("missing-trigger")
    self._add_rule("unknown-trigger", trigger="unsupported")
    self._add_rule("empty-glob", trigger="glob")
    for rule_name in COPILOT_INCOMPATIBLE_RULES:
      self._add_rule(Path(rule_name).stem, trigger="always_on")

    self.assertEqual(0, self.installer.install(force=False))

    instructions = self.installer.copilot_instructions.read_text(
        encoding="utf-8")
    self.assertIn("## Optional rules", instructions)
    self.assertIn("- `agents/rules/chooseable.md`: Use for CLs.", instructions)
    for name in ("missing-trigger", "unknown-trigger", "empty-glob"):
      self.assertIn(f"- `agents/rules/{name}.md`", instructions)
    for name in ("chooseable", "missing-trigger", "unknown-trigger",
                 "empty-glob"):
      self.assertFalse(self._rule_adapter(name).exists())
    for rule_name in COPILOT_INCOMPATIBLE_RULES:
      self.assertNotIn(f"agents/rules/{rule_name}", instructions)

  def test_stale_rule_adapters_are_removed(self):
    always = self._add_rule("stale-always", trigger="always_on")
    conditional = self._add_rule(
        "stale-glob", trigger="glob", globs="src/**/*.tq")
    self.assertEqual(0, self.installer.install(force=False))
    for name in ("stale-always", "stale-glob"):
      self.assertTrue(self._rule_adapter(name).is_file())

    always.unlink()
    conditional.unlink()
    self.assertEqual(0, self.installer.install(force=False))

    for name in ("stale-always", "stale-glob"):
      self.assertFalse(self._rule_adapter(name).exists())

  def test_force_preserves_user_managed_rule_adapters(self):
    self._add_rule("always", trigger="always_on")
    self._add_rule("conditional", trigger="glob", globs="src/**/*.tq")
    for name in ("always", "conditional"):
      self._write(self._rule_adapter(name), "User rule.\n")

    self.assertEqual(0, self.installer.install(force=True))

    for name in ("always", "conditional"):
      self.assertEqual("User rule.\n",
                       self._rule_adapter(name).read_text(encoding="utf-8"))

  def test_rejects_instructions_symlink_before_mutating_anything(self):
    self._add_rule("always", trigger="always_on")
    with tempfile.TemporaryDirectory() as external_temp_dir:
      external = Path(external_temp_dir) / "external-instructions"
      self._write(external / "keep.txt", "External tree sentinel.\n")
      self.installer.github_dir.mkdir(parents=True)
      self._symlink_or_skip(self.installer.github_instructions_dir, external)
      try:
        with mock.patch("sys.stderr", new_callable=io.StringIO) as stderr:
          self.assertEqual(1, self.installer.install(force=True))

        self.assertIn("is a symlink or reparse point", stderr.getvalue())
        self.assertFalse(self.installer.copilot_instructions.exists())
        self.assertEqual(["keep.txt"], [p.name for p in external.iterdir()])
      finally:
        if self.installer.github_instructions_dir.is_symlink():
          self.installer.github_instructions_dir.unlink()

  def test_frontmatter_uses_yaml_and_preserves_original_block(self):
    markdown_file = self.repo_root / "frontmatter.md"
    frontmatter = ("---\n"
                   'name: "name: with colon"\n'
                   "description: >-\n"
                   "  Folded description.\n"
                   "---")
    self._write(markdown_file, f"{frontmatter}\n\nBody.\n")

    metadata, block = self.installer._frontmatter(markdown_file)

    self.assertEqual("name: with colon", metadata["name"])
    self.assertEqual("Folded description.", metadata["description"])
    self.assertEqual(frontmatter, block)

  def test_force_refreshes_generated_wrapper(self):
    skill_dir = self._add_skill("test-skill")
    with self._without_symlink_support():
      self.assertEqual(0, self.installer.install(force=False))
      self._write(
          skill_dir / "SKILL.md",
          "---\nname: test-skill\ndescription: Updated description.\n---\n")
      self.assertEqual(0, self.installer.install(force=True))

    installed = self.installer.github_skills_dir / "test-skill" / "SKILL.md"
    self.assertIn("description: Updated description.",
                  installed.read_text(encoding="utf-8"))

  def test_stale_generated_wrapper_is_removed(self):
    stale_skill = self._add_skill("stale-skill")
    with self._without_symlink_support():
      self.assertEqual(0, self.installer.install(force=False))
      installed = self.installer.github_skills_dir / "stale-skill"
      self._write(installed / "user-notes.md", "Keep me.\n")
      shutil.rmtree(stale_skill)
      self.assertEqual(0, self.installer.install(force=False))

    self.assertTrue(installed.is_dir())
    self.assertFalse((installed / "SKILL.md").exists())
    self.assertEqual("Keep me.\n",
                     (installed / "user-notes.md").read_text(encoding="utf-8"))

  def test_force_preserves_user_files_beside_generated_wrapper(self):
    self._add_skill("test-skill")
    with self._without_symlink_support():
      self.assertEqual(0, self.installer.install(force=False))
      installed = self.installer.github_skills_dir / "test-skill"
      self._write(installed / "user-notes.md", "Keep me.\n")
      self.assertEqual(0, self.installer.install(force=True))

    self.assertTrue((installed / "SKILL.md").is_file())
    self.assertEqual("Keep me.\n",
                     (installed / "user-notes.md").read_text(encoding="utf-8"))

  def test_incompatible_skills_are_not_installed(self):
    for skill_name in COPILOT_INCOMPATIBLE_SKILLS:
      self._add_skill(skill_name)

    self.assertEqual(0, self.installer.install(force=False))

    for skill_name in COPILOT_INCOMPATIBLE_SKILLS:
      self.assertFalse((self.installer.github_skills_dir / skill_name).exists())

  def test_incompatible_adapter_sources_exist_in_checkout(self):
    for rule_name in COPILOT_INCOMPATIBLE_RULES:
      with self.subTest(rule=rule_name):
        self.assertTrue((REPO_ROOT / "agents" / "rules" / rule_name).is_file())
    for skill_name in COPILOT_INCOMPATIBLE_SKILLS:
      with self.subTest(skill=skill_name):
        self.assertTrue((REPO_ROOT / "agents" / "skills" / skill_name /
                         "SKILL.md").is_file())

  def test_file_system_errors_propagate(self):
    generated_file = self.installer.copilot_instructions
    self._write(generated_file, GENERATED_MARKER)
    with mock.patch.object(
        Path, "read_text", side_effect=OSError("unreadable")):
      with self.assertRaisesRegex(OSError, "unreadable"):
        self.installer._write_generated_file(generated_file, "new contents")

  def test_force_preserves_user_managed_files(self):
    self._add_skill("test-skill")
    self._write(self.installer.copilot_instructions, "User instructions.\n")
    installed = self.installer.github_skills_dir / "test-skill"
    self._write(installed / "SKILL.md", "User skill.\n")

    self.assertEqual(0, self.installer.install(force=True))

    self.assertEqual(
        "User instructions.\n",
        self.installer.copilot_instructions.read_text(encoding="utf-8"))
    self.assertEqual("User skill.\n",
                     (installed / "SKILL.md").read_text(encoding="utf-8"))

  @unittest.skipUnless(os.name == "nt", "Directory junctions require Windows")
  def test_rejects_github_junction_before_mutation(self):
    self._add_skill("test-skill")
    with tempfile.TemporaryDirectory() as external_temp_dir:
      external_github = Path(external_temp_dir) / "external-github"
      self._write(external_github / "copilot-instructions.md",
                  f"{GENERATED_MARKER}\n\nExternal instructions sentinel.\n")
      self._write(external_github / "skills" / "stale-skill" / "SKILL.md",
                  f"{GENERATED_MARKER}\n\nExternal skill sentinel.\n")
      self._write(external_github / "keep.txt", "External tree sentinel.\n")
      original_files = self._file_snapshot(external_github)

      self._create_junction(self.installer.github_dir, external_github)
      original_target = os.readlink(self.installer.github_dir)
      try:
        with mock.patch("sys.stderr", new_callable=io.StringIO) as stderr:
          self.assertEqual(1, self.installer.install(force=True))

        self.assertIn(
            f"Unsafe adapter destination: {self.installer.github_dir}",
            stderr.getvalue())
        self.assertEqual(original_target,
                         os.readlink(self.installer.github_dir))
        self.assertEqual(original_files, self._file_snapshot(external_github))
        self.assertFalse((external_github / "skills" / "test-skill").exists())
      finally:
        self._remove_junction(self.installer.github_dir)

  @unittest.skipUnless(os.name == "nt", "Directory junctions require Windows")
  def test_rejects_skills_junction_before_mutation(self):
    self._add_skill("test-skill")
    self._write(self.installer.copilot_instructions,
                "User instructions sentinel.\n")
    original_instructions = self.installer.copilot_instructions.read_bytes()
    with tempfile.TemporaryDirectory() as external_temp_dir:
      external_skills = Path(external_temp_dir) / "external-skills"
      self._write(external_skills / "test-skill" / "SKILL.md",
                  f"{GENERATED_MARKER}\n\nExternal skill sentinel.\n")
      self._write(external_skills / "keep.txt", "External tree sentinel.\n")
      original_files = self._file_snapshot(external_skills)

      self._create_junction(self.installer.github_skills_dir, external_skills)
      original_target = os.readlink(self.installer.github_skills_dir)
      try:
        with mock.patch("sys.stderr", new_callable=io.StringIO) as stderr:
          self.assertEqual(1, self.installer.install(force=True))

        self.assertIn(
            f"Unsafe adapter destination: {self.installer.github_skills_dir}",
            stderr.getvalue())
        self.assertEqual(original_instructions,
                         self.installer.copilot_instructions.read_bytes())
        self.assertEqual(original_target,
                         os.readlink(self.installer.github_skills_dir))
        self.assertEqual(original_files, self._file_snapshot(external_skills))
      finally:
        self._remove_junction(self.installer.github_skills_dir)

  def test_rejects_github_symlink_before_mutation(self):
    self._add_skill("test-skill")
    with tempfile.TemporaryDirectory() as external_temp_dir:
      external_github = Path(external_temp_dir) / "external-github"
      self._write(external_github / "keep.txt", "External tree sentinel.\n")
      self._symlink_or_skip(self.installer.github_dir, external_github)
      try:
        with mock.patch("sys.stderr", new_callable=io.StringIO) as stderr:
          self.assertEqual(1, self.installer.install(force=True))

        self.assertIn("is a symlink or reparse point", stderr.getvalue())
        self.assertTrue(self.installer.github_dir.is_symlink())
        self.assertEqual("External tree sentinel.\n",
                         (external_github /
                          "keep.txt").read_text(encoding="utf-8"))
        self.assertFalse((external_github / "copilot-instructions.md").exists())
        self.assertFalse((external_github / "skills").exists())
      finally:
        if self.installer.github_dir.is_symlink():
          self.installer.github_dir.unlink()

  def test_rejects_skills_symlink_before_mutation(self):
    self._add_skill("test-skill")
    self._write(self.installer.copilot_instructions,
                "User instructions sentinel.\n")
    original_instructions = self.installer.copilot_instructions.read_bytes()
    with tempfile.TemporaryDirectory() as external_temp_dir:
      external_skills = Path(external_temp_dir) / "external-skills"
      self._write(external_skills / "keep.txt", "External tree sentinel.\n")
      self._symlink_or_skip(self.installer.github_skills_dir, external_skills)
      try:
        with mock.patch("sys.stderr", new_callable=io.StringIO) as stderr:
          self.assertEqual(1, self.installer.install(force=True))

        self.assertIn("is a symlink or reparse point", stderr.getvalue())
        self.assertEqual(original_instructions,
                         self.installer.copilot_instructions.read_bytes())
        self.assertTrue(self.installer.github_skills_dir.is_symlink())
        self.assertEqual("External tree sentinel.\n",
                         (external_skills /
                          "keep.txt").read_text(encoding="utf-8"))
        self.assertFalse((external_skills / "test-skill").exists())
      finally:
        if self.installer.github_skills_dir.is_symlink():
          self.installer.github_skills_dir.unlink()

  def test_force_does_not_follow_user_managed_symlink(self):
    skill_dir = self._add_skill("test-skill")
    external_skill = self.repo_root / "external-skill"
    self._write(external_skill / "SKILL.md",
                f"{GENERATED_MARKER}\n\nUser-managed target.\n")
    installed = self.installer.github_skills_dir / "test-skill"
    installed.parent.mkdir(parents=True)
    self._symlink_or_skip(installed, external_skill)

    self.assertEqual(0, self.installer.install(force=True))

    self.assertTrue(installed.is_symlink())
    self.assertEqual(f"{GENERATED_MARKER}\n\nUser-managed target.\n",
                     (external_skill / "SKILL.md").read_text(encoding="utf-8"))
    self.assertNotEqual(
        installed.resolve(strict=False), skill_dir.resolve(strict=False))

  @unittest.skipUnless(os.name == "nt", "Directory junctions require Windows")
  def test_force_does_not_follow_user_managed_junction(self):
    self._add_skill("test-skill")
    external_skill = self.repo_root / "junction-target"
    target_contents = (
        f"{GENERATED_MARKER}\r\n\r\nUser-managed junction target.\r\n")
    self._write(external_skill / "SKILL.md", target_contents)
    installed = self.installer.github_skills_dir / "test-skill"
    installed.parent.mkdir(parents=True)
    self._create_junction(installed, external_skill)

    original_contents = (external_skill / "SKILL.md").read_bytes()
    try:
      self.assertEqual(0, self.installer.install(force=True))

      self.assertEqual(original_contents,
                       (external_skill / "SKILL.md").read_bytes())
      self.assertTrue(self._is_junction(installed))
    finally:
      self._remove_junction(installed)

  @unittest.skipUnless(os.name == "nt", "Directory junctions require Windows")
  def test_force_preserves_dangling_user_managed_junction(self):
    self._add_skill("test-skill")
    installed = self.installer.github_skills_dir / "test-skill"
    installed.parent.mkdir(parents=True)

    with tempfile.TemporaryDirectory() as external_temp_dir:
      external_skill = Path(external_temp_dir) / "junction-target"
      moved_skill = Path(external_temp_dir) / "moved-target"
      target_contents = (
          f"{GENERATED_MARKER}\r\n\r\nUser-managed junction target.\r\n")
      self._write(external_skill / "SKILL.md", target_contents)
      self._create_junction(installed, external_skill)

      original_target = os.readlink(installed)
      original_contents = (external_skill / "SKILL.md").read_bytes()
      try:
        external_skill.rename(moved_skill)
        self.assertFalse(external_skill.exists())
        self.assertFalse(installed.exists())

        self.assertEqual(0, self.installer.install(force=True))

        self.assertEqual(original_target, os.readlink(installed))
        self.assertEqual(original_contents,
                         (moved_skill / "SKILL.md").read_bytes())
        self.assertTrue(self._is_junction(installed))
      finally:
        self._remove_junction(installed)

  def test_non_utf8_user_files_are_skipped(self):
    self._add_skill("test-skill")
    installed = self.installer.github_skills_dir / "test-skill"
    installed.mkdir(parents=True)
    self.installer.copilot_instructions.write_bytes(b"\xe9 user notes\n")
    (installed / "SKILL.md").write_bytes(b"\xe9 user skill\n")

    with self._without_symlink_support():
      self.assertEqual(0, self.installer.install(force=True))

    self.assertEqual(b"\xe9 user notes\n",
                     self.installer.copilot_instructions.read_bytes())
    self.assertEqual(b"\xe9 user skill\n",
                     (installed / "SKILL.md").read_bytes())

  def test_generated_file_symlink_is_not_followed(self):
    external = self.repo_root / "shared-instructions.md"
    self._write(external, f"{GENERATED_MARKER}\n\nShared checkout content.\n")
    self.installer.github_dir.mkdir(parents=True)
    self._symlink_or_skip(
        self.installer.copilot_instructions, external, is_directory=False)
    original = external.read_bytes()

    self.assertEqual(0, self.installer.install(force=True))

    self.assertEqual(original, external.read_bytes())
    self.assertTrue(self.installer.copilot_instructions.is_symlink())

  def test_without_force_existing_adapters_are_left_alone(self):
    skill_dir = self._add_skill("test-skill")
    with self._without_symlink_support():
      self.assertEqual(0, self.installer.install(force=False))
      installed = self.installer.github_skills_dir / "test-skill" / "SKILL.md"
      original = installed.read_bytes()

      self._write(skill_dir / "SKILL.md",
                  "---\nname: test-skill\ndescription: Updated.\n---\n")
      self.assertEqual(0, self.installer.install(force=False))

    self.assertEqual(original, installed.read_bytes())

  def test_stale_removal_preserves_user_managed_adapters(self):
    self._add_skill("test-skill")
    user_rule = self.installer.github_instructions_dir / "mine.instructions.md"
    user_skill = self.installer.github_skills_dir / "mine" / "SKILL.md"
    self._write(user_rule, "User rule.\n")
    self._write(user_skill, "User skill.\n")

    self.assertEqual(0, self.installer.install(force=False))

    self.assertEqual("User rule.\n", user_rule.read_text(encoding="utf-8"))
    self.assertEqual("User skill.\n", user_skill.read_text(encoding="utf-8"))

  def test_missing_skills_directory_fails(self):
    missing_root = self.repo_root / "missing"
    self.assertEqual(1, CopilotInstaller(missing_root).install(force=False))


if __name__ == "__main__":
  unittest.main()
