#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#elif defined(TUMPUK_GL_RUNTIME)
// Linux QA can link the system OpenGL runtime without development headers.
using GLenum = unsigned int;
using GLuint = unsigned int;
using GLsizei = int;
using GLint = int;
using GLbitfield = unsigned int;
using GLfloat = float;
using GLdouble = double;
inline constexpr GLenum GL_COLOR_BUFFER_BIT=0x4000, GL_DEPTH_BUFFER_BIT=0x0100,
 GL_BLEND=0x0BE2, GL_DEPTH_TEST=0x0B71, GL_TEXTURE_2D=0x0DE1,
 GL_SRC_ALPHA=0x0302, GL_ONE_MINUS_SRC_ALPHA=0x0303, GL_LEQUAL=0x0203,
 GL_PROJECTION=0x1701, GL_MODELVIEW=0x1700, GL_QUADS=0x0007, GL_TRIANGLE_FAN=0x0006,
 GL_LINE_LOOP=0x0002, GL_LINES=0x0001, GL_LINEAR=0x2601,
 GL_TEXTURE_MIN_FILTER=0x2801, GL_TEXTURE_MAG_FILTER=0x2800, GL_ALPHA=0x1906,
 GL_UNSIGNED_BYTE=0x1401, GL_RGBA=0x1908, GL_UNPACK_ALIGNMENT=0x0CF5,
 GL_NO_ERROR=0, GL_VERSION=0x1F02, GL_CLIP_PLANE0=0x3000;
extern "C" {
void glClearColor(GLfloat,GLfloat,GLfloat,GLfloat); void glClear(GLbitfield);
void glEnable(GLenum); void glDisable(GLenum); void glBlendFunc(GLenum,GLenum);
void glClipPlane(GLenum,const GLdouble*);
void glDepthFunc(GLenum); void glViewport(GLint,GLint,GLsizei,GLsizei);
void glMatrixMode(GLenum); void glLoadIdentity();
void glOrtho(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble);
void glPushMatrix(); void glPopMatrix(); void glTranslatef(GLfloat,GLfloat,GLfloat);
void glRotatef(GLfloat,GLfloat,GLfloat,GLfloat); void glScalef(GLfloat,GLfloat,GLfloat);
void glBegin(GLenum); void glEnd(); void glVertex2f(GLfloat,GLfloat);
void glVertex3f(GLfloat,GLfloat,GLfloat); void glColor4f(GLfloat,GLfloat,GLfloat,GLfloat);
void glLineWidth(GLfloat); void glTexCoord2f(GLfloat,GLfloat);
void glGenTextures(GLsizei,GLuint*); void glBindTexture(GLenum,GLuint);
void glTexParameteri(GLenum,GLenum,GLint); void glPixelStorei(GLenum,GLint);
void glTexImage2D(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*);
void glReadPixels(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*);
GLenum glGetError(); const unsigned char* glGetString(GLenum);
void glFinish();
}
#else
#include <GL/gl.h>
#endif
