"""Blender preview renderer. Geometry comes from the exported CAD meshes."""
import bpy
import math
import json
import sys
from pathlib import Path
from mathutils import Vector
HERE=Path(__file__).resolve().parent
REF=HERE/'exports/reference'
OUT=HERE/'previews';OUT.mkdir(exist_ok=True)
B=json.loads((HERE/'board_features.json').read_text())
P=json.loads((HERE/'parameters.json').read_text())
M=json.loads((HERE/'assembly_metadata.json').read_text())
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
scene.render.engine='CYCLES';scene.cycles.samples=32
scene.cycles.use_denoising=True
scene.render.resolution_x=1300;scene.render.resolution_y=950;scene.render.resolution_percentage=100
scene.world.color=(.3,.3,.3)
scene.view_settings.view_transform='AgX'

def mat(name,color,rough=.4,metal=0.,trans=0.):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True
    bs=m.node_tree.nodes.get('Principled BSDF')
    bs.inputs['Base Color'].default_value=(*color,1)
    bs.inputs['Roughness'].default_value=rough
    bs.inputs['Metallic'].default_value=metal
    bs.inputs['Transmission Weight'].default_value=trans
    bs.inputs['IOR'].default_value=1.46
    return m
case_mat=mat('Matte petrol polymer',(.047,.079,.088),.34)
cap_mat=mat('Grey polymer',(.26,.31,.32),.4)
accent=mat('Muted orange polymer',(.75,.23,.07),.4)
black=mat('Chip package',(.018,.021,.024),.28)
socket=mat('Socket polymer',(.028,.032,.035),.45)
metal=mat('Nickel and tin',(.51,.55,.58),.23,.8)
board_mat=mat('PCB solder mask',(.018,.14,.059),.45)
display=mat('OLED glass',(.006,.009,.015),.14)
white=mat('Printing',(.55,.58,.60),.4)
light=bpy.data.materials.new('OLED pixels');light.use_nodes=True
n=light.node_tree.nodes.get('Principled BSDF');n.inputs['Base Color'].default_value=(.75,.85,.95,1)
n.inputs['Emission Color'].default_value=(.55,.75,1,1);n.inputs['Emission Strength'].default_value=2.2
objects={};lid_objects=[];pcb_objects=[]

def mesh(name,filename,material,group=None):
    bpy.ops.wm.stl_import(filepath=str(filename),global_scale=.001)
    ob=bpy.context.object;ob.name=name;ob.data.materials.append(material)
    objects[name]=ob
    if group is not None:group.append(ob)
    return ob

mesh('Base',REF/'base.stl',case_mat)
mesh('Lid',REF/'lid.stl',case_mat,lid_objects)
mesh('PCB',REF/'pcb.stl',board_mat,pcb_objects)
first=B['features']['SW5']['center']
for i,ref in enumerate(['SW5','SW6','SW7']):
    ob=mesh('Button '+str(i+1),REF/'button_cap.stl',accent if i==1 else cap_mat,lid_objects)
    pt=B['features'][ref]['center'];ob.location.x+=(pt[0]-first[0])*.001
    ob.location.y+=(pt[1]-first[1])*.001
for name in M['visual_components']:
    if name=='OLED_glass': material=display
    elif name=='OLED_PCB':material=board_mat
    elif name.startswith('OLED_spacer'):material=metal
    elif name.startswith(('J1','SW')) or '_lead' in name:material=metal
    elif name.endswith('_socket'):material=socket
    else:material=black
    mesh(name,REF/(name+'.stl'),material,pcb_objects)
# Open TRS sockets visually: black centre recessed at the actual PCB edge.
for ref in ['J2','J3','J6','J7','J8','J9']:
    x,y=B['features'][ref]['center'];left=x<42.5
    bpy.ops.mesh.primitive_cylinder_add(vertices=48,radius=.0018,depth=.0002,
        location=((-.06 if left else 85.06)*.001,y*.001,(M['pcb_top_z']+P['jack_axis_above_pcb_mm'])*.001),rotation=(0,math.pi/2,0))
    ob=bpy.context.object;ob.name=ref+' aperture';ob.data.materials.append(black);pcb_objects.append(ob)
