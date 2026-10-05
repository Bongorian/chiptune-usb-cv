#!/usr/bin/python3
"""Make a display-only copy with readable refdes; never use it for fabrication."""
import sys
import pcbnew

board = pcbnew.LoadBoard(sys.argv[1])
for footprint in board.GetFootprints():
    reference = footprint.Reference()
    labels = [item for item in footprint.GraphicalItems()
              if hasattr(item, 'GetText') and item.GetText() == '${REFERENCE}']
    if labels or footprint.GetReference().startswith('H'):
        # Existing in-body labels suffice; hide the duplicate outside labels.
        reference.SetVisible(False)
        for label in labels:
            label.SetTextAngle(pcbnew.EDA_ANGLE(0, pcbnew.DEGREES_T))
        if footprint.GetReference() in {'D1', 'D5', 'D6'}:
            label = labels[0]
            position = footprint.GetPosition()
            offset = (-2.1, 0) if footprint.GetReference() in {'D1', 'D5'} else (0, -1.6)
            label.SetPosition(pcbnew.VECTOR2I(position.x + pcbnew.FromMM(offset[0]),
                                              position.y + pcbnew.FromMM(offset[1])))
            label.SetTextAngle(pcbnew.EDA_ANGLE(0, pcbnew.DEGREES_T))
            label.SetTextSize(pcbnew.VECTOR2I(pcbnew.FromMM(0.55), pcbnew.FromMM(0.55)))
    else:
        reference.SetVisible(True)
        reference.SetLayer(pcbnew.B_Fab if footprint.GetLayer() == pcbnew.B_Cu
                           else pcbnew.F_Fab)
        reference.SetPosition(footprint.GetPosition())
        reference.SetTextAngle(pcbnew.EDA_ANGLE(0, pcbnew.DEGREES_T))
        reference.SetTextSize(pcbnew.VECTOR2I(pcbnew.FromMM(0.8), pcbnew.FromMM(0.8)))
        if footprint.GetReference() == 'J5':
            pads = [pad for pad in footprint.Pads() if pad.GetNumber()]
            reference.SetPosition(pcbnew.VECTOR2I(
                round(sum(p.GetPosition().x for p in pads) / len(pads)),
                round(sum(p.GetPosition().y for p in pads) / len(pads)) - pcbnew.FromMM(1.4)))
pcbnew.SaveBoard(sys.argv[2], board)
