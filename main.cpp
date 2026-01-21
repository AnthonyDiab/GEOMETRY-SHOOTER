#include <GL/glut.h>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>
#include <fstream>
#include <algorithm>

struct Vec2 { float x, y; };
struct Entity { Vec2 pos, vel; float radius; bool alive; int health; };
struct AmmoBox { Vec2 pos; float radius; bool alive; float rotation; };
struct PowerUp { Vec2 pos; float radius; bool alive; float rotation; float pulse; int type; };
struct Trail { Vec2 pos; float radius; float alpha; };

enum GameState { MENU, PLAYING, PAUSED, GAME_OVER, WAVE_TRANSITION };
enum EnemyType { NORMAL, FAST, TANK };

struct Enemy { Vec2 pos, vel; float radius; bool alive; int health; EnemyType type; };

Entity player = { {400, 300}, {0, 0}, 10, true, 3 };
std::vector<Entity> bullets, particles;
std::vector<Enemy> enemies;
std::vector<AmmoBox> ammoBoxes;
std::vector<PowerUp> powerUps;
std::vector<Trail> trails;

float keys[256] = { 0 };
bool specialKeys[256] = { 0 };
int score = 0, highScore = 0, ammo = 50, nukes = 0, playerHealth = 3, maxHealth = 3;
int wave = 1, enemiesLeftInWave = 5, comboCount = 0, mouseX = 400, mouseY = 300;
float spawnTimer = 0, ammoSpawnTimer = 0, powerUpSpawnTimer = 0, shootCooldown = 0;
float dashCooldown = 0, rapidFireTimer = 0, waveTransitionTimer = 0, comboTimer = 0;
float screenShakeX = 0, screenShakeY = 0, screenShakeIntensity = 0, survivalTime = 0;
float slowMoTimer = 0;
bool mouseHeld = false, hasShield = false;
GameState gameState = MENU;

Vec2 operator+(Vec2 a, Vec2 b) { return { a.x + b.x, a.y + b.y }; }
Vec2 operator*(Vec2 a, float s) { return { a.x * s, a.y * s }; }
float length(Vec2 v) { return sqrt(v.x * v.x + v.y * v.y); }
Vec2 normalize(Vec2 v) { float l = length(v); return l > 0 ? Vec2{ v.x / l, v.y / l } : v; }

void saveHighScore() {
    std::ofstream file("highscore.dat");
    if (file.is_open()) { file << highScore; file.close(); }
}

void loadHighScore() {
    std::ifstream file("highscore.dat");
    if (file.is_open()) { file >> highScore; file.close(); }
}

void resetGame() {
    player = { {400, 300}, {0, 0}, 10, true, 3 };
    bullets.clear(); enemies.clear(); particles.clear(); ammoBoxes.clear();
    powerUps.clear(); trails.clear();
    score = ammo = 0; ammo = 50; nukes = playerHealth = 0; playerHealth = 3;
    wave = 1; enemiesLeftInWave = 5; comboCount = 0;
    spawnTimer = ammoSpawnTimer = powerUpSpawnTimer = shootCooldown = 0;
    dashCooldown = rapidFireTimer = waveTransitionTimer = comboTimer = 0;
    screenShakeIntensity = survivalTime = slowMoTimer = 0;
    mouseHeld = hasShield = false;
    gameState = PLAYING;
}

void addScreenShake(float intensity) { screenShakeIntensity = intensity; }

void drawCircle(float x, float y, float r, float R, float G, float B, bool filled = false) {
    glColor3f(R, G, B);
    glBegin(filled ? GL_POLYGON : GL_LINE_LOOP);
    for (int i = 0; i < 30; i++) {
        float angle = i * 3.14159f * 2 / 30;
        glVertex2f(x + cos(angle) * r, y + sin(angle) * r);
    }
    glEnd();
}

