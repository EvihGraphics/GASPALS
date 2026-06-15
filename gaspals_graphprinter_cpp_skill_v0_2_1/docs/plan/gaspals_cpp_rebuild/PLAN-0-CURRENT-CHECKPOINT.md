# PLAN-0: Current Checkpoint

## Current Status

GASPALS currently has a working GraphPrinter-based capture flow that can batch open Blueprint / AnimBlueprint / WidgetBlueprint assets and export semantic module PNGs.

Known current state:

```text
Assets total=86 ok=57 skip=20 fail=9
Modules ok=628 skip=27 fail=11
OK module PNGs contain GraphEditor text chunk
```

Current request protocol:

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

## What Is Already Good

```text
[ ] GraphPrinter plugin exists and is integrated.
[ ] Remote WebSocket control exists.
[ ] Batch asset-opening script exists.
[ ] Semantic module split exists.
[ ] PNG TextChunk evidence exists for OK modules.
[ ] QA doc exists.
```

## What Is Not Closed

```text
[ ] Environment-driven automation is incomplete.
[ ] Stable _graph_module_catalog.json is missing or insufficient.
[ ] GraphEditor chunk extraction sidecars are not a first-class output.
[ ] Blueprint IR is not yet normalized.
[ ] Split module stitching is not yet a first-class contract.
[ ] Blueprint-to-C++ mapping is not closed.
[ ] C++ reconstruction task docs are not enough for parity.
[ ] Native visual parity harness does not exist.
[ ] Atomic pose/action tests do not exist.
```

## Current Strategic Decision

Do not start by rewriting the full AnimGraph.

Start by making the GraphPrinter evidence pipeline reconstruction-grade, then generate C++ tasks, then implement observe/mirror native logic, then validate with atomic parity tests.

## Immediate Next Plan

Proceed to:

```text
PLAN-1-GRAPHPRINTER-TO-BLUEPRINT-IR.md
```

Then:

```text
PLAN-2-BLUEPRINT-IR-TO-NATIVE.md
PLAN-3-NATIVE-POSE-UNIT-TESTS.md
```