# Illustrative markings on the IC packages, not engraving on the printed case.
def text(name,body,x,y,z,size,material):
    curve=bpy.data.curves.new(name,'FONT');curve.body=body;curve.size=size*.001
    curve.align_x='CENTER';curve.align_y='CENTER';curve.extrude=0
    ob=bpy.data.objects.new(name,curve);bpy.context.collection.objects.link(ob)
    ob.location=(x*.001,y*.001,z*.001);ob.data.materials.append(material);pcb_objects.append(ob)
    return ob
for ref,label in [('U10','SAA1099'),('U11','SN76489AN'),('U12','SN76489AN')]:
    pts=[p['center'] for p in B['features'][ref]['pads']]
    xc=(min(p[0] for p in pts)+max(p[0] for p in pts))/2
    text(ref+' mark',label,xc,45.5,M['pcb_top_z']+8.515,1.55,white)
# 128x64 screen illustration; does not represent running device firmware.
xc,yc=42.5,M['oled_active_y'];z=M['pcb_top_z']+P['oled_spacer_height_mm']+P['oled_pcb_thickness_mm']+P['oled_front_height_above_pcb_mm']+.015
text('Screen title','MIX',xc,yc+3.7,z,1.4,light)
for j,label in enumerate(['SAA   08','SN1   10','SN2   06']):
    text('Screen row '+str(j),label,xc,yc+1.1-2.35*j,z,1.1,light)
# Assembly fasteners: M2 screw heads below the base and brass inserts in the lid.
brass=mat('Brass inserts',(.48,.31,.09),.23,.75)
for ref in ['H1','H2','H3','H4']:
    x,y=B['features'][ref]['center']
    bpy.ops.mesh.primitive_cylinder_add(vertices=40,radius=.00175,depth=.003,location=(x*.001,y*.001,(M['pcb_top_z']+1.5)*.001))
    ob=bpy.context.object;ob.name=ref+' insert';ob.data.materials.append(brass);lid_objects.append(ob)
    bpy.ops.mesh.primitive_cylinder_add(vertices=40,radius=.0019,depth=.002,location=(x*.001,y*.001,.001))
    ob=bpy.context.object;ob.name=ref+' screw';ob.data.materials.append(metal)
# Studio setup.
for name,loc,power,size in [('Key',(.05,-.08,.20),18,.15),('Rim',(-.07,.09,.15),12,.10),('Fill',(.16,.06,.12),8,.10)]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power/80;data.shape='DISK';data.size=size
    ob=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(ob);ob.location=loc
    ob.rotation_euler=(Vector((.0425,.027,.012))-ob.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.0002))
plane=bpy.context.object;plane.name='Studio ground';plane.data.materials.append(mat('Ground',(.23,.26,.27),.6))
cam_data=bpy.data.cameras.new('Camera');cam=bpy.data.objects.new('Camera',cam_data);bpy.context.collection.objects.link(cam);scene.camera=cam
cam_data.type='ORTHO';cam_data.lens=50

def render(filename,loc,target,scale):
    cam.location=loc;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()
    cam_data.ortho_scale=scale
    scene.render.filepath=str(OUT/filename)
    bpy.ops.render.render(write_still=True)

render('assembled-test.png' if '--quick' in sys.argv else 'assembled.png',(.13,-.11,.29),(.0425,.027,.013),.125)
if '--quick' in sys.argv:
    sys.exit(0)
# Save editable assembled Blender scene before changing transforms.
scene.render.filepath='//../previews/assembled.png'
bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'exports/enclosure_preview.blend'))
render('top.png',(.0425,.027,.35),(.0425,.027,0),.112)
for ob in lid_objects:ob.location.z+=.045
for ob in pcb_objects:ob.location.z+=.012
render('exploded.png',(.16,-.14,.23),(.0425,.027,.043),.17)
# Base interior, with all upper parts hidden; useful for service access/fasteners.
for ob in lid_objects+pcb_objects:ob.hide_render=True
render('base_inside.png',(.14,-.12,.18),(.0425,.027,.003),.12)
print('Rendered enclosure views')
