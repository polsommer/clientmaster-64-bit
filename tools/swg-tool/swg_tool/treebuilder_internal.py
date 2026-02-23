"""Internal TreeFile builder implementation."""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import os
from pathlib import Path
import struct
from typing import Optional, Sequence
import zlib

from .response import ResponseFileBuilder


CRC_INIT = 0xFFFFFFFF
CRC_TABLE = (
    0x00000000,
    0x04C11DB7,
    0x09823B6E,
    0x0D4326D9,
    0x130476DC,
    0x17C56B6B,
    0x1A864DB2,
    0x1E475005,
    0x2608EDB8,
    0x22C9F00F,
    0x2F8AD6D6,
    0x2B4BCB61,
    0x350C9B64,
    0x31CD86D3,
    0x3C8EA00A,
    0x384FBDBD,
    0x4C11DB70,
    0x48D0C6C7,
    0x4593E01E,
    0x4152FDA9,
    0x5F15ADAC,
    0x5BD4B01B,
    0x569796C2,
    0x52568B75,
    0x6A1936C8,
    0x6ED82B7F,
    0x639B0DA6,
    0x675A1011,
    0x791D4014,
    0x7DDC5DA3,
    0x709F7B7A,
    0x745E66CD,
    0x9823B6E0,
    0x9CE2AB57,
    0x91A18D8E,
    0x95609039,
    0x8B27C03C,
    0x8FE6DD8B,
    0x82A5FB52,
    0x8664E6E5,
    0xBE2B5B58,
    0xBAEA46EF,
    0xB7A96036,
    0xB3687D81,
    0xAD2F2D84,
    0xA9EE3033,
    0xA4AD16EA,
    0xA06C0B5D,
    0xD4326D90,
    0xD0F37027,
    0xDDB056FE,
    0xD9714B49,
    0xC7361B4C,
    0xC3F706FB,
    0xCEB42022,
    0xCA753D95,
    0xF23A8028,
    0xF6FB9D9F,
    0xFBB8BB46,
    0xFF79A6F1,
    0xE13EF6F4,
    0xE5FFEB43,
    0xE8BCCD9A,
    0xEC7DD02D,
    0x34867077,
    0x30476DC0,
    0x3D044B19,
    0x39C556AE,
    0x278206AB,
    0x23431B1C,
    0x2E003DC5,
    0x2AC12072,
    0x128E9DCF,
    0x164F8078,
    0x1B0CA6A1,
    0x1FCDBB16,
    0x018AEB13,
    0x054BF6A4,
    0x0808D07D,
    0x0CC9CDCA,
    0x7897AB07,
    0x7C56B6B0,
    0x71159069,
    0x75D48DDE,
    0x6B93DDDB,
    0x6F52C06C,
    0x6211E6B5,
    0x66D0FB02,
    0x5E9F46BF,
    0x5A5E5B08,
    0x571D7DD1,
    0x53DC6066,
    0x4D9B3063,
    0x495A2DD4,
    0x44190B0D,
    0x40D816BA,
    0xACA5C697,
    0xA864DB20,
    0xA527FDF9,
    0xA1E6E04E,
    0xBFA1B04B,
    0xBB60ADFC,
    0xB6238B25,
    0xB2E29692,
    0x8AAD2B2F,
    0x8E6C3698,
    0x832F1041,
    0x87EE0DF6,
    0x99A95DF3,
    0x9D684044,
    0x902B669D,
    0x94EA7B2A,
    0xE0B41DE7,
    0xE4750050,
    0xE9362689,
    0xEDF73B3E,
    0xF3B06B3B,
    0xF771768C,
    0xFA325055,
    0xFEF34DE2,
    0xC6BCF05F,
    0xC27DEDE8,
    0xCF3ECB31,
    0xCBFFD686,
    0xD5B88683,
    0xD1799B34,
    0xDC3ABDED,
    0xD8FBA05A,
    0x690CE0EE,
    0x6DCDFD59,
    0x608EDB80,
    0x644FC637,
    0x7A089632,
    0x7EC98B85,
    0x738AAD5C,
    0x774BB0EB,
    0x4F040D56,
    0x4BC510E1,
    0x46863638,
    0x42472B8F,
    0x5C007B8A,
    0x58C1663D,
    0x558240E4,
    0x51435D53,
    0x251D3B9E,
    0x21DC2629,
    0x2C9F00F0,
    0x285E1D47,
    0x36194D42,
    0x32D850F5,
    0x3F9B762C,
    0x3B5A6B9B,
    0x0315D626,
    0x07D4CB91,
    0x0A97ED48,
    0x0E56F0FF,
    0x1011A0FA,
    0x14D0BD4D,
    0x19939B94,
    0x1D528623,
    0xF12F560E,
    0xF5EE4BB9,
    0xF8AD6D60,
    0xFC6C70D7,
    0xE22B20D2,
    0xE6EA3D65,
    0xEBA91BBC,
    0xEF68060B,
    0xD727BBB6,
    0xD3E6A601,
    0xDEA580D8,
    0xDA649D6F,
    0xC423CD6A,
    0xC0E2D0DD,
    0xCDA1F604,
    0xC960EBB3,
    0xBD3E8D7E,
    0xB9FF90C9,
    0xB4BCB610,
    0xB07DABA7,
    0xAE3AFBA2,
    0xAAFBE615,
    0xA7B8C0CC,
    0xA379DD7B,
    0x9B3660C6,
    0x9FF77D71,
    0x92B45BA8,
    0x9675461F,
    0x8832161A,
    0x8CF30BAD,
    0x81B02D74,
    0x857130C3,
    0x5D8A9099,
    0x594B8D2E,
    0x5408ABF7,
    0x50C9B640,
    0x4E8EE645,
    0x4A4FFBF2,
    0x470CDD2B,
    0x43CDC09C,
    0x7B827D21,
    0x7F436096,
    0x7200464F,
    0x76C15BF8,
    0x68860BFD,
    0x6C47164A,
    0x61043093,
    0x65C52D24,
    0x119B4BE9,
    0x155A565E,
    0x18197087,
    0x1CD86D30,
    0x029F3D35,
    0x065E2082,
    0x0B1D065B,
    0x0FDC1BEC,
    0x3793A651,
    0x3352BBE6,
    0x3E119D3F,
    0x3AD08088,
    0x2497D08D,
    0x2056CD3A,
    0x2D15EBE3,
    0x29D4F654,
    0xC5A92679,
    0xC1683BCE,
    0xCC2B1D17,
    0xC8EA00A0,
    0xD6AD50A5,
    0xD26C4D12,
    0xDF2F6BCB,
    0xDBEE767C,
    0xE3A1CBC1,
    0xE760D676,
    0xEA23F0AF,
    0xEEE2ED18,
    0xF0A5BD1D,
    0xF464A0AA,
    0xF9278673,
    0xFDE69BC4,
    0x89B8FD09,
    0x8D79E0BE,
    0x803AC667,
    0x84FBDBD0,
    0x9ABC8BD5,
    0x9E7D9662,
    0x933EB0BB,
    0x97FFAD0C,
    0xAFB010B1,
    0xAB710D06,
    0xA6322BDF,
    0xA2F33668,
    0xBCB4666D,
    0xB8757BDA,
    0xB5365D03,
    0xB1F740B4,
)

