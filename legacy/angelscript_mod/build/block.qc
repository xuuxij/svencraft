$modelname "block.mdl"
$cd "."
$cdtexture "./tex"
$scale 1.0
$origin 0 0 0
$bbox -20 -20 -20 20 20 20
$cbox -20 -20 -20 20 20 20
$bodygroup "shape"
{
	studio "cube"
	studio "slab"
	studio "plate"
	studio "sheet"
}
$sequence "idle" "cube" fps 1

$texrendermode "glass.bmp" "masked"
$texrendermode "leaves.bmp" "masked"

$texturegroup "skinfamilies"
{
	{ "grass_top.bmp" "grass_side.bmp" "dirt.bmp" }
	{ "dirt.bmp" "dirt.bmp" "dirt.bmp" }
	{ "stone.bmp" "stone.bmp" "stone.bmp" }
	{ "cobble.bmp" "cobble.bmp" "cobble.bmp" }
	{ "brick.bmp" "brick.bmp" "brick.bmp" }
	{ "planks.bmp" "planks.bmp" "planks.bmp" }
	{ "log_top.bmp" "log_side.bmp" "log_top.bmp" }
	{ "leaves.bmp" "leaves.bmp" "leaves.bmp" }
	{ "sand.bmp" "sand.bmp" "sand.bmp" }
	{ "gravel.bmp" "gravel.bmp" "gravel.bmp" }
	{ "snow.bmp" "snow.bmp" "snow.bmp" }
	{ "metal.bmp" "metal.bmp" "metal.bmp" }
	{ "concrete.bmp" "concrete.bmp" "concrete.bmp" }
	{ "tile.bmp" "tile.bmp" "tile.bmp" }
	{ "rubber.bmp" "rubber.bmp" "rubber.bmp" }
	{ "glass.bmp" "glass.bmp" "glass.bmp" }
	{ "circuit.bmp" "circuit.bmp" "circuit.bmp" }
	{ "vent.bmp" "vent.bmp" "vent.bmp" }
	{ "bench_top.bmp" "bench_side.bmp" "planks.bmp" }
	{ "iron_ore.bmp" "iron_ore.bmp" "iron_ore.bmp" }
	{ "crystal_ore.bmp" "crystal_ore.bmp" "crystal_ore.bmp" }
	{ "glass.bmp" "glass.bmp" "glass.bmp" }
	{ "planks.bmp" "planks.bmp" "planks.bmp" }
}
