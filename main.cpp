////////////////////////////////////////////////////////////////
// FireQuest_Simulation.cpp
//
// Simulasi APAR interaktif sederhana menggunakan C++ dan Legacy OpenGL.
// Dibuat berdasarkan proyek "FireQuest" dan "box.cpp".
//
// Kontrol:
//   WASD  - Bergerak Maju/Mundur/Kiri/Kanan
//   Mouse - Melihat sekeliling (Aim)
//   'E'   - Menarik Pin APAR (Pull)
//   Klik Kiri (Tahan) - Menyemprot (Squeeze & Sweep)
//   'R'   - Reset Simulasi
//   ESC   - Keluar
//
////////////////////////////////////////////////////////////////

#include <iostream>
#include <string>
#include <vector>     // Diperlukan untuk std::string
#include <cmath>      // Diperlukan untuk sin, cos (kamera)

#include <GL\glew.h>
#include <GL\freeglut.h> 
#include <GL\freeglut_ext.h>
#include <GL\freeglut_std.h>

#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "freeglut.lib")
#pragma comment(lib, "opengl32.lib")

// --- Variabel Global untuk Simulasi ---

// Jendela & Kamera
int winW = 1024;
int winH = 768;
int lastMouseX = winW / 2;
int lastMouseY = winH / 2;
bool mouseWarped = false;

// Posisi & Orientasi Kamera (Pemain)
float camX = 0.0f;
float camY = 5.0f; // Tinggi mata
float camZ = 20.0f;
float camYaw = 0.0f;   // Rotasi Kiri/Kanan (derajat)
float camPitch = 0.0f; // Rotasi Atas/Bawah (derajat)
float moveSpeed = 0.5f;

// Vektor Arah Kamera (ke mana kita melihat)
float lookX = 0.0f;
float lookY = 0.0f;
float lookZ = -1.0f; // Awalnya melihat ke Z negatif

// Status Simulasi APAR (Logika "PASS")
bool pinPulled = false;
bool isSpraying = false;

// Status Api
bool fireActive = true;
float fireHealth = 100.0f;
float fireX = 0.0f, fireY = 2.5f, fireZ = -5.0f; // Posisi api

// Pesan UI
std::string uiMessage;

// --- Prototipe Fungsi ---
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
void drawUI(void);
void updateLogic(void);
void resetSim(void);

// Konversi Derajat ke Radian
const float PI = 3.1415926535;
float toRadians(float degrees) {
    return degrees * PI / 180.0f;
}

// --- Fungsi Utama (Main) ---
int main(int argc, char** argv)
{
    glutInit(&argc, argv);

    glutInitContextVersion(4, 3); // Seperti di box.cpp
    glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);

    // Kita perlu DOUBLE buffer untuk animasi dan DEPTH test untuk 3D
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("FireQuest - Simulasi APAR");

    // Daftarkan semua fungsi callback
    glutDisplayFunc(drawScene);
    glutReshapeFunc(resize);
    glutKeyboardFunc(keyInput);
    glutMouseFunc(mouseClick);
    glutPassiveMotionFunc(passiveMotion); // Mouse-look
    glutTimerFunc(16, update, 0);       // Timer untuk game loop (~60 FPS)

    // Sembunyikan kursor
    glutSetCursor(GLUT_CURSOR_NONE);

    glewExperimental = GL_TRUE;
    glewInit();

    setup(); // Panggil inisialisasi

    glutMainLoop();
    return 0;
}

// Inisialisasi OpenGL
void setup(void)
{
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Latar belakang abu-abu gelap
    glEnable(GL_DEPTH_TEST); // Aktifkan depth testing untuk 3D
    resetSim();
}

// Reset simulasi ke status awal
void resetSim(void)
{
    pinPulled = false;
    isSpraying = false;
    fireActive = true;
    fireHealth = 100.0f;
    camX = 0.0f;
    camY = 5.0f;
    camZ = 20.0f;
    camYaw = 0.0f;
    camPitch = 0.0f;

    // Set mouse ke tengah
    glutWarpPointer(winW / 2, winH / 2);
    lastMouseX = winW / 2;
    lastMouseY = winH / 2;
}


// --- Fungsi Menggambar ---

// Menggambar seluruh adegan (dipanggil oleh display func)
void drawScene(void)
{
    // Bersihkan buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set matriks Model-View
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Atur kamera menggunakan gluLookAt
    // Mata kita ada di (camX, camY, camZ)
    // Kita melihat ke (camX + lookX, camY + lookY, camZ + lookZ)
    // Arah "atas" adalah (0, 1, 0)
    gluLookAt(camX, camY, camZ,
        camX + lookX, camY + lookY, camZ + lookZ,
        0.0f, 1.0f, 0.0f);

    // Gambar elemen-elemen adegan
    drawRoom();
    drawFire();
    if (isSpraying) {
        drawSpray();
    }

    // Gambar UI (2D) di atas adegan 3D
    drawUI();

    // Tukar buffer (depan dan belakang)
    glutSwapBuffers();
}