CT_NONE = 0
CT_ZLIB = 2
CT_ZLIB_BEST = 3

TAG_TREE = 0x54524545
TAG_TRES = 0x54524553
TAG_TRESX = 0x54525358
TAG_0005 = int.from_bytes(b"0005", "big")

HEADER_STRUCT = struct.Struct("<9I")
TOC_STRUCT = struct.Struct("<Iiiiii")
I32_MIN = -(2**31)
I32_MAX = 2**31 - 1


@dataclass
class InternalBuildReport:
    command: list[str]
    stdout: str
    stderr: str
    returncode: int


@dataclass
class InternalFileEntry:
    disk_path: Optional[Path]
    tree_name: str
    crc: int
    uncompressed: bool
    deleted: bool
    offset: int = 0
    length: int = 0
    compressor: int = CT_NONE
    compressed_length: int = 0
    md5: bytes = b"\x00" * 16


class InternalTreeFileBuilderError(RuntimeError):
    """Raised when the internal tree builder cannot create an archive."""


class InternalTreeFileBuilder:
    """Builds .tre/.tres/.tresx archives without external executables."""

    def __init__(self) -> None:
        self._capabilities = {
            "supports_passphrase": True,
            "supports_encrypt": True,
            "supports_no_encrypt": True,
            "supports_quiet": True,
            "supports_dry_run": True,
            "supports_gpu": False,
        }

    @property
    def capabilities(self) -> dict[str, bool]:
        return dict(self._capabilities)

    def build(
        self,
        *,
        response_file: Optional[Path] = None,
        sources: Optional[Sequence[Path | str]] = None,
        output_file: Path,
        no_toc_compression: bool,
        no_file_compression: bool,
        dry_run: bool,
        quiet: bool,
        force_encrypt: bool,
        disable_encrypt: bool,
        passphrase: Optional[str],
        stdout_callback,
        stderr_callback,
    ) -> InternalBuildReport:
        if response_file is None and not sources:
            raise InternalTreeFileBuilderError(
                "A response file or source list is required to build a tree file."
            )

        output_type = _output_type_from_path(output_file)
        encrypt_content = _should_encrypt(
            output_type=output_type,
            force_encrypt=force_encrypt,
            disable_encrypt=disable_encrypt,
        )

        entries = self._load_entries(response_file=response_file, sources=sources)
        if dry_run:
            return InternalBuildReport(
                command=["internal-treebuilder", "--noCreate", str(output_file)],
                stdout="Scan complete. No errors.\n",
                stderr="",
                returncode=0,
            )

        if encrypt_content:
            encryption_key = _derive_key(passphrase or "")
        else:
            encryption_key = None

        stdout_chunks: list[str] = []
        stderr_chunks: list[str] = []

        def _emit_stdout(message: str) -> None:
            stdout_chunks.append(message)
            if stdout_callback is not None:
                stdout_callback(message)

        def _emit_stderr(message: str) -> None:
            stderr_chunks.append(message)
            if stderr_callback is not None:
                stderr_callback(message)

        active_entries = [entry for entry in entries if not entry.deleted]
        if not active_entries:
            raise InternalTreeFileBuilderError(
                "No valid entries available to build the tree file."
            )
        if not quiet:
            _emit_stdout(
                f"Building tree file with {len(active_entries)} entries.\n"
            )

        toc_order = sorted(
            active_entries, key=lambda entry: (entry.crc, entry.tree_name)
        )

        output_file.parent.mkdir(parents=True, exist_ok=True)
        try:
            with output_file.open("wb+") as handle:
                encryption_offset = 0

                def _write(data: bytes, encrypt: bool) -> None:
                    nonlocal encryption_offset
                    if encrypt and encryption_key is not None:
                        data = _transform_buffer(data, encryption_key, encryption_offset)
                        encryption_offset += len(data)
                    handle.write(data)

                header = HEADER_STRUCT.pack(
                    _token_for_type(output_type),
                    TAG_0005,
                    len(active_entries),
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                )
                handle.write(header)
                if encrypt_content:
                    encryption_offset = 0

                data_size = 0
                for entry in active_entries:
                    assert entry.disk_path is not None
                    entry.offset = handle.tell()
                    payload = entry.disk_path.read_bytes()
                    entry.length = len(payload)
                    compressed, compressor = _compress_payload(
                        payload,
                        output_type=output_type,
                        disable=no_file_compression or entry.uncompressed,
                    )
                    entry.compressor = compressor
                    if compressor == CT_NONE:
                        entry.compressed_length = 0
                        entry.md5 = hashlib.md5(payload).digest()
                        data = payload
                    else:
                        entry.compressed_length = len(compressed)
                        entry.md5 = hashlib.md5(compressed).digest()
                        data = compressed

                    current_offset = handle.tell()
                    if current_offset > I32_MAX:
                        raise InternalTreeFileBuilderError(
                            "Tree file exceeds 32-bit limits for offset on "
                            f"{entry.tree_name}: {current_offset}. Split the archive "
                            f"or remove content to stay under {I32_MAX} bytes."
                        )
                    if current_offset + len(data) > I32_MAX:
                        raise InternalTreeFileBuilderError(
                            "Tree file exceeds 32-bit limits for offset on "
                            f"{entry.tree_name}: {current_offset + len(data)}. "
                            "Split the archive or remove content to stay under "
                            f"{I32_MAX} bytes."
                        )

                    _write(data, encrypt_content)
                    data_size += len(data)

                toc_offset = HEADER_STRUCT.size + data_size

                toc_bytes, toc_compressor = _build_toc(toc_order)
                toc_data, toc_compressor = _compress_payload(
                    toc_bytes,
                    output_type=output_type,
                    disable=no_toc_compression,
                )
                _write(toc_data, encrypt_content)

                name_block, uncomp_name_size = _build_name_block(toc_order)
                name_block_data, block_compressor = _compress_payload(
                    name_block,
                    output_type=output_type,
                    disable=no_toc_compression,
                )
                _write(name_block_data, encrypt_content)

                md5_block = b"".join(entry.md5 for entry in toc_order)
                _write(md5_block, encrypt_content)

                header = HEADER_STRUCT.pack(
                    _token_for_type(output_type),
                    TAG_0005,
                    len(active_entries),
                    toc_offset,
                    toc_compressor,
                    len(toc_data),
                    block_compressor,
                    len(name_block_data),
                    uncomp_name_size,
                )
                handle.seek(0)
                handle.write(header)
        except OSError as exc:
            _emit_stderr(str(exc))
            raise InternalTreeFileBuilderError(str(exc)) from exc

        return InternalBuildReport(
            command=["internal-treebuilder", str(output_file)],
            stdout="".join(stdout_chunks),
            stderr="".join(stderr_chunks),
            returncode=0,
        )

    def _load_entries(
        self,
        *,
        response_file: Optional[Path],
        sources: Optional[Sequence[Path | str]],
    ) -> list[InternalFileEntry]:
        if response_file is not None:
            if not response_file.exists():
                raise InternalTreeFileBuilderError(
                    f"Response file not found: {response_file}"
                )
            return _parse_response_file(response_file)

        builder = ResponseFileBuilder()
        entries = builder.build_entries(sources or [])
        return [_entry_from_response(entry.entry, entry.source) for entry in entries]


