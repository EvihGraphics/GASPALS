# GASPALS Native Pose/Action Unit Tests

## Purpose

C++ reconstruction must be tested action-by-action.

Do not begin with one combined demo that mixes all locomotion, jump, pivot, overlay, and traversal behavior.

## Test Layers

```text
L1 Atomic pose/action unit tests
L2 Small transition tests
L3 Long integrated showcase/regression tests
```

L1 must pass before L2. L2 must pass before L3 is used as evidence.

## L1 Atomic Tests

### Locomotion Basic

```text
UT-LOCO-001 Idle
UT-LOCO-002 Walk Forward
UT-LOCO-003 Walk Backward
UT-LOCO-004 Run Forward
UT-LOCO-005 Sprint Forward
UT-LOCO-006 Strafe Left
UT-LOCO-007 Strafe Right
```

### Start / Stop

```text
UT-TRANS-001 Idle To Walk
UT-TRANS-002 Idle To Run
UT-TRANS-003 Run To Stop
UT-TRANS-004 Sprint To Stop
```

### Rotation / Pivot

```text
UT-ROT-001 Pivot 90 Left
UT-ROT-002 Pivot 90 Right
UT-ROT-003 Pivot 180
UT-ROT-004 Turn In Place Left
UT-ROT-005 Turn In Place Right
```

### Air / Jump

```text
UT-AIR-001 Jump Start
UT-AIR-002 Falling Loop
UT-AIR-003 Land Small Height
UT-AIR-004 Land High Fall
```

### Overlay / Weapon

```text
UT-OVR-001 Overlay Unarmed
UT-OVR-002 Overlay Rifle
UT-OVR-003 Overlay Pistol
UT-OVR-004 Weapon Attach
UT-OVR-005 Weapon Detach
```

### Traversal

```text
UT-TRAV-001 Mantle Low
UT-TRAV-002 Mantle High
UT-TRAV-003 Vault
```

Traversal should be later than locomotion core unless the current task specifically targets traversal.

## L2 Small Transition Tests

```text
IT-TRANS-001 Idle -> Walk -> Idle
IT-TRANS-002 Idle -> Run -> Stop
IT-TRANS-003 Walk -> Run -> Walk
IT-TRANS-004 Run -> Jump -> Land -> Stop
IT-ROT-001 Run -> Pivot 90 -> Run
IT-ROT-002 Run -> Pivot 180 -> Run
IT-OVR-001 Idle Unarmed -> Rifle -> Idle Rifle
```

## L3 Integrated Showcase

```text
SHOWCASE-001 GASPALS Classic Locomotion Loop
SHOWCASE-002 GASPALS Locomotion + Pivot + Jump
SHOWCASE-003 GASPALS Overlay + Weapon Demo
SHOWCASE-004 GASPALS Full Chain Demo
```

These are not substitutes for L1/L2 tests.

## Per-Test Required Fields

```json
{
  "test_id": "UT-LOCO-004",
  "name": "Run Forward",
  "level": "L1",
  "category": "Locomotion Basic",
  "frames": 300,
  "fixed_delta_seconds": 0.0166667,
  "spawn_transform": {},
  "input_script": "run_forward.input.jsonl",
  "expected_state": {
    "gait": "Run",
    "is_in_air": false,
    "overlay": "Default"
  },
  "required_reports": [
    "visual_parity_report.json",
    "pose_parity_report.json",
    "root_motion_parity_report.json",
    "trajectory_parity_report.json",
    "foot_contact_parity_report.json"
  ]
}
```

## Required Rule For Codex

When implementing tests:

```text
Create many small deterministic scenario cases.
Do not create only one all-in-one animation demo.
Each GASPALS classic pose/action must be independently runnable and independently report pass/fail.
```

## Completion Semantics

A C++ module can be marked complete only against its required test list.

Example:

```text
Update_MovementDirection native mirror:
  required:
    UT-LOCO-002
    UT-LOCO-004
    UT-LOCO-006
    UT-LOCO-007
    UT-ROT-001
  not required yet:
    UT-AIR-001
    UT-OVR-002
```
