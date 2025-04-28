// [코드 시작]

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <GLFW/glfw3.h>
#include <Windows.h>
#include <iostream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <string>
#include <sstream>

enum GameState { START, PLAYING, GAME_OVER }; //게임 상태를 나타내는 코드
GameState gameState = START; 

float playerX = -1.2f;   // 플레이어 위치 좌표 
float playerY = 0.0f;
float velocityY = 0.0f;  // 위아래 움직이는 속도 ( 중력에 따라 다름 )
const float gravity = -0.001f;  // 밑으로 당기는 힘
const float thrust = 0.0025f;  // 위로 밀어주는 힘
float scrollSpeed = 0.003f;  // 맵이 왼쪽으로 지나가는 속도처리
bool isFlying = false; // 플레이어 현재 위치 

const float playerWidth = 0.1f; // 플레이어 가로, 세로
const float playerHeight = 0.3f;

int score = 0;
float scoreTimer = 0.0f;  // 초마다 점수 늘어나는 변수
const int maxHP = 5;  // 플레이어 최대 체력 (짧게 해보려고 5로 설정함 )
int playerHP = maxHP; 

struct Obstacle { float x, y, width, height; };  // 장애물 위치
struct Projectile { float x, y, width, height; }; // 플레이어 미사일
struct Enemy { float x, y, width, height; float speedX, speedY; };

std::vector<Obstacle> obstacles;
std::vector<Projectile> projectiles;
std::vector<Enemy> enemies;

float obstacleSpawnTimer = 0.0f; //2초마다 장애물 생성
float obstacleSpawnInterval = 2.0f; // 3초마다 적 생성

float enemySpawnTimer = 0.0f;
float enemySpawnInterval = 3.0f;  // 미사일 쿨타임 설정

float projectileCooldown = 0.0f;
const float projectileCooldownTime = 0.3f; 

GLuint playerTexture; // 플레이어 캐릭터 사진 ( 나중에 사진추가 )

void resetGame() {   // 게임 리셋하고 다시 할 때 초기화 변수
    playerX = -1.2f;
    playerY = 0.0f;
    velocityY = 0.0f;
    scrollSpeed = 0.003f;
    obstacles.clear();
    projectiles.clear();
    enemies.clear();
    obstacleSpawnTimer = 0.0f;
    enemySpawnTimer = 0.0f;
    score = 0;
    scoreTimer = 0.0f;
    playerHP = maxHP;
    projectileCooldown = 0.0f;
}

GLuint loadTexture(const char* filename) { //opengl 텍스트 변환 
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);
    if (!data) {
        std::cout << "Failed to load texture: " << filename << std::endl;
        return 0;
    }
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return texture;
}

void renderText(float x, float y, const char* str) {  // x,y 위치에 문자열 출력 (임시로 )
    glRasterPos2f(x, y); 
    for (const char* c = str; *c != '\0'; c++) {
        glCallList(1000 + *c);
    }
}

void renderTextCentered(float y, const char* str) { // 글자 가운데 정렬 
    float textWidth = 0.3f;
    renderText(-textWidth / 2, y, str);
}

