p = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft\svencraft.as"
s = open(p, newline='').read()
anchor = "	g_PlayerFuncs.BotDisconnect( pBot );\n}"
new = '''	SC_SelfTestCraftUI( pBot );
	g_PlayerFuncs.BotDisconnect( pBot );
}

// Simulate one key press for the crafting window.
void SC_TestPress( CBasePlayer@ pBot, int iButton )
{
	uint uiFlags;
	pBot.pev.button = iButton;
	SC_CraftPreThink( pBot, uiFlags );
	pBot.pev.button = 0;
	SC_CraftPreThink( pBot, uiFlags );
}

void SC_SelfTestCraftUI( CBasePlayer@ pBot )
{
	SCInventory@ inv = SC_Inv( pBot );
	for( int k = 0; k < MAT_COUNT; k++ ) inv.count[k] = 0;
	inv.count[MAT_LOG] = 2;
	inv.craftCat = CRAFT_BASIC;
	inv.craftSel = 0;
	inv.craftOpen = false;
	inv.craftClosedTime = 0;
	pBot.pev.deadflag = DEAD_NO;
	pBot.pev.health = 100;
	SC_OpenCrafting( pBot );
	string szLog = "SCTEST craftui open=" + inv.craftOpen + " frozen=" + ( ( pBot.pev.flags & FL_FROZEN ) != 0 ) + " alive=" + pBot.IsAlive();
	SC_TestPress( pBot, IN_ATTACK );                 // Basic #0: log -> 4 planks
	szLog += " | after craft#0 logs=" + inv.Whole( MAT_LOG ) + " planks=" + inv.Whole( MAT_PLANKS );
	SC_TestPress( pBot, IN_ATTACK );
	szLog += " | again logs=" + inv.Whole( MAT_LOG ) + " planks=" + inv.Whole( MAT_PLANKS );
	SC_TestPress( pBot, IN_MOVERIGHT );              // Basic #1: workbench
	szLog += " | sel=" + inv.craftCat + "/" + inv.craftSel;
	SC_TestPress( pBot, IN_ATTACK );
	szLog += " | workbench=" + inv.Whole( MAT_WORKBENCH ) + " planks=" + inv.Whole( MAT_PLANKS );
	SC_TestPress( pBot, IN_MOVERIGHT );              // past the end of Basic -> Blocks #0
	szLog += " | next cat/sel=" + inv.craftCat + "/" + inv.craftSel;
	SC_TestPress( pBot, IN_MOVELEFT );               // back to Basic, last item
	szLog += " | back cat/sel=" + inv.craftCat + "/" + inv.craftSel;
	SC_TestPress( pBot, IN_ATTACK2 );                // close
	szLog += " | closed=" + !inv.craftOpen + " frozen=" + ( ( pBot.pev.flags & FL_FROZEN ) != 0 );
	string szIcons = "";
	for( uint i = 0; i < g_SCRecipes.length(); i++ )
		szIcons += g_SCRecipes[i].icon + ",";
	SC_TestOut( szLog + "\\nSCTEST recipe icons: " + szIcons + "\\n" );
}'''
if 'SC_SelfTestCraftUI' not in s:
    assert anchor in s
    s = s.replace(anchor, new, 1)
open(p, 'w', newline='').write(s)
print('ok')
