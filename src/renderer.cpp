#include "renderer.hpp"
#include "platform.hpp"
#include <GL/gl.h>
#include <wincodec.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace atelie {
namespace {
constexpr GLenum arrayBuffer=0x8892,elementBuffer=0x8893,staticDraw=0x88E4,dynamicDraw=0x88E8;
constexpr GLenum vertexShader=0x8B31,fragmentShader=0x8B30,compileStatus=0x8B81,linkStatus=0x8B82,infoLogLength=0x8B84;
constexpr GLenum texture0=0x84C0,srgbRGBA=0x8C43,redChannel=0x1903,red8=0x8229,clampToEdge=0x812F;
constexpr GLenum textureMaxLevel=0x813D;
using GLsizeiptr=std::ptrdiff_t;
bool validProc(PROC p){auto v=reinterpret_cast<std::intptr_t>(p);return p&&v!=1&&v!=2&&v!=3&&v!=-1;}
template<class T>T loadProc(const char* name){
    PROC p=wglGetProcAddress(name);
    if(!validProc(p))p=GetProcAddress(GetModuleHandleW(L"opengl32.dll"),name);
    if(!validProc(p))throw std::runtime_error(std::string("Missing OpenGL entry point: ")+name);
    return reinterpret_cast<T>(p);
}
// All functions newer than Microsoft's OpenGL 1.1 header are resolved explicitly.
struct GL {
#define GL_PROC(result,name,args) using name##Proc=result (APIENTRY*)args;name##Proc name=nullptr;
    GL_PROC(void,GenVertexArrays,(GLsizei,GLuint*))
    GL_PROC(void,BindVertexArray,(GLuint))
    GL_PROC(void,DeleteVertexArrays,(GLsizei,const GLuint*))
    GL_PROC(void,GenBuffers,(GLsizei,GLuint*))
    GL_PROC(void,BindBuffer,(GLenum,GLuint))
    GL_PROC(void,BufferData,(GLenum,GLsizeiptr,const void*,GLenum))
    GL_PROC(void,DeleteBuffers,(GLsizei,const GLuint*))
    GL_PROC(void,EnableVertexAttribArray,(GLuint))
    GL_PROC(void,VertexAttribPointer,(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*))
    GL_PROC(GLuint,CreateShader,(GLenum))
    GL_PROC(void,ShaderSource,(GLuint,GLsizei,const char*const*,const GLint*))
    GL_PROC(void,CompileShader,(GLuint))
    GL_PROC(void,GetShaderiv,(GLuint,GLenum,GLint*))
    GL_PROC(void,GetShaderInfoLog,(GLuint,GLsizei,GLsizei*,char*))
    GL_PROC(void,DeleteShader,(GLuint))
    GL_PROC(GLuint,CreateProgram,())
    GL_PROC(void,AttachShader,(GLuint,GLuint))
    GL_PROC(void,LinkProgram,(GLuint))
    GL_PROC(void,GetProgramiv,(GLuint,GLenum,GLint*))
    GL_PROC(void,GetProgramInfoLog,(GLuint,GLsizei,GLsizei*,char*))
    GL_PROC(void,DeleteProgram,(GLuint))
    GL_PROC(void,UseProgram,(GLuint))
    GL_PROC(GLint,GetUniformLocation,(GLuint,const char*))
    GL_PROC(void,Uniform1i,(GLint,GLint))
    GL_PROC(void,Uniform1f,(GLint,GLfloat))
    GL_PROC(void,Uniform2f,(GLint,GLfloat,GLfloat))
    GL_PROC(void,Uniform3f,(GLint,GLfloat,GLfloat,GLfloat))
    GL_PROC(void,UniformMatrix4fv,(GLint,GLsizei,GLboolean,const GLfloat*))
    GL_PROC(void,ActiveTexture,(GLenum))
    GL_PROC(void,GenerateMipmap,(GLenum))
#undef GL_PROC
    void load(){
#define ATELIE_LOAD_GL(name) name=loadProc<name##Proc>("gl" #name);
        ATELIE_LOAD_GL(GenVertexArrays) ATELIE_LOAD_GL(BindVertexArray) ATELIE_LOAD_GL(DeleteVertexArrays)
        ATELIE_LOAD_GL(GenBuffers) ATELIE_LOAD_GL(BindBuffer) ATELIE_LOAD_GL(BufferData) ATELIE_LOAD_GL(DeleteBuffers)
        ATELIE_LOAD_GL(EnableVertexAttribArray) ATELIE_LOAD_GL(VertexAttribPointer)
        ATELIE_LOAD_GL(CreateShader) ATELIE_LOAD_GL(ShaderSource) ATELIE_LOAD_GL(CompileShader) ATELIE_LOAD_GL(GetShaderiv) ATELIE_LOAD_GL(GetShaderInfoLog) ATELIE_LOAD_GL(DeleteShader)
        ATELIE_LOAD_GL(CreateProgram) ATELIE_LOAD_GL(AttachShader) ATELIE_LOAD_GL(LinkProgram) ATELIE_LOAD_GL(GetProgramiv) ATELIE_LOAD_GL(GetProgramInfoLog) ATELIE_LOAD_GL(DeleteProgram) ATELIE_LOAD_GL(UseProgram)
        ATELIE_LOAD_GL(GetUniformLocation) ATELIE_LOAD_GL(Uniform1i) ATELIE_LOAD_GL(Uniform1f) ATELIE_LOAD_GL(Uniform2f) ATELIE_LOAD_GL(Uniform3f) ATELIE_LOAD_GL(UniformMatrix4fv)
        ATELIE_LOAD_GL(ActiveTexture) ATELIE_LOAD_GL(GenerateMipmap)
#undef ATELIE_LOAD_GL
    }
};
std::string readText(const std::filesystem::path& path){
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("Cannot open shader: "+path.string());
    std::ostringstream source;source<<input.rdbuf();
    std::string value=source.str();if(value.empty())throw std::runtime_error("Empty shader: "+path.string());
    // UTF-8 BOM is accepted in editable files but not passed to the GLSL compiler.
    if(value.size()>=3&&static_cast<unsigned char>(value[0])==0xef&&static_cast<unsigned char>(value[1])==0xbb&&static_cast<unsigned char>(value[2])==0xbf)value.erase(0,3);
    return value;
}
GLuint compile(GL& gl,GLenum kind,const std::filesystem::path& file){
    std::string source=readText(file);const char* data=source.c_str();GLuint shader=gl.CreateShader(kind);
    if(!shader)throw std::runtime_error("Cannot allocate shader: "+file.string());
    gl.ShaderSource(shader,1,&data,nullptr);gl.CompileShader(shader);
    GLint success=0;gl.GetShaderiv(shader,compileStatus,&success);
    if(!success){
        GLint length=0;gl.GetShaderiv(shader,infoLogLength,&length);std::string log(static_cast<std::size_t>(std::max(1,length)),'\0');
        gl.GetShaderInfoLog(shader,static_cast<GLsizei>(log.size()),nullptr,log.data());gl.DeleteShader(shader);
        throw std::runtime_error("GLSL compilation failed in "+file.string()+":\n"+log);
    }
    return shader;
}
GLuint program(GL& gl,const std::filesystem::path& vertex,const std::filesystem::path& fragment){
    GLuint vs=0,fs=0,result=0;
    try{
        vs=compile(gl,vertexShader,vertex);fs=compile(gl,fragmentShader,fragment);result=gl.CreateProgram();
        if(!result)throw std::runtime_error("Cannot allocate an OpenGL shader program.");
        gl.AttachShader(result,vs);gl.AttachShader(result,fs);gl.LinkProgram(result);
        GLint success=0;gl.GetProgramiv(result,linkStatus,&success);
        if(!success){
            GLint length=0;gl.GetProgramiv(result,infoLogLength,&length);std::string log(static_cast<std::size_t>(std::max(1,length)),'\0');
            gl.GetProgramInfoLog(result,static_cast<GLsizei>(log.size()),nullptr,log.data());
            throw std::runtime_error("GLSL link failed ("+vertex.filename().string()+", "+fragment.filename().string()+"):\n"+log);
        }
        gl.DeleteShader(vs);gl.DeleteShader(fs);return result;
    }catch(...){if(vs)gl.DeleteShader(vs);if(fs)gl.DeleteShader(fs);if(result)gl.DeleteProgram(result);throw;}
}
float noise(int x,int y){
    std::uint32_t h=static_cast<std::uint32_t>(x)*374761393u+static_cast<std::uint32_t>(y)*668265263u+1619u;
    h=(h^(h>>13))*1274126177u;return static_cast<float>((h^(h>>16))&65535u)/65535.f;
}
std::vector<unsigned char> proceduralTexture(int kind,int size){
    std::vector<unsigned char> pixels(static_cast<std::size_t>(size)*size*4);
    for(int y=0;y<size;++y)for(int x=0;x<size;++x){
        float value=1.f,n=noise(x,y);
        if(kind==Wood){
            float warp=3.f*std::sin(y*.045f)+1.5f*std::sin(y*.11f);
            float grain=std::sin((x+warp)*.55f)+.5f*std::sin((x+warp)*1.42f);
            value=.79f+.06f*grain+.055f*(n-.5f);
            if(x%64<2)value-=.10f;
        }else if(kind==Fabric){
            float a=(x%6<3)?.08f:-.05f,b=(y%6<3)?.06f:-.04f;
            value=.86f+a+b+.035f*(n-.5f);
        }else if(kind==Plaster)value=.90f+.075f*(n-.5f)+.014f*std::sin(x*.13f)*std::cos(y*.11f);
        else if(kind==Tiles){
            bool grout=(x%64<3)||(y%64<3);value=grout?.56f:.93f+.025f*(n-.5f);
        }else if(kind==Yarn)value=.81f+.11f*std::sin((x+y*.45f)*1.30f)+.045f*(n-.5f);
        auto gray=static_cast<unsigned char>(clamp(value,0.f,1.f)*255);
        std::size_t i=(static_cast<std::size_t>(y)*size+x)*4;pixels[i]=pixels[i+1]=pixels[i+2]=gray;pixels[i+3]=255;
    }
    return pixels;
}
template<class T>void releaseCOM(T*& p){if(p){p->Release();p=nullptr;}}
bool readImage(IWICImagingFactory* factory,const std::filesystem::path& path,int& width,int& height,std::vector<unsigned char>& rgba){
    if(!factory||!std::filesystem::exists(path))return false;
    IWICBitmapDecoder* decoder=nullptr;IWICBitmapFrameDecode* frame=nullptr;
    IWICFormatConverter* converter=nullptr;IWICBitmapScaler* scaler=nullptr;bool success=false;
    auto clean=[&](){releaseCOM(converter);releaseCOM(scaler);releaseCOM(frame);releaseCOM(decoder);};
    try{
        HRESULT hr=factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder);
        if(FAILED(hr)){clean();return false;}
        hr=decoder->GetFrame(0,&frame);if(FAILED(hr)){clean();return false;}
        UINT w=0,h=0;hr=frame->GetSize(&w,&h);if(FAILED(hr)||!w||!h){clean();return false;}
        IWICBitmapSource* source=frame;
        if(std::max(w,h)>1024){
            double factor=1024.0/std::max(w,h);w=std::max(1u,static_cast<UINT>(w*factor));h=std::max(1u,static_cast<UINT>(h*factor));
            hr=factory->CreateBitmapScaler(&scaler);
            if(SUCCEEDED(hr))hr=scaler->Initialize(frame,w,h,WICBitmapInterpolationModeFant);
            if(FAILED(hr)){clean();return false;}source=scaler;
        }
        hr=factory->CreateFormatConverter(&converter);
        if(SUCCEEDED(hr))hr=converter->Initialize(source,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
        if(SUCCEEDED(hr)){
            rgba.resize(static_cast<std::size_t>(w)*h*4);
            hr=converter->CopyPixels(nullptr,w*4,static_cast<UINT>(rgba.size()),rgba.data());
            if(SUCCEEDED(hr)){width=static_cast<int>(w);height=static_cast<int>(h);success=true;}
        }
        clean();return success;
    }catch(...){clean();throw;}
}
struct GpuMesh {GLuint vao=0,vbo=0,ebo=0;GLsizei count=0;};
struct FontAtlas {
    GLuint texture=0;int pixels=18,width=0,height=0,cellWidth=0,cellHeight=0;
    std::array<float,256> advance{};
};
struct HudVertex {float x,y,u,v,r,g,b,a;};
void appendQuad(std::vector<HudVertex>& target,float x,float y,float width,float height,float u0,float v0,float u1,float v1,Vec3 color,float alpha){
    HudVertex a{x,y,u0,v0,color.x,color.y,color.z,alpha},b{x+width,y,u1,v0,color.x,color.y,color.z,alpha};
    HudVertex c{x+width,y+height,u1,v1,color.x,color.y,color.z,alpha},d{x,y+height,u0,v1,color.x,color.y,color.z,alpha};
    target.insert(target.end(),{a,b,c,a,c,d});
}
std::string glString(GLenum kind){const auto* p=glGetString(kind);return p?reinterpret_cast<const char*>(p):"unavailable";}
void write16(std::ostream& s,std::uint16_t v){char b[]={static_cast<char>(v),static_cast<char>(v>>8)};s.write(b,2);}
void write32(std::ostream& s,std::uint32_t v){char b[]={static_cast<char>(v),static_cast<char>(v>>8),static_cast<char>(v>>16),static_cast<char>(v>>24)};s.write(b,4);}
}

struct Renderer::Impl {
    Window& window;GL gl;
    GLuint sceneProgram=0,hudProgram=0,hudVao=0,hudVbo=0;
    std::vector<GpuMesh> meshes;
    std::array<GLuint,6> textures{};
    std::map<int,FontAtlas> fonts;
    IWICImagingFactory* imageFactory=nullptr;bool ownCOM=false;
    struct SceneUniforms {GLint model,viewProjection,color,specular,shininess,uvScale,emission,eye,lamp,lampOn,texture;} uniforms{};
    GLint hudViewport=-1,hudTexture=-1;
    std::string information,lastDiagnostics="No OpenGL errors.";
    std::uint64_t renderSwapCount=0;int renderedWidth=0,renderedHeight=0;bool hasFrame=false;

    Impl(Window& w,const std::wstring& assets):window(w){
        try{
            if(wglGetCurrentContext()!=window.context()&&!wglMakeCurrent(window.dc(),window.context()))throw std::runtime_error("Cannot activate the rendering context.");
            gl.load();while(glGetError()!=GL_NO_ERROR){}
            GLint major=0,minor=0,profile=0;glGetIntegerv(0x821B,&major);glGetIntegerv(0x821C,&minor);glGetIntegerv(0x9126,&profile);
            if(major<3||(major==3&&minor<3)||!(profile&1))throw std::runtime_error("This application requires OpenGL 3.3 Core or later.");
            information="OpenGL "+glString(GL_VERSION)+" | "+glString(GL_VENDOR)+" | "+glString(GL_RENDERER);
            std::filesystem::path shaders=std::filesystem::path(assets)/L"shaders";
            sceneProgram=program(gl,shaders/L"scene.vert",shaders/L"scene.frag");
            hudProgram=program(gl,shaders/L"hud.vert",shaders/L"hud.frag");
            auto uniform=[&](const char* name){GLint location=gl.GetUniformLocation(sceneProgram,name);if(location<0)throw std::runtime_error(std::string("Missing scene shader uniform: ")+name);return location;};
            uniforms={uniform("uModel"),uniform("uViewProjection"),uniform("uColor"),uniform("uSpecular"),uniform("uShininess"),uniform("uUVScale"),uniform("uEmission"),uniform("uEye"),uniform("uLampPosition"),uniform("uLampOn"),uniform("uTexture")};
            hudViewport=gl.GetUniformLocation(hudProgram,"uViewport");hudTexture=gl.GetUniformLocation(hudProgram,"uAtlas");
            if(hudViewport<0||hudTexture<0)throw std::runtime_error("Missing HUD shader uniforms.");
            gl.GenVertexArrays(1,&hudVao);gl.GenBuffers(1,&hudVbo);gl.BindVertexArray(hudVao);gl.BindBuffer(arrayBuffer,hudVbo);
            gl.EnableVertexAttribArray(0);gl.VertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(HudVertex),reinterpret_cast<void*>(offsetof(HudVertex,x)));
            gl.EnableVertexAttribArray(1);gl.VertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(HudVertex),reinterpret_cast<void*>(offsetof(HudVertex,u)));
            gl.EnableVertexAttribArray(2);gl.VertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(HudVertex),reinterpret_cast<void*>(offsetof(HudVertex,r)));
            gl.BindVertexArray(0);gl.BindBuffer(arrayBuffer,0);
            HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);ownCOM=SUCCEEDED(initialized);
            if(SUCCEEDED(initialized)||initialized==RPC_E_CHANGED_MODE){
                CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_IWICImagingFactory,reinterpret_cast<void**>(&imageFactory));
            }
            const wchar_t* names[]={L"",L"wood.jpg",L"fabric.jpg",L"plaster.jpg",L"tiles.jpg",L"yarn.jpg"};
            int loadedImages=0;
            glGenTextures(static_cast<GLsizei>(textures.size()),textures.data());
            for(int kind=0;kind<static_cast<int>(textures.size());++kind){
                int width=256,height=256;std::vector<unsigned char> pixels;
                bool loaded=kind!=Plain&&readImage(imageFactory,std::filesystem::path(assets)/L"textures"/names[kind],width,height,pixels);
                if(loaded)++loadedImages;else pixels=proceduralTexture(kind,width);
                glBindTexture(GL_TEXTURE_2D,textures[kind]);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
                glTexImage2D(GL_TEXTURE_2D,0,srgbRGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                gl.GenerateMipmap(GL_TEXTURE_2D);
            }
            information+=" | Textures: "+std::to_string(loadedImages)+" images, "+std::to_string(6-loadedImages)+" procedural";
            atlas(18);check("Renderer initialization");
        }catch(...){dispose();throw;}
    }
    ~Impl(){dispose();}
    void dispose(){
        if(window.context()&&wglGetCurrentContext()!=window.context())wglMakeCurrent(window.dc(),window.context());
        clearMeshes();
        for(auto& entry:fonts)if(entry.second.texture)glDeleteTextures(1,&entry.second.texture);
        fonts.clear();glDeleteTextures(static_cast<GLsizei>(textures.size()),textures.data());textures.fill(0);
        if(hudVbo&&gl.DeleteBuffers)gl.DeleteBuffers(1,&hudVbo);if(hudVao&&gl.DeleteVertexArrays)gl.DeleteVertexArrays(1,&hudVao);
        if(sceneProgram&&gl.DeleteProgram)gl.DeleteProgram(sceneProgram);if(hudProgram&&gl.DeleteProgram)gl.DeleteProgram(hudProgram);
        hudVbo=hudVao=sceneProgram=hudProgram=0;releaseCOM(imageFactory);if(ownCOM){CoUninitialize();ownCOM=false;}
    }
    void clearMeshes(){
        for(auto& m:meshes){if(m.ebo&&gl.DeleteBuffers)gl.DeleteBuffers(1,&m.ebo);if(m.vbo&&gl.DeleteBuffers)gl.DeleteBuffers(1,&m.vbo);if(m.vao&&gl.DeleteVertexArrays)gl.DeleteVertexArrays(1,&m.vao);}meshes.clear();
    }
    void check(const char* stage){
        std::ostringstream errors;bool found=false;
        for(int i=0;i<16;++i){GLenum error=glGetError();if(error==GL_NO_ERROR)break;found=true;errors<<" 0x"<<std::hex<<std::uppercase<<error;}
        if(found){lastDiagnostics=std::string(stage)+": OpenGL error(s)"+errors.str();throw std::runtime_error(lastDiagnostics);}
        lastDiagnostics="No OpenGL errors.";
    }
    FontAtlas& atlas(int pixelSize){
        pixelSize=std::max(10,std::min(48,pixelSize));auto known=fonts.find(pixelSize);if(known!=fonts.end())return known->second;
        FontAtlas font;font.pixels=pixelSize;font.cellWidth=pixelSize+10;font.cellHeight=pixelSize+10;font.width=font.cellWidth*16;font.height=font.cellHeight*16;
        HDC dc=CreateCompatibleDC(nullptr);if(!dc)throw std::runtime_error("Cannot create the HUD font DC.");
        BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmap.bmiHeader.biWidth=font.width;bitmap.bmiHeader.biHeight=-font.height;
        bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;bitmap.bmiHeader.biCompression=BI_RGB;
        void* bitmapPixels=nullptr;HBITMAP dib=CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&bitmapPixels,nullptr,0);
        HFONT face=CreateFontW(-pixelSize,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        if(!dib||!face){if(dib)DeleteObject(dib);if(face)DeleteObject(face);DeleteDC(dc);throw std::runtime_error("Cannot rasterize the HUD font.");}
        HGDIOBJ oldBitmap=SelectObject(dc,dib),oldFont=SelectObject(dc,face);
        auto clean=[&](){SelectObject(dc,oldFont);SelectObject(dc,oldBitmap);DeleteObject(face);DeleteObject(dib);DeleteDC(dc);};
        try{
            std::memset(bitmapPixels,0,static_cast<std::size_t>(font.width)*font.height*4);SetTextColor(dc,RGB(255,255,255));SetBkColor(dc,RGB(0,0,0));SetBkMode(dc,OPAQUE);
            for(int code=32;code<256;++code){
                wchar_t character=static_cast<wchar_t>(code);int x=(code%16)*font.cellWidth+3,y=(code/16)*font.cellHeight+3;
                TextOutW(dc,x,y,&character,1);SIZE extent{};GetTextExtentPoint32W(dc,&character,1,&extent);font.advance[code]=static_cast<float>(extent.cx);
            }
            GdiFlush();auto* bgra=static_cast<const unsigned char*>(bitmapPixels);
            std::vector<unsigned char> coverage(static_cast<std::size_t>(font.width)*font.height);
            for(std::size_t i=0;i<coverage.size();++i)coverage[i]=std::max(bgra[i*4],std::max(bgra[i*4+1],bgra[i*4+2]));
            // A white texel also allows rectangles to use the same shader and buffer.
            for(int y=0;y<3;++y)for(int x=0;x<3;++x)coverage[static_cast<std::size_t>(y)*font.width+x]=255;
            glGenTextures(1,&font.texture);glBindTexture(GL_TEXTURE_2D,font.texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
            glTexImage2D(GL_TEXTURE_2D,0,red8,font.width,font.height,0,redChannel,GL_UNSIGNED_BYTE,coverage.data());
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,clampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,clampToEdge);glTexParameteri(GL_TEXTURE_2D,textureMaxLevel,0);
            auto inserted=fonts.emplace(pixelSize,std::move(font));clean();return inserted.first->second;
        }catch(...){if(font.texture)glDeleteTextures(1,&font.texture);clean();throw;}
    }
    void drawHud(const Hud& hud){
        glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        gl.UseProgram(hudProgram);gl.Uniform2f(hudViewport,static_cast<float>(window.width),static_cast<float>(window.height));gl.Uniform1i(hudTexture,0);gl.ActiveTexture(texture0);gl.BindVertexArray(hudVao);
        std::vector<HudVertex> vertices;vertices.reserve(2048);FontAtlas* current=&atlas(18);
        auto flush=[&](){
            if(vertices.empty())return;
            glBindTexture(GL_TEXTURE_2D,current->texture);gl.BindBuffer(arrayBuffer,hudVbo);
            gl.BufferData(arrayBuffer,static_cast<GLsizeiptr>(vertices.size()*sizeof(HudVertex)),vertices.data(),dynamicDraw);
            glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(vertices.size()));vertices.clear();
        };
        float whiteU=1.5f/current->width,whiteV=1.5f/current->height;
        for(const auto& r:hud.rects)if(r.width>0&&r.height>0)appendQuad(vertices,r.x,r.y,r.width,r.height,whiteU,whiteV,whiteU,whiteV,r.color,clamp(r.alpha,0.f,1.f));
        for(const auto& text:hud.texts){
            if(text.size<=0)continue;
            FontAtlas& font=atlas(static_cast<int>(std::lround(text.size)));
            if(&font!=current){flush();current=&font;}
            float factor=text.size/font.pixels,x=text.x,y=text.y;
            for(wchar_t character:text.text){
                if(character==L'\n'){x=text.x;y+=text.size*1.35f;continue;}
                if(character==L'\r')continue;
                if(character==L'\t'){x+=font.advance[L' ']*factor*4;continue;}
                unsigned code=character>=32&&character<256?static_cast<unsigned>(character):static_cast<unsigned>('?');
                float u0=static_cast<float>((code%16)*font.cellWidth)/font.width,v0=static_cast<float>((code/16)*font.cellHeight)/font.height;
                float u1=u0+static_cast<float>(font.cellWidth)/font.width,v1=v0+static_cast<float>(font.cellHeight)/font.height;
                appendQuad(vertices,x-3*factor,y-3*factor,font.cellWidth*factor,font.cellHeight*factor,u0,v0,u1,v1,text.color,1.f);x+=font.advance[code]*factor;
            }
        }
        flush();gl.BindVertexArray(0);gl.BindBuffer(arrayBuffer,0);glDisable(GL_BLEND);glDepthMask(GL_TRUE);glEnable(GL_DEPTH_TEST);
    }
};

Renderer::Renderer(Window& window,const std::wstring& assets):impl_(std::make_unique<Impl>(window,assets)){}
Renderer::~Renderer()=default;
void Renderer::upload(const Scene& scene){
    auto& p=*impl_;p.clearMeshes();
    try{
        for(const auto& data:scene.meshes){
            if(data.vertices.empty()||data.indices.empty())throw std::runtime_error("A scene mesh has no vertices or triangles.");
            if(data.indices.size()>static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()))throw std::runtime_error("A scene mesh exceeds the OpenGL index limit.");
            for(auto index:data.indices)if(index>=data.vertices.size())throw std::runtime_error("A scene mesh contains an invalid vertex index.");
            p.meshes.emplace_back();auto& mesh=p.meshes.back();mesh.count=static_cast<GLsizei>(data.indices.size());
            p.gl.GenVertexArrays(1,&mesh.vao);p.gl.GenBuffers(1,&mesh.vbo);p.gl.GenBuffers(1,&mesh.ebo);p.gl.BindVertexArray(mesh.vao);
            p.gl.BindBuffer(arrayBuffer,mesh.vbo);p.gl.BufferData(arrayBuffer,static_cast<GLsizeiptr>(data.vertices.size()*sizeof(Vertex)),data.vertices.data(),staticDraw);
            p.gl.BindBuffer(elementBuffer,mesh.ebo);p.gl.BufferData(elementBuffer,static_cast<GLsizeiptr>(data.indices.size()*sizeof(std::uint32_t)),data.indices.data(),staticDraw);
            p.gl.EnableVertexAttribArray(0);p.gl.VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,position)));
            p.gl.EnableVertexAttribArray(1);p.gl.VertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,normal)));
            p.gl.EnableVertexAttribArray(2);p.gl.VertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,uv)));
        }
        p.gl.BindVertexArray(0);p.gl.BindBuffer(arrayBuffer,0);p.check("Mesh upload");
    }catch(...){p.gl.BindVertexArray(0);p.clearMeshes();throw;}
}
void Renderer::render(const std::vector<DrawItem>& items,const Camera& camera,bool lampOn,Vec3 lampPosition,const Hud& hud){
    auto& p=*impl_;if(p.window.width<=0||p.window.height<=0)return;
    glViewport(0,0,p.window.width,p.window.height);glClearColor(.84f,.86f,.86f,1.f);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);p.gl.UseProgram(p.sceneProgram);
    Mat4 vp=perspective(camera.fov,static_cast<float>(p.window.width)/p.window.height)*lookAt(camera.eye,camera.target);
    p.gl.UniformMatrix4fv(p.uniforms.viewProjection,1,GL_FALSE,vp.m.data());p.gl.Uniform3f(p.uniforms.eye,camera.eye.x,camera.eye.y,camera.eye.z);
    p.gl.Uniform3f(p.uniforms.lamp,lampPosition.x,lampPosition.y,lampPosition.z);p.gl.Uniform1i(p.uniforms.lampOn,lampOn?1:0);
    p.gl.ActiveTexture(texture0);p.gl.Uniform1i(p.uniforms.texture,0);
    GLuint lastVao=0,lastTexture=0;
    for(const auto& item:items){
        if(item.mesh<0||static_cast<std::size_t>(item.mesh)>=p.meshes.size())throw std::runtime_error("Invalid mesh for scene object: "+item.name);
        const auto& mesh=p.meshes[static_cast<std::size_t>(item.mesh)];
        GLuint texture=p.textures[static_cast<std::size_t>(std::max(0,std::min(5,item.material.texture)))];
        if(lastTexture!=texture){glBindTexture(GL_TEXTURE_2D,texture);lastTexture=texture;}
        if(lastVao!=mesh.vao){p.gl.BindVertexArray(mesh.vao);lastVao=mesh.vao;}
        const auto& m=item.material;p.gl.UniformMatrix4fv(p.uniforms.model,1,GL_FALSE,item.model.m.data());
        p.gl.Uniform3f(p.uniforms.color,m.color.x,m.color.y,m.color.z);p.gl.Uniform1f(p.uniforms.specular,m.specular);
        p.gl.Uniform1f(p.uniforms.shininess,m.shininess);p.gl.Uniform1f(p.uniforms.uvScale,m.uvScale);p.gl.Uniform1f(p.uniforms.emission,m.emission);
        glDrawElements(GL_TRIANGLES,mesh.count,GL_UNSIGNED_INT,nullptr);
    }
    p.gl.BindVertexArray(0);p.drawHud(hud);p.gl.UseProgram(0);p.check("Frame rendering");
    p.renderSwapCount=p.window.swapCount();p.renderedWidth=p.window.width;p.renderedHeight=p.window.height;p.hasFrame=true;
}
void Renderer::screenshot(const std::wstring& file){
    auto& p=*impl_;if(!p.hasFrame)throw std::runtime_error("Render a frame before requesting a screenshot.");
    if(p.renderedWidth!=p.window.width||p.renderedHeight!=p.window.height)throw std::runtime_error("Render again after resizing before taking a screenshot.");
    int width=p.renderedWidth,height=p.renderedHeight;std::size_t row=static_cast<std::size_t>(width)*3,stride=(row+3)&~std::size_t(3);
    std::uint64_t imageSize=static_cast<std::uint64_t>(stride)*height;
    if(imageSize>std::numeric_limits<std::uint32_t>::max()-54)throw std::runtime_error("Screenshot dimensions exceed BMP limits.");
    std::vector<unsigned char> rgb(row*height);glFinish();glPixelStorei(GL_PACK_ALIGNMENT,1);
    GLint previousRead=GL_BACK;glGetIntegerv(GL_READ_BUFFER,&previousRead);
    glReadBuffer(p.window.swapCount()==p.renderSwapCount?GL_BACK:GL_FRONT);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());glReadBuffer(static_cast<GLenum>(previousRead));
    p.check("Screenshot readback");
    std::ofstream output(std::filesystem::path(file),std::ios::binary);if(!output)throw std::runtime_error("Cannot open screenshot output file.");
    output.put('B');output.put('M');write32(output,static_cast<std::uint32_t>(imageSize)+54);write32(output,0);write32(output,54);
    write32(output,40);write32(output,static_cast<std::uint32_t>(width));write32(output,static_cast<std::uint32_t>(height));write16(output,1);write16(output,24);
    write32(output,0);write32(output,static_cast<std::uint32_t>(imageSize));write32(output,2835);write32(output,2835);write32(output,0);write32(output,0);
    std::vector<unsigned char> bgr(stride,0);
    for(int y=0;y<height;++y){
        const auto* source=rgb.data()+static_cast<std::size_t>(y)*row;
        for(int x=0;x<width;++x){bgr[x*3]=source[x*3+2];bgr[x*3+1]=source[x*3+1];bgr[x*3+2]=source[x*3];}
        output.write(reinterpret_cast<const char*>(bgr.data()),static_cast<std::streamsize>(bgr.size()));
    }
    if(!output)throw std::runtime_error("Cannot finish writing the screenshot.");
}
std::string Renderer::info()const{return impl_->information;}
std::string Renderer::diagnostics()const{return impl_->lastDiagnostics;}
}
