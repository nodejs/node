---
name: writing-skills-rule
trigger: agents/**/*
---

# Best Practices for Writing Skills and Rules in `agents/`

This rule applies to any addition or modification to the `agents/` directory. When creating new skills or rules, adhere to the following best practices.

## 1. Rules vs. Skills: When to Use Which

*   **Use a Rule** when you need to enforce high-level constraints, workflow mandates, role restrictions, or high-level directives. Rules are usually "always on" or triggered by specific situations (e.g., debugging). They restrict behavior or mandate specific orchestration.
*   **Use a Skill** when you need to provide a specific technical guide, procedure explanation, or reference implementation for a focused task. Skills provide step-by-step instructions or reference material for a specific domain (e.g., porting a class, using a tool).

## 2. Structure of a Skill

Every skill must follow this directory structure:
```
skill-name/
├── SKILL.md              # Required: Metadata + core instructions (<500 lines)
├── scripts/              # Executable code (Python/Bash) designed as tiny CLIs
├── references/           # Supplementary context (schemas, cheatsheets)
└── assets/               # Templates or static files used in output
```

*   **SKILL.md**: Acts as the "brain." Use it for navigation and high-level procedures.
*   **References**: Link directly from `SKILL.md`. Keep them one level deep only.
*   **Scripts**: Use for fragile/repetitive operations where variation is a bug. Place long-lived library code in standard repo CLI directories.

## 3. Frontmatter Optimization

The `name` and `description` in the frontmatter of your `SKILL.md` are the only fields that the agent sees before triggering a skill.

*   **Strict Naming**: The `name` field must be 1-64 characters, contain only lowercase letters, numbers, and hyphens (no consecutive hyphens), and must exactly match the parent directory name (e.g., `name: wiz-testing` must live in `wiz-testing/SKILL.md`). Consider using gerund form (verb + -ing) for Skill names (e.g., `processing-pdfs`).
*   **Trigger-Optimized Descriptions**: (Max 1,024 characters). Describe the capability in the third person and include specific triggers and "negative triggers". Ensure descriptions do not contain XML tags.
    *   *Bad*: "Wiz skills." (Too vague).
    *   *Good*: "Creates and builds Wiz components using external Sass. Use when the user wants to update component styles or build configurations. Don't use it for external Sass styles with other frameworks."

## 4. Progressive Disclosure and Resource Management

Maintain a pristine context window by loading information only when needed.

*   **Keep SKILL.md Lean**: Limit the main file to <500 lines. Use it for navigation and primary procedures.
*   **Use Flat Subdirectories**: Move bulky context to standard folders. Keep files exactly one level deep (e.g., `references/schema.md`, not `references/db/v1/schema.md`).
*   **Keep references one level deep** from `SKILL.md` to ensure complete file reads.
*   **Just-in-Time (JiT) Loading**: Explicitly instruct the agent when to read a file (e.g., "See `references/auth-flow.md` for specific error codes").
*   **Explicit Pathing**: Always use relative paths with forward slashes (`/`), regardless of the OS. Avoid Windows-style paths.
*   **For Agents, Not Humans**: Omit `README.md`, `CHANGELOG.md`, or installation guides. Delete redundant logic.
*   **Self-Contained**: A skill should be entirely self-contained within its directory. Maintain isolation by keeping all files internal to the skill.

## 5. Writing Instructions for Agents

Create instructions for LLMs instead of humans.

*   **Tone and Style**:
    *   **Positive Framing**: Focus on what the agent *should* do. If you must specify a constraint, provide an alternative.
        *   *Bad (Negative)*: "Do not use Python 2."
        *   *Good (Positive)*: "Always use Python 3 for new scripts."
    *   **Third-Person Imperative**: Frame instructions as direct commands (e.g., "Extract the text..." rather than "I will extract...").
*   **Procedural Instructions**:
    *   **Step-by-Step Numbering**: Define workflows as strict chronological sequences.
    *   **Degrees of Freedom**: Match specificity to task fragility. Use low freedom (exact commands) for fragile operations like DB migrations, and high freedom for context-dependent tasks like code reviews.
*   **Precision and Terminology**:
    *   Use identical terminology consistently.
    *   Use the most specific terminology native to the domain (e.g., "template" instead of "html" in Angular).
*   **Templates and Examples**:
    *   Provide concrete templates in `assets/` and instruct the agent to copy them.
    *   Provide input/output pairs to guide style and detail.
*   **Intentional Modifications**: Only modify a skill when there is a concrete functional reason to do so. Avoid minor rephrasing, cosmetic cleanups, or no-op changes.

## 6. Deterministic Scripts

Do not write instructions in `SKILL.md` that require the agent to generate complex parsing logic or boilerplate code from scratch.

*   **Offload Fragile Tasks**: Create tested Python, Bash, or Node scripts in `scripts/` for operations like parsing complex datasets or querying databases. Instruct the agent to execute these scripts rather than generating code.
*   **Graceful Error Handling**: Ensure scripts return highly descriptive, human-readable error messages on stdout/stderr so the agent can self-correct based on the output.
*   **Avoid Voodoo Constants**: Document and justify configuration parameters within the scripts or references.

## 7. Skill Validation and Evaluation

When creating or modifying a skill, you must validate your draft before declaring it complete. If you have the capability to invoke subagents or start new conversations, use the prompts below to automate this validation. If not, perform a self-simulation of these steps.

*   **Evaluation-Driven Development**: Create evaluation scenarios before writing extensive documentation. Define baseline performance expectations.
*   **Validation Workflow**:
    1.  **Discovery Validation**: Invoke a subagent with the following prompt (fill in your frontmatter):
        ```text
        I am building an Agent Skill based on the spec. Agents will decide whether to load this skill based entirely on the YAML metadata below.

        [Paste your YAML frontmatter here]

        Based strictly on this description:
        1. Generate 3 realistic user prompts that you are 100% confident should trigger this skill.
        2. Generate 3 user prompts that sound similar but should NOT trigger this skill.
        3. Critique the description: Is it too broad? Suggest an optimized rewrite.
        ```

    2.  **Logic Validation**: Invoke a subagent with the following prompt (fill in your content):
        ```text
        Here is the full draft of my SKILL.md and the directory tree of its supporting files.

        [Paste your directory structure and SKILL.md here]

        Act as an autonomous agent that has just triggered this skill. Simulate your execution step-by-step based on a request to: [Insert a sample request here]

        For each step, write out your internal monologue:
        1. What exactly are you doing?
        2. Which specific file/script are you reading or running?
        3. Flag any Execution Blockers: Point out the exact line where you are forced to guess or hallucinate because my instructions are ambiguous.
        ```

    3.  **Edge Case Testing**: Invoke a subagent with the following prompt:
        ```text
        Act as a ruthless QA tester. Your goal is to break this skill.

        Ask me 3 to 5 highly specific, challenging questions about edge cases, failure states, or missing fallbacks in the SKILL.md. Focus on fragile operations, environment assumptions, and unsupported configurations.

        Do not fix these issues yet. Just ask the numbered questions and wait for my answer.
        ```


## 8. Checklist for Effective Skills

Before finalizing a skill, verify:
*   Description is specific, includes triggers, and is in third person.
*   `SKILL.md` body is under 500 lines.
*   File references are one level deep.
*   No time-sensitive information.
*   Scripts handle errors explicitly and use Unix-style paths.
*   Evaluations are created and tested.
*   The skill is entirely self-contained and does not reference other skills.
*   Modifications are intentional and functional, avoiding cosmetic or no-op changes.

