#!/usr/bin/env python3
"""Embed the vendored, unmodified OFL Liberation Serif font for portable menus."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
font = (root / "third_party/liberation/LiberationSerif-Regular.ttf").read_bytes()
out = root / "third_party/liberation/font_data.h"
lines = ["// Byte-for-byte embedding of LiberationSerif-Regular.ttf 2.1.5; SIL OFL-1.1.\n"
         "#pragma once\nnamespace a2::menu_font_data {\ninline constexpr unsigned char bytes[] = {\n"]
for i in range(0, len(font), 24):
    lines.append("  " + ",".join(str(n) for n in font[i:i + 24]) + ",\n")
lines.append("};\n}\n")
out.write_text("".join(lines))
print(f"Embedded {len(font)} unmodified font bytes into {out}")
