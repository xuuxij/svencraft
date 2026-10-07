import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"
S = r"."

def patch(name, reps):
    p = os.path.join(D, name)
    s = open(p, newline='').read()
    for a, b in reps:
        assert a in s, (name, a[:70])
        s = s.replace(a, b, 1)
    open(p, 'w', newline='').write(s)

# crafting: drop the text menus, add the window
p = os.path.join(D, 'sc_crafting.as')
s = open(p, newline='').read()
cut = s.index('//\n// Menus: categories -> recipes.')
s = s[:cut] + open(os.path.join(S, 'craftui_code.as')).read()
s = s.replace("	string item;             // entity output (weapon_*, ammo_*, item_*)\n	int itemCount = 0;",
              "	string item;             // entity output (weapon_*, ammo_*, item_*)\n	int itemCount = 0;\n	int icon = 0;            // craftslots.spr icon id (SC_AssignRecipeIcons)")
s = s.replace('	SC_ItemRecipe( CRAFT_SUPPLIES, "Armor Battery", "circuit:1,metal:1", "item_battery" );\n}',
              '	SC_ItemRecipe( CRAFT_SUPPLIES, "Armor Battery", "circuit:1,metal:1", "item_battery" );\n\n	SC_AssignRecipeIcons();\n}')
s = s.replace('	g_SoundSystem.PrecacheSound( "common/wpn_denyselect.wav" );\n}',
              '	g_SoundSystem.PrecacheSound( "common/wpn_denyselect.wav" );\n	SC_PrecacheCraftUI();\n}')
assert 'SC_AssignRecipeIcons();' in s and 'SC_PrecacheCraftUI();' in s and 'int icon = 0;' in s
open(p, 'w', newline='').write(s)

patch('sc_inventory.as', [
    ("	CTextMenu@ craftMenu = null;", "	bool craftOpen = false;     // crafting window (sc_crafting.as)\n	int craftCat = 0;\n	int craftSel = 0;\n	int lastButtons = 0;\n	float craftClosedTime = 0;"),
])
# the hotbar must not draw over the crafting window (they share HUD channels)
patch('sc_hud.as', [
    ("""	if( pPlayer is null || !pPlayer.IsConnected() )
		return;
	SCInventory@ inv = SC_Inv( pPlayer );

	array<int> owned;""", """	if( pPlayer is null || !pPlayer.IsConnected() )
		return;
	SCInventory@ inv = SC_Inv( pPlayer );
	if( inv.craftOpen )
		return;   // the crafting window is using the HUD channels; it redraws the hotbar on close

	array<int> owned;"""),
])
patch('svencraft.as', [
    ("	g_Hooks.RegisterHook( Hooks::Player::PlayerSpawn, @SC_PlayerSpawn );\n",
     "	g_Hooks.RegisterHook( Hooks::Player::PlayerSpawn, @SC_PlayerSpawn );\n	g_Hooks.RegisterHook( Hooks::Player::PlayerPreThink, @SC_CraftPreThink );\n"),
])
print('ok')
