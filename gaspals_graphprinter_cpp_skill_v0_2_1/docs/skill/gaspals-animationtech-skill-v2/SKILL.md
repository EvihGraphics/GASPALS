---
name: gaspals-animationtech
description: Use this skill when working on EvihGraphics/GASPALS as the primary Unreal Engine 5.7 animation host, especially GraphPrinter blueprint capture, Blueprint-to-C++ reconstruction, Motion Matching / Pose Search analysis, ALS overlay layering, Shadow Mode native observers, and Blueprint-vs-Native visual parity tests for GASPALS classic locomotion actions.
---

# GASPALS AnimationTech Skill v2

## Mission

Use GASPALS as the primary UE runtime host for learning, documenting, and progressively rewriting the animation stack into a native C++ implementation.

The long-term pipeline is:

```text
GASPALS Blueprint runtime
  -> GraphPrinter semantic module screenshots
  -> GraphEditor text chunks
  -> Blueprint IR and stitched graph/module catalog
  -> C++ reconstruction task graph
  -> native mirror implementation
  -> atomic pose/action parity tests
  -> Blueprint-vs-Native visual comparison package
  -> staged takeover with fallback
```

The C++ rewrite is not complete when it compiles. It is complete only when the native candidate character can reproduce the relevant GASPALS action cases against the Blueprint reference character from the same starting state.

## Source Truth

```text
Primary repo: EvihGraphics/GASPALS
Historical user-facing repo name: logic-three-body/GASPALS
Upstream reference: PolygonHive/GASPALS
Engine association: UE 5.7
Primary runtime: GASPALS
Primary tooling under development: Plugins/GraphPrinter + Tools/ue_open_assets.py
Future native shadow module: GASPALSShadow / GASPALSNative, initially observe-only
```

Reference repositories are not the runtime host:

```text
References/ControlOperators
References/Motion-Matching
References/Learned-Motion-Matching
References/Unreal-3rd-Person-Parkour
References/Learned_Motion_Matching_Training
```

## Non-Negotiables

1. **GASPALS remains the formal runtime host.**
   - Do not move the first implementation target into a reference repository.
   - Do not replace Motion Matching, ALS overlay layering, or traversal logic in early phases.

2. **GraphPrinter evidence is not C++ parity.**
   - Screenshots, text chunks, and Blueprint IR only prove capture and reconstruction evidence.
   - Native parity requires Blueprint-vs-Native deterministic scenario replay.

3. **Never accept native C++ rewrite without parity packages.**
   - A C++ rewrite is incomplete without visual media, telemetry, reports, and review artifacts for the target GASPALS scenario set.

4. **Atomic pose/action tests first.**
   - Do not validate Walk, Run, Jump, Pivot, Overlay, and Traversal in one all-in-one demo first.
   - Each action family must have independent tests and independent pass/fail reasons.
   - Long integrated demos are allowed only after the atomic tests pass.

5. **Phase 0-2 are observe-only / evidence-only.**
   - No AnimBP main graph replacement.
   - No Pose Search database replacement.
   - No overlay selection override.
   - No traversal chooser override.
   - No formal locomotion output takeover.

6. **Keep MLDeformer downstream risk visible.**
   - GASPALS or a future native/neural stack is the pose producer.
   - MLDeformer is the downstream pose consumer.
   - Any change to pose distribution can become an out-of-distribution deformation risk.

## Runtime Layer Taxonomy

Every captured Blueprint module and every generated C++ task must be classified into one of:

```text
input
character_movement
anim_update
trajectory
motion_matching_pose_search
blend_stack
als_overlay_linked_layer
ik_postprocess_final_pose
debug_editor_tooling
unknown
```

## Migration Modes

Each C++ reconstruction task must specify exactly one migration mode:

```text
observe-only
mirror-only
replace-with-fallback
formal-takeover
```

Early GASPALS work should normally stay in `observe-only` or `mirror-only`.

## Blueprint-to-C++ Native Boundary

Prefer ALS-Refactored-style C++ boundaries:

```text
FGaspalsLocomotionInput
FGaspalsMovementState
FGaspalsTrajectorySample
FGaspalsTrajectoryState
FGaspalsOverlayState
FGaspalsMotionMatchingDecision
FGaspalsNativeAnimTelemetryFrame

UGaspalsShadowObserverComponent
UGaspalsNativeCharacterComponent
UGaspalsNativeAnimInstanceBridge
UGaspalsVisualParitySubsystem

UGaspalsTrajectoryService
UGaspalsMotionStateService
UGaspalsOverlayRoutingService
UGaspalsMotionMatchingMirrorService
```

## Native Rewrite Closure

Native rewrite closure has four gates:

```text
G0 compile/load
G1 observe-only shadow telemetry parity
G2 native mirror numeric parity
G3 Blueprint-vs-Native visual parity
```

G3 requires:

```text
same map
same skeletal mesh
same animation assets
same Pose Search / Chooser data when still shared
same overlay assets
same spawn transform
same input script
same camera
same fixed timestep
same frame count
same output media contract
```

## Atomic Pose/Action Test Rule

The native parity test harness must implement separate deterministic tests such as:

```text
UT-LOCO-001 Idle
UT-LOCO-002 Walk Forward
UT-LOCO-004 Run Forward
UT-LOCO-005 Sprint Forward
UT-LOCO-006 Strafe Left
UT-TRANS-003 Run To Stop
UT-ROT-001 90 Degree Pivot Left
UT-ROT-003 180 Degree Pivot
UT-AIR-001 Jump Start
UT-AIR-003 Land
UT-OVR-002 Rifle Overlay
UT-TRAV-001 Mantle Low
```

Do not begin with only one combined animation showcase. The combined showcase is an integration test, not the first correctness proof.

## Expected Final Artifact Shape

For each accepted native parity case:

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
  telemetry_blueprint.jsonl
  telemetry_native.jsonl
  visual_parity_report.json
  pose_parity_report.json
  root_motion_parity_report.json
  trajectory_parity_report.json
  foot_contact_parity_report.json
  visual_review.json
```

## Response Style

When helping with this skill:

- State whether the current task is capture, IR extraction, C++ task generation, native mirror, or visual parity.
- State which GASPALS runtime layer is affected.
- State whether the work is observe-only, mirror-only, replace-with-fallback, or formal takeover.
- For C++ rewrite claims, state which atomic tests pass and which remain untested.
- Never imply full native parity from a single integrated demo.
