/**
 * Dependencies
 */
#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/**
 * Constants
 */
#define SCREEN_W                   800
#define SCREEN_H                   600
#define PLAYER_SPEED            180.0f
#define BULLET_SPEED            400.0f
#define ENEMY_SPEED              60.0f
#define ENEMY_BULLET_SPEED      200.0f
#define WIN_TIME                 60.0f
#define MAX_BULLETS                256
#define MAX_ENEMIES                 64
#define MAX_PROPS                  256
#define PLAYER_COOLDOWN_FRAMES      20
#define ENEMY_FIRE_COOLDOWN_FRAMES  90
#define MAX_BACKGROUND_DOTS       5000

/**
 * Structs
 */
typedef enum {
    PROP_TREE,
    PROP_ROCK,
    PROP_WIRE
} PropType;

typedef struct {
    float x, y;
    PropType kind;
    bool alive;
} StaticProp;

typedef struct {
    float x, y;
    float vx, vy;
    bool alive;
    bool from_enemy;
} Bullet;

typedef struct {
    float x, y;
    float vx, vy;
    bool alive;
    int  death_timer;
    int  fire_cooldown;
} Enemy;

typedef struct {
    float x, y;
    float aimx, aimy;
    int shoot_cooldown;
    bool alive;
} Player;

typedef struct {
    int x, y;
    Uint8 r, g, b;
} Dot;

/**
 * Globals
 */
static Player player;
static Bullet bullets[MAX_BULLETS];
static Enemy enemies[MAX_ENEMIES];
static StaticProp props[MAX_PROPS];
static int prop_count = 0;
static Dot dots[MAX_BACKGROUND_DOTS];
static int dot_count = 0;
static float survival_time = 0.0f;
static bool game_over = false;
static bool game_won = false;
static bool paused = false;
static bool running = true;
static float enemy_spawn_timer = 0.0f;
static uint64_t prev = 0;
static double freq = 0;

/**
 * Helper functions
 */
static float frand01(void) {
    return (float)rand() / (float)RAND_MAX;
}

static float frand_range(float a, float b) {
    return a + frand01()*(b-a);
}

static float length(float x, float y) {
    return sqrtf(x*x + y*y);
}

static void normalize(float *x, float *y) {
    float L = length(*x, *y);
    if (L > 0.0001f) {
        *x /= L;
        *y /= L;
    }
}

static float dist2(float ax, float ay, float bx, float by) {
    float dx = ax - bx;
    float dy = ay - by;
    return dx*dx + dy*dy;
}

static bool circle_hit(float ax, float ay, float ar,
                       float bx, float by, float br) {
    float dx = ax - bx;
    float dy = ay - by;
    float rr = ar + br;
    return (dx*dx + dy*dy) <= rr*rr;
}

/**
 * Add background dots
 */
static void generate_dots(void) {
    dot_count = 0;
    for (int i=0; i<MAX_BACKGROUND_DOTS; i++) {
        Dot d;
        d.x = (int)frand_range(0.0f, (float)SCREEN_W);
        d.y = (int)frand_range(0.0f, (float)SCREEN_H);

        // pick one of a few earthy tones
        float pick = frand01();
        if (pick < 0.5f) {
            // darker mud spots
            d.r = 30; d.g = 22; d.b = 16;
        } else if (pick < 0.8f) {
            // slightly lighter dirt chip
            d.r = 70; d.g = 55; d.b = 40;
        } else {
            // muted moss/lichen dot
            d.r = 70; d.g = 90; d.b = 60;
        }

        dots[dot_count++] = d;
        if (dot_count >= MAX_BACKGROUND_DOTS) break;
    }
}

/**
 * Add battlefield props
 */
