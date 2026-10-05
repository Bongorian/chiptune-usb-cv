#!/usr/bin/env python3
"""Parametric enclosure: millimetres; XY matches board_features.json.

Requires cadquery and trimesh. Exported print STL files are bed-oriented.
Reference meshes use assembly coordinates and are not manufacturing parts.
"""
import json
import math
from pathlib import Path
import cadquery as cq
import trimesh

HERE = Path(__file__).resolve().parent
P = json.loads((HERE/'parameters.json').read_text())
B = json.loads((HERE/'board_features.json').read_text())
F = B['features']
OUT = HERE/'exports'; OUT.mkdir(exist_ok=True)
REF = OUT/'reference'; REF.mkdir(exist_ok=True)
W, D = B['width_mm'], B['depth_mm']
PCB_Z = P['pcb_bottom_z_mm']
PCB_TOP = PCB_Z + B['thickness_mm']
H = P['case_height_mm']; TOP_IN = H-P['lid_thickness_mm']
EDGE = P['pcb_edge_clearance_mm']; WALL = P['wall_mm']
MOUNTS = [F[k]['center'] for k in ['H1','H2','H3','H4']]
JACK_Z = PCB_TOP+P['jack_axis_above_pcb_mm']
SEAM = JACK_Z
BOSS_R = P['corner_boss_radius_mm']
PILOT = P['insert_pilot_diameter_mm']
CLEAR = P['screw_clearance_diameter_mm']
OLED = F['J5']['center']
OLED_BOTTOM = PCB_TOP+P['oled_spacer_height_mm']
OLED_TOP = OLED_BOTTOM+P['oled_pcb_thickness_mm']
OLED_HOLES = [p['center'] for p in F['J5']['pads'] if p['number']=='']
OLED_AA_Y = OLED[1]+27.8/2-P['oled_active_area_offset_from_module_top_mm']-10.864/2
WINDOWS = [dict(name='sound_window',x=W/2,y=45.5,w=73.,d=12.,r=6.,z=TOP_IN),
           dict(name='oled_window',x=OLED[0],y=OLED_AA_Y,w=26.,d=17.,r=1.5,z=TOP_IN-P['oled_window_recess_mm'])]
OLED_RETAIN = []
BUTTONS = [F[k]['center'] for k in ['SW5','SW6','SW7']]

def box(x,y,z,w,d,h):
    return cq.Workplane('XY').box(w,d,h,centered=(True,True,False)).translate((x,y,z))

def rr(x,y,z,w,d,h,r):
    obj = box(x,y,z,w,d,h)
    return obj.edges('|Z').fillet(min(r,min(w,d)/2-.001)) if r else obj

def cyl(x,y,z,r,h):
    return cq.Workplane('XY').circle(r).extrude(h).translate((x,y,z))

def volume(obj):
    return sum(s.Volume() for s in obj.solids().vals())

def intersect(a,b):
    return volume(a.intersect(b))

def shell_ring(z,h,outer_pad,inner_pad):
    return rr(W/2,D/2,z,W+2*outer_pad,D+2*outer_pad,h,3+outer_pad).cut(
        rr(W/2,D/2,z-.1,W+2*inner_pad,D+2*inner_pad,h+.2,3+inner_pad))

# Base: PCB rests on four posts, with four screw counterbores underneath.
base = rr(W/2,D/2,0,W+2*(EDGE+WALL),D+2*(EDGE+WALL),SEAM,3+EDGE+WALL)
base = base.cut(rr(W/2,D/2,P['floor_mm'],W+2*EDGE,D+2*EDGE,SEAM+2,3+EDGE))
for x,y in MOUNTS:
    base = base.union(cyl(x,y,P['floor_mm']-.1,BOSS_R,PCB_Z-P['floor_mm']+.1))
    base = base.cut(cyl(x,y,-.1,CLEAR/2,PCB_Z+.2))
    base = base.cut(cyl(x,y,-.1,2.3,2.3))
tongue = shell_ring(SEAM-.7,2.0,EDGE+.2,-.4)
for x,y in MOUNTS:
    tongue = tongue.cut(cyl(x,y,SEAM-1, BOSS_R+.35,3))
