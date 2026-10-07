p = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft\svencraft.as"
s = open(p, newline='').read()
old = '''	SC_TestOut( "SCTEST entities in use: " + g_EngineFuncs.NumberOfEntities() + " pieces=" + g_SCBlockCount + "\\n" );
	SC_TestOut( "SCTEST done\\n" );
	File@ f = g_FileSystem.OpenFile( "scripts/plugins/store/sc_selftest.txt", OpenFile::WRITE );'''
new = '''	SC_TestOut( "SCTEST entities in use: " + g_EngineFuncs.NumberOfEntities() + " pieces=" + g_SCBlockCount + "\\n" );
	SC_SelfTestProps();
	SC_TestOut( "SCTEST done\\n" );
	File@ f = g_FileSystem.OpenFile( "scripts/plugins/store/sc_selftest.txt", OpenFile::WRITE );'''
if 'SC_SelfTestProps();\n\tSC_TestOut( "SCTEST done\\n" );\n\tFile@' not in s:
    assert old in s, 'anchor'
    s = s.replace(old, new)
# also report solid/hull state of item_generic props in the props test
old2 = '''			szMix += szModel + "=" + ( szM == "" ? "nothing" : szM ) + "(" + int( SC_BoxBlocks( mx - mn ) * 10 ) / 10.0 + "blk,solid" + pEnt.pev.solid + ") ";'''
new2 = '''			szMix += szModel + "=" + ( szM == "" ? "nothing" : szM ) + "(" + int( SC_BoxBlocks( mx - mn ) * 10 ) / 10.0 + "blk,solid" + pEnt.pev.solid + ( SC_IsHugeProp( mats.length() > 0 ? mats[0] : MAT_METAL, SC_BoxBlocks( mx - mn ) ) ? ",HARVEST-ONLY" : "" ) + ") ";'''
if old2 in s:
    s = s.replace(old2, new2)
s = s.replace('if( seen.getSize() <= 14 )', 'if( seen.getSize() <= 24 )')
open(p, 'w', newline='').write(s)
print('ok')