static void generate_props(void) {
    prop_count = 0;
    for (int i=0; i<MAX_PROPS; i++) {
        float r = frand01();
        PropType k;
        if (r < 0.8f)      k = PROP_TREE;
        else if (r < 0.9f) k = PROP_ROCK;
        else               k = PROP_WIRE;

        float x = frand_range(30.0f, SCREEN_W - 30.0f);
        float y = frand_range(30.0f, SCREEN_H - 30.0f);

        // avoid spawn zone
        float d2c = dist2(x, y, SCREEN_W/2.0f, SCREEN_H/2.0f);
        if (d2c < 100.0f*100.0f) {
            i--;
            continue;
        }

        props[prop_count].x = x;
        props[prop_count].y = y;
        props[prop_count].kind = k;
        props[prop_count].alive = true;
        prop_count++;

        if (prop_count >= MAX_PROPS) break;
    }
}

/**
 * Reset the game
 */
static void reset_game(void) {
    player.x = SCREEN_W/2.0f;
    player.y = SCREEN_H/2.0f;
    player.aimx = 0.0f;
    player.aimy = -1.0f;
    player.shoot_cooldown = 0;
    player.alive = true;

    for (int i=0;i<MAX_BULLETS;i++) {
        bullets[i].alive = false;
    }
    for (int i=0;i<MAX_ENEMIES;i++) {
        enemies[i].alive = false;
        enemies[i].death_timer = 0;
        enemies[i].fire_cooldown = ENEMY_FIRE_COOLDOWN_FRAMES;
    }

    generate_dots();
    generate_props();

    survival_time = 0.0f;
    enemy_spawn_timer = 0.0f;
    game_over = false;
    game_won = false;
    paused = false;
}

/**
 * Spawn bullet
 */
static void spawn_bullet(float x, float y, float dx, float dy,
                         float speed, bool from_enemy) {
    for (int i=0; i<MAX_BULLETS; i++) {
        if (!bullets[i].alive) {
            normalize(&dx,&dy);
            bullets[i].x = x;
            bullets[i].y = y;
            bullets[i].vx = dx * speed;
            bullets[i].vy = dy * speed;
            bullets[i].alive = true;
            bullets[i].from_enemy = from_enemy;
            return;
        }
    }
}

/**
 * Let player fire
 */
static void try_player_fire(void) {
    if (!player.alive) return;
    if (player.shoot_cooldown > 0) return;

    float dx = player.aimx;
    float dy = player.aimy;
    if (fabsf(dx) < 0.0001f && fabsf(dy) < 0.0001f) {
        dx = 0.0f; dy = -1.0f;
    }

    spawn_bullet(player.x, player.y, dx, dy, BULLET_SPEED, false);
    player.shoot_cooldown = PLAYER_COOLDOWN_FRAMES;
}

/**
 * Let enemy fire
 */
static void enemy_try_fire(Enemy *e) {
    if (!e->alive) return;
    if (e->fire_cooldown > 0) return;
    if (!player.alive) return;

    float dx = player.x - e->x;
    float dy = player.y - e->y;
    float d2p = dist2(player.x, player.y, e->x, e->y);
    if (d2p > 250.0f*250.0f) {
        return;
    }

    spawn_bullet(e->x, e->y, dx, dy, ENEMY_BULLET_SPEED, true);
    e->fire_cooldown = ENEMY_FIRE_COOLDOWN_FRAMES;
}

/**
 * Spawn enemies
 */
static void spawn_enemy(void) {
    int idx = -1;
    for (int i=0;i<MAX_ENEMIES;i++) {
        if (!enemies[i].alive && enemies[i].death_timer==0) {
            idx = i; break;
        }
    }
    if (idx < 0) return;

    int edge = rand()%4;
    float x,y;
    if (edge==0) { // top
        x = frand_range(0, SCREEN_W);
        y = -20;
    } else if (edge==1) { // bottom
        x = frand_range(0, SCREEN_W);
        y = SCREEN_H + 20;
    } else if (edge==2) { // left
        x = -20;
        y = frand_range(0, SCREEN_H);
    } else { // right
        x = SCREEN_W + 20;
        y = frand_range(0, SCREEN_H);
    }

    enemies[idx].x = x;
    enemies[idx].y = y;
    enemies[idx].alive = true;
    enemies[idx].death_timer = 0;
    enemies[idx].fire_cooldown = ENEMY_FIRE_COOLDOWN_FRAMES;
}

/**
 * Player controls
 */
