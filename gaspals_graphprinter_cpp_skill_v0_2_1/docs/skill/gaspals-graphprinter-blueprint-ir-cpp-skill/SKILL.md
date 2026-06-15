---
name: gaspals-graphprinter-blueprint-ir-cpp
description: Use this skill when optimizing GASPALS GraphPrinter blueprint capture into high-resolution semantic graph chunks, extracting GraphEditor text chunks, building Blueprint IR, stitching split graph modules, generating C++ reconstruction tasks, and preparing native Blueprint-vs-C++ visual parity tests.
---

# GASPALS GraphPrinter -> Blueprint IR -> C++ Rebuild Skill

## Mission

Turn GraphPrinter from a screenshot tool into a reconstruction-grade evidence pipeline:

```text
Blueprint asset
  -> semantic module PNGs
  -> embedded GraphEditor text chunk validation
  -> extracted sidecars
  -> Blueprint IR
  -> stitched module graph
  -> C++ reconstruction tasks
  -> native mirror implementation
  -> atomic visual parity tests
```

GraphPrinter does not directly prove native C++ parity. It produces the evidence required to rebuild and verify the native implementation.

## Current Capture Baseline

Current batch request shape:

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

Known current QA baseline:

```text
Assets total=86 ok=57 skip=20 fail=9
Modules ok=628 skip=27 fail=11
OK module PNGs contain GraphEditor text chunk
Main failure classes include CanImportNodesFromTextFalse and NoRecoverableContainerNode
```

## Capture Pipeline Requirements

The batch flow must be reproducible and environment-driven.

Preferred environment variables:

```text
GASPALS_PROJECT_DIR
GASPALS_UPROJECT
GASPALS_UE_EDITOR
GASPALS_OUTPUT_ROOT
GASPALS_STAGING_DIR
GASPALS_WS_URL
GASPALS_GRAPHPRINTER_MAX_MODULE_NODES
GASPALS_GRAPHPRINTER_MAX_MODULE_PIXELS
GASPALS_GRAPHPRINTER_REQUIRE_TEXT_CHUNK
```

Avoid hard-coded local paths in final automation.

## Required Outputs

GraphPrinter capture must produce:

```text
Graph/Blueprints/_asset_list.json
Graph/Blueprints/_index.json
Graph/Blueprints/_index.txt
Graph/Blueprints/_graph_module_catalog.json
Graph/Blueprints/_graph_module_catalog.tsv
Graph/Blueprints/_chunk_extract/**/*.txt
Graph/Blueprints/_chunk_extract/**/*.meta.json
Graph/Blueprints/_blueprint_ir/**/*.json
Graph/Blueprints/_stitch/**/*.json
Docs/CppRebuildTasks/*.md
Docs/GraphPrinter_FailureDiagnostics.md
```

## Graph Module Catalog

Each module entry must include:

```text
package_name
asset_name
asset_class
module_bucket
editor_graph_name
module_id
module_title
node_count
edge_count
png_path
png_width
png_height
text_chunk_present
chunk_extract_path
chunk_decode_status
blueprint_ir_path
stitch_group_id
incoming_module_links
outgoing_module_links
status
failure_stage
runtime_layer
migration_priority
source_commit
engine_version
plugin_version
capture_command_hash
```

## Blueprint IR Requirement

Every successfully decoded module should emit a normalized IR. The IR is not expected to be perfect in the first pass, but it must separate:

```text
nodes
pins
edges
variables read
variables written
function calls
macro calls
asset references
comment boxes
graph entry points
graph exit points
exec flow edges
data flow edges
latent/action nodes
state-machine nodes
animation graph nodes
```

If decoding is incomplete, write a partial IR with:

```text
decode_status=partial
decode_blockers=[...]
raw_chunk_path=<path>
```

Do not silently drop graph evidence.

## Chunk Stitching Requirement

Large Blueprints split into modules must be reconstructable.

Each chunk must record:

```text
chunk_id
parent_asset
parent_graph
split_reason
comment_box
bounds_in_graph_space
overlap_margin
incoming_links
outgoing_links
neighbor_chunks
reroute_continuations
variable_read_write_links
exec_continuation_links
stitch_confidence
```

