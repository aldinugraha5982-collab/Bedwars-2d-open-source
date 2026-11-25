#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
constexpr int kTileSize = 40;
constexpr int kMapWidth = 48;
constexpr int kMapHeight = 30;
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;
constexpr double kMatchLength = 1200.0;
struct Vec2 {
    double x = 0;
    double y = 0;
};

struct Color {
    Uint8 r, g, b, a = 255;
};

constexpr std::array<Color, 4> kTeamColor{{
    {59, 130, 246}, {239, 68, 68}, {34, 197, 94}, {234, 179, 8}
}};
constexpr std::array<Color, 4> kTeamDark{{
    {29, 78, 216}, {185, 28, 28}, {21, 128, 61}, {161, 98, 7}
}};
constexpr std::array<const char*, 4> kTeamName{{"BIRU", "MERAH", "HIJAU", "KUNING"}};
constexpr std::array<const char*, 6> kSlotName{{
    "WOOL", "PAPAN", "TNT", "FIREBALL", "APPLE", "EGG"
}};

enum class Tile : uint8_t { Void, Ground, Block, Bed, Wall };
enum class Resource : int { Iron, Gold, Diamond };

struct Cell {
    Tile tile = Tile::Void;
    int team = -1;
    int hp = 0;
    bool plank = false;
};

struct Inventory {
    int iron = 0, gold = 0, diamond = 0;
    int blocks = 20, planks = 0, tnt = 0, fire = 0, apple = 0, egg = 0;
};

struct Fighter {
    std::string name;
    int team = 0;
    Vec2 pos, spawn, face{1, 0}, knockback;
    Inventory inv;
    double hp = 20, attackCooldown = 0, invulnerable = 0;
    double deathTimer = 0, thinkTimer = 0, regenTimer = 0;
    int sword = 0, armor = 0, kills = 0, beds = 0;
    bool bot = false, dead = false, out = false;
};

struct Pickup {
    Vec2 pos;
    Resource type = Resource::Iron;
    double timer = 0;
};

struct Generator {
    Vec2 pos;
    Resource type = Resource::Iron;
    int team = -1;
    double interval = 1, elapsed = 0;
};

struct Fuse {
    Vec2 pos;
    double timer = 2.6;
    int team = 0;
};

struct Projectile {
    Vec2 pos, velocity;
    double remaining = 0;
    int team = 0;
    bool bridgeEgg = false;
};

struct TeamUpgrade {
    int forge = 0, sharp = 0, protection = 0;
};

SDL_Window* gWindow = nullptr;
SDL_Renderer* gRenderer = nullptr;
std::array<Cell, kMapWidth * kMapHeight> gMap{};
std::array<bool, 4> gBedAlive{};
std::array<Vec2, 4> gBedPosition{};
std::array<TeamUpgrade, 4> gUpgrades{};
std::vector<Fighter> gFighters;
std::vector<Pickup> gPickups;
std::vector<Generator> gGenerators;
std::vector<Fuse> gFuses;
std::vector<Projectile> gProjectiles;
Fighter* gPlayer = nullptr;
double gCameraX = 0, gCameraY = 0, gTime = 0;
bool gRunning = true, gPlaying = false, gShopOpen = false, gGameOver = false;
int gWinner = -2, gSelectedSlot = 0;
bool gAutoBridge = true;
std::string gNotice = "Hancurkan bed lawan untuk mencegah mereka respawn!";
double gNoticeTimer = 5;

int cellIndex(int x, int y) {
    if (x < 0 || y < 0 || x >= kMapWidth || y >= kMapHeight) return -1;
    return y * kMapWidth + x;
}

Cell* cellAt(int x, int y) {
    const int index = cellIndex(x, y);
    return index < 0 ? nullptr : &gMap[index];
}

Cell* cellAt(Vec2 p) {
    return cellAt(static_cast<int>(std::floor(p.x / kTileSize)),
                  static_cast<int>(std::floor(p.y / kTileSize)));
}

bool walkable(int x, int y) {
    const Cell* c = cellAt(x, y);
    return c && (c->tile == Tile::Ground || c->tile == Tile::Block);
}

Vec2 centerOf(int x, int y) {
    return {x * kTileSize + kTileSize / 2.0, y * kTileSize + kTileSize / 2.0};
}