def _output_type_from_path(path: Path) -> str:
    suffix = path.suffix.lower().lstrip(".")
    if suffix in {"tre", "tres", "tresx"}:
        return suffix
    return "tre"


def _token_for_type(output_type: str) -> int:
    if output_type == "tresx":
        return TAG_TRESX
    if output_type == "tres":
        return TAG_TRES
    return TAG_TREE


def _should_encrypt(*, output_type: str, force_encrypt: bool, disable_encrypt: bool) -> bool:
    if force_encrypt:
        return True
    if disable_encrypt:
        return False
    return output_type in {"tres", "tresx"}


def _derive_key(passphrase: str) -> bytes:
    return hashlib.md5(passphrase.encode("utf-8")).digest()


def _transform_buffer(data: bytes, key: bytes, offset: int) -> bytes:
    output = bytearray(data)
    key_length = len(key)
    for index, value in enumerate(output):
        key_index = (offset + index) % key_length
        output[index] = value ^ key[key_index]
    return bytes(output)


def _crc_string(value: str) -> int:
    if not value:
        return 0
    crc = CRC_INIT
    for char in value.encode("utf-8"):
        crc = CRC_TABLE[((crc >> 24) ^ char) & 0xFF] ^ ((crc << 8) & 0xFFFFFFFF)
    return crc ^ CRC_INIT