for x,y in BUTTONS:
    tongue = tongue.cut(cyl(x,y,SEAM-1,4.25,3))
base = base.union(tongue)

# Lid: internal locating groove, corner posts, and M2 heat-set insert pockets.
lid = rr(W/2,D/2,SEAM,W+2*(EDGE+WALL),D+2*(EDGE+WALL),H-SEAM,3+EDGE+WALL)
lid = lid.cut(rr(W/2,D/2,SEAM-.1,W+2*EDGE,D+2*EDGE,TOP_IN-SEAM+.1,3+EDGE))
lid = lid.cut(rr(W/2,D/2,SEAM-.1,W+2*(EDGE+.2+P['tongue_clearance_mm']),
                 D+2*(EDGE+.2+P['tongue_clearance_mm']),1.65,3+EDGE+.2+P['tongue_clearance_mm']))
for x,y in MOUNTS:
    lid = lid.union(cyl(x,y,PCB_TOP,BOSS_R,TOP_IN-PCB_TOP+.2))
    lid = lid.cut(cyl(x,y,PCB_TOP-.05,PILOT/2,P['insert_length_mm']+.4))
    lid = lid.cut(cyl(x,y,PCB_TOP-.05,CLEAR/2,5.05))

# Openings are split by the seam. Large side holes allow plug shoulders to enter
# the 2.35 mm recess, rather than shortening TRS insertion depth.
port_cutters=[]
for ref in ['J2','J6','J7','J3','J8','J9']:
    x,y=F[ref]['center']
    port_cutters.append(cq.Workplane('YZ').center(y,JACK_Z).circle(
        P['jack_plug_clearance_diameter_mm']/2).extrude(110).translate((-10,0,0)))
usb_z=PCB_TOP+P['usb_axis_above_pcb_mm']
usb=box(F['J1']['center'][0],-2.0,usb_z-P['usb_plug_opening_height_mm']/2,
        P['usb_plug_opening_width_mm'],10,P['usb_plug_opening_height_mm']).edges('|Y').fillet(1.8)
port_cutters.append(usb)
for cutter in port_cutters:
    base=base.cut(cutter); lid=lid.cut(cutter)

# BOOT/RESET service slots; the board itself remains unchanged.
for ref in ['JP1','JP2']:
    x,y=F[ref]['center']
    base=base.cut(rr(x,y,-.1,3.4,2.2,P['floor_mm']+.2,.4))

# OLED remains a separate module on four 5 mm spacers above the main PCB.
# A 45-degree recessed bezel brings the open aperture closer to the screen.
retainers=[]
def tapered_rect(x,y,z,w,d,h,r):
    lower=rr(x,y,z,w,d,.01,r).faces('<Z').val().outerWire()
    upper=rr(x,y,z+h,w+2*h,d+2*h,.01,r+h).faces('<Z').val().outerWire()
    return cq.Workplane('XY').newObject([cq.Solid.makeLoft([lower,upper],True)])
well_z=TOP_IN-P['oled_window_recess_mm']
well=tapered_rect(OLED[0],OLED_AA_Y,well_z,34,20,P['oled_window_recess_mm']+.1,3)
lid=lid.union(well)
well_void=tapered_rect(OLED[0],OLED_AA_Y,well_z+P['lid_thickness_mm'],34,20,
                      P['oled_window_recess_mm']+.1,3)
lid=lid.cut(well_void)

# Long guided plungers: flange retains each cap, recessed annular foot limits
# overtravel against the switch cover (cover height 1.2 mm, nominal travel 0.2).
CAP_TIP=PCB_TOP+P['switch_height_mm']+P['button_max_up_gap_mm']
FLANGE_BOTTOM=CAP_TIP+.55
GUIDE_BOTTOM=FLANGE_BOTTOM+.8
caps=[]
for x,y in BUTTONS:
    tube=cyl(x,y,GUIDE_BOTTOM,4.,TOP_IN-GUIDE_BOTTOM+.2).cut(
        cyl(x,y,GUIDE_BOTTOM-.1,P['button_guide_diameter_mm']/2,TOP_IN-GUIDE_BOTTOM+.4))
    lid=lid.union(tube)
    lid=lid.cut(cyl(x,y,GUIDE_BOTTOM-.1,P['button_guide_diameter_mm']/2,H-GUIDE_BOTTOM+.3))
    cap=cyl(x,y,GUIDE_BOTTOM,P['button_head_diameter_mm']/2,H+P['button_protrusion_mm']-GUIDE_BOTTOM)
    cap=cap.union(cyl(x,y,FLANGE_BOTTOM,3.7,.8))
    foot=cyl(x,y,CAP_TIP,2.35,.55).cut(cyl(x,y,CAP_TIP-.01,1.2,.26))
    cap=cap.union(foot).union(cyl(x,y,CAP_TIP,1.,.65))
    caps.append(cap)

