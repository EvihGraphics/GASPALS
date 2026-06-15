# Codex Prompt: GraphPrinter Blueprint IR Pass

Focus only on GraphPrinter evidence and Blueprint IR.

Do not implement native runtime takeover.

## Read

```text
docs/skill/gaspals-graphprinter-blueprint-ir-cpp-skill/SKILL.md
docs/spec/graphprinter_blueprint_ir_schema.md
docs/spec/graph_chunk_stitching_contract.md
docs/plan/gaspals_cpp_rebuild/PLAN-1-GRAPHPRINTER-TO-BLUEPRINT-IR.md
Tools/ue_open_assets.py
Tools/ws_controller.py
Docs/GraphPrinter_Plugin_Architecture.md
Docs/GraphPrinter_QA_2026-04-29.md
```

## Implement

```text
1. Environment-driven GraphPrinter capture config.
2. _graph_module_catalog.json.
3. _graph_module_catalog.tsv.
4. TextChunk sidecar metadata for OK PNGs.
5. Partial Blueprint IR JSON.
6. Stitch group metadata.
7. Failure diagnostics report.
```

## Avoid

```text
- Do not edit formal GASPALS animation logic.
- Do not enable takeover.
- Do not require perfect chunk decoding before producing partial IR.
- Do not silently drop failed modules.
```

## Acceptance

```text
[ ] Existing PNG export still works.
[ ] OK PNGs still validate GraphEditor chunk.
[ ] Catalog and TSV exist.
[ ] Sidecar metadata exists.
[ ] Partial IR exists for decodable modules.
[ ] Stitch report exists.
[ ] Failure diagnostics report exists.
```
