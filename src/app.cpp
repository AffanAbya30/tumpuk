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
#include "font_atlas.inc"

struct Color { float r, g, b, a = 1; };
constexpr Color color(unsigned int rgb, float a = 1) {
    return {((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, a};
}
constexpr Color paper = color(0xF1F1FA), muted = color(0xADB3CB);
constexpr Color dim = color(0x7D87A6), mint = color(0x81E6CD);
constexpr Color ink = color(0x0F1727), border = color(0x333C57);
constexpr Color panel = color(0x1A2137), violet = color(0xB8A8FF);
constexpr Color blush = color(0xFFA99D);
constexpr float pi = 3.14159265f;
GLuint fontTex = 0;

void shade(Color c) { glColor4f(c.r,c.g,c.b,c.a); }
Color fade(Color c, float a) { c.a = a; return c; }
Color tint(Color c, float factor) {
    return {std::min(1.0f,c.r*factor),std::min(1.0f,c.g*factor),std::min(1.0f,c.b*factor),c.a};
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
void card(float x,float y,float w,float h,float radius,Color fill=panel,Color stroke=border) {
    roundRect(x,y,w,h,radius,stroke);
    roundRect(x+1,y+1,w-2,h-2,radius-1,fill);
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
void gradient() {
    glBegin(GL_QUADS);
    shade(color(0x12192D)); glVertex2f(0,0); glVertex2f(screenW,0);
    shade(color(0x131324)); glVertex2f(screenW,screenH); glVertex2f(0,screenH);
    glEnd();
    glow(886,347,576,374,color(0x6554AA,0.27f));
    glow(997,585,363,157,color(0x534A8C,0.20f));
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
void label(float x,float y,std::string_view value,Color c=dim) {
    text(x,y,value,12,c,true,1.8f);
}
std::string fitText(std::string value,float size,float width,bool bold=false) {
    if (textWidth(value,size,bold)<=width) return value;
    while (!value.empty() && textWidth(value+"...",size,bold)>width) value.pop_back();
    return value+"...";
}
void keycap(float x,float y,float width,std::string_view key) {
    card(x,y,width,26,7,color(0x242D45));
    center(x+width/2,y+18,key,11,muted,true);
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
void cuboid(const Block& b, float rotation=0, Axis axis=Axis::X) {
    const Color c=palette[static_cast<std::size_t>(b.tint)%palette.size()];
    glPushMatrix();
    glTranslatef(b.x,b.y+blockH*0.5f,b.z);
    if (rotation!=0) glRotatef(rotation,axis==Axis::X?0.0f:1.0f,0,axis==Axis::X?1.0f:0.0f);
    const float x=b.w*0.5f, y=blockH*0.5f, z=b.d*0.5f;
    quad3(tint(c,1.11f),-x,y,-z,x,y,-z,x,y,z,-x,y,z);
    quad3(tint(c,0.57f),-x,-y,-z,-x,-y,z,x,-y,z,x,-y,-z);
    quad3(tint(c,0.82f),-x,-y,z,x,-y,z,x,y,z,-x,y,z);
    quad3(tint(c,0.68f),x,-y,-z,x,y,-z,x,y,z,x,-y,z);
    quad3(tint(c,0.62f),-x,-y,-z,-x,y,-z,-x,y,z,-x,-y,z);
    quad3(tint(c,0.74f),-x,-y,-z,x,-y,-z,x,y,-z,-x,y,-z);
    shade(fade(tint(c,1.22f),0.54f));
    glBegin(GL_LINE_LOOP);
    glVertex3f(-x,y+0.001f,-z); glVertex3f(x,y+0.001f,-z);
    glVertex3f(x,y+0.001f,z); glVertex3f(-x,y+0.001f,z);
    glEnd();
    glPopMatrix();
}
void footprint(const Block& b) {
    const float x=b.w*0.5f,z=b.d*0.5f,y=b.y+blockH+0.009f;
    shade(fade(paper,0.54f));
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
    glRotatef(-45.0f+(preview?3.0f*std::sin(time*0.45f):0.0f),0,1,0);
    glTranslatef(0,-camera,0);
    glEnable(GL_DEPTH_TEST);
}
void endScene() { glDisable(GL_DEPTH_TEST); glPopMatrix(); }
void towerPreview(float time) {
    glow(875,504,348,107,color(0xA393E4,0.21f));
    beginScene(872,460,58,0,time,true);
    baseGrid();
    constexpr std::array<float,10> widths{3.9f,3.8f,3.5f,3.3f,3.16f,2.94f,2.72f,2.72f,2.43f,2.43f};
    for (std::size_t i=0;i<widths.size();++i)
        cuboid({0.17f*std::sin(static_cast<float>(i)*0.81f),0.14f*std::cos(static_cast<float>(i)*0.92f),widths[i],
                widths[i]+0.10f,static_cast<float>(i)*blockH,static_cast<int>(i)});
    cuboid({0.86f*std::sin(time*1.6f),-0.08f,2.43f,2.53f,10*blockH+0.13f,10});
    endScene();
}
void towerLive(const Round& r) {
    glow(835,508,332,122,color(0xA393E4,0.19f));
    beginScene(836,449,57,r.camera,0,false);
    if (r.camera<4.3f) baseGrid();
    for (const auto& b:r.tower) if (b.y>=r.camera-6.0f) cuboid(b);
    if (r.alive) {
        footprint(r.tower.back());
        cuboid(r.active);
    }
    for (const auto& f:r.fragments) cuboid(f.block,f.rotation,f.axis);
    endScene();
}
void brandMark(float x,float y) {
    roundRect(x,y,36,36,9,color(0x263149));
    for(int i=0;i<3;++i)
        roundRect(x+8+i*2.0f,y+22-i*6.0f,20-i*4.0f,4,1.6f,i==2?mint:violet);
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
    if(screen_==Screen::Playing && mouseX_>=460 && mouseY_>=93 && mouseY_<650) drop();
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
    time_=std::fmod(time_+dt,1000.0f);
    if(screen_==Screen::Playing||screen_==Screen::Result) round_.update(dt);
}

void App::button(float x,float y,float w,float h,Action a,const std::string& s,
                 bool primary,const std::string& hotkey) {
    buttons_.push_back({x,y,w,h,a});
    const bool hover=mouseX_>=x&&mouseX_<=x+w&&mouseY_>=y&&mouseY_<=y+h;
    if(primary) {
        roundRect(x,y,w,h,12,hover?color(0xA0F5DF):mint);
        text(x+19,y+h*0.5f+6,s,17,ink,true);
    } else {
        card(x,y,w,h,12,hover?color(0x313A55):color(0x222A42),hover?color(0x7985A9):border);
        text(x+19,y+h*0.5f+6,s,16,paper,true);
    }
    if(!hotkey.empty()) {
        const float capW=std::max(28.0f,textWidth(hotkey,10,true)+15);
        const Color cap=primary?color(0x183738,0.14f):color(0x455070,0.66f);
        roundRect(x+w-capW-13,y+(h-23)/2,capW,23,6,cap);
        center(x+w-capW*0.5f-13,y+h*0.5f+4,hotkey,10,primary?ink:muted,true);
    }
}
void App::header() {
    brandMark(54,26);
    text(103,50,"TUMPUK",20,paper,true,1.5f);
    rect(54,86,1172,1,border);
    if(screen_==Screen::Playing) button(1090,23,136,45,Action::Pause,"Jeda",false,"ESC");
    else {
        card(1082,29,144,30,8,color(0x1C273B));
        roundRect(1094,40,7,7,3.5f,mint);
        text(1109,49,"PC / OFFLINE",11,muted,true,1.25f);
    }
}
void App::footer() {
    rect(54,658,1172,1,border);
    label(54,688,"PROYEK KOMPUTER GRAFIS");
    const std::string_view end="3D ISOMETRIK  /  VERSI 1.0";
    text(1226-textWidth(end,11,true,1.25f),688,end,11,dim,true,1.25f);
}
void App::menu() {
    towerPreview(time_);
    roundRect(54,154,34,3,1.5f,mint);
    label(99,161,"GAME ARCADE 3D",mint);
    text(48,254,"TUMPUK",94,paper,true,-2);
    text(56,302,"Susun setinggi mungkin.",25,violet,true);
    text(56,345,"Jatuhkan balok saat sejajar. Bagian yang",17,muted);
    text(56,371,"meleset akan terpotong dan jatuh.",17,muted);
    button(54,410,334,53,Action::Play,"Mulai",true,"ENTER");
    button(54,476,334,49,Action::Credits,"Credits",false,"C");
    button(54,536,334,49,Action::Exit,"Keluar",false,"ESC");
    center(872,616,"SATU BALOK. SATU KESEMPATAN.",12,muted,true);
}
void App::gameplay() {
    towerLive(round_);
    label(54,133,"RUN BERJALAN",mint);
    card(54,156,332,204,18,color(0x202942));
    label(76,190,"SKOR");
    text(73,270,std::to_string(round_.score),79,paper,true);
    rect(76,294,288,1,border);
    label(76,330,"REKOR",muted);
    const auto score=std::to_string(best_);
    text(364-textWidth(score,23,true),331,score,23,mint,true);
    label(54,405,"KONTROL");
    keycap(54,425,65,"SPACE");
    text(133,445,"Jatuhkan balok",15,paper);
    keycap(54,461,65,"KLIK");
    text(133,481,"Atau klik arena",15,muted);
    keycap(54,497,65,"P / ESC");
    text(133,517,"Jeda permainan",15,muted);
    if(round_.alive) {
        const bool perfect=round_.lastLand==Land::Perfect;
        if(round_.notice>0) {
            const std::string message=perfect
                ? (round_.combo>=2 ? "PAS!  COMBO x"+std::to_string(round_.combo) : "PAS!")
                : "TERPOTONG " + std::to_string(static_cast<int>(std::round(round_.cutPercent))) + "%";
            const float alpha=std::min(1.0f,round_.notice*3.3f);
            const float w=textWidth(message,15,true)+34;
            roundRect(836-w/2,113,w,34,10,color(0x242B46,alpha));
            center(836,136,message,15,fade(perfect?mint:blush,alpha),true);
        } else {
            center(836,136,round_.score==0?"TUNGGU HINGGA BALOK SEJAJAR":
                   (round_.axis==Axis::X?"BALOK BERGERAK DI SUMBU X":"BALOK BERGERAK DI SUMBU Z"),12,muted,true);
        }
    }
    if(screen_==Screen::Pause||screen_==Screen::Result) modal();
}
void App::modal() {
    if(screen_==Screen::Result && round_.resultTime<0.45f) return;
    buttons_.clear();
    rect(0,0,screenW,screenH,color(0x080D1B,0.77f));
    card(405,177,470,366,22,color(0x1B243A),color(0x576487));
    const bool result=screen_==Screen::Result;
    center(640,219,result?(round_.score>initialBest_?"REKOR BARU":"RUN SELESAI"):"JEDA",13,mint,true);
    center(640,280,result?"Menara berhenti":"Permainan dijeda",35,paper,true);
    if(result) {
        center(640,326,"Skor " + std::to_string(round_.score) + "  /  Rekor " +
               std::to_string(best_),21,violet,true);
        center(640,356,"Combo terbaik: "+std::to_string(round_.longestCombo),15,muted);
    } else {
        center(640,328,"Balok berhenti sampai kamu lanjut.",17,muted);
    }
    button(458,389,364,52,result?Action::Retry:Action::Resume,result?"Coba lagi":"Lanjutkan",true,
           result?"R":"ENTER");
    button(458,456,364,50,Action::Home,"Menu utama",false,"M");
}
void App::credits() {
    label(54,140,"TIM PENGEMBANG",mint);
    text(50,220,"Credits",64,paper,true);
    text(54,260,"TUMPUK  /  Proyek Komputer Grafis",17,muted);
    card(54,291,1172,266,17,color(0x1A2137));
    label(80,327,"NAMA",violet);
    label(829,327,"NIM",violet);
    rect(80,341,1118,1,border);
    for(std::size_t i=0;i<5&&creditsPage_*5+i<people_.size();++i) {
        const auto& p=people_[creditsPage_*5+i];
        const float y=369+static_cast<float>(i)*36.0f;
        text(80,y,fitText(p.name,18,710,true),18,paper,true);
        text(829,y,fitText(p.nim,16,325),16,muted);
        if(i<4 && creditsPage_*5+i+1<people_.size()) rect(80,y+12,1118,1,fade(border,0.70f));
    }
    button(54,580,244,48,Action::Home,"Menu utama",false,"ESC");
    const std::size_t pages=(people_.size()+4)/5;
    if(pages>1) {
        center(918,610,std::to_string(creditsPage_+1)+" / "+std::to_string(pages),14,muted,true);
        if(creditsPage_>0) button(730,580,142,48,Action::Previous,"Sebelum",false,"A");
        if(creditsPage_+1<pages) button(964,580,168,48,Action::Next,"Berikut",false,"D");
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
    gradient();
    header();
    footer();
    if(screen_==Screen::Menu) menu();
    else if(screen_==Screen::Credits) credits();
    else gameplay();
}
}
