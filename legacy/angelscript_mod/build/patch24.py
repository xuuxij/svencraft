p = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft\svencraft.as"
s = open(p, newline='').read()
a = s.index('void SC_SelfTestCraftUI( CBasePlayer@ pBot )')
b = s.index('\n}\n', a) + 3
new = '''void SC_SelfTestCraftUI( CBasePlayer@ pBot )
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
	pBot.pev.v_angle = Vector( 10, 50, 0 );
	SC_OpenCrafting( pBot );
	string szLog = "SCTEST craftui open=" + inv.craftOpen + " frozen=" + ( ( pBot.pev.flags & FL_FROZEN ) != 0 ) + " cursor=" + inv.cursorX + "," + inv.cursorY;
	// mouse: turning right 7 degrees and down 7 degrees should move the cursor +56,+56 (grid slot 4 -> slot 8) and snap the view back
	pBot.pev.v_angle = Vector( 17, 43, 0 );
	SC_CraftPostThink( pBot );
	szLog += " | moved cursor=" + inv.cursorX + "," + inv.cursorY + " view=" + pBot.pev.v_angle.x + "," + pBot.pev.v_angle.y + " fix=" + pBot.pev.fixangle;
	inv.cursorX = -164; inv.cursorY = -64;          // over grid slot 0 (planks recipe, already selected)
	SC_TestPress( pBot, IN_ATTACK );
	szLog += " | click slot0 logs=" + inv.Whole( MAT_LOG ) + " planks=" + inv.Whole( MAT_PLANKS );
	inv.cursorX = -108; inv.cursorY = -64;          // slot 1 (workbench): first click selects
	SC_TestPress( pBot, IN_ATTACK );
	szLog += " | click slot1 sel=" + inv.craftSel + " workbench=" + inv.Whole( MAT_WORKBENCH );
	inv.cursorX = 132; inv.cursorY = -8;            // result slot crafts the selected recipe
	SC_TestPress( pBot, IN_ATTACK );
	szLog += " | click result workbench=" + inv.Whole( MAT_WORKBENCH ) + " planks=" + inv.Whole( MAT_PLANKS );
	inv.cursorX = -40; inv.cursorY = 98;            // next-category button
	SC_TestPress( pBot, IN_ATTACK );
	szLog += " | next button cat=" + inv.craftCat;
	inv.cursorX = -176; inv.cursorY = 98;           // previous-category button
	SC_TestPress( pBot, IN_ATTACK );
	szLog += " | prev button cat=" + inv.craftCat;
	SC_TestPress( pBot, IN_ATTACK2 );               // close
	szLog += " | closed=" + !inv.craftOpen + " frozen=" + ( ( pBot.pev.flags & FL_FROZEN ) != 0 );
	SC_TestOut( szLog + "\\n" );
}
'''
s = s[:a] + new + s[b:]
open(p, 'w', newline='').write(s)
print('ok')