def _parse_response_file(response_file: Path) -> list[InternalFileEntry]:
    entries: list[InternalFileEntry] = []
    seen: dict[str, InternalFileEntry] = {}
    with response_file.open("r", encoding="utf-8", errors="ignore") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line or "@" not in line:
                continue

            line = _normalize_response_line(line)
            if "@" not in line:
                continue

            tree_name, disk_entry, uncompressed = _split_response_line(line)
            entry = _entry_from_response(tree_name, disk_entry, uncompressed)
            existing = seen.get(entry.tree_name)
            if existing:
                if existing.disk_path != entry.disk_path:
                    raise InternalTreeFileBuilderError(
                        f"Duplicate tree entry detected for {entry.tree_name}."
                    )
                continue

            seen[entry.tree_name] = entry
            entries.append(entry)

    if not entries:
        raise InternalTreeFileBuilderError("No valid response entries were found.")

    return entries


def _normalize_response_line(line: str) -> str:
    if "TF::open" in line:
        start = line.find("TF::open")
        start = line.find("(", start)
        end = line.find(",", start)
        if start != -1 and end != -1:
            return line[start + 1 : end].strip()
    return line


def _split_response_line(line: str) -> tuple[str, str, bool]:
    left, right = line.split("@", 1)
    tree_name = left.strip()
    disk_entry = right.strip()
    uncompressed = False
    if disk_entry.startswith("u") and (len(disk_entry) == 1 or disk_entry[1].isspace()):
        uncompressed = True
        disk_entry = disk_entry[1:].strip()
    if not tree_name:
        tree_name = disk_entry
    return tree_name, disk_entry, uncompressed


