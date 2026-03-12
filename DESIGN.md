# WAD Explorer Design

## Purpose
- Provide a single-file Tkinter WAD asset browser in [`wad_explorer.py`](/e:/M-Github-Game/defiance-tools/wad_explorer.py).
- Support Defiance WAD archive browsing, decompressed HexView inspection, image preview where practical, and `.cache` extraction with path preservation.
- Keep `GLView` intentionally unimplemented but structurally reserved.

## Runtime Constraints
- Use the local virtual environment at `.venv`.
- Avoid non-stdlib Python dependencies.
- Target Windows desktop usage first.
- Keep the implementation self-contained so agents can modify one file without cross-module coordination.

## Scope Implemented
- Toolbar with root path display, `Open Folder`, and `Open File`.
- Split layout with left `Treeview` and right `Notebook` tabs for `HexView`, `ImageView`, and `GLView`.
- Status bar with a debug/status string and progress bar.
- WAD parsing for headers, chained index blocks, and name resolution.
- RMID handling for raw payloads, container decompression, and texture special-case container handling.
- Preview support for Tk-compatible embedded images and Defiance texture formats `RGBA32`, `RGBA64` down-converted, `DXT1`, and `DXT5` path for formats `3` and `8`.
- Extraction support for file and folder nodes into `.cache/<wad_name>/<asset_path>`.

## Non-Goals
- Full mesh parsing or rendering.
- Audio playback.
- Full RMID reference visualization.
- Background worker threads.
- Packaging or installer integration.

## Architecture

### High-Level Flow
1. User opens a WAD directory or a single `.wad` file.
2. `WadParser` scans WAD metadata and resolves record names.
3. UI builds a tree grouped by WAD file and logical folder path.
4. Selecting a record loads raw bytes from disk.
5. If the payload is RMID container data, it is decompressed before HexView display.
6. If the decompressed payload is a supported texture or image, `ImageView` renders it.
7. Context menu extraction writes decompressed payloads into `.cache`.

### Main Responsibilities
- `WadParser`
  - enumerate WAD files
  - parse headers and index chains
  - construct immutable `WadRecord` objects
- RMID helpers
  - identify RMID payloads
  - unpack container payloads
  - expose decompressed bytes for HexView and extraction
- Preview helpers
  - decode texture payloads
  - build Tk-compatible image data
- `WadExplorerApp`
  - own UI widgets
  - react to tree selection
  - maintain payload cache
  - handle extraction commands

## Data Model

### `WadRecord`
- Immutable record descriptor for one WAD asset.
- Stores source WAD path, asset id, asset type, offsets, sizes, and resolved name.

### `WadFileData`
- Contains one `.wad` path and its parsed `WadRecord` list.

### `RmidData`
- Represents either a raw RMID or a decompressed RMID container result.
- Tracks final bytes, resolved RMID header, whether decompression occurred, and a diagnostic note.

### `AssetPayload`
- Cached per record.
- Bundles original stored bytes, view bytes used in HexView, optional RMID interpretation, optional preview payload, and extraction suffix.

## UI Layout

### Toolbar
- Root path display is read-only by design.
- Buttons open either a WAD directory or a single `.wad`.

### Tree
- Top level: WAD filename nodes.
- Nested folder nodes are synthesized from asset names split on `/` and `\`.
- Leaf nodes are real asset records.
- Columns show type and stored size.

### Right Pane
- Detail label shows selected asset metadata and RMID diagnostic note.
- `HexView` shows decompressed bytes where possible and truncates large payloads for UI stability.
- `ImageView` uses a scrollable canvas with generated PPM or embedded Tk-compatible image bytes.
- `GLView` remains a placeholder tab.

### Status Bar
- Left side is a human-readable debug/status string.
- Right side is a visible progress indicator.

## Extraction Semantics
- Extraction target is always `.cache`.
- Layout rule is `.cache/<safe_wad_name>/<preserved_asset_path>`.
- Suffix rule is `.rmid` for RMID-derived payloads and `.bin` otherwise.
- Extraction writes the decompressed bytes used by `HexView`, not the original compressed WAD slice.

## Preview Pipeline

### Standard Images
- Supported when payload bytes are already PNG, GIF, or PPM/PGM.

### Defiance Texture Preview
- Implemented locally to avoid Pillow.
- Supported:
  - format `0` as raw RGBA
  - format `1` as DXT1
  - format `3` and `8` through the DXT5 path, matching the original C tool
  - format `6` as 64-bit RGBA reduced to 8-bit preview

### Why PPM
- Tk `PhotoImage` can load PPM without external libraries.
- This keeps the app inside stdlib-only constraints.

## GLView Roadmap
- Keep `GLView` as a tab, not a future layout change.
- Expected future work:
  - parse `MES` and `SKI` payload structures
  - decode materials and referenced textures
  - select an OpenGL host strategy for Tkinter on Windows
  - manage GPU resource lifetime outside the current single-file preview path

## Extension Points

### Safe Next Changes
- Add tree search and filtering.
- Add richer RMID header and reference inspection.
- Add thumbnail caching for texture records.
- Add export commands for PNG, DDS, OBJ, and WAV by shelling out to existing repository tools.
- Add background worker support for very large WAD directories.

### Risky Areas
- RMID container handling for non-texture special cases is based on observed C behavior, not a complete formal spec.
- Texture formats beyond the current subset remain unsupported.
- Large assets can still block the UI because loading is synchronous.

## Agent Guidance

### When Updating Parsing
- Prefer preserving current dataclass shapes unless a change clearly reduces complexity.
- Keep WAD parsing and RMID decoding separate from Tk widget code.
- If adding new binary structures, define them with explicit `struct.Struct` constants near the top of the file.

### When Updating UI
- Keep the left tree and right notebook split stable.
- Avoid introducing hidden global state outside `WadExplorerApp`.
- Preserve the status bar contract of one debug string and one progress indicator.

### When Adding `GLView`
- Do not remove the placeholder tab first.
- Implement rendering behind reusable decoding helpers so `HexView` and `ImageView` stay independent.
- Make render initialization failure non-fatal and keep other tabs functional.

## Validation
- Syntax validation command:

```powershell
.\.venv\Scripts\python.exe -m py_compile wad_explorer.py
```

- Launch command:

```powershell
.\.venv\Scripts\python.exe .\wad_explorer.py
```
