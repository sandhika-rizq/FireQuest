////////////////////////////////////////////////////////////////
// FireQuest_Simulation.cpp
//
// Versi 2.2 - Visual tweaks: shorter & wider tube, pin shown on side,
// improved handle geometry for better visibility from camera POV.
//
////////////////////////////////////////////////////////////////

#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <array>

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/glu.h>

#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "freeglut.lib")
#pragma comment(lib, "opengl32.lib")

// --- Variabel Global ---
int winW = 1024;
int winH = 768;
int lastMouseX = winW / 2;
int lastMouseY = winH / 2;

float camX = 0.0f;
float camY = 5.0f;
float camZ = 20.0f;
float camYaw = 0.0f;
float camPitch = 0.0f;
float moveSpeed = 0.5f;

float lookX = 0.0f;
float lookY = 0.0f;
float lookZ = -1.0f;

bool pinPulled = false;
bool isSpraying = false;

bool fireActive = true;
float fireHealth = 100.0f;
float fireX = 0.0f, fireY = 2.5f, fireZ = -5.0f;

std::string uiMessage;

// Prototipe fungsi
void setup(void);
void drawScene(void);
void resize(int w, int h);
void keyInput(unsigned char key, int x, int y);
void passiveMotion(int x, int y);
void mouseClick(int button, int state, int x, int y);
void update(int value);
void drawText(float x, float y, const char* text);
void drawRoom(void);
void drawFire(void);
void drawSpray(void);
void drawApar(void);
void drawUI(void);
void updateLogic(void);
void resetSim(void);

// helpers
static const float PI = 3.14159265358979323846f;
static float toRadians(float degrees) { return degrees * PI / 180.0f; }

static GLUquadric* gQuad = nullptr;
static void ensureQuadric()
{
    if (!gQuad) {
        gQuad = gluNewQuadric();
        gluQuadricNormals(gQuad, GLU_SMOOTH);
        gluQuadricTexture(gQuad, GL_FALSE);
    }
}
static void freeQuadric()
{
    if (gQuad) { gluDeleteQuadric(gQuad); gQuad = nullptr; }
}

static void setDiffuseColor(float r, float g, float b, float a = 1.0f)
{
    GLfloat col[4] = { r, g, b, a };
    glColor4fv(col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);
}
static void setSpecular(float v = 0.25f)
{
    GLfloat spec[4] = { v, v, v, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);
}

// --- Main ---
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitContextVersion(4, 3);
    glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("FireQuest - Simulasi APAR (ref image)");

    glutDisplayFunc(drawScene);
    glutReshapeFunc(resize);
    glutKeyboardFunc(keyInput);
    glutMouseFunc(mouseClick);
    glutPassiveMotionFunc(passiveMotion);
    glutTimerFunc(16, update, 0);

    glutSetCursor(GLUT_CURSOR_NONE);

    glewExperimental = GL_TRUE;
    glewInit();

    setup();
    glutMainLoop();

    // cleanup (not normally reached because glutMainLoop doesn't return)
    freeQuadric();
    return 0;
}

// --- Setup ---
void setup(void)
{
    glClearColor(0.95f, 0.95f, 0.95f, 1.0f); // neutral background similar to photo studio
    glEnable(GL_DEPTH_TEST);

    // lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    GLfloat lp[4] = { 10.0f, 30.0f, 30.0f, 1.0f };
    GLfloat la[4] = { 0.12f, 0.12f, 0.12f, 1.0f };
    GLfloat ld[4] = { 0.95f, 0.95f, 0.95f, 1.0f };
    GLfloat ls[4] = { 0.9f, 0.9f, 0.9f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lp);
    glLightfv(GL_LIGHT0, GL_AMBIENT, la);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, ld);
    glLightfv(GL_LIGHT0, GL_SPECULAR, ls);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    setSpecular(0.35f);

    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);

    ensureQuadric();

    resetSim();
}

void resetSim(void)
{
    pinPulled = false;
    isSpraying = false;
    fireActive = true;
    fireHealth = 100.0f;
    camX = 0.0f; camY = 5.0f; camZ = 20.0f;
    camYaw = 0.0f; camPitch = 0.0f;
    glutWarpPointer(winW / 2, winH / 2);
    lastMouseX = winW / 2; lastMouseY = winH / 2;
}

