#include "../src/challenge.hpp"
#include <iostream>
#include <string>

using namespace atelie;

namespace {
int checks=0,failures=0;
void check(bool condition,const char* message){
    ++checks;
    if(!condition){++failures;std::cerr<<"FAIL: "<<message<<'\n';}
}
bool near(float a,float b,float tolerance=1e-4f){return std::abs(a-b)<=tolerance;}
bool near(Vec3 a,Vec3 b,float tolerance=1e-4f){return length(a-b)<=tolerance;}

void challengeTests(){
    Challenge challenge;
    check(!challenge.showingSolution,"new challenge starts in attempt view");
    check(challenge.mistakes()==18,"empty attempt has eighteen missing colored cells");
    for(int row=0;row<8;++row)for(int col=0;col<8;++col){
        check(Challenge::expected(row,col)==Challenge::expected(row,7-col),"reference is symmetric");
        check(challenge.answer[static_cast<std::size_t>(row*8+col)]==-1,"new attempt is empty");
    }
    check(Challenge::expected(3,4)==1,"blue maps to color 1");
    check(Challenge::expected(3,5)==1,"second blue cell is preserved");
    check(Challenge::expected(3,6)==2,"yellow maps to color 2");
    check(Challenge::expected(3,7)==0,"coral maps to color 0");
    check(Challenge::expected(0,7)==-1,"empty reference cells stay empty");
    check(Challenge::expected(-1,0)==-1&&Challenge::expected(8,0)==-1,"reference rejects invalid rows");
    check(Challenge::expected(0,-1)==-1&&Challenge::expected(0,8)==-1,"reference rejects invalid columns");

    const auto pristine=challenge.answer;
    check(!challenge.paint(3,3,0),"left reference is read only");
    check(!challenge.paint(-1,4,0)&&!challenge.paint(8,4,0),"painting rejects invalid rows");
    check(!challenge.paint(3,-1,0)&&!challenge.paint(3,8,0),"painting rejects invalid columns");
    check(!challenge.paint(3,4,-2)&&!challenge.paint(3,4,3),"painting rejects unknown colors");
    check(challenge.answer==pristine,"rejected painting does not change the attempt");
    check(challenge.displayed(-1,4)==-1&&challenge.displayed(3,8)==-1,"display safely handles invalid cells");

    for(int row=0;row<8;++row)for(int col=4;col<8;++col)
        challenge.paint(row,col,Challenge::expected(row,col));
    check(challenge.mistakes()==0,"exact mirrored solution is accepted");
    check(challenge.paint(3,4,0)&&challenge.mistakes()==1,"wrong color counts as an error");
    challenge.paint(3,4,-1);
    check(challenge.mistakes()==1,"missing expected color counts as an error");
    challenge.paint(3,4,Challenge::expected(3,4));
    challenge.paint(0,4,2);
    check(challenge.mistakes()==1,"filled expected empty cell counts as an error");
    const auto attempt=challenge.answer;
    challenge.showingSolution=true;
    check(!challenge.paint(3,4,2),"solution view blocks painting");
    check(challenge.mistakes()==1,"solution view does not mask errors in the attempt");
    const auto solution=challenge.displayedCells();
    for(int row=0;row<8;++row)for(int col=0;col<8;++col)
        check(solution[static_cast<std::size_t>(row*8+col)]==Challenge::expected(row,col),"solution view matches the reference");
    check(challenge.answer==attempt,"solution view preserves attempt bytes");
    challenge.showingSolution=false;
    check(challenge.displayed(0,4)==2,"leaving solution restores the attempt view");
    check(challenge.displayed(3,3)==1,"left half displays the fixed reference");
    challenge.showingSolution=true;
    challenge.reset();
    check(!challenge.showingSolution&&challenge.answer==pristine,"reset clears attempt and leaves solution view");
}

void pickingTests(){
    int row=-1,col=-1;
    const Vec3 center{3,1,-2};
    check(pickCell({3.125f,3,-1.875f},{0,-1,0},center,2,row,col)&&row==4&&col==4,"board world offset and center cell mapping");
    for(int r=0;r<8;++r)for(int c=0;c<8;++c){
        const Vec3 origin{center.x-1+(c+.5f)*.25f,3,center.z-1+(r+.5f)*.25f};
        check(pickCell(origin,{0,-2,0},center,2,row,col)&&row==r&&col==c,"all sixty-four cell centers map correctly");
    }
    row=71;col=72;
    check(!pickCell({3,3,-2},{1,0,0},center,2,row,col),"parallel ray misses board");
    check(!pickCell({3,3,-2},{0,1,0},center,2,row,col),"board behind ray is rejected");
    check(!pickCell(center,{0,-1,0},center,2,row,col),"ray starting on board plane is rejected");
    check(!pickCell({4,3,-2},{0,-1,0},center,2,row,col),"positive X border cannot produce column eight");
    check(!pickCell({3,3,-1},{0,-1,0},center,2,row,col),"positive Z border cannot produce row eight");
    check(!pickCell({1.99f,3,-2},{0,-1,0},center,2,row,col),"point outside negative X edge is rejected");
    check(!pickCell({3,3,-3.01f},{0,-1,0},center,2,row,col),"point outside negative Z edge is rejected");
    check(!pickCell({3,3,-2},{0,-1,0},center,0,row,col),"zero board size is rejected");
    check(!pickCell({3,3,-2},{0,-1,0},center,-1,row,col),"negative board size is rejected");
    check(row==71&&col==72,"failed picking preserves output values");
    check(pickCell({2,3,-3},{0,-1,0},center,2,row,col)&&row==0&&col==0,"minimum board edge belongs to first cell");
    check(pickCell({2,3,-3},{.5625f,-1,.5625f},center,2,row,col)&&row==4&&col==4,"oblique ray uses its intersection with board plane");
}

void mathAndRayTests(){
    const Vec3 p=transformPoint(trs({10,2,3},{2,3,4},radians(90)),{1,1,1});
    check(near(p,{14,5,1}),"TRS applies local scale then rotation then translation");
    check(near(transformPoint(translate({1,2,3})*translate({4,5,6}),{}),{5,7,9}),"hierarchy composes parent and child translations");
    const Vec3 a{1,2,3},b{3,5,7};
    const Mat4 tube=segment(a,b,.1f);
    check(near(transformPoint(tube,{0,-.5f,0}),a)&&near(transformPoint(tube,{0,.5f,0}),b),"segment mesh endpoints match geometry endpoints");

    const Aabb box{{-1,-1,-1},{1,1,1}};
    float t=-1;
    check(rayBox({0,0,5},{0,0,-1},box,t)&&near(t,4),"ray hits box front face");
    check(rayBox({0,0,0},{1,0,0},box,t)&&near(t,0),"ray from inside starts at distance zero");
    check(!rayBox({2,0,5},{0,0,-1},box,t),"parallel ray outside slab misses");
    check(!rayBox({0,0,5},{0,0,1},box,t),"box behind ray is rejected");
    check(!rayBox({0,0,5},{0,0,-1},box,t,3.9f),"ray respects maximum reach");
    check(rayBox({0,0,5},{0,0,-1},box,t,4)&&near(t,4),"ray accepts hit at exact maximum reach");
    check(rayBox({1,0,5},{0,0,-1},box,t),"ray along box boundary is handled");
    const Aabb grow=expanded(box,.2f);
    check(near(grow.min,{-1.2f,-1.2f,-1.2f})&&near(grow.max,{1.2f,1.2f,1.2f}),"camera collision box expands by its protection radius");

    Camera camera{{0,2,5},{0,2,0},radians(60)};
    check(near(screenRay(camera,640,360,1280,720),{0,0,-1}),"center screen ray follows camera target");
    const Vec3 left=screenRay(camera,0,360,1280,720),right=screenRay(camera,1280,360,1280,720);
    check(left.x<0&&right.x>0&&near(left.x,-right.x),"screen ray horizontal signs and symmetry");
    check(screenRay(camera,640,0,1280,720).y>0&&screenRay(camera,640,720,1280,720).y<0,"screen ray vertical axis matches top-left pixels");
    check(near(length(right),1),"screen rays are unit length");
    camera.eye={4,2,1};camera.target={0,1,0};
    check(near(screenRay(camera,400,300,800,600),normalized(camera.target-camera.eye)),"center ray follows an oblique camera");
}

void doorAndCollisionTests(){
    Scene scene;
    const Vec3 hinge=scene.doorHinge;
    const Aabb closed=doorBounds(scene,0),open=doorBounds(scene,radians(90));
    check(near(closed.min,hinge+Vec3{0,0,-.045f})&&near(closed.max,hinge+Vec3{1,2.26f,.045f}),"closed door bounds include width height and thickness");
    check(near(open.min,hinge+Vec3{-.045f,0,-1})&&near(open.max,hinge+Vec3{.045f,2.26f,0}),"open door rotates bounds around hinge");
    check(near(transformPoint(doorModel(scene,radians(37)),{-.5f,-1.13f,0}),hinge),"door hinge stays fixed during rotation");
    float t=0;
    check(rayBox(hinge+Vec3{.5f,1,2},{0,0,-1},closed,t),"closed door obstructs its doorway");
    check(!rayBox(hinge+Vec3{.5f,1,2},{0,0,-1},open,t),"open door clears the same center doorway ray");

    const Aabb wall{{1,0,-10},{1.02f,3,10}};
    check(bodyIntersects({.9f,0,0},wall),"cylinder radius intersects nearby wall");
    check(!bodyIntersects({.7f,0,0},wall),"body outside wall radius is clear");
    check(!bodyIntersects({0,0,0},{{-2,-.2f,-2},{2,0,2}}),"floor touching feet does not block body");
    check(!bodyIntersects({0,0,0},{{-2,2,-2},{2,3,2}}),"overhead object above body does not block it");
    check(!bodyIntersects({.83f,0,.83f},{{1,0,1},{2,2,2}}),"cylinder avoids false square-corner collision");
    const std::vector<Aabb> obstacles{wall};
    const Vec3 stopped=moveBody({0,0,0},{5,0,0},obstacles);
    check(stopped.x>.70f&&stopped.x<=.781f&&!bodyIntersects(stopped,wall),"substeps prevent tunnelling across a thin wall");
    const Vec3 slid=moveBody({0,0,0},{2,9,2},obstacles);
    check(slid.x<=.781f&&near(slid.z,2,.001f)&&near(slid.y,0),"blocked diagonal slides along wall and stays on floor");
    const Vec3 backward=moveBody({2,0,0},{-5,0,0},obstacles);
    check(backward.x>=1.239f&&!bodyIntersects(backward,wall),"thin wall blocks travel in opposite direction");
    const std::vector<Aabb> corner{wall,{{-10,0,1},{10,3,1.02f}}};
    const Vec3 inCorner=moveBody({0,0,0},{2,0,2},corner);
    check(inCorner.x<=.781f&&inCorner.z<=.781f,"two walls stop a body at an inside corner");
    Vec3 lowRate{},highRate{};
    for(int frame=0;frame<30;++frame)lowRate=moveBody(lowRate,{2.f/30,0,1.f/30},{});
    for(int frame=0;frame<240;++frame)highRate=moveBody(highRate,{2.f/240,0,1.f/240},{});
    check(near(lowRate,{2,0,1},.001f)&&near(lowRate,highRate,.001f),"equal total displacement agrees across frame partitions");
}
}

int main(){
    challengeTests();
    pickingTests();
    mathAndRayTests();
    doorAndCollisionTests();
    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
