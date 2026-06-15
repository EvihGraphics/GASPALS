# PLAN-2: Blueprint IR to Native C++ Reconstruction

## Objective

Use GraphPrinter evidence, Blueprint IR, and stitching metadata to create safe native C++ reconstruction tasks and initial mirror-only implementation targets.

## Phase 1: Generate C++ Rebuild Tasks

Add or update:

```text
Tools/generate_cpp_rebuild_tasks.py
```

Inputs:

```text
_graph_module_catalog.json
_blueprint_ir/**/*.json
_stitch/**/*.json
```

Outputs:

```text
Docs/CppRebuildTasks/README.md
Docs/CppRebuildTasks/ABP_SandboxCharacter.md
Docs/CppRebuildTasks/CBP_SandboxCharacter.md
Docs/CppRebuildTasks/OverlaySystem.md
Docs/CppRebuildTasks/PostProcess.md
```

## Phase 2: Blueprint-to-C++ Mapping

For each candidate function/module, generate mapping docs:

```text
Blueprint variable -> C++ field
Blueprint function -> C++ method
Blueprint asset reference -> native asset pointer/config
```

Preferred initial native types:

```text
FGaspalsLocomotionInput
FGaspalsMovementState
FGaspalsTrajectorySample
FGaspalsTrajectoryState
FGaspalsOverlayState
FGaspalsMotionMatchingDecision
FGaspalsNativeAnimTelemetryFrame
```

## Phase 3: Native Plugin Skeleton

Prepare observe/mirror infrastructure only:

```text
Plugins/GASPALSNative/
Plugins/GASPALSNative/Source/GASPALSNative/Public/
Plugins/GASPALSNative/Source/GASPALSNative/Private/
```

Suggested classes:

```text
UGaspalsShadowObserverComponent
UGaspalsNativeCharacterComponent
UGaspalsNativeAnimInstanceBridge
UGaspalsTrajectoryService
UGaspalsMotionStateService
UGaspalsOverlayRoutingService
UGaspalsVisualParitySubsystem
```

Do not enable formal takeover.

## Phase 4: First Mirror Targets

Prioritize:

```text
Update_EssentialValues
Update_Trajectory
Update_MovementDirection
Update_TargetRotation
Update_States
```

Do not start with the final AnimGraph or overlay full replacement.

## Phase 5: Required Test Mapping

Each native method must list atomic tests.

Example:

```text
UGaspalsMotionStateService::UpdateMovementDirection
  required:
    UT-LOCO-002 Walk Forward
    UT-LOCO-004 Run Forward
    UT-LOCO-006 Strafe Left
    UT-LOCO-007 Strafe Right
```

## Acceptance

```text
[ ] C++ rebuild task docs exist.
[ ] Mapping docs exist.
[ ] Native plugin skeleton compiles if implemented.
[ ] No formal runtime takeover is enabled.
[ ] Each native candidate lists required atomic tests.
[ ] No module is accepted from compilation alone.
```
