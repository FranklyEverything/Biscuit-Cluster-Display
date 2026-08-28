from pathlib import Path
from PIL import Image, ImageSequence

SOURCE = Path(__file__).resolve().parents[1] / "assets" / "biscuit-logo-turntable.gif"
OUTPUT = Path(__file__).resolve().parents[1] / "src" / "logo_frames.h"
SIZE = 150


def pack_xbitmap(frame: Image.Image) -> bytes:
    rgba = frame.convert("RGBA")
    rgba.thumbnail((SIZE, SIZE), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    canvas.alpha_composite(rgba, ((SIZE - rgba.width) // 2, (SIZE - rgba.height) // 2))

    packed = bytearray()
    for y in range(SIZE):
        for x0 in range(0, SIZE, 8):
            value = 0
            for bit in range(8):
                x = x0 + bit
                if x >= SIZE:
                    continue
                r, g, b, a = canvas.getpixel((x, y))
                if a > 64 and (r + g + b) > 300:
                    value |= 1 << bit
            packed.append(value)
    return bytes(packed)


with Image.open(SOURCE) as animation:
    frames = [pack_xbitmap(frame.copy()) for frame in ImageSequence.Iterator(animation)]

lines = [
    "#pragma once",
    "#include <Arduino.h>",
    f"static constexpr uint16_t LOGO_WIDTH = {SIZE};",
    f"static constexpr uint16_t LOGO_HEIGHT = {SIZE};",
    f"static constexpr uint8_t LOGO_FRAME_COUNT = {len(frames)};",
    "static constexpr uint16_t LOGO_FRAME_MS = 120;",
]

for index, data in enumerate(frames):
    lines.append(f"static const uint8_t logo_frame_{index}[] PROGMEM = {{")
    for offset in range(0, len(data), 20):
        chunk = data[offset:offset + 20]
        lines.append("  " + ", ".join(f"0x{value:02X}" for value in chunk) + ",")
    lines.append("};")

lines.append("static const uint8_t* const LOGO_FRAMES[] PROGMEM = {")
for index in range(len(frames)):
    lines.append(f"  logo_frame_{index},")
lines.append("};")

OUTPUT.parent.mkdir(parents=True, exist_ok=True)
OUTPUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"Generated {len(frames)} frames at {SIZE}x{SIZE}: {OUTPUT}")
