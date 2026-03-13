from __future__ import annotations

import base64
import datetime as dt
import re
import struct
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Optional

import tkinter as tk
from tkinter import filedialog, messagebox, ttk


WAD_HEADER_STRUCT = struct.Struct("<8I")
WAD_INDEX_HEADER_STRUCT = struct.Struct("<4I")
WAD_INDEX_RECORD_STRUCT = struct.Struct("<IIIIQII")

RMID_HEADER_STRUCT = struct.Struct("<IIHHI")
RMID_CON_HEADER_STRUCT = struct.Struct("<7QIBII")

WAD_MAGIC = b"WADF"
RMID_MAGIC = struct.unpack("<I", b"RMID")[0]

RMID_TYPE_RAW = 0x0001
RMID_TYPE_TEX = 0x0003
RMID_TYPE_CON = 0x0016
RMID_TEX_HEADER_SIZE = 872

HEX_VIEW_LIMIT = 512 * 1024
STANDARD_IMAGE_FORMATS = (
    (b"\x89PNG\r\n\x1a\n", "PNG"),
    (b"GIF87a", "GIF"),
    (b"GIF89a", "GIF"),
)

ASSET_TYPE_NAMES = {
    0x0001: "Raw",
    0x0002: "Shader",
    0x0003: "Texture",
    0x0004: "Mesh",
    0x0005: "Skin",
    0x0006: "Actor",
    0x0007: "Skeleton",
    0x0008: "Animation",
    0x0009: "Sound",
    0x0011: "Movie",
    0x0012: "Speech",
    0x0016: "Container",
    0x0023: "LipSync",
}


def asset_type_name(asset_type: int) -> str:
    return ASSET_TYPE_NAMES.get(asset_type, f"0x{asset_type:04X}")


def format_size(size: int) -> str:
    value = float(size)
    for unit in ("B", "KB", "MB", "GB"):
        if value < 1024.0 or unit == "GB":
            return f"{value:.1f} {unit}" if unit != "B" else f"{int(value)} B"
        value /= 1024.0
    return f"{size} B"


def format_timestamp(epoch_value: int) -> str:
    try:
        timestamp = dt.datetime.fromtimestamp(epoch_value)
        return timestamp.strftime("%Y-%m-%d %H:%M:%S")
    except (OverflowError, OSError, ValueError):
        return f"{epoch_value}"


def sanitize_component(name: str) -> str:
    cleaned = re.sub(r'[<>:"|?*\x00-\x1F]', "_", name.strip())
    return cleaned or "_"


def split_asset_parts(asset_name: str) -> list[str]:
    normalized = asset_name.replace("\\", "/")
    parts = [sanitize_component(part) for part in normalized.split("/") if part and part != "."]
    return parts or ["unnamed"]


def split_tree_parts(asset_name: str) -> list[str]:
    normalized = asset_name.replace("\\", "/")
    path_parts = [part for part in normalized.split("/") if part and part != "."]
    if not path_parts:
        return ["unnamed"]

    tree_parts: list[str] = []
    for part in path_parts:
        underscore_parts = [token for token in part.split("_") if token]
        if len(underscore_parts) >= 3:
            tree_parts.append(sanitize_component(underscore_parts[1]))
            tree_parts.append(sanitize_component("_".join(underscore_parts[2:])))
        else:
            tree_parts.append(sanitize_component(part))
    return tree_parts or ["unnamed"]


def ensure_output_name(asset_name: str, suffix: str) -> Path:
    output = Path(*split_asset_parts(asset_name))
    if not output.suffix:
        output = output.with_name(output.name + suffix)
    return output


def read_c_string(file_obj, offset: int, max_bytes: int = 512) -> str:
    current = file_obj.tell()
    try:
        file_obj.seek(offset)
        data = file_obj.read(max_bytes)
    finally:
        file_obj.seek(current)

    end = data.find(b"\x00")
    if end >= 0:
        data = data[:end]
    return data.decode("utf-8", errors="replace").strip() or f"unnamed_{offset:X}"


def hexdump(data: bytes, max_bytes: int = HEX_VIEW_LIMIT) -> str:
    visible = data[:max_bytes]
    lines: list[str] = []
    for offset in range(0, len(visible), 16):
        chunk = visible[offset : offset + 16]
        left = " ".join(f"{byte:02X}" for byte in chunk[:8])
        right = " ".join(f"{byte:02X}" for byte in chunk[8:])
        hex_part = f"{left:<23}  {right:<23}".rstrip()
        ascii_part = "".join(chr(byte) if 32 <= byte < 127 else "." for byte in chunk)
        lines.append(f"{offset:08X}  {hex_part:<48}  {ascii_part}")

    if len(data) > max_bytes:
        lines.append("")
        lines.append(
            f"... truncated: showing {format_size(max_bytes)} of {format_size(len(data))} ..."
        )
    return "\n".join(lines)


