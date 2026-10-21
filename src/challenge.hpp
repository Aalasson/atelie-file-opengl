#pragma once
#include "core.hpp"

namespace atelie {

// The challenge state has no renderer dependency. The left half is immutable;
// showing a solution changes the view, never the stored attempt.
class Challenge {
public:
    std::array<int,64> answer{};
    bool showingSolution=false;

    Challenge(){reset();}
    void reset(){answer.fill(-1);showingSolution=false;}

    static int expected(int row,int col){
        if(row<0||row>=8||col<0||col>=8)return -1;
        static constexpr const char* reference[8]={
            "........","..cc....",".cgg....","cgbb....",
            "cgbb....",".cgg....","..cc....","........"
        };
        const int reflected=col<4?col:7-col;
        const char code=reference[row][reflected];
        return code=='c'?0:code=='b'?1:code=='g'?2:-1;
    }

    bool paint(int row,int col,int color){
        if(showingSolution||row<0||row>=8||col<4||col>=8||color< -1||color>2)return false;
        answer[static_cast<std::size_t>(row*8+col)]=color;
        return true;
    }

    int displayed(int row,int col)const{
        if(row<0||row>=8||col<0||col>=8)return -1;
        return col<4||showingSolution?expected(row,col):answer[static_cast<std::size_t>(row*8+col)];
    }

    int mistakes()const{
        int result=0;
        for(int row=0;row<8;++row)for(int col=4;col<8;++col)
            if(answer[static_cast<std::size_t>(row*8+col)]!=expected(row,col))++result;
        return result;
    }

    std::array<int,64> displayedCells()const{
        std::array<int,64> cells{};
        for(int row=0;row<8;++row)for(int col=0;col<8;++col)
            cells[static_cast<std::size_t>(row*8+col)]=displayed(row,col);
        return cells;
    }
};

// Board is horizontal at boardCenter.y. Bounds are [minimum, maximum) so an
// exact positive edge cannot produce cell 8. Outputs stay unchanged on failure.
inline bool pickCell(Vec3 origin,Vec3 dir,Vec3 boardCenter,float boardSize,int& row,int& col){
    if(!std::isfinite(boardSize)||boardSize<=0||!std::isfinite(dir.y)||std::abs(dir.y)<1e-7f)return false;
    const float divisor=std::copysign(std::max(std::abs(dir.y),1e-7f),dir.y);
    const float t=(boardCenter.y-origin.y)/divisor;
    if(!std::isfinite(t)||t<=0)return false;
    const Vec3 hit=origin+dir*t;
    const float u=(hit.x-boardCenter.x)/boardSize+.5f;
    const float v=(hit.z-boardCenter.z)/boardSize+.5f;
    if(!std::isfinite(u)||!std::isfinite(v)||u<0||u>=1||v<0||v>=1)return false;
    col=static_cast<int>(u*8);
    row=static_cast<int>(v*8);
    return true;
}

// Upright cylinder on a flat floor. Mere contact is allowed, including an
// obstacle surface exactly at the feet, to avoid treating the floor as a wall.
inline bool bodyIntersects(Vec3 feet,Aabb box,float radius=.22f,float height=1.78f){
    if(radius<=0||height<=0||feet.y>=box.max.y||feet.y+height<=box.min.y)return false;
    const float nearestX=clamp(feet.x,box.min.x,box.max.x);
    const float nearestZ=clamp(feet.z,box.min.z,box.max.z);
    const float dx=feet.x-nearestX,dz=feet.z-nearestZ;
    return dx*dx+dz*dz<radius*radius;
}

// Small steps prevent tunnelling through thin walls; resolving axes separately
// lets the body slide along furniture. Vertical motion is intentionally ignored.
inline Vec3 moveBody(Vec3 feet,Vec3 delta,const std::vector<Aabb>& obstacles){
    if(!std::isfinite(delta.x)||!std::isfinite(delta.z))return feet;
    const float distance=std::hypot(delta.x,delta.z);
    const int steps=std::max(1,static_cast<int>(std::ceil(distance/.04f)));
    const Vec3 step{delta.x/steps,0,delta.z/steps};
    const auto blocked=[&](Vec3 candidate){
        for(const Aabb& obstacle:obstacles)if(bodyIntersects(candidate,obstacle))return true;
        return false;
    };
    for(int i=0;i<steps;++i){
        Vec3 candidate=feet+Vec3{step.x,0,0};
        if(!blocked(candidate))feet=candidate;
        candidate=feet+Vec3{0,0,step.z};
        if(!blocked(candidate))feet=candidate;
    }
    return feet;
}

} // namespace atelie
