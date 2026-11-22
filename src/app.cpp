#include "app.hpp"
#include "gl_api.hpp"

#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string_view>
#include <utility>

namespace tumpuk {
namespace {
struct Glyph { int u,v,w,h,left,top,advance; };
#include "home_font_atlas.inc"
GLuint fontTex = 0;

struct Color { float r, g, b, a = 1; };
constexpr Color color(unsigned int rgb, float a = 1) {
    return {((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, a};
}
constexpr Color paper = color(0xF1F1FA), muted = color(0xADB3CB);
constexpr Color mint = color(0x81E6CD);
constexpr Color ink = color(0x0F1727), violet = color(0xB8A8FF);
constexpr Color logoBase = color(0x263149);
constexpr Color blush = color(0xFFA99D);
constexpr float pi = 3.14159265f;
constexpr float arenaLeft = 350, arenaX = 802;
constexpr float homeMargin = 64, homeGutter = 40;
constexpr float homeContentWidth = screenW-2*homeMargin-homeGutter;
constexpr float homeArtX = screenW-homeMargin-homeContentWidth*7/24;
constexpr float previewHold = 1.25f, previewResetTime = 0.75f;
constexpr float previewTurnRate = 0.45f;
constexpr int previewLayers = 5;
constexpr GLdouble floorPlane[]{0,1,0,0};

bool menuTheme(Screen screen) { return screen==Screen::Menu||screen==Screen::Credits; }

void shade(Color c) { glColor4f(c.r,c.g,c.b,c.a); }
Color fade(Color c, float a) { c.a = a; return c; }
Color tint(Color c, float factor) {
    return {std::min(1.0f,c.r*factor),std::min(1.0f,c.g*factor),std::min(1.0f,c.b*factor),c.a};
}

constexpr Color mix(Color from,Color to,float t) {
    return {from.r+(to.r-from.r)*t,from.g+(to.g-from.g)*t,
            from.b+(to.b-from.b)*t,from.a+(to.a-from.a)*t};
}

void rect(float x, float y, float w, float h, Color c) {
    shade(c);
    glBegin(GL_QUADS);
    glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h);
    glEnd();
}
void roundRect(float x, float y, float w, float h, float r, Color c) {
    r = std::min({r,w*0.5f,h*0.5f});
    shade(c);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x+w*0.5f,y+h*0.5f);
    const std::array<float,4> xs{x+w-r,x+w-r,x+r,x+r}, ys{y+r,y+h-r,y+h-r,y+r};
    for (int corner=0; corner<4; ++corner) {
        for (int s=0; s<=8; ++s) {
            const float a = (-90.0f+corner*90.0f+s*11.25f)*pi/180;
            glVertex2f(xs[corner]+r*std::cos(a),ys[corner]+r*std::sin(a));
        }
    }
    glVertex2f(x+w-r,y);
    glEnd();
}
void blockButton(float x,float y,float w,float h,float depth,Color face) {
    // The darker lower and right faces give the button the thickness of a block.
    rect(x,y,w,h,tint(face,0.56f));
    rect(x,y,w-4,h-depth,face);
    rect(x,y,w-4,2,tint(face,1.35f));
    rect(x+w-4,y,4,h-depth,tint(face,0.79f));
    rect(x+3,y+h-depth+1,w-10,1,tint(face,0.72f));
}
void glow(float x,float y,float rx,float ry,Color c) {
    glBegin(GL_TRIANGLE_FAN);
    shade(c); glVertex2f(x,y);
    shade(fade(c,0));
    for (int i=0;i<=64;++i) {
        const float a=2*pi*i/64;
        glVertex2f(x+rx*std::cos(a),y+ry*std::sin(a));
    }
    glEnd();
}
void gradient(bool home) {
    glBegin(GL_QUADS);
    shade(home?tint(logoBase,0.63f):color(0x12192D));
    glVertex2f(0,0); glVertex2f(screenW,0);
    shade(home?tint(logoBase,0.45f):color(0x131324));
    glVertex2f(screenW,screenH); glVertex2f(0,screenH);
    glEnd();
    glow(886,347,576,374,home?fade(violet,0.14f):color(0x6554AA,0.27f));
    glow(997,585,363,157,home?fade(violet,0.08f):color(0x534A8C,0.20f));
    for (int x=528;x<1234;x+=31)
        for (int y=118;y<645;y+=31)
            rect(static_cast<float>(x),static_cast<float>(y),1.5f,1.5f,color(0xA2A2C4,0.12f));
}

int glyph(unsigned char ch, bool bold) {
    return (bold ? 95 : 0) + (ch>=32 && ch<=126 ? ch : '?')-32;
}
float textWidth(std::string_view text, float size, bool bold=false, float tracking=0) {
    float width=0;
    for (unsigned char ch : text) {
        if ((ch & 0xc0)==0x80) continue;
        width += glyphData[glyph(ch,bold)].advance*size/(16.0f*fontMaster)+tracking;
    }
    return std::max(0.0f,width-tracking);
}
void text(float x,float baseline,std::string_view value,float size,Color c=paper,
          bool bold=false,float tracking=0) {
    const float scale=size/fontMaster;
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,fontTex); shade(c);
    glBegin(GL_QUADS);
    for (unsigned char ch : value) {
        if ((ch & 0xc0)==0x80) continue;
        const auto& g=glyphData[glyph(ch,bold)];
        if (g.w>0 && g.h>0) {
            const float x0=x+(g.left-1)*scale,y0=baseline+(g.top-1)*scale;
            const float x1=x0+(g.w+2)*scale,y1=y0+(g.h+2)*scale;
            const float u0=(g.u-1.0f)/atlasW,v0=(g.v-1.0f)/atlasH;
            const float u1=(g.u+g.w+1.0f)/atlasW,v1=(g.v+g.h+1.0f)/atlasH;
            glTexCoord2f(u0,v0); glVertex2f(x0,y0);
            glTexCoord2f(u1,v0); glVertex2f(x1,y0);
            glTexCoord2f(u1,v1); glVertex2f(x1,y1);
            glTexCoord2f(u0,v1); glVertex2f(x0,y1);
        }
        x+=g.advance*scale/16.0f+tracking;
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
}
void center(float x,float y,std::string_view value,float size,Color c=paper,bool bold=false) {
    text(x-textWidth(value,size,bold)*0.5f,y,value,size,c,bold);
}
std::string fitText(std::string value,float size,float width,bool bold=false) {
    if (textWidth(value,size,bold)<=width) return value;
    while (!value.empty() && textWidth(value+"...",size,bold)>width) value.pop_back();
    return value+"...";
}
constexpr std::array<Color,12> palette{
    color(0x696897),color(0x8C83D3),color(0x9C91E5),color(0xBAA1ED),
    color(0xD5A5E6),color(0xE9AADB),color(0xF2B8C8),color(0xF9C4AE),
    color(0xF1D0A9),color(0xCEDCB2),color(0xA9DDC9),color(0x91CCDA)
};
void quad3(Color c,float ax,float ay,float az,float bx,float by,float bz,
           float cx,float cy,float cz,float dx,float dy,float dz) {
    shade(c);
    glBegin(GL_QUADS);
    glVertex3f(ax,ay,az); glVertex3f(bx,by,bz); glVertex3f(cx,cy,cz); glVertex3f(dx,dy,dz);
    glEnd();
}
struct BlockStyle {
    Color bottom, top;
    float height = blockH;
    bool outline = true;
};
void blockSide(const BlockStyle& style,float light,float ax,float az,float bx,float bz) {
    const float y=style.height*0.5f;
    glBegin(GL_QUADS);
    shade(tint(style.bottom,light));
    glVertex3f(ax,-y,az); glVertex3f(bx,-y,bz);
    shade(tint(style.top,light));
    glVertex3f(bx,y,bz); glVertex3f(ax,y,az);
    glEnd();
}
void cuboid(const Block& b,const BlockStyle& style,float rotation=0,Axis axis=Axis::X) {
    glPushMatrix();
    glTranslatef(b.x,b.y+style.height*0.5f,b.z);
    if (rotation!=0) glRotatef(rotation,axis==Axis::X?0.0f:1.0f,0,axis==Axis::X?1.0f:0.0f);
    const float x=b.w*0.5f, y=style.height*0.5f, z=b.d*0.5f;
    const Color top=style.outline?tint(style.top,1.11f):mix(style.top,paper,0.12f);
    quad3(top,-x,y,-z,x,y,-z,x,y,z,-x,y,z);
    quad3(tint(style.bottom,0.57f),-x,-y,-z,-x,-y,z,x,-y,z,x,-y,-z);
    blockSide(style,0.82f,-x,z,x,z);
    blockSide(style,0.68f,x,-z,x,z);
    blockSide(style,0.62f,-x,z,-x,-z);
    blockSide(style,0.74f,-x,-z,x,-z);
    if(style.outline) {
        shade(fade(tint(style.top,1.22f),0.54f));
        glBegin(GL_LINE_LOOP);
        glVertex3f(-x,y+0.001f,-z); glVertex3f(x,y+0.001f,-z);
        glVertex3f(x,y+0.001f,z); glVertex3f(-x,y+0.001f,z);
        glEnd();
    }
    glPopMatrix();
}
void cuboid(const Block& b,float rotation=0,Axis axis=Axis::X) {
    const auto index=static_cast<std::size_t>(b.tint)%palette.size();
    const Color top=palette[index];
    const Color bottom=b.tint==0?tint(top,0.90f):palette[(index+palette.size()-1)%palette.size()];
    cuboid(b,{bottom,top},rotation,axis);
}
void footprint(const Block& b, Color edge) {
    const float x=b.w*0.5f,z=b.d*0.5f,y=b.y+blockH+0.009f;
    shade(edge);
    glBegin(GL_LINE_LOOP);
    glVertex3f(b.x-x,y,b.z-z); glVertex3f(b.x+x,y,b.z-z);
    glVertex3f(b.x+x,y,b.z+z); glVertex3f(b.x-x,y,b.z+z);
    glEnd();
}
void baseGrid() {
    quad3(color(0x21263C),-4.0f,-0.015f,-4.0f,4.0f,-0.015f,-4.0f,
          4.0f,-0.015f,4.0f,-4.0f,-0.015f,4.0f);
    for(int i=-6;i<=6;++i) {
        const float a=i*0.65f;
        shade(color(0x7984A7,i==0?0.46f:0.22f));
        glBegin(GL_LINES);
        glVertex3f(a,0,-4.0f); glVertex3f(a,0,4.0f);
        glVertex3f(-4.0f,0,a); glVertex3f(4.0f,0,a);
        glEnd();
    }
}
void beginScene(float x,float y,float size,float camera,float time,bool preview) {
    glPushMatrix();
    glTranslatef(x,y,0);
    glScalef(size,-size,size);
    glRotatef(35.264f,1,0,0);
    glRotatef(-45.0f+(preview?5.5f*std::sin(time*previewTurnRate):0.0f),0,1,0);
    glTranslatef(0,-camera,0);
    glEnable(GL_DEPTH_TEST);
}
void endScene() { glDisable(GL_DEPTH_TEST); glPopMatrix(); }
float previewOffset(const Round& round) {
    const auto& top=round.tower.back();
    return (round.axis==Axis::X?round.active.x-top.x:round.active.z-top.z)*round.heading;
}
void previewBlock(Block block,float opacity,float rotation=0,Axis axis=Axis::X) {
    if(opacity<=0) return;
    const Color bottom=mix(mint,paper,0.22f*(block.tint-1)/previewLayers);
    const Color top=mix(mint,paper,0.22f*block.tint/previewLayers);
    cuboid(block,{fade(bottom,opacity),fade(top,opacity),blockH,false},rotation,axis);
}
void towerPreview(const Round& round,float time,float hold) {
    glow(homeArtX,463,318,132,fade(violet,0.16f));
    beginScene(homeArtX,447,56,0,time,true);
    baseGrid();
    glClipPlane(GL_CLIP_PLANE0,floorPlane);
    constexpr std::array<float,3> widths{baseSize+1.4f,baseSize+0.7f,baseSize};
    constexpr std::array<Color,4> tones{mix(logoBase,violet,0.78f),violet,mix(violet,mint,0.58f),mint};
    for(std::size_t i=0;i<widths.size();++i)
        cuboid({0,0,widths[i],widths[i],i*blockH},{tones[i],tones[i+1],blockH,false});

    // Align the support with the logo, then lower the finished stack into it.
    const float reset=std::clamp((hold-previewHold)/previewResetTime,0.0f,1.0f);
    const float sink=previewLayers*blockH*reset*reset*(3-2*reset);
    glTranslatef(0,blockH*(widths.size()-1)-sink,0);
    for(std::size_t i=1;i<round.tower.size();++i)
        previewBlock(round.tower[i],1);
    if(round.score<previewLayers) {
        const float entrance=std::clamp((round.range()+previewOffset(round))*1.6f,0.0f,1.0f);
        previewBlock(round.active,entrance);
    }
    glEnable(GL_CLIP_PLANE0);
    for(const auto& fragment:round.fragments)
        previewBlock(fragment.block,1,fragment.rotation,fragment.axis);
    glDisable(GL_CLIP_PLANE0);
    endScene();
}
void towerLive(const Round& r) {
    glow(arenaX,535,365,135,color(0xA393E4,0.19f));
    beginScene(arenaX,465,66,r.camera,0,false);
    if (r.camera<4.3f) baseGrid();
    for (const auto& b:r.tower) if (b.y>=r.camera-6.0f) cuboid(b);
    if (r.alive) {
        const float flash = r.lastLand==Land::Perfect ? std::max(0.0f,r.notice-0.65f)*4 : 0;
        footprint(r.tower.back(),fade(paper,0.78f+0.22f*flash));
        cuboid(r.active);
        if(flash>0) footprint(r.tower.back(),fade(mint,flash));
    }
    glClipPlane(GL_CLIP_PLANE0,floorPlane);
    glEnable(GL_CLIP_PLANE0);
    for (const auto& f:r.fragments) cuboid(f.block,f.rotation,f.axis);
    glDisable(GL_CLIP_PLANE0);
    endScene();
}
void brandMark(float x,float y,float size) {
    const float scale=size/36;
    roundRect(x,y,size,size,9*scale,logoBase);
    for(int i=0;i<3;++i)
        roundRect(x+(8+i*2.0f)*scale,y+(22-i*6.0f)*scale,
                  (20-i*4.0f)*scale,4*scale,1.6f*scale,i==2?mint:violet);
}
void crown(float x,float y) {
    shade(mint);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x+9,y+8);
    glVertex2f(x+2,y+8); glVertex2f(x,y+1);
    glVertex2f(x+5,y+4); glVertex2f(x+9,y);
    glVertex2f(x+13,y+4); glVertex2f(x+18,y+1);
    glVertex2f(x+16,y+8);
    glEnd();
    rect(x+2,y+9,14,2,mint);
}
std::string trim(std::string s) {
    constexpr auto whitespace = " \t\r\n";
    const auto first=s.find_first_not_of(whitespace);
    if(first==std::string::npos) return {};
    return s.substr(first,s.find_last_not_of(whitespace)-first+1);
}
}