# Direct LED viewing slot in the front wall; no light pipe or clear material.
led_x,led_y=F['D2']['center']
led_slot=box(led_x,-1.5,PCB_TOP-.3,4.,6.,2.).edges('|Y').fillet(.7)
base=base.cut(led_slot)

# Open cutouts: no cover sheets, retaining pockets, or adhesive recesses.
for v in WINDOWS:
    opening=rr(v['x'],v['y'],v['z']-.1,v['w'],v['d'],H-v['z']+.2,v['r'])
    lid=lid.cut(opening)
base=base.clean(); lid=lid.clean()

# Insert-fit coupon; order of holes is documented in the separate drawing.
coupon=rr(29,8,0,58,16,5,2)
for i,dia in enumerate([2.9,3.0,3.1,3.2,3.3]):
    coupon=coupon.cut(cyl(5+10*i,5,1.6,dia/2,3.5))
for i,dia in enumerate([5.7,5.9,6.1]):
    coupon=coupon.cut(cyl(15+14*i,11,-.1,dia/2,5.2))

# Reference board and component envelopes. Exact XY comes from KiCad; heights
# and DIP/socket dimensions are documented assumptions, not scanned hardware.
pcb=rr(W/2,D/2,PCB_Z,W,D,B['thickness_mm'],3)
for x,y in MOUNTS:
    pcb=pcb.cut(cyl(x,y,PCB_Z-.1,1.1,B['thickness_mm']+.2))
for x,y in OLED_HOLES:
    pcb=pcb.cut(cyl(x,y,PCB_Z-.1,1.25,B['thickness_mm']+.2))
components=[]; visuals=[]
for ref in ['U10','U11','U12']:
    pts=[p['center'] for p in F[ref]['pads']]
    xmin=min(p[0] for p in pts)-1.4; xmax=max(p[0] for p in pts)+1.4
    yc=sum(p[1] for p in pts)/len(pts); xc=(xmin+xmax)/2
    env=box(xc,yc,PCB_TOP,xmax-xmin,10.3,P['dip_socket_and_ic_max_height_mm'])
    components.append((ref,env))
    visuals.append((ref+'_socket',box(xc,yc,PCB_TOP,xmax-xmin,10.3,4.5)))
    visuals.append((ref+'_chip',rr(xc,yc,PCB_TOP+4.5,xmax-xmin,6.35,4.0,.4)))
    for lead_index,p in enumerate(pts):
        visuals.append((ref+'_lead_'+str(lead_index),box(p[0],p[1],PCB_TOP+3.0,.55,.55,2.2)))
for ref in ['J2','J6','J7','J3','J8','J9']:
    x,y=F[ref]['center']; left=x<W/2
    xc=8.0 if left else W-8.0
    comp=box(xc,y,PCB_TOP,12.1,6.1,5.0)
    front=(cq.Workplane('YZ').center(y,JACK_Z).circle(2.5).extrude(2.0)
           .translate((-.05 if left else W-1.95,0,0)))
    comp=comp.union(front)
    components.append((ref,comp)); visuals.append((ref,comp))
usb_comp=box(F['J1']['center'][0],3.65,PCB_TOP,8.94,7.3,3.26)
components.append(('J1',usb_comp)); visuals.append(('J1',usb_comp))
for ref in ['SW5','SW6','SW7']:
    x,y=F[ref]['center']; comp=box(x,y,PCB_TOP,5.1,5.1,1.2).union(cyl(x,y,PCB_TOP+1.2,1.,.3))
    components.append((ref,comp)); visuals.append((ref,comp))
# Header is optional; include its height for enclosure fit.
components.append(('J4',box(11.81,4,PCB_TOP,10.16,2.54,8.5)))
for ref,entry in F.items():
    if 'fab_bounds' in entry:
        xmin,ymin,xmax,ymax=entry['fab_bounds']; w=xmax-xmin; d=ymax-ymin
        if 0<w<22 and 0<d<22:
            h=3.3 if ref=='L1' else 4.5 if ref=='U13' else 1.0 if ref.startswith(('R','C')) else 1.3 if ref.startswith(('D','Q')) else 2.0
            comp=box((xmin+xmax)/2,(ymin+ymax)/2,PCB_TOP,w,d,h)
            components.append((ref,comp)); visuals.append((ref,comp))
# OLED PCB with actual mounting holes; front/back envelope avoids edge-clip areas.
oled_pcb=box(OLED[0],OLED[1],OLED_BOTTOM,27.3,27.8,P['oled_pcb_thickness_mm'])
for x,y in OLED_HOLES:
    oled_pcb=oled_pcb.cut(cyl(x,y,OLED_BOTTOM-.1,1.25,P['oled_pcb_thickness_mm']+.2))
oled_glass=box(OLED[0],OLED[1]+13.9-4.87-16.9/2,OLED_TOP,24.74,16.9,P['oled_front_height_above_pcb_mm'])
oled_back=box(OLED[0],OLED[1],OLED_BOTTOM-P['oled_back_component_height_mm'],23,24,P['oled_back_component_height_mm'])
components.extend([('OLED PCB',oled_pcb),('OLED glass',oled_glass),('OLED back',oled_back)])
for index,(x,y) in enumerate(OLED_HOLES):
    spacer=cyl(x,y,PCB_TOP,2.05,P['oled_spacer_height_mm']).cut(cyl(x,y,PCB_TOP-.1,1.,P['oled_spacer_height_mm']+.2))
    components.append(('OLED spacer '+str(index+1),spacer))
    visuals.append(('OLED_spacer_'+str(index+1),spacer))
    components.append(('OLED screw '+str(index+1),cyl(x,y,OLED_TOP,1.9,1.5)))
    components.append(('OLED bottom screw '+str(index+1),cyl(x,y,PCB_Z-1.5,1.9,1.5)))

visuals.extend([('OLED_PCB',oled_pcb),('OLED_glass',oled_glass)])

# Mechanical collision audit excludes intentional mating-face contact (zero volume).
checks={}
checks['case_halves_overlap_mm3']=round(intersect(base,lid),6)
checks['pcb_case_overlap_mm3']=round(intersect(pcb,base)+intersect(pcb,lid),6)
collisions=[]
for name,component in components:
    v=intersect(component,base)+intersect(component,lid)
    for ret in retainers: v+=intersect(component,ret)
    if v>1e-5: collisions.append(dict(component=name,volume_mm3=round(v,6)))
checks['component_case_collisions']=collisions
checks['additional_transparent_parts']=0
checks['open_apertures_mm']=[{k:v[k] for k in ['name','w','d','r']} for v in WINDOWS]
checks['cap_case_overlap_mm3']=[round(intersect(c,lid)+intersect(c,base),6) for c in caps]
cap_motion=CAP_TIP-(PCB_TOP+1.2)
checks['cap_pressed_case_overlap_mm3']=[round(intersect(c.translate((0,0,-cap_motion)),lid)+intersect(c.translate((0,0,-cap_motion)),base)+intersect(c.translate((0,0,-cap_motion)),pcb),6) for c in caps]
checks['cap_neighbor_collisions']=[]
for i,cap in enumerate(caps):
    moved=cap.translate((0,0,-cap_motion))
    for name,component in components:
        if name.startswith('SW'): continue
        v=intersect(moved,component)
        if v>1e-5: checks['cap_neighbor_collisions'].append(dict(button=i+1,component=name,volume_mm3=round(v,6)))

checks['dip_clearance_to_lid_underside_mm']=round(TOP_IN-PCB_TOP-P['dip_socket_and_ic_max_height_mm'],3)
checks['oled_clearance_to_bezel_underside_mm']=round(well_z-OLED_TOP-P['oled_front_height_above_pcb_mm'],3)
checks['oled_spacer_height_mm']=P['oled_spacer_height_mm']
checks['pcb_tail_to_floor_clearance_mm']=round(PCB_Z-3.0-P['floor_mm'],3)
checks['button_max_up_gap_mm']=P['button_max_up_gap_mm']
checks['button_total_motion_to_cover_stop_mm']=round(CAP_TIP-(PCB_TOP+1.2),3)
checks['pcb_corner_hole_centers_mm']=MOUNTS
checks['oled_active_area_center_mm']=[OLED[0],round(OLED_AA_Y,4)]
checks['case_outer_dimensions_mm']=[round(W+2*(EDGE+WALL),3),round(D+2*(EDGE+WALL),3),H]
checks['insert_count']=4
checks['notes']=['XY datums extracted from the KiCad PCB; height envelopes are documented assumptions.',
                 'Actual insert fit, socket height, cable shoulder diameter and switch travel require a prototype.']

# Export solid assembly and individual, bed-oriented print parts.
parts=[('base',base),('lid',lid),('button_cap',caps[0]),('fit_coupon',coupon)]
mesh_report={}
for name,obj in parts:
    assert obj.val().isValid(),name+' invalid BREP'
    assert len(obj.solids().vals())==1,name+' disconnected solids'
    if name in ['lid','button_cap']:
        oriented=obj.rotate((0,0,0),(1,0,0),180)
    else: oriented=obj
    bb=oriented.val().BoundingBox()
    oriented=oriented.translate((-bb.xmin,-bb.ymin,-bb.zmin))
    cq.exporters.export(oriented,str(OUT/(name+'.stl')),tolerance=.025,angularTolerance=.08)
    cq.exporters.export(obj,str(OUT/(name+'.step')))
    cq.exporters.export(obj,str(REF/(name+'.stl')),tolerance=.04,angularTolerance=.1)
    mesh=trimesh.load(str(OUT/(name+'.stl')),force='mesh')
    mesh_report[name]=dict(watertight=bool(mesh.is_watertight),winding_consistent=bool(mesh.is_winding_consistent),
        volume_mm3=round(float(mesh.volume),3),dimensions_mm=[round(float(v),3) for v in mesh.extents],
        connected_components=len(mesh.split(only_watertight=False)),triangles=len(mesh.faces))
    assert mesh.is_watertight and mesh.is_winding_consistent and mesh.volume>0,name+' invalid STL'
    assert mesh_report[name]['connected_components']==1,name+' disconnected mesh'
assembly=cq.Assembly(name='Chiptune_Enclosure')
for name,obj in [('base',base),('lid',lid),('pcb',pcb)]:
    assembly.add(obj,name=name)
for i,obj in enumerate(caps): assembly.add(obj,name='button_'+str(i+1))
for name,obj in visuals: assembly.add(obj,name=name)
assembly.export(str(OUT/'assembly.step'))
cq.exporters.export(pcb,str(REF/'pcb.stl'),tolerance=.04,angularTolerance=.1)
for name,obj in visuals:
    cq.exporters.export(obj,str(REF/(name+'.stl')),tolerance=.06,angularTolerance=.15)
checks['print_meshes']=mesh_report
checks['pass']=checks['case_halves_overlap_mm3']<1e-5 and checks['pcb_case_overlap_mm3']<1e-5 and not collisions and all(v<1e-5 for v in checks['cap_case_overlap_mm3']+checks['cap_pressed_case_overlap_mm3']) and not checks['cap_neighbor_collisions']
(HERE/'mechanical_verification.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2)+'\n')
(HERE/'assembly_metadata.json').write_text(json.dumps(dict(parameters=P,windows=WINDOWS,
    pcb_top_z=PCB_TOP,seam_z=SEAM,cap_tip_z=CAP_TIP,guide_bottom_z=GUIDE_BOTTOM,
    oled_active_y=OLED_AA_Y,oled_holes=OLED_HOLES,oled_retainers=OLED_RETAIN,
    visual_components=[name for name,obj in visuals]),indent=2)+'\n')
print(json.dumps(checks,indent=2))
assert checks['pass'],'Mechanical interference requires correction'