// Menggambar lantai dan dinding
void drawRoom(void)
{
    // Lantai
    glColor3f(0.5f, 0.5f, 0.5f); // Abu-abu
    glPushMatrix();
    glTranslatef(0.0f, -0.5f, 0.0f); // Posisi di bawah
    glScalef(50.0f, 1.0f, 50.0f); // Besar dan tipis
    glutSolidCube(1.0);
    glPopMatrix();

    // Dinding Belakang
    glColor3f(0.8f, 0.8f, 0.7f); // Krem
    glPushMatrix();
    glTranslatef(0.0f, 10.0f, -25.0f); // Di belakang
    glScalef(50.0f, 20.0f, 1.0f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Dinding Kiri
    glColor3f(0.8f, 0.7f, 0.8f);
    glPushMatrix();
    glTranslatef(-25.0f, 10.0f, 0.0f);
    glScalef(1.0f, 20.0f, 50.0f);
    glutSolidCube(1.0);
    glPopMatrix();
}

// Menggambar api (jika aktif)
void drawFire(void)
{
    if (!fireActive) return; // Jika api padam, jangan gambar

    // Api terdiri dari 2 kerucut (cone) yang berflicker
    float flicker = 1.0f + (rand() % 100) / 500.0f; // Variasi kecil

    glPushMatrix();
    glTranslatef(fireX, fireY, fireZ); // Pindah ke posisi api

    // Api bagian dalam (Kuning)
    glColor3f(1.0f, 1.0f, 0.0f);
    glScalef(1.5f * flicker, 5.0f * flicker, 1.5f * flicker);
    glutSolidCone(1.0, 1.0, 10, 2);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(fireX, fireY, fireZ);
    // Api bagian luar (Oranye)
    glColor3f(1.0f, 0.5f, 0.0f);
    glScalef(2.0f, 7.0f, 2.0f);
    glutSolidCone(1.0, 1.0, 10, 2);
    glPopMatrix();
}

// Menggambar efek semprotan (kerucut putih transparan)
void drawSpray(void)
{
    // Aktifkan blending untuk transparansi
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(1.0f, 1.0f, 1.0f, 0.3f); // Putih, 30% transparan

    glPushMatrix();

    // Posisikan "semprotan" sedikit di depan kamera dan sedikit ke bawah
    // dan arahkan sesuai arah pandang kamera
    glTranslatef(camX + lookX * 1.5f, camY + lookY * 1.5f - 0.5f, camZ + lookZ * 1.5f);

    // Kita perlu memutar kerucut agar menghadap ke arah "lookVector"
    // Ini sedikit rumit, jadi kita sederhanakan:
    // Kita akan gambar kerucut yang sangat panjang ke arah Z
    // dan kita putar agar sesuai dengan camYaw dan camPitch
    glRotatef(camYaw, 0.0f, 1.0f, 0.0f);
    glRotatef(camPitch, 1.0f, 0.0f, 0.0f);

    glScalef(5.0f, 5.0f, 20.0f); // Tipis dan panjang (panjang di Z)
    glutSolidCone(1.0, 1.0, 10, 2); // Kerucut

    glPopMatrix();

    // Matikan blending
    glDisable(GL_BLEND);
}

// Menggambar UI Teks (2D)
void drawUI(void)
{
    // Simpan matriks 3D
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH); // Ganti ke mode 2D

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Matikan depth test agar UI selalu di atas
    glDisable(GL_DEPTH_TEST);

    // Gambar pesan
    if (fireActive) {
        glColor3f(1.0f, 1.0f, 0.0f); // Kuning
    }
    else {
        glColor3f(0.0f, 1.0f, 0.0f); // Hijau
    }
    drawText(10.0f, 50.0f, uiMessage.c_str());

    // Info tambahan
    glColor3f(1.0f, 1.0f, 1.0f); // Putih
    drawText(10.0f, 30.0f, "Tekan 'R' untuk Reset, 'ESC' untuk Keluar");

    // Crosshair
    drawText(winW / 2 - 5, winH / 2 - 5, "+");

    // Kembalikan status OpenGL
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// Fungsi helper untuk menggambar teks bitmap
void drawText(float x, float y, const char* text)
{
    glRasterPos2f(x, y);
    while (*text) {
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *text);
        text++;
    }
}

// --- Fungsi Logika dan Input ---

// Update loop (dipanggil oleh timer)
void update(int value)
{
    updateLogic(); // Update status game
    glutPostRedisplay(); // Minta gambar ulang adegan
    glutTimerFunc(16, update, 0); // Set timer berikutnya
}

