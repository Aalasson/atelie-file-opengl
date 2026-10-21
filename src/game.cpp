#include "game.hpp"
#include <cstdio>
#include <cwchar>

namespace atelie {
std::vector<Aabb> Game::obstacles()const{
    auto result=scene.obstacles;result.push_back(doorBounds(scene,state.doorAngle));return result;
}
Target Game::target()const{
    Vec3 origin=state.player+Vec3{0,1.25f,0};
    const std::array<std::pair<Target,Vec3>,3> candidates{{
        {Target::Board,scene.boardCenter}, {Target::Lamp,scene.lampPosition},
        {Target::Door,scene.doorHinge+Vec3{.5f,1.1f,0}}
    }};
    float best=1e6f;Target result=Target::None;
    Vec3 forward=normalized(camera().target-camera().eye);
    for(auto candidate:candidates){
        Vec3 p=candidate.second,flat=p-state.player;flat.y=0;float d=length(flat);
        if(d>1.95f) continue;
        Vec3 delta=p-origin;float rayLength=length(delta);bool visible=true;
        for(auto box:scene.obstacles){float hit=0;if(rayBox(origin,normalized(delta),box,hit,rayLength-.12f)){visible=false;break;}}
        if(visible&&candidate.first!=Target::Door){float hit=0;
            if(rayBox(origin,normalized(delta),doorBounds(scene,state.doorAngle),hit,rayLength-.12f))visible=false;}
        if(!visible)continue;
        float facing=dot(normalized(p-camera().eye),forward);
        if(facing<.2f) continue;
        float score=d+(1-facing)*.5f;
        if(score<best){best=score;result=candidate.first;}
    }return result;
}
Camera Game::camera()const{
    Mode current=mode==Mode::Paused?pauseReturn:mode;
    if(current==Mode::Work) return {{0,2.50f,.62f},{0,1.02f,-1},radians(46)};
    if(current==Mode::CG) return {{.15f,2.25f,.85f},{-.55f,1.08f,-.95f},radians(46)};
    Vec3 targetPosition=state.player+Vec3{0,1.26f,0};
    Vec3 offset{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
    return {targetPosition+offset*safeDistance,targetPosition,radians(52)};
}
void Game::update(Window&w,float dt,bool simulation){
    dt=clamp(dt,0,.05f);messageTime=std::max(0.f,messageTime-dt);
    const bool focused=w.focused||simulation;
    if(!focused&&mode!=Mode::Paused){pauseReturn=mode;mode=Mode::Paused;confirmReset=false;velocity={};}
    if(w.pressed[VK_F1])help=!help;
    if(w.pressed[VK_ESCAPE]){
        if(confirmReset)confirmReset=false;
        else if(mode==Mode::Paused){if(focused)mode=pauseReturn;}
        else{pauseReturn=mode;mode=Mode::Paused;velocity={};}
    }
    if(w.pressed[VK_F2]&&mode!=Mode::Paused&&!confirmReset){
        if(mode==Mode::CG)mode=cgReturn;else{cgReturn=mode;mode=Mode::CG;}
    }
    w.capture(!simulation&&mode==Mode::Explore&&focused);
    if(mode==Mode::Paused||!focused)return;
    if(confirmReset){
        if(w.pressed[VK_RETURN]){challenge.reset();confirmReset=false;say(L"Uma nova trama. Observe a referência à esquerda.");}
        return;
    }
    if(mode==Mode::Explore){
        yaw-=w.mouseDX*.003f;pitch=clamp(pitch+w.mouseDY*.0025f,-.08f,.80f);
        distance=clamp(distance-w.scroll*.22f,1.3f,3.4f);
        Vec3 forward{-std::sin(yaw),0,-std::cos(yaw)},right{std::cos(yaw),0,-std::sin(yaw)};
        Vec3 input=forward*float(int(w.keys['W'])-int(w.keys['S']))+right*float(int(w.keys['D'])-int(w.keys['A']));
        if(length(input)>1)input=normalized(input);
        Vec3 desired=input*1.65f;velocity=mix(velocity,desired,smooth(length(input)>.01f?10.f:13.f,dt));
        Vec3 before=state.player;state.player=moveBody(state.player,velocity*dt,obstacles());
        // The room is the playable area; the doorway does not load another level.
        state.player.x=clamp(state.player.x,-3.72f,3.72f);state.player.z=clamp(state.player.z,-3.72f,3.72f);
        float traveled=length(state.player-before),speed=dt>0?traveled/dt:0;
        state.walk+=traveled*7.5f;state.walkAmount+=(clamp(speed/1.65f,0,1)-state.walkAmount)*smooth(12,dt);
        if(length(input)>.05f){float a=std::atan2(-input.x,-input.z),difference=std::remainder(a-state.heading,2*pi);
            state.heading+=difference*smooth(10,dt);}
        if(w.pressed['E']){
            switch(target()){
                case Target::Board:mode=Mode::Work;velocity={};say(L"Complete o reflexo à direita. 1 / 2 / 3 escolhem a cor.");break;
                case Target::Lamp:state.lampOn=!state.lampOn;say(state.lampOn?L"Luminária acesa.":L"Luminária apagada.");break;
                case Target::Door:doorTarget=doorTarget<-.4f?0:-pi*.49f;say(doorTarget<0?L"Abrindo a porta.":L"Fechando a porta.");break;
                default:say(L"Aproxime-se de um objeto indicado para usar E.");break;
            }
        }
    }else if(mode==Mode::Work){
        state.walkAmount*=std::exp(-12*dt);
        for(int i=0;i<3;++i)if(w.pressed['1'+i])selected=i;
        if(w.pressed['Q']){mode=Mode::Explore;say(L"Sua tentativa foi guardada. Você pode voltar à bancada.");return;}
        if(w.pressed['X']){challenge.showingSolution=!challenge.showingSolution;say(challenge.showingSolution?L"Exemplo completo. X retorna à sua tentativa.":L"Sua tentativa foi recuperada.");}
        if(w.pressed['R']){confirmReset=true;return;}
        if(w.pressed['V']){int errors=challenge.mistakes();
            say(errors==0?L"Trama concluída! Os dois lados estão simétricos.":L"Ainda há "+std::to_wstring(errors)+L" células para corrigir. Confira as cores e os vazios.",6);}
        if(w.mousePressed[0]||w.mousePressed[1]){
            // Palette buttons are handled before the 3D picking pass.
            bool palette=false;
            for(int i=0;i<3;++i)if(w.mouseX>=24+i*150&&w.mouseX<164+i*150&&w.mouseY>=w.height-108&&w.mouseY<w.height-64){selected=i;palette=true;}
            if(!palette){
                Camera c=camera();Vec3 ray=screenRay(c,w.mouseX,w.mouseY,float(w.width),float(w.height));
                bool yarn=false;float closest=1e6f;int yarnChoice=-1;
                for(int i=0;i<3;++i){Vec3 p=scene.yarnPositions[i]+(i==0?state.yarnOffset:Vec3{});float hit=0;
                    if(rayBox(c.eye,ray,{p-Vec3{.16f,.16f,.16f},p+Vec3{.16f,.16f,.16f}},hit)&&hit<closest){closest=hit;yarnChoice=i;}}
                if(yarnChoice>=0){selected=yarnChoice;yarn=true;}
                int row=0,col=0;
                if(!yarn&&pickCell(c.eye,ray,scene.boardCenter,scene.boardSize,row,col)){
                    if(challenge.paint(row,col,w.mousePressed[1]?-1:selected))say(w.mousePressed[1]?L"Fios removidos.":L"Fios adicionados.",1.5f);
                    else if(challenge.showingSolution)say(L"Pressione X para voltar à tentativa antes de tecer.");
                    else say(L"O lado esquerdo é a referência. Complete o lado direito.");
                }
            }
        }
    }else if(mode==Mode::CG){
        state.walkAmount*=std::exp(-12*dt);
        state.yarnOffset.x+=float(int(w.keys[VK_RIGHT])-int(w.keys[VK_LEFT]))*dt*.3f;
        state.yarnOffset.z+=float(int(w.keys[VK_DOWN])-int(w.keys[VK_UP]))*dt*.3f;
        state.yarnOffset.x=clamp(state.yarnOffset.x,-.30f,.30f);state.yarnOffset.z=clamp(state.yarnOffset.z,-.25f,.25f);
        state.yarnRotation+=float(int(w.keys['C'])-int(w.keys['Z']))*dt*1.5f;
        state.yarnScale=clamp(state.yarnScale+float(int(w.keys[VK_OEM_PLUS])-int(w.keys[VK_OEM_MINUS]))*dt*.6f,.5f,1.7f);
        if(w.pressed[VK_BACK]){state.yarnOffset={};state.yarnRotation=0;state.yarnScale=1;}
    }
    float next=state.doorAngle+(doorTarget-state.doorAngle)*smooth(5,dt);
    if(!bodyIntersects(state.player,doorBounds(scene,next)))state.doorAngle=next;
    else if(std::abs(doorTarget-state.doorAngle)>.05f)say(L"Afaste-se da porta para liberar o movimento.",1.5f);
    Vec3 anchor=state.player+Vec3{0,1.26f,0};
    Vec3 direction{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
    float safe=distance;
    for(auto box:obstacles()){float hit=0;if(rayBox(anchor,direction,expanded(box,.16f),hit,distance))safe=std::min(safe,std::max(.02f,hit-.03f));}
    safeDistance=std::min(safeDistance,safe);safeDistance+=(safe-safeDistance)*smooth(9,dt);
}
VisualState Game::visual()const{
    auto v=state;v.cells=challenge.displayedCells();Mode m=mode==Mode::Paused?pauseReturn:mode;
    v.characterVisible=m==Mode::Explore&&safeDistance>.65f;return v;
}
Hud Game::hud(int width,int height)const{
    Hud h;const Vec3 ink{.13f,.25f,.21f},paper{.98f,.96f,.90f},white{1,1,1},green{.13f,.34f,.27f};
    auto rect=[&](float x,float y,float w,float ht,Vec3 c,float a=1){h.rects.push_back({x,y,w,ht,c,a});};
    auto text=[&](float x,float y,std::wstring s,float size=18,Vec3 c=Vec3{1,1,1}){h.texts.push_back({x,y,std::move(s),size,c});};
    if(mode==Mode::Paused||confirmReset){rect(0,0,float(width),float(height),{.02f,.06f,.04f},.78f);
        float x=width*.5f-260,y=height*.5f-100;rect(x,y,520,200,paper,.99f);
        text(x+30,y+25,confirmReset?L"Recomeçar a trama?":L"Pausa",28,ink);
        text(x+30,y+78,confirmReset?L"Enter confirma. Esc preserva sua tentativa.":L"Esc continua. Sua tentativa está guardada.",18,ink);
        text(x+30,y+125,L"Ateliê do Filé  /  Modelagem 3D em OpenGL",16,green);return h;}
    rect(0,0,float(width),80,ink,.95f);text(24,13,L"Ateliê do Filé",28);text(25,49,L"MATEMÁTICA NAS TRAMAS  /  OPENGL",13,{.79f,.85f,.74f});
    Mode m=mode==Mode::Paused?pauseReturn:mode;
    text(float(width)-235,25,m==Mode::Explore?L"EXPLORAR O ATELIÊ":m==Mode::Work?L"TRABALHAR NA TRAMA":L"ENTENDER A CG",15);
    if(m==Mode::Work){
        rect(0,float(height)-124,float(width),124,paper,.98f);
        const Vec3 colors[]={{.80f,.39f,.31f},{.28f,.49f,.57f},{.83f,.64f,.24f}};
        const wchar_t* names[]={L"1  Coral",L"2  Azul",L"3  Amarelo"};
        for(int i=0;i<3;++i){float x=24.f+i*150;
            if(selected==i)rect(x-3,float(height)-111,146,50,green);
            rect(x,float(height)-108,140,44,colors[i]);text(x+12,float(height)-98,names[i],18,white);}
        text(490,float(height)-101,L"V  Conferir   X  Exemplo   R  Reiniciar",17,ink);
        text(24,float(height)-45,L"Clique: tecer   |   Botão direito: apagar   |   Q: voltar ao ateliê",16,ink);
        if(challenge.showingSolution){rect(24,97,430,46,green,.96f);text(40,109,L"EXEMPLO COMPLETO  /  X volta à tentativa",16);}
    }else if(m==Mode::Explore){
        rect(18,float(height)-80,520,62,ink,.90f);text(34,float(height)-64,L"WASD  Caminhar    Mouse  Olhar    E  Interagir",17);
        text(34,float(height)-39,L"Roda  Aproximar    Esc  Pausa    F1  Ajuda    F2  CG",14,{.80f,.86f,.76f});
        Target t=target();if(t!=Target::None){std::wstring s=t==Target::Board?L"E   Trabalhar no bastidor":t==Target::Lamp?(state.lampOn?L"E   Apagar a luminária":L"E   Acender a luminária"):(doorTarget<-.4f?L"E   Fechar a porta":L"E   Abrir a porta");
            rect(float(width)*.5f-190,float(height)-153,380,46,green,.98f);text(float(width)*.5f-170,float(height)-142,s,19);}
    }else if(m==Mode::CG){
        rect(24,102,352,238,paper,.97f);text(42,118,L"Transforme o novelo coral",22,ink);
        text(42,157,L"Setas: translação no plano XZ",17,ink);text(42,188,L"Z / C: rotação em Y",17,ink);
        text(42,219,L"- / =: escala    Backspace: restaurar",16,ink);text(42,252,L"F2: voltar ao modo anterior",16,green);
        wchar_t buffer[160];std::swprintf(buffer,160,L"X %.2f   Z %.2f   Escala %.2f",state.yarnOffset.x,state.yarnOffset.z,state.yarnScale);
        text(42,295,buffer,16,ink);
    }
    if(messageTime>0&&m!=Mode::CG&&!(m==Mode::Work&&challenge.showingSolution)){
        rect(24,92,float(std::min(width-48,940)),40,paper,.94f);text(38,103,message,16,ink);}
    if(help){rect(float(width)-410,150,386,251,paper,.98f);text(float(width)-390,168,L"Como explorar",23,ink);
        text(float(width)-390,207,L"Caminhe com WASD; olhe com o mouse.",16,ink);text(float(width)-390,238,L"Use E quando aparecer uma indicação.",16,ink);
        text(float(width)-390,269,L"Na trama, complete o lado direito.",16,ink);text(float(width)-390,300,L"F2 mostra as transformações do novelo.",16,ink);
        text(float(width)-390,331,L"F12 salva uma captura. F1 fecha a ajuda.",16,green);text(float(width)-390,365,L"Uma interpretação didática do filé.",15,ink);}
    return h;
}
}