std::filesystem::path scoreLocation(const std::filesystem::path& exeFolder) {
#ifdef _WIN32
    if (const wchar_t* root = _wgetenv(L"LOCALAPPDATA"))
        return std::filesystem::path(root)/L"Tumpuk"/L"score.txt";
#else
    if (const char* root = std::getenv("XDG_DATA_HOME"); root && *root)
        return std::filesystem::path(root)/"Tumpuk"/"score.txt";
    if (const char* home = std::getenv("HOME"); home && *home)
        return std::filesystem::path(home)/".local"/"share"/"Tumpuk"/"score.txt";
#endif
    return exeFolder/"score.txt";
}

App::App(const std::filesystem::path& creditsFile,const std::filesystem::path& scoreFile)
    : creditsFile_(creditsFile),scoreFile_(scoreFile) {
    resetPreview();
    loadCredits();
    std::ifstream file(scoreFile_);
    int stored=0;
    if(file>>stored) best_=std::clamp(stored,0,10000000);
}
void App::loadCredits() {
    people_.clear();
    std::ifstream file(creditsFile_);
    std::string line;
    while(std::getline(file,line) && people_.size()<96) {
        if(line.compare(0,3,"\xef\xbb\xbf")==0) line.erase(0,3);
        line=trim(line);
        if(line.empty() || line.front()=='#') continue;
        const auto sep=line.find('|');
        if(sep==std::string::npos) continue;
        auto name=trim(line.substr(0,sep)),nim=trim(line.substr(sep+1));
        if(!name.empty()&&!nim.empty()) people_.push_back({std::move(name),std::move(nim)});
    }
    if(people_.empty()) people_.push_back({"Affan Abyarahman","245150207111020"});
}
void App::saveBest() const {
    std::error_code ec;
    const auto parent=scoreFile_.parent_path();
    if(!parent.empty()) std::filesystem::create_directories(parent,ec);
    if(std::ofstream file(scoreFile_); file) file<<best_<<'\n';
}

