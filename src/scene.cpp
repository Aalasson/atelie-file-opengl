#include "core.hpp"
#include <initializer_list>
#include <utility>

namespace atelie {
namespace {
// These mesh slots belong to this module. All geometry is generated locally;
// no scene, mesh, texture coordinate or animation is loaded from Godot.
enum MeshSlot {
    Cube, Sphere, Cylinder, Torus, Spool, Basket, Shade, Blade,
    Shirt, Hips, Face, Hair, Apron, Thigh, Shin, Sleeve, Forearm,
    Winding, CellWeave, BasketWeave, Flowerpot
};
constexpr float tau = 2.f*pi;
struct Ring { float y,rx,rz,z=0; };
struct Profile { float y,r; };

Material mat(Vec3 color,int texture=Plain,float specular=.12f,float shine=24.f,
             float uvScale=1.f,float emission=0.f) {
    Material m; m.color=color; m.texture=texture; m.specular=specular;
    m.shininess=shine; m.uvScale=uvScale; m.emission=emission; return m;
}
const Material oak=mat({.66f,.43f,.25f},Wood,.19f,36.f,2.f);
const Material paleOak=mat({.83f,.65f,.43f},Wood,.16f,32.f,2.f);
const Material darkOak=mat({.34f,.23f,.16f},Wood,.13f,24.f,2.f);
const Material brass=mat({.73f,.53f,.27f},Plain,.72f,80.f);
const Material steel=mat({.73f,.76f,.75f},Plain,.85f,112.f);
const Material cream=mat({.90f,.85f,.72f},Fabric,.06f,16.f,3.f);
const std::array<Vec3,3> yarnColors{{{.84f,.31f,.24f},{.20f,.47f,.64f},{.93f,.67f,.21f}}};

void triangle(MeshData& m,std::uint32_t a,std::uint32_t b,std::uint32_t c) {
    m.indices.insert(m.indices.end(),{a,b,c});
}
void quad(MeshData& m,Vec3 a,Vec3 b,Vec3 c,Vec3 d,Vec3 n) {
    auto first=static_cast<std::uint32_t>(m.vertices.size());
    m.vertices.insert(m.vertices.end(),{{a,n,{0,0}},{b,n,{1,0}},{c,n,{1,1}},{d,n,{0,1}}});
    triangle(m,first,first+1,first+2); triangle(m,first,first+2,first+3);
}
MeshData cubeMesh() {
    MeshData m;
    quad(m,{-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f},{0,0,1});
    quad(m,{.5f,-.5f,-.5f},{-.5f,-.5f,-.5f},{-.5f,.5f,-.5f},{.5f,.5f,-.5f},{0,0,-1});
    quad(m,{.5f,-.5f,.5f},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{.5f,.5f,.5f},{1,0,0});
    quad(m,{-.5f,-.5f,-.5f},{-.5f,-.5f,.5f},{-.5f,.5f,.5f},{-.5f,.5f,-.5f},{-1,0,0});
    quad(m,{-.5f,.5f,.5f},{.5f,.5f,.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f},{0,1,0});
    quad(m,{-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,-.5f,.5f},{-.5f,-.5f,.5f},{0,-1,0});
    return m;
}
MeshData sphereMesh(int slices=28,int stacks=18) {
    MeshData m;
    for(int j=0;j<=stacks;++j) {
        float v=float(j)/stacks,theta=v*pi;
        for(int i=0;i<=slices;++i) {
            float u=float(i)/slices,phi=u*tau;
            Vec3 n{std::sin(theta)*std::cos(phi),std::cos(theta),std::sin(theta)*std::sin(phi)};
            m.vertices.push_back({n*.5f,n,{u,1-v}});
        }
    }
    for(int j=0;j<stacks;++j)for(int i=0;i<slices;++i) {
        auto a=std::uint32_t(j*(slices+1)+i),b=a+slices+1;
        // Skip the coincident pole triangles rather than submit zero areas.
        if(j!=0) triangle(m,a,a+1,b);
        if(j!=stacks-1) triangle(m,a+1,b+1,b);
    }
    return m;
}
MeshData lathe(const std::vector<Profile>& p,int slices=32,bool caps=false) {
    MeshData m;
    float extent=0;
    for(std::size_t j=1;j<p.size();++j)extent+=std::hypot(p[j].y-p[j-1].y,p[j].r-p[j-1].r);
    float distance=0;
    for(std::size_t j=0;j+1<p.size();++j) {
        const auto before=p[j],after=p[j+1];
        float dy=after.y-before.y,dr=after.r-before.r,span=std::hypot(dy,dr);
        auto first=static_cast<std::uint32_t>(m.vertices.size());
        // Separate vertices at profile corners preserve flat lips/flanges and
        // avoid averaging the inner and outer normals of a thin shade rim.
        for(int row=0;row<2;++row)for(int i=0;i<=slices;++i) {
            const auto r=row?after:before;
            float u=float(i)/slices,a=u*tau,c=std::cos(a),s=std::sin(a);
            m.vertices.push_back({{r.r*c,r.y,r.r*s},normalized({dy*c,-dr,dy*s}),
                                  {u,(distance+row*span)/std::max(extent,.001f)}});
        }
        for(int i=0;i<slices;++i) {
            auto a=first+i,b=a+slices+1;
            triangle(m,a,b,b+1); triangle(m,a,b+1,a+1);
        }
        distance+=span;
    }
    if(caps)for(int end=0;end<2;++end) {
        const auto r=end?p.back():p.front(); float ny=end?1.f:-1.f;
        auto center=static_cast<std::uint32_t>(m.vertices.size());
        m.vertices.push_back({{0,r.y,0},{0,ny,0},{.5f,.5f}});
        for(int i=0;i<=slices;++i) {
            float a=tau*float(i)/slices,c=std::cos(a),s=std::sin(a);
            m.vertices.push_back({{r.r*c,r.y,r.r*s},{0,ny,0},{.5f+.5f*c,.5f+.5f*s}});
        }
        for(int i=0;i<slices;++i) {
            if(end)triangle(m,center,center+i+2,center+i+1);
            else triangle(m,center,center+i+1,center+i+2);
        }
    }
    return m;
}
MeshData torusMesh(int rings=36,int sides=10) {
    MeshData m; constexpr float major=.42f,minor=.08f;
    for(int i=0;i<=rings;++i)for(int j=0;j<=sides;++j) {
        float u=tau*float(i)/rings,v=tau*float(j)/sides;
        float c=std::cos(u),s=std::sin(u),cv=std::cos(v),sv=std::sin(v);
        m.vertices.push_back({{(major+minor*cv)*c,minor*sv,(major+minor*cv)*s},
                              {cv*c,sv,cv*s},{float(i)/rings,float(j)/sides}});
    }
    for(int i=0;i<rings;++i)for(int j=0;j<sides;++j) {
        auto a=std::uint32_t(i*(sides+1)+j),b=a+sides+1;
        triangle(m,a,a+1,b); triangle(m,a+1,b+1,b);
    }
    return m;
}
MeshData loft(std::initializer_list<Ring> list,int sides=24) {
    std::vector<Ring> p(list); MeshData m;
    const float range=std::max(.001f,p.back().y-p.front().y);
    for(std::size_t j=0;j<p.size();++j) {
        const Ring before=p[j?j-1:j],after=p[j+1<p.size()?j+1:j];
        float dy=after.y-before.y,dx=after.rx-before.rx,dz=after.rz-before.rz,shift=after.z-before.z;
        for(int i=0;i<=sides;++i) {
            float u=float(i)/sides,a=u*tau,c=std::cos(a),s=std::sin(a);
            Vec3 n=normalized({dy*c*p[j].rz,
                -dx*c*c*p[j].rz-dz*s*s*p[j].rx-shift*s*p[j].rx,dy*s*p[j].rx});
            m.vertices.push_back({{c*p[j].rx,p[j].y,s*p[j].rz+p[j].z},n,
                                  {u,(p[j].y-p.front().y)/range}});
        }
    }
    for(std::size_t j=0;j+1<p.size();++j)for(int i=0;i<sides;++i) {
        auto a=std::uint32_t(j*(sides+1)+i),b=a+sides+1;
        triangle(m,a,b,b+1); triangle(m,a,b+1,a+1);
    }
    // Planar caps close the torso and limbs at their attachment points.
    for(int end=0;end<2;++end) {
        const Ring r=end?p.back():p.front();float ny=end?1.f:-1.f;
        auto center=static_cast<std::uint32_t>(m.vertices.size());
        m.vertices.push_back({{0,r.y,r.z},{0,ny,0},{.5f,.5f}});
        for(int i=0;i<=sides;++i) {
            float a=tau*float(i)/sides,c=std::cos(a),s=std::sin(a);
            m.vertices.push_back({{r.rx*c,r.y,r.rz*s+r.z},{0,ny,0},{.5f+.5f*c,.5f+.5f*s}});
        }
        for(int i=0;i<sides;++i) {
            if(end)triangle(m,center,center+i+2,center+i+1);
            else triangle(m,center,center+i+1,center+i+2);
        }
    }
    return m;
}
// Append a tubular path. A repeated end point closes a loop. Each path has its
// own seam and vertex normals; no dependency on GL line width is required.
void tube(MeshData& m,const std::vector<Vec3>& path,float radius,int sides=7) {
    if(path.size()<2)return;
    const bool closed=length(path.front()-path.back())<1e-6f;
    auto first=static_cast<std::uint32_t>(m.vertices.size());float along=0;
    for(std::size_t i=0;i<path.size();++i) {
        if(i)along+=length(path[i]-path[i-1]);
        Vec3 before=i?path[i-1]:(closed?path[path.size()-2]:path.front());
        Vec3 after=i+1<path.size()?path[i+1]:(closed?path[1]:path.back());
        Vec3 tangent=normalized(after-before),helper=std::abs(tangent.y)>.9f?Vec3{1,0,0}:Vec3{0,1,0};
        Vec3 right=normalized(cross(helper,tangent)),up=cross(tangent,right);
        for(int j=0;j<=sides;++j) {
            float a=tau*float(j)/sides; Vec3 n=right*std::cos(a)+up*std::sin(a);
            m.vertices.push_back({path[i]+n*radius,n,{float(j)/sides,along}});
        }
    }
    for(std::size_t i=0;i+1<path.size();++i)for(int j=0;j<sides;++j) {
        auto a=first+std::uint32_t(i*(sides+1)+j),b=a+sides+1;
        triangle(m,a,a+1,b); triangle(m,a+1,b+1,b);
    }
}
MeshData windingMesh() {
    MeshData m;std::vector<Vec3> path;
    // A spiral following the sphere surface, plus four oblique windings.
    for(int i=0;i<=360;++i) {
        float t=float(i)/360,y=-.478f+.956f*t,a=t*tau*11;
        float r=std::sqrt(std::max(0.f,.505f*.505f-y*y));
        path.push_back({r*std::cos(a),y,r*std::sin(a)});
    }
    tube(m,path,.0105f,6);
    for(int coil=0;coil<4;++coil) {
        path.clear();float tilt=.45f+coil*.54f;
        for(int i=0;i<=96;++i) {
            float a=tau*float(i)/96;
            path.push_back({.507f*std::cos(a),.507f*std::sin(a)*std::cos(tilt),.507f*std::sin(a)*std::sin(tilt)});
        }
        tube(m,path,.010f,6);
    }
    return m;
}
MeshData cellMesh() {
    MeshData m;
    // Clipped diagonals form real crossed threads with open space between them.
    for(int direction : {-1,1})for(int band=-3;band<=3;++band) {
        float intercept=band*.15f;
        float lo=std::max(-.45f,-.45f-intercept),hi=std::min(.45f,.45f-intercept);
        if(hi<=lo)continue;
        tube(m,{{lo,direction*.007f,direction*(lo+intercept)},
                {hi,direction*.007f,direction*(hi+intercept)}},.021f,6);
    }
    return m;
}
MeshData basketWeaveMesh() {
    MeshData m;
    for(int i=0;i<28;++i)for(int sign : {-1,1}) {
        std::vector<Vec3> path;
        for(int j=0;j<=14;++j) {
            float t=float(j)/14,y=.045f+t*.485f;
            float a=tau*float(i)/28+sign*t*.30f,r=.321f+t*.087f;
            path.push_back({r*std::cos(a),y,r*std::sin(a)});
        }
        tube(m,path,.0075f,6);
    }
    for(int i=0;i<7;++i) {
        std::vector<Vec3> path;float y=.065f+i*.071f,r=.325f+(y-.045f)/.485f*.087f;
        for(int j=0;j<=72;++j) {float a=tau*float(j)/72;path.push_back({r*std::cos(a),y,r*std::sin(a)});}
        tube(m,path,.007f,6);
    }
    return m;
}
Vec3 hairPoint(float t,float angle) {
    float edge=.101f+.128f*(.5f-.5f*std::sin(angle));
    float r=std::pow(std::max(std::cos(t*pi*.5f),.00002f),.82f);
    return {std::cos(angle)*.139f*r,edge+( .331f-edge)*std::sin(t*pi*.5f),
            std::sin(angle)*.128f*r+.009f};
}
MeshData hairMesh() {
    MeshData m;constexpr int rings=10,sides=32;
    for(int i=0;i<=rings;++i)for(int j=0;j<=sides;++j) {
        float t=float(i)/rings,a=tau*float(j)/sides;
        Vec3 along=hairPoint(std::min(t+.002f,.9999f),a)-hairPoint(std::max(t-.002f,0.f),a);
        Vec3 around=hairPoint(std::min(t,.999f),a+.002f)-hairPoint(std::min(t,.999f),a-.002f);
        m.vertices.push_back({hairPoint(std::min(t,.99999f),a),normalized(cross(along,around)*1000000.f),
                              {float(j)/sides,t}});
    }
    for(int i=0;i<rings;++i)for(int j=0;j<sides;++j) {
        auto a=std::uint32_t(i*(sides+1)+j),b=a+sides+1;
        triangle(m,a,b,b+1);triangle(m,a,b+1,a+1);
    }
    return m;
}
MeshData apronMesh() {
    const std::array<Vec3,4> rows{{{-.165f,.175f,-.165f},{0,.19f,-.149f},
                                  {.13f,.145f,-.149f},{.29f,.12f,-.153f}}};
    MeshData m;constexpr int across=12;
    for(int back=0;back<2;++back) {
        auto first=static_cast<std::uint32_t>(m.vertices.size());
        for(const auto& row:rows)for(int j=0;j<=across;++j) {
            float t=float(j)/across,a=2*t-1;
            Vec3 p{a*row.y,row.x,row.z+a*a*.025f+back*.006f};
            m.vertices.push_back({p,normalized({a*.15f,0,back?1.f:-1.f}),{t,(row.x+.165f)/.455f}});
        }
        for(int i=0;i<3;++i)for(int j=0;j<across;++j) {
            auto a=first+std::uint32_t(i*(across+1)+j),b=a+across+1;
            if(!back){triangle(m,a,b,b+1);triangle(m,a,b+1,a+1);}
            else {triangle(m,a,b+1,b);triangle(m,a,a+1,b+1);}
        }
    }
    for(int side : {-1,1})for(int i=0;i<3;++i) {
        Vec3 a{side*rows[i].y,rows[i].x,rows[i].z+.025f};
        Vec3 b{side*rows[i+1].y,rows[i+1].x,rows[i+1].z+.025f};
        Vec3 c=b+Vec3{0,0,.006f},d=a+Vec3{0,0,.006f};
        if(side>0)quad(m,a,b,c,d,{1,0,0}); else quad(m,a,d,c,b,{-1,0,0});
    }
    return m;
}
MeshData bladeMesh() {
    MeshData m;
    const std::array<Vec2,4> p{{{-.025f,.025f},{-.023f,-.19f},{.002f,-.34f},{.025f,-.025f}}};
    // A convex extruded four-point blade; each side has its own flat normal.
    for(int top=0;top<2;++top) {
        float y=top?.006f:-.006f;Vec3 n{0,top?1.f:-1.f,0};
        auto a=std::uint32_t(m.vertices.size());
        for(auto v:p)m.vertices.push_back({{v.x,y,v.y},n,{v.x*10+.5f,-v.y*2}});
        if(top){triangle(m,a,a+2,a+1);triangle(m,a,a+3,a+2);}
        else{triangle(m,a,a+1,a+2);triangle(m,a,a+2,a+3);}
    }
    for(int i=0;i<4;++i) {
        auto a=p[i],b=p[(i+1)%4];Vec3 n=normalized({b.y-a.y,0,a.x-b.x});
        quad(m,{a.x,-.006f,a.y},{a.x,.006f,a.y},{b.x,.006f,b.y},{b.x,-.006f,b.y},n);
    }
    return m;
}

void add(std::vector<DrawItem>& objects,int mesh,const Mat4& model,Material material,const char* name) {
    objects.push_back({mesh,model,material,name});
}
void box(std::vector<DrawItem>& objects,Vec3 p,Vec3 dimensions,Material material,const char* name,float yaw=0) {
    add(objects,Cube,trs(p,dimensions,yaw),material,name);
}
void ball(std::vector<DrawItem>& objects,Vec3 p,Vec3 dimensions,Material material,const char* name,const Mat4& parent=Mat4::identity()) {
    add(objects,Sphere,parent*trs(p,dimensions),material,name);
}
void rod(std::vector<DrawItem>& objects,Vec3 a,Vec3 b,float radius,Material material,const char* name,const Mat4& parent=Mat4::identity()) {
    add(objects,Cylinder,parent*segment(a,b,radius),material,name);
}
void obstacle(Scene& s,Vec3 p,Vec3 size) {s.obstacles.push_back({p-size*.5f,p+size*.5f});}
void wall(Scene& s,Vec3 p,Vec3 size) {
    box(s.objects,p,size,mat({.86f,.82f,.72f},Plaster,.06f,16.f,4.f),"parede");
    obstacle(s,p,size);
}
void yarn(std::vector<DrawItem>& o,Vec3 p,int color,float size=.26f,const Mat4& extra=Mat4::identity()) {
    Mat4 parent=translate(p)*extra*scale({size,size*.91f,size});
    add(o,Sphere,parent,mat(yarnColors[color],Yarn,.06f,12.f,4.f),"novelo");
    Vec3 strand=mix(yarnColors[color],{1,.93f,.82f},.18f);
    add(o,Winding,parent,mat(strand,Yarn,.04f,12.f,2.f),"fio-enrolado");
}
void spool(std::vector<DrawItem>& o,Vec3 p,int color,float factor=1) {
    Mat4 parent=translate(p)*scale({factor,factor,factor});
    add(o,Spool,parent,paleOak,"carretel-madeira");
    add(o,Cylinder,parent*trs({0,.125f,0},{.135f,.165f,.135f}),mat(yarnColors[color],Yarn,.06f,12.f,5.f),"carretel-fio");
    add(o,Cylinder,parent*trs({0,.252f,0},{.031f,.005f,.031f}),darkOak,"carretel-eixo");
}
void plant(std::vector<DrawItem>& o,Vec3 at,float factor) {
    Mat4 parent=translate(at)*scale({factor,factor,factor});
    add(o,Flowerpot,parent,mat({.70f,.37f,.25f},Plain,.16f,24),"vaso-de-barro");
    add(o,Cylinder,parent*trs({0,.215f,0},{.225f,.012f,.225f}),mat({.24f,.17f,.10f}),"terra");
    for(int i=0;i<7;++i) {
        float a=tau*i/7;Vec3 tip{.11f*std::cos(a),.46f+(i%3)*.055f,.11f*std::sin(a)};
        rod(o,{0,.22f,0},tip,.006f,mat({.28f,.44f,.26f}),"caule",parent);
        add(o,Sphere,parent*translate(tip)*rotateY(-a)*rotateZ(.45f*std::cos(a))*scale({.13f,.24f,.045f}),
            mat({.34f+(i%2)*.09f,.54f,.31f},Plain,.06f,16),"folha");
    }
}

void room(Scene& s) {
    auto& o=s.objects;
    box(o,{0,-.065f,0},{8.16f,.13f,8.16f},mat({.80f,.73f,.61f},Tiles,.12f,24,8),"piso");
    // The rear wall is split around the door, including its actual header.
    wall(s,{-3.70f,1.65f,-4},{.60f,3.30f,.16f});
    wall(s,{.80f,1.65f,-4},{6.40f,3.30f,.16f});
    wall(s,{-2.90f,2.80f,-4},{1.f,1.f,.16f});
    wall(s,{4,1.65f,0},{.16f,3.3f,8});wall(s,{0,1.65f,4},{8,3.3f,.16f});
    // Four solid pieces around a glazed opening in the left wall.
    wall(s,{-4,.55f,0},{.16f,1.10f,8});wall(s,{-4,2.975f,0},{.16f,.65f,8});
    wall(s,{-4,1.875f,-3.45f},{.16f,1.55f,1.10f});
    wall(s,{-4,1.875f,1.5f},{.16f,1.55f,5.f});
    obstacle(s,{-4,1.875f,-1.95f},{.16f,1.55f,1.9f}); // Window glass still blocks the camera/body.
    box(o,{-4.045f,1.875f,-1.95f},{.02f,1.55f,1.9f},mat({.66f,.82f,.84f},Plain,.1f,24,1,.22f),"ceu-da-janela");
    // Distant foliage is modeled as shallow silhouettes behind the frame.
    ball(o,{-4.025f,1.26f,-2.15f},{.024f,.50f,1.55f},mat({.50f,.65f,.50f},Plain,.04f,12,1,.12f),"jardim-distante");
    ball(o,{-4.01f,1.29f,-1.27f},{.028f,.37f,.53f},mat({.36f,.55f,.43f},Plain,.04f,12,1,.10f),"jardim-distante");
    for(float z : {-2.94f,-1.95f,-.96f})box(o,{-3.895f,1.875f,z},{.11f,1.69f,.065f},paleOak,"janela-montante");
    for(float y : {1.065f,1.865f,2.685f})box(o,{-3.895f,y,-1.95f},{.11f,.065f,2.05f},paleOak,"janela-travessa");
    box(o,{-3.83f,1.045f,-1.95f},{.34f,.09f,2.16f},paleOak,"peitoril");
    plant(o,{-3.80f,1.09f,-1.35f},.76f);
    const auto sage=mat({.53f,.65f,.57f},Plaster,.06f,16,3);
    box(o,{3.905f,.38f,0},{.035f,.72f,7.84f},sage,"lambril");
    box(o,{-3.905f,.38f,0},{.035f,.72f,7.84f},sage,"lambril");
    box(o,{.80f,.38f,-3.905f},{6.30f,.72f,.035f},sage,"lambril");
    box(o,{-3.70f,.38f,-3.905f},{.50f,.72f,.035f},sage,"lambril");
    box(o,{0,.38f,3.905f},{7.84f,.72f,.035f},sage,"lambril");
    for(float x : {-3.89f,3.89f}) {
        box(o,{x,.075f,0},{.055f,.15f,7.85f},paleOak,"rodape");
        box(o,{x,.77f,0},{.06f,.045f,7.85f},paleOak,"moldura-lambril");
    }
    for(float y : {.075f,.77f}) {
        float h=y<.1f?.15f:.045f;
        box(o,{.80f,y,-3.89f},{6.30f,h,.055f},paleOak,"rodape");
        box(o,{-3.70f,y,-3.89f},{.50f,h,.055f},paleOak,"rodape");
        box(o,{0,y,3.89f},{7.85f,h,.055f},paleOak,"rodape");
    }
    box(o,{0,3.36f,0},{8.16f,.12f,8.16f},mat({.91f,.88f,.80f},Plaster,.05f,16,4),"teto");
    obstacle(s,{0,3.36f,0},{8.16f,.12f,8.16f});
    for(float z : {-3.87f,0.f,3.87f})box(o,{0,3.25f,z},{7.90f,.10f,.13f},paleOak,"viga");
    // The exterior landing makes opening the door reveal a physical space.
    box(o,{-2.9f,-.07f,-4.70f},{1.3f,.14f,1.4f},mat({.65f,.61f,.51f},Tiles,.10f,24,2),"soleira-externa");
    box(o,{-2.9f,1.2f,-5.43f},{1.3f,2.4f,.08f},mat({.68f,.79f,.73f},Plaster,.05f,12),"patio-externo");
    for(float x : {-3.46f,-2.34f})box(o,{x,1.17f,-3.89f},{.10f,2.34f,.19f},paleOak,"batente-porta");
    box(o,{-2.9f,2.32f,-3.89f},{1.22f,.12f,.19f},paleOak,"batente-porta");
    box(o,{-2.9f,.015f,-3.91f},{1.0f,.03f,.22f},brass,"soleira");
    // Textile artwork, built from thread segments rather than an imported decal.
    Vec3 art{.65f,2.05f,-3.87f};
    box(o,art,{1.32f,1.07f,.045f},cream,"quadro-tecido");
    for(float x : {-0.04f,1.34f})box(o,{x,2.05f,-3.82f},{.065f,1.19f,.07f},oak,"quadro-moldura");
    for(float y : {1.475f,2.625f})box(o,{.65f,y,-3.82f},{1.45f,.065f,.07f},oak,"quadro-moldura");
    for(int k=-3;k<=3;++k) {
        float x=.65f+k*.155f;
        rod(o,{x,1.55f,-3.835f},{x,2.55f,-3.835f},.003f,paleOak,"quadro-urdidura");
    }
    for(int row=0;row<3;++row)for(int col=0;col<3;++col) {
        Vec3 c{.65f+(col-1)*.29f,2.05f+(row-1)*.29f,-3.80f};
        Material thread=mat(yarnColors[(row+col)%3],Yarn,.04f,12);
        for(int k=-2;k<=2;++k) {
            float r=.115f-std::abs(k)*.022f;
            rod(o,c+Vec3{-r,k*.035f,0},c+Vec3{0,k*.035f+r,0},.006f,thread,"quadro-trama");
            rod(o,c+Vec3{0,k*.035f+r,0},c+Vec3{r,k*.035f,0},.006f,thread,"quadro-trama");
        }
    }
    box(o,{0,.006f,-.65f},{3.55f,.012f,3.10f},mat({.70f,.59f,.43f},Fabric,.04f,12,6),"tapete");
    for(float z : {-2.10f,.80f})box(o,{0,.014f,z},{3.36f,.005f,.045f},mat({.33f,.47f,.43f},Fabric,.05f,12,6),"tapete-borda");
}

void furniture(Scene& s) {
    auto& o=s.objects;
    // 1. Workbench: useful top height is 1.01 m. Its collision stops there.
    box(o,{0,.95f,-1},{2.5f,.12f,1.5f},paleOak,"bancada-tampo");
    for(float x : {-1.04f,1.04f})for(float z : {-1.58f,-.42f}) {
        box(o,{x,.445f,z},{.13f,.89f,.13f},oak,"bancada-perna");
        box(o,{x,.77f,z},{.19f,.065f,.19f},darkOak,"bancada-junta");
    }
    for(float z : {-1.60f,-.40f}) {
        box(o,{0,.83f,z},{2.18f,.16f,.065f},oak,"bancada-saia");
        box(o,{0,.22f,z},{2.18f,.07f,.07f},oak,"bancada-travessa");
    }
    for(float x : {-1.04f,1.04f})box(o,{x,.22f,-1},{.07f,.07f,1.16f},oak,"bancada-travessa");
    obstacle(s,{0,.505f,-1},{2.5f,1.01f,1.5f});
    // 2. Loom: boardSize describes the editable opening, outside rails add .1m.
    const Vec3 c=s.boardCenter;const float outer=s.boardSize+.12f,half=(s.boardSize+.065f)*.5f;
    for(float x : {-half,half})box(o,c+Vec3{x,0,0},{.065f,.045f,outer},oak,"bastidor-lateral");
    for(float z : {-half,half})box(o,c+Vec3{0,0,z},{outer,.045f,.065f},oak,"bastidor-travessa");
    for(float x : {-half,half})for(float z : {-half,half}) {
        add(o,Cylinder,trs(c+Vec3{x,.026f,z},{.023f,.007f,.023f}),brass,"bastidor-parafuso");
        box(o,c+Vec3{x,.031f,z},{.015f,.002f,.0025f},darkOak,"bastidor-fenda");
    }
    const auto baseThread=mat({.91f,.82f,.62f},Yarn,.04f,12,3);
    for(int i=0;i<=8;++i) {
        float t=-s.boardSize*.5f+i*s.boardSize/8;
        rod(o,c+Vec3{t,.010f,-s.boardSize*.5f},c+Vec3{t,.010f,s.boardSize*.5f},.0026f,baseThread,"bastidor-urdidura");
        rod(o,c+Vec3{-s.boardSize*.5f,.011f,t},c+Vec3{s.boardSize*.5f,.011f,t},.0026f,baseThread,"bastidor-trama-base");
    }
    // Distinctive middle guide between reference and editable half.
    rod(o,c+Vec3{0,.014f,-s.boardSize*.5f},c+Vec3{0,.014f,s.boardSize*.5f},.0032f,
        mat({.33f,.46f,.39f},Yarn,.05f,16),"bastidor-eixo-simetria");
    // 4. Stool: splayed legs and footrest below a round seat.
    Vec3 stool{-1.90f,0,-.55f};
    add(o,Cylinder,trs(stool+Vec3{0,.58f,0},{.55f,.09f,.55f}),paleOak,"banqueta-assento");
    add(o,Torus,trs(stool+Vec3{0,.627f,0},{.46f,.055f,.46f}),oak,"banqueta-aro-assento");
    for(int i=0;i<4;++i) {
        float a=pi*.25f+i*pi*.5f;
        Vec3 bottom{std::cos(a)*.25f,.04f,std::sin(a)*.25f},top{std::cos(a)*.16f,.55f,std::sin(a)*.16f};
        rod(o,stool+bottom,stool+top,.036f,oak,"banqueta-perna");
        ball(o,stool+bottom,{.074f,.025f,.074f},darkOak,"banqueta-sapata");
    }
    add(o,Torus,trs(stool+Vec3{0,.23f,0},{.47f,.07f,.47f}),oak,"banqueta-apoio");
    obstacle(s,stool+Vec3{0,.325f,0},{.57f,.65f,.57f});
    // 5. Shelf: shelves run along Z against the right wall.
    Vec3 shelf{3.46f,0,-1.55f};
    for(float y : {.19f,.80f,1.41f,2.02f,2.63f})box(o,shelf+Vec3{0,y,0},{.59f,.075f,2.55f},paleOak,"prateleira-tabua");
    for(float z : {-1.23f,1.23f})for(float x : {-.23f,.23f})box(o,shelf+Vec3{x,1.38f,z},{.075f,2.60f,.075f},oak,"prateleira-coluna");
    box(o,shelf+Vec3{.255f,1.40f,0},{.045f,2.44f,2.50f},mat({.45f,.57f,.50f},Wood,.08f,20,3),"prateleira-fundo");
    obstacle(s,shelf+Vec3{0,1.35f,0},{.62f,2.70f,2.61f});
    for(int i=0;i<6;++i)spool(o,shelf+Vec3{-.04f,.84f,-.92f+i*.36f},i%3,1.15f);
    for(int i=0;i<3;++i)yarn(o,shelf+Vec3{-.04f,1.60f,-.79f+i*.39f},i,.29f);
    for(int i=0;i<3;++i) {
        box(o,shelf+Vec3{-.045f,.30f+i*.065f,.56f},{.44f,.055f,.72f},
            mat(i==1?Vec3{.54f,.66f,.61f}:Vec3{.82f,.76f,.62f},Fabric,.04f,16,4),"tecido-dobrado");
    }
    plant(o,shelf+Vec3{-.03f,2.67f,.70f},.65f);
    // 6. Spools on the workbench use the same shared procedural profile.
    spool(o,{.82f,1.015f,-1.59f},1,.75f);spool(o,{.61f,1.015f,-1.59f},2,.65f);
    // 7. Basket: double surface, a real woven overlay and rolled rim.
    Vec3 basket{2.25f,.015f,1.05f};
    add(o,Basket,translate(basket),mat({.63f,.46f,.28f},Fabric,.05f,16,3),"cesto-corpo");
    add(o,BasketWeave,translate(basket),mat({.82f,.66f,.44f},Wood,.07f,20,2),"cesto-trancado");
    add(o,Cylinder,trs(basket+Vec3{0,.018f,0},{.60f,.035f,.60f}),oak,"cesto-fundo");
    add(o,Torus,trs(basket+Vec3{0,.54f,0},{.85f,.13f,.85f}),paleOak,"cesto-borda");
    for(float z : {-.35f,.35f})add(o,Torus,translate(basket+Vec3{0,.54f,z})*rotateX(pi*.5f)*scale({.27f,.18f,.27f}),paleOak,"cesto-alca");
    yarn(o,basket+Vec3{-.12f,.43f,-.05f},2,.27f);yarn(o,basket+Vec3{.13f,.46f,.05f},0,.25f);
    box(o,basket+Vec3{.08f,.53f,-.17f},{.34f,.04f,.33f},mat({.63f,.72f,.64f},Fabric,.04f,12,4),"cesto-pano",.3f);
    obstacle(s,basket+Vec3{0,.32f,0},{.87f,.64f,.87f});
    // 8. Scissors: two blades, two finger rings and one pivot screw.
    Mat4 scissors=translate({.87f,1.033f,-.57f})*rotateY(-.42f);
    for(int side : {-1,1}) {
        Mat4 blade=scissors*rotateY(side*.17f);
        add(o,Blade,blade,steel,"tesoura-lamina");
        rod(o,{0,0,0},{side*.04f,0,.085f},.013f,steel,"tesoura-haste",blade);
        add(o,Torus,blade*trs({side*.045f,0,.13f},{.11f,.075f,.14f}),brass,"tesoura-anel");
    }
    add(o,Cylinder,scissors*trs({0,.009f,0},{.037f,.010f,.037f}),brass,"tesoura-pivo");
    box(o,{.37f,1.025f,-1.60f},{.20f,.016f,.24f},mat({.86f,.80f,.65f},Fabric,.05f,16,3),"amostra-tecido",-.10f);
    // 10. Lamp base and stand stay static; bulb and shade change with the state.
    Vec3 p=s.lampPosition;
    add(o,Cylinder,trs({p.x,1.035f,p.z},{.32f,.045f,.32f}),mat({.30f,.43f,.37f},Plain,.25f,40),"luminaria-base");
    add(o,Cylinder,trs({p.x,1.06f,p.z},{.25f,.018f,.25f}),brass,"luminaria-base-aro");
    rod(o,{p.x,1.065f,p.z},{p.x,1.44f,p.z},.018f,brass,"luminaria-haste");
    ball(o,{p.x,1.435f,p.z},{.069f,.069f,.069f},brass,"luminaria-articulacao");
    box(o,{p.x+.07f,1.063f,p.z+.045f},{.035f,.018f,.04f},darkOak,"luminaria-interruptor");
}

void character(std::vector<DrawItem>& o,const VisualState& state) {
    const Material skin=mat({.72f,.50f,.37f},Plain,.10f,24);
    const Material hair=mat({.19f,.14f,.105f},Plain,.10f,28);
    const Material hairHighlight=mat({.29f,.20f,.14f},Plain,.10f,24);
    const Material shirt=mat({.44f,.59f,.50f},Fabric,.05f,16,4);
    const Material cuff=mat({.54f,.65f,.56f},Fabric,.05f,16,4);
    const Material trousers=mat({.22f,.31f,.37f},Fabric,.05f,16,5);
    const Material hem=mat({.29f,.37f,.41f},Fabric,.05f,16,5);
    const Material linen=mat({.78f,.70f,.58f},Fabric,.05f,16,5);
    const Material pocket=mat({.73f,.65f,.51f},Fabric,.05f,16,5);
    const Material leather=mat({.50f,.37f,.27f},Plain,.16f,32);
    const Material sole=mat({.26f,.24f,.20f},Plain,.05f,16);
    const Material lace=mat({.77f,.70f,.58f},Fabric,.04f,12,2);
    float amount=clamp(state.walkAmount,0,1),phase=state.walk;
    float hipHeight=.94f-.018f*amount+.004f*std::cos(2*phase)*amount;
    Mat4 root=translate(state.player)*rotateY(state.heading);
    Mat4 body=root*translate({0,hipHeight,0});
    add(o,Hips,body,trousers,"personagem-quadril");
    Mat4 torso=body*translate({0,.08f,0})*rotateZ(std::sin(phase)*amount*.014f);
    add(o,Shirt,torso,shirt,"personagem-blusa");
    add(o,Apron,torso,linen,"personagem-avental");
    for(int sign : {-1,1}) {
        rod(o,{sign*.106f,.285f,-.14f},{sign*.12f,.398f,-.061f},.011f,linen,"personagem-alca-avental",torso);
        rod(o,{sign*.12f,.398f,-.061f},{sign*.13f,.13f,.12f},.010f,linen,"personagem-alca-costas",torso);
        rod(o,{0,.06f,.135f},{sign*.078f,.082f,.143f},.008f,linen,"personagem-laco",torso);
        rod(o,{0,.06f,.14f},{sign*.05f,-.08f,.149f},.006f,linen,"personagem-laco-ponta",torso);
        rod(o,{sign*.066f,.427f,-.04f},{0,.373f,-.11f},.011f,cuff,"personagem-gola",torso);
    }
    add(o,Cube,torso*trs({0,-.047f,-.168f},{.20f,.115f,.014f}),pocket,"personagem-bolso");
    rod(o,{-.092f,.007f,-.178f},{.092f,.007f,-.178f},.002f,linen,"personagem-costura-bolso",torso);
    rod(o,{.018f,.005f,-.178f},{.018f,-.10f,-.178f},.0018f,linen,"personagem-divisao-bolso",torso);
    rod(o,{0,.421f,0},{0,.497f,0},.057f,skin,"personagem-pescoco",torso);
    Mat4 head=torso*translate({0,.453f,0})*rotateY(.025f*std::sin(phase)*amount);
    add(o,Face,head,skin,"personagem-rosto");add(o,Hair,head,hair,"personagem-cabelo");
    ball(o,{0,.219f,.134f},{.139f,.13f,.12f},hair,"personagem-coque",head);
    add(o,Torus,head*translate({0,.217f,.128f})*rotateX(pi*.5f)*scale({.098f,.045f,.098f}),cuff,"personagem-prendedor");
    for(int i=0;i<7;++i) {
        float a=i*.20f-.6f;
        rod(o,{std::sin(a)*.050f,.261f,.169f},{std::sin(a)*.049f,.178f,.169f},.0018f,hairHighlight,"personagem-mecha",head);
    }
    const auto whites=mat({.91f,.87f,.81f},Plain,.14f,32),iris=mat({.20f,.16f,.12f},Plain,.28f,48),lip=mat({.50f,.32f,.25f});
    for(int sign : {-1,1}) {
        ball(o,{sign*.124f,.137f,.012f},{.036f,.065f,.043f},skin,"personagem-orelha",head);
        ball(o,{sign*.045f,.175f,-.101f},{.045f,.026f,.022f},whites,"personagem-olho",head);
        ball(o,{sign*.045f,.176f,-.113f},{.018f,.020f,.008f},iris,"personagem-iris",head);
        ball(o,{sign*.045f-.003f,.181f,-.117f},{.004f,.004f,.004f},whites,"personagem-reflexo-olho",head);
        rod(o,{sign*.024f,.205f,-.103f},{sign*.067f,.200f,-.098f},.0045f,hair,"personagem-sobrancelha",head);
    }
    ball(o,{0,.131f,-.108f},{.038f,.052f,.052f},skin,"personagem-nariz",head);
    rod(o,{-.028f,.080f,-.102f},{0,.074f,-.108f},.003f,lip,"personagem-boca",head);
    rod(o,{0,.074f,-.108f},{.028f,.080f,-.102f},.003f,lip,"personagem-boca",head);

    for(int sign : {-1,1}) {
        float stride=std::sin(phase+(sign<0?pi:0)),armSwing=-stride*amount*.30f;
        Mat4 arm=torso*translate({sign*.236f,.352f,0})*rotateZ(sign*.045f)*rotateX(armSwing);
        ball(o,{sign*.006f,-.039f,0},{.145f,.15f,.157f},shirt,"personagem-ombro",arm);
        add(o,Sleeve,arm,shirt,"personagem-manga");
        add(o,Cylinder,arm*trs({0,-.158f,0},{.134f,.025f,.138f}),cuff,"personagem-barra-manga");
        rod(o,{0,-.15f,0},{0,-.255f,0},.050f,skin,"personagem-braco",arm);
        Mat4 elbow=arm*translate({0,-.255f,0})*rotateX(-.10f-std::max(0.f,-stride)*amount*.20f);
        ball(o,{0,0,0},{.097f,.097f,.097f},skin,"personagem-cotovelo",elbow);
        add(o,Forearm,elbow,skin,"personagem-antebraco");
        Mat4 wrist=elbow*translate({0,-.238f,0});
        ball(o,{0,0,0},{.067f,.060f,.071f},skin,"personagem-punho",wrist);
        ball(o,{0,-.045f,-.007f},{.074f,.11f,.047f},skin,"personagem-palma",wrist);
        ball(o,{-sign*.036f,-.036f,-.012f},{.027f,.065f,.032f},skin,"personagem-polegar",wrist);
        for(int finger=0;finger<3;++finger)ball(o,{-.020f+finger*.019f,-.090f,-.007f},{.021f,.044f,.036f},skin,"personagem-dedo",wrist);

        // A small analytic two-bone construction keeps the shoe level while
        // bending each knee. It is rebuilt from the walk phase every frame.
        constexpr float upper=.415f,lower=.425f;
        float ankleY=.10f+.09f*std::max(0.f,stride)*amount;
        float dy=ankleY-hipHeight,dz=-.18f*stride*amount;
        float d=clamp(std::hypot(dy,dz),.05f,upper+lower-.00001f);
        float hip=std::atan2(-dz,-dy)+std::acos(clamp((upper*upper+d*d-lower*lower)/(2*upper*d),-1,1));
        float knee=-std::acos(clamp((d*d-upper*upper-lower*lower)/(2*upper*lower),-1,1));
        Mat4 leg=body*translate({sign*.105f,0,0})*rotateX(hip);
        add(o,Thigh,leg,trousers,"personagem-coxa");
        Mat4 joint=leg*translate({0,-upper,0})*rotateX(knee);
        ball(o,{0,0,0},{.129f,.132f,.135f},trousers,"personagem-joelho",joint);
        add(o,Shin,joint,trousers,"personagem-canela");
        add(o,Cylinder,joint*trs({0,-.385f,0},{.104f,.036f,.112f}),hem,"personagem-barra-calca");
        Mat4 ankle=joint*translate({0,-lower,0})*rotateX(-hip-knee);
        ball(o,{0,-.061f,-.062f},{.164f,.078f,.282f},sole,"personagem-solado",ankle);
        ball(o,{0,-.040f,-.061f},{.155f,.075f,.268f},leather,"personagem-sapato",ankle);
        for(int i=0;i<3;++i)rod(o,{-.038f,-.001f,-.055f-i*.022f},{.038f,-.001f,-.069f-i*.022f},.002f,lace,"personagem-cadarco",ankle);
    }
}
} // namespace

Scene buildScene() {
    Scene s;
    s.meshes.reserve(21);
    s.meshes.push_back(cubeMesh());
    s.meshes.push_back(sphereMesh());
    s.meshes.push_back(lathe({{-.5f,.5f},{.5f,.5f}},32,true));
    s.meshes.push_back(torusMesh());
    s.meshes.push_back(lathe({{0,.095f},{.025f,.095f},{.035f,.055f},{.215f,.055f},{.225f,.095f},{.25f,.095f}},28,true));
    s.meshes.push_back(lathe({{0,.30f},{.04f,.32f},{.48f,.40f},{.54f,.41f},{.55f,.39f},{.05f,.295f},{0,.30f}},48));
    s.meshes.push_back(lathe({{-.10f,.18f},{.12f,.08f},{.12f,.072f},{-.10f,.172f},{-.10f,.18f}},36));
    s.meshes.push_back(bladeMesh());
    s.meshes.push_back(loft({{-.005f,.17f,.113f},{.055f,.179f,.12f},{.21f,.19f,.14f},
                            {.31f,.213f,.133f},{.367f,.226f,.106f},{.401f,.17f,.085f},{.433f,.064f,.059f}}));
    s.meshes.push_back(loft({{-.09f,.12f,.089f},{-.045f,.17f,.118f},{.04f,.178f,.119f},{.10f,.158f,.11f}}));
    s.meshes.push_back(loft({{0,.025f,.04f,-.023f},{.025f,.075f,.075f,-.015f},{.075f,.108f,.099f,-.009f},
                            {.15f,.126f,.111f},{.23f,.12f,.11f,.005f},{.287f,.089f,.08f,.01f},{.315f,.002f,.002f,.01f}}));
    s.meshes.push_back(hairMesh());
    s.meshes.push_back(apronMesh());
    s.meshes.push_back(loft({{-.414f,.065f,.065f},{-.29f,.074f,.077f},{-.10f,.088f,.097f},{.018f,.087f,.095f}}));
    s.meshes.push_back(loft({{-.394f,.049f,.052f},{-.25f,.060f,.066f},{-.07f,.066f,.071f},{.02f,.063f,.065f}}));
    s.meshes.push_back(loft({{-.166f,.065f,.067f},{-.07f,.073f,.077f},{.015f,.053f,.060f}}));
    s.meshes.push_back(loft({{-.237f,.033f,.035f},{-.16f,.042f,.042f},{-.05f,.051f,.051f},{0,.046f,.047f}}));
    s.meshes.push_back(windingMesh());
    s.meshes.push_back(cellMesh());
    s.meshes.push_back(basketWeaveMesh());
    s.meshes.push_back(lathe({{0,.08f},{.02f,.085f},{.19f,.13f},{.23f,.135f},{.23f,.117f},{.03f,.07f}},24,true));
    s.cube=Cube;s.sphere=Sphere;s.cylinder=Cylinder;s.torus=Torus;
    s.objects.reserve(420);s.obstacles.reserve(20);
    room(s);furniture(s);
    s.inventory={
        {"Bancada","Tampo, pernas, saias, juntas e travessas com cubos dimensionados; escala em metros."},
        {"Bastidor","Quatro barrotes, parafusos, 9+9 fios tubulares e preenchimento volumetrico das 64 celulas."},
        {"Novelo","Esfera indexada, espiral de onze voltas e quatro enrolamentos obliquos em tubos compartilhados."},
        {"Banqueta","Assento cilindrico, pernas inclinadas, juntas e apoios toroidais."},
        {"Prateleira","Cinco tabuas, quatro colunas e fundo; composicao de caixas com texturas UV."},
        {"Carretel","Perfil de revolucao com flanges e eixo, cilindro de fio; tamanhos e cores reutilizam a malha."},
        {"Cesto","Perfil de dupla superficie, fundo, borda, alcas e 56 tiras inclinadas com 7 aros tubulares."},
        {"Tesoura","Laminas convexas extrudadas, aneis toroidais, hastes e pivo cilindrico."},
        {"Porta","Folha, paineis, dobradicas e macanetas subordinadas a um pivo animado na lateral."},
        {"Luminaria","Base, haste, articulacao, cupula conica de dupla superficie e lampada emissiva."}
    };
    return s;
}

std::vector<DrawItem> buildFrame(const Scene& s,const VisualState& state) {
    std::vector<DrawItem> o=s.objects;o.reserve(s.objects.size()+220);
    // 9. All pieces are children of one hinge transform, including the knob.
    Mat4 door=translate(s.doorHinge)*rotateY(state.doorAngle);
    add(o,Cube,door*trs({.5f,1.13f,0},{1.f,2.26f,.09f}),mat({.35f,.48f,.39f},Wood,.13f,32,3),"porta-folha");
    for(float z : {-.049f,.049f}) {
        for(float y : {.57f,1.64f}) {
            add(o,Cube,door*trs({.5f,y,z},{.76f,.83f,.016f}),mat({.43f,.56f,.46f},Wood,.14f,32,2),"porta-painel");
            for(float x : {.105f,.895f})add(o,Cube,door*trs({x,y,z*1.17f},{.035f,.90f,.02f}),paleOak,"porta-moldura");
            for(float yy : {y-.44f,y+.44f})add(o,Cube,door*trs({.5f,yy,z*1.17f},{.82f,.035f,.02f}),paleOak,"porta-moldura");
        }
        float side=z<0?-1.f:1.f;
        add(o,Cube,door*trs({.855f,1.04f,side*.062f},{.055f,.14f,.022f}),brass,"porta-espelho-macaneta");
        rod(o,{.855f,1.04f,side*.066f},{.855f,1.04f,side*.115f},.015f,brass,"porta-eixo-macaneta",door);
        rod(o,{.855f,1.04f,side*.115f},{.75f,1.04f,side*.115f},.013f,brass,"porta-macaneta",door);
    }
    for(float y : {.30f,1.13f,1.96f})add(o,Cylinder,door*trs({.015f,y,.052f},{.035f,.12f,.035f}),brass,"porta-dobradica");

    const Vec3 p=s.lampPosition;
    add(o,Shade,translate(p),mat({.38f,.54f,.44f},Plain,.28f,48,1,state.lampOn?.035f:0),"luminaria-cupula");
    add(o,Torus,trs(p+Vec3{0,-.10f,0},{.36f,.035f,.36f}),brass,"luminaria-borda-cupula");
    ball(o,p+Vec3{0,-.045f,0},{.10f,.12f,.10f},
        mat(state.lampOn?Vec3{1,.83f,.48f}:Vec3{.52f,.50f,.42f},Plain,.10f,24,1,state.lampOn?1.25f:0),"luminaria-lampada");
    for(int i=0;i<3;++i) {
        Mat4 extra=Mat4::identity();Vec3 at=s.yarnPositions[i];
        if(i==0) {
            at+=state.yarnOffset;float size=std::max(.05f,state.yarnScale);
            extra=rotateY(state.yarnRotation)*scale({size,size,size});
        }
        yarn(o,at,i,.26f,extra);
        // Loose strands are local children, so the CG demonstration transforms
        // the complete coral object rather than moving only its center sphere.
        Mat4 parent=translate(at)*extra;
        const auto thread=mat(yarnColors[i],Yarn,.04f,12,3);
        rod(o,{.09f,-.06f,.03f},{.16f,-.093f,.09f},.003f,thread,"novelo-ponta",parent);
        rod(o,{.16f,-.093f,.09f},{.23f,-.093f,.03f},.003f,thread,"novelo-ponta",parent);
    }
    float cell=s.boardSize/8.f;
    for(int row=0;row<8;++row)for(int col=0;col<8;++col) {
        int color=state.cells[row*8+col];if(color<0||color>2)continue;
        Vec3 center=s.boardCenter+Vec3{(col+.5f-4)*cell,.019f,(row+.5f-4)*cell};
        add(o,CellWeave,trs(center,{cell,cell,cell}),mat(yarnColors[color],Yarn,.05f,16,3),"bastidor-celula-tecida");
    }
    if(state.characterVisible)character(o,state);
    return o;
}
} // namespace atelie
