$modelname "door.mdl"
$cd "."
$cdtexture "./tex"
$scale 1.0
$origin 0 0 0 270
$bbox -3 -3 0 37 3 80
$cbox -3 -3 0 37 3 80
$bodygroup "mode"
{
	studio "door_hinge"
	studio "door_item"
}
$texrendermode "door.bmp" "masked"
$sequence "idle" "door_hinge" fps 1
