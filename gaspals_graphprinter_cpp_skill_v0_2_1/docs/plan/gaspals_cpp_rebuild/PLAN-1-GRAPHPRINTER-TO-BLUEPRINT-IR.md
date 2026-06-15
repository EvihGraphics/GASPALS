# PLAN-1: GraphPrinter to Blueprint IR

## Objective

Upgrade GraphPrinter capture from PNG evidence to reconstruction-grade Blueprint evidence.

## Phase 1: Reproducible Capture Configuration

Tasks:

```text
[ ] Replace hard-coded project paths with environment-driven config.
[ ] Add a capture config JSON or YAML snapshot.
[ ] Write the active capture command into every catalog entry.
[ ] Preserve existing behavior when env vars are absent.
```

Acceptance:

```text
[ ] Existing batch still runs.
[ ] Output location can be changed without editing scripts.
[ ] WebSocket URL can be changed without editing scripts.
```

## Phase 2: Stable Module Catalog

Tasks:

```text
[ ] Generate _graph_module_catalog.json.
[ ] Generate _graph_module_catalog.tsv.
[ ] Include module path, asset, graph, status, text chunk, runtime layer, priority.
[ ] Include source commit / engine / plugin version if available.
```

Acceptance:

```text
[ ] Every OK/SKIP/FAIL module is represented.
[ ] Runtime-critical ABP_SandboxCharacter modules are easy to filter.
```

## Phase 3: TextChunk Extraction Sidecars

Tasks:

```text
[ ] Extract GraphEditor text chunk from OK PNGs when possible.
[ ] Write raw chunk sidecar.
[ ] Write sidecar metadata.
[ ] If decode is not implemented, mark decode_status=pending_decoder.
```

Acceptance:

```text
[ ] OK PNGs have sidecar metadata.
[ ] Missing chunk causes failed evidence status.
```

## Phase 4: Blueprint IR

Tasks:

```text
[ ] Implement partial GraphEditor chunk decoder or structured placeholder.
[ ] Emit _blueprint_ir/**/*.json.
[ ] Extract at least nodes, pins, edges where possible.
[ ] Extract variable read/write and function calls when possible.
[ ] Preserve raw chunk for undecoded modules.
```

Acceptance:

```text
[ ] Decodable modules produce IR.
[ ] Partial modules clearly state blockers.
[ ] No silent data loss.
```

## Phase 5: Stitching Metadata

Tasks:

```text
[ ] Group module chunks by asset and graph.
[ ] Emit _stitch/**/*.json.
[ ] Record neighbor chunks and unresolved links.
[ ] Add report for low-confidence stitching.
```

Acceptance:

```text
[ ] Large Blueprint chunks can be navigated as one graph group.
[ ] Cross-chunk loss is explicit.
```

## Phase 6: Failure Diagnostics

Tasks:

```text
[ ] Generate Docs/GraphPrinter_FailureDiagnostics.md.
[ ] Preserve failure stages.
[ ] Mark runtime-critical blockers.
```

Acceptance:

```text
[ ] Known failures are actionable.
[ ] Runtime-critical modules are prioritized.
```

## Deliverables

```text
Graph/Blueprints/_graph_module_catalog.json
Graph/Blueprints/_graph_module_catalog.tsv
Graph/Blueprints/_chunk_extract/**
Graph/Blueprints/_blueprint_ir/**
Graph/Blueprints/_stitch/**
Docs/GraphPrinter_FailureDiagnostics.md
```