This is critical. A visually clean screenshot is not enough if the Agent cannot recover the global graph flow.

## C++ Reconstruction Task Generation

Add or maintain:

```text
Tools/generate_cpp_rebuild_tasks.py
```

Input:

```text
Graph/Blueprints/_graph_module_catalog.json
Graph/Blueprints/_blueprint_ir/**/*.json
Graph/Blueprints/_stitch/**/*.json
```

Output:

```text
Docs/CppRebuildTasks/README.md
Docs/CppRebuildTasks/ABP_SandboxCharacter.md
Docs/CppRebuildTasks/CBP_SandboxCharacter.md
Docs/CppRebuildTasks/OverlaySystem.md
Docs/CppRebuildTasks/PostProcess.md
```

Each generated task must include:

```text
source asset
source graph
source module ids
source PNG paths
Blueprint IR paths
stitch group ids
runtime layer
migration mode
recommended native class / struct
recommended C++ method signature
Blueprint variables read/written
C++ field mapping candidates
asset references
required atomic unit tests
required visual parity cases
formal animation path risk
MLDeformer downstream pose-distribution risk
```

## Preferred First Rewrite Targets

Prioritize:

```text
ABP_SandboxCharacter::Update_EssentialValues
ABP_SandboxCharacter::Update_Trajectory
ABP_SandboxCharacter::Update_MovementDirection
ABP_SandboxCharacter::Update_TargetRotation
ABP_SandboxCharacter::Update_States
ABP_SandboxCharacter::Update_MotionMatching
ABP_SandboxCharacter::Update_MotionMatching_PostSelection
ABP_SandboxCharacter::SetBlendStackAnimFromChooser
CBP_SandboxCharacter input/state aggregation
ABP_LayerBlending / ALI_Overlay observation
```

Do not start by rewriting the whole AnimGraph final pose chain.

## Native Parity Link

Generated C++ tasks must declare their required atomic tests.

Example:

```text
Update_MovementDirection
  required tests:
    UT-LOCO-002 Walk Forward
    UT-LOCO-004 Run Forward
    UT-LOCO-006 Strafe Left
    UT-LOCO-007 Strafe Right
    UT-ROT-001 Pivot Left 90
```

Walk and Run must remain separate tests. Do not mark a module complete because it passed one long mixed showcase.

## Failure Diagnostics

For each failed module, emit:

```text
asset
graph
module id
failure stage
likely cause
recoverability
suggested next fix
whether runtime-critical
whether blocking native rebuild
```

Known failure stages to preserve:

```text
CanImportNodesFromTextFalse
NoRecoverableContainerNode
MissingGraphEditorChunk
WrongWidgetCaptured
PngMissing
ChunkDecodeFailed
StitchingAmbiguous
```

## Acceptance

A successful pass of this skill requires:

```text
[ ] GraphPrinter capture still produces valid PNGs.
[ ] OK PNGs still contain GraphEditor text chunks.
[ ] _graph_module_catalog.json exists.
[ ] _graph_module_catalog.tsv exists.
[ ] OK modules have sidecar metadata.
[ ] Decodable modules emit Blueprint IR.
[ ] Split graph modules emit stitching metadata.
[ ] C++ rebuild task docs are generated.
[ ] Each C++ task references atomic parity tests.
[ ] No GASPALS runtime behavior is changed.
[ ] No native parity claim is made from capture alone.
```

## Troubleshooting

```text
Details panel screenshot captured:
- Verify GenericGraphPrinter priority is above DetailsPanelPrinter.
- Verify GetActiveGraphEditor fallback is used.

Graph chunk has PNG but no GraphEditor chunk:
- Treat as failed evidence.
- Do not feed it into C++ reconstruction.

Large graph is split but cannot be stitched:
- Preserve chunks.
- Mark stitch_confidence=low.
- Emit missing link diagnostics.

Native code compiles but visual parity fails:
- Check the failing atomic case first.
- Do not debug through an all-in-one demo until atomic cases pass.
```