// --- Drawing primitives for APAR (clean, based on reference image) ---

// Draw a smooth vertical cylinder with sphere caps. Base at y=0, top at y=height.
static void drawCappedCylinder(float radius, float height, int slices = 48, int stacks = 12)
{
    ensureQuadric();
    // Cylinder along +Y: rotate GLU cylinder which points along +Z
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    gluCylinder(gQuad, radius, radius, height, slices, stacks);
    glPopMatrix();

    // bottom sphere
    glPushMatrix();
    glutSolidSphere(radius * 0.995f, slices, stacks);
    glPopMatrix();

    // top sphere
    glPushMatrix();
    glTranslatef(0.0f, height, 0.0f);
    glutSolidSphere(radius * 0.995f, slices, stacks);
    glPopMatrix();
}

// Draw hose as quadratic bezier sampled with cylinders (smooth tubular appearance).
static void drawHoseBezierTube(float p0x, float p0y, float p0z,
    float p1x, float p1y, float p1z,
    float p2x, float p2y, float p2z,
    int segments = 24, float tubeRadius = 0.04f) // more segments and slightly thicker tube
{
    ensureQuadric();
    // sample points and draw small cylinders oriented between samples
    std::vector<std::array<float, 3>> pts;
    pts.reserve(segments);
    for (int i = 0; i < segments; ++i) {
        float t = (float)i / (segments - 1);
        float omt = 1.0f - t;
        float x = omt * omt * p0x + 2 * omt * t * p1x + t * t * p2x;
        float y = omt * omt * p0y + 2 * omt * t * p1y + t * t * p2y;
        float z = omt * omt * p0z + 2 * omt * t * p1z + t * t * p2z;
        pts.push_back({ x,y,z });
    }

    // draw small cylinders between consecutive pts
    for (int i = 0; i < (int)pts.size() - 1; ++i) {
        auto a = pts[i];
        auto b = pts[i + 1];
        float vx = b[0] - a[0], vy = b[1] - a[1], vz = b[2] - a[2];
        float len = sqrtf(vx * vx + vy * vy + vz * vz);
        if (len <= 1e-5f) continue;

        // compute rotation to align +Z to vector (vx,vy,vz)
        float ax = 0.0f, ay = 0.0f, az = 1.0f; // source axis (glu cylinder points +Z originally)
        // rotation axis = cross(ax, v)
        float rx = ay * vz - az * vy;
        float ry = az * vx - ax * vz;
        float rz = ax * vy - ay * vx;
        float rlen = sqrtf(rx * rx + ry * ry + rz * rz);
        float dot = ax * vx + ay * vy + az * vz;
        float angle = 0.0f;
        if (rlen > 1e-5f) {
            angle = acosf(dot / (len)) * 180.0f / PI;
            // normalize axis
            rx /= rlen; ry /= rlen; rz /= rlen;
        }
        else {
            // parallel or anti-parallel
            rx = 1.0f; ry = 0.0f; rz = 0.0f;
            angle = (dot >= 0.0f) ? 0.0f : 180.0f;
        }

        glPushMatrix();
        glTranslatef(a[0], a[1], a[2]);
        glRotatef(angle, rx, ry, rz);
        // draw cylinder along +Z
        glRotatef(-90.0f, 1, 0, 0); // now gluCylinder points +Y, rotate so it points +Z
        setDiffuseColor(0.06f, 0.06f, 0.06f);
        gluCylinder(gQuad, tubeRadius, tubeRadius, len, 12, 2);
        glPopMatrix();
    }

    // caps at ends
    glPushMatrix(); glTranslatef(pts.front()[0], pts.front()[1], pts.front()[2]);
    glutSolidSphere(tubeRadius * 1.02f, 12, 8);
    glPopMatrix();
    glPushMatrix(); glTranslatef(pts.back()[0], pts.back()[1], pts.back()[2]);
    glutSolidSphere(tubeRadius * 1.02f, 12, 8);
    glPopMatrix();
}

// Draw a small metallic coupling (silver ring)
static void drawCoupling(float x, float y, float z)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    setDiffuseColor(0.78f, 0.78f, 0.78f);
    glutSolidTorus(0.01f, 0.04f, 12, 20);
    glPopMatrix();
}

