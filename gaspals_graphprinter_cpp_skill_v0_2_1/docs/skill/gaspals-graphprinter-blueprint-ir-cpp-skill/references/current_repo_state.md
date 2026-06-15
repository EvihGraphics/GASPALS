# Current GASPALS / GraphPrinter State

## Repository

```text
primary connector repo: EvihGraphics/GASPALS
historical user-facing repo: logic-three-body/GASPALS
upstream: PolygonHive/GASPALS
default branch: main
engine association: 5.7
```

## Project Runtime

GASPALS is a UE 5.7 project integrating the Game Animation Sample Motion Matching stack with ALS-style overlay layering. Current formal runtime remains Blueprint-driven.

`GASPALSShadow` is present in the project plugin list but disabled. Treat it as a future observe-only/native mirror concept, not as a takeover path. Do not assume an implementation directory exists unless it is present in the checkout.

## Reference Repositories

`.gitmodules` includes reference repositories:

```text
References/ControlOperators
References/Motion-Matching
References/Learned-Motion-Matching
References/Unreal-3rd-Person-Parkour
References/Learned_Motion_Matching_Training
```

These are references, not the primary runtime host.

## GraphPrinter Current Behavior

Current automation is based on:

```text
Tools/ue_open_assets.py
Tools/setup_and_launch.ps1
Tools/ws_controller.py
Plugins/GraphPrinter
```

Current request:

```json
{
  "Command": "PrintGraphModules",
  "TargetKind": "GraphOnly",
  "SplitMode": "SemanticModules",
  "MaxModuleNodes": 40,
  "MaxModulePixels": [6000, 6000],
  "RequireTextChunk": true
}
```

Current QA baseline from docs:

```text
Assets total=86 ok=57 skip=20 fail=9
Modules ok=628 skip=27 fail=11
OK module PNGs contain GraphEditor text chunk
```

Known unrecoverable or difficult modules include postprocess/overlay graph cases and container-node failures.

## Existing Important Docs

```text
Docs/GraphPrinter_Plugin_Architecture.md
Docs/GraphPrinter_QA_2026-04-29.md
Docs/GASPALS_MotionMatching_Architecture.md
Docs/RuntimeInsertionPoints.md
```

## Initial Runtime-Critical Graph Targets

```text
Graph/Blueprints/01_Core/ABP_SandboxCharacter/
  ABP_SandboxCharacter_Update_EssentialValues_*.png
  ABP_SandboxCharacter_Update_Trajectory_*.png
  ABP_SandboxCharacter_Update_MovementDirection_*.png
  ABP_SandboxCharacter_Update_TargetRotation_*.png
  ABP_SandboxCharacter_Update_States_*.png
  ABP_SandboxCharacter_Update_MotionMatching_*.png
  ABP_SandboxCharacter_SetBlendStackAnimFromChooser_*.png
  ABP_SandboxCharacter_AnimGraph_*.png
```

## Current Missing Closure

The current pipeline can capture evidence, but the native rewrite is not closed until it includes:

```text
Blueprint IR
chunk stitching metadata
Blueprint-to-C++ mapping
native mirror implementation
atomic pose/action tests
Blueprint-vs-Native visual parity reports
```
