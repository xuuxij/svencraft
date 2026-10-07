# Crafting design reference (from LambdaCraft-Legacy survey, MIT code; design ideas only — no assets)

Two layers: workbench makes intermediate "materials" (each also costs 1 Box), then tiered "crafter" blocks
turn materials into HL gear. Tiers gated by crafter heat cap (Weapon 4000 / Advanced 7000 / Electric 10000).

Intermediates: Box x10 <- 5 tin + glass. Ammunition x4 <- copper+redstone+gunpowder; Accessories x4 <- copper+redstone+coal;
Explosive x4 <- steel+TNT+gunpowder; Light x2 <- steel+copper+glowstone; Pistol x2 <- 2 steel+copper;
Heavy x2 <- steel block+lapis block+blaze; Tech x2 <- diamond+Lambda Chip+glowstone; Armor x2 <- steel block+diamond+chip; Bio x3 <- flesh+ender eye+DNA.

Weapon crafter: crowbar 2 bar+1 acc (700); glock 2 pistol (1200); 357 3 pistol+2 acc (1400); mp5 3 light+1 acc (2700);
shotgun 5 light+3 acc (3000); grenades x10 2 light+4 expl; 9mm x18 3 ammo; 357 ammo x12 3 ammo+2 acc; shells x8 4 ammo+1 acc.
Advanced: tripmine/satchel x15 3 light+1 tech+6 expl; gauss 8 light+3 tech+5 xen crystal; egon 5 heavy+4 tech+8 crystal;
crossbow 6 light+3 acc+2 bar; hornetgun 4 bio+3 acc; rockets x6 1 heavy+3 expl.
Electric: HEV pieces (tech/light/heavy/armor), sentry 2 tech+2 ammo+2 heavy, medkit x3 2 acc+2 light+1 chip.

Ores: uranium deep & rare (iron pick), tin/copper common shallow (stone pick). Xen crystals from Xen dimension. DNA from mob drops.

Mapping to Svencraft materials: steel<-metal, copper/tin<-circuit/metal ores (to add), glass, planks, cobble, rubber.
Sven already ships all HL weapons (weapon_*), so crafting = GiveNamedItem; no assets needed.

From halfcraft survey: 40 units/block (adopted); greedy box merge (merge_blocks in hc_block_solids.cpp, MIT) usable
to merge terrain pieces back together / compress saves.