// Draw the nozzle tip
static void drawNozzleTip(float x, float y, float z, float yawDeg = -120.0f, float pitchDeg = 90.0f)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(yawDeg, 0, 1, 0);
    glRotatef(pitchDeg, 1, 0, 0);
    setDiffuseColor(0.12f, 0.12f, 0.12f);
    glutSolidCone(0.045f, 0.14f, 16, 6);
    // small metallic ring
    glTranslatef(0.0f, 0.02f, 0.0f);
    setDiffuseColor(0.78f, 0.78f, 0.78f);
    glutSolidTorus(0.008f, 0.03f, 10, 20);
    glPopMatrix();
}

// --- Draw APAR (completely rewritten, reference: provided photo) ---
// Modifications: shorter & wider tube, pin moved to side (positive X), improved handle geometry.
void drawApar(void)
{
    // local model scale: base at y=0
    const float bodyRadius = 0.36f;    // slightly wider
    const float bodyHeight = 3.2f;     // shorter
    const float baseThickness = 0.16f;
    const float valveStemH = 0.16f;
    const float leverLength = 0.95f;
    const float leverThickness = 0.055f;

    glPushMatrix(); // model root

    // --- Base (black protective boot) ---
    glPushMatrix();
    setDiffuseColor(0.06f, 0.06f, 0.06f);
    glTranslatef(0.0f, -baseThickness * 0.5f, 0.0f);
    glRotatef(-90.0f, 1, 0, 0);
    // fat short cylinder to mimic plastic boot
    gluCylinder(gQuad, bodyRadius + 0.09f, bodyRadius + 0.09f, baseThickness, 36, 2);
    // outer rim
    glTranslatef(0.0f, 0.0f, baseThickness);
    glutSolidTorus(0.02f, bodyRadius + 0.06f, 20, 36);
    glPopMatrix();

    // --- Body (red cylinder with rounded ends) ---
    setDiffuseColor(0.90f, 0.08f, 0.08f);
    setSpecular(0.22f);
    drawCappedCylinder(bodyRadius, bodyHeight, 48, 10);

    // --- Valve stem and block (silver) ---
    glPushMatrix();
    setDiffuseColor(0.78f, 0.78f, 0.78f);
    // position at top of body
    glTranslatef(0.0f, bodyHeight + 0.02f, 0.0f);
    // short stem
    glPushMatrix();
    glRotatef(-90.0f, 1, 0, 0);
    gluCylinder(gQuad, 0.055f, 0.055f, valveStemH, 20, 2);
    glPopMatrix();

    // valve block body (small rectangular)
    glPushMatrix();
    glTranslatef(0.0f, valveStemH + 0.02f, 0.0f);
    glScalef(0.34f, 0.14f, 0.22f);
    setDiffuseColor(0.78f, 0.78f, 0.78f);
    glutSolidCube(1.0f);
    glPopMatrix();
    glPopMatrix();

    // --- Carrying handle (improved: slightly rounded top tube) ---
    glPushMatrix();
    // uprights
    setDiffuseColor(0.08f, 0.08f, 0.08f);
    glPushMatrix();
    glTranslatef(-0.18f, bodyHeight - 0.02f + 0.22f, 0.0f);
    glScalef(0.05f, 0.28f, 0.05f);
    glutSolidCube(1.0f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0.18f, bodyHeight - 0.02f + 0.22f, 0.0f);
    glScalef(0.05f, 0.28f, 0.05f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // top bar as rounded cylinder for better realism
    glPushMatrix();
    glTranslatef(0.0f, bodyHeight - 0.02f + 0.44f, 0.0f);
    // draw a cylinder oriented across X: rotate GLU cylinder from +Z to +X
    glRotatef(90.0f, 0, 1, 0);
    setDiffuseColor(0.08f, 0.08f, 0.08f);
    gluCylinder(gQuad, 0.028f, 0.028f, 0.36f, 20, 4);
    glPopMatrix();

    // small rounded inner grip (rubber) attached to top bar (slightly inset)
    glPushMatrix();
    glTranslatef(0.0f, bodyHeight - 0.02f + 0.44f, 0.0f);
    setDiffuseColor(0.12f, 0.12f, 0.12f);
    glutSolidTorus(0.006f, 0.18f, 12, 24); // visual ring-like grip
    glPopMatrix();
    glPopMatrix();

    // --- Lever (red two-piece squeeze handle, improved geometry) ---
    glPushMatrix();
    // pivot point in front of valve block
    glTranslatef(0.0f, bodyHeight + 0.12f, 0.10f);
    if (isSpraying) glRotatef(-18.0f, 1, 0, 0); // slight squeeze animation

    // lever back plate (thin)
    glPushMatrix();
    setDiffuseColor(0.88f, 0.06f, 0.06f);
    glTranslatef(0.0f, 0.18f, leverLength * 0.45f - 0.12f);
    glScalef(0.06f, leverThickness, leverLength * 0.9f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // lever top handle (rounded small cylinder for finger grip)
    glPushMatrix();
    setDiffuseColor(0.88f, 0.06f, 0.06f);
    glTranslatef(0.0f, 0.36f, leverLength - 0.05f);
    // orient small cylinder across X
    glRotatef(90.0f, 0, 1, 0);
    gluCylinder(gQuad, 0.03f, 0.03f, 0.22f, 16, 4);
    glPopMatrix();

    // hinge rivet (metal)
    glPushMatrix();
    setDiffuseColor(0.06f, 0.06f, 0.06f);
    glTranslatef(0.0f, 0.16f, 0.02f);
    glutSolidSphere(0.03f, 12, 8);
    glPopMatrix();

    glPopMatrix();

    // --- Safety pin & ring (yellow) moved to SIDE (+X) for better visibility ---
    if (!pinPulled) {
        glPushMatrix();
        // pin rod (silver) - now on positive X side so camera sees it clearly
        setDiffuseColor(0.78f, 0.78f, 0.78f);
        glTranslatef(0.12f, bodyHeight + 0.14f, 0.10f); // moved to +X and slightly forward
        glRotatef(0.0f, 0, 1, 0);
        glScalef(0.02f, 0.02f, 0.36f);
        glutSolidCube(1.0f);
        glPopMatrix();

        glPushMatrix();
        // ring (yellow) near end of pin on side
        glTranslatef(0.18f, bodyHeight + 0.12f, 0.40f);
        glRotatef(90.0f, 0, 1, 0);
        setDiffuseColor(0.98f, 0.82f, 0.06f);
        glutSolidTorus(0.012f, 0.045f, 12, 20); // slightly larger ring
        glPopMatrix();
    }

    // --- Pressure gauge (small) ---
    glPushMatrix();
    glTranslatef(0.16f, bodyHeight + 0.14f, 0.08f);
    setDiffuseColor(0.95f, 0.95f, 0.95f);
    glutSolidSphere(0.04f, 12, 10);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.03f);
    setDiffuseColor(0.06f, 0.06f, 0.06f);
    glutSolidSphere(0.02f, 10, 8);
    glPopMatrix();
    glPopMatrix();

    // --- Hose: from valve block to nozzle (adjusted endpoints for new body size) ---
    float p0x = -bodyRadius - 0.02f, p0y = bodyHeight + 0.02f, p0z = 0.04f;
    float p1x = -bodyRadius - 0.50f, p1y = bodyHeight - 0.7f, p1z = 0.9f;
    float p2x = -bodyRadius - 1.05f, p2y = bodyHeight - 1.5f, p2z = 0.6f;

    // small silver coupling at valve side
    drawCoupling(p0x + 0.02f, p0y + 0.0f, p0z);

    // draw flexible hose as tube (more segments, thicker tube)
    drawHoseBezierTube(p0x, p0y, p0z, p1x, p1y, p1z, p2x, p2y, p2z, 24, 0.042f);

    // metallic coupling near nozzle
    drawCoupling(p2x + 0.02f, p2y + 0.02f, p2z);

    // nozzle
    drawNozzleTip(p2x, p2y, p2z, -90.0f, 90.0f);

    // --- Label (white rectangle with red stripes/text-like bars) ---
    glPushMatrix();
    glTranslatef(0.0f, bodyHeight * 0.45f, bodyRadius - 0.001f); // slightly in front
    glRotatef(90.0f, 0, 1, 0); // face viewer when in view-model
    // white background
    setDiffuseColor(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(-0.18f, 0.40f, 0.0f);
    glVertex3f(0.18f, 0.40f, 0.0f);
    glVertex3f(0.18f, -0.40f, 0.0f);
    glVertex3f(-0.18f, -0.40f, 0.0f);
    glEnd();
    // red header box (brand)
    setDiffuseColor(0.85f, 0.06f, 0.06f);
    glBegin(GL_QUADS);
    glVertex3f(-0.16f, 0.34f, 0.001f);
    glVertex3f(0.16f, 0.34f, 0.001f);
    glVertex3f(0.16f, 0.24f, 0.001f);
    glVertex3f(-0.16f, 0.24f, 0.001f);
    glEnd();
    // stripes to mimic instructions
    setDiffuseColor(0.06f, 0.06f, 0.06f);
    glBegin(GL_LINES);
    glVertex3f(-0.14f, 0.10f, 0.001f); glVertex3f(0.14f, 0.10f, 0.001f);
    glVertex3f(-0.14f, -0.04f, 0.001f); glVertex3f(0.14f, -0.04f, 0.001f);
    glVertex3f(-0.14f, -0.18f, 0.001f); glVertex3f(0.14f, -0.18f, 0.001f);
    glEnd();
    glPopMatrix();

    glPopMatrix(); // model root end
}

// --- Scene & UI ---
void drawScene(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 3D world: set projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (float)winW / (float)winH, 0.1f, 1000.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // camera
    gluLookAt(camX, camY, camZ,
        camX + lookX, camY + lookY, camZ + lookZ,
        0.0f, 1.0f, 0.0f);

    // world objects
    drawRoom();
    drawFire();
    if (isSpraying) drawSpray();

    // View-model APAR (draw on top)
    glClear(GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // position view-model (right-hand)
    glTranslatef(0.45f, -0.30f, -1.15f);
    glRotatef(-8.0f, 0, 1, 0);
    glRotatef(-8.0f, 1, 0, 0);
    glScalef(0.16f, 0.16f, 0.16f); // uniform scale important

    drawApar();

    // UI overlay
    drawUI();

    glutSwapBuffers();
}

void drawRoom(void)
{
    // floor
    setDiffuseColor(0.85f, 0.85f, 0.85f);
    glPushMatrix();
    glColor3f(0.9f, 0.9f, 0.9f);
    glTranslatef(0.0f, -0.5f, 0.0f);
    glScalef(50.0f, 1.0f, 50.0f);
    glutSolidCube(1.0);
    glPopMatrix();

    // back wall
    setDiffuseColor(0.96f, 0.96f, 0.94f);
    glPushMatrix();
    glTranslatef(0.0f, 10.0f, -25.0f);
    glScalef(50.0f, 20.0f, 1.0f);
    glutSolidCube(1.0);
    glPopMatrix();
}

void drawFire(void)
{
    if (!fireActive) return;
    float flicker = 1.0f + (rand() % 100) / 500.0f;
    glPushMatrix();
    glTranslatef(fireX, fireY, fireZ);
    setDiffuseColor(1.0f, 0.6f, 0.08f);
    glScalef(1.6f * flicker, 6.0f * flicker, 1.6f * flicker);
    glutSolidCone(1.0, 1.0, 16, 4);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(fireX, fireY, fireZ);
    setDiffuseColor(1.0f, 1.0f, 0.0f);
    glScalef(1.2f * flicker, 4.0f * flicker, 1.2f * flicker);
    glutSolidCone(1.0, 1.0, 16, 4);
    glPopMatrix();
}

void drawSpray(void)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT);
    glDisable(GL_LIGHTING); // keep spray bright
    glColor4f(1.0f, 1.0f, 1.0f, 0.28f);
    glPushMatrix();
    glTranslatef(camX + lookX * 1.5f, camY + lookY * 1.5f - 0.5f, camZ + lookZ * 1.5f);
    glRotatef(camYaw, 0, 1, 0);
    glRotatef(camPitch, 1, 0, 0);
    glScalef(4.2f, 4.2f, 16.0f);
    glutSolidCone(0.6, 1.0, 12, 4);
    glPopMatrix();
    glEnable(GL_LIGHTING);
    glPopAttrib();

    glDisable(GL_BLEND);
}

void drawUI(void)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);

    if (fireActive) setDiffuseColor(1.0f, 1.0f, 1.0f);
    else setDiffuseColor(0.0f, 1.0f, 0.0f);
    drawText(10.0f, 50.0f, uiMessage.c_str());

    setDiffuseColor(0.0f, 0.0f, 0.0f);
    drawText(10.0f, 30.0f, "Tekan 'R' untuk Reset, 'ESC' untuk Keluar");
    drawText(winW / 2 - 5, winH / 2 - 5, "+");

    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void drawText(float x, float y, const char* text)
{
    glRasterPos2f(x, y);
    while (*text) {
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *text++);
    }
}

