# -*- coding: utf-8 -*-
"""准备 Godot 展示层素材（由 wiki-asset 派生，可重复执行）：

1. 敌人动画：ENEMY_<名字>_{Idle,Attack,Death}.gif → 帧序列（AnimatedSprite3D 用）
2. 猫职业动画：CLASS_*_{Idle,Start,Victory}.gif → 帧序列
3. 敌人静态立绘（level-editor PNG，兜底）
4. 图标：技能 / 状态 / 地形 → godot/assets/icons/*.svg（Godot 导入）

用法:  python tools/prepare_godot_assets.py
"""

from __future__ import annotations

import json
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(__file__))
import convert_sprites  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "wiki-asset", "sprites")
OUT_SPRITES = os.path.join(ROOT, "godot", "assets", "sprites")
OUT_ICONS = os.path.join(ROOT, "godot", "assets", "icons")

# demo 敌人（unit def id → wiki ENEMY_ 名字；动画 Idle/Attack/Death 视存在与否）
ENEMY_ANIMS = {
    2: "Lil'_Rat",       # 老鼠
    3: "Bat",            # 蝙蝠
    4: "Lumpy",          # 大块头（原暴徒）
    5: "Cat_Caller",     # 唤猫者（远程施法，原火妖）
    6: "Glass_Spitter",  # 玻璃喷吐者
}

# 技能 ID → wiki 图标
ABILITY_ICONS = {
    201: "ABILITY_Fire_Shot.svg",        # 火球
    202: "ABILITY_Healing_Salve.svg",    # 治疗
    203: "ABILITY_Chain_Lightning.svg",  # 电击
    204: "ABILITY_Poison_Gas.svg",       # 毒液喷吐
    205: "ABILITY_Fire_Blast.svg",       # 烈焰扫荡
}

# 状态 ID → wiki 图标
STATUS_ICONS = {
    1: "STATUS_All_Stats_Up_Icon.svg",
    2: "STATUS_Strength_Up_Icon.svg",
    3: "STATUS_Burn_Icon.svg",
    4: "STATUS_Poison_Icon.svg",
    5: "STATUS_Bleed_Icon.svg",
    6: "STATUS_Stun_Icon.svg",
    7: "STATUS_Weakness_Icon.svg",
    8: "STATUS_Wet_Icon.svg",
}

# 地形编码 → wiki 图标（缺失则回退程序化颜色）
TILE_ICONS = {
    1: "TILE_Empty_Tile_Icon.svg",
    2: "TILE_Road_Tile_Icon.svg",
    3: "TILE_Grass_Tile_Icon.svg",
    4: "TILE_Tall_Grass_Tile_Icon.svg",
    5: "TILE_Flower_Tile_Icon.svg",
    6: "TILE_Tall_Flower_Tile_Icon.svg",
    7: "TILE_Bramble_Tile_Icon.svg",
    8: "TILE_Thick_Bramble_Tile_Icon.svg",
    9: "TILE_Water_Tile_Icon.svg",
    10: "TILE_Water_Current_Tile_Icon.svg",
    11: "TILE_Creep_Tile_Icon.svg",
    12: "TILE_Lava_Tile_Icon.svg",
    13: "TILE_Shadow_Icon.svg",
    14: "TILE_Basalt_Tile_Icon.svg",
}

# 猫职业（13 个 wiki CLASS_* 动画）
CLASS_KEYS = ["Butcher", "Cleric", "Collarless", "Druid", "Fighter", "Hunter",
              "Jester", "Mage", "Monk", "Necromancer", "Psychic", "Tank", "Thief"]


def stem_of(fname: str) -> str:
    stem = os.path.splitext(fname)[0].lower()
    return "".join(c if c.isalnum() else "_" for c in stem)


def main() -> int:
    os.makedirs(OUT_SPRITES, exist_ok=True)
    os.makedirs(OUT_ICONS, exist_ok=True)

    if not os.path.isdir(SRC):
        print("[assets] wiki-asset/sprites 不存在（先完成 wiki 下载），仅清理输出")
        return 1

    manifest: dict = {}
    seen: dict = {}

    def convert_one(fname: str) -> None:
        path = os.path.join(SRC, fname)
        if not os.path.isfile(path):
            return
        stem = stem_of(fname)
        if stem in seen:
            seen[stem] += 1
            stem = f"{stem}_{seen[stem]}"
        else:
            seen[stem] = 0
        ext = os.path.splitext(fname)[1].lower()
        try:
            if ext == ".gif":
                convert_sprites.convert_gif(path, OUT_SPRITES, stem, manifest)
            elif ext in (".png", ".jpg", ".jpeg", ".webp"):
                convert_sprites.copy_static(path, OUT_SPRITES, stem, manifest)
        except Exception as e:
            print(f"[assets] SKIP {fname}: {e}")

    # 1. 敌人动画（Idle/Attack/Death）
    for def_id, name in ENEMY_ANIMS.items():
        for anim in ("Idle", "Attack", "Death"):
            convert_one(f"ENEMY_{name}_{anim}.gif")

    # 2. 猫职业动画
    for cls in CLASS_KEYS:
        for anim in ("Idle", "Start", "Victory"):
            convert_one(f"CLASS_{cls}_{anim}.gif")

    # 3. 敌人静态立绘（兜底）
    for fname in sorted(os.listdir(SRC)):
        if "level_editor" in fname.lower() and fname.lower().endswith(".png"):
            convert_one(fname)

    with open(os.path.join(OUT_SPRITES, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=1)
    print(f"[assets] {len(manifest)} sprites -> {OUT_SPRITES}")

    # 4. 图标复制
    def copy_icon(src_name: str, dst_name: str) -> bool:
        p = os.path.join(SRC, src_name)
        if os.path.isfile(p):
            shutil.copyfile(p, os.path.join(OUT_ICONS, dst_name))
            return True
        print(f"[assets] WARN: 缺少 {src_name}")
        return False

    for aid, fname in ABILITY_ICONS.items():
        copy_icon(fname, f"ability_{aid}.svg")
    # 原版回合指示图标（wiki UI_Turn_*）
    copy_icon("UI_Turn_(Ally)_Icon.svg", "turn_ally.svg")
    copy_icon("UI_Turn_(Enemy)_Icon.svg", "turn_enemy.svg")
    for sid, fname in STATUS_ICONS.items():
        copy_icon(fname, f"status_{sid}.svg")
    for code, fname in TILE_ICONS.items():
        copy_icon(fname, f"tile_{code}.svg")

    # 敌人名清单（board.gd 构建动画用）
    with open(os.path.join(OUT_SPRITES, "enemy_anims.json"), "w", encoding="utf-8") as f:
        json.dump({str(k): v for k, v in ENEMY_ANIMS.items()}, f, ensure_ascii=False, indent=1)
    print("[assets] icons + enemy_anims.json prepared")
    return 0


if __name__ == "__main__":
    sys.exit(main())
