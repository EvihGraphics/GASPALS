# Blueprint-to-C++ Mapping Contract

## Purpose

C++ reconstruction requires stable mapping from Blueprint variables/functions/assets to native fields and services.

## Output Location

```text
Docs/CppRebuildTasks/BlueprintToCppMapping/*.json
Docs/CppRebuildTasks/BlueprintToCppMapping/*.md
```

## Mapping Record

```json
{
  "schema_version": "gaspals.bp_to_cpp_mapping.v1",
  "source_asset": "ABP_SandboxCharacter",
  "source_graph": "Update_EssentialValues",
  "source_modules": [],
  "native_target": {
    "plugin": "GASPALSNative",
    "class": "UGaspalsNativeAnimInstanceBridge",
    "service": "UGaspalsMotionStateService",
    "method": "UpdateEssentialValues"
  },
  "variables": [],
  "functions": [],
  "assets": [],
  "tests": []
}
```

## Variable Mapping

```json
{
  "blueprint_name": "",
  "cpp_name": "",
  "cpp_type": "",
  "owner": "Character|AnimInstance|Component|Service|Struct|Unknown",
  "access": "read|write|read_write",
  "update_frequency": "per_frame|on_state_change|on_asset_change|unknown",
  "thread_safe": false,
  "source_of_truth": "Blueprint|NativeMirror|NativeFormal",
  "computed_by": "",
  "consumed_by": [],
  "tolerance": {
    "absolute": null,
    "relative": null,
    "unit": ""
  },
  "telemetry_field": ""
}
```

## Function Mapping

```json
{
  "blueprint_function": "",
  "native_method": "",
  "native_class": "",
  "migration_mode": "observe-only|mirror-only|replace-with-fallback|formal-takeover",
  "runtime_layer": "",
  "inputs": [],
  "outputs": [],
  "side_effects": [],
  "required_tests": []
}
```

## Required Native Types

Prefer these initial names unless existing code establishes better ones:

```text
FGaspalsLocomotionInput
FGaspalsMovementState
FGaspalsTrajectorySample
FGaspalsTrajectoryState
FGaspalsOverlayState
FGaspalsMotionMatchingDecision
FGaspalsNativeAnimTelemetryFrame
```

## Acceptance

A mapping is not accepted until:

```text
[ ] Every mapped variable has C++ type and owner.
[ ] Every mapped function has migration mode.
[ ] Every mapped function has required atomic tests.
[ ] Tolerances exist for telemetry comparison.
[ ] Asset references are explicit.
[ ] No formal takeover is implied without visual parity cases.
```
