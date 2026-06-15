# GASPALS Native C++ Rebuild Acceptance

## Purpose

This document defines when C++ reconstruction can be considered accepted.

## Gate G0: Compile / Load

```text
[ ] GASPALSNative plugin compiles. If a real Plugins/GASPALSShadow directory exists in the checkout, it also compiles; do not require or assume it when absent.
[ ] Native component/class loads in editor.
[ ] No startup crash.
[ ] No missing asset hard failure.
[ ] No formal animation takeover is enabled by default.
```

## Gate G1: Observe-Only Shadow Telemetry

```text
[ ] Native observer attaches to Blueprint reference character.
[ ] Captures input intent, movement state, trajectory, overlay, and key animation variables.
[ ] Writes telemetry JSONL.
[ ] Does not change formal animation output.
```

## Gate G2: Native Mirror Numeric Parity

```text
[ ] Native code computes the target logic independently.
[ ] Blueprint remains formal animation source.
[ ] Native values are compared against Blueprint values.
[ ] Required atomic tests pass within tolerance.
```

## Gate G3: Blueprint-vs-Native Visual Parity

```text
[ ] Blueprint reference character and native candidate character run from the same start.
[ ] Both consume the same deterministic input script.
[ ] Both use the same map, camera, fixed timestep, skeletal mesh, and asset scope.
[ ] PNG sequences are produced.
[ ] MP4s are produced.
[ ] Side-by-side video is produced.
[ ] Telemetry reports are produced.
[ ] Human visual_review.json is signed for accepted cases.
```

## Never Accept

Do not accept C++ reconstruction if the only evidence is:

```text
compiled successfully
generated C++ stubs
generated docs
one hand-recorded video
one all-in-one demo
editor looked okay once
```

## Required Atomic Test Closure

Every native module must list its required tests. Example:

```text
UGaspalsTrajectoryService:
  UT-LOCO-002 Walk Forward
  UT-LOCO-004 Run Forward
  UT-LOCO-005 Sprint Forward
  UT-TRANS-003 Run To Stop
  UT-ROT-001 Pivot 90 Left
```

If Run fails, do not mark Walk as failed automatically. If Walk passes, do not assume Run passes.

## MLDeformer Risk Clause

For any native output that changes final pose distribution:

```text
[ ] Record producer change.
[ ] Mark MLDeformer downstream risk.
[ ] Identify whether the case is locomotion-only, overlay, jump, or traversal.
[ ] Do not claim MLDeformer safety without downstream validation.
```