void drawTexture(GLuint texture, float x, float y, float width, float height) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(1, 0); glVertex2f(x + width, y);
    glTexCoord2f(1, 1); glVertex2f(x + width, y + height);
    glTexCoord2f(0, 1); glVertex2f(x, y + height);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void drawRect(float x, float y, float width, float height) {
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

bool checkCollision(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2) {  // aabb 충돌체크 
    return !(x1 + w1 < x2 || x1 > x2 + w2 || y1 + h1 < y2 || y1 > y2 + h2);
}

void spawnObstacle() { // 장애물 생성
    float height = 0.3f + static_cast<float>(rand()) / RAND_MAX * 0.3f;
    float y = -1.0f + static_cast<float>(rand()) / RAND_MAX * (2.0f - height);
    obstacles.push_back({ 1.5f, y, 0.08f, height });
}

void spawnEnemy() { //적 생성
    float size = playerHeight / 5.0f;
    float y = -1.0f + static_cast<float>(rand()) / RAND_MAX * (2.0f - size);
    float speedX = 0.003f + static_cast<float>(rand()) / RAND_MAX * 0.002f;
    float speedY = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.01f;
    enemies.push_back({ 1.5f, y, size, size, speedX, speedY });
}

void processInput(GLFWwindow* window) { //입력 처리 함수
    isFlying = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (gameState == START && glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
        gameState = PLAYING;
        resetGame();
    }
    if (gameState == GAME_OVER && glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        gameState = START;
    }
    if (gameState == PLAYING && glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS && projectileCooldown <= 0.0f) {
        Projectile p;
        p.x = playerX + playerWidth + 0.02f;
        p.y = playerY + playerHeight / 2 - 0.02f;
        p.width = 0.06f;
        p.height = 0.02f;
        projectiles.push_back(p);
        projectileCooldown = projectileCooldownTime;
    }
}
 
int main() {  //glfw 초기화 
    srand(static_cast<unsigned int>(time(0)));
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(1600, 900, "Jetpack Joyride", NULL, NULL);
    if (!window) return -1;
    glfwMakeContextCurrent(window);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.5, 1.5, -1, 1, -1, 1);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);

    HDC hdc = wglGetCurrentDC();
    SelectObject(hdc, GetStockObject(SYSTEM_FONT));
    wglUseFontBitmaps(hdc, 0, 256, 1000);
    playerTexture = loadTexture("astronaut.png");

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;
        processInput(window);

        if (gameState == PLAYING) {
            scoreTimer += deltaTime;
            if (scoreTimer >= 1.0f) {
                score++;
                scrollSpeed += 0.0002f;
                scoreTimer = 0.0f;
            }

            if (isFlying) velocityY += thrust;
            velocityY += gravity;
            playerY += velocityY;
            if (playerY < -0.95f) { playerY = -0.95f; velocityY = 0; }
            if (playerY > 0.95f) { playerY = 0.95f; velocityY = 0; }

            for (auto& obs : obstacles) obs.x -= scrollSpeed;
            for (auto& proj : projectiles) proj.x += 0.02f;
            for (auto& enemy : enemies) {
                enemy.x -= enemy.speedX;
                enemy.y += enemy.speedY;
                if (enemy.y < -1.0f || enemy.y > 1.0f) enemy.speedY = -enemy.speedY;
            }

            obstacles.erase(std::remove_if(obstacles.begin(), obstacles.end(), [](Obstacle& o) { return o.x + o.width < -1.5f; }), obstacles.end());
            projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](Projectile& p) { return p.x > 1.5f; }), projectiles.end());
            enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](Enemy& e) { return e.x + e.width < -1.5f; }), enemies.end());

            obstacleSpawnTimer += deltaTime;
            enemySpawnTimer += deltaTime;
            if (obstacleSpawnTimer > obstacleSpawnInterval) { spawnObstacle(); obstacleSpawnTimer = 0.0f; }
            if (enemySpawnTimer > enemySpawnInterval) { spawnEnemy(); enemySpawnTimer = 0.0f; }

            if (projectileCooldown > 0.0f) projectileCooldown -= deltaTime;

            for (const auto& obs : obstacles) {
                if (checkCollision(playerX, playerY, playerWidth, playerHeight, obs.x, obs.y, obs.width, obs.height)) gameState = GAME_OVER;
            }

            for (auto& enemy : enemies) {
                if (checkCollision(playerX, playerY, playerWidth, playerHeight, enemy.x, enemy.y, enemy.width, enemy.height)) {
                    playerHP--;
                    enemy.x = -2.0f;
                    if (playerHP <= 0) gameState = GAME_OVER;
                }
            }
        }

        glClear(GL_COLOR_BUFFER_BIT);

        if (gameState == START) {
            glColor3f(1, 1, 1);
            renderTextCentered(0.0f, "Press ENTER to Start");
        }
        else {
            drawTexture(playerTexture, playerX, playerY, playerWidth, playerHeight);

            glColor3f(1.0f, 1.0f, 0.0f);
            for (const auto& proj : projectiles) drawRect(proj.x, proj.y, proj.width, proj.height);

            glColor3f(1.0f, 0.3f, 0.3f);
            for (const auto& obs : obstacles) drawRect(obs.x, obs.y, obs.width, obs.height);

            glColor3f(0.0f, 1.0f, 1.0f);
            for (const auto& enemy : enemies) drawRect(enemy.x, enemy.y, enemy.width, enemy.height);

            std::stringstream ss;
            ss << "Score: " << score;
            glColor3f(1, 1, 0);
            renderText(-1.45f, 0.9f, ss.str().c_str());

            // HP 바 그리기  ( 나중에 점수 아래나 위치 조정 예정 )
            float hpBarWidth = 0.6f;
            float hpBarHeight = 0.05f;
            float hpBarX = -0.3f;
            float hpBarY = 0.85f;
            float hpPercent = static_cast<float>(playerHP) / maxHP;

            // 체력바 테두리
            glColor3f(0.5f, 0.5f, 0.5f);
            drawRect(hpBarX - 0.01f, hpBarY - 0.01f, hpBarWidth + 0.02f, hpBarHeight + 0.02f);
            // 초록색 실제 체력바
            glColor3f(0.0f, 1.0f, 0.0f);
            drawRect(hpBarX, hpBarY, hpBarWidth * hpPercent, hpBarHeight);

            if (gameState == GAME_OVER) {
                glColor3f(1, 0, 0);
                renderTextCentered(0.0f, "GAME OVER");
                glColor3f(1, 1, 1);
                renderTextCentered(-0.1f, "Press R to Restart");
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}