void drawGlow(float x, float y, float r, float R, float G, float B, float alpha = 0.3f) {
    for (int i = 0; i < 3; i++) {
        glColor4f(R, G, B, (alpha - i * 0.1f));
        glBegin(GL_POLYGON);
        for (int j = 0; j < 30; j++) {
            float angle = j * 3.14159f * 2 / 30;
            glVertex2f(x + cos(angle) * (r + i * 3), y + sin(angle) * (r + i * 3));
        }
        glEnd();
    }
}

void drawSquare(float x, float y, float size, float rotation, float R, float G, float B) {
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(rotation, 0, 0, 1);
    glColor3f(R, G, B);
    glBegin(GL_LINE_LOOP);
    glVertex2f(-size, -size); glVertex2f(size, -size);
    glVertex2f(size, size); glVertex2f(-size, size);
    glEnd();
    glPopMatrix();
}

void drawStar(float x, float y, float size, float rotation, float R, float G, float B) {
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(rotation, 0, 0, 1);
    glColor3f(R, G, B);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 10; i++) {
        float angle = i * 3.14159f / 5;
        float r = (i % 2 == 0) ? size : size * 0.4f;
        glVertex2f(cos(angle) * r, sin(angle) * r);
    }
    glEnd();
    glPopMatrix();
}

void drawHeart(float x, float y, float size, bool filled) {
    glPushMatrix();
    glTranslatef(x, y, 0);
    glColor3f(1, 0, 0);
    glBegin(filled ? GL_POLYGON : GL_LINE_LOOP);
    for (int i = 0; i < 100; i++) {
        float t = i * 3.14159f * 2 / 100;
        float hx = size * 16 * pow(sin(t), 3);
        float hy = size * (13 * cos(t) - 5 * cos(2 * t) - 2 * cos(3 * t) - cos(4 * t));
        glVertex2f(hx * 0.05f, -hy * 0.05f);
    }
    glEnd();
    glPopMatrix();
}

void drawText(float x, float y, const char* text, float R, float G, float B) {
    glColor3f(R, G, B);
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; c++)
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);
}

void drawTextLarge(float x, float y, const char* text, float R, float G, float B) {
    glColor3f(R, G, B);
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
}

void drawButton(float x, float y, float w, float h, const char* text, float R, float G, float B) {
    glColor3f(R, G, B);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
    drawText(x + 20, y + 15, text, R, G, B);
}

bool tooCloseToPlayer(float x, float y, float minDist) {
    return length({ x - player.pos.x, y - player.pos.y }) < minDist;
}

void damagePlayer() {
    if (hasShield) {
        hasShield = false;
        addScreenShake(8);
        for (int i = 0; i < 30; i++) {
            float a = rand() % 360 * 3.14159f / 180;
            particles.push_back({ player.pos, {cos(a) * 5, sin(a) * 5}, 3, true, 1 });
        }
    }
    else {
        playerHealth--;
        addScreenShake(15);
        comboCount = 0;
        if (playerHealth <= 0) {
            player.alive = false;
            gameState = GAME_OVER;
            if (score > highScore) { highScore = score; saveHighScore(); }
        }
    }
}

void activateNuke() {
    if (nukes > 0) {
        nukes--;
        addScreenShake(20);
        for (int i = 0; i < 150; i++) {
            float a = rand() % 360 * 3.14159f / 180;
            particles.push_back({ player.pos, {cos(a) * 10, sin(a) * 10}, 6, true, 1 });
        }
        for (size_t i = 0; i < enemies.size(); i++) {
            if (enemies[i].alive) {
                score += 10 * (comboCount / 5 + 1);
                comboCount++; comboTimer = 3.0f;
                for (int j = 0; j < 15; j++) {
                    float a = rand() % 360 * 3.14159f / 180;
                    particles.push_back({ enemies[i].pos, {cos(a) * 5, sin(a) * 5}, 4, true, 1 });
                }
            }
        }
        enemies.clear();
        enemiesLeftInWave = 0;
    }
}

