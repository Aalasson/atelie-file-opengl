#pragma once
#include "core.hpp"
#include "challenge.hpp"
#include "platform.hpp"

namespace atelie {
enum class Mode { Explore, Work, Paused, CG };
enum class Target { None, Board, Lamp, Door };
class Game {
public:
    explicit Game(const Scene& s):scene(s){}
    void update(Window&,float dt,bool simulation=false);
    Camera camera() const;
    Hud hud(int width,int height) const;
    VisualState visual()const;
    std::vector<Aabb> obstacles() const;
    Target target() const;
    const Scene& scene;
    Challenge challenge;
    VisualState state;
    Mode mode=Mode::Explore, pauseReturn=Mode::Explore,cgReturn=Mode::Explore;
    Vec3 velocity{};
    float yaw=0,pitch=.26f,distance=3.05f,safeDistance=3.05f,doorTarget=0;
    int selected=0;
    bool help=false,confirmReset=false;
    std::wstring message=L"Caminhe até a bancada e pressione E para começar sua trama.";
    float messageTime=6;
private:
    void say(std::wstring text,float seconds=4){message=std::move(text);messageTime=seconds;}
};
}