// --- Logic & Input ---
void update(int value)
{
    updateLogic();
    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void updateLogic(void)
{
    // look vector from yaw/pitch
    lookX = cosf(toRadians(camYaw)) * cosf(toRadians(camPitch));
    lookY = sinf(toRadians(camPitch));
    lookZ = sinf(toRadians(camYaw)) * cosf(toRadians(camPitch));
    float len = sqrtf(lookX * lookX + lookY * lookY + lookZ * lookZ);
    if (len > 1e-6f) { lookX /= len; lookY /= len; lookZ /= len; }

    if (!fireActive) {
        uiMessage = "Api Berhasil Dipadamkan! Tekan 'R' untuk Reset.";
        isSpraying = false;
    }
    else if (!pinPulled) {
        uiMessage = "APAR Terkunci. Tekan 'E' untuk Tarik Pin.";
    }
    else if (!isSpraying) {
        uiMessage = "APAR Siap. Tahan Klik Kiri untuk Semprot (SQUEEZE).";
    }
    else {
        uiMessage = "Menyemprot! Arahkan ke DASAR Api (AIM & SWEEP)!";
    }

    if (isSpraying && fireActive) {
        float vx = fireX - camX, vy = fireY - camY, vz = fireZ - camZ;
        float dist = sqrtf(vx * vx + vy * vy + vz * vz);
        if (dist > 1e-6f) { vx /= dist; vy /= dist; vz /= dist; }
        float dot = lookX * vx + lookY * vy + lookZ * vz;
        if (dot > 0.95f && dist < 25.0f) fireHealth -= 1.5f;
    }

    if (fireHealth <= 0.0f) fireActive = false;
}

void resize(int w, int h)
{
    if (h == 0) h = 1;
    winW = w; winH = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (float)w / (float)h, 0.1f, 1000.0f);
}