static void control_player(float dt, const Uint8 *keys) {
    if (!player.alive) return;

    // W A S D to move
    float mx = 0.0f;
    float my = 0.0f;
    if (keys[SDL_SCANCODE_W]) my -= 1.0f;
    if (keys[SDL_SCANCODE_S]) my += 1.0f;
    if (keys[SDL_SCANCODE_A]) mx -= 1.0f;
    if (keys[SDL_SCANCODE_D]) mx += 1.0f;

    float mvx = mx;
    float mvy = my;
    normalize(&mvx,&mvy);

    player.x += mvx * PLAYER_SPEED * dt;
    player.y += mvy * PLAYER_SPEED * dt;

    // clamp to map
    if (player.x < 10) player.x = 10;
    if (player.x > SCREEN_W-10) player.x = SCREEN_W-10;
    if (player.y < 10) player.y = 10;
    if (player.y > SCREEN_H-10) player.y = SCREEN_H-10;

    // I J K L to aim and fire
    float ax = 0.0f;
    float ay = 0.0f;
    bool aiming_now = false;

    if (keys[SDL_SCANCODE_I]) { ay -= 1.0f; aiming_now = true; }
    if (keys[SDL_SCANCODE_K]) { ay += 1.0f; aiming_now = true; }
    if (keys[SDL_SCANCODE_J]) { ax -= 1.0f; aiming_now = true; }
    if (keys[SDL_SCANCODE_L]) { ax += 1.0f; aiming_now = true; }

    if (aiming_now) {
        player.aimx = ax;
        player.aimy = ay;
        normalize(&player.aimx, &player.aimy);
        try_player_fire();
    }

    if (player.shoot_cooldown > 0) {
        player.shoot_cooldown--;
    }
}

/**
 * Move bullets
 */
static void move_bullets(float dt) {
    for (int i=0;i<MAX_BULLETS;i++) {
        if (!bullets[i].alive) continue;
        bullets[i].x += bullets[i].vx * dt;
        bullets[i].y += bullets[i].vy * dt;

        if (bullets[i].x < -50 || bullets[i].x > SCREEN_W+50 ||
            bullets[i].y < -50 || bullets[i].y > SCREEN_H+50) {
            bullets[i].alive = false;
        }
    }
}

/**
 * Move enemies
 */
static void move_enemies(float dt) {
    for (int i=0;i<MAX_ENEMIES;i++) {
        Enemy *e = &enemies[i];

        if (e->alive) {
            // chase player
            float dx = player.x - e->x;
            float dy = player.y - e->y;
            normalize(&dx,&dy);

            e->vx = dx * ENEMY_SPEED;
            e->vy = dy * ENEMY_SPEED;

            e->x += e->vx * dt;
            e->y += e->vy * dt;

            // try to shoot
            e->fire_cooldown--;
            if (e->fire_cooldown < 0) e->fire_cooldown = 0;
            enemy_try_fire(e);

            // bayonet melee kill the player
            if (player.alive) {
                float d2p = dist2(player.x, player.y, e->x, e->y);
                if (d2p < 12.0f*12.0f) {
                    player.alive = false;
                }
            }
        } else {
            if (e->death_timer > 0) {
                e->death_timer--;
            }
        }
    }
}

/**
 * Environment interactions
 */
static void handle_props_effects(void) {
    // bullets vs props
    for (int b=0; b<MAX_BULLETS; b++) {
        if (!bullets[b].alive) continue;

        for (int p=0; p<prop_count; p++) {
            if (!props[p].alive) continue;

            float px = props[p].x;
            float py = props[p].y;
            float bx = bullets[b].x;
            float by = bullets[b].y;

            switch (props[p].kind) {
                case PROP_TREE:
                    // trees get destroyed by any bullet
                    // tree radius ~12, bullet ~2
                    if (circle_hit(px, py, 12.0f, bx, by, 2.0f)) {
                        props[p].alive = false;
                        bullets[b].alive = false;
                        break;
                    }
                    break;

                case PROP_ROCK:
                    // rock absorbs bullet, radius ~10
                    if (circle_hit(px, py, 10.0f, bx, by, 2.0f)) {
                        bullets[b].alive = false;
                        break;
                    }
                    break;

                case PROP_WIRE:
                    // wire doesn't block bullets
                    break;
            }
        }
    }

    // player vs wire
    if (player.alive) {
        for (int p=0; p<prop_count; p++) {
            if (!props[p].alive) continue;
            if (props[p].kind != PROP_WIRE) continue;

            // wire radius ~10, player radius ~10
            if (circle_hit(props[p].x, props[p].y, 10.0f, player.x, player.y, 10.0f)) {
                player.alive = false;
                break;
            }
        }
    }

    // enemies vs wire
    for (int e=0; e<MAX_ENEMIES; e++) {
        Enemy *en = &enemies[e];
        if (!en->alive) continue;

        for (int p=0; p<prop_count; p++) {
            if (!props[p].alive) continue;
            if (props[p].kind != PROP_WIRE) continue;

            // enemy radius ~10
            if (circle_hit(props[p].x, props[p].y, 10.0f, // was 14
                           en->x,       en->y,       10.0f)) { // was 12
                en->alive = false;
                en->death_timer = 15;
                break;
            }
        }
    }
}

/**
 * Bullets hitting actors
 */
static void handle_bullet_actor_collisions(void) {
    for (int b=0; b<MAX_BULLETS; b++) {
        if (!bullets[b].alive) continue;

        if (!bullets[b].from_enemy) {
            // player bullet vs enemy
            for (int e=0; e<MAX_ENEMIES; e++) {
                Enemy *en = &enemies[e];
                if (!en->alive) continue;

                if (circle_hit(bullets[b].x, bullets[b].y, 2.0f, en->x, en->y, 12.0f)) {
                    en->alive = false;
                    en->death_timer = 15;
                    bullets[b].alive = false;
                    break;
                }
            }
        } else {
            // enemy bullet vs player
            if (player.alive) {
                if (circle_hit(bullets[b].x, bullets[b].y, 2.0f, player.x, player.y, 10.0f)) {
                    player.alive = false;
                    bullets[b].alive = false;
                }
            }
        }
    }
}

/**
 * Draws a rectangle at exact start and end coordinates
 */
static void draw_rect(SDL_Renderer *ren, int x, int y, int w, int h) {
    SDL_Rect r = { x, y, w, h };
    SDL_RenderFillRect(ren, &r);
}

/**
 * Draws a rectangle using starting coordinates and scale
 */
static void draw_rect_scaled(SDL_Renderer *ren, int x, int y, int s) {
    SDL_Rect r = { x, y, s, s };
    SDL_RenderFillRect(ren, &r);
}

/**
 * Draws a circle
 */
static void draw_filled_circle(SDL_Renderer *ren, int cx, int cy, int r) {
    for (int dy=-r; dy<=r; dy++) {
        for (int dx=-r; dx<=r; dx++) {
            if (dx*dx + dy*dy <= r*r) {
                SDL_RenderDrawPoint(ren, cx+dx, cy+dy);
            }
        }
    }
}

/**
 * Draws a soldier
 */
static void draw_soldier(SDL_Renderer *ren, int x, int y, bool player_flag) {
    // outline
    SDL_SetRenderDrawColor(ren, 20, 15, 10, 255);
    draw_rect(ren, x-9, y-9, 18,18);

    // coat / body
    if (player_flag) {
    	SDL_SetRenderDrawColor(ren, 110, 90, 50, 255);
    } else {
    	SDL_SetRenderDrawColor(ren, 80, 100, 80, 255);
    }
    draw_rect(ren, x-8, y-8, 16,14);

    // boots / lower
    if (player_flag) {
    	SDL_SetRenderDrawColor(ren, 70, 50, 30, 255);
    } else {
    	SDL_SetRenderDrawColor(ren, 40, 50, 40, 255);
    }
    draw_rect(ren, x-8, y+2, 16,4);

    // helmet
    if (player_flag) {
    	SDL_SetRenderDrawColor(ren, 90, 70, 40, 255);
    } else {
    	SDL_SetRenderDrawColor(ren, 60, 80, 60, 255);
    }
    draw_rect(ren, x-7, y-12, 14,5);

    // helmet rim highlight
    if (player_flag) {
    	SDL_SetRenderDrawColor(ren, 200, 180, 120, 255);
    } else {
    	SDL_SetRenderDrawColor(ren, 140, 170, 140, 255);
    }
    draw_rect(ren, x-7, y-12, 14,2);
}

/**
 * Minimal pixel font
 */
static const char *glyph_for_char(char c) {
    switch (c) {
    	case ' ': return "....""....""....""....""....""....";
        case '.': return "....""....""....""....""..#.""..#.";
        case ':': return "....""..#.""..#.""....""..#.""..#.";
        case '-': return "....""....""####""....""....""....";
        case '!': return ".##."".##."".##."".##.""...."".##.";
        case '0': return "####""#..#""#..#""#..#""#..#""####";
        case '1': return "..#.""..#.""..#.""..#.""..#.""..#.";
        case '2': return "####""...#""...#""####""#...""####";
        case '3': return "####""...#""...#""####""...#""####";
        case '4': return "#..#""#..#""#..#""####""...#""...#";
        case '5': return "####""#...""#...""####""...#""####";
        case '6': return "####""#...""#...""####""#..#""####";
        case '7': return "####""...#""...#""..#.""..#.""..#.";
        case '8': return "####""#..#""#..#""####""#..#""####";
        case '9': return "####""#..#""#..#""####""...#""####";
        case 'A': return ".##.""#..#""#..#""####""#..#""#..#";
        case 'C': return ".###""#...""#...""#...""#..."".###";
        case 'D': return "###.""#..#""#..#""#..#""#..#""###.";
        case 'E': return "####""#...""#...""###.""#...""####";
        case 'R': return "###.""#..#""#..#""###.""#.#.""#..#";
        case 'V': return "#..#""#..#""#..#""#..#"".#.#""..#.";
        case 'P': return "###.""#..#""#..#""###.""#...""#...";
        case 'S': return ".###""#...""#..."".##.""...#""###.";
        case 'T': return "####"" .#."" .#."" .#."" .#."" .#.";
        case 'O': return ".##.""#..#""#..#""#..#""#..#"".##.";
        case 'M': return "####""####""##.#""#..#""#..#""#..#";
        case 'U': return "#..#""#..#""#..#""#..#""#..#"".##.";
        case 'H': return "#..#""#..#""####""#..#""#..#""#..#";
        case 'L': return "#...""#...""#...""#...""#...""####";
        case 'I': return "###."".#.."".#.."".#.."".#..""###.";
        case 'Y': return "#..#""#..#"".#.#""..#.""..#.""..#.";
        default:  return NULL;
    }
}

/**
 * Draw text
 */
static void draw_text(SDL_Renderer *ren, int x, int y, const char *msg,
                      SDL_Color color) {
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
    int cursor_x = x;
    for (const char *p = msg; *p; p++) {
        const char *g = glyph_for_char(*p);
        if (!g) {
            cursor_x += 10;
            continue;
        }
        for (int gy=0; gy<6; gy++) {
            for (int gx=0; gx<4; gx++) {
                char pixel = g[gy*4 + gx];
                if (pixel == '#') {
                    draw_rect_scaled(ren, cursor_x + gx*2, y + gy*2, 2);
                }
            }
        }
        cursor_x += 10;
    }
}

/**
 * Draw tree
 */
static void draw_tree(SDL_Renderer *ren, int x, int y) {
    SDL_SetRenderDrawColor(ren, 40, 25, 15, 255); // stump
    draw_rect(ren, x-2, y-8, 4,16);
    SDL_SetRenderDrawColor(ren, 50, 70, 40, 255); // canopy
    draw_rect(ren, x-6, y-14, 12,8);
    SDL_SetRenderDrawColor(ren, 100, 130, 80, 255); // highlight
    draw_rect(ren, x-4, y-14, 4,4);
}

/**
 * Draw rock
 */
