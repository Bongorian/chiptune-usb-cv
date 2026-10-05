#!/usr/bin/env python3
"""Remove native-DRC-reported dead ends; trim protected escape tails at junctions."""
import sys,json,re
from pathlib import Path
sys.path.append('/usr/lib/python3/dist-packages')
import pcbnew as p
from shapely.geometry import Point,LineString
from shapely.ops import nearest_points
from routing_rules import apply_to_board
src=Path(sys.argv[1]);report=json.loads(Path(sys.argv[2]).read_text());out=Path(sys.argv[3]);b=p.LoadBoard(str(src));apply_to_board(b)
def xy(v):return (p.ToMM(v.x),p.ToMM(v.y))
def vec(pt):return p.VECTOR2I(p.FromMM(round(pt[0],6)),p.FromMM(round(pt[1],6)))
tracks=list(b.GetTracks());byid={t.m_Uuid.AsString():t for t in tracks};pads=[pd for f in b.GetFootprints() for pd in f.Pads()];removed=set();trimmed=[]
for violation in report['violations']:
 if violation['type']!='via_dangling':continue
 via=byid[violation['items'][0]['uuid']];centre=Point(xy(via.GetPosition()));radius=p.ToMM(via.GetWidth(p.F_Cu))/2
 contacts=[]
 for track in tracks:
  if isinstance(track,p.PCB_VIA) or track.GetNetCode()!=via.GetNetCode():continue
  line=LineString([xy(track.GetStart()),xy(track.GetEnd())])
  if line.distance(centre)<radius+p.ToMM(track.GetWidth())/2-.001:contacts.append((track,line))
 active={track.GetLayer() for track,line in contacts}
 if len(active)>1:raise RuntimeError('Reported dangling via has tracks on multiple layers: '+via.m_Uuid.AsString())
 for track,line in contacts:
  distance=line.project(centre);point=line.interpolate(distance)
  if point.distance(centre)<.000001:continue
  if distance<.000001:track.SetStart(via.GetPosition())
  elif line.length-distance<.000001:track.SetEnd(via.GetPosition())
  else:
   branch=p.PCB_TRACK(b);branch.SetStart(vec(point.coords[0]));branch.SetEnd(via.GetPosition());branch.SetWidth(track.GetWidth());branch.SetLayer(track.GetLayer());branch.SetNet(via.GetNet());b.Add(branch)
for v in report['violations']:
 if v['type']=='via_dangling':removed.update(i['uuid'] for i in v['items'])
 if v['type']=='starved_thermal':
  ids={i['uuid'] for i in v['items']}
  for pad in pads:
   if pad.m_Uuid.AsString() in ids:pad.SetLocalZoneConnection(p.ZONE_CONNECTION_FULL)
for v in report['violations']:
 if v['type']!='track_dangling':continue
 for item in v['items']:
  t=byid[item['uuid']]
  if isinstance(t,p.PCB_VIA):continue
  if p.ToMM(t.GetLength())<=.4:removed.add(item['uuid']);continue
  line=LineString([xy(t.GetStart()),xy(t.GetEnd())]);hits=[]
  for other in tracks:
   if other==t or other.GetNetCode()!=t.GetNetCode() or other.m_Uuid.AsString() in removed:continue
   if isinstance(other,p.PCB_VIA):
    point=Point(xy(other.GetPosition()))
    if line.distance(point)<p.ToMM(other.GetWidth(p.F_Cu))/2:hits.append(line.project(point))
   elif other.GetLayer()==t.GetLayer():
    otherline=LineString([xy(other.GetStart()),xy(other.GetEnd())]);hit=line.intersection(otherline.buffer(.000002,cap_style=2))
    if hit.is_empty:
     if line.distance(otherline)<p.ToMM(other.GetWidth())/2-.000001:hits.append(line.project(nearest_points(line,otherline)[0]))
     continue
    if hit.geom_type=='Point':hits.append(line.project(hit))
    elif hit.geom_type=='LineString':hits.extend(line.project(Point(c)) for c in hit.coords)
    elif hasattr(hit,'geoms'):
     for geom in hit.geoms:
      if geom.geom_type=='Point':hits.append(line.project(geom))
      elif geom.geom_type=='LineString':hits.extend(line.project(Point(c)) for c in geom.coords)
  for pad in pads:
   if pad.GetNetCode()==t.GetNetCode() and pad.IsOnLayer(t.GetLayer()):
    point=Point(xy(pad.GetPosition()))
    if line.distance(point)<.00001 and line.project(point)<=line.length:hits.append(line.project(point))
  if len(hits)<2 or max(hits)-min(hits)<.00001:removed.add(item['uuid']);continue
  a=max(0,min(hits));c=min(line.length,max(hits));t.SetStart(vec(line.interpolate(a).coords[0]));t.SetEnd(vec(line.interpolate(c).coords[0]));trimmed.append((t.GetNetname(),round(line.length,4),round(c-a,4)))
b.BuildConnectivity();p.ZONE_FILLER(b).Fill(b.Zones());p.SaveBoard(str(out),b)
s=out.read_text()
def filt(m):
 uid=re.search(r'\(uuid "([^"]+)"\)',m[0]);return '' if uid and uid[1] in removed else m[0]
out.write_text(re.sub(r'^\t\((?:segment|via)\n.*?^\t\)\n',filt,s,flags=re.M|re.S))
print('Removed',len(removed),'unused copper objects; trimmed',trimmed)
