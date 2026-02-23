"""Lightweight builder for SOE/SWG IFF-style binary files."""

from __future__ import annotations

import base64
import json
import struct
from dataclasses import dataclass
from typing import Iterable, List, Union


class IffDefinitionError(ValueError):
    """Raised when an IFF definition cannot be parsed."""


@dataclass
class IffNode:
    tag: bytes

    def to_bytes(self) -> bytes:
        raise NotImplementedError

    def describe(self, indent: int = 0) -> str:
        raise NotImplementedError


@dataclass
class IffChunk(IffNode):
    data: bytes

    def to_bytes(self) -> bytes:
        size = len(self.data)
        payload = _pad_even(self.data)
        return self.tag + struct.pack(">I", size) + payload

    def describe(self, indent: int = 0) -> str:
        tag = self.tag.decode("ascii")
        return " " * indent + f"CHUNK {tag} ({len(self.data)} bytes)"


@dataclass
class IffForm(IffNode):
    children: List[IffNode]

    def to_bytes(self) -> bytes:
        children_bytes = b"".join(child.to_bytes() for child in self.children)
        size = 4 + len(children_bytes)
        payload = _pad_even(children_bytes)
        return b"FORM" + struct.pack(">I", size) + self.tag + payload

    def describe(self, indent: int = 0) -> str:
        tag = self.tag.decode("ascii")
        header = " " * indent + f"FORM {tag}"
        if not self.children:
            return header + " (empty)"
        child_lines = [child.describe(indent + 2) for child in self.children]
        return "\n".join([header] + child_lines)


class IffBuilder:
    """Create IFF binary payloads from structured definitions.

    The format is intentionally simple compared to the full SIE GUI: a JSON
    definition describes a tree of FORM and CHUNK entries, and the builder
    produces the corresponding big-endian IFF binary.
    """

    def __init__(self, root: IffNode):
        self.root = root

    @classmethod
    def from_definition(cls, definition: dict) -> "IffBuilder":
        node = _parse_node(definition)
        return cls(node)

    def build_bytes(self) -> bytes:
        return self.root.to_bytes()

    def write(self, path) -> None:
        path.write_bytes(self.build_bytes())

    def describe(self) -> str:
        return self.root.describe()


def _parse_node(definition: dict) -> IffNode:
    if not isinstance(definition, dict):
        raise IffDefinitionError("IFF definition must be a dictionary")

    if "chunk" in definition:
        tag = _parse_tag(definition["chunk"])
        data_value = definition.get("data", b"")
        encoding = definition.get("encoding")
        data = _coerce_data(data_value, encoding)
        return IffChunk(tag=tag, data=data)

    if "form" in definition:
        tag = _parse_tag(definition["form"])
        children_def = definition.get("children", [])
        if isinstance(children_def, (str, bytes)) or not isinstance(
            children_def, Iterable
        ):
            raise IffDefinitionError("FORM children must be an iterable of nodes")
        children = [_parse_node(child) for child in children_def]
        return IffForm(tag=tag, children=children)

    raise IffDefinitionError("Definition must contain either 'chunk' or 'form'")


def _parse_tag(raw: Union[str, bytes]) -> bytes:
    if isinstance(raw, bytes):
        tag = raw
    elif isinstance(raw, str):
        tag = raw.encode("ascii")
    else:
        raise IffDefinitionError("Tag values must be text or bytes")

    if len(tag) != 4:
        raise IffDefinitionError(f"IFF tags must be 4 characters long: {tag!r}")
    return tag


def _coerce_data(value, encoding: str | None) -> bytes:
    if isinstance(value, bytes):
        data = value
    elif isinstance(value, str):
        data = _encode_string(value, encoding)
    elif isinstance(value, list):
        if all(isinstance(item, int) and 0 <= item <= 255 for item in value):
            data = bytes(value)
        else:
            data = json.dumps(value, indent=2).encode("utf-8")
    elif isinstance(value, (dict, int, float)):
        data = json.dumps(value, indent=2).encode("utf-8")
    else:
        raise IffDefinitionError(f"Unsupported data type for chunk payload: {type(value)}")

    return data


def _encode_string(value: str, encoding: str | None) -> bytes:
    if encoding is None or encoding == "text":
        return value.encode("utf-8")
    if encoding == "hex":
        try:
            return bytes.fromhex(value)
        except ValueError as exc:  # pragma: no cover - defensive
            raise IffDefinitionError(f"Invalid hex payload: {exc}") from exc
    if encoding == "base64":
        try:
            return base64.b64decode(value)
        except Exception as exc:  # pragma: no cover - defensive
            raise IffDefinitionError(f"Invalid base64 payload: {exc}") from exc

    raise IffDefinitionError(f"Unknown encoding '{encoding}' for string payloads")


def _pad_even(payload: bytes) -> bytes:
    if len(payload) % 2 == 0:
        return payload
    return payload + b"\x00"


__all__ = ["IffBuilder", "IffChunk", "IffDefinitionError", "IffForm"]
