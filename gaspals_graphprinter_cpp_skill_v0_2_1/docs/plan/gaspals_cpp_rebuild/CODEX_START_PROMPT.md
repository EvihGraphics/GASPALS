# Codex Start Prompt: GASPALS GraphPrinter -> Blueprint IR -> Native C++ Parity

You are working in the GASPALS repository.

Read these files first:

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
```

Also inspect current repository files:

```text
Docs/GraphPrinter_Plugin_Architecture.md
Docs/GraphPrinter_QA_2026-04-29.md
Docs/GASPALS_MotionMatching_Architecture.md
Docs/RuntimeInsertionPoints.md
Tools/ue_open_assets.py
Tools/setup_and_launch.ps1
Tools/ws_controller.py
Plugins/GraphPrinter/
Graph/Blueprints/
```

## Mission

Upgrade the current GraphPrinter pipeline into a reconstruction-grade Blueprint evidence system. The active implementation scope for this start prompt is PLAN-1 unless the user explicitly switches to PLAN-2 or PLAN-3.

The final long-term goal is not only C++ code. The final goal is Blueprint-vs-Native visual parity for GASPALS classic actions, but do not jump to native runtime work before the GraphPrinter / Blueprint IR evidence chain is complete.

## First Development Pass

This is the current active pass for the default prompt. Implement the following in order:

```text
1. Make GraphPrinter batch capture environment-driven while preserving current defaults.
2. Generate _graph_module_catalog.json and _graph_module_catalog.tsv.
3. Extract GraphEditor text chunk sidecars or structured decode-status sidecars.
4. Emit partial Blueprint IR JSON for OK modules.
5. Emit stitch metadata for split graph modules.
6. Generate Docs/GraphPrinter_FailureDiagnostics.md.
7. Add Tools/generate_cpp_rebuild_tasks.py.
8. Generate Docs/CppRebuildTasks/*.md from the catalog/IR/stitch data.
```

## Deferred Second Development Pass

Do not implement this pass unless the user explicitly activates PLAN-2. When activated, prepare native reconstruction scaffolding without takeover:

```text
1. Add mapping docs for Blueprint variables/functions to C++ fields/methods.
2. Add an initial GASPALSNative C++ skeleton only when PLAN-2 is explicitly active. Do NOT extend or depend on `Plugins/GASPALSShadow/` — it is temporarily deprecated and must not be used as the PLAN-2 scaffold until explicitly reactivated.
3. Implement observe-only and mirror-only boundaries.
4. Do not replace AnimBP main graph.
5. Do not replace Pose Search databases.
6. Do not override ALS overlay selection.
```

## Deferred Third Development Pass

Do not implement this pass unless the user explicitly activates PLAN-3. When activated, prepare the visual parity harness:

```text
1. Implement deterministic case contracts under Saved/VisualParity/<case_id>/.
2. Spawn Blueprint reference and Native candidate from the same start.
3. Use the same input script, camera, fixed timestep, and asset scope.
4. Output PNG, MP4, telemetry JSONL, and reports.
5. Implement atomic test cases first: Walk, Run, Sprint, Strafe, Stop, Pivot, Jump, Overlay.
6. Do not use one all-in-one demo as the first correctness gate.
```

## Non-Negotiables

```text
- Do not claim C++ parity from compile success.
- Do not claim C++ parity from generated docs.
- Do not claim C++ parity from one mixed showcase.
- Walk and Run must be separate tests.
- Jump phases must be separate tests.
- Pivot must be separate from ordinary run.
- Overlay must be separate from locomotion core.
- Integrated showcases are allowed only after atomic tests pass.
- GASPALS runtime behavior must remain unchanged unless explicitly implementing a replace-with-fallback phase.
```

## Expected Pull Request Summary

When finished, report:

```text
- What capture outputs were added.
- How many modules are cataloged.
- How many modules have text chunks.
- How many modules have Blueprint IR.
- How many stitch groups exist.
- What C++ task docs were generated.
- What native skeleton or parity harness files were added.
- Which atomic tests exist.
- Which native parity gates remain blocked.
```
