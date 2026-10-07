"""
convert_fp.py - runs INSIDE Blender (tools/fpmodel/build_fp.py starts it):
    blender -b --factory-startup -P convert_fp.py -- <config.json> <work dir>

Turns a rigged, animated first-person model (glTF/GLB, FBX or .blend) into GoldSrc studiomdl sources:
  <work>/ref.smd            the mesh in its rest pose, every vertex on the one bone that moves it most
  <work>/<sequence>.smd     one animation per sequence of the config (a frame range of an action)
  <work>/tex_<n>.png        each material's base colour, at most tex_size pixels a side
  <work>/manifest.json      bones, textures, triangle count, what was done
Space: the view model's own, as Sven's decompiled view models have it: the eye at the origin, -Y forward, +X left,
+Z up, 1 unit = 1 inch. The config's "transform" (rotate degrees XYZ, then scale, then offset) takes the source's
world space there; "forward_axis" of the source is reported to help set it up.
Meshes are decimated to about target_tris triangles in all (collapse, keeping UVs and weights).
"""
import bpy, bmesh, json, math, os, sys
from mathutils import Matrix, Vector, Euler

args = sys.argv[sys.argv.index('--') + 1:]
cfg = json.load(open(args[0]))
work = args[1]
os.makedirs(work, exist_ok=True)
log = []


def note(*a):
    s = ' '.join(str(x) for x in a)
    log.append(s)
    print('[convert_fp]', s)


# ---------------------------------------------------------------- import
src = cfg['input']
ext = os.path.splitext(src)[1].lower()
if ext == '.blend':
    bpy.ops.wm.open_mainfile(filepath=src)
else:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if ext in ('.glb', '.gltf'):
        bpy.ops.import_scene.gltf(filepath=src)
    elif ext == '.fbx':
        bpy.ops.import_scene.fbx(filepath=src, use_anim=True)
    else:
        raise SystemExit('unknown input type ' + ext)
scene = bpy.context.scene
arms = [o for o in scene.objects if o.type == 'ARMATURE']
if cfg.get('armature'):
    arms = [o for o in arms if o.name == cfg['armature']]
if not arms:
    raise SystemExit('no armature in ' + src)
arm = max(arms, key=lambda o: len(o.data.bones))
meshes = [o for o in scene.objects if o.type == 'MESH' and o.visible_get() and o.name not in cfg.get('skip_meshes', [])]
note('armature', arm.name, len(arm.data.bones), 'bones;', len(meshes), 'meshes:', [m.name for m in meshes])
note('actions', [a.name for a in bpy.data.actions], 'scene frames', scene.frame_start, scene.frame_end)

# ---------------------------------------------------------------- space conversion
tf = cfg.get('transform', {})
R = Euler([math.radians(v) for v in tf.get('rotate', [0, 0, 0])], 'XYZ').to_matrix().to_4x4()
S = tf.get('scale', 1.0)
O = Vector(tf.get('offset', [0, 0, 0]))


def to_smd_point(p):
    return (R @ (p * S).to_4d()).to_3d() + O


def to_smd_rot(m3):
    return R.to_3x3() @ m3


# bones exported: the deforming ones and their ancestors (or all, by config)
bones = list(arm.data.bones)
order = []


def visit(b):
    order.append(b)
    for c in b.children:
        visit(c)


for b in bones:
    if b.parent is None:
        visit(b)
index = {b.name: i for i, b in enumerate(order)}
if len(order) > 128:
    raise SystemExit('%d bones: GoldSrc allows 128' % len(order))


def bone_frame():
    """every bone's transform relative to its parent, in SMD space: (position, euler xyz)"""
    out = []
    world = {}
    for b in order:
        pb = arm.pose.bones[b.name]
        m = arm.matrix_world @ pb.matrix
        pos = to_smd_point(m.to_translation())
        rot = to_smd_rot(m.to_3x3().normalized())
        world[b.name] = (pos, rot)
        if b.parent:
            ppos, prot = world[b.parent.name]
            inv = prot.transposed()
            lpos = inv @ (pos - ppos)
            lrot = inv @ rot
        else:
            lpos, lrot = pos, rot
        e = lrot.to_euler('XYZ')
        out.append((lpos, e))
    return out


def skeleton_lines(frames):
    L = ['nodes'] + ['%d "%s" %d' % (i, b.name.replace('"', ''), index[b.parent.name] if b.parent else -1) for i, b in enumerate(order)]
    L += ['end', 'skeleton']
    for t, fr in enumerate(frames):
        L.append('time %d' % t)
        for i, (p, e) in enumerate(fr):
            L.append('%d %.6f %.6f %.6f %.6f %.6f %.6f' % (i, p.x, p.y, p.z, e.x, e.y, e.z))
    L.append('end')
    return L


# ---------------------------------------------------------------- textures
tex_of_material = {}
size = cfg.get('tex_size', 512)


