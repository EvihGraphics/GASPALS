# PLAN-3: Native Pose/Action Unit Tests and Visual Parity

## Objective

Build the deterministic Blueprint-vs-Native parity harness that closes the C++ rewrite.

## Core Rule

Do not create only one all-in-one animation demo.

Each GASPALS classic pose/action must be independently runnable and independently report pass/fail.

## Phase 1: Test Harness Skeleton

Create an automation layer that can:

```text
[ ] Spawn Blueprint reference character.
[ ] Spawn Native candidate character.
[ ] Apply the same spawn transform.
[ ] Apply the same deterministic input script.
[ ] Run fixed timestep.
[ ] Capture PNG frames.
[ ] Encode MP4.
[ ] Write telemetry JSONL for both characters.
[ ] Generate reports.
```

Suggested output root:

```text
Saved/VisualParity/<case_id>/
```

## Phase 2: Atomic Test Set

Implement L1 cases first:

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

If a current asset/feature is not available, mark the case unavailable with a blocker instead of silently skipping.

## Phase 3: Reports

Each case must emit:

```text
visual_parity_report.json
pose_parity_report.json
root_motion_parity_report.json
trajectory_parity_report.json
motion_matching_decision_report.json
foot_contact_parity_report.json
visual_review.json
```

## Phase 4: Human Review

Visual parity is not fully accepted without a signed review:

```json
{
  "reviewed_by": "",
  "reviewed_at_utc": "",
  "visual_review_pass": false,
  "notes": ""
}
```

Missing reviewer/timestamp fails closed.

## Phase 5: Small Transition Tests

After atomic tests pass, implement L2:

```text
IT-TRANS-001 Idle -> Walk -> Idle
IT-TRANS-002 Idle -> Run -> Stop
IT-TRANS-003 Walk -> Run -> Walk
IT-TRANS-004 Run -> Jump -> Land -> Stop
IT-ROT-001 Run -> Pivot 90 -> Run
```

## Phase 6: Integrated Showcase

Only after L1/L2 pass:

```text
SHOWCASE-001 GASPALS Classic Locomotion Loop
SHOWCASE-002 GASPALS Locomotion + Pivot + Jump
SHOWCASE-003 GASPALS Overlay + Weapon Demo
SHOWCASE-004 GASPALS Full Chain Demo
```

## Acceptance

```text
[ ] Atomic tests are separate files/configs.
[ ] Walk and Run are separate tests.
[ ] Jump phases are separate tests.
[ ] Pivot and Turn-In-Place are separate tests.
[ ] Overlay is separate from locomotion core.
[ ] Integrated showcase does not substitute for unit tests.
[ ] Every native module maps to required tests.
```
