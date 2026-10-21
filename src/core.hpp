#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace atelie {
constexpr float pi = 3.14159265358979323846f;
inline float radians(float degrees) { return degrees * pi / 180.f; }
inline float clamp(float x,float a,float b) { return std::max(a,std::min(b,x)); }
struct Vec2 { float x=0,y=0; };
struct Vec3 {
    float x=0,y=0,z=0;
    Vec3 operator+(Vec3 b)const{return {x+b.x,y+b.y,z+b.z};}
    Vec3 operator-(Vec3 b)const{return {x-b.x,y-b.y,z-b.z};}
    Vec3 operator-()const{return {-x,-y,-z};}
    Vec3 operator*(float s)const{return {x*s,y*s,z*s};}
    Vec3 operator/(float s)const{return *this*(1.f/s);}
    Vec3& operator+=(Vec3 b){*this=*this+b;return *this;}
};
inline float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float length(Vec3 a){return std::sqrt(dot(a,a));}
inline Vec3 normalized(Vec3 a){float l=length(a);return l>1e-7f?a/l:Vec3{};}
inline Vec3 mix(Vec3 a,Vec3 b,float t){return a*(1-t)+b*t;}
inline float smooth(float rate,float dt){return 1.f-std::exp(-rate*dt);}
// Column-major matrices, column vectors: world = parent * T * R * S * local.
struct Mat4 {
    std::array<float,16> m{};
    static Mat4 identity(){Mat4 r; r.m[0]=r.m[5]=r.m[10]=r.m[15]=1;return r;}
};
inline Mat4 operator*(const Mat4&a,const Mat4&b){
    Mat4 r;
    for(int c=0;c<4;++c)for(int row=0;row<4;++row)for(int k=0;k<4;++k){
        r.m[c*4+row]+=a.m[k*4+row]*b.m[c*4+k];
    }
    return r;
}
inline Vec3 transformPoint(const Mat4&m,Vec3 p){
    float w=m.m[3]*p.x+m.m[7]*p.y+m.m[11]*p.z+m.m[15];
    if(std::abs(w)<1e-8f) w=1;
    return {(m.m[0]*p.x+m.m[4]*p.y+m.m[8]*p.z+m.m[12])/w,
        (m.m[1]*p.x+m.m[5]*p.y+m.m[9]*p.z+m.m[13])/w,
        (m.m[2]*p.x+m.m[6]*p.y+m.m[10]*p.z+m.m[14])/w};
}
inline Mat4 translate(Vec3 p){auto r=Mat4::identity();r.m[12]=p.x;r.m[13]=p.y;r.m[14]=p.z;return r;}
inline Mat4 scale(Vec3 p){auto r=Mat4::identity();r.m[0]=p.x;r.m[5]=p.y;r.m[10]=p.z;return r;}
inline Mat4 rotateY(float a){auto r=Mat4::identity();float c=std::cos(a),s=std::sin(a);r.m[0]=c;r.m[2]=-s;r.m[8]=s;r.m[10]=c;return r;}
inline Mat4 rotateX(float a){auto r=Mat4::identity();float c=std::cos(a),s=std::sin(a);r.m[5]=c;r.m[6]=s;r.m[9]=-s;r.m[10]=c;return r;}
inline Mat4 rotateZ(float a){auto r=Mat4::identity();float c=std::cos(a),s=std::sin(a);r.m[0]=c;r.m[1]=s;r.m[4]=-s;r.m[5]=c;return r;}
inline Mat4 trs(Vec3 p,Vec3 s,float yaw=0){return translate(p)*rotateY(yaw)*scale(s);}
inline Mat4 perspective(float fov,float aspect,float nearPlane=.08f,float farPlane=60.f){
    Mat4 r;float f=1.f/std::tan(fov*.5f);r.m[0]=f/aspect;r.m[5]=f;
    r.m[10]=(farPlane+nearPlane)/(nearPlane-farPlane);r.m[11]=-1;
    r.m[14]=(2*farPlane*nearPlane)/(nearPlane-farPlane);return r;
}
inline Mat4 lookAt(Vec3 eye,Vec3 target,Vec3 up={0,1,0}){
    Vec3 f=normalized(target-eye),s=normalized(cross(f,up)),u=cross(s,f);auto r=Mat4::identity();
    r.m[0]=s.x;r.m[4]=s.y;r.m[8]=s.z;r.m[1]=u.x;r.m[5]=u.y;r.m[9]=u.z;
    r.m[2]=-f.x;r.m[6]=-f.y;r.m[10]=-f.z;r.m[12]=-dot(s,eye);r.m[13]=-dot(u,eye);r.m[14]=dot(f,eye);return r;
}
// Unit cylinder's local Y axis is aligned with a segment. Radius uses diameter scale.
inline Mat4 segment(Vec3 a,Vec3 b,float radius){
    Vec3 y=normalized(b-a),helper=std::abs(y.y)>.95f?Vec3{1,0,0}:Vec3{0,1,0};
    Vec3 x=normalized(cross(helper,y)),z=cross(x,y);auto r=Mat4::identity();
    r.m[0]=x.x*radius*2;r.m[1]=x.y*radius*2;r.m[2]=x.z*radius*2;
    r.m[4]=y.x*length(b-a);r.m[5]=y.y*length(b-a);r.m[6]=y.z*length(b-a);
    r.m[8]=z.x*radius*2;r.m[9]=z.y*radius*2;r.m[10]=z.z*radius*2;
    Vec3 p=(a+b)*.5f;r.m[12]=p.x;r.m[13]=p.y;r.m[14]=p.z;return r;
}
struct Aabb { Vec3 min,max; };
inline Aabb expanded(Aabb b,float r){return {b.min-Vec3{r,r,r},b.max+Vec3{r,r,r}};}
inline bool rayBox(Vec3 origin,Vec3 direction,Aabb box,float& t,float maxT=1e6f){
    float lo=0,hi=maxT;const float o[]={origin.x,origin.y,origin.z},d[]={direction.x,direction.y,direction.z};
    const float a[]={box.min.x,box.min.y,box.min.z},b[]={box.max.x,box.max.y,box.max.z};
    for(int i=0;i<3;++i){if(std::abs(d[i])<1e-7f){if(o[i]<a[i]||o[i]>b[i])return false;}
        else{float x=(a[i]-o[i])/d[i],y=(b[i]-o[i])/d[i];if(x>y)std::swap(x,y);lo=std::max(lo,x);hi=std::min(hi,y);if(lo>hi)return false;}}
    t=lo;return hi>=0;
}
inline Aabb transformedBox(const Mat4&m,Vec3 half){
    Aabb b{{1e6f,1e6f,1e6f},{-1e6f,-1e6f,-1e6f}};
    for(int i=0;i<8;++i){Vec3 p=transformPoint(m,{(i&1)?half.x:-half.x,(i&2)?half.y:-half.y,(i&4)?half.z:-half.z});
        b.min={std::min(b.min.x,p.x),std::min(b.min.y,p.y),std::min(b.min.z,p.z)};
        b.max={std::max(b.max.x,p.x),std::max(b.max.y,p.y),std::max(b.max.z,p.z)};}return b;
}
struct Vertex { Vec3 position,normal; Vec2 uv; };
struct MeshData { std::vector<Vertex> vertices; std::vector<std::uint32_t> indices; };
enum TextureKind {Plain=0,Wood=1,Fabric=2,Plaster=3,Tiles=4,Yarn=5};
struct Material {Vec3 color{1,1,1};float specular=.15f,shininess=24;int texture=Plain;float uvScale=1,emission=0;};
struct DrawItem {int mesh=0;Mat4 model=Mat4::identity();Material material;std::string name;};
struct ObjectInfo {std::string name,construction;};
struct Scene {
    std::vector<MeshData> meshes;
    std::vector<DrawItem> objects;
    std::vector<Aabb> obstacles;
    std::vector<ObjectInfo> inventory;
    int cube=0,sphere=1,cylinder=2,torus=3;
    Vec3 boardCenter{0,1.035f,-1};float boardSize=1.10f;
    Vec3 lampPosition{1,1.50f,-1.15f};
    Vec3 doorHinge{-3.4f,0,-3.92f};
    std::array<Vec3,3> yarnPositions{{{-.92f,1.12f,-.55f},{-.98f,1.12f,-.98f},{-.92f,1.12f,-1.42f}}};
};
struct VisualState {
    Vec3 player{0,0,1.1f};float heading=0,walk=0,walkAmount=0,doorAngle=0;
    bool lampOn=true,characterVisible=true;Vec3 yarnOffset{};float yarnRotation=0,yarnScale=1;
    std::array<int,64> cells{};
};
Scene buildScene();
std::vector<DrawItem> buildFrame(const Scene&,const VisualState&);
inline Mat4 doorModel(const Scene&s,float angle){return translate(s.doorHinge)*rotateY(angle)*translate({.5f,1.13f,0});}
inline Aabb doorBounds(const Scene&s,float angle){return transformedBox(doorModel(s,angle),{.5f,1.13f,.045f});}
struct HudRect {float x,y,width,height;Vec3 color;float alpha=1;};
struct HudText {float x,y;std::wstring text;float size=18;Vec3 color{1,1,1};};
struct Hud {std::vector<HudRect>rects;std::vector<HudText>texts;};
struct Camera {Vec3 eye,target;float fov=radians(48);};
inline Vec3 screenRay(const Camera&c,float x,float y,float width,float height){
    Vec3 f=normalized(c.target-c.eye),r=normalized(cross(f,{0,1,0})),u=cross(r,f);
    float t=std::tan(c.fov*.5f);return normalized(f+r*((2*x/width-1)*t*width/height)+u*((1-2*y/height)*t));
}
}