def material_texture(mat):
    if mat is None:
        return None
    if mat.name in tex_of_material:
        return tex_of_material[mat.name]
    img = None
    if mat.use_nodes:
        for n in mat.node_tree.nodes:
            if n.type == 'BSDF_PRINCIPLED':
                inp = n.inputs.get('Base Color')
                if inp and inp.is_linked:
                    src_node = inp.links[0].from_node
                    # through a mix/multiply (vertex colour or AO tints) to the image
                    seen = 0
                    while src_node.type != 'TEX_IMAGE' and seen < 4:
                        lk = [i for i in src_node.inputs if i.is_linked]
                        if not lk:
                            break
                        src_node = lk[0].links[0].from_node
                        seen += 1
                    if src_node.type == 'TEX_IMAGE':
                        img = src_node.image
        if img is None:
            for n in mat.node_tree.nodes:
                if n.type == 'TEX_IMAGE' and n.image:
                    img = n.image
                    break
    name = 'tex_%d' % len(tex_of_material)
    path = os.path.join(work, name + '.png')
    if img is not None:
        cp = img.copy()
        w, h = cp.size
        k = min(1.0, size / max(w, h))
        cp.scale(max(8, int(w * k) // 8 * 8), max(8, int(h * k) // 8 * 8))
        cp.filepath_raw = path
        cp.file_format = 'PNG'
        cp.save()
        note('texture', mat.name, '->', name, img.size[:], '->', cp.size[:])
    else:
        # a flat colour: the material's base colour
        col = (0.6, 0.6, 0.6)
        if mat.use_nodes:
            for n in mat.node_tree.nodes:
                if n.type == 'BSDF_PRINCIPLED':
                    col = tuple(n.inputs['Base Color'].default_value[:3])
        im = bpy.data.images.new(name, 8, 8)
        im.pixels = [c for _ in range(64) for c in (col[0], col[1], col[2], 1.0)]
        im.filepath_raw = path
        im.file_format = 'PNG'
        im.save()
        note('texture', mat.name, '-> flat colour', col)
    tex_of_material[mat.name] = name + '.bmp'
    return tex_of_material[mat.name]


# ---------------------------------------------------------------- decimate and write the reference mesh
total = 0
for m in meshes:
    total += sum(len(p.vertices) - 2 for p in m.data.polygons)
target = cfg.get('target_tris', 6000)
ratio = min(1.0, target / max(total, 1))
note('triangles', total, '-> target', target, 'ratio %.3f' % ratio)

arm.data.pose_position = 'REST'
bpy.context.view_layer.update()
deps = bpy.context.evaluated_depsgraph_get()
tris = []
for m in meshes:
    if ratio < 0.999:
        mod = m.modifiers.new('fp_decimate', 'DECIMATE')
        mod.ratio = ratio
        mod.use_collapse_triangulate = True
        # before the armature, so the mesh is decimated in its rest shape
        while m.modifiers.find('fp_decimate') > 0:
            with bpy.context.temp_override(object=m):
                bpy.ops.object.modifier_move_up(modifier='fp_decimate')
        with bpy.context.temp_override(object=m):
            bpy.ops.object.modifier_apply(modifier='fp_decimate')
    deps = bpy.context.evaluated_depsgraph_get()
    ev = m.evaluated_get(deps)
    me = ev.to_mesh()
    me.calc_loop_triangles()
    uv = me.uv_layers.active.data if me.uv_layers.active else None
    groups = {g.index: g.name for g in m.vertex_groups}
    # the bone of each vertex: its heaviest deforming group, else the bone the object hangs from
    fallback = m.parent_bone if m.parent_type == 'BONE' and m.parent_bone in index else order[0].name
    vb = []
    for v in me.vertices:
        best, bw = None, 0.0
        for g in v.groups:
            nm = groups.get(g.group)
            if nm in index and g.weight > bw:
                best, bw = nm, g.weight
        vb.append(index[best or fallback])
    mw = ev.matrix_world
    nm3 = mw.to_3x3().inverted().transposed()
    for lt in me.loop_triangles:
        mat = m.material_slots[lt.material_index].material if m.material_slots else None
        tex = material_texture(mat) or 'tex_none.bmp'
        rows = [tex]
        for k in (0, 1, 2):
            vi = lt.vertices[k]
            li = lt.loops[k]
            p = to_smd_point(mw @ me.vertices[vi].co)
            n = (to_smd_rot(nm3) @ me.loops[li].normal).normalized() if hasattr(me.loops[li], 'normal') else Vector((0, 0, 1))
            u, w = (uv[li].uv[0], uv[li].uv[1]) if uv else (0.0, 0.0)
            rows.append('%d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f' % (vb[vi], p.x, p.y, p.z, n.x, n.y, n.z, u, w))
        # studiomdl wants the faces wound the other way round when a mirror flipped the space
        if R.to_3x3().determinant() * (1 if S > 0 else -1) < 0:
            rows[2], rows[3] = rows[3], rows[2]
        tris.extend(rows)
    ev.to_mesh_clear()
note('triangles written', len(tris) // 4)

rest = bone_frame()
open(os.path.join(work, 'ref.smd'), 'w', newline='\n').write(
    '\n'.join(['version 1'] + skeleton_lines([rest]) + ['triangles'] + tris + ['end']) + '\n')

# ---------------------------------------------------------------- the sequences
arm.data.pose_position = 'POSE'
ad = arm.animation_data
seqs = []
for s in cfg.get('sequences', []):
    if s.get('action') and ad is not None:
        ad.action = bpy.data.actions[s['action']]
    a, b = s['range']
    step = 1 if b >= a else -1
    frames = []
    for f in range(a, b + step, step):
        scene.frame_set(f)
        frames.append(bone_frame())
    open(os.path.join(work, s['name'] + '.smd'), 'w', newline='\n').write(
        '\n'.join(['version 1'] + skeleton_lines(frames)) + '\n')
    seqs.append({'name': s['name'], 'frames': len(frames)})

# where the gun's muzzle is, for the attachment
att = []
for a_ in cfg.get('attachments', []):
    att.append({'bone': a_['bone'], 'index': index.get(a_['bone'], 0), 'offset': a_.get('offset', [0, 0, 0])})

json.dump({'bones': [b.name for b in order], 'textures': sorted(set(tex_of_material.values())), 'sequences': seqs,
           'triangles_in': total, 'triangles_out': len(tris) // 4, 'attachments': att, 'log': log},
          open(os.path.join(work, 'manifest.json'), 'w'), indent=1)
print('[convert_fp] done')