def _entry_from_response(
    tree_name: str, disk_entry: Path | str, uncompressed: bool = False
) -> InternalFileEntry:
    normalized_name = tree_name.replace("\\", "/").lower().strip()
    deleted = str(disk_entry) == "deleted"
    disk_path: Optional[Path]
    if deleted:
        disk_path = None
    else:
        disk_path = Path(disk_entry).expanduser().resolve()
        if not disk_path.exists() or not disk_path.is_file():
            deleted = True
        else:
            size = os.path.getsize(disk_path)
            if size <= 0 or size > (2**31 - 1):
                deleted = True

    return InternalFileEntry(
        disk_path=disk_path,
        tree_name=normalized_name,
        crc=_crc_string(normalized_name),
        uncompressed=uncompressed,
        deleted=deleted,
    )


def _compress_payload(
    data: bytes,
    *,
    output_type: str,
    disable: bool,
) -> tuple[bytes, int]:
    if disable or len(data) <= 1024:
        return data, CT_NONE

    compressors = [(CT_ZLIB, 6)]
    if output_type == "tresx":
        compressors.append((CT_ZLIB_BEST, 9))

    smallest_data = data
    smallest_compressor = CT_NONE

    for compressor, level in compressors:
        compressed = zlib.compress(data, level)
        if len(compressed) < len(smallest_data):
            smallest_data = compressed
            smallest_compressor = compressor

    return smallest_data, smallest_compressor


def _build_toc(
    toc_order: Sequence[InternalFileEntry],
) -> tuple[bytes, int]:
    offset = 0
    entries = []
    for entry in toc_order:
        for label, value in (
            ("length", entry.length),
            ("offset", entry.offset),
            ("compressor", entry.compressor),
            ("compressed_length", entry.compressed_length),
            ("name_offset", offset),
        ):
            if value < I32_MIN or value > I32_MAX:
                raise InternalTreeFileBuilderError(
                    "Tree file exceeds 32-bit limits for "
                    f"{label} on {entry.tree_name}: {value}. "
                    "Split the archive or remove content to stay under "
                    f"{I32_MAX} bytes."
                )
        entries.append(
            TOC_STRUCT.pack(
                entry.crc,
                entry.length,
                entry.offset,
                entry.compressor,
                entry.compressed_length,
                offset,
            )
        )
        offset += len(entry.tree_name.encode("utf-8")) + 1
    return b"".join(entries), CT_NONE


def _build_name_block(entries: Sequence[InternalFileEntry]) -> tuple[bytes, int]:
    parts: list[bytes] = []
    total = 0
    for entry in entries:
        name_bytes = entry.tree_name.encode("utf-8") + b"\x00"
        parts.append(name_bytes)
        total += len(name_bytes)
    return b"".join(parts), total


__all__ = [
    "InternalTreeFileBuilder",
    "InternalTreeFileBuilderError",
    "InternalBuildReport",
]