def rgba_to_ppm(width: int, height: int, rgba: bytes) -> bytes:
    rgb = bytearray(width * height * 3)
    rgb_index = 0
    for i in range(0, min(len(rgba), width * height * 4), 4):
        rgb[rgb_index : rgb_index + 3] = rgba[i : i + 3]
        rgb_index += 3
    header = f"P6\n{width} {height}\n255\n".encode("ascii")
    return header + bytes(rgb)


def photo_image_from_bytes(data: bytes, image_format: Optional[str] = None) -> tk.PhotoImage:
    encoded = base64.b64encode(data).decode("ascii")
    kwargs = {"data": encoded}
    if image_format:
        kwargs["format"] = image_format
    return tk.PhotoImage(**kwargs)


def rgb565_to_rgb(color: int) -> tuple[int, int, int]:
    r = (color >> 11) & 0x1F
    g = (color >> 5) & 0x3F
    b = color & 0x1F
    r = (r << 3) | (r >> 2)
    g = (g << 2) | (g >> 4)
    b = (b << 3) | (b >> 2)
    return r, g, b


def decode_dxt1(width: int, height: int, blocks: bytes) -> bytes:
    rgba = bytearray(width * height * 4)
    block_count_x = (width + 3) // 4
    block_count_y = (height + 3) // 4
    offset = 0

    for block_y in range(block_count_y):
        for block_x in range(block_count_x):
            block = blocks[offset : offset + 8]
            if len(block) < 8:
                break
            offset += 8

            color0, color1, code = struct.unpack_from("<HHI", block, 0)
            r0, g0, b0 = rgb565_to_rgb(color0)
            r1, g1, b1 = rgb565_to_rgb(color1)
            colors = [(r0, g0, b0, 255), (r1, g1, b1, 255)]
            if color0 > color1:
                colors.append(((2 * r0 + r1) // 3, (2 * g0 + g1) // 3, (2 * b0 + b1) // 3, 255))
                colors.append(((r0 + 2 * r1) // 3, (g0 + 2 * g1) // 3, (b0 + 2 * b1) // 3, 255))
            else:
                colors.append(((r0 + r1) // 2, (g0 + g1) // 2, (b0 + b1) // 2, 255))
                colors.append((0, 0, 0, 0))

            for py in range(4):
                for px in range(4):
                    x = block_x * 4 + px
                    y = block_y * 4 + py
                    if x >= width or y >= height:
                        continue
                    color_index = (code >> (2 * (4 * py + px))) & 0x03
                    r, g, b, a = colors[color_index]
                    pixel_offset = (y * width + x) * 4
                    rgba[pixel_offset : pixel_offset + 4] = bytes((r, g, b, a))

    return bytes(rgba)


def decode_dxt5(width: int, height: int, blocks: bytes) -> bytes:
    rgba = bytearray(width * height * 4)
    block_count_x = (width + 3) // 4
    block_count_y = (height + 3) // 4
    offset = 0

    for block_y in range(block_count_y):
        for block_x in range(block_count_x):
            block = blocks[offset : offset + 16]
            if len(block) < 16:
                break
            offset += 16

            alpha0 = block[0]
            alpha1 = block[1]
            alpha_bits = int.from_bytes(block[2:8], "little")
            alpha_table = [alpha0, alpha1]
            if alpha0 > alpha1:
                alpha_table.extend(
                    ((8 - i) * alpha0 + (i - 1) * alpha1) // 7 for i in range(2, 8)
                )
            else:
                alpha_table.extend(
                    ((6 - i) * alpha0 + (i - 1) * alpha1) // 5 for i in range(2, 6)
                )
                alpha_table.extend((0, 255))

            color0, color1, code = struct.unpack_from("<HHI", block, 8)
            r0, g0, b0 = rgb565_to_rgb(color0)
            r1, g1, b1 = rgb565_to_rgb(color1)
            colors = [
                (r0, g0, b0),
                (r1, g1, b1),
                ((2 * r0 + r1) // 3, (2 * g0 + g1) // 3, (2 * b0 + b1) // 3),
                ((r0 + 2 * r1) // 3, (g0 + 2 * g1) // 3, (b0 + 2 * b1) // 3),
            ]

            for py in range(4):
                for px in range(4):
                    x = block_x * 4 + px
                    y = block_y * 4 + py
                    if x >= width or y >= height:
                        continue
                    pixel_index = 4 * py + px
                    alpha_index = (alpha_bits >> (3 * pixel_index)) & 0x07
                    color_index = (code >> (2 * pixel_index)) & 0x03
                    r, g, b = colors[color_index]
                    a = alpha_table[alpha_index]
                    pixel_offset = (y * width + x) * 4
                    rgba[pixel_offset : pixel_offset + 4] = bytes((r, g, b, a))

    return bytes(rgba)


@dataclass(frozen=True)
class WadRecord:
    wad_path: Path
    wad_name: str
    asset_id: int
    asset_type: int
    name_offset: int
    data_offset: int
    data_size: int
    modified_time: int
    name: str

    @property
    def key(self) -> str:
        return f"{self.wad_path}|{self.asset_id}|{self.data_offset}|{self.data_size}"


@dataclass(frozen=True)
class WadFileData:
    path: Path
    records: list[WadRecord]


@dataclass(frozen=True)
class RmidHeaderInfo:
    asset_id: int
    version: int
    num_references: int
    asset_type: int
    magic: int


@dataclass(frozen=True)
class RmidData:
    data: bytes
    header: RmidHeaderInfo
    was_container: bool
    note: str


@dataclass(frozen=True)
class TexturePreview:
    width: int
    height: int
    ppm_bytes: bytes
    description: str


@dataclass(frozen=True)
class AssetPayload:
    source_bytes: bytes
    view_bytes: bytes
    rmid: Optional[RmidData]
    hex_title: str
    preview: Optional[TexturePreview]
    image_bytes: Optional[bytes]
    image_format: Optional[str]
    output_suffix: str


class WadParser:
    @staticmethod
    def load(
        source_path: Path,
        progress: Optional[Callable[[str, int, int], None]] = None,
    ) -> tuple[Path, list[WadFileData]]:
        if source_path.is_dir():
            wad_paths = sorted(source_path.glob("*.wad"))
            root_path = source_path
        elif source_path.is_file():
            wad_paths = [source_path]
            root_path = source_path.parent
        else:
            raise FileNotFoundError(f"Path not found: {source_path}")

        if not wad_paths:
            raise FileNotFoundError("No .wad files were found.")

        files: list[WadFileData] = []
        total = len(wad_paths)
        for index, wad_path in enumerate(wad_paths, start=1):
            if progress:
                progress(f"Loading {wad_path.name}", index - 1, total)
            files.append(WadParser._load_wad_file(wad_path))
            if progress:
                progress(f"Loaded {wad_path.name}", index, total)
        return root_path, files

    @staticmethod
    def _load_wad_file(wad_path: Path) -> WadFileData:
        records: list[WadRecord] = []
        with wad_path.open("rb") as file_obj:
            header_data = file_obj.read(WAD_HEADER_STRUCT.size)
            if len(header_data) != WAD_HEADER_STRUCT.size or header_data[:4] != WAD_MAGIC:
                raise ValueError(f"{wad_path} is not a valid WAD file.")

            _, _, total_records, _, _, _, _, _ = WAD_HEADER_STRUCT.unpack(header_data) # "<8I"
            next_offset = file_obj.tell()

            while len(records) < total_records:
                file_obj.seek(next_offset)
                index_data = file_obj.read(WAD_INDEX_HEADER_STRUCT.size)
                if len(index_data) != WAD_INDEX_HEADER_STRUCT.size:
                    raise ValueError(f"Unexpected EOF while reading {wad_path}.")
                record_count, next_header_offset, _, _ = WAD_INDEX_HEADER_STRUCT.unpack(index_data) # "<4I"

                for _ in range(record_count):
                    raw_record = file_obj.read(WAD_INDEX_RECORD_STRUCT.size)
                    if len(raw_record) != WAD_INDEX_RECORD_STRUCT.size:
                        raise ValueError(f"Unexpected EOF while reading index records from {wad_path}.")
                    (
                        asset_id,
                        data_offset,
                        data_size,
                        name_offset,
                        modified_time,
                        asset_type,
                        _,
                    ) = WAD_INDEX_RECORD_STRUCT.unpack(raw_record) # "<IIIIQII"
                    name = read_c_string(file_obj, name_offset)
                    records.append(
                        WadRecord(
                            wad_path=wad_path,
                            wad_name=wad_path.stem,
                            asset_id=asset_id,
                            asset_type=asset_type,
                            name_offset=name_offset,
                            data_offset=data_offset,
                            data_size=data_size,
                            modified_time=modified_time,
                            name=name,
                        )
                    )

                if len(records) >= total_records:
                    break
                next_offset = next_header_offset if next_header_offset else file_obj.tell()

        return WadFileData(path=wad_path, records=records)


def parse_rmid_header(data: bytes, offset: int = 0) -> Optional[RmidHeaderInfo]:
    if len(data) < offset + RMID_HEADER_STRUCT.size:
        return None
    asset_id, version, num_references, asset_type, magic = RMID_HEADER_STRUCT.unpack_from( #"<IIHHI"
        data, offset
    )
    return RmidHeaderInfo(
        asset_id=asset_id,
        version=version,
        num_references=num_references,
        asset_type=asset_type,
        magic=magic,
    )


def decode_rmid(data: bytes) -> Optional[RmidData]:
    header = parse_rmid_header(data)
    if not header or header.magic != RMID_MAGIC:
        return None

    if header.asset_type != RMID_TYPE_CON:
        return RmidData(data=data, header=header, was_container=False, note="raw-rmid")

    if len(data) < RMID_HEADER_STRUCT.size + RMID_CON_HEADER_STRUCT.size:
        return RmidData(data=data, header=header, was_container=True, note="container-header-short")

    (
        _data_offset_offset,
        _id_offset,
        _type_offset,
        _compressed_size_offset,
        _uncompressed_size_offset,
        _unk1,
        _data_offset,
        container_id,
        _container_type,
        _compressed_size,
        uncompressed_size,
    ) = RMID_CON_HEADER_STRUCT.unpack_from(data, RMID_HEADER_STRUCT.size)

    payload_offset = RMID_HEADER_STRUCT.size + RMID_CON_HEADER_STRUCT.size
    next_header = parse_rmid_header(data, payload_offset)

    try:
        if next_header and next_header.asset_id == container_id and next_header.asset_type == RMID_TYPE_TEX:
            prefix_end = payload_offset + RMID_HEADER_STRUCT.size + RMID_TEX_HEADER_SIZE
            if len(data) < prefix_end:
                return RmidData(
                    data=data,
                    header=header,
                    was_container=True,
                    note="container-texture-prefix-short",
                )
            prefix = data[payload_offset:prefix_end]
            inflated = zlib.decompress(data[prefix_end:])
            decoded = prefix + inflated
        else:
            decoded = zlib.decompress(data[payload_offset:])
    except zlib.error:
        return RmidData(data=data, header=header, was_container=True, note="inflate-failed")

    final_header = parse_rmid_header(decoded)
    if not final_header or final_header.magic != RMID_MAGIC:
        final_header = header

    note = "decompressed"
    if uncompressed_size and len(decoded) != uncompressed_size:
        note = f"decompressed-size-mismatch({len(decoded)}/{uncompressed_size})"

    return RmidData(data=decoded, header=final_header, was_container=True, note=note)


def decode_texture_preview(data: bytes) -> Optional[TexturePreview]:
    if len(data) < RMID_HEADER_STRUCT.size + RMID_TEX_HEADER_SIZE:
        return None

    header = parse_rmid_header(data)
    if not header or header.asset_type != RMID_TYPE_TEX:
        return None

    tex_offset = RMID_HEADER_STRUCT.size
    cubemap_flag = data[tex_offset + 96]
    texture_format = data[tex_offset + 98]
    bits_per_pixel = data[tex_offset + 100]

    width, height, _unk1, mipmap_count = struct.unpack_from("<4I", data, tex_offset + 112)
    mip_records = [
        struct.unpack_from("<4I", data, tex_offset + 128 + (index * 16))
        for index in range(13)
    ]

    body_offset = tex_offset + RMID_TEX_HEADER_SIZE
    description = f"format={texture_format} mipmaps={mipmap_count}"

    if texture_format in (1, 3, 8):
        selected_width = max(mip_records[0][0], 4)
        selected_height = max(mip_records[0][1], 4)
        block_size = 8 if texture_format == 1 else 16
        body_size = ((selected_width + 3) // 4) * ((selected_height + 3) // 4) * block_size
        body = data[body_offset : body_offset + body_size]
        if texture_format == 1:
            rgba = decode_dxt1(selected_width, selected_height, body)
            description += " DXT1"
        else:
            rgba = decode_dxt5(selected_width, selected_height, body)
            description += " DXT5"
        return TexturePreview(
            width=selected_width,
            height=selected_height,
            ppm_bytes=rgba_to_ppm(selected_width, selected_height, rgba),
            description=description,
        )

    if texture_format == 0:
        body_size = width * height * 4
        body = data[body_offset : body_offset + body_size]
        if len(body) < body_size:
            return None
        if cubemap_flag:
            description += " cubemap-face=1"
        return TexturePreview(
            width=width,
            height=height,
            ppm_bytes=rgba_to_ppm(width, height, body),
            description=description + " RGBA32",
        )

    if texture_format == 6 and bits_per_pixel == 64:
        pixel_count = width * height
        raw = data[body_offset : body_offset + (pixel_count * 8)]
        if len(raw) < pixel_count * 8:
            return None
        rgba = bytearray(pixel_count * 4)
        for index in range(pixel_count):
            source = index * 8
            target = index * 4
            rgba[target + 0] = raw[source + 1]
            rgba[target + 1] = raw[source + 3]
            rgba[target + 2] = raw[source + 5]
            rgba[target + 3] = raw[source + 7]
        return TexturePreview(
            width=width,
            height=height,
            ppm_bytes=rgba_to_ppm(width, height, bytes(rgba)),
            description=description + " RGBA64",
        )

    return None


def detect_standard_image(data: bytes) -> tuple[Optional[bytes], Optional[str]]:
    for signature, image_format in STANDARD_IMAGE_FORMATS:
        if data.startswith(signature):
            return data, image_format
    if data.startswith(b"P6\n") or data.startswith(b"P5\n"):
        return data, None
    return None, None


class WadExplorerApp(tk.Tk):
    def __init__(self, initial_path: Optional[Path] = None) -> None:
        super().__init__()
        self.title("WAD Explorer")
        self.geometry("1440x920")
        self.minsize(1024, 720)

        self.root_path: Optional[Path] = None
        self.wad_files: list[WadFileData] = []
        self.item_meta: dict[str, dict[str, object]] = {}
        self.payload_cache: dict[str, AssetPayload] = {}
        self.active_image: Optional[tk.PhotoImage] = None

        self.path_var = tk.StringVar(value="")
        self.status_var = tk.StringVar(value="Idle")
        self.detail_var = tk.StringVar(value="No WAD loaded.")
        self.image_info_var = tk.StringVar(value="No preview available.")

        self._build_ui()

        if initial_path:
            self.after(100, lambda: self.load_path(initial_path))

    def _build_ui(self) -> None:
        self.columnconfigure(0, weight=1)
        self.rowconfigure(1, weight=1)

        toolbar = ttk.Frame(self, padding=(8, 8, 8, 6))
        toolbar.grid(row=0, column=0, sticky="ew")
        toolbar.columnconfigure(1, weight=1)

        ttk.Label(toolbar, text="WAD Root").grid(row=0, column=0, padx=(0, 8), sticky="w")
        path_entry = ttk.Entry(toolbar, textvariable=self.path_var, state="readonly")
        path_entry.grid(row=0, column=1, sticky="ew")
        ttk.Button(toolbar, text="Open Folder", command=self.open_folder).grid(
            row=0, column=2, padx=(8, 4)
        )
        ttk.Button(toolbar, text="Open File", command=self.open_file).grid(
            row=0, column=3, padx=(4, 0)
        )

        body = ttk.Panedwindow(self, orient=tk.HORIZONTAL)
        body.grid(row=1, column=0, sticky="nsew")

        left_frame = ttk.Frame(body, padding=(8, 0, 4, 8))
        left_frame.columnconfigure(0, weight=1)
        left_frame.rowconfigure(0, weight=1)

        self.tree = ttk.Treeview(
            left_frame,
            columns=("type", "size"),
            displaycolumns=("type", "size"),
            show="tree headings",
            selectmode="browse",
        )
        self.tree.heading("#0", text="Assets")
        self.tree.heading("type", text="Type")
        self.tree.heading("size", text="Size")
        self.tree.column("#0", width=360, anchor="w")
        self.tree.column("type", width=90, anchor="center")
        self.tree.column("size", width=90, anchor="e")
        self.tree.grid(row=0, column=0, sticky="nsew")

        tree_scroll_y = ttk.Scrollbar(left_frame, orient="vertical", command=self.tree.yview)
        tree_scroll_y.grid(row=0, column=1, sticky="ns")
        tree_scroll_x = ttk.Scrollbar(left_frame, orient="horizontal", command=self.tree.xview)
        tree_scroll_x.grid(row=1, column=0, sticky="ew")
        self.tree.configure(yscrollcommand=tree_scroll_y.set, xscrollcommand=tree_scroll_x.set)

        right_frame = ttk.Frame(body, padding=(4, 0, 8, 8))
        right_frame.columnconfigure(0, weight=1)
        right_frame.rowconfigure(1, weight=1)

        ttk.Label(
            right_frame,
            textvariable=self.detail_var,
            anchor="w",
            justify="left",
        ).grid(row=0, column=0, sticky="ew", pady=(0, 6))

        self.notebook = ttk.Notebook(right_frame)
        self.notebook.grid(row=1, column=0, sticky="nsew")

        hex_tab = ttk.Frame(self.notebook, padding=6)
        hex_tab.columnconfigure(0, weight=1)
        hex_tab.rowconfigure(0, weight=1)
        self.hex_text = tk.Text(
            hex_tab,
            wrap="none",
            font=("Consolas", 10),
            state="disabled",
        )
        self.hex_text.grid(row=0, column=0, sticky="nsew")
        hex_scroll_y = ttk.Scrollbar(hex_tab, orient="vertical", command=self.hex_text.yview)
        hex_scroll_y.grid(row=0, column=1, sticky="ns")
        hex_scroll_x = ttk.Scrollbar(hex_tab, orient="horizontal", command=self.hex_text.xview)
        hex_scroll_x.grid(row=1, column=0, sticky="ew")
        self.hex_text.configure(yscrollcommand=hex_scroll_y.set, xscrollcommand=hex_scroll_x.set)
        self.notebook.add(hex_tab, text="HexView")

        image_tab = ttk.Frame(self.notebook, padding=6)
        image_tab.columnconfigure(0, weight=1)
        image_tab.rowconfigure(1, weight=1)
        ttk.Label(image_tab, textvariable=self.image_info_var, anchor="w").grid(
            row=0, column=0, sticky="ew", pady=(0, 6)
        )
        image_host = ttk.Frame(image_tab)
        image_host.grid(row=1, column=0, sticky="nsew")
        image_host.columnconfigure(0, weight=1)
        image_host.rowconfigure(0, weight=1)
        self.image_canvas = tk.Canvas(image_host, background="#1E1E1E", highlightthickness=0)
        self.image_canvas.grid(row=0, column=0, sticky="nsew")
        image_scroll_y = ttk.Scrollbar(image_host, orient="vertical", command=self.image_canvas.yview)
        image_scroll_y.grid(row=0, column=1, sticky="ns")
        image_scroll_x = ttk.Scrollbar(
            image_host, orient="horizontal", command=self.image_canvas.xview
        )
        image_scroll_x.grid(row=1, column=0, sticky="ew")
        self.image_canvas.configure(
            yscrollcommand=image_scroll_y.set,
            xscrollcommand=image_scroll_x.set,
        )
        self.notebook.add(image_tab, text="ImageView")

        gl_tab = ttk.Frame(self.notebook, padding=16)
        gl_tab.columnconfigure(0, weight=1)
        gl_tab.rowconfigure(0, weight=1)
        ttk.Label(
            gl_tab,
            text="GLView placeholder\nReserved for future mesh and material rendering.",
            anchor="center",
            justify="center",
        ).grid(row=0, column=0, sticky="nsew")
        self.notebook.add(gl_tab, text="GLView")

        body.add(left_frame, weight=2)
        body.add(right_frame, weight=3)

        statusbar = ttk.Frame(self, padding=(8, 4, 8, 8))
        statusbar.grid(row=2, column=0, sticky="ew")
        statusbar.columnconfigure(0, weight=1)
        ttk.Label(statusbar, textvariable=self.status_var, anchor="w").grid(
            row=0, column=0, sticky="ew"
        )
        self.progress = ttk.Progressbar(statusbar, orient="horizontal", mode="determinate", length=240)
        self.progress.grid(row=0, column=1, sticky="e", padx=(8, 0))

        self.tree.bind("<<TreeviewSelect>>", self.on_tree_select)
        self.tree.bind("<Button-3>", self.show_context_menu)

        self.menu = tk.Menu(self, tearoff=False)
        self.menu.add_command(label="Extract File", command=self.extract_selected_file)
        self.menu.add_command(label="Extract Folder", command=self.extract_selected_folder)

    def set_status(self, message: str) -> None:
        self.status_var.set(message)
        self.update_idletasks()

    def set_progress(self, current: int, total: int) -> None:
        total = max(total, 1)
        self.progress.configure(mode="determinate", maximum=total, value=current)
        self.update_idletasks()

    def set_busy(self, active: bool) -> None:
        if active:
            self.progress.configure(mode="indeterminate")
            self.progress.start(10)
        else:
            self.progress.stop()
            self.progress.configure(mode="determinate", maximum=1, value=0)
        self.update_idletasks()

    def open_folder(self) -> None:
        selected = filedialog.askdirectory(initialdir=self.path_var.get() or ".")
        if selected:
            self.load_path(Path(selected))

    def open_file(self) -> None:
        selected = filedialog.askopenfilename(
            initialdir=self.path_var.get() or ".",
            filetypes=(("WAD files", "*.wad"), ("All files", "*.*")),
        )
        if selected:
            self.load_path(Path(selected))

    def load_path(self, source_path: Path) -> None:
        try:
            self.clear_views()
            self.item_meta.clear()
            self.payload_cache.clear()
            self.tree.delete(*self.tree.get_children())
            self.set_status(f"Opening {source_path}")
            self.set_busy(True)

            def on_progress(message: str, current: int, total: int) -> None:
                self.set_status(message)
                self.set_progress(current, total)

            root_path, wad_files = WadParser.load(source_path, progress=on_progress)
            self.root_path = root_path
            self.wad_files = wad_files
            self.path_var.set(str(root_path))
            self.populate_tree()

            total_records = sum(len(wad.records) for wad in wad_files)
            self.detail_var.set(
                f"Loaded {len(wad_files)} WAD file(s), {total_records} record(s). Select an asset."
            )
            self.set_status(f"Loaded {total_records} record(s) from {len(wad_files)} WAD file(s)")
        except Exception as exc:
            self.set_status("Load failed")
            messagebox.showerror("Load Error", str(exc))
        finally:
            self.set_busy(False)

    def populate_tree(self) -> None:
        for wad_file in self.wad_files:
            wad_item = self.tree.insert(
                "",
                "end",
                text=wad_file.path.name,
                values=("WAD", format_size(sum(record.data_size for record in wad_file.records))),
                open=False,
            )
            self.item_meta[wad_item] = {"kind": "wad", "wad_file": wad_file}

            folder_map = {"": wad_item}
            for record in sorted(wad_file.records, key=lambda item: item.name.lower()):
                parts = split_tree_parts(record.name)
                parent = wad_item
                folder_key = ""

                for folder in parts[:-1]:
                    folder_key = f"{folder_key}/{folder}" if folder_key else folder
                    if folder_key not in folder_map:
                        node = self.tree.insert(parent, "end", text=folder, values=("Folder", ""))
                        self.item_meta[node] = {
                            "kind": "folder",
                            "wad_file": wad_file,
                            "folder_key": folder_key,
                        }
                        folder_map[folder_key] = node
                    parent = folder_map[folder_key]

                record_item = self.tree.insert(
                    parent,
                    "end",
                    text=parts[-1],
                    values=(asset_type_name(record.asset_type), format_size(record.data_size)),
                )
                self.item_meta[record_item] = {"kind": "record", "record": record}

    def show_context_menu(self, event) -> None:
        item = self.tree.identify_row(event.y)
        if not item:
            return
        self.tree.selection_set(item)
        meta = self.item_meta.get(item, {})
        kind = meta.get("kind")
        self.menu.entryconfigure("Extract File", state="normal" if kind == "record" else "disabled")
        self.menu.entryconfigure("Extract Folder", state="normal" if kind in {"wad", "folder"} else "disabled")
        self.menu.tk_popup(event.x_root, event.y_root)

    def on_tree_select(self, _event) -> None:
        selection = self.tree.selection()
        if not selection:
            return
        item = selection[0]
        meta = self.item_meta.get(item)
        if not meta:
            return

        kind = meta["kind"]
        if kind == "record":
            self.show_record(meta["record"])
        elif kind == "folder":
            wad_file = meta["wad_file"]
            folder_key = meta["folder_key"]
            self.detail_var.set(f"Folder: {folder_key}\nWAD: {wad_file.path.name}")
            self.set_hex_text("Select a record to inspect its hex payload.")
            self.clear_image("Folder selected. Preview is available only for records.")
        else:
            wad_file = meta["wad_file"]
            self.detail_var.set(
                f"WAD: {wad_file.path}\nRecords: {len(wad_file.records)}\nUse the context menu to extract all assets in this WAD."
            )
            self.set_hex_text("Select a record to inspect its hex payload.")
            self.clear_image("WAD selected. Preview is available only for records.")

    def read_record_bytes(self, record: WadRecord) -> bytes:
        with record.wad_path.open("rb") as file_obj:
            file_obj.seek(record.data_offset)
            return file_obj.read(record.data_size)

    def build_payload(self, record: WadRecord) -> AssetPayload:
        cached = self.payload_cache.get(record.key)
        if cached:
            return cached

        source_bytes = self.read_record_bytes(record)
        rmid = decode_rmid(source_bytes)
        view_bytes = rmid.data if rmid else source_bytes
        output_suffix = ".rmid" if rmid else ".bin"
        hex_title = "decompressed" if rmid and rmid.was_container else "raw"

        preview = None
        image_bytes = None
        image_format = None

        if rmid and rmid.header.asset_type == RMID_TYPE_TEX:
            preview = decode_texture_preview(rmid.data)
            if preview:
                image_bytes = preview.ppm_bytes
                image_format = "PPM"
        else:
            image_bytes, image_format = detect_standard_image(view_bytes)

        payload = AssetPayload(
            source_bytes=source_bytes,
            view_bytes=view_bytes,
            rmid=rmid,
            hex_title=hex_title,
            preview=preview,
            image_bytes=image_bytes,
            image_format=image_format,
            output_suffix=output_suffix,
        )
        self.payload_cache[record.key] = payload
        return payload

    def show_record(self, record: WadRecord) -> None:
        try:
            self.set_status(f"Reading {record.name}")
            self.set_busy(True)
            payload = self.build_payload(record)
            detail_lines = [
                f"Name: {record.name}",
                f"WAD: {record.wad_path.name}",
                f"ID: 0x{record.asset_id:08X}",
                f"Type: {asset_type_name(record.asset_type)}",
                f"Stored Size: {format_size(record.data_size)}",
                f"Modified: {format_timestamp(record.modified_time)}",
            ]
            if payload.rmid:
                detail_lines.append(
                    "RMID: "
                    f"type={asset_type_name(payload.rmid.header.asset_type)} "
                    f"refs={payload.rmid.header.num_references} "
                    f"mode={payload.hex_title} "
                    f"note={payload.rmid.note}"
                )
                detail_lines.append(f"Hex Source: {format_size(len(payload.view_bytes))}")
            else:
                detail_lines.append("RMID: not detected")
            self.detail_var.set("\n".join(detail_lines))

            self.set_hex_text(hexdump(payload.view_bytes))

            if payload.preview:
                self.show_image(
                    payload.image_bytes,
                    payload.image_format,
                    f"{payload.preview.width}x{payload.preview.height} {payload.preview.description}",
                )
            elif payload.image_bytes:
                self.show_image(
                    payload.image_bytes,
                    payload.image_format,
                    f"Embedded image preview ({payload.image_format or 'PPM/PGM'})",
                )
            else:
                note = (
                    payload.rmid.note
                    if payload.rmid
                    else "Preview is only available for supported image payloads."
                )
                self.clear_image(f"No preview available. {note}")

            self.set_status(f"Loaded {record.name}")
        except Exception as exc:
            self.set_status("Read failed")
            self.set_hex_text("")
            self.clear_image("Preview failed.")
            messagebox.showerror("Asset Error", str(exc))
        finally:
            self.set_busy(False)

    def set_hex_text(self, content: str) -> None:
        self.hex_text.configure(state="normal")
        self.hex_text.delete("1.0", "end")
        self.hex_text.insert("1.0", content)
        self.hex_text.configure(state="disabled")

    def clear_views(self) -> None:
        self.set_hex_text("")
        self.clear_image("No preview available.")

    def clear_image(self, message: str) -> None:
        self.image_canvas.delete("all")
        self.image_canvas.create_text(
            24,
            24,
            anchor="nw",
            fill="#D8D8D8",
            text=message,
            width=520,
        )
        self.image_canvas.configure(scrollregion=(0, 0, 640, 480))
        self.image_info_var.set(message)
        self.active_image = None

    def show_image(self, image_bytes: Optional[bytes], image_format: Optional[str], description: str) -> None:
        if not image_bytes:
            self.clear_image("No preview data.")
            return

        try:
            image = photo_image_from_bytes(image_bytes, image_format)
        except tk.TclError:
            self.clear_image("Image preview could not be created by Tk.")
            return

        self.active_image = image
        self.image_canvas.delete("all")
        self.image_canvas.create_image(0, 0, anchor="nw", image=image)
        self.image_canvas.configure(scrollregion=(0, 0, image.width(), image.height()))
        self.image_info_var.set(description)

    def get_selected_item(self) -> Optional[str]:
        selection = self.tree.selection()
        return selection[0] if selection else None

    def collect_records(self, item: str) -> list[WadRecord]:
        meta = self.item_meta.get(item, {})
        if meta.get("kind") == "record":
            return [meta["record"]]

        records: list[WadRecord] = []

        def walk(node: str) -> None:
            node_meta = self.item_meta.get(node, {})
            if node_meta.get("kind") == "record":
                records.append(node_meta["record"])
            for child in self.tree.get_children(node):
                walk(child)

        walk(item)
        records.sort(key=lambda record: record.name.lower())
        return records

    def cache_output_path(self, record: WadRecord, suffix: str) -> Path:
        relative = ensure_output_name(record.name, suffix)
        return Path(".cache") / sanitize_component(record.wad_name) / relative

    def extract_selected_file(self) -> None:
        item = self.get_selected_item()
        if not item:
            return
        meta = self.item_meta.get(item, {})
        if meta.get("kind") != "record":
            return
        self.extract_records([meta["record"]], scope_label="file")

    def extract_selected_folder(self) -> None:
        item = self.get_selected_item()
        if not item:
            return
        records = self.collect_records(item)
        if not records:
            return
        self.extract_records(records, scope_label="folder")

    def extract_records(self, records: list[WadRecord], scope_label: str) -> None:
        try:
            self.set_status(f"Extracting {len(records)} {scope_label} record(s)")
            self.set_progress(0, len(records))
            for index, record in enumerate(records, start=1):
                payload = self.build_payload(record)
                output_path = self.cache_output_path(record, payload.output_suffix)
                output_path.parent.mkdir(parents=True, exist_ok=True)
                output_path.write_bytes(payload.view_bytes)
                self.set_status(f"Extracted {record.name} -> {output_path}")
                self.set_progress(index, len(records))
            self.set_status(f"Extraction complete: {len(records)} record(s) into .cache")
        except Exception as exc:
            self.set_status("Extraction failed")
            messagebox.showerror("Extraction Error", str(exc))


def main() -> None:
    initial_path = Path(sys.argv[1]) if len(sys.argv) > 1 else None
    app = WadExplorerApp(initial_path=initial_path)
    app.mainloop()


if __name__ == "__main__":
    main()