// Logika utama simulasi
void updateLogic(void)
{
    // 1. Hitung vektor arah (look vector) dari yaw dan pitch
    lookX = cos(toRadians(camYaw)) * cos(toRadians(camPitch));
    lookY = sin(toRadians(camPitch));
    lookZ = sin(toRadians(camYaw)) * cos(toRadians(camPitch));

    // Normalisasi (sebenarnya sudah, tapi untuk keamanan)
    float len = sqrt(lookX * lookX + lookY * lookY + lookZ * lookZ);
    lookX /= len;
    lookY /= len;
    lookZ /= len;

    // 2. Update Pesan UI berdasarkan status
    if (!fireActive) {
        uiMessage = "Api Berhasil Dipadamkan! Tekan 'R' untuk Reset.";
        isSpraying = false; // Berhenti semprot otomatis
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

    // 3. Logika "Collision" Sederhana
    if (isSpraying && fireActive)
    {
        // Hitung vektor dari kamera ke api
        float vecToFireX = fireX - camX;
        float vecToFireY = fireY - camY;
        float vecToFireZ = fireZ - camZ;

        // Normalisasi vektor ke api
        float distToFire = sqrt(vecToFireX * vecToFireX + vecToFireY * vecToFireY + vecToFireZ * vecToFireZ);
        vecToFireX /= distToFire;
        vecToFireY /= distToFire;
        vecToFireZ /= distToFire;

        // Hitung Dot Product
        // (lookVector) . (vecToFire)
        // Hasilnya 1 jika melihat tepat ke api, 0 jika 90 derajat, -1 jika membelakangi
        float dot = (lookX * vecToFireX) + (lookY * vecToFireY) + (lookZ * vecToFireZ);

        // Jika kita melihat cukup dekat ke api (dot > 0.95) dan jaraknya masuk akal
        if (dot > 0.95f && distToFire < 25.0f)
        {
            fireHealth -= 1.5f; // Kurangi kesehatan api
        }
    }

    // 4. Cek jika api padam
    if (fireHealth <= 0.0f) {
        fireActive = false;
    }
}


// Fungsi reshape (menyesuaikan viewport)
void resize(int w, int h)
{
    if (h == 0) h = 1; // Mencegah pembagian dengan nol
    winW = w;
    winH = h;

    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // Gunakan gluPerspective untuk viewport 3D yang benar
    gluPerspective(45.0f, (float)w / (float)h, 0.1f, 1000.0f);
}

// Input Keyboard (Tombol Ditekan)
void keyInput(unsigned char key, int x, int y)
{
    // Gunakan vektor 'look' dan 'right' untuk gerakan (strafe)
    float rightX = cos(toRadians(camYaw - 90.0f));
    float rightZ = sin(toRadians(camYaw - 90.0f));

    switch (key)
    {
    case 27: // ESC
        exit(0);
        break;
    case 'w': // Maju
        camX += lookX * moveSpeed;
        camZ += lookZ * moveSpeed;
        break;
    case 's': // Mundur
        camX -= lookX * moveSpeed;
        camZ -= lookZ * moveSpeed;
        break;
    case 'a': // Kiri (Strafe)
        camX += rightX * moveSpeed;
        camZ += rightZ * moveSpeed;
        break;
    case 'd': // Kanan (Strafe)
        camX -= rightX * moveSpeed;
        camZ -= rightZ * moveSpeed;
        break;
    case 'e': // Aksi (Pull Pin)
        if (!pinPulled) {
            pinPulled = true;
        }
        break;
    case 'r': // Reset
        resetSim();
        break;
    }
}


// Input Mouse (Klik)
void mouseClick(int button, int state, int x, int y)
{
    // Tombol Kiri Ditekan
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
    {
        if (pinPulled && fireActive) { // Hanya bisa semprot jika pin ditarik
            isSpraying = true;
        }
    }
    // Tombol Kiri Dilepas
    else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP)
    {
        isSpraying = false;
    }
}

// Input Mouse (Gerakan)
void passiveMotion(int x, int y)
{
    // Hitung delta (perubahan) posisi mouse
    float deltaX = (float)(x - lastMouseX);
    float deltaY = (float)(y - lastMouseY);

    // Simpan posisi terakhir
    lastMouseX = x;
    lastMouseY = y;

    float sensitivity = 0.15f;
    camYaw += deltaX * sensitivity;
    camPitch -= deltaY * sensitivity; // Dibalik karena y=0 ada di atas

    // Batasi pitch (melihat atas/bawah) agar tidak terbalik
    if (camPitch > 89.0f) camPitch = 89.0f;
    if (camPitch < -89.0f) camPitch = -89.0f;

    // "Warp" kursor kembali ke tengah jendela
    // Ini penting untuk mouse-look FPS agar kursor tidak mentok di pinggir
    if (x != winW / 2 || y != winH / 2) {
        glutWarpPointer(winW / 2, winH / 2);
        lastMouseX = winW / 2;
        lastMouseY = winH / 2;
    }
}