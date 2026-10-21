#include "game.hpp"
#include "renderer.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace atelie;
namespace fs=std::filesystem;
int main(int argc,char**argv){
    try{
        bool smoke=false;fs::path output="evidence";
        for(int i=1;i<argc;++i){std::string a=argv[i];if(a=="--smoke-test")smoke=true;else if(a=="--output"&&i+1<argc)output=argv[++i];
            else if(a=="--help"){std::cout<<"AtelieFileOpenGL [--smoke-test] [--output folder]\n";return 0;}}
        fs::path exe=fs::absolute(argv[0]).parent_path(),assets=exe/"assets";
        if(!fs::exists(assets/"shaders"))assets=fs::path(ATELIE_SOURCE_DIR)/"assets";
        Window window(1280,800,!smoke);Renderer renderer(window,assets.wstring());
        Scene scene=buildScene();renderer.upload(scene);Game game(scene);
        fs::create_directories(output);std::ofstream log(output/"runtime.log");
        log<<renderer.info()<<"\nObjects: "<<scene.inventory.size()<<"\n";
        std::cout<<renderer.info()<<"\n";
        if(smoke){
            // Real GPU render checks, without injecting input into another application.
            auto capture=[&](const char*name){renderer.render(buildFrame(scene,game.visual()),game.camera(),game.state.lampOn,scene.lampPosition,game.hud(window.width,window.height));renderer.screenshot((output/name).wstring());window.swap();log<<"Rendered "<<name<<"\n";};
            int checks=0;
            auto require=[&](bool ok,const char* description){if(!ok)throw std::runtime_error(std::string("Integration: ")+description);++checks;log<<"PASS "<<description<<"\n";};
            auto key=[&](int code){window.poll();window.pressed[code]=true;game.update(window,1.f/60,true);};
            auto tick=[&](){window.poll();game.update(window,1.f/60,true);};
            for(int i=0;i<60;++i){window.poll();game.update(window,1.f/60,true);}
            capture("01-exploracao.bmp");
            require(scene.inventory.size()>=10,"ten distinct modeled object types");
            game.state.player={0,0,.25f};game.yaw=0;key('E');
            require(game.mode==Mode::Work,"E enters the board through distance and visibility checks");
            key('2');require(game.selected==1,"color selection from keyboard");
            Vec3 cell{scene.boardCenter.x+scene.boardSize*(5.5f/8-.5f),scene.boardCenter.y,scene.boardCenter.z+scene.boardSize*(2.5f/8-.5f)};
            auto c=game.camera();Vec3 projected=transformPoint(perspective(c.fov,float(window.width)/window.height)*lookAt(c.eye,c.target),cell);
            auto click=[&](int button){window.poll();window.mouseX=(projected.x+1)*window.width*.5f;window.mouseY=(1-projected.y)*window.height*.5f;window.mousePressed[button]=true;game.update(window,1.f/60,true);};
            click(0);require(game.challenge.answer[2*8+5]==1,"3D mouse ray paints the selected cell");
            auto attempt=game.challenge.answer;key('X');click(1);
            require(game.challenge.showingSolution&&game.challenge.answer==attempt,"solution view blocks painting and preserves the attempt");
            key('X');click(1);require(game.challenge.answer[2*8+5]==-1,"right click erases a cell");
            key(VK_ESCAPE);require(game.mode==Mode::Paused,"Escape pauses the board");
            key('Q');require(game.mode==Mode::Paused,"game inputs are blocked while paused");
            key(VK_ESCAPE);require(game.mode==Mode::Work,"Escape resumes the previous mode");
            key(VK_F2);require(game.mode==Mode::CG,"F2 enters the transformations demonstration");
            window.poll();window.keys[VK_OEM_PLUS]=true;game.update(window,.05f,true);window.keys[VK_OEM_PLUS]=false;
            require(game.state.yarnScale>1,"scale control changes the object");
            key(VK_BACK);require(game.state.yarnScale==1&&length(game.state.yarnOffset)==0,"Backspace restores object transforms");
            key(VK_F2);require(game.mode==Mode::Work,"CG returns to the board mode");
            key('R');require(game.confirmReset,"R asks before discarding an attempt");key(VK_ESCAPE);
            require(!game.confirmReset&&game.mode==Mode::Work,"Escape cancels reset without opening pause");
            game.messageTime=0;capture("02-bastidor.bmp");
            for(int row=0;row<8;++row)for(int col=4;col<8;++col)game.challenge.paint(row,col,Challenge::expected(row,col));
            require(game.challenge.mistakes()==0,"full solution includes colors and empty cells");
            key('V');
            capture("03-trama-concluida.bmp");
            auto solved=game.challenge.answer;key('Q');require(game.mode==Mode::Explore&&game.challenge.answer==solved,"leaving the board preserves the attempt");
            game.state.player={1.65f,0,-.1f};game.yaw=.5f;key('E');
            require(!game.state.lampOn,"E toggles the point light near the lamp");
            game.mode=Mode::Work;game.messageTime=0;capture("04-luminaria-apagada.bmp");
            game.state.lampOn=true;key(VK_F2);game.state.yarnOffset={.2f,0,.1f};game.state.yarnScale=1.5f;game.state.yarnRotation=.8f;
            capture("05-transformacoes.bmp");
            game.mode=Mode::Explore;game.state.player={-2.5f,0,-2.2f};game.yaw=-.55f;game.doorTarget=-pi*.49f;
            game.doorTarget=0;game.state.player={-2.9f,0,-2.6f};game.yaw=0;key('E');
            require(game.doorTarget<-.4f,"E starts the hinged door animation");
            for(int i=0;i<90;++i){window.poll();game.update(window,1.f/60,true);}
            require(game.state.doorAngle<-.5f,"the door advances to an open rotation");
            game.messageTime=0;
            capture("06-porta-aberta.bmp");
            game.state.player={-2.9f,0,-3.70f};key('E');
            for(int i=0;i<180;++i)tick();
            require(game.state.doorAngle<-.05f&&!bodyIntersects(game.state.player,doorBounds(scene,game.state.doorAngle)),"door closing stops before intersecting the character");
            game.state.player={0,0,2.2f};game.velocity={};game.yaw=0;
            for(int i=0;i<180;++i){window.poll();window.keys['W']=true;game.update(window,1.f/60,true);}window.keys['W']=false;
            bool bodyClear=true;for(auto box:game.obstacles())if(bodyIntersects(game.state.player,box))bodyClear=false;
            require(game.state.player.z<1&&bodyClear&&game.state.walk>0,"WASD moves and animates the character while furniture blocks penetration");
            game.state.player={0,0,3.70f};game.yaw=0;tick();
            Vec3 anchor=game.state.player+Vec3{0,1.26f,0};Camera protectedCamera=game.camera();float camDistance=length(protectedCamera.eye-anchor);
            bool clear=true;for(auto box:game.obstacles()){float hit=0;if(rayBox(anchor,normalized(protectedCamera.eye-anchor),expanded(box,.16f),hit,camDistance-.001f))clear=false;}
            require(clear&&camDistance<.30f,"camera remains in the free space when the player touches a wall");
            require(!game.visual().characterVisible,"character hides when the protected camera is too close");
            log<<renderer.diagnostics()<<"\n"<<checks<<" integration checks passed.\nSMOKE PASS\n";
            std::cout<<checks<<" integration checks passed.\nSMOKE PASS\n";return 0;
        }
        auto last=std::chrono::steady_clock::now();
        while(window.running){
            window.poll();auto now=std::chrono::steady_clock::now();float dt=std::chrono::duration<float>(now-last).count();last=now;
            if(!window.running)break;
            if(window.width<=0||window.height<=0){std::this_thread::sleep_for(std::chrono::milliseconds(20));continue;}
            game.update(window,dt);
            renderer.render(buildFrame(scene,game.visual()),game.camera(),game.state.lampOn,scene.lampPosition,game.hud(window.width,window.height));
            if(window.pressed[VK_F12]){auto timestamp=std::chrono::system_clock::now().time_since_epoch();auto millis=std::chrono::duration_cast<std::chrono::milliseconds>(timestamp).count();
                renderer.screenshot((output/("captura-"+std::to_string(millis)+".bmp")).wstring());}
            window.swap();
        }
        log<<"Normal exit\n";return 0;
    }catch(const std::exception&e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 1;}
}
