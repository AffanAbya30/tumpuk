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
constexpr Color paper  = color(0xF4F6FC);           // teks utama putih bersih
constexpr Color dim    = color(0x8A96B2);           // teks sekunder slate lembut
constexpr Color ink    = color(0x0C121E);           // teks gelap di atas tombol primary
constexpr Color border = color(0x28344A);           // border kartu / tombol
constexpr Color panel  = color(0x131A27);           // surface panel / modal
constexpr Color surface= color(0x172030);           // surface tombol secondary
constexpr Color surfaceHover = color(0x222E44);     // hover tombol secondary
constexpr Color accent = color(0x7CE3C8);           // pastel mint (Primary CTA)
constexpr Color accentHover = color(0x9BF0DA);      // hover tombol primary
constexpr Color violet = color(0xB8ACFF);           // pastel lavender (secondary accent)
constexpr Color danger = color(0xFCA5A5);           // soft coral pastel (notifikasi cut)
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

void gradient() {
    glBegin(GL_QUADS);
    shade(color(0x0E1320)); glVertex2f(0,0); glVertex2f(screenW,0);
    shade(color(0x090D16)); glVertex2f(screenW,screenH); glVertex2f(0,screenH);
    glEnd();
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
    quad3(color(0x1B2338),-4.0f,-0.015f,-4.0f,4.0f,-0.015f,-4.0f,
          4.0f,-0.015f,4.0f,-4.0f,-0.015f,4.0f);
    for(int i=-6;i<=6;++i) {
        const float a=i*0.65f;
        shade(color(0x6878A0,i==0?0.48f:0.22f));
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
    // 3 blok bertumpuk selaras palet balok (slate purple -> lavender -> mint)
    roundRect(x,    y+24, 24, 5, 1.5f, color(0x696897));
    roundRect(x+4,  y+16, 16, 5, 1.5f, violet);
    roundRect(x+8,  y+8,  8,  5, 1.5f, accent);
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

Color toneColor(BtnTone tone) {
    switch(tone) {
    case BtnTone::Mint:     return color(0x82DFC6); // pastel mint (balok 10)
    case BtnTone::Lavender: return color(0xB8ACFF); // pastel lavender (balok 3)
    case BtnTone::Peach:    return color(0xF6B2BE); // pastel peach/rose (balok 6)
    case BtnTone::Cyan:     return color(0x91CCDA); // pastel sky cyan (balok 11)
    case BtnTone::Sage:     return color(0xCEDCB2); // pastel sage green (balok 9)
    }
    return color(0x82DFC6);
}

void App::button(float x,float y,float w,float h,Action a,const std::string& s,
                 BtnTone tone,const std::string& hotkey) {
    buttons_.push_back({x,y,w,h,a});
    const bool hover=mouseX_>=x&&mouseX_<=x+w&&mouseY_>=y&&mouseY_<=y+h;
    const Color c = toneColor(tone);
    const Color bg = hover ? tint(c, 1.12f) : c;
    roundRect(x,y,w,h,7,bg);
    text(x+18,y+h*0.5f+6,s,16,ink,true);
    if(!hotkey.empty()) {
        const float capW=std::max(26.0f,textWidth(hotkey,9,true)+12);
        const Color cap=color(0x0C121E,0.18f);
        roundRect(x+w-capW-12,y+(h-20)/2,capW,20,4,cap);
        center(x+w-capW*0.5f-12,y+h*0.5f+3,hotkey,9,ink,true);
    }
}
void App::header() {
    brandMark(54,28);
    text(90,50,"TUMPUK",18,paper,true,2.0f);
    if(screen_==Screen::Playing) button(1140,24,96,40,Action::Pause,"Jeda",BtnTone::Cyan,"ESC");
}
void App::footer() {
    // Tanpa garis pembatas apapun di bagian bawah
    text(54,683,"Proyek Komputer Grafis",11,dim);
}
void App::menu() {
    towerPreview(time_);
    text(82,268,"TUMPUK",94,paper,true,-2);
    text(88,310,"Susun setinggi mungkin.",20,violet);
    text(88,344,"Jatuhkan balok saat posisi sejajar.",14,dim);
    button(88,384,284,48,Action::Play,"Mulai",BtnTone::Mint,"ENTER");
    button(88,444,284,44,Action::Credits,"Credits",BtnTone::Lavender,"C");
    button(88,500,284,44,Action::Exit,"Keluar",BtnTone::Peach,"ESC");
}
void App::gameplay() {
    towerLive(round_);
    // Area skor bersih tepat di tengah vertikal layar (y ~ 360) dengan ukuran lebih besar
    constexpr float hudX = 230.0f;
    center(hudX, 390, std::to_string(round_.score), 144, paper, true);
    const auto bestStr = std::to_string(best_);
    center(hudX, 430, "best " + bestStr, 22, dim);
    // Hint kontrol bersih di bawahnya (hanya saat skor masih 0)
    if(round_.score==0 && round_.alive) {
        center(hudX, 480, "SPACE  jatuhkan", 12, fade(dim, 0.65f));
        center(hudX, 504, "P / ESC  jeda", 12, fade(dim, 0.45f));
    }
    if(screen_==Screen::Pause||screen_==Screen::Result) modal();
}
void App::modal() {
    if(screen_==Screen::Result && round_.resultTime<0.45f) return;
    buttons_.clear();
    rect(0,0,screenW,screenH,color(0x070B14,0.85f));
    roundRect(430,196,420,330,8,panel);
    const bool result=screen_==Screen::Result;
    const bool newBest=result&&round_.score>initialBest_;
    center(640,230,result?(newBest?"rekor baru":"selesai"):"jeda",11,newBest?accent:dim,true);
    center(640,274,result?"Menara berhenti":"Permainan dijeda",28,paper,true);
    if(result) {
        center(640,314,"Skor "+std::to_string(round_.score)+"   /   Rekor "+std::to_string(best_),16,paper,true);
        center(640,342,"combo terbaik  "+std::to_string(round_.longestCombo),13,dim);
    } else {
        center(640,318,"Balok berhenti sampai kamu lanjut.",14,dim);
    }
    button(466,375,348,48,result?Action::Retry:Action::Resume,
           result?"Coba lagi":"Lanjutkan",BtnTone::Mint,result?"R":"ENTER");
    button(466,437,348,46,Action::Home,"Menu utama",BtnTone::Lavender,"M");
}
void App::credits() {
    text(54,195,"Credits",58,paper,true);
    text(54,230,"Proyek Komputer Grafis  /  TUMPUK",14,dim);
    // Tabel dengan panel surface dan border
    roundRect(54,258,1172,282,8,panel);
    roundRect(54,258,1172,1,0,border);
    label(80,296,"NAMA",violet);
    label(829,296,"NIM",violet);
    rect(80,308,1118,1,fade(border,0.80f));
    for(std::size_t i=0;i<5&&creditsPage_*5+i<people_.size();++i) {
        const auto& p=people_[creditsPage_*5+i];
        const float y=336+static_cast<float>(i)*38.0f;
        text(80,y,fitText(p.name,17,710,true),17,paper,true);
        text(829,y,fitText(p.nim,15,325),15,dim);
        if(i<4 && creditsPage_*5+i+1<people_.size()) rect(80,y+14,1118,1,fade(border,0.50f));
    }
    button(54,564,220,46,Action::Home,"Menu utama",BtnTone::Lavender,"ESC");
    const std::size_t pages=(people_.size()+4)/5;
    if(pages>1) {
        center(918,592,std::to_string(creditsPage_+1)+" / "+std::to_string(pages),12,dim,true);
        if(creditsPage_>0) button(730,564,130,46,Action::Previous,"Sebelum",BtnTone::Sage,"A");
        if(creditsPage_+1<pages) button(952,564,148,46,Action::Next,"Berikut",BtnTone::Cyan,"D");
    }
}
void App::render() {
    buttons_.clear();
    glViewport(viewport_.x,viewport_.y,viewport_.w,viewport_.h);
    glClearColor(0.047f,0.059f,0.078f,1);
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
