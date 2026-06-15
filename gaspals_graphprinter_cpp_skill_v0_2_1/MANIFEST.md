# GASPALS GraphPrinter -> Blueprint IR -> Native C++ Skill Pack v0.2

This package upgrades the earlier GraphPrinter evidence-chain plan into a full native rewrite closure plan.

## Core idea

GASPALS C++ rewrite is not complete when code compiles.

It is complete only when the native candidate character and the original Blueprint reference character can run the same deterministic GASPALS action cases from the same starting state, produce visual media and telemetry, and pass action-specific parity gates.

## Contents

```text
docs/skill/gaspals-animationtech-skill-v2/SKILL.md
docs/skill/gaspals-graphprinter-blueprint-ir-cpp-skill/SKILL.md
docs/skill/gaspals-graphprinter-blueprint-ir-cpp-skill/references/current_repo_state.md

docs/spec/graphprinter_blueprint_ir_schema.md
docs/spec/graph_chunk_stitching_contract.md
docs/spec/blueprint_to_cpp_mapping_contract.md
docs/spec/gaspals_visual_parity_contract.md
docs/spec/gaspals_native_pose_unit_tests.md
docs/spec/gaspals_native_rebuild_acceptance.md

docs/plan/gaspals_cpp_rebuild/PLAN-0-CURRENT-CHECKPOINT.md
docs/plan/gaspals_cpp_rebuild/PLAN-1-GRAPHPRINTER-TO-BLUEPRINT-IR.md
docs/plan/gaspals_cpp_rebuild/PLAN-2-BLUEPRINT-IR-TO-NATIVE.md
docs/plan/gaspals_cpp_rebuild/PLAN-3-NATIVE-POSE-UNIT-TESTS.md

docs/plan/gaspals_cpp_rebuild/CODEX_START_PROMPT.md
docs/plan/gaspals_cpp_rebuild/CODEX_PROMPT_GRAPHPRINTER_IR.md
docs/plan/gaspals_cpp_rebuild/CODEX_PROMPT_NATIVE_PARITY.md
```

## Recommended Codex entry

Use:

```text
docs/plan/gaspals_cpp_rebuild/CODEX_START_PROMPT.md
```

For a narrower first pass, use:

```text
docs/plan/gaspals_cpp_rebuild/CODEX_PROMPT_GRAPHPRINTER_IR.md
```

For the native parity harness pass, use:

```text
docs/plan/gaspals_cpp_rebuild/CODEX_PROMPT_NATIVE_PARITY.md
```

## Non-negotiable closure rule

A native C++ rewrite is not accepted by compile success, generated C++ stubs, or a single all-in-one demo.

It requires atomic pose/action parity cases:
- Walk is its own test.
- Run is its own test.
- Sprint is its own test.
- Strafe is its own test.
- Jump phases are separate tests.
- Pivot and Turn-In-Place are separate tests.
- Overlay/weapon changes are separate tests.
- Traversal is separate and later.

Integrated long demos are allowed only after the atomic cases pass.


## v0.2.1 Consistency Fixes

- Default start prompt now activates PLAN-1 only; PLAN-2 and PLAN-3 are explicitly deferred unless requested.
- GASPALSShadow is treated as optional/observe-only and must not be assumed to exist from the disabled .uproject entry.
- Native parity prompt now reads PLAN-2 before PLAN-3.