void performDash() {
    if (dashCooldown <= 0) {
        Vec2 dir = { 0, 0 };
        if (keys['w'] || specialKeys[GLUT_KEY_UP]) dir.y += 1;
        if (keys['s'] || specialKeys[GLUT_KEY_DOWN]) dir.y -= 1;
        if (keys['a'] || specialKeys[GLUT_KEY_LEFT]) dir.x -= 1;
        if (keys['d'] || specialKeys[GLUT_KEY_RIGHT]) dir.x += 1;

        if (length(dir) > 0) {
            Vec2 dashDir = normalize(dir);
            player.pos = player.pos + dashDir * 80.0f;
            player.pos.x = std::max(20.0f, std::min(780.0f, player.pos.x));
            player.pos.y = std::max(20.0f, std::min(580.0f, player.pos.y));
            dashCooldown = 1.5f;
            addScreenShake(5);
            for (int i = 0; i < 20; i++)
                particles.push_back({ player.pos, {(rand() % 100 - 50) / 10.0f, (rand() % 100 - 50) / 10.0f}, 3, true, 1 });
        }
    }
}

void shootBullet() {
    float fireRate = (rapidFireTimer > 0) ? 0.08f : 0.15f;
    if (ammo > 0 && shootCooldown <= 0) {
        Vec2 dir = normalize({ (float)mouseX - player.pos.x, (float)(600 - mouseY) - player.pos.y });
        bullets.push_back({ player.pos, dir * 12.0f, 4, true, 1 });
        ammo--;
        shootCooldown = fireRate;
    }
}

void spawnEnemy() {
    float spawnAngle = rand() % 360 * 3.14159f / 180;
    float spawnX, spawnY;
    do {
        spawnX = 400 + cos(spawnAngle) * (350 + rand() % 150);
        spawnY = 300 + sin(spawnAngle) * (250 + rand() % 150);
    } while (tooCloseToPlayer(spawnX, spawnY, 200));

    EnemyType type = NORMAL;
    int health = 1;
    float radius = 12;

    int roll = rand() % 100;
    if (wave >= 3) {
        if (roll < 40) { type = NORMAL; health = 1; }
        else if (roll < 70) { type = FAST; health = 1; radius = 10; }
        else { type = TANK; health = 2; radius = 15; }
    }
    if (wave >= 5) {
        if (type == TANK) health = 3;
        else if (rand() % 100 < 30) health = 2;
    }

    Enemy e;
    e.pos = { spawnX, spawnY };
    e.vel = { 0, 0 };
    e.radius = radius;
    e.alive = true;
    e.health = health;
    e.type = type;
    enemies.push_back(e);
}

void startNextWave() {
    wave++;
    enemiesLeftInWave = 5 + wave * 2;
    waveTransitionTimer = 3.0f;
    gameState = WAVE_TRANSITION;
    ammo += 20;
    if (wave % 3 == 0) nukes++;
}