void keyInput(unsigned char key, int x, int y)
{
    float rightX = cosf(toRadians(camYaw - 90.0f));
    float rightZ = sinf(toRadians(camYaw - 90.0f));

    switch (key)
    {
    case 27: exit(0); break;
    case 'w': camX += lookX * moveSpeed; camZ += lookZ * moveSpeed; break;
    case 's': camX -= lookX * moveSpeed; camZ -= lookZ * moveSpeed; break;
    case 'a': camX += rightX * moveSpeed; camZ += rightZ * moveSpeed; break;
    case 'd': camX -= rightX * moveSpeed; camZ -= rightZ * moveSpeed; break;
    case 'e': if (!pinPulled) pinPulled = true; break;
    case 'r': resetSim(); break;
    }
}

void mouseClick(int button, int state, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (pinPulled && fireActive) isSpraying = true;
    }
    else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        isSpraying = false;
    }
}

void passiveMotion(int x, int y)
{
    float deltaX = (float)(x - lastMouseX);
    float deltaY = (float)(y - lastMouseY);
    lastMouseX = x; lastMouseY = y;
    float sensitivity = 0.15f;
    camYaw += deltaX * sensitivity;
    camPitch -= deltaY * sensitivity;
    if (camPitch > 89.0f) camPitch = 89.0f;
    if (camPitch < -89.0f) camPitch = -89.0f;

    if (x != winW / 2 || y != winH / 2) {
        glutWarpPointer(winW / 2, winH / 2);
        lastMouseX = winW / 2; lastMouseY = winH / 2;
    }
}