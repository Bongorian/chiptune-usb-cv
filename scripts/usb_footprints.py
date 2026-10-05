from pathlib import Path

def write_interfaces(lib):
    s='(footprint "OLED_HS96_INTERFACE" (version 20241229) (generator "pcbnew") (layer "F.Cu") (attr through_hole)\n(property "Reference" "REF**" (at 0 0) (layer "F.Fab") (effects (font (size 1 1))))\n(property "Value" "OLED interface" (at 0 3) (layer "F.Fab") (effects (font (size 1 1))))\n'
    s+='(fp_rect (start -13.65 -13.9) (end 13.65 13.9) (stroke (width 0.15) (type default)) (fill none) (layer "Dwgs.User"))\n'
    for i,x in enumerate([-3.81,-1.27,1.27,3.81],1):
        s+=f'(pad "{i}" thru_hole circle (at {x} -12.58) (size 1.7 1.7) (drill 1) (layers "*.Cu" "*.Mask"))\n'
    s+='(fp_rect (start -4.8 -13.6) (end 4.8 -11.5) (stroke (width 0.05) (type default)) (fill none) (layer "F.CrtYd"))\n'
    for x in [-11.65,11.65]:
        for y in [-11.9,11.9]:
            s+=f'(pad "" np_thru_hole circle (at {x} {y}) (size 2.5 2.5) (drill 2.5) (layers "*.Cu" "*.Mask"))\n(fp_circle (center {x} {y}) (end {x+2.05} {y}) (stroke (width 0.05) (type default)) (fill none) (layer "F.CrtYd"))\n'
    (lib/'OLED_HS96_INTERFACE.kicad_mod').write_text(s+')\n')
    s='(footprint "SERVICE_PADS" (version 20241229) (generator "pcbnew") (layer "F.Cu") (attr smd)\n(property "Reference" "REF**" (at 0 -1) (layer "F.Fab") (effects (font (size 0.8 0.8))))\n(property "Value" "SERVICE" (at 0 1) (layer "F.Fab") (effects (font (size 0.8 0.8))))\n'
    for n,x in [(1,-.75),(2,.75)]:s+=f'(pad "{n}" smd roundrect (at {x} 0) (size 1 1.5) (layers "F.Cu" "F.Mask") (roundrect_rratio 0.2))\n'
    (lib/'SERVICE_PADS.kicad_mod').write_text(s+')\n')
