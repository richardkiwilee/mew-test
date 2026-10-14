# -*- coding: utf-8 -*-
"""把 wiki-asset 下载的图片转换为 Godot 可用资产。

- GIF/APNG：拆帧保存为 <name>_fNNN.png（Godot AnimatedSprite3D 用）
- 普通 PNG：直接复制（作为静态图标/立绘）
- 输出 manifest.json：name -> {frames, fps, size, source}

用法:  python tools/convert_sprites.py [--src wiki-asset/sprites] [--out godot/assets/sprites]
"""

from __future__ import annotations

import argparse
import json
import os
import sys

from PIL import Image


def convert_gif(src: str, out_dir: str, stem: str, manifest: dict) -> None:
    img = Image.open(src)
    frames = []
    fps = 8
    try:
        duration = img.info.get("duration", 125)  # ms/帧
        if duration:
            fps = max(1, round(1000 / duration))
    except Exception:
        pass
    i = 0
    while True:
        try:
            img.seek(i)
            frame = img.convert("RGBA")
            w, h = frame.size
            # 限制单帧尺寸（战斗棋子无需超高分辨率）
            max_side = 256
            if max(w, h) > max_side:
                scale = max_side / max(w, h)
                frame = frame.resize((max(1, int(w * scale)), max(1, int(h * scale))),
                                     Image.Resampling.LANCZOS)
            out_path = os.path.join(out_dir, f"{stem}_f{i:03d}.png")
            frame.save(out_path, "PNG")
            frames.append(os.path.basename(out_path))
            i += 1
        except EOFError:
            break
    manifest[stem] = {
        "frames": frames,
        "fps": fps,
        "size": list(img.size),
        "source": os.path.basename(src),
        "animated": len(frames) > 1,
    }
    print(f"[convert] {os.path.basename(src)} -> {len(frames)} frames @ {fps}fps")


def copy_static(src: str, out_dir: str, stem: str, manifest: dict) -> None:
    img = Image.open(src).convert("RGBA")
    w, h = img.size
    max_side = 256
    if max(w, h) > max_side:
        scale = max_side / max(w, h)
        img = img.resize((max(1, int(w * scale)), max(1, int(h * scale))),
                         Image.Resampling.LANCZOS)
    out_path = os.path.join(out_dir, f"{stem}.png")
    img.save(out_path, "PNG")
    manifest[stem] = {
        "frames": [os.path.basename(out_path)],
        "fps": 1,
        "size": list(img.size),
        "source": os.path.basename(src),
        "animated": False,
    }
    print(f"[convert] {os.path.basename(src)} -> static {img.size}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--src", default="wiki-asset/sprites")
    parser.add_argument("--out", default="godot/assets/sprites")
    args = parser.parse_args()

    if not os.path.isdir(args.src):
        print(f"[convert] source dir not found: {args.src}")
        return 1
    os.makedirs(args.out, exist_ok=True)

    manifest: dict = {}
    seen_stems: dict = {}
    for fname in sorted(os.listdir(args.src)):
        path = os.path.join(args.src, fname)
        if not os.path.isfile(path):
            continue
        stem = os.path.splitext(fname)[0].lower()
        stem = "".join(c if c.isalnum() else "_" for c in stem)
        if stem in seen_stems:  # 重名去重
            seen_stems[stem] += 1
            stem = f"{stem}_{seen_stems[stem]}"
        else:
            seen_stems[stem] = 0
        ext = os.path.splitext(fname)[1].lower()
        try:
            if ext == ".gif":
                convert_gif(path, args.out, stem, manifest)
            elif ext in (".png", ".jpg", ".jpeg", ".webp"):
                copy_static(path, args.out, stem, manifest)
        except Exception as e:  # 单个文件失败不阻断整体
            print(f"[convert] SKIP {fname}: {e}")

    with open(os.path.join(args.out, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=1)
    print(f"[convert] done: {len(manifest)} sprites -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
