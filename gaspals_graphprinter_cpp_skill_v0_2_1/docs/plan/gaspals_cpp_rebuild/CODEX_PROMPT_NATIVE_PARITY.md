# Codex Prompt: Native Blueprint-vs-C++ Visual Parity Pass

Focus on the native parity test harness.

Do not use one all-in-one demo as the first correctness proof.

## Read

```text
docs/skill/gaspals-animationtech-skill-v2/SKILL.md
docs/spec/gaspals_visual_parity_contract.md
docs/spec/gaspals_native_pose_unit_tests.md
docs/spec/gaspals_native_rebuild_acceptance.md
docs/plan/gaspals_cpp_rebuild/PLAN-2-BLUEPRINT-IR-TO-NATIVE.md
docs/plan/gaspals_cpp_rebuild/PLAN-3-NATIVE-POSE-UNIT-TESTS.md
```

## Implement

Create a deterministic parity harness that can:

```text
1. Spawn Blueprint reference character.
2. Spawn Native candidate character.
3. Apply same spawn transform.
4. Apply same deterministic input script.
5. Run fixed timestep.
6. Capture PNG frames.
7. Encode MP4 if available in the environment.
8. Write blueprint/native telemetry JSONL.
9. Write visual and numeric parity reports.
10. Write visual_review.json template.
```

## Required Atomic Tests

Start with separate cases:

```text
UT-LOCO-001 Idle
UT-LOCO-002 Walk Forward
UT-LOCO-004 Run Forward
UT-LOCO-005 Sprint Forward
UT-LOCO-006 Strafe Left
UT-LOCO-007 Strafe Right
UT-TRANS-003 Run To Stop
UT-ROT-001 Pivot 90 Left
UT-ROT-003 Pivot 180
UT-AIR-001 Jump Start
UT-AIR-003 Land Small Height
UT-OVR-002 Overlay Rifle
```

If a feature is unavailable, create a clear blocker report.

## Avoid

```text
- Do not create only SHOWCASE-001 and call it done.
- Do not combine Walk and Run into one initial test.
- Do not mark native parity accepted without visual media and telemetry.
- Do not mutate Pose Search databases.
- Do not change ALS overlay behavior unless explicitly scoped.
```

## Acceptance

```text
[ ] Each atomic case has its own contract.
[ ] Each atomic case has its own input script.
[ ] Each atomic case has its own output directory.
[ ] Each atomic case has its own pass/fail report.
[ ] Integrated showcase is absent or marked post-atomic.
[ ] Missing reviewer/timestamp fails visual review closed.
```