double length(Vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

Vec2 normalized(Vec2 v) {
    const double n = length(v);
    return n > 0.0001 ? Vec2{v.x / n, v.y / n} : Vec2{1, 0};
}

void setColor(Color c) {
    SDL_SetRenderDrawColor(gRenderer, c.r, c.g, c.b, c.a);
}

void fillRect(int x, int y, int w, int h, Color c) {
    setColor(c);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(gRenderer, &rect);
}

void outlineRect(int x, int y, int w, int h, Color c) {
    setColor(c);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderDrawRect(gRenderer, &rect);
}

void line(int x1, int y1, int x2, int y2, Color c) {
    setColor(c);
    SDL_RenderDrawLine(gRenderer, x1, y1, x2, y2);
}

void circle(int cx, int cy, int radius, Color c) {
    setColor(c);
    for (int dy = -radius; dy <= radius; ++dy) {
        const int dx = static_cast<int>(std::sqrt(radius * radius - dy * dy));
        SDL_RenderDrawLine(gRenderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

const std::unordered_map<char, std::array<uint8_t, 7>>& font() {
    static const std::unordered_map<char, std::array<uint8_t, 7>> glyphs{
        {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
        {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
        {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
        {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
        {'I',{14,4,4,4,4,4,14}}, {'J',{7,2,2,2,18,18,12}},
        {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
        {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
        {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
        {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
        {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
        {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
        {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
        {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
        {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
        {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
        {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
        {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
        {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
        {':',{0,4,4,0,4,4,0}}, {'/',{1,2,2,4,8,8,16}},
        {'-',{0,0,0,31,0,0,0}}, {'+',{0,4,4,31,4,4,0}},
        {'.',{0,0,0,0,0,12,12}}, {'!',{4,4,4,4,4,0,4}},
        {'?',{14,17,1,2,4,0,4}}, {' ',{0,0,0,0,0,0,0}}
    };
    return glyphs;
}

void text(const std::string& value, int x, int y, int scale, Color color) {
    int cursor = x;
    setColor(color);
    for (char raw : value) {
        const char ch = raw >= 'a' && raw <= 'z' ? static_cast<char>(raw - 'a' + 'A') : raw;
        const auto it = font().find(ch);
        const auto& glyph = it == font().end() ? font().at('?') : it->second;
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((glyph[row] & (1u << (4 - col))) == 0) continue;
                SDL_Rect pixel{cursor + col * scale, y + row * scale, scale, scale};
                SDL_RenderFillRect(gRenderer, &pixel);
            }
        }
        cursor += 6 * scale;
    }
}

void showNotice(const std::string& message) {
    gNotice = message;
    gNoticeTimer = 3.0;
}

void addIsland(int x1, int y1, int x2, int y2) {
    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            if (Cell* c = cellAt(x, y)) c->tile = Tile::Ground;
}

Fighter makeFighter(const std::string& name, int team, int x, int y, bool bot) {
    Fighter f;
    f.name = name;
    f.team = team;
    f.pos = f.spawn = centerOf(x, y);
    f.bot = bot;
    return f;
}

void addGenerator(int x, int y, Resource type, int team, double interval) {
    gGenerators.push_back({centerOf(x, y), type, team, interval, 0});
}

void startMatch() {
    gMap.fill({});
    gBedAlive.fill(true);
    gUpgrades.fill({});
    gFighters.clear();
    gPickups.clear();
    gGenerators.clear();
    gFuses.clear();
    gProjectiles.clear();
    gTime = 0;
    gWinner = -2;
    gSelectedSlot = 0;
    gAutoBridge = true;
    gShopOpen = false;
    gGameOver = false;

    addIsland(6, 12, 11, 17);
    addIsland(37, 12, 42, 17);
    addIsland(21, 2, 26, 7);
    addIsland(21, 22, 26, 27);
    addIsland(21, 12, 26, 17);
    addIsland(14, 13, 17, 16);
    addIsland(30, 13, 33, 16);
    addIsland(23, 8, 24, 10);
    addIsland(23, 19, 24, 21);

    const std::array<std::array<int, 2>, 4> beds{{
        {{7, 14}}, {{41, 14}}, {{23, 4}}, {{23, 25}}
    }};
    const std::array<std::array<int, 2>, 4> spawns{{
        {{9, 15}}, {{39, 15}}, {{24, 6}}, {{24, 23}}
    }};
    for (int team = 0; team < 4; ++team) {
        const int bx = beds[team][0], by = beds[team][1];
        gBedPosition[team] = centerOf(bx, by);
        Cell* bed = cellAt(bx, by);
        bed->tile = Tile::Bed;
        bed->team = team;
        bed->hp = 14;
        for (const auto& d : std::array<std::array<int, 2>, 4>{{{{1,0}},{{-1,0}},{{0,1}},{{0,-1}}}}) {
            Cell* wall = cellAt(bx + d[0], by + d[1]);
            if (wall) *wall = {Tile::Wall, team, 10, false};
        }
        gFighters.push_back(makeFighter(team == 0 ? "KAMU" : kTeamName[team], team,
                                        spawns[team][0], spawns[team][1], team != 0));
    }
    gPlayer = &gFighters[0];

    for (int team = 0; team < 4; ++team) {
        const int gx = team == 0 ? 9 : team == 1 ? 40 : 24;
        const int gy = team == 0 ? 15 : team == 1 ? 15 : team == 2 ? 6 : 23;
        addGenerator(gx, gy, Resource::Iron, team, 1.1);
        addGenerator(gx, gy, Resource::Gold, team, 7.0);
    }
    addGenerator(23, 14, Resource::Diamond, -1, 16.0);
    addGenerator(24, 15, Resource::Diamond, -1, 19.0);
    showNotice("WASD GERAK  SPACE SERANG  E GUNAKAN  B TOKO");
    gPlaying = true;
}

int& resource(Fighter& f, Resource type) {
    if (type == Resource::Iron) return f.inv.iron;
    if (type == Resource::Gold) return f.inv.gold;
    return f.inv.diamond;
}

bool placeBlock(Fighter& f, int x, int y, bool plank) {
    Cell* target = cellAt(x, y);
    if (!target || target->tile != Tile::Void) return false;
    bool adjacent = false;
    for (const auto& d : std::array<std::array<int, 2>, 4>{{{{1,0}},{{-1,0}},{{0,1}},{{0,-1}}}})
        adjacent = adjacent || walkable(x + d[0], y + d[1]);
    if (!adjacent) return false;
    const Vec2 p = centerOf(x, y);
    if (length({p.x - f.pos.x, p.y - f.pos.y}) > 4 * kTileSize) return false;
    int& count = plank ? f.inv.planks : f.inv.blocks;
    if (count <= 0) return false;
    --count;
    *target = {Tile::Block, f.team, plank ? 4 : 2, plank};
    return true;
}

void breakCell(Cell& c, Fighter& attacker) {
    if (c.tile == Tile::Bed) {
        const int team = c.team;
        gBedAlive[team] = false;
        if (attacker.team != team) ++attacker.beds;
        showNotice(std::string("BED ") + kTeamName[team] + " HANCUR!");
        c = {Tile::Ground, -1, 0, false};
        return;
    }
    if (c.tile == Tile::Wall) {
        c = {Tile::Ground, -1, 0, false};
        return;
    }
    if (c.tile == Tile::Block) c = {};
}

void damage(Fighter& target, Fighter& attacker) {
    if (target.dead || target.out || target.invulnerable > 0) return;
    const double armor = std::min(0.85, target.armor * 0.2 +
                                           gUpgrades[target.team].protection * 0.08);
    const double power = std::array<double, 4>{{4, 5, 6, 7}}[attacker.sword] *
                         (1.0 + gUpgrades[attacker.team].sharp * 0.15);
    target.hp -= power * (1.0 - armor);
    target.invulnerable = 0.35;
    const Vec2 direction = normalized({target.pos.x - attacker.pos.x,
                                       target.pos.y - attacker.pos.y});
    target.knockback.x += direction.x * 240;
    target.knockback.y += direction.y * 240;
    if (target.hp <= 0) {
        target.dead = true;
        target.deathTimer = gBedAlive[target.team] ? 4.0 : -1;
        if (target.deathTimer < 0) target.out = true;
        if (target.team != attacker.team) ++attacker.kills;
        showNotice(target.name + " DIKALAHKAN");
    }
}

void attack(Fighter& f) {
    if (f.dead || f.out || f.attackCooldown > 0) return;
    f.attackCooldown = f.bot ? 0.8 : 0.42;
    Fighter* victim = nullptr;
    double nearest = 58;
    for (Fighter& other : gFighters) {
        if (&other == &f || other.team == f.team || other.dead || other.out) continue;
        const Vec2 delta{other.pos.x - f.pos.x, other.pos.y - f.pos.y};
        const double distance = length(delta);
        if (distance > nearest) continue;
        const double facing = (delta.x * f.face.x + delta.y * f.face.y) / (distance + 0.001);
        if (distance > 24 && facing < 0.2) continue;
        victim = &other;
        nearest = distance;
    }
    if (victim) {
        damage(*victim, f);
        return;
    }
    for (double distance : {24.0, 40.0, 56.0}) {
        const int x = static_cast<int>((f.pos.x + f.face.x * distance) / kTileSize);
        const int y = static_cast<int>((f.pos.y + f.face.y * distance) / kTileSize);
        Cell* c = cellAt(x, y);
        if (!c || c->tile == Tile::Void || c->tile == Tile::Ground) continue;
        if (c->tile == Tile::Block && c->team == f.team) continue;
        c->hp -= std::array<int, 4>{{4, 5, 6, 7}}[f.sword];
        if (c->hp <= 0) breakCell(*c, f);
        return;
    }
}

void buyItem(Fighter& f, int id) {
    if (id == 0 && f.inv.iron >= 4) {
        f.inv.iron -= 4; f.inv.blocks += 16;
    } else if (id == 1 && f.inv.gold >= 4) {
        f.inv.gold -= 4; f.inv.planks += 12;
    } else if (id == 2 && f.inv.iron >= 10 && f.sword < 1) {
        f.inv.iron -= 10; f.sword = 1;
    } else if (id == 3 && f.inv.gold >= 7 && f.sword < 2) {
        f.inv.gold -= 7; f.sword = 2;
    } else if (id == 4 && f.inv.diamond >= 4 && f.sword < 3) {
        f.inv.diamond -= 4; f.sword = 3;
    } else if (id == 5 && f.inv.gold >= 3) {
        f.inv.gold -= 3; ++f.inv.apple;
    } else if (id == 6 && f.inv.gold >= 4) {
        f.inv.gold -= 4; ++f.inv.tnt;
    } else if (id == 7 && f.inv.iron >= 24) {
        f.inv.iron -= 24; ++f.inv.fire;
    } else if (id == 8 && f.inv.diamond >= 2) {
        f.inv.diamond -= 2; ++f.inv.egg;
    } else {
        showNotice("RESOURCE KURANG ATAU ITEM SUDAH MAKSIMAL");
        return;
    }
    showNotice("ITEM BERHASIL DIBELI");
}

void buyUpgrade(Fighter& f, int id) {
    TeamUpgrade& u = gUpgrades[f.team];
    if (id == 0 && u.forge < 3 && f.inv.diamond >= 2 + u.forge * 2) {
        f.inv.diamond -= 2 + u.forge * 2; ++u.forge;
    } else if (id == 1 && u.sharp == 0 && f.inv.diamond >= 4) {
        f.inv.diamond -= 4; ++u.sharp;
    } else if (id == 2 && u.protection < 3 && f.inv.diamond >= 2 + u.protection * 2) {
        f.inv.diamond -= 2 + u.protection * 2; ++u.protection;
    } else {
        showNotice("BUTUH DIAMOND UNTUK UPGRADE");
        return;
    }
    showNotice("UPGRADE TIM AKTIF");
}

void useItem(Fighter& f, int targetX, int targetY) {
    if (f.dead || f.out) return;
    const int cx = static_cast<int>(f.pos.x / kTileSize);
    const int cy = static_cast<int>(f.pos.y / kTileSize);
    if (gSelectedSlot == 0 || gSelectedSlot == 1) {
        int tx = targetX, ty = targetY;
        if (length({(tx + 0.5) * kTileSize - f.pos.x,
                    (ty + 0.5) * kTileSize - f.pos.y}) > 4 * kTileSize) {
            tx = cx + (std::abs(f.face.x) > std::abs(f.face.y) ?
                       (f.face.x > 0 ? 1 : -1) : 0);
            ty = cy + (std::abs(f.face.y) >= std::abs(f.face.x) ?
                       (f.face.y > 0 ? 1 : -1) : 0);
        }
        placeBlock(f, tx, ty, gSelectedSlot == 1);
    } else if (gSelectedSlot == 2 && f.inv.tnt > 0) {
        --f.inv.tnt;
        gFuses.push_back({centerOf(cx, cy), 2.6, f.team});
    } else if (gSelectedSlot == 3 && f.inv.fire > 0) {
        --f.inv.fire;
        const Vec2 direction = normalized({(targetX + 0.5) * kTileSize - f.pos.x,
                                           (targetY + 0.5) * kTileSize - f.pos.y});
        gProjectiles.push_back({f.pos, {direction.x * 430, direction.y * 430}, 420, f.team});
    } else if (gSelectedSlot == 4 && f.inv.apple > 0 && f.hp < 20) {
        --f.inv.apple;
        f.hp = std::min(20.0, f.hp + 8);
    } else if (gSelectedSlot == 5 && f.inv.egg > 0) {
        --f.inv.egg;
        const Vec2 direction = normalized({(targetX + 0.5) * kTileSize - f.pos.x,
                                           (targetY + 0.5) * kTileSize - f.pos.y});
        gProjectiles.push_back({f.pos, {direction.x * 520, direction.y * 520}, 360, f.team, true});
    }
}

void respawn(Fighter& f) {
    if (!gBedAlive[f.team]) {
        f.out = true;
        return;
    }
    f.dead = false;
    f.hp = 20;
    f.pos = f.spawn;
    f.invulnerable = 2;
    f.inv.blocks = std::max(f.inv.blocks, 12);
}

int livingTeams() {
    int count = 0;
    for (int t = 0; t < 4; ++t) {
        if (std::any_of(gFighters.begin(), gFighters.end(), [t](const Fighter& f) {
            return f.team == t && !f.out;
        })) ++count;
    }
    return count;
}

void checkWinner() {
    if (livingTeams() > 1) return;
    gGameOver = true;
    gWinner = -1;
    for (const Fighter& f : gFighters) if (!f.out) gWinner = f.team;
    gShopOpen = false;
}

Vec2 botDirection(Fighter& bot) {
    Fighter* enemy = nullptr;
    double closest = 1e9;
    for (Fighter& other : gFighters) {
        if (other.team == bot.team || other.dead || other.out) continue;
        const double d = length({other.pos.x - bot.pos.x, other.pos.y - bot.pos.y});
        if (d < closest) { closest = d; enemy = &other; }
    }
    if (enemy && closest < 52) {
        bot.face = normalized({enemy->pos.x - bot.pos.x, enemy->pos.y - bot.pos.y});
        attack(bot);
        return {};
    }

    int targetTeam = -1;
    double targetDistance = 1e9;
    for (int team = 0; team < 4; ++team) {
        if (team == bot.team || !gBedAlive[team]) continue;
        const double d = length({gBedPosition[team].x - bot.pos.x,
                                 gBedPosition[team].y - bot.pos.y});
        if (d < targetDistance) { targetDistance = d; targetTeam = team; }
    }
    if (enemy && closest < 120) {
        bot.face = normalized({enemy->pos.x - bot.pos.x, enemy->pos.y - bot.pos.y});
        if (walkable(static_cast<int>((bot.pos.x + bot.face.x * 28) / kTileSize),
                     static_cast<int>((bot.pos.y + bot.face.y * 28) / kTileSize)))
            return bot.face;
    }
    if (targetTeam < 0) return {};
    const Vec2 target = gBedPosition[targetTeam];
    const double dx = target.x - bot.pos.x, dy = target.y - bot.pos.y;
    Vec2 direction = std::abs(dx) > std::abs(dy) ? Vec2{dx > 0 ? 1.0 : -1.0, 0}
                                                   : Vec2{0, dy > 0 ? 1.0 : -1.0};
    bot.face = direction;
    const int cx = static_cast<int>(bot.pos.x / kTileSize);
    const int cy = static_cast<int>(bot.pos.y / kTileSize);
    const int nx = cx + static_cast<int>(direction.x);
    const int ny = cy + static_cast<int>(direction.y);
    Cell* next = cellAt(nx, ny);
    if (!next) return {};
    if (next->tile == Tile::Void) {
        if (bot.inv.blocks > 0 && placeBlock(bot, nx, ny, false)) return {};
        return direction;
    }
    if (next->tile == Tile::Bed || next->tile == Tile::Wall ||
        (next->tile == Tile::Block && next->team != bot.team)) {
        if (std::abs(dx) < 70 && std::abs(dy) < 70) attack(bot);
        return {};
    }
    return direction;
}

void updateFighter(Fighter& f, double dt, Vec2 input) {
    f.attackCooldown = std::max(0.0, f.attackCooldown - dt);
    f.invulnerable = std::max(0.0, f.invulnerable - dt);
    if (f.dead) {
        if (f.deathTimer > 0 && (f.deathTimer -= dt) <= 0) respawn(f);
        return;
    }
    if (f.out) return;

    if (f.bot) {
        f.thinkTimer -= dt;
        input = botDirection(f);
        if (f.thinkTimer <= 0) {
            f.thinkTimer = 0.7;
            if (f.inv.iron < 8) { f.inv.iron += 5; }
            if (f.sword < 3 && f.inv.iron >= 10) { f.inv.iron -= 10; ++f.sword; }
            if (f.inv.blocks < 5) f.inv.blocks += 8;
        }
    }

    const double speed = f.bot ? 125 : 175;
    const double magnitude = length(input);
    if (magnitude > 1) { input.x /= magnitude; input.y /= magnitude; }
    if (length(input) > 0.01) f.face = normalized(input);
    const double dx = (input.x * speed + f.knockback.x) * dt;
    const double dy = (input.y * speed + f.knockback.y) * dt;
    const double radius = 9;
    const auto collidesWithSolid = [&f, radius](double x, double y) {
        const std::array<Vec2, 4> corners{{
            {x - radius, y - radius}, {x + radius, y - radius},
            {x - radius, y + radius}, {x + radius, y + radius}
        }};
        for (const Vec2 corner : corners) {
            const Cell* c = cellAt(corner);
            if (c && (c->tile == Tile::Bed || c->tile == Tile::Wall)) return true;
        }
        return false;
    };
    if (!collidesWithSolid(f.pos.x + dx, f.pos.y))
        f.pos.x += dx;
    if (!collidesWithSolid(f.pos.x, f.pos.y + dy))
        f.pos.y += dy;
    f.knockback.x *= std::exp(-7 * dt);
    f.knockback.y *= std::exp(-7 * dt);
    if (!cellAt(f.pos) || cellAt(f.pos)->tile == Tile::Void) {
        f.hp = 0;
        f.dead = true;
        f.deathTimer = gBedAlive[f.team] ? 4 : -1;
        if (f.deathTimer < 0) f.out = true;
        showNotice(f.name + " JATUH KE VOID");
    }
    if (!f.dead && f.hp < 20) f.hp = std::min(20.0, f.hp + dt * 0.2);
}

void update(double dt, const Uint8* keys) {
    if (!gPlaying || gGameOver || !gPlayer) return;
    gTime += dt;
    gNoticeTimer = std::max(0.0, gNoticeTimer - dt);
    if (gTime >= kMatchLength) {
        int bestTeam = -1, bestAlive = -1, bestKills = -1;
        for (int team = 0; team < 4; ++team) {
            int alive = 0, kills = 0;
            for (const Fighter& f : gFighters) if (f.team == team) {
                alive += !f.out;
                kills += f.kills;
            }
            if (alive > bestAlive || (alive == bestAlive && kills > bestKills)) {
                bestTeam = team; bestAlive = alive; bestKills = kills;
            }
        }
        gWinner = bestTeam;
        gGameOver = true;
        return;
    }

    for (Generator& g : gGenerators) {
        double speed = g.team >= 0 ? 1 + gUpgrades[g.team].forge * 0.5 : 1;
        g.elapsed += dt * speed;
        if (g.elapsed >= g.interval) {
            g.elapsed = 0;
            const int nearby = static_cast<int>(std::count_if(gPickups.begin(), gPickups.end(),
                [&g](const Pickup& p) { return p.type == g.type && length({p.pos.x-g.pos.x,p.pos.y-g.pos.y}) < 30; }));
            if (nearby < 12) gPickups.push_back({g.pos, g.type, 0});
        }
    }

    for (std::size_t i = 0; i < gFighters.size(); ++i) {
        Fighter& f = gFighters[i];
        Vec2 input{};
        if (!f.bot && !gShopOpen) {
            input.x = (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) -
                      (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]);
            input.y = (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) -
                      (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]);
            if (gAutoBridge && length(input) > 0.01) {
                const int cx = static_cast<int>(f.pos.x / kTileSize);
                const int cy = static_cast<int>(f.pos.y / kTileSize);
                const int tx = cx + (std::abs(input.x) > 0.3 ? (input.x > 0 ? 1 : -1) : 0);
                const int ty = cy + (std::abs(input.y) > 0.3 ? (input.y > 0 ? 1 : -1) : 0);
                if (cellAt(tx, ty) && cellAt(tx, ty)->tile == Tile::Void)
                    placeBlock(f, tx, ty, false);
            }
        }
        updateFighter(f, dt, input);
    }

    for (auto it = gPickups.begin(); it != gPickups.end();) {
        bool collected = false;
        for (Fighter& f : gFighters) {
            if (f.dead || f.out || length({f.pos.x-it->pos.x, f.pos.y-it->pos.y}) > 20) continue;
            ++resource(f, it->type);
            it = gPickups.erase(it);
            collected = true;
            break;
        }
        if (!collected) ++it;
    }

    for (auto it = gFuses.begin(); it != gFuses.end();) {
        it->timer -= dt;
        if (it->timer > 0) { ++it; continue; }
        const Vec2 pos = it->pos;
        const int ownerTeam = it->team;
        for (Fighter& f : gFighters) {
            const double d = length({f.pos.x-pos.x, f.pos.y-pos.y});
            if (!f.dead && !f.out && d < 100) {
                f.hp -= std::max(0.0, 12.0 * (1 - d / 120));
                if (f.hp <= 0) { f.dead = true; f.deathTimer = gBedAlive[f.team] ? 4 : -1; f.out = f.deathTimer < 0; }
            }
        }
        for (int y = static_cast<int>(pos.y / kTileSize) - 2; y <= pos.y / kTileSize + 2; ++y)
            for (int x = static_cast<int>(pos.x / kTileSize) - 2; x <= pos.x / kTileSize + 2; ++x) {
                Cell* c = cellAt(x, y);
                if (c && (c->tile == Tile::Block || c->tile == Tile::Wall || c->tile == Tile::Bed) &&
                    c->team != ownerTeam && length({centerOf(x,y).x-pos.x,centerOf(x,y).y-pos.y}) < 90) {
                    c->hp -= 10;
                    if (c->hp <= 0 && c->tile == Tile::Bed) breakCell(*c, gFighters[ownerTeam]);
                    else if (c->hp <= 0) *c = {};
                }
            }
        it = gFuses.erase(it);
    }

    for (auto it = gProjectiles.begin(); it != gProjectiles.end();) {
        it->pos.x += it->velocity.x * dt;
        it->pos.y += it->velocity.y * dt;
        it->remaining -= 430 * dt;
        if (it->bridgeEgg) {
            const int tx = static_cast<int>(it->pos.x / kTileSize);
            const int ty = static_cast<int>(it->pos.y / kTileSize);
            Cell* c = cellAt(tx, ty);
            if (c && c->tile == Tile::Void) {
                bool adjacent = false;
                for (const auto& d : std::array<std::array<int, 2>, 4>{{{{1,0}},{{-1,0}},{{0,1}},{{0,-1}}}})
                    adjacent = adjacent || walkable(tx + d[0], ty + d[1]);
                if (adjacent) *c = {Tile::Block, it->team, 2, false};
            }
        }
        bool boom = it->remaining <= 0;
        for (Fighter& f : gFighters) {
            if (f.team != it->team && !f.dead && !f.out &&
                length({f.pos.x-it->pos.x, f.pos.y-it->pos.y}) < 18) {
                f.hp -= 6;
                if (f.hp <= 0) { f.dead = true; f.deathTimer = gBedAlive[f.team] ? 4 : -1; f.out = f.deathTimer < 0; }
                boom = true;
            }
        }
        if (boom) {
            const Vec2 pos = it->pos;
            for (int y = static_cast<int>(pos.y/kTileSize)-1; y <= pos.y/kTileSize+1; ++y)
                for (int x = static_cast<int>(pos.x/kTileSize)-1; x <= pos.x/kTileSize+1; ++x) {
                    Cell* c = cellAt(x, y);
                    if (c && (c->tile == Tile::Block || c->tile == Tile::Wall || c->tile == Tile::Bed) &&
                        c->team != it->team) {
                        c->hp -= 4;
                        if (c->hp <= 0 && c->tile == Tile::Bed) breakCell(*c, gFighters[it->team]);
                        else if (c->hp <= 0) *c = {};
                    }
                }
            it = gProjectiles.erase(it);
        } else ++it;
    }
    checkWinner();
}

void drawTile(int tx, int ty, const Cell& c) {
    const int x = static_cast<int>(tx * kTileSize - gCameraX);
    const int y = static_cast<int>(ty * kTileSize - gCameraY);
    if (x < -kTileSize || y < -kTileSize || x > kWindowWidth || y > kWindowHeight) return;
    if (c.tile == Tile::Ground) {
        const Color ground = ((tx + ty) & 1) ? Color{86, 164, 70} : Color{94, 177, 78};
        fillRect(x, y, kTileSize, kTileSize, ground);
        fillRect(x, y + kTileSize - 5, kTileSize, 5, {70, 127, 54});
    } else if (c.tile == Tile::Block) {
        fillRect(x, y, kTileSize, kTileSize, c.plank ? Color{168, 119, 68} : kTeamColor[c.team]);
        outlineRect(x + 2, y + 2, kTileSize - 4, kTileSize - 4, kTeamDark[c.team]);
        fillRect(x + 5, y + 6, kTileSize - 10, 3, {255,255,255,65});
    } else if (c.tile == Tile::Bed) {
        fillRect(x + 4, y + 8, 32, 24, kTeamDark[c.team]);
        fillRect(x + 7, y + 10, 26, 18, kTeamColor[c.team]);
        fillRect(x + 9, y + 12, 7, 14, {244,244,244});
        fillRect(x + 17, y + 12, 14, 14, {225,225,225});
    } else if (c.tile == Tile::Wall) {
        fillRect(x, y, kTileSize, kTileSize, {117, 125, 139});
        outlineRect(x + 2, y + 2, kTileSize - 4, kTileSize - 4, kTeamColor[c.team]);
        line(x, y + 20, x + 40, y + 20, {76, 80, 90});
    }
}

void drawFighter(const Fighter& f) {
    if (f.dead || f.out) return;
    const int x = static_cast<int>(f.pos.x - gCameraX);
    const int y = static_cast<int>(f.pos.y - gCameraY);
    if (x < -50 || y < -50 || x > kWindowWidth + 50 || y > kWindowHeight + 50) return;
    circle(x, y + 9, 12, {0, 0, 0, 70});
    if (f.invulnerable > 0 && static_cast<int>(gTime * 12) % 2) return;
    circle(x, y, 12, f.invulnerable > 0 ? Color{255,255,255} : kTeamColor[f.team]);
    circle(x, y, 9, kTeamDark[f.team]);
    circle(x, y, 7, f.invulnerable > 0 ? Color{255,255,255} : kTeamColor[f.team]);
    line(x, y, x + static_cast<int>(f.face.x * 23), y + static_cast<int>(f.face.y * 23),
         {238, 235, 220});
    fillRect(x - 16, y - 25, 32, 4, {24, 24, 30});
    fillRect(x - 16, y - 25, static_cast<int>(32 * std::max(0.0, f.hp / 20)), 4,
             f.team == 0 ? Color{72, 220, 110} : Color{240, 100, 92});
    text(f.name, x - static_cast<int>(f.name.size()) * 3, y - 38, 1, {255,255,255});
}

void drawHud() {
    fillRect(0, 0, kWindowWidth, 65, {19, 24, 39, 245});
    fillRect(0, 64, kWindowWidth, 2, {77, 89, 115});
    text("BEDWARS  " + std::to_string(static_cast<int>(gPlayer->hp)) + " HP", 18, 12, 2, {255,255,255});
    text("IRON " + std::to_string(gPlayer->inv.iron) +
         "  GOLD " + std::to_string(gPlayer->inv.gold) +
         "  DIA " + std::to_string(gPlayer->inv.diamond), 18, 42, 1, {226,230,239});
    char timer[32];
    const int remaining = std::max(0, static_cast<int>(kMatchLength - gTime));
    std::snprintf(timer, sizeof(timer), "%02d:%02d", remaining / 60, remaining % 60);
    text(timer, kWindowWidth - 115, 14, 2, {255,232,133});

    int x = 14, y = kWindowHeight - 106;
    for (int team = 0; team < 4; ++team) {
        fillRect(x, y, 206, 28, {17, 22, 36, 235});
        fillRect(x, y, 5, 28, kTeamColor[team]);
        text(std::string(kTeamName[team]) + (gBedAlive[team] ? " BED OK" : " NO BED"),
             x + 12, y + 8, 1, {255,255,255});
        x += 214;
    }

    const std::array<int, 6> counts{{
        gPlayer->inv.blocks, gPlayer->inv.planks, gPlayer->inv.tnt,
        gPlayer->inv.fire, gPlayer->inv.apple, gPlayer->inv.egg
    }};
    const int total = 6 * 116;
    x = (kWindowWidth - total) / 2;
    y = kWindowHeight - 58;
    for (int slot = 0; slot < 6; ++slot) {
        fillRect(x + slot * 116, y, 108, 48, slot == gSelectedSlot ?
                 Color{61, 76, 111, 245} : Color{22, 28, 44, 235});
        outlineRect(x + slot * 116, y, 108, 48, slot == gSelectedSlot ?
                    Color{255, 213, 94} : Color{102, 113, 140});
        text(std::to_string(slot + 1) + " " + kSlotName[slot], x + slot * 116 + 8, y + 8, 1,
             {255,255,255});
        text(std::to_string(counts[slot]), x + slot * 116 + 8, y + 27, 1, {255,226,137});
    }
    if (gNoticeTimer > 0) {
        const int width = static_cast<int>(gNotice.size()) * 8 + 30;
        fillRect((kWindowWidth-width)/2, 79, width, 34, {16,20,31,230});
        text(gNotice, (kWindowWidth-width)/2+15, 92, 1, {255,255,255});
    }
}

void drawShop() {
    fillRect(0, 0, kWindowWidth, kWindowHeight, {0, 0, 0, 155});
    const int x = 280, y = 110, w = 720, h = 570;
    fillRect(x, y, w, h, {21, 27, 43, 250});
    outlineRect(x, y, w, h, {111, 129, 165});
    text("TOKO BED  -  B TUTUP", x + 24, y + 22, 2, {255,226,137});
    text("ITEM                    HARGA                 UPGRADE TIM", x + 25, y + 58, 1, {185,196,216});
    const std::array<std::string, 9> items{{
        "Q WOOL X16             4 IRON", "W PAPAN X12            4 GOLD",
        "E PEDANG BATU         10 IRON", "R PEDANG BESI          7 GOLD",
        "T PEDANG DIAMOND       4 DIA", "Y APEL EMAS            3 GOLD",
        "U TNT                  4 GOLD", "I BOLA API            24 IRON",
        "O BRIDGE EGG           2 DIA"
    }};
    int rowY = y + 86;
    for (const std::string& item : items) {
        text(item, x + 28, rowY, 1, {242,244,249});
        rowY += 24;
    }
    text("1 FORGE          2 DIA", x + 365, y + 94, 1, {242,244,249});
    text("2 PEDANG TAJAM    4 DIA", x + 365, y + 123, 1, {242,244,249});
    text("3 ARMOR PENGUAT   2 DIA", x + 365, y + 152, 1, {242,244,249});
    text("UPGRADE TIM BUTUH DIAMOND", x + 365, y + 181, 1, {150,210,255});
    text("KLIK ITEM ATAU PAKAI TOMBOL Q W E R T Y U I O", x + 365, y + 177, 1, {185,196,216});
    text("GERAK WASD  SERANG SPACE/KLIK KIRI  ITEM E/KLIK KANAN", x + 24, y + h - 58, 1, {185,196,216});
}

void drawEndScreen() {
    fillRect(0, 0, kWindowWidth, kWindowHeight, {0, 0, 0, 185});
    const int x = 370, y = 250, w = 540, h = 260;
    fillRect(x, y, w, h, {21, 27, 43, 250});
    outlineRect(x, y, w, h, {111, 129, 165});
    text(gWinner == 0 ? "KAMU MENANG!" : gWinner < 0 ? "HASIL SERI" : "KAMU KALAH",
         x + 55, y + 45, 3, gWinner == 0 ? Color{255,226,137} : Color{255,255,255});
    text(gWinner >= 0 ? std::string("TIM ") + kTeamName[gWinner] + " MENANG" : "TIDAK ADA PEMENANG",
         x + 55, y + 105, 2, {220,228,240});
    text("ENTER MAIN LAGI  ESC KELUAR", x + 55, y + 190, 1, {185,196,216});
}

void render() {
    setColor({41, 58, 78});
    SDL_RenderClear(gRenderer);
    fillRect(0, 0, kWindowWidth, kWindowHeight, {72, 133, 186});
    for (int y = 0; y < kMapHeight; ++y)
        for (int x = 0; x < kMapWidth; ++x)
            if (gMap[y * kMapWidth + x].tile != Tile::Void)
                drawTile(x, y, gMap[y * kMapWidth + x]);
    for (const Pickup& p : gPickups) {
        const int x = static_cast<int>(p.pos.x-gCameraX), y = static_cast<int>(p.pos.y-gCameraY);
        const Color c = p.type == Resource::Iron ? Color{220,225,232} :
                        p.type == Resource::Gold ? Color{255,205,64} : Color{67,225,231};
        fillRect(x-5, y-5, 10, 10, c);
        outlineRect(x-5, y-5, 10, 10, {31,35,45});
    }
    for (const Fuse& fuse : gFuses) {
        const int x = static_cast<int>(fuse.pos.x-gCameraX), y = static_cast<int>(fuse.pos.y-gCameraY);
        fillRect(x-12, y-12, 24, 24, {218,55,49});
        fillRect(x-12, y-4, 24, 8, {245,240,230});
        text("TNT", x-9, y-3, 1, {45,38,38});
    }
    for (const Projectile& p : gProjectiles)
        circle(static_cast<int>(p.pos.x-gCameraX), static_cast<int>(p.pos.y-gCameraY), 7,
               {255,152,42});
    std::vector<const Fighter*> sorted;
    for (const Fighter& f : gFighters) sorted.push_back(&f);
    std::sort(sorted.begin(), sorted.end(), [](const Fighter* a, const Fighter* b) {
        return a->pos.y < b->pos.y;
    });
    for (const Fighter* f : sorted) drawFighter(*f);
    drawHud();
    if (gShopOpen) drawShop();
    if (gGameOver) drawEndScreen();
    SDL_RenderPresent(gRenderer);
}

int mouseTileX() {
    int x = 0, y = 0;
    SDL_GetMouseState(&x, &y);
    return static_cast<int>((x + gCameraX) / kTileSize);
}

int mouseTileY() {
    int x = 0, y = 0;
    SDL_GetMouseState(&x, &y);
    return static_cast<int>((y + gCameraY) / kTileSize);
}

void handleShopKey(SDL_Keycode key) {
    switch (key) {
        case SDLK_q: buyItem(*gPlayer, 0); break;
        case SDLK_w: buyItem(*gPlayer, 1); break;
        case SDLK_e: buyItem(*gPlayer, 2); break;
        case SDLK_r: buyItem(*gPlayer, 3); break;
        case SDLK_t: buyItem(*gPlayer, 4); break;
        case SDLK_y: buyItem(*gPlayer, 5); break;
        case SDLK_u: buyItem(*gPlayer, 6); break;
        case SDLK_i: buyItem(*gPlayer, 7); break;
        case SDLK_o: buyItem(*gPlayer, 8); break;
        case SDLK_1: buyUpgrade(*gPlayer, 0); break;
        case SDLK_2: buyUpgrade(*gPlayer, 1); break;
        case SDLK_3: buyUpgrade(*gPlayer, 2); break;
        default: break;
    }
}

void handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) gRunning = false;
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            const SDL_Keycode key = event.key.keysym.sym;
            if (gGameOver) {
                if (key == SDLK_RETURN || key == SDLK_KP_ENTER) startMatch();
                if (key == SDLK_ESCAPE) gRunning = false;
                continue;
            }
            if (!gPlaying) {
                if (key == SDLK_RETURN || key == SDLK_SPACE) startMatch();
                if (key == SDLK_ESCAPE) gRunning = false;
                continue;
            }
            if (key == SDLK_ESCAPE || key == SDLK_b) gShopOpen = !gShopOpen;
            else if (gShopOpen) handleShopKey(key);
            else if (key >= SDLK_1 && key <= SDLK_6) gSelectedSlot = static_cast<int>(key - SDLK_1);
            else if (key == SDLK_t) gAutoBridge = !gAutoBridge;
            else if (key == SDLK_SPACE || key == SDLK_j) attack(*gPlayer);
            else if (key == SDLK_e) useItem(*gPlayer, mouseTileX(), mouseTileY());
        }
        if (event.type == SDL_MOUSEMOTION && gPlayer && gPlaying && !gGameOver) {
            const Vec2 target{event.motion.x + gCameraX, event.motion.y + gCameraY};
            gPlayer->face = normalized({target.x-gPlayer->pos.x, target.y-gPlayer->pos.y});
        }
        if (event.type == SDL_MOUSEBUTTONDOWN && gPlayer && gPlaying && !gGameOver) {
            if (event.button.button == SDL_BUTTON_LEFT && !gShopOpen) attack(*gPlayer);
            if (event.button.button == SDL_BUTTON_RIGHT && !gShopOpen)
                useItem(*gPlayer, mouseTileX(), mouseTileY());
            if (gShopOpen && event.button.button == SDL_BUTTON_LEFT) {
                const int row = (event.button.y - 196) / 24;
                if (event.button.x < 650 && event.button.y >= 196 && row >= 0 && row < 9)
                    buyItem(*gPlayer, row);
                else if (event.button.x >= 650 && event.button.y >= 204 &&
                         event.button.y < 286 && (event.button.y - 204) % 29 < 24)
                    buyUpgrade(*gPlayer, (event.button.y - 204) / 29);
            }
        }
    }
}
}  // namespace

int main(int, char**) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL_Init gagal: %s\n", SDL_GetError());
        return 1;
    }
    gWindow = SDL_CreateWindow("Bedwars PC - C++ / SDL2",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               kWindowWidth, kWindowHeight, SDL_WINDOW_SHOWN);
    if (!gWindow) {
        std::fprintf(stderr, "Gagal membuat window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!gRenderer) gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_SOFTWARE);
    if (!gRenderer) {
        std::fprintf(stderr, "Gagal membuat renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(gWindow);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_BLEND);
    startMatch();

    Uint64 previous = SDL_GetPerformanceCounter();
    while (gRunning) {
        handleEvents();
        const Uint64 now = SDL_GetPerformanceCounter();
        double dt = static_cast<double>(now - previous) / SDL_GetPerformanceFrequency();
        previous = now;
        dt = std::min(dt, 0.05);

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        update(dt, keys);
        if (gPlayer && gPlaying) {
            gCameraX = std::clamp(gPlayer->pos.x - kWindowWidth / 2.0, 0.0,
                                  static_cast<double>(kMapWidth * kTileSize - kWindowWidth));
            gCameraY = std::clamp(gPlayer->pos.y - kWindowHeight / 2.0, 0.0,
                                  static_cast<double>(kMapHeight * kTileSize - kWindowHeight));
        }
        render();
    }

    SDL_DestroyRenderer(gRenderer);
    SDL_DestroyWindow(gWindow);
    SDL_Quit();
    return 0;
}