void update(int v) {
    if (screenShakeIntensity > 0) {
        screenShakeX = (rand() % 100 - 50) / 50.0f * screenShakeIntensity;
        screenShakeY = (rand() % 100 - 50) / 50.0f * screenShakeIntensity;
        screenShakeIntensity *= 0.9f;
        if (screenShakeIntensity < 0.1f) screenShakeIntensity = screenShakeX = screenShakeY = 0;
    }

    if (gameState == MENU || gameState == GAME_OVER || gameState == PAUSED) {
        glutPostRedisplay();
        glutTimerFunc(16, update, 0);
        return;
    }

    if (gameState == WAVE_TRANSITION) {
        waveTransitionTimer -= 0.016f;
        if (waveTransitionTimer <= 0) gameState = PLAYING;
        glutPostRedisplay();
        glutTimerFunc(16, update, 0);
        return;
    }

    survivalTime += 0.016f;
    if (shootCooldown > 0) shootCooldown -= 0.016f;
    if (dashCooldown > 0) dashCooldown -= 0.016f;
    if (rapidFireTimer > 0) rapidFireTimer -= 0.016f;
    if (slowMoTimer > 0) slowMoTimer -= 0.016f;
    if (comboTimer > 0) comboTimer -= 0.016f;
    else comboCount = 0;

    if (mouseHeld && player.alive) shootBullet();

    Vec2 dir = { 0, 0 };
    if (keys['w'] || specialKeys[GLUT_KEY_UP]) dir.y += 1;
    if (keys['s'] || specialKeys[GLUT_KEY_DOWN]) dir.y -= 1;
    if (keys['a'] || specialKeys[GLUT_KEY_LEFT]) dir.x -= 1;
    if (keys['d'] || specialKeys[GLUT_KEY_RIGHT]) dir.x += 1;
    player.vel = normalize(dir) * 4.5f;
    player.pos = player.pos + player.vel;
    player.pos.x = std::max(20.0f, std::min(780.0f, player.pos.x));
    player.pos.y = std::max(20.0f, std::min(580.0f, player.pos.y));

    if (length(player.vel) > 0) trails.push_back({ player.pos, 6, 0.5f });

    for (size_t i = 0; i < bullets.size(); i++) {
        trails.push_back({ bullets[i].pos, 3, 0.3f });
        bullets[i].pos = bullets[i].pos + bullets[i].vel;
        if (bullets[i].pos.x < 0 || bullets[i].pos.x > 800 || bullets[i].pos.y < 0 || bullets[i].pos.y > 600)
            bullets[i].alive = false;
    }

    if (enemiesLeftInWave > 0) {
        spawnTimer += 0.016f;
        float spawnRate = std::max(0.3f, 1.0f - wave * 0.05f);
        if (spawnTimer > spawnRate) {
            spawnTimer = 0;
            spawnEnemy();
            enemiesLeftInWave--;
        }
    }

    if (enemiesLeftInWave <= 0 && enemies.empty()) startNextWave();

    float slowMult = (slowMoTimer > 0) ? 0.3f : 1.0f;
    for (size_t i = 0; i < enemies.size(); i++) {
        Vec2 toPlayer = { player.pos.x - enemies[i].pos.x, player.pos.y - enemies[i].pos.y };
        float speed = 2.0f;
        if (enemies[i].type == FAST) speed = 2.8f;
        else if (enemies[i].type == TANK) speed = 1.2f;
        enemies[i].vel = normalize(toPlayer) * speed * slowMult;
        enemies[i].pos = enemies[i].pos + enemies[i].vel;
    }

    ammoSpawnTimer += 0.016f;
    if (ammoSpawnTimer > 8.0f) {
        ammoSpawnTimer = 0;
        ammoBoxes.push_back({ {float(rand() % 700 + 50), float(rand() % 500 + 50)}, 15, true, 0 });
    }
    for (size_t i = 0; i < ammoBoxes.size(); i++) {
        ammoBoxes[i].rotation += 1.0f;
        if (length({ ammoBoxes[i].pos.x - player.pos.x, ammoBoxes[i].pos.y - player.pos.y }) < ammoBoxes[i].radius + player.radius) {
            ammoBoxes[i].alive = false;
            ammo += 25;
            for (int j = 0; j < 15; j++) {
                float a = rand() % 360 * 3.14159f / 180;
                particles.push_back({ ammoBoxes[i].pos, {cos(a) * 3, sin(a) * 3}, 2, true, 1 });
            }
        }
    }

    powerUpSpawnTimer += 0.016f;
    if (powerUpSpawnTimer > 20.0f) {
        powerUpSpawnTimer = 0;
        int type = rand() % 4; // 0=nuke, 1=shield, 2=rapid, 3=slowmo
        powerUps.push_back({ {float(rand() % 700 + 50), float(rand() % 500 + 50)}, 18, true, 0, 0, type });
    }
    for (size_t i = 0; i < powerUps.size(); i++) {
        powerUps[i].rotation += 2.0f;
        powerUps[i].pulse += 0.1f;
        if (length({ powerUps[i].pos.x - player.pos.x, powerUps[i].pos.y - player.pos.y }) < powerUps[i].radius + player.radius) {
            powerUps[i].alive = false;
            if (powerUps[i].type == 0) nukes++;
            else if (powerUps[i].type == 1) hasShield = true;
            else if (powerUps[i].type == 2) rapidFireTimer = 10.0f;
            else if (powerUps[i].type == 3) slowMoTimer = 8.0f;
            for (int j = 0; j < 20; j++) {
                float a = rand() % 360 * 3.14159f / 180;
                particles.push_back({ powerUps[i].pos, {cos(a) * 4, sin(a) * 4}, 3, true, 1 });
            }
        }
    }

    for (size_t i = 0; i < enemies.size(); i++) {
        if (!enemies[i].alive) continue;
        for (size_t j = 0; j < bullets.size(); j++) {
            if (!bullets[j].alive) continue;
            if (length({ enemies[i].pos.x - bullets[j].pos.x, enemies[i].pos.y - bullets[j].pos.y }) < enemies[i].radius + bullets[j].radius) {
                bullets[j].alive = false;
                enemies[i].health--;
                for (int k = 0; k < 8; k++) {
                    float a = rand() % 360 * 3.14159f / 180;
                    particles.push_back({ enemies[i].pos, {cos(a) * 3, sin(a) * 3}, 2, true, 1 });
                }
                if (enemies[i].health <= 0) {
                    enemies[i].alive = false;
                    score += 10 * (comboCount / 5 + 1);
                    comboCount++; comboTimer = 3.0f;
                    addScreenShake(3);
                    for (int k = 0; k < 20; k++) {
                        float a = rand() % 360 * 3.14159f / 180;
                        particles.push_back({ enemies[i].pos, {cos(a) * 4, sin(a) * 4}, 3, true, 1 });
                    }
                }
            }
        }
        if (enemies[i].alive && length({ enemies[i].pos.x - player.pos.x, enemies[i].pos.y - player.pos.y }) < enemies[i].radius + player.radius) {
            enemies[i].alive = false;
            damagePlayer();
        }
    }

    bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](Entity& e) {return !e.alive; }), bullets.end());
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](Enemy& e) {return !e.alive; }), enemies.end());
    ammoBoxes.erase(std::remove_if(ammoBoxes.begin(), ammoBoxes.end(), [](AmmoBox& b) {return !b.alive; }), ammoBoxes.end());
    powerUps.erase(std::remove_if(powerUps.begin(), powerUps.end(), [](PowerUp& p) {return !p.alive; }), powerUps.end());

    for (size_t i = 0; i < trails.size(); i++) {
        trails[i].alpha *= 0.92f;
        trails[i].radius *= 0.95f;
    }
    trails.erase(std::remove_if(trails.begin(), trails.end(), [](Trail& t) {return t.alpha < 0.05f; }), trails.end());

    for (size_t i = 0; i < particles.size(); i++) {
        particles[i].pos = particles[i].pos + particles[i].vel;
        particles[i].vel = particles[i].vel * 0.97f;
        particles[i].radius *= 0.96f;
        if (particles[i].radius < 0.3f) particles[i].alive = false;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(), [](Entity& e) {return !e.alive; }), particles.end());

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glPushMatrix();
    glTranslatef(screenShakeX, screenShakeY, 0);

    glColor3f(0.1f, 0.1f, 0.15f);
    glBegin(GL_LINES);
    for (int i = 0; i < 800; i += 40) { glVertex2f(i, 0); glVertex2f(i, 600); }
    for (int i = 0; i < 600; i += 40) { glVertex2f(0, i); glVertex2f(800, i); }
    glEnd();

    if (gameState == MENU) {
        drawTextLarge(300, 400, "GEOMETRY SHOOTER", 0, 1, 1);
        drawButton(300, 320, 200, 40, "START GAME (ENTER)", 0, 1, 0);
        drawButton(300, 260, 200, 40, "QUIT (ESC)", 1, 0, 0);
        char highScoreText[50];
        sprintf(highScoreText, "HIGH SCORE: %d", highScore);
        drawText(320, 200, highScoreText, 1, 1, 0);
        drawText(200, 100, "Controls:", 1, 1, 1);
        drawText(200, 80, "WASD/Arrows - Move", 0.8f, 0.8f, 0.8f);
        drawText(200, 60, "Mouse - Aim & Shoot", 0.8f, 0.8f, 0.8f);
        drawText(200, 40, "E - Dash", 0.8f, 0.8f, 0.8f);
        drawText(200, 20, "SPACE - Nuke", 0.8f, 0.8f, 0.8f);
        glPopMatrix();
        glutSwapBuffers();
        return;
    }

    for (size_t i = 0; i < trails.size(); i++) {
        glColor4f(0, 0.8f, 1, trails[i].alpha);
        glBegin(GL_POLYGON);
        for (int j = 0; j < 20; j++) {
            float angle = j * 3.14159f * 2 / 20;
            glVertex2f(trails[i].pos.x + cos(angle) * trails[i].radius, trails[i].pos.y + sin(angle) * trails[i].radius);
        }
        glEnd();
    }

    for (size_t i = 0; i < ammoBoxes.size(); i++) {
        drawGlow(ammoBoxes[i].pos.x, ammoBoxes[i].pos.y, ammoBoxes[i].radius, 0, 1, 0);
        drawSquare(ammoBoxes[i].pos.x, ammoBoxes[i].pos.y, ammoBoxes[i].radius, ammoBoxes[i].rotation, 0, 1, 0);
        drawSquare(ammoBoxes[i].pos.x, ammoBoxes[i].pos.y, ammoBoxes[i].radius * 0.6f, -ammoBoxes[i].rotation, 0.5f, 1, 0.5f);
    }

    for (size_t i = 0; i < powerUps.size(); i++) {
        float pulseSize = powerUps[i].radius + sin(powerUps[i].pulse) * 3;
        if (powerUps[i].type == 0) {
            drawGlow(powerUps[i].pos.x, powerUps[i].pos.y, pulseSize, 1, 0, 1);
            drawStar(powerUps[i].pos.x, powerUps[i].pos.y, powerUps[i].radius, powerUps[i].rotation, 1, 0, 1);
        }
        else if (powerUps[i].type == 1) {
            drawGlow(powerUps[i].pos.x, powerUps[i].pos.y, pulseSize, 0, 0.5f, 1);
            drawCircle(powerUps[i].pos.x, powerUps[i].pos.y, powerUps[i].radius, 0, 0.5f, 1);
        }
        else if (powerUps[i].type == 2) {
            drawGlow(powerUps[i].pos.x, powerUps[i].pos.y, pulseSize, 1, 0.5f, 0);
            drawSquare(powerUps[i].pos.x, powerUps[i].pos.y, powerUps[i].radius, powerUps[i].rotation, 1, 0.5f, 0);
        }
        else {
            drawGlow(powerUps[i].pos.x, powerUps[i].pos.y, pulseSize, 0, 1, 1);
            drawCircle(powerUps[i].pos.x, powerUps[i].pos.y, powerUps[i].radius, 0, 1, 1);
            drawCircle(powerUps[i].pos.x, powerUps[i].pos.y, powerUps[i].radius * 0.5f, 0.5f, 1, 1);
        }
    }

    if (player.alive) {
        drawGlow(player.pos.x, player.pos.y, player.radius, 0, 0.8f, 1);
        drawCircle(player.pos.x, player.pos.y, player.radius, 0, 1, 1);
        drawCircle(player.pos.x, player.pos.y, player.radius * 0.6f, 0.5f, 1, 1);
        if (hasShield) drawCircle(player.pos.x, player.pos.y, player.radius + 5, 0, 0.5f, 1);
    }

    for (size_t i = 0; i < bullets.size(); i++) {
        drawGlow(bullets[i].pos.x, bullets[i].pos.y, bullets[i].radius, 1, 1, 0);
        drawCircle(bullets[i].pos.x, bullets[i].pos.y, bullets[i].radius, 1, 1, 0, true);
    }

    for (size_t i = 0; i < enemies.size(); i++) {
        float R = 1, G = 0, B = 0;
        if (enemies[i].type == FAST) { R = 1; G = 0.5f; B = 0; }
        else if (enemies[i].type == TANK) { R = 0.8f; G = 0; B = 0.8f; }

        drawGlow(enemies[i].pos.x, enemies[i].pos.y, enemies[i].radius, R, G, B);
        drawCircle(enemies[i].pos.x, enemies[i].pos.y, enemies[i].radius, R, G, B);
        drawCircle(enemies[i].pos.x, enemies[i].pos.y, enemies[i].radius * 0.5f, R * 0.7f, G * 0.7f, B * 0.7f);

        if (enemies[i].health > 1) {
            glColor3f(1, 1, 0);
            glLineWidth(2);
            glBegin(GL_LINES);
            for (int j = 0; j < enemies[i].health; j++) {
                glVertex2f(enemies[i].pos.x - 10 + j * 7, enemies[i].pos.y + enemies[i].radius + 5);
                glVertex2f(enemies[i].pos.x - 6 + j * 7, enemies[i].pos.y + enemies[i].radius + 5);
            }
            glEnd();
            glLineWidth(1);
        }
    }

    for (size_t i = 0; i < particles.size(); i++) {
        glColor3f(1, 0.7f, 0);
        glPointSize(particles[i].radius * 2);
        glBegin(GL_POINTS);
        glVertex2f(particles[i].pos.x, particles[i].pos.y);
        glEnd();
    }

    glPopMatrix();

    char text[100];
    sprintf(text, "SCORE: %d", score);
    drawText(10, 580, text, 0, 1, 1);
    sprintf(text, "AMMO: %d", ammo);
    drawText(10, 560, text, 1, 1, 0);
    sprintf(text, "NUKES: %d", nukes);
    drawText(10, 540, text, 1, 0, 1);
    sprintf(text, "WAVE: %d", wave);
    drawText(10, 520, text, 1, 0.5f, 0);
    sprintf(text, "TIME: %.1fs", survivalTime);
    drawText(650, 580, text, 0.8f, 0.8f, 0.8f);

    for (int i = 0; i < playerHealth; i++) drawHeart(720 + i * 25, 550, 1, true);
    for (int i = playerHealth; i < maxHealth; i++) drawHeart(720 + i * 25, 550, 1, false);

    if (comboCount > 0) {
        sprintf(text, "COMBO x%d", comboCount / 5 + 1);
        drawTextLarge(10, 490, text, 1, 1, 0);
    }

    if (dashCooldown > 0) {
        sprintf(text, "DASH: %.1fs", dashCooldown);
        drawText(10, 470, text, 0.6f, 0.6f, 1);
    }
    else drawText(10, 470, "DASH: READY (E)", 0, 1, 0);

    if (rapidFireTimer > 0) {
        sprintf(text, "RAPID FIRE: %.1fs", rapidFireTimer);
        drawText(10, 450, text, 1, 0.5f, 0);
    }

    if (slowMoTimer > 0) {
        sprintf(text, "SLOW-MO: %.1fs", slowMoTimer);
        drawText(10, 430, text, 0, 1, 1);
    }

    if (hasShield) drawText(10, 410, "SHIELD ACTIVE", 0, 0.8f, 1);

    if (gameState == WAVE_TRANSITION) {
        sprintf(text, "WAVE %d COMPLETE!", wave - 1);
        drawTextLarge(280, 350, text, 0, 1, 0);
        sprintf(text, "WAVE %d STARTING...", wave);
        drawTextLarge(280, 320, text, 1, 1, 0);
        drawText(300, 280, "+20 Ammo", 1, 1, 0);
        if (wave % 3 == 1) drawText(300, 260, "+1 Nuke", 1, 0, 1);
    }

    if (gameState == PAUSED) {
        drawTextLarge(340, 350, "PAUSED", 1, 1, 1);
        drawText(310, 310, "Press P to Resume", 0.8f, 0.8f, 0.8f);
    }

    if (gameState == GAME_OVER) {
        drawTextLarge(320, 380, "GAME OVER", 1, 0, 0);
        sprintf(text, "Final Score: %d", score);
        drawText(330, 350, text, 1, 1, 1);
        sprintf(text, "Waves Survived: %d", wave - 1);
        drawText(320, 330, text, 0, 1, 1);
        sprintf(text, "Time: %.1fs", survivalTime);
        drawText(340, 310, text, 1, 1, 0);
        if (score == highScore && score > 0) drawText(310, 280, "NEW HIGH SCORE!", 1, 1, 0);
        drawButton(325, 230, 150, 30, "RESTART (R)", 0, 1, 0);
        drawButton(325, 190, 150, 30, "MENU (M)", 1, 1, 0);
    }

    glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y) {
    keys[key] = 1;
    if (gameState == MENU) {
        if (key == 13 || key == 10) resetGame();
        if (key == 27) exit(0);
    }
    if (gameState == PLAYING) {
        if (key == 'r' || key == 'R') resetGame();
        if (key == ' ') activateNuke();
        if (key == 'p' || key == 'P') gameState = PAUSED;
        if (key == 'e' || key == 'E') performDash();
    }
    if (gameState == PAUSED && (key == 'p' || key == 'P')) gameState = PLAYING;
    if (gameState == GAME_OVER) {
        if (key == 'r' || key == 'R') resetGame();
        if (key == 'm' || key == 'M') gameState = MENU;
    }
}

