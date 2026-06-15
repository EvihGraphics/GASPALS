# GASPALS Blueprint-vs-Native Visual Parity Contract

## Purpose

This is the final closure contract for C++ reconstruction.

The native candidate must be compared against the original Blueprint reference character from the same start state, with the same input script, same assets, same camera, same fixed timestep, and independent per-action test cases.

## Case Contract

```json
{
  "schema_version": "gaspals.visual_parity_contract.v1",
  "case_id": "UT-LOCO-004",
  "case_name": "Run Forward",
  "category": "Locomotion Basic",
  "map": "/Game/GASPALS/Maps/...",
  "blueprint_character_class": "/Game/GASPALS/.../CBP_SandboxCharacter",
  "native_character_class": "/Script/GASPALSNative.GASPALSNativeCharacter",
  "skeletal_mesh": "",
  "anim_asset_scope": "shared",
  "pose_search_scope": "shared|native_mirror|native_formal",
  "overlay_scope": "shared|native_mirror|native_formal",
  "spawn_transform": {
    "location": [0, 0, 100],
    "rotation": [0, 0, 0],
    "scale": [1, 1, 1]
  },
  "camera_transform": {
    "location": [-500, 0, 180],
    "rotation": [0, -12, 0],
    "fov": 45
  },
  "fixed_delta_seconds": 0.0166667,
  "frames": 300,
  "input_script": "input_script.jsonl",
  "expected_state_windows": [],
  "outputs": {
    "root": "Saved/VisualParity/UT-LOCO-004"
  }
}
```

## Required Output Package

```text
Saved/VisualParity/<case_id>/
  contract.json
  input_script.jsonl

  blueprint/frames/*.png
  native/frames/*.png
  comparison/side_by_side/*.png
  comparison/overlay/*.png

  blueprint.mp4
  native.mp4
  side_by_side.mp4
  overlay_diff.mp4

  telemetry_blueprint.jsonl
  telemetry_native.jsonl

  visual_parity_report.json
  pose_parity_report.json
  root_motion_parity_report.json
  trajectory_parity_report.json
  motion_matching_decision_report.json
  foot_contact_parity_report.json
  visual_review.json
```

## Input Script Format

```json
{"frame": 0, "move": [0, 0], "look": [0, 0], "jump": false, "sprint": false, "overlay": "Default"}
{"frame": 30, "move": [1, 0], "look": [0, 0], "jump": false, "sprint": true, "overlay": "Default"}
{"frame": 240, "move": [0, 0], "look": [0, 0], "jump": false, "sprint": false, "overlay": "Default"}
```

## Telemetry Frame

```json
{
  "frame": 0,
  "time": 0.0,
  "character_transform": {},
  "root_bone_transform": {},
  "velocity": [0, 0, 0],
  "acceleration": [0, 0, 0],
  "movement_direction": "",
  "target_rotation": [0, 0, 0],
  "gait": "",
  "stance": "",
  "rotation_mode": "",
  "is_in_air": false,
  "trajectory_samples": [],
  "pose_search_database": "",
  "selected_animation": "",
  "selected_pose_index": null,
  "blend_stack_asset": "",
  "overlay_state": "",
  "foot_contacts": {
    "left": false,
    "right": false
  },
  "key_bones": {}
}
```

## Comparison Reports

The test harness should produce report fields:

```text
visual_parity_pass
pose_parity_pass
root_motion_parity_pass
trajectory_parity_pass
motion_matching_decision_pass
foot_contact_parity_pass
case_acceptance_pass
blockers
```

## Suggested Initial Tolerances

```text
Root location drift:
  locomotion core <= 3 cm over 300 frames

Root yaw drift:
  average <= 2 degrees
  max <= 5 degrees except pivot/turn cases

Foot contact mismatch:
  run/walk loop <= 2 frames
  start/stop <= 3 frames

Key bone transform:
  report-only in early mirror phases
  strict only after formal takeover scope is declared
```

## Human Review

`visual_review.json` must be explicit:

```json
{
  "reviewed_by": "",
  "reviewed_at_utc": "",
  "case_id": "",
  "visual_review_pass": false,
  "checklist": {
    "start_sync_ok": false,
    "stop_sync_ok": false,
    "root_motion_ok": false,
    "foot_sliding_ok": false,
    "orientation_ok": false,
    "pose_pop_ok": false,
    "overlay_ok": true,
    "camera_media_ok": false
  },
  "notes": ""
}
```

A missing reviewer or timestamp fails closed.
