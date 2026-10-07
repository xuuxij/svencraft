import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"

def patch(name, reps):
    p = os.path.join(D, name)
    s = open(p, newline='').read()
    for a, b in reps:
        assert a in s, (name, a[:70])
        s = s.replace(a, b, 1)
    open(p, 'w', newline='').write(s)

# Menus: never replace/unregister a menu from inside its own callback (that crashed the game).
# Callbacks schedule the next menu for just after they return.
patch('sc_crafting.as', [
    ("""	int iCat = 0;
	pItem.m_pUserData.retrieve( iCat );
	SC_OpenRecipes( pPlayer, iCat, 0 );
}""", """	int iCat = 0;
	pItem.m_pUserData.retrieve( iCat );
	g_Scheduler.SetTimeout( "SC_OpenRecipesLater", 0.05, EHandle( pPlayer ), iCat, 0 );
}

// Menu transitions run outside menu callbacks: the menu that called back is still in use until it returns.
void SC_OpenRecipesLater( EHandle hPlayer, int iCat, int iPage )
{
	CBasePlayer@ pPlayer = cast<CBasePlayer@>( hPlayer.GetEntity() );
	if( pPlayer !is null && pPlayer.IsConnected() )
		SC_OpenRecipes( pPlayer, iCat, iPage );
}

void SC_OpenCraftingLater( EHandle hPlayer )
{
	CBasePlayer@ pPlayer = cast<CBasePlayer@>( hPlayer.GetEntity() );
	if( pPlayer !is null && pPlayer.IsConnected() )
		SC_OpenCrafting( pPlayer );
}"""),
    ("""	SC_OpenRecipes( pPlayer, g_SCRecipes[iRecipe].category, iIndex / 7 );""",
     """	g_Scheduler.SetTimeout( "SC_OpenRecipesLater", 0.05, EHandle( pPlayer ), g_SCRecipes[iRecipe].category, iIndex / 7 );"""),
])
patch('sc_hud.as', [
    ("""	if( iMat == -2 )
	{
		SC_OpenCrafting( pPlayer );
		return;
	}""", """	if( iMat == -2 )
	{
		g_Scheduler.SetTimeout( "SC_OpenCraftingLater", 0.05, EHandle( pPlayer ) );
		return;
	}"""),
])
# Workbench +use also goes through the deferred path (Use can fire while a menu is open).
for f in ('sc_sandbox.as', 'sc_blocks.as'):
    patch(f, [("			SC_OpenCrafting( cast<CBasePlayer@>( pActivator ) );",
               "			g_Scheduler.SetTimeout( \"SC_OpenCraftingLater\", 0.05, EHandle( pActivator ) );")])

# Every mined piece gives one whole block (Minecraft rule), however thin it is.
patch('sc_blocks.as', [
    ("""	// A full 32-unit block gives 1; thinner pieces give their share of a block.
	SC_GiveDrop( pPlayer, iTool, iMat, vecBox.x * vecBox.y * vecBox.z / ( SC_BLOCK_SIZE * SC_BLOCK_SIZE * SC_BLOCK_SIZE ) );""",
     """	// Every piece gives one whole block, however thin (Minecraft rule: one block mined = one item).
	SC_GiveDrop( pPlayer, iTool, iMat, 1.0 );"""),
])
print('ok')
