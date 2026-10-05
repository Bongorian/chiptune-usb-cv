#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <queue>
#include <vector>
#include <limits>
struct Q {float f,g;int i;bool operator<(const Q&o)const{return f>o.f;}};
int main(int argc,char**argv){
 std::ifstream f(argv[1],std::ios::binary);int w,h,n,nlayers;f.read((char*)&w,4);f.read((char*)&h,4);f.read((char*)&nlayers,4);f.read((char*)&n,4);int area=w*h,total=area*nlayers;
 std::vector<int> starts(n);f.read((char*)starts.data(),4*n);
 std::vector<uint8_t> blocked(total),goal(total),hop(area);std::vector<float> heur(area),penalty(total);
 f.read((char*)blocked.data(),total);f.read((char*)goal.data(),total);f.read((char*)hop.data(),area);f.read((char*)heur.data(),4*area);
 f.read((char*)penalty.data(),4*total);
 std::vector<float> d(total,std::numeric_limits<float>::infinity());std::vector<int> prev(total,-1),lastvia(total,-1);std::priority_queue<Q> q;
 for(int i:starts){d[i]=0;prev[i]=-2;q.push({heur[i%area],0,i});}
 int dx[]={1,0,-1,0,1,-1,-1,1},dy[]={0,1,0,-1,1,1,-1,-1};int end=-1;size_t expanded=0;
 while(!q.empty()){
  Q a=q.top();q.pop();if(a.g>d[a.i]+.0001f)continue;if(goal[a.i]){end=a.i;break;}expanded++;
  int base=a.i/area,xy=a.i%area,x=xy%w,y=xy/w;
  for(int k=0;k<8;k++){
   int xx=x+dx[k],yy=y+dy[k];if(xx<0||yy<0||xx>=w||yy>=h)continue;int j=base*area+yy*w+xx;if(blocked[j])continue;
   if(k>=4 && (blocked[base*area+y*w+xx]||blocked[base*area+yy*w+x]))continue;
   float cost=k<4?1.f:1.41421356f;if(prev[a.i]>=0 && abs(a.i-prev[a.i])!=abs(j-a.i))cost+=.05f;
   float g=a.g+cost+penalty[j];if(g<d[j]){d[j]=g;prev[j]=a.i;lastvia[j]=lastvia[a.i];q.push({g+heur[j%area],g,j});}
  }
  if(hop[xy]){for(int dest=0;dest<nlayers;dest++){if(dest==base)continue;int j=dest*area+xy;bool near=false;int lv=lastvia[a.i];if(hop[xy]==1 && lv>=0){int vx=lv%w,vy=lv/w;near=(x-vx)*(x-vx)+(y-vy)*(y-vy)<196;}if(!blocked[j]&&!near){float g=a.g+(hop[xy]==2?1.f:60.f)+penalty[j];if(g<d[j]){d[j]=g;prev[j]=a.i;lastvia[j]=hop[xy]==1?xy:lv;q.push({g+heur[xy],g,j});}}}}
 }
 std::ofstream out(argv[2]);if(end<0){out<<"FAIL "<<expanded<<"\n";return 2;}
 std::vector<int> path;for(int i=end;i>=0;i=prev[i])path.push_back(i);std::reverse(path.begin(),path.end());out<<"OK "<<expanded<<" "<<path.size()<<"\n";
 for(int i:path)out<<i/area<<" "<<(i%area)%w<<" "<<(i%area)/w<<"\n";
}