static void draw_rock(SDL_Renderer *ren, int x, int y) {
    SDL_SetRenderDrawColor(ren, 80, 80, 80, 255); // dark base
    draw_rect(ren, x-6, y-4, 12,8);
    SDL_SetRenderDrawColor(ren, 140, 140, 140, 255); // highlight
    draw_rect(ren, x-2, y-4, 4,3);
}

/**
 * Draw barbed wire
 */
static void draw_wire(SDL_Renderer *ren, int x, int y) {
    SDL_SetRenderDrawColor(ren, 150,150,150,255);
    draw_rect(ren, x-10, y-1, 20,2);   // strand
    draw_rect(ren, x-6,  y-5, 2,8);    // barb
    draw_rect(ren, x+2,  y-5, 2,8);    // barb
}

/**
 * Draw all props at their randomized locations
 */
static void draw_props(SDL_Renderer *ren) {
    for (int i=0; i<prop_count; i++) {
        if (!props[i].alive) continue;
        int x = (int)props[i].x;
        int y = (int)props[i].y;
        switch (props[i].kind) {
            case PROP_TREE: draw_tree(ren, x, y); break;
            case PROP_ROCK: draw_rock(ren, x, y); break;
            case PROP_WIRE: draw_wire(ren, x, y); break;
        }
    }
}

/**
 * Draw the background dots
 */
static void draw_dots(SDL_Renderer *ren) {
    for (int i=0; i<dot_count; i++) {
        SDL_SetRenderDrawColor(ren, dots[i].r, dots[i].g, dots[i].b, 255);
        // draw a 1-2 pixel speckle
        // tiny jitter to avoid perfect squares
        draw_rect(ren, dots[i].x, dots[i].y, 2, 2);
    }
}

/**
 * Render graphics
 */
static void render(SDL_Renderer *ren) {
    SDL_SetRenderDrawColor(ren, 45, 35, 25, 255); // base muddy ground
    SDL_RenderClear(ren);
    draw_dots(ren);
    draw_props(ren);

    // draw enemy blood
    for (int i=0;i<MAX_ENEMIES;i++) {
        Enemy *e = &enemies[i];
        if (!e->alive && e->death_timer > 0) {
            SDL_SetRenderDrawColor(ren, 140, 0, 0, 255);
            draw_filled_circle(ren, (int)e->x, (int)e->y, 10);
        }
    }

    // draw enemies alive
    for (int i=0;i<MAX_ENEMIES;i++) {
        Enemy *e = &enemies[i];
        if (e->alive) {
            draw_soldier(ren, (int)e->x, (int)e->y, false);
        }
    }

    // draw bullets
    for (int i=0;i<MAX_BULLETS;i++) {
        if (!bullets[i].alive) continue;
        if (bullets[i].from_enemy) {
            SDL_SetRenderDrawColor(ren, 200, 60, 40, 255); // enemy tracer
        } else {
            SDL_SetRenderDrawColor(ren, 240, 220, 80, 255); // player tracer
        }
        draw_rect(ren, (int)bullets[i].x-2, (int)bullets[i].y-2, 4,4);
    }

    // draw player
    if (player.alive) {
        draw_soldier(ren, (int)player.x, (int)player.y, true);

        // rifle direction marker
        float dx = player.aimx;
        float dy = player.aimy;
        if (fabsf(dx) < 0.001f && fabsf(dy) < 0.001f) {
            dx = 0.0f; dy = -1.0f;
        }
        normalize(&dx,&dy);
        int gunx = (int)(player.x + dx*12.0f);
        int guny = (int)(player.y + dy*12.0f);

        SDL_SetRenderDrawColor(ren, 90, 56 ,34, 255); // outer 'woody' color
        draw_rect(ren, gunx-3, guny-3, 6,6);

        SDL_SetRenderDrawColor(ren, 140, 140, 140, 255); // inner 'metal' color
        draw_rect(ren, gunx-2, guny-2, 4,4);
    } else {
        if (game_won){
        	SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        	draw_rect(ren, 0, 0, SCREEN_W, SCREEN_H);
        }else{
        	SDL_SetRenderDrawColor(ren, 180, 0, 0, 255);
        	draw_filled_circle(ren, (int)player.x, (int)player.y, 14);
        }
    }

    SDL_Color fontcol = {255, 220, 180, 255};

    // timer
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "TIME %.1f", survival_time);
        draw_text(ren, 10, 10, buf, fontcol);
    }
    
    // pause
    if (paused) {
        SDL_Color pause_color = {255, 255, 255, 255};
        draw_text(ren, SCREEN_W/2 - 40, SCREEN_H/2 - 10, "PAUSED", fontcol);
    }

    // game over message
    if (game_over) {
        if (game_won) {
            draw_text(ren, SCREEN_W/2 - 90, SCREEN_H/2 - 50, "YOU SURVIVED!", fontcol);
        } else {
            draw_text(ren, SCREEN_W/2 - 50, SCREEN_H/2 - 50, "DEAD", fontcol);
        }
        draw_text(ren, SCREEN_W/2 - 80, SCREEN_H/2 - 20, "PRESS SPACE", fontcol);
        draw_text(ren, SCREEN_W/2 - 75, SCREEN_H/2 + 10, "TO RESTART", fontcol);
	}

    SDL_RenderPresent(ren);
}