void keyboardUp(unsigned char key, int x, int y) { keys[key] = 0; }
void specialKeyboard(int key, int x, int y) { specialKeys[key] = true; }
void specialKeyboardUp(int key, int x, int y) { specialKeys[key] = false; }

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            mouseHeld = true;
            int glY = 600 - y;
            if (gameState == MENU) {
                if (x >= 300 && x <= 500 && glY >= 320 && glY <= 360) resetGame();
                if (x >= 300 && x <= 500 && glY >= 260 && glY <= 300) exit(0);
            }
            if (gameState == GAME_OVER) {
                if (x >= 325 && x <= 475 && glY >= 230 && glY <= 260) resetGame();
                if (x >= 325 && x <= 475 && glY >= 190 && glY <= 220) gameState = MENU;
            }
            if (gameState == PLAYING && player.alive) shootBullet();
        }
        else mouseHeld = false;
    }
}

void mouseMotion(int x, int y) { mouseX = x; mouseY = y; }
void passiveMotion(int x, int y) { mouseX = x; mouseY = y; }

int main(int argc, char** argv) {
    srand(time(0));
    loadHighScore();
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Geometry Shooter - Enhanced");
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 600);
    glClearColor(0.05f, 0.05f, 0.1f, 1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_POINT_SMOOTH);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialKeyboard);
    glutSpecialUpFunc(specialKeyboardUp);
    glutMouseFunc(mouse);
    glutMotionFunc(mouseMotion);
    glutPassiveMotionFunc(passiveMotion);
    glutTimerFunc(0, update, 0);
    glutMainLoop();
    return 0;
}