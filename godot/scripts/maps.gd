class_name Maps
## ============================================================================
## 战斗地图数据：3 张 10×10 地图（地形编码与 C++ schema TerrainType 枚举序一致：
## EMPTY=1 ROAD=2 GRASS=3 TALL_GRASS=4 FLOWER=5 TALL_FLOWER=6 BRAMBLE=7
## THICK_BRAMBLE=8 WATER=9 WATER_CURRENT=10 CREEP=11 LAVA=12 PIT=13 WALL=14）
## 每张地图有不同的地形交互要素：
##   草原 —— 花丛回复 / 荆棘伤害 / 道路加速
##   河湾 —— 水域(湿润) / 苔沼 / 花丛 / 激流
##   熔岩回廊 —— 熔岩 / 高草闪避 / 荆棘 / 墙
## =============================================================================

const CODES := {
	".": 1, "r": 2, "g": 3, "t": 4, "f": 5, "F": 6,
	"b": 7, "B": 8, "w": 9, "W": 10, "c": 11, "l": 12, "p": 13, "#": 14,
}

# 草原：道路横贯、花丛与荆棘散布
const MAP_GRASSLAND := [
	"gggggggggg",
	"gffgggffgg",
	"gfggtgffgr",
	"rrrrrrrrrr",
	"ggggbggggg",
	"ggtgggbggg",
	"ggggggggff",
	"ggffgggggg",
	"ggggtggggg",
	"gggggggggg",
]

# 河湾：南北向河流 + 两条道路桥 + 苔沼岸
const MAP_RIVER := [
	"ggggwwgggg",
	"gggrwwgggg",
	"ggffwwcggg",
	"gggcwwcggg",
	"gggrwwgggg",
	"ggggwwtggg",
	"ffggwwgggg",
	"gcggwwggff",
	"gggrwwgggg",
	"ggggwwgggg",
]

# 熔岩回廊：墙构成回廊、熔岩池、高草与荆棘
const MAP_LAVA := [
	"##########",
	"#gglgggggg",
	"#gg#gggggg",
	"#gg#glgggg",
	"#ggggggtgg",
	"#ggbgggggg",
	"#gggggg#gg",
	"#ggglgg#gg",
	"#ggggggggg",
	"##########",
]

const MAPS := [
	{"name": "草原", "desc": "道路加速行军，花丛回复，荆棘拦路",
	 "rows": MAP_GRASSLAND,
	 "cats": [Vector2i(0, 1), Vector2i(0, 3), Vector2i(0, 6), Vector2i(0, 8)],
	 "enemies": [[2, 9, 2], [2, 9, 4], [2, 9, 6], [6, 9, 8]]},       # 3 鼠 + 1 喷毒者
	{"name": "河湾", "desc": "水域施加湿润（火焰蒸发），苔沼减速伤害",
	 "rows": MAP_RIVER,
	 "cats": [Vector2i(0, 1), Vector2i(0, 3), Vector2i(0, 6), Vector2i(0, 8)],
	 "enemies": [[3, 9, 2], [2, 9, 4], [2, 9, 6], [5, 9, 8]]},       # 1 蝠 + 2 鼠 + 1 火妖
	{"name": "熔岩回廊", "desc": "熔岩致命，高草隐蔽，荆棘与墙构成回廊",
	 "rows": MAP_LAVA,
	 "cats": [Vector2i(1, 1), Vector2i(1, 3), Vector2i(1, 6), Vector2i(1, 8)],
	 "enemies": [[4, 8, 2], [4, 8, 5], [5, 8, 7], [2, 8, 8], [2, 7, 4]]},  # 2 暴徒 + 1 火妖 + 2 鼠
]


static func terrain_codes(rows: Array) -> PackedInt32Array:
	var out := PackedInt32Array()
	for row in rows:
		for ch in row:
			out.append(CODES.get(ch, 1))
	return out


static func get_map(index: int) -> Dictionary:
	return MAPS[clampi(index, 0, MAPS.size() - 1)]
