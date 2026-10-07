import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"
def patch(name, pairs):
    p = os.path.join(D, name); s = open(p, newline='', encoding='utf-8').read()
    for old, new in pairs:
        assert s.count(old) == 1, (name, old[:80], s.count(old)); s = s.replace(old, new)
    open(p, 'w', newline='', encoding='utf-8').write(s); print('patched', name)

patch('sc_save.as', [("if( szModel.Length() > 1 && szModel[0] == '*' )", 'if( szModel.Length() > 1 && szModel.StartsWith( "*" ) )')])
patch('svencraft.as', [
    ('#include "sc_devtour"\n', '#include "sc_devtour"\n#include "sc_save"\n'),
    ('''	g_Hooks.RegisterHook( Hooks::Player::PlayerPostThink, @SC_CraftPostThink );

	SC_InitMaterials();
	SC_InitMixRules();
	SC_InitRecipes();
}''', '''	g_Hooks.RegisterHook( Hooks::Player::PlayerPostThink, @SC_CraftPostThink );
	g_Hooks.RegisterHook( Hooks::Game::MapChange, @SC_MapChange );
	g_Hooks.RegisterHook( Hooks::Player::ClientDisconnect, @SC_ClientDisconnect );

	SC_InitMaterials();
	SC_InitMixRules();
	SC_InitRecipes();

	g_SCSaveEnabled = !SC_FlagFile( "sc_autotest" ) && !SC_FlagFile( "sc_visualtest" );
	SC_LoadPlayers();
}'''),
    ('''	SC_FindTemplates();
	if( g_SCSandbox )
		SC_GenerateTerrain();
''', '''	SC_FindTemplates();
	g_SCGone.deleteAll();
	g_SCLastWorldSave = "";
	if( g_SCSandbox && !( g_SCSaveEnabled && SC_LoadWorld() ) )
		SC_GenerateTerrain();
	SC_StartSaveTimer();
'''),
    ('''		SC_Chat( pPlayer, "!inv  !craft  !block <name>  !creative  !tools  !clearblocks  !resetworld (sandbox)" );''',
     '''		SC_Chat( pPlayer, "!inv  !craft  !block <name>  !creative  !tools  !save  !clearblocks  !resetworld (sandbox)" );'''),
    ('''	else if( szCmd == "!inv" || szCmd == "!i" )''', '''	else if( szCmd == "!save" )
	{
		SC_SaveWorld( true );
		SC_SavePlayers( true );
		SC_Chat( pPlayer, g_SCSaveEnabled ? ( g_SCSandbox ? "World and inventories saved" : "Inventories saved" ) : "Saving is off (test mode)" );
	}
	else if( szCmd == "!inv" || szCmd == "!i" )'''),
])
patch('sc_sandbox.as', [('''	g_SCBlockCount = 0;
	g_SCCellProgress.deleteAll();
	SC_ClearOres();''', '''	g_SCBlockCount = 0;
	g_SCCellProgress.deleteAll();
	SC_ClearOres();
	SC_ClearWorldSave();''')])
patch('sc_props.as', [
    ('''	SC_BreakSound( vecCenter, m.family );
	g_EntityFuncs.Remove( pEnt );
	SC_GiveMix( pPlayer, iTool, mats, weights, flAmount, vecCenter );''', '''	SC_BreakSound( vecCenter, m.family );
	SC_MarkGone( pEnt );
	g_EntityFuncs.Remove( pEnt );
	SC_GiveMix( pPlayer, iTool, mats, weights, flAmount, vecCenter );'''),
    ('''	if( pieces.length() == 0 )
		return KIND_PROP;
	g_EntityFuncs.Remove( pEnt );''', '''	if( pieces.length() == 0 )
		return KIND_PROP;
	SC_MarkGone( pEnt );
	g_EntityFuncs.Remove( pEnt );'''),
    ('''	if( !bBroken )
		return;
	array<int> mats = { iTexMat == MAT_NONE ? MAT_PLANKS : iTexMat };''', '''	if( !bBroken )
		return;
	SC_MarkGone( pEnt );
	array<int> mats = { iTexMat == MAT_NONE ? MAT_PLANKS : iTexMat };'''),
])
