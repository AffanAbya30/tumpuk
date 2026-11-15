#include "app.hpp"
#include "runtime_paths.hpp"
#include <GL/freeglut.h>
#include <chrono>

namespace {
using Clock=std::chrono::steady_clock;
tumpuk::App* app=nullptr;
Clock::time_point last;
void display() { app->render(); glutSwapBuffers(); }
void resize(int w,int h) { app->resize(w,h); }
void key(unsigned char k,int,int) {
    app->key(k);
    if(app->shouldExit()) glutLeaveMainLoop();
    else glutPostRedisplay();
}
void pointer(int x,int y) { app->pointer(x,y); }
void mouse(int button,int state,int x,int y) {
    if(button==GLUT_LEFT_BUTTON&&state==GLUT_DOWN) {
        app->click(x,y);
        if(app->shouldExit()) glutLeaveMainLoop();
        else glutPostRedisplay();
    }
}
void timer(int) {
    const auto now=Clock::now();
    app->update(std::chrono::duration<float>(now-last).count());
    last=now;
    glutPostRedisplay();
    glutTimerFunc(16,timer,0);
}
}

int main(int argc,char** argv) {
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_DEPTH);
    const int w=glutGet(GLUT_SCREEN_WIDTH)<1310?960:1280;
    glutInitWindowSize(w,w*9/16);
    glutCreateWindow("TUMPUK - Stack Arcade");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE,GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glutIgnoreKeyRepeat(1);
    const auto folder=executableFolder();
    tumpuk::App game(folder/"credits.txt",tumpuk::scoreLocation(folder));
    app=&game;
    game.initGraphics();
    game.resize(w,w*9/16);
    glutDisplayFunc(display);
    glutReshapeFunc(resize);
    glutKeyboardFunc(key);
    glutMouseFunc(mouse);
    glutPassiveMotionFunc(pointer);
    glutMotionFunc(pointer);
    last=Clock::now();
    glutTimerFunc(16,timer,0);
    glutMainLoop();
    app=nullptr;
}