static void update_game(void *arg) {
    SDL_Renderer *ren = (SDL_Renderer *)arg;

    // start counting how long it takes to render 1 frame
    uint64_t frame_start = SDL_GetPerformanceCounter();

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) running = false;
        if (ev.type == SDL_KEYDOWN) {
            if (ev.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
            else if (ev.key.keysym.sym == SDLK_SPACE) {
                if (!player.alive) {
                    // when game over, SPACE restarts
                    reset_game();
                } else {
                    // when alive, SPACE pauses
                    paused = !paused;
                }
            }
        }
    }

    const Uint8 *keys = SDL_GetKeyboardState(NULL);

    // timing
    uint64_t now = SDL_GetPerformanceCounter();
    double dt = (now - prev) / freq;
    prev = now;

    // delta frame time clamp
    // if a frame for some reason takes >200ms, set to 200ms to prevent
    // frame time explosions and unstable physics causing glitches,
    // overshooting, teleporting, missed collisions, etc
    if (dt > 0.05) dt = 0.05;

    if (player.alive && !paused) {
        control_player((float)dt, keys);
        move_bullets((float)dt);
        move_enemies((float)dt);
        handle_props_effects();
        handle_bullet_actor_collisions();

        // spawn enemies every 1.0 second
        enemy_spawn_timer -= (float)dt;
        if (enemy_spawn_timer <= 0.0f) {
            spawn_enemy();
            enemy_spawn_timer = 1.0f;
        }

        // survival timer
        survival_time += (float)dt;
        if (!game_won && survival_time >= WIN_TIME) {
            survival_time = WIN_TIME; // clamp
            game_won = true;
            player.alive = false; // end the round
       }
    } else if (!player.alive) {
        game_over = true;
    }

    render(ren);

    // add delay to limit frames to exactly 60 fps (or less..)
    #ifndef __EMSCRIPTEN__
    uint64_t frame_end = SDL_GetPerformanceCounter();
    uint64_t frame_ticks = frame_end - frame_start;
    uint64_t target_ticks = SDL_GetPerformanceFrequency() / 60;

    if (frame_ticks < target_ticks) {
        uint32_t delay_ms = (uint32_t)((target_ticks - frame_ticks) * 1000 / SDL_GetPerformanceFrequency());
        SDL_Delay(delay_ms);
    }
    #endif
}

/**
 * Main game loop
 */
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    srand((unsigned int)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "The Lost Tommy - survive for one minute to win",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H,
        SDL_WINDOW_SHOWN
    );
    if (!win) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(
        win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!ren) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    reset_game();

    prev = SDL_GetPerformanceCounter();
    freq = (double)SDL_GetPerformanceFrequency();



    #ifdef __EMSCRIPTEN__
        emscripten_set_main_loop_arg(update_game, ren, 0, 1);
    #else
        while (running) {
            update_game(ren);
        }
    #endif


    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