void App::initGraphics() {
    std::vector<unsigned char> pixels;
    pixels.reserve(atlasW*atlasH);
    for(std::size_t i=0;i<sizeof(atlasRle);i+=2)
        pixels.insert(pixels.end(),atlasRle[i],atlasRle[i+1]);
    glGenTextures(1,&fontTex);
    glBindTexture(GL_TEXTURE_2D,fontTex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,GL_ALPHA,atlasW,atlasH,0,GL_ALPHA,GL_UNSIGNED_BYTE,pixels.data());
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LEQUAL);
}
void App::resize(int width,int height) {
    width_=std::max(1,width); height_=std::max(1,height);
    const float s=std::min(width_/screenW,height_/screenH);
    const int w=std::max(1,static_cast<int>(screenW*s+0.5f));
    const int h=std::max(1,static_cast<int>(screenH*s+0.5f));
    viewport_={(width_-w)/2,(height_-h)/2,w,h};
}
void App::pointer(int x,int y) {
    mouseX_=(x-viewport_.x)*screenW/viewport_.w;
    const int top=height_-viewport_.y-viewport_.h;
    mouseY_=(y-top)*screenH/viewport_.h;
}
void App::click(int x,int y) {
    pointer(x,y);
    if(mouseX_<0||mouseY_<0||mouseX_>=screenW||mouseY_>=screenH) return;
    for(auto it=buttons_.rbegin();it!=buttons_.rend();++it) {
        if(mouseX_>=it->x&&mouseX_<=it->x+it->w&&mouseY_>=it->y&&mouseY_<=it->y+it->h) {
            const Action picked=it->action;
            action(picked);
            return;
        }
    }
    if(screen_==Screen::Playing && mouseX_>=arenaLeft && mouseY_>=80) drop();
}
void App::start() {
    round_.reset();
    initialBest_=best_;
    screen_=Screen::Playing;
}
void App::drop() {
    if(!round_.drop()) return;
    if(round_.score>best_) { best_=round_.score; saveBest(); }
    if(!round_.alive) screen_=Screen::Result;
}
void App::action(Action picked) {
    buttons_.clear();
    switch(picked) {
    case Action::Play: case Action::Retry: start(); break;
    case Action::Credits: loadCredits(); creditsPage_=0; screen_=Screen::Credits; break;
    case Action::Exit: exit_=true; break;
    case Action::Resume: screen_=Screen::Playing; break;
    case Action::Pause: screen_=Screen::Pause; break;
    case Action::Home: screen_=Screen::Menu; break;
    case Action::Previous: if(creditsPage_>0) --creditsPage_; break;
    case Action::Next: if((creditsPage_+1)*5<people_.size()) ++creditsPage_; break;
    }
}
void App::key(unsigned char input) {
    const int k=std::tolower(input);
    switch(screen_) {
    case Screen::Menu:
        if(k=='\r'||k=='\n'||k==' ') action(Action::Play);
        else if(k=='c') action(Action::Credits);
        else if(k==27) action(Action::Exit);
        break;
    case Screen::Playing:
        if(k==' ') drop();
        else if(k==27||k=='p') action(Action::Pause);
        break;
    case Screen::Pause:
        if(k=='\r'||k=='\n'||k=='p'||k==27) action(Action::Resume);
        else if(k=='m') action(Action::Home);
        break;
    case Screen::Result:
        if(round_.resultTime<0.45f) break;
        if(k=='\r'||k=='\n'||k=='r') action(Action::Retry);
        else if(k==27||k=='m') action(Action::Home);
        break;
    case Screen::Credits:
        if(k==27||k=='\r'||k=='\n') action(Action::Home);
        else if(k=='a') action(Action::Previous);
        else if(k=='d') action(Action::Next);
        break;
    }
}
void App::update(float dt) {
    dt=std::clamp(dt,0.0f,0.05f);
    time_=std::fmod(time_+dt,2*pi/previewTurnRate);
    if(screen_==Screen::Menu) updatePreview(dt);
    else if(screen_==Screen::Playing||screen_==Screen::Result) round_.update(dt);
}

void App::resetPreview() {
    preview_.reset();
    previewWait_=0;
}
void App::updatePreview(float dt) {
    preview_.update(dt);
    if(preview_.score==previewLayers) {
        previewWait_+=dt;
        if(previewWait_>=previewHold+previewResetTime) resetPreview();
        return;
    }
    // Drop just before alignment so the real round logic trims the overhang.
    if(previewOffset(preview_)>=-0.72f) preview_.drop();
}

void App::button(float x,float y,float w,float h,Action a,const std::string& s,
                 bool primary,const std::string& hotkey) {
    buttons_.push_back({x,y,w,h,a});
    const bool hover=mouseX_>=x&&mouseX_<=x+w&&mouseY_>=y&&mouseY_<=y+h;
    const float depth=primary?8:6;
    const float middle=y+(h-depth)*0.5f;
    const Color secondary=menuTheme(screen_)?tint(violet,0.48f):color(0x222A42);
    const Color face=primary?mint:secondary;
    blockButton(x,y,w,h,depth,tint(face,hover?1.16f:1.0f));
    const Color foreground=primary?ink:paper;
    text(x+19,middle+6,s,primary?18:16,foreground,true);
    if(!hotkey.empty()) {
        const float capW=std::max(28.0f,textWidth(hotkey,11,true)+15);
        const float capX=x+w-capW-13;
        rect(capX,middle-11.5f,capW,23,tint(face,primary?0.86f:0.68f));
        center(capX+capW*0.5f,middle+4,hotkey,11,foreground,true);
    }
}
void App::menu() {
    towerPreview(preview_,time_,previewWait_);
    brandMark(homeMargin,48,44);
    text(homeMargin,290,"Tumpuk",72,paper,true);
    text(homeMargin,333,"Jatuhkan balok saat sejajar.",21,muted);
    constexpr float width=336, gap=16, secondary=(width-gap)/2;
    button(homeMargin,376,width,56,Action::Play,"Mulai",true,"ENTER");
    button(homeMargin,448,secondary,44,Action::Credits,"Kredit",false,"C");
    button(homeMargin+secondary+gap,448,secondary,44,Action::Exit,"Keluar",false,"ESC");
}
void App::gameplay() {
    towerLive(round_);
    if(screen_==Screen::Pause) {
        pause();
        return;
    }
    if(screen_==Screen::Result && round_.resultTime>=0.45f) {
        result();
        return;
    }
    text(homeMargin,128,"Skor",18,muted,true);
    text(homeMargin,204,std::to_string(round_.score),76,paper,true);
    text(homeMargin,237,"Rekor " + std::to_string(best_),19,muted);
    if(screen_==Screen::Playing) {
        button(screenW-homeMargin-132,48,132,46,Action::Pause,"Jeda",false,"ESC");
        if(round_.score==0)
            center(arenaX,688,"Space atau klik arena untuk menjatuhkan balok",20,paper);
        if(round_.notice>0) landingFeedback();
    }
}
void App::landingFeedback() {
    const bool perfect = round_.lastLand==Land::Perfect;
    std::string message = perfect ? "Pas!" :
        "Terpotong " + std::to_string(static_cast<int>(std::round(round_.cutPercent))) + "%";
    if(perfect && round_.combo>=2) message += "  x" + std::to_string(round_.combo);
    const float alpha = std::min(1.0f,round_.notice*3.3f);
    const float y = 296-std::min(round_.tower.back().y-round_.camera,1.7f)*48;
    center(arenaX+1,y+1,message,23,fade(ink,alpha),true);
    center(arenaX,y,message,23,fade(perfect?mint:blush,alpha),true);
}
void App::pause() {
    rect(0,0,screenW,screenH,color(0x080D1B,0.26f));
    text(homeMargin,292,"Jeda",58,paper,true);
    button(homeMargin,335,272,56,Action::Resume,"Lanjutkan",true,"ENTER");
    button(homeMargin,407,272,46,Action::Home,"Menu utama",false,"M");
}
void App::result() {
    const bool newRecord=round_.score>initialBest_;
    text(homeMargin,232,"Permainan selesai",34,paper,true);
    text(homeMargin,274,newRecord?"Rekor baru":"Skor akhir",18,mint,true);
    text(homeMargin,362,std::to_string(round_.score),88,paper,true);
    if(!newRecord) text(homeMargin,403,"Rekor " + std::to_string(best_),19,muted);
    text(homeMargin,newRecord?403:432,"Combo terbaik " + std::to_string(round_.longestCombo),17,muted);
    button(homeMargin,476,272,56,Action::Retry,"Coba lagi",true,"R");
    button(homeMargin,548,272,46,Action::Home,"Menu utama",false,"M");
}
void App::credits() {
    constexpr std::size_t perPage=5;
    constexpr float tableWidth=900, nameX=homeMargin+20, nimX=homeMargin+590;
    constexpr float tableY=248, headerHeight=46, rowHeight=49;
    const std::size_t first=creditsPage_*perPage;
    const std::size_t count=std::min(perPage,people_.size()-first);
    brandMark(homeMargin,48,44);
    text(homeMargin,170,"Kredit",58,paper,true);
    text(homeMargin,210,"Nama Tim: Ketuanya Ijal ajah",21,muted);
    rect(homeMargin,tableY,tableWidth,headerHeight,tint(logoBase,0.95f));
    rect(homeMargin,tableY,tableWidth,2,tint(violet,0.52f));
    text(nameX,tableY+29,"Nama",16,violet,true);
    text(nimX,tableY+29,"NIM",16,violet,true);
    for(std::size_t i=0;i<count;++i) {
        const auto& p=people_[first+i];
        const float y=tableY+headerHeight+i*rowHeight;
        rect(homeMargin,y,tableWidth,rowHeight,tint(logoBase,i%2==0?0.74f:0.83f));
        rect(nameX,y+rowHeight-1,tableWidth-40,1,fade(violet,0.14f));
        const auto name=fitText(p.name,20,nimX-nameX-48,true);
        text(nameX,y+31,name,20,paper,true);
        if(p.nim=="245150207111063") crown(nameX+textWidth(name,20,true)+10,y+17);
        text(nimX,y+31,fitText(p.nim,19,tableWidth-(nimX-homeMargin)-20),19,muted);
    }
    const float controlsY=tableY+headerHeight+count*rowHeight+25;
    button(homeMargin,controlsY,220,48,Action::Home,"Menu utama",false,"ESC");
    const std::size_t pages=(people_.size()+perPage-1)/perPage;
    if(pages>1) {
        center(740,controlsY+29,std::to_string(creditsPage_+1)+" / "+std::to_string(pages),16,muted,true);
        if(creditsPage_>0) button(540,controlsY,142,48,Action::Previous,"Sebelum",false,"A");
        if(creditsPage_+1<pages) button(816,controlsY,148,48,Action::Next,"Berikut",false,"D");
    }
}
void App::render() {
    buttons_.clear();
    glViewport(viewport_.x,viewport_.y,viewport_.w,viewport_.h);
    glClearColor(0.032f,0.043f,0.080f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0,screenW,screenH,0,-6000,6000);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    gradient(menuTheme(screen_));
    if(screen_==Screen::Menu) menu();
    else if(screen_==Screen::Credits) credits();
    else gameplay();
}
}
