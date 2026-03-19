#pragma GCC optimize("O3,unroll-loops,fast-math")
#pragma GCC target("avx2,fma")
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
/* Constants */
#define MAX_W 44
#define MAX_H 24
#define MAX_BIRDS 8
#define MAX_BODY 200
#define MAX_APPLES 200
#define MAX_BIRDS_PP 4
#define POLICY_APB 5
#define POLICY_OUT (MAX_BIRDS_PP*POLICY_APB)
#define GRID_CH 19
#define SCALAR_N 6
#define MAX_CH 128
#define MAX_ACTIONS 256
#define CONTEST_MAX_TURNS 200
#define DIR_N 0
#define DIR_E 1
#define DIR_S 2
#define DIR_W 3
#define DIR_UNSET 4

/* Coord/Direction */
typedef struct { int x, y; } Coord;
static inline Coord coord(int x, int y) { return (Coord){x, y}; }
static inline Coord coord_add(Coord a, Coord b) { return coord(a.x+b.x, a.y+b.y); }
static inline int manhattan(Coord a, Coord b) { return abs(a.x-b.x)+abs(a.y-b.y); }
static const int DX[5] = {0,1,0,-1,0};
static const int DY[5] = {-1,0,1,0,0};
static inline Coord dir_delta(int d) { return coord(DX[d], DY[d]); }
static inline int dir_opposite(int d) {
    static const int opp[5] = {DIR_S, DIR_W, DIR_N, DIR_E, DIR_UNSET};
    return opp[d];
}
static inline int dir_from_coord(Coord c) {
    if (c.x==0 && c.y==-1) return DIR_N;
    if (c.x==1 && c.y==0) return DIR_E;
    if (c.x==0 && c.y==1) return DIR_S;
    if (c.x==-1 && c.y==0) return DIR_W;
    return DIR_UNSET;
}
static inline int turn_left(int d) {
    static const int tl[5] = {DIR_W, DIR_N, DIR_E, DIR_S, DIR_UNSET};
    return tl[d];
}
static inline int turn_right(int d) {
    static const int tr[5] = {DIR_E, DIR_S, DIR_W, DIR_N, DIR_UNSET};
    return tr[d];
}
static const char* dir_word(int d) {
    static const char* w[5] = {"UP","RIGHT","DOWN","LEFT","UP"};
    return w[d];
}

/* Grid */
typedef struct {
    int w, h;
    bool walls[MAX_H][MAX_W];
    Coord apples[MAX_APPLES];
    int napples;
} Grid;
static inline bool grid_valid(const Grid* g, Coord c) {
    return c.x>=0 && c.x<g->w && c.y>=0 && c.y<g->h;
}
static inline Coord grid_opposite(const Grid* g, Coord c) {
    return coord(g->w - 1 - c.x, c.y);
}
static int apple_index(const Grid* g, Coord c) {
    for (int i = 0; i < g->napples; i++)
        if (g->apples[i].x == c.x && g->apples[i].y == c.y) return i;
    return -1;
}
static void remove_apple(Grid* g, int idx) {
    g->apples[idx] = g->apples[--g->napples];
}

/* Bird */
typedef struct {
    int id, owner;
    Coord body[MAX_BODY];
    int head, tail, len;
    bool alive;
    int dir;
} Bird;
static inline Coord bird_head(const Bird* b) { return b->body[b->head]; }
static inline int bird_facing(const Bird* b) {
    if (b->len < 2) return DIR_UNSET;
    Coord h = b->body[b->head];
    int ni = (b->head + 1) % MAX_BODY;
    Coord n = b->body[ni];
    return dir_from_coord(coord(h.x - n.x, h.y - n.y));
}
static inline void bird_push_front(Bird* b, Coord c) {
    b->head = (b->head - 1 + MAX_BODY) % MAX_BODY;
    b->body[b->head] = c;
    b->len++;
}
static inline void bird_pop_back(Bird* b) {
    b->tail = (b->tail - 1 + MAX_BODY) % MAX_BODY;
    b->len--;
}
static inline void bird_pop_front(Bird* b) {
    b->head = (b->head + 1) % MAX_BODY;
    b->len--;
}
static inline Coord bird_seg(const Bird* b, int i) {
    return b->body[(b->head + i) % MAX_BODY];
}
static bool bird_contains(const Bird* b, Coord c) {
    for (int i = 0; i < b->len; i++)
        if (bird_seg(b, i).x == c.x && bird_seg(b, i).y == c.y) return true;
    return false;
}

/* State */
typedef struct {
    Grid grid;
    Bird birds[MAX_BIRDS];
    int nbirds;
    int losses[2];
    int turn;
} State;
static int body_scores(const State* s, int scores[2]) {
    scores[0] = scores[1] = 0;
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].alive) scores[s->birds[i].owner] += s->birds[i].len;
    return 0;
}
static void final_scores(const State* s, int out[2]) {
    body_scores(s, out);
    if (out[0] == out[1]) {
        out[0] -= s->losses[0];
        out[1] -= s->losses[1];
    }
}
static bool is_game_over(const State* s) {
    if (s->grid.napples == 0) return true;
    for (int p = 0; p < 2; p++) {
        bool any = false;
        for (int i = 0; i < s->nbirds; i++)
            if (s->birds[i].owner == p && s->birds[i].alive) { any = true; break; }
        if (!any) return true;
    }
    return false;
}
static bool is_terminal(const State* s, int max_turns) {
    return is_game_over(s) || s->turn >= max_turns;
}

/* Actions */
typedef struct {
    int bird_id[MAX_BIRDS_PP];
    int cmd[MAX_BIRDS_PP];
    int n;
} Action;
static int legal_cmds(const State* s, int bird_id, int out[5]) {
    int n = 0;
    const Bird* b = NULL;
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].id == bird_id && s->birds[i].alive) { b = &s->birds[i]; break; }
    if (!b) return 0;
    int facing = bird_facing(b);
    out[n++] = -1;
    if (facing == DIR_UNSET) {
        for (int d = 0; d < 4; d++) out[n++] = d;
    } else {
        for (int d = 0; d < 4; d++) {
            if (d == facing) continue;
            if (d == dir_opposite(facing)) continue;
            out[n++] = d;
        }
    }
    return n;
}
static int ordered_cmds(const State* s, int bird_id, int out[5]) {
    const Bird* b = NULL;
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].id == bird_id && s->birds[i].alive) { b = &s->birds[i]; break; }
    if (!b) return 0;
    int facing = bird_facing(b);
    int n = 0;
    if (facing == DIR_UNSET) {
        /* No facing → must choose a direction. "Keep" means don't move = useless. */
        for (int d = 0; d < 4; d++) out[n++] = d;
    } else {
        out[n++] = -1; /* keep (go straight) — only valid when we have a facing */
        out[n++] = turn_left(facing);
        out[n++] = turn_right(facing);
    }
    return n;
}
static int enum_joint_for_ids(const State* s, const int* bids, int nb, Action* out, int max_actions) {
    if (nb == 0) {
        out[0].n = 0;
        return 1;
    }
    int cmds[MAX_BIRDS_PP][5], ncmds[MAX_BIRDS_PP];
    for (int i = 0; i < nb; i++)
        ncmds[i] = ordered_cmds(s, bids[i], cmds[i]);
    int total = 1;
    for (int i = 0; i < nb; i++) total *= ncmds[i];
    if (total > max_actions) total = max_actions;
    int idx[MAX_BIRDS_PP];
    memset(idx, 0, sizeof(idx));
    for (int a = 0; a < total; a++) {
        out[a].n = nb;
        for (int i = 0; i < nb; i++) {
            out[a].bird_id[i] = bids[i];
            out[a].cmd[i] = cmds[i][idx[i]];
        }
        for (int i = nb - 1; i >= 0; i--) {
            idx[i]++;
            if (idx[i] < ncmds[i]) break;
            idx[i] = 0;
        }
    }
    return total;
}
static int enum_joint(const State* s, int owner, Action* out, int max_actions) {
    int bids[MAX_BIRDS_PP], nb = 0;
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].owner == owner && s->birds[i].alive && nb < MAX_BIRDS_PP)
            bids[nb++] = s->birds[i].id;
    return enum_joint_for_ids(s, bids, nb, out, max_actions);
}
static int cmd_for_bird(const Action* a, int bird_id) {
    for (int i = 0; i < a->n; i++)
        if (a->bird_id[i] == bird_id) return a->cmd[i];
    return -1;
}

/* Step */
static bool body_in_list(const Coord* body, int len, Coord c) {
    for (int i = 0; i < len; i++)
        if (body[i].x == c.x && body[i].y == c.y) return true;
    return false;
}
static bool solid_under(const State* s, Coord c, const Coord* ignore, int nignore) {
    Coord below = coord(c.x, c.y + 1);
    if (body_in_list(ignore, nignore, below)) return false;
    if (grid_valid(&s->grid, below) && s->grid.walls[below.y][below.x]) return true;
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].alive && bird_contains(&s->birds[i], below)) return true;
    if (apple_index(&s->grid, below) >= 0) return true;
    return false;
}
static void shift_bird_down(Bird* b) {
    for (int i = 0; i < b->len; i++) {
        int idx = (b->head + i) % MAX_BODY;
        b->body[idx].y += 1;
    }
}
static int collect_body(const Bird* b, Coord* out) {
    for (int i = 0; i < b->len; i++)
        out[i] = bird_seg(b, i);
    return b->len;
}
static bool apply_individual_falls(State* s) {
    bool moved = false;
    Coord buf[MAX_BODY];
    for (int i = 0; i < s->nbirds; i++) {
        if (!s->birds[i].alive) continue;
        int n = collect_body(&s->birds[i], buf);
        bool can_fall = true;
        for (int j = 0; j < n; j++)
            if (solid_under(s, buf[j], buf, n)) { can_fall = false; break; }
        if (can_fall) {
            moved = true;
            shift_bird_down(&s->birds[i]);
            bool all_off = true;
            for (int j = 0; j < s->birds[i].len; j++)
                if (bird_seg(&s->birds[i], j).y < s->grid.h + 1) { all_off = false; break; }
            if (all_off) s->birds[i].alive = false;
        }
    }
    return moved;
}
static bool birds_touching(const Bird* a, const Bird* b) {
    for (int i = 0; i < a->len; i++)
        for (int j = 0; j < b->len; j++)
            if (manhattan(bird_seg(a, i), bird_seg(b, j)) == 1) return true;
    return false;
}
static bool apply_intercoiled_falls(State* s) {
    bool moved = false;
    bool seen[MAX_BIRDS];
    memset(seen, 0, sizeof(seen));
    int alive_idx[MAX_BIRDS], nalive = 0;
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].alive) alive_idx[nalive++] = i;
    for (int start = 0; start < nalive; start++) {
        int si = alive_idx[start];
        if (seen[si]) continue;
        int group[MAX_BIRDS], ng = 0;
        int queue[MAX_BIRDS], qh = 0, qt = 0;
        queue[qt++] = si;
        while (qh < qt) {
            int cur = queue[qh++];
            if (seen[cur]) continue;
            seen[cur] = true;
            group[ng++] = cur;
            for (int j = 0; j < nalive; j++) {
                int oi = alive_idx[j];
                if (cur == oi || seen[oi]) continue;
                if (birds_touching(&s->birds[cur], &s->birds[oi]))
                    queue[qt++] = oi;
            }
        }
        if (ng <= 1) continue;
        Coord meta[MAX_BIRDS * MAX_BODY];
        int nm = 0;
        for (int i = 0; i < ng; i++) {
            Bird* b = &s->birds[group[i]];
            for (int j = 0; j < b->len; j++)
                meta[nm++] = bird_seg(b, j);
        }
        bool can_fall = true;
        for (int i = 0; i < nm; i++)
            if (solid_under(s, meta[i], meta, nm)) { can_fall = false; break; }
        if (!can_fall) continue;
        moved = true;
        for (int i = 0; i < ng; i++) {
            shift_bird_down(&s->birds[group[i]]);
            if (bird_head(&s->birds[group[i]]).y >= s->grid.h)
                s->birds[group[i]].alive = false;
        }
    }
    return moved;
}
static void apply_falls(State* s) {
    bool something = true;
    while (something) {
        something = false;
        while (apply_individual_falls(s)) something = true;
        if (apply_intercoiled_falls(s)) something = true;
    }
}
static void step(State* s, const Action* p0, const Action* p1) {
    s->turn++;
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].alive) s->birds[i].dir = -1;
    for (int i = 0; i < s->nbirds; i++) {
        Bird* b = &s->birds[i];
        if (!b->alive) continue;
        int cmd = (b->owner == 0) ? cmd_for_bird(p0, b->id) : cmd_for_bird(p1, b->id);
        int facing = bird_facing(b);
        if (cmd == -1) {
            if (b->dir == -1) b->dir = facing;
        } else {
            if (facing != DIR_UNSET && cmd == dir_opposite(facing))
                b->dir = facing;
            else
                b->dir = cmd;
        }
        int d = (b->dir >= 0) ? b->dir : DIR_UNSET;
        Coord new_head = coord_add(bird_head(b), dir_delta(d));
        bool will_eat = (apple_index(&s->grid, new_head) >= 0);
        if (!will_eat) bird_pop_back(b);
        bird_push_front(b, new_head);
    }
    bool eaten[MAX_APPLES];
    memset(eaten, 0, sizeof(eaten));
    for (int i = 0; i < s->nbirds; i++) {
        if (!s->birds[i].alive) continue;
        int ai = apple_index(&s->grid, bird_head(&s->birds[i]));
        if (ai >= 0) eaten[ai] = true;
    }
    for (int i = s->grid.napples - 1; i >= 0; i--)
        if (eaten[i]) remove_apple(&s->grid, i);
    int to_behead[MAX_BIRDS], nbehead = 0;
    for (int i = 0; i < s->nbirds; i++) {
        Bird* b = &s->birds[i];
        if (!b->alive) continue;
        Coord h = bird_head(b);
        bool in_wall = grid_valid(&s->grid, h) && s->grid.walls[h.y][h.x];
        if (!grid_valid(&s->grid, h)) in_wall = true;
        bool in_bird = false;
        for (int j = 0; j < s->nbirds && !in_bird; j++) {
            if (!s->birds[j].alive) continue;
            if (s->birds[j].id != b->id) {
                if (bird_contains(&s->birds[j], h)) in_bird = true;
            } else {
                for (int k = 1; k < b->len; k++)
                    if (bird_seg(b, k).x == h.x && bird_seg(b, k).y == h.y) { in_bird = true; break; }
            }
        }
        if (in_wall || in_bird) to_behead[nbehead++] = i;
    }
    for (int i = 0; i < nbehead; i++) {
        Bird* b = &s->birds[to_behead[i]];
        if (b->len <= 3) {
            s->losses[b->owner] += b->len;
            b->alive = false;
        } else {
            bird_pop_front(b);
            s->losses[b->owner] += 1;
        }
    }
    apply_falls(s);
}

/* Features */
static float feat_grid[GRID_CH][MAX_H][MAX_W];
static float feat_scalars[SCALAR_N];
static inline void mark_ch(int ch, Coord c, int h, int w) {
    if (c.x >= 0 && c.y >= 0 && c.y < h && c.x < w)
        feat_grid[ch][c.y][c.x] = 1.0f;
}
static bool seg_has_support(const State* s, int bird_id, Coord c) {
    Coord below = coord(c.x, c.y + 1);
    if (grid_valid(&s->grid, below) && s->grid.walls[below.y][below.x]) return true;
    if (apple_index(&s->grid, below) >= 0) return true;
    for (int i = 0; i < s->nbirds; i++) {
        if (!s->birds[i].alive || s->birds[i].id == bird_id) continue;
        if (bird_contains(&s->birds[i], below)) return true;
    }
    return false;
}
static int bird_slot_ids(const State* s, int owner, int out[MAX_BIRDS_PP]) {
    int n = 0;
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].owner == owner && n < MAX_BIRDS_PP)
            out[n++] = s->birds[i].id;
    return n;
}
static int live_count(const State* s, int owner) {
    int n = 0;
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].owner == owner && s->birds[i].alive) n++;
    return n;
}
static int break_count(const State* s, int owner) {
    int n = 0;
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].owner == owner && s->birds[i].alive && s->birds[i].len >= 4) n++;
    return n;
}
static int mobility_count(const State* s, int owner) {
    int total = 0;
    int cmds[5];
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].owner == owner && s->birds[i].alive)
            total += legal_cmds(s, s->birds[i].id, cmds);
    return total;
}
static inline Coord canon(const Grid* g, int owner, Coord c) {
    if (owner == 0) return c;
    return grid_opposite(g, c);
}
static void encode_features(const State* s, int owner) {
    int h = s->grid.h, w = s->grid.w;
    memset(feat_grid, 0, sizeof(feat_grid));
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (s->grid.walls[y][x]) mark_ch(0, canon(&s->grid, owner, coord(x, y)), h, w);
    for (int i = 0; i < s->grid.napples; i++)
        mark_ch(1, canon(&s->grid, owner, s->grid.apples[i]), h, w);
    for (int i = 0; i < s->nbirds; i++) {
        if (!s->birds[i].alive) continue;
        for (int j = 0; j < s->birds[i].len; j++) {
            Coord seg = bird_seg(&s->birds[i], j);
            if (!seg_has_support(s, s->birds[i].id, seg))
                mark_ch(2, canon(&s->grid, owner, seg), h, w);
        }
    }
    int own_ids[MAX_BIRDS_PP], nown = bird_slot_ids(s, owner, own_ids);
    for (int si = 0; si < nown; si++) {
        int hch = 3 + si * 2, bch = hch + 1;
        for (int i = 0; i < s->nbirds; i++) {
            if (s->birds[i].id != own_ids[si] || !s->birds[i].alive) continue;
            mark_ch(hch, canon(&s->grid, owner, bird_head(&s->birds[i])), h, w);
            for (int j = 1; j < s->birds[i].len; j++)
                mark_ch(bch, canon(&s->grid, owner, bird_seg(&s->birds[i], j)), h, w);
        }
    }
    int opp = 1 - owner;
    int opp_ids[MAX_BIRDS_PP], nopp = bird_slot_ids(s, opp, opp_ids);
    for (int si = 0; si < nopp; si++) {
        int hch = 11 + si * 2, bch = hch + 1;
        for (int i = 0; i < s->nbirds; i++) {
            if (s->birds[i].id != opp_ids[si] || !s->birds[i].alive) continue;
            mark_ch(hch, canon(&s->grid, owner, bird_head(&s->birds[i])), h, w);
            for (int j = 1; j < s->birds[i].len; j++)
                mark_ch(bch, canon(&s->grid, owner, bird_seg(&s->birds[i], j)), h, w);
        }
    }
    int bs[2]; body_scores(s, bs);
    float own_body = (float)bs[owner], opp_body = (float)bs[opp];
    float live_d = (float)(live_count(s, owner) - live_count(s, opp));
    float break_d = (float)(break_count(s, owner) - break_count(s, opp));
    float mob_d = (float)(mobility_count(s, owner) - mobility_count(s, opp));
    feat_scalars[0] = fminf(fmaxf((float)s->turn / 200.0f, 0.0f), 1.0f);
    feat_scalars[1] = fminf(fmaxf((own_body - opp_body) / 32.0f, -1.0f), 1.0f);
    feat_scalars[2] = fminf(fmaxf((float)(s->losses[opp] - s->losses[owner]) / 32.0f, -1.0f), 1.0f);
    feat_scalars[3] = fminf(fmaxf(live_d / 4.0f, -1.0f), 1.0f);
    feat_scalars[4] = fminf(fmaxf(break_d / 4.0f, -1.0f), 1.0f);
    feat_scalars[5] = fminf(fmaxf(mob_d / 16.0f, -1.0f), 1.0f);
}

/* CNN */
static float conv_buf_a[MAX_CH * MAX_H * MAX_W];
static float conv_buf_b[MAX_CH * MAX_H * MAX_W];
static void conv3x3_int4(const int8_t* weights, const float* scales, const float* bias,
                          int in_ch, int out_ch, int H, int W,
                          const float* input, float* output) {
    int hw = H * W;
    for (int oc = 0; oc < out_ch; oc++) {
        float bv = bias[oc];
        int base = oc * hw;
        for (int i = 0; i < hw; i++) output[base + i] = bv;
    }
    for (int oc = 0; oc < out_ch; oc++) {
        float sc = scales[oc];
        int obase = oc * hw;
        for (int ic = 0; ic < in_ch; ic++) {
            int ibase = ic * hw;
            int wbase = (oc * in_ch + ic) * 9;
            float w[9];
            for (int k = 0; k < 9; k++) w[k] = (float)weights[wbase + k] * sc;
            for (int y = 1; y < H - 1; y++) {
                int ro = y * W;
                for (int x = 1; x < W - 1; x++) {
                    float acc = 0;
                    acc += input[ibase + (y-1)*W + (x-1)] * w[0];
                    acc += input[ibase + (y-1)*W + x]     * w[1];
                    acc += input[ibase + (y-1)*W + (x+1)] * w[2];
                    acc += input[ibase + y*W + (x-1)]     * w[3];
                    acc += input[ibase + y*W + x]         * w[4];
                    acc += input[ibase + y*W + (x+1)]     * w[5];
                    acc += input[ibase + (y+1)*W + (x-1)] * w[6];
                    acc += input[ibase + (y+1)*W + x]     * w[7];
                    acc += input[ibase + (y+1)*W + (x+1)] * w[8];
                    output[obase + ro + x] += acc;
                }
            }
            for (int edge = 0; edge < 2; edge++) {
                int y = edge == 0 ? 0 : H - 1;
                for (int x = 0; x < W; x++) {
                    float acc = 0;
                    for (int ky = 0; ky < 3; ky++) {
                        int iy = y + ky - 1;
                        if (iy < 0 || iy >= H) continue;
                        for (int kx = 0; kx < 3; kx++) {
                            int ix = x + kx - 1;
                            if (ix < 0 || ix >= W) continue;
                            acc += input[ibase + iy * W + ix] * w[ky * 3 + kx];
                        }
                    }
                    output[obase + y * W + x] += acc;
                }
            }
            for (int y = 1; y < H - 1; y++) {
                for (int edge = 0; edge < 2; edge++) {
                    int x = edge == 0 ? 0 : W - 1;
                    float acc = 0;
                    for (int ky = 0; ky < 3; ky++) {
                        int iy = y + ky - 1;
                        for (int kx = 0; kx < 3; kx++) {
                            int ix = x + kx - 1;
                            if (ix < 0 || ix >= W) continue;
                            acc += input[ibase + iy * W + ix] * w[ky * 3 + kx];
                        }
                    }
                    output[obase + y * W + x] += acc;
                }
            }
        }
    }
    int total = out_ch * hw;
    for (int i = 0; i < total; i++)
        if (output[i] < 0) output[i] = 0;
}
static void global_avg_pool(const float* input, int ch, int H, int W, float* output) {
    int hw = H * W;
    float norm = (float)(hw > 0 ? hw : 1);
    for (int c = 0; c < ch; c++) {
        float sum = 0;
        int base = c * hw;
        for (int i = 0; i < hw; i++) sum += input[base + i];
        output[c] = sum / norm;
    }
}
static void linear_f32(const float* weights, const float* bias, int in_f, int out_f,
                        const float* input, float* output) {
    for (int o = 0; o < out_f; o++) {
        float acc = bias[o];
        int base = o * in_f;
        for (int i = 0; i < in_f; i++) acc += input[i] * weights[base + i];
        output[o] = acc;
    }
}
static int CNN_CH1 = 0, CNN_CH2 = 0, CNN_CH3 = 0;
#define MAX_CONV_W (MAX_CH*MAX_CH*9)
#define MAX_LIN_W (POLICY_OUT*(MAX_CH+SCALAR_N))
static int8_t conv1_w[MAX_CH*GRID_CH*9], conv2_w[MAX_CONV_W], conv3_w[MAX_CONV_W];
static float conv1_s[MAX_CH], conv1_b[MAX_CH];
static float conv2_s[MAX_CH], conv2_b[MAX_CH];
static float conv3_s[MAX_CH], conv3_b[MAX_CH];
static float policy_w[MAX_LIN_W], policy_b[POLICY_OUT];
static float value_w[MAX_CH+SCALAR_N], value_b[1];
#define CONV1_W conv1_w
#define CONV1_S conv1_s
#define CONV1_B conv1_b
#define CONV2_W conv2_w
#define CONV2_S conv2_s
#define CONV2_B conv2_b
#define CONV3_W conv3_w
#define CONV3_S conv3_s
#define CONV3_B conv3_b
#define POLICY_W policy_w
#define POLICY_B policy_b
#define VALUE_W value_w
#define VALUE_B value_b
static float pool_buf[MAX_CH + SCALAR_N];
static float value_out[1];
typedef struct {
    float policy[POLICY_OUT];
    float value;
} NNPred;
static bool nn_enabled = false;
static NNPred nn_forward(int H, int W) {
    NNPred pred;
    int hw = H * W;
    int in_ch = GRID_CH;
    for (int c = 0; c < in_ch; c++)
        for (int y = 0; y < H; y++)
            memcpy(&conv_buf_a[c * hw + y * W], feat_grid[c][y], W * sizeof(float));
    conv3x3_int4(CONV1_W, CONV1_S, CONV1_B, in_ch, CNN_CH1, H, W, conv_buf_a, conv_buf_b);
    conv3x3_int4(CONV2_W, CONV2_S, CONV2_B, CNN_CH1, CNN_CH2, H, W, conv_buf_b, conv_buf_a);
    int last_ch;
    float* last_buf;
    if (CNN_CH3 > 0) {
        conv3x3_int4(CONV3_W, CONV3_S, CONV3_B, CNN_CH2, CNN_CH3, H, W, conv_buf_a, conv_buf_b);
        last_ch = CNN_CH3;
        last_buf = conv_buf_b;
    } else {
        last_ch = CNN_CH2;
        last_buf = conv_buf_a;
    }
    global_avg_pool(last_buf, last_ch, H, W, pool_buf);
    for (int i = 0; i < SCALAR_N; i++) pool_buf[last_ch + i] = feat_scalars[i];
    int feat_dim = last_ch + SCALAR_N;
    linear_f32(POLICY_W, POLICY_B, feat_dim, POLICY_OUT, pool_buf, pred.policy);
    linear_f32(VALUE_W, VALUE_B, feat_dim, 1, pool_buf, value_out);
    pred.value = tanhf(value_out[0]);
    return pred;
}

/* Eval */
static float EVAL_BODY = 120.0f, EVAL_LOSS = 18.0f, EVAL_MOBILITY = 7.5f, EVAL_APPLE = 60.0f;
static float EVAL_STABILITY = 10.0f, EVAL_BREAKPOINT = 25.0f, EVAL_FRAGILE = 8.0f, EVAL_TERMINAL = 10000.0f;
static float EVAL_COVERAGE = 40.0f;
static int SEARCH_FIRST_MS = 950, SEARCH_LATER_MS = 45;
static int min_dist(Coord target, const Coord* heads, int n) {
    int best = 99;
    for (int i = 0; i < n; i++) {
        int d = manhattan(target, heads[i]);
        if (d < best) best = d;
    }
    return best;
}
static double mobility_score(const State* s, int owner) {
    double total = 0;
    int cmds[5];
    for (int i = 0; i < s->nbirds; i++) {
        if (!s->birds[i].alive) continue;
        int n = legal_cmds(s, s->birds[i].id, cmds);
        if (s->birds[i].owner == owner) total += n;
        else total -= n;
    }
    return total;
}
static double apple_race(const State* s, int owner) {
    /* Competitive apple race: for each apple, who's closer? (matches Rust eval) */
    Coord my_h[MAX_BIRDS_PP], opp_h[MAX_BIRDS_PP];
    int nm = 0, no = 0;
    for (int i = 0; i < s->nbirds; i++) {
        if (!s->birds[i].alive) continue;
        Coord h = bird_head(&s->birds[i]);
        if (s->birds[i].owner == owner) { if (nm < MAX_BIRDS_PP) my_h[nm++] = h; }
        else { if (no < MAX_BIRDS_PP) opp_h[no++] = h; }
    }
    double total = 0;
    for (int a = 0; a < s->grid.napples; a++) {
        int my_d = min_dist(s->grid.apples[a], my_h, nm);
        int opp_d = min_dist(s->grid.apples[a], opp_h, no);
        int denom = my_d + opp_d + 1;
        if (denom < 1) denom = 1;
        total += (double)(opp_d - my_d) / (double)denom;
    }
    return total;
}
static bool has_solid_below_eval(const State* s, int bird_id, Coord c) {
    Coord below = coord(c.x, c.y + 1);
    if (grid_valid(&s->grid, below) && s->grid.walls[below.y][below.x]) return true;
    if (apple_index(&s->grid, below) >= 0) return true;
    for (int i = 0; i < s->nbirds; i++) {
        if (!s->birds[i].alive || s->birds[i].id == bird_id) continue;
        if (bird_contains(&s->birds[i], below)) return true;
    }
    return false;
}
static double support_stability(const State* s, int owner) {
    double score = 0;
    for (int i = 0; i < s->nbirds; i++) {
        if (!s->birds[i].alive) continue;
        bool supported = false;
        for (int j = 0; j < s->birds[i].len && !supported; j++)
            if (has_solid_below_eval(s, s->birds[i].id, bird_seg(&s->birds[i], j)))
                supported = true;
        double v = supported ? 1.0 : -1.0;
        if (s->birds[i].owner == owner) score += v;
        else score -= v;
    }
    return score;
}
static double breakpoint_sc(const State* s, int owner) {
    return (double)(break_count(s, owner) - break_count(s, 1 - owner));
}
/* Distinct-apple coverage: reward spreading birds across different apples */
static double apple_coverage(const State* s, int owner) {
    /* Greedy assignment: for each side, assign birds to distinct nearest apples */
    double my_sc = 0, opp_sc = 0;
    for (int side = 0; side < 2; side++) {
        int p = (side == 0) ? owner : 1 - owner;
        Coord heads[MAX_BIRDS_PP];
        int nh = 0;
        for (int i = 0; i < s->nbirds; i++)
            if (s->birds[i].owner == p && s->birds[i].alive && nh < MAX_BIRDS_PP)
                heads[nh++] = bird_head(&s->birds[i]);
        if (nh == 0 || s->grid.napples == 0) continue;
        bool bird_used[MAX_BIRDS_PP] = {false};
        bool apple_used[MAX_APPLES] = {false};
        double score = 0;
        int nassign = nh < s->grid.napples ? nh : s->grid.napples;
        for (int k = 0; k < nassign; k++) {
            int best_b = -1, best_a = -1, best_d = 9999;
            for (int b = 0; b < nh; b++) {
                if (bird_used[b]) continue;
                for (int a = 0; a < s->grid.napples; a++) {
                    if (apple_used[a]) continue;
                    int d = manhattan(heads[b], s->grid.apples[a]);
                    if (d < best_d) { best_d = d; best_b = b; best_a = a; }
                }
            }
            if (best_b < 0) break;
            bird_used[best_b] = true;
            apple_used[best_a] = true;
            score += 1.0 / (double)(best_d + 1);
        }
        if (side == 0) my_sc = score; else opp_sc = score;
    }
    return my_sc - opp_sc;
}

static double fragile_attack(const State* s, int owner) {
    Coord my_h[MAX_BIRDS_PP];
    int nm = 0;
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].owner == owner && s->birds[i].alive && nm < MAX_BIRDS_PP)
            my_h[nm++] = bird_head(&s->birds[i]);
    double total = 0;
    for (int i = 0; i < s->nbirds; i++) {
        if (s->birds[i].owner == owner || !s->birds[i].alive || s->birds[i].len > 3) continue;
        int d = min_dist(bird_head(&s->birds[i]), my_h, nm);
        total += 1.0 / (double)(d + 1);
    }
    /* Damp in symmetric openings: when all birds are len ≤ 3, this is noise */
    bool all_fragile = true;
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].alive && s->birds[i].len > 3) { all_fragile = false; break; }
    if (all_fragile) total *= 0.25;
    return total;
}
/* How many steps until this bird hits a wall/edge going straight? */
static int turns_to_wall(const State* s, const Bird* b) {
    int facing = bird_facing(b);
    if (facing == DIR_UNSET) return 99;
    Coord pos = bird_head(b);
    Coord d = dir_delta(facing);
    for (int i = 1; i <= 5; i++) {
        Coord next = coord(pos.x + d.x * i, pos.y + d.y * i);
        if (!grid_valid(&s->grid, next)) return i;
        if (s->grid.walls[next.y][next.x]) return i;
        /* Check body collision with any bird */
        for (int j = 0; j < s->nbirds; j++)
            if (s->birds[j].alive && bird_contains(&s->birds[j], next) && s->birds[j].id != b->id)
                return i;
    }
    return 99;
}
/* Gravity danger: penalize birds with no support below any segment */
static double gravity_danger(const State* s, int owner) {
    double total = 0;
    for (int i = 0; i < s->nbirds; i++) {
        if (!s->birds[i].alive) continue;
        /* Check if ANY segment has solid support below */
        bool supported = false;
        for (int j = 0; j < s->birds[i].len && !supported; j++) {
            Coord seg = bird_seg(&s->birds[i], j);
            Coord below = coord(seg.x, seg.y + 1);
            if (!grid_valid(&s->grid, below)) { supported = true; break; } /* bottom of map = support */
            if (s->grid.walls[below.y][below.x]) { supported = true; break; }
            if (apple_index(&s->grid, below) >= 0) { supported = true; break; }
            for (int k = 0; k < s->nbirds; k++) {
                if (k == i) continue;
                if (s->birds[k].alive && bird_contains(&s->birds[k], below)) { supported = true; break; }
            }
        }
        /* Unsupported = will fall. Penalize, especially short birds */
        if (!supported) {
            double penalty = -15.0;
            if (s->birds[i].len <= 3) penalty = -40.0;
            if (s->birds[i].owner == owner) total += penalty;
            else total -= penalty;
        }
        /* Extra: penalize facing UP for short birds (fighting gravity) */
        if (s->birds[i].len <= 4 && bird_facing(&s->birds[i]) == DIR_N) {
            double up_penalty = -8.0;
            if (s->birds[i].owner == owner) total += up_penalty;
            else total -= up_penalty;
        }
    }
    return total;
}
/* Danger score: penalize birds heading toward walls */
static double danger_score(const State* s, int owner) {
    double total = 0;
    for (int i = 0; i < s->nbirds; i++) {
        if (!s->birds[i].alive) continue;
        int ttw = turns_to_wall(s, &s->birds[i]);
        double penalty = 0;
        if (ttw <= 1) penalty = -100.0;       /* about to die */
        else if (ttw <= 2) penalty = -40.0;   /* 2 turns from death */
        else if (ttw <= 3) penalty = -10.0;   /* getting close */
        /* Short birds get higher penalty (death = total loss) */
        if (s->birds[i].len <= 3) penalty *= 1.5;
        if (s->birds[i].owner == owner) total += penalty;
        else total -= penalty;
    }
    return total;
}
static double evaluate(const State* s, int owner) {
    if (is_terminal(s, CONTEST_MAX_TURNS)) {
        int bs[2]; body_scores(s, bs);
        int bd = bs[owner] - bs[1 - owner];
        int ld = s->losses[1 - owner] - s->losses[owner];
        /* Scale terminal by remaining turns — early death is much worse than late */
        double remaining = (double)(CONTEST_MAX_TURNS - s->turn + 1);
        return EVAL_BODY * (double)bd * remaining + EVAL_LOSS * (double)ld * remaining;
    }
    int bs[2]; body_scores(s, bs);
    double bd = (double)(bs[owner] - bs[1 - owner]);
    double ld = (double)(s->losses[1 - owner] - s->losses[owner]);
    return EVAL_BODY * bd
         + EVAL_LOSS * ld
         + EVAL_MOBILITY * mobility_score(s, owner)
         + EVAL_APPLE * apple_race(s, owner)
         + EVAL_COVERAGE * apple_coverage(s, owner)
         + EVAL_STABILITY * support_stability(s, owner)
         + EVAL_BREAKPOINT * breakpoint_sc(s, owner)
         + EVAL_FRAGILE * fragile_attack(s, owner)
         + danger_score(s, owner)
         + gravity_danger(s, owner);
}

/* Hybrid */
static int policy_idx_for_cmd(int cmd) {
    if (cmd == -1) return 0;
    return cmd + 1;
}

/* Search */
static State sim_state(const State* s, int owner, const Action* my, const Action* opp) {
    State next = *s;
    if (owner == 0) step(&next, my, opp);
    else step(&next, opp, my);
    return next;
}
static Action empty_action(void) {
    Action a; a.n = 0; return a;
}
static struct timespec search_start;
static double elapsed_ms(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - search_start.tv_sec) * 1000.0
         + (now.tv_nsec - search_start.tv_nsec) / 1.0e6;
}
static bool expired(double deadline_ms) {
    if (deadline_ms <= 0) return false;
    return elapsed_ms() >= deadline_ms;
}
typedef struct {
    Action action;
    int action_id, action_count;
    double score, mean_score;
} SearchResult;

/* Render */
static void render_action(const Action* a, char* buf) {
    int ids[MAX_BIRDS_PP], dirs[MAX_BIRDS_PP], nc = 0;
    for (int i = 0; i < a->n; i++) {
        if (a->cmd[i] >= 0) {
            ids[nc] = a->bird_id[i];
            dirs[nc] = a->cmd[i];
            nc++;
        }
    }
    for (int i = 0; i < nc - 1; i++)
        for (int j = i + 1; j < nc; j++)
            if (ids[j] < ids[i]) {
                int t = ids[i]; ids[i] = ids[j]; ids[j] = t;
                t = dirs[i]; dirs[i] = dirs[j]; dirs[j] = t;
            }
    if (nc == 0) { strcpy(buf, "WAIT"); return; }
    buf[0] = 0;
    for (int i = 0; i < nc; i++) {
        if (i > 0) strcat(buf, ";");
        char tmp[32];
        sprintf(tmp, "%d %s", ids[i], dir_word(dirs[i]));
        strcat(buf, tmp);
    }
}

/* IO */
typedef struct {
    int player_idx;
    int my_ids[MAX_BIRDS_PP], opp_ids[MAX_BIRDS_PP];
    int nmy, nopp;
    Grid template_grid;
} BotIO;
typedef struct {
    Coord apples[MAX_APPLES];
    int napples;
    int live_ids[MAX_BIRDS];
    Coord live_bodies[MAX_BIRDS][MAX_BODY];
    int live_lens[MAX_BIRDS];
    int nlive;
} Frame;
static char line_buf[8192];
static char* read_line_s(void) {
    if (!fgets(line_buf, sizeof(line_buf), stdin)) return NULL;
    int len = strlen(line_buf);
    while (len > 0 && (line_buf[len-1] == '\n' || line_buf[len-1] == '\r')) line_buf[--len] = 0;
    return line_buf;
}
static void read_init(BotIO* io) {
    io->player_idx = atoi(read_line_s());
    int w = atoi(read_line_s());
    int h = atoi(read_line_s());
    io->template_grid.w = w;
    io->template_grid.h = h;
    io->template_grid.napples = 0;
    memset(io->template_grid.walls, 0, sizeof(io->template_grid.walls));
    for (int y = 0; y < h; y++) {
        char* row = read_line_s();
        for (int x = 0; x < w && row[x]; x++)
            if (row[x] == '#') io->template_grid.walls[y][x] = true;
    }
    int bpp = atoi(read_line_s());
    io->nmy = bpp;
    io->nopp = bpp;
    for (int i = 0; i < bpp; i++) io->my_ids[i] = atoi(read_line_s());
    for (int i = 0; i < bpp; i++) io->opp_ids[i] = atoi(read_line_s());
}
static bool read_frame(Frame* f) {
    char* s = read_line_s();
    if (!s) return false;
    f->napples = atoi(s);
    for (int i = 0; i < f->napples; i++) {
        s = read_line_s();
        sscanf(s, "%d %d", &f->apples[i].x, &f->apples[i].y);
    }
    int nlive = atoi(read_line_s());
    f->nlive = nlive;
    for (int i = 0; i < nlive; i++) {
        s = read_line_s();
        int id;
        char body_str[4096];
        sscanf(s, "%d %s", &id, body_str);
        f->live_ids[i] = id;
        f->live_lens[i] = 0;
        char* tok = strtok(body_str, ":");
        while (tok) {
            int bx, by;
            sscanf(tok, "%d,%d", &bx, &by);
            f->live_bodies[i][f->live_lens[i]++] = coord(bx, by);
            tok = strtok(NULL, ":");
        }
    }
    return true;
}
static int owner_of(const BotIO* io, int bird_id) {
    for (int i = 0; i < io->nmy; i++)
        if (io->my_ids[i] == bird_id) return io->player_idx;
    return 1 - io->player_idx;
}
static State build_state(const BotIO* io, const Frame* f, int losses[2], int turn) {
    State s;
    s.grid = io->template_grid;
    s.grid.napples = f->napples;
    for (int i = 0; i < f->napples; i++) s.grid.apples[i] = f->apples[i];
    s.losses[0] = losses[0];
    s.losses[1] = losses[1];
    s.turn = turn;
    s.nbirds = 0;
    int all_ids[MAX_BIRDS], nall = 0;
    for (int i = 0; i < io->nmy; i++) all_ids[nall++] = io->my_ids[i];
    for (int i = 0; i < io->nopp; i++) all_ids[nall++] = io->opp_ids[i];
    for (int i = 0; i < nall - 1; i++)
        for (int j = i + 1; j < nall; j++)
            if (all_ids[j] < all_ids[i]) { int t = all_ids[i]; all_ids[i] = all_ids[j]; all_ids[j] = t; }
    for (int a = 0; a < nall; a++) {
        int bid = all_ids[a];
        Bird* b = &s.birds[s.nbirds++];
        b->id = bid;
        b->owner = owner_of(io, bid);
        b->dir = -1;
        int fi = -1;
        for (int i = 0; i < f->nlive; i++)
            if (f->live_ids[i] == bid) { fi = i; break; }
        if (fi >= 0) {
            b->alive = true;
            b->len = f->live_lens[fi];
            b->head = 0;
            b->tail = b->len;
            for (int i = 0; i < b->len; i++) b->body[i] = f->live_bodies[fi][i];
        } else {
            b->alive = false;
            b->len = 0;
            b->head = 0;
            b->tail = 0;
        }
    }
    return s;
}

/* Reconcile */
static bool sig_match(const State* s, const Frame* f) {
    if (s->grid.napples != f->napples) return false;
    Coord sa[MAX_APPLES], fa[MAX_APPLES];
    for (int i = 0; i < s->grid.napples; i++) sa[i] = s->grid.apples[i];
    for (int i = 0; i < f->napples; i++) fa[i] = f->apples[i];
    for (int i = 0; i < s->grid.napples - 1; i++)
        for (int j = i + 1; j < s->grid.napples; j++)
            if (sa[j].y < sa[i].y || (sa[j].y == sa[i].y && sa[j].x < sa[i].x))
                { Coord t = sa[i]; sa[i] = sa[j]; sa[j] = t; }
    for (int i = 0; i < f->napples - 1; i++)
        for (int j = i + 1; j < f->napples; j++)
            if (fa[j].y < fa[i].y || (fa[j].y == fa[i].y && fa[j].x < fa[i].x))
                { Coord t = fa[i]; fa[i] = fa[j]; fa[j] = t; }
    for (int i = 0; i < s->grid.napples; i++)
        if (sa[i].x != fa[i].x || sa[i].y != fa[i].y) return false;
    int nlive_s = 0;
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].alive) nlive_s++;
    if (nlive_s != f->nlive) return false;
    for (int fi = 0; fi < f->nlive; fi++) {
        int bid = f->live_ids[fi];
        const Bird* sb = NULL;
        for (int i = 0; i < s->nbirds; i++)
            if (s->birds[i].id == bid && s->birds[i].alive) { sb = &s->birds[i]; break; }
        if (!sb) return false;
        if (sb->len != f->live_lens[fi]) return false;
        for (int j = 0; j < sb->len; j++) {
            Coord seg = bird_seg(sb, j);
            if (seg.x != f->live_bodies[fi][j].x || seg.y != f->live_bodies[fi][j].y) return false;
        }
    }
    return true;
}
static State reconcile(const BotIO* io, const Frame* f, const State* prev, const Action* last_my) {
    if (!prev) return build_state(io, f, (int[]){0, 0}, 0);
    int opp = 1 - io->player_idx;
    Action opp_acts[MAX_ACTIONS];
    int nopp = enum_joint(prev, opp, opp_acts, MAX_ACTIONS);
    for (int i = 0; i < nopp; i++) {
        State sim = *prev;
        if (io->player_idx == 0) step(&sim, last_my, &opp_acts[i]);
        else step(&sim, &opp_acts[i], last_my);
        if (sig_match(&sim, f)) return sim;
    }
    return build_state(io, f, (int[]){prev->losses[0], prev->losses[1]}, prev->turn + 1);
}
static void init_weights(void);

/* Maximin search with scripted short birds and selective deepening */

#define SHORT_THRESHOLD 0  /* 0 = search all birds; >0 = script birds with len <= this */

#ifndef DEBUG_VERBOSE
#define DEBUG_VERBOSE 1    /* 1 = full debug output on stderr */
#endif
#define DEEPEN_TOP_MY 6
#define DEEPEN_TOP_OPP 8
#define DEEPEN_CHILD_MY 3
#define DEEPEN_CHILD_OPP 3

/* Script a short bird: safety-first, then apple-chase */
static int script_short_bird(const State* s, const Bird* b) {
    Coord head = bird_head(b);
    int facing = bird_facing(b);
    int cmds[5], ncmds = 0;
    if (facing == DIR_UNSET) {
        for (int d = 0; d < 4; d++) cmds[ncmds++] = d;
    } else {
        cmds[ncmds++] = -1;
        cmds[ncmds++] = turn_left(facing);
        cmds[ncmds++] = turn_right(facing);
    }
    int best_cmd = cmds[0], best_score = -999999;
    for (int ci = 0; ci < ncmds; ci++) {
        int cmd = cmds[ci];
        int dir = (cmd == -1) ? facing : cmd;
        if (dir < 0 || dir > 3) continue;
        Coord nh = coord_add(head, dir_delta(dir));
        int score = 0;
        /* Safety: walls/edges */
        bool fatal = false;
        if (!grid_valid(&s->grid, nh) || s->grid.walls[nh.y][nh.x]) fatal = true;
        /* Safety: body collisions */
        if (!fatal) {
            for (int j = 0; j < s->nbirds && !fatal; j++) {
                if (!s->birds[j].alive) continue;
                if (s->birds[j].id == b->id) {
                    for (int k = 1; k < b->len; k++)
                        if (bird_seg(b, k).x == nh.x && bird_seg(b, k).y == nh.y)
                            { fatal = true; break; }
                } else if (bird_contains(&s->birds[j], nh)) fatal = true;
            }
        }
        if (fatal) {
            if (b->len <= 3) continue; /* instant death for fragile birds */
            score -= 100000; /* beheading — last resort */
        }
        /* Apple proximity (dominant signal) */
        int best_d = 999;
        for (int a = 0; a < s->grid.napples; a++) {
            int d = manhattan(nh, s->grid.apples[a]);
            if (d < best_d) best_d = d;
        }
        score += (50 - best_d) * ((b->len <= 3) ? 200 : 100);
        /* Support bonus (gravity safety) */
        Coord below = coord(nh.x, nh.y + 1);
        bool supported = !grid_valid(&s->grid, below);
        if (!supported) supported = s->grid.walls[below.y][below.x];
        if (!supported) supported = (apple_index(&s->grid, below) >= 0);
        if (!supported) {
            for (int j = 0; j < s->nbirds; j++)
                if (s->birds[j].alive && s->birds[j].id != b->id &&
                    bird_contains(&s->birds[j], below)) { supported = true; break; }
        }
        if (supported) score += 500;
        /* Avoid going up (fighting gravity) */
        if (dir == DIR_N) score -= 100;
        /* Wall clearance */
        int cx = nh.x < s->grid.w - 1 - nh.x ? nh.x : s->grid.w - 1 - nh.x;
        int cy = nh.y < s->grid.h - 1 - nh.y ? nh.y : s->grid.h - 1 - nh.y;
        score += (cx + cy) * 5;
        if (score > best_score) { best_score = score; best_cmd = cmd; }
    }
    return best_cmd;
}

/* Sort helper: partial insertion sort of indices by score (descending) */
static void sort_desc(int* order, const double* scores, int n) {
    for (int i = 1; i < n; i++) {
        int key = order[i];
        double ks = scores[key];
        int j = i - 1;
        while (j >= 0 && scores[order[j]] < ks) {
            order[j + 1] = order[j]; j--;
        }
        order[j + 1] = key;
    }
}
static void sort_asc(int* order, const double* scores, int n) {
    for (int i = 1; i < n; i++) {
        int key = order[i];
        double ks = scores[key];
        int j = i - 1;
        while (j >= 0 && scores[order[j]] > ks) {
            order[j + 1] = order[j]; j--;
        }
        order[j + 1] = key;
    }
}

/* Scripted rollout: both sides greedily chase nearest apple, avoid walls */
static int script_greedy_cmd(const State* s, const Bird* b) {
    Coord head = bird_head(b);
    int facing = bird_facing(b);
    int cmds[5], ncmds = 0;
    if (facing == DIR_UNSET) {
        for (int d = 0; d < 4; d++) cmds[ncmds++] = d;
    } else {
        cmds[ncmds++] = -1;
        cmds[ncmds++] = turn_left(facing);
        cmds[ncmds++] = turn_right(facing);
    }
    int best_cmd = cmds[0], best_score = -999999;
    for (int ci = 0; ci < ncmds; ci++) {
        int cmd = cmds[ci];
        int dir = (cmd == -1) ? facing : cmd;
        if (dir < 0 || dir > 3) continue;
        Coord nh = coord_add(head, dir_delta(dir));
        if (!grid_valid(&s->grid, nh) || s->grid.walls[nh.y][nh.x]) continue;
        int score = 0;
        int best_d = 999;
        for (int a = 0; a < s->grid.napples; a++) {
            int d = manhattan(nh, s->grid.apples[a]);
            if (d < best_d) best_d = d;
        }
        score += 1000 - best_d * 20;
        Coord below = coord(nh.x, nh.y + 1);
        if (!grid_valid(&s->grid, below) || s->grid.walls[below.y][below.x])
            score += 300;
        if (dir == DIR_N) score -= 80;
        if (score > best_score) { best_score = score; best_cmd = cmd; }
    }
    return best_cmd;
}
static Action script_all_birds(const State* s, int owner) {
    Action a; a.n = 0;
    for (int i = 0; i < s->nbirds; i++) {
        if (s->birds[i].owner != owner || !s->birds[i].alive) continue;
        if (a.n >= MAX_BIRDS_PP) break;
        a.bird_id[a.n] = s->birds[i].id;
        a.cmd[a.n] = script_greedy_cmd(s, &s->birds[i]);
        a.n++;
    }
    return a;
}

/* Selective deepening: mini-maximin at next depth */
static double deepen_branch_c(const State* s, int owner, double deadline_ms) {
    Action my_acts[MAX_ACTIONS], opp_acts[MAX_ACTIONS];
    int nmy = enum_joint(s, owner, my_acts, MAX_ACTIONS);
    int nopp = enum_joint(s, 1 - owner, opp_acts, MAX_ACTIONS);
    int lim_my = DEEPEN_CHILD_MY < nmy ? DEEPEN_CHILD_MY : nmy;
    int lim_opp = DEEPEN_CHILD_OPP < nopp ? DEEPEN_CHILD_OPP : nopp;

    double msc[MAX_ACTIONS];
    int mord[MAX_ACTIONS];
    for (int a = 0; a < nmy; a++) {
        State nx = sim_state(s, owner, &my_acts[a], &opp_acts[0]);
        msc[a] = evaluate(&nx, owner);
        mord[a] = a;
    }
    sort_desc(mord, msc, nmy);

    double osc[MAX_ACTIONS];
    int oord[MAX_ACTIONS];
    for (int o = 0; o < nopp; o++) {
        State nx = sim_state(s, owner, &my_acts[mord[0]], &opp_acts[o]);
        osc[o] = evaluate(&nx, owner);
        oord[o] = o;
    }
    sort_asc(oord, osc, nopp);

    double best = -1e18;
    for (int mi = 0; mi < lim_my; mi++) {
        if (expired(deadline_ms)) break;
        double worst = 1e18;
        for (int oi = 0; oi < lim_opp; oi++) {
            if (expired(deadline_ms)) break;
            State nx = sim_state(s, owner, &my_acts[mord[mi]], &opp_acts[oord[oi]]);
            double v = evaluate(&nx, owner);
            if (v < worst) worst = v;
        }
        if (worst > best) best = worst;
    }
    return best > -1e18 ? best : evaluate(s, owner);
}

/* Merge scripted bird commands into an action */
static void merge_scripts(Action* a, const int* sids, const int* scmds, int nscript) {
    int n = a->n;
    for (int i = 0; i < nscript; i++) {
        a->bird_id[n + i] = sids[i];
        a->cmd[n + i] = scmds[i];
    }
    a->n = n + nscript;
}

static SearchResult choose_action_maximin(const State* s, int owner, double deadline_ms) {
    /* Phase 0: Script short birds */
    int script_ids[MAX_BIRDS_PP], script_cmds[MAX_BIRDS_PP], nscript = 0;
    int search_ids[MAX_BIRDS_PP], nsearch = 0;
    for (int i = 0; i < s->nbirds; i++) {
        if (s->birds[i].owner != owner || !s->birds[i].alive) continue;
        if (s->birds[i].len <= SHORT_THRESHOLD) {
            script_ids[nscript] = s->birds[i].id;
            script_cmds[nscript] = script_short_bird(s, &s->birds[i]);
            nscript++;
        } else {
            if (nsearch < MAX_BIRDS_PP) search_ids[nsearch++] = s->birds[i].id;
        }
    }

    /* Phase 1: Enumerate my joint actions (search birds + merge scripts) */
    Action my_acts[MAX_ACTIONS];
    int nmy;
    if (nsearch == 0) {
        nmy = 1;
        my_acts[0].n = 0;
        merge_scripts(&my_acts[0], script_ids, script_cmds, nscript);
    } else {
        nmy = enum_joint_for_ids(s, search_ids, nsearch, my_acts, MAX_ACTIONS);
        for (int a = 0; a < nmy; a++)
            merge_scripts(&my_acts[a], script_ids, script_cmds, nscript);
    }
    if (nmy == 0) {
        SearchResult sr; sr.action = empty_action(); sr.action_id = 0;
        sr.action_count = 1; sr.score = sr.mean_score = -1e18;
        return sr;
    }

    /* Phase 2: Enumerate opponent joint actions */
    Action opp_acts[MAX_ACTIONS];
    int nopp = enum_joint(s, 1 - owner, opp_acts, MAX_ACTIONS);

    /* Phase 3: Order my actions by eval vs 3 representative opp actions */
    /* First find 2 worst opp actions (plus default) for better ordering signal */
    int opp_repr[3]; int nrepr = 0;
    opp_repr[nrepr++] = 0; /* default opp */
    {
        double osc_tmp[MAX_ACTIONS];
        for (int o = 0; o < nopp; o++) {
            State nx = sim_state(s, owner, &my_acts[0], &opp_acts[o]);
            osc_tmp[o] = evaluate(&nx, owner);
        }
        /* Find 2 worst (lowest score) opp actions */
        for (int k = 0; k < 2 && k < nopp; k++) {
            int bi = -1; double bv = 1e18;
            for (int o = 0; o < nopp; o++) {
                bool used = false;
                for (int r = 0; r < nrepr; r++) if (opp_repr[r] == o) used = true;
                if (!used && osc_tmp[o] < bv) { bv = osc_tmp[o]; bi = o; }
            }
            if (bi >= 0) opp_repr[nrepr++] = bi;
        }
    }
    double my_prior[MAX_ACTIONS];
    int my_order[MAX_ACTIONS];
    for (int a = 0; a < nmy; a++) {
        double sum = 0;
        for (int r = 0; r < nrepr; r++) {
            State nx = sim_state(s, owner, &my_acts[a], &opp_acts[opp_repr[r]]);
            sum += evaluate(&nx, owner);
        }
        my_prior[a] = sum / nrepr;
        my_order[a] = a;
    }
    sort_desc(my_order, my_prior, nmy);

    /* Phase 4: Order opp actions (worst for me first) */
    double opp_sc[MAX_ACTIONS];
    int opp_order[MAX_ACTIONS];
    for (int o = 0; o < nopp; o++) {
        State nx = sim_state(s, owner, &my_acts[my_order[0]], &opp_acts[o]);
        opp_sc[o] = evaluate(&nx, owner);
        opp_order[o] = o;
    }
    sort_asc(opp_order, opp_sc, nopp);

    /* Phase 5: Maximin root pass with alpha-beta cutoffs */
    typedef struct { int idx; double worst; double mean; double cvar3; double rank_score; } RootEntry;
    RootEntry roots[MAX_ACTIONS];
    int nroots = 0;
    double best_worst = -1e18;
    int pairs = 0, cutoffs = 0;

    for (int mi = 0; mi < nmy; mi++) {
        if ((mi & 3) == 0 && expired(deadline_ms)) break;
        int a = my_order[mi];
        double worst = 1e18, total = 0;
        int eval_count = 0;
        for (int oi = 0; oi < nopp; oi++) {
            if (expired(deadline_ms)) break;
            int o = opp_order[oi];
            State nx = sim_state(s, owner, &my_acts[a], &opp_acts[o]);
            double sc = evaluate(&nx, owner);
            pairs++;
            total += sc;
            eval_count++;
            if (sc < worst) worst = sc;
            /* Alpha-beta cutoff: this action already worse than best known */
            if (worst < best_worst) { cutoffs++; break; }
        }
        if (eval_count > 0) {
            roots[nroots].idx = a;
            roots[nroots].worst = worst;
            roots[nroots].mean = total / eval_count;
            nroots++;
            if (worst > best_worst) best_worst = worst;
        }
    }

    /* Sort roots: best worst-case first, break ties by mean */
    for (int i = 0; i < nroots - 1; i++)
        for (int j = i + 1; j < nroots; j++)
            if (roots[j].worst > roots[i].worst ||
                (roots[j].worst == roots[i].worst && roots[j].mean > roots[i].mean))
                { RootEntry t = roots[i]; roots[i] = roots[j]; roots[j] = t; }

    /* Phase 6: Selective deepening of top actions (time-limited) */
    int deep_max = DEEPEN_TOP_MY;
    /* On first turn, deepen more since we have ~900ms */
    if (s->turn == 0) deep_max = nroots; /* time check handles the limit */
    int deep_count = deep_max < nroots ? deep_max : nroots;
    int deep_done = 0;
    for (int ri = 0; ri < deep_count; ri++) {
        if (expired(deadline_ms)) break;
        int a = roots[ri].idx;
        /* Find the worst opponent responses for this action */
        double resp_sc[MAX_ACTIONS];
        int resp_ord[MAX_ACTIONS];
        for (int oi = 0; oi < nopp; oi++) {
            int o = opp_order[oi]; /* reuse original ordering */
            State nx = sim_state(s, owner, &my_acts[a], &opp_acts[o]);
            resp_sc[oi] = evaluate(&nx, owner);
            resp_ord[oi] = oi;
        }
        sort_asc(resp_ord, resp_sc, nopp);
        /* Deepen the worst DEEPEN_TOP_OPP responses */
        int dlim = DEEPEN_TOP_OPP < nopp ? DEEPEN_TOP_OPP : nopp;
        for (int di = 0; di < dlim; di++) {
            if (expired(deadline_ms)) break;
            int oi = resp_ord[di];
            int o = opp_order[oi];
            State nx = sim_state(s, owner, &my_acts[a], &opp_acts[o]);
            double child_sc = deepen_branch_c(&nx, owner, deadline_ms);
            resp_sc[oi] = child_sc;
        }
        /* Recompute worst, mean, and cvar3 */
        double new_worst = 1e18, new_total = 0;
        for (int oi = 0; oi < nopp; oi++) {
            if (resp_sc[oi] < new_worst) new_worst = resp_sc[oi];
            new_total += resp_sc[oi];
        }
        /* CVaR3: average of 3 worst responses */
        double cvar3 = new_worst;
        if (nopp >= 3) {
            /* resp_ord is already sorted ascending, pick bottom 3 */
            double csum = 0;
            int cn = 3 < nopp ? 3 : nopp;
            for (int k = 0; k < cn; k++) csum += resp_sc[resp_ord[k]];
            cvar3 = csum / cn;
        }
        roots[ri].worst = new_worst;
        roots[ri].mean = new_total / nopp;
        roots[ri].cvar3 = cvar3;
        deep_done++;
    }

    /* Compute CVaR rank scores for deepened roots.
       Lambda adapts: behind/opening = more cvar (less pessimistic),
       ahead/endgame = more worst (safer) */
    {
        int bs2[2]; body_scores(s, bs2);
        int body_diff = bs2[owner] - bs2[1 - owner];
        double lambda;
        if (body_diff >= 3 || s->grid.napples <= 3)
            lambda = 0.05; /* ahead or endgame: trust worst */
        else if (body_diff <= -3 || s->grid.napples >= 8)
            lambda = 0.25; /* behind or opening: less pessimistic */
        else
            lambda = 0.15;
        for (int ri = 0; ri < nroots; ri++) {
            if (ri < deep_done) {
                /* Deepened: use CVaR blend */
                roots[ri].rank_score = (1.0 - lambda) * roots[ri].worst + lambda * roots[ri].cvar3;
            } else {
                /* Not deepened: use worst (truncated mean unreliable) */
                roots[ri].rank_score = roots[ri].worst;
            }
        }
    }

    /* Find best root by rank_score */
    int best_ri = 0;
    for (int i = 1; i < nroots; i++)
        if (roots[i].rank_score > roots[best_ri].rank_score ||
            (roots[i].rank_score == roots[best_ri].rank_score && roots[i].mean > roots[best_ri].mean))
            best_ri = i;

    SearchResult sr;
    sr.action = nroots > 0 ? my_acts[roots[best_ri].idx] : my_acts[0];
    sr.action_id = nroots > 0 ? roots[best_ri].idx : 0;
    sr.action_count = nmy;
    sr.score = nroots > 0 ? roots[best_ri].worst : -1e18;
    sr.mean_score = nroots > 0 ? roots[best_ri].mean : -1e18;

    fprintf(stderr, "MAXIMIN: pairs=%d cuts=%d nmy=%d nopp=%d srch=%d scr=%d deep=%d best=%.1f\n",
            pairs, cutoffs, nmy, nopp, nsearch, nscript, deep_done, sr.score);
#if DEBUG_VERBOSE
    /* Dump top 5 root actions with scores */
    {
        int show = 5 < nroots ? 5 : nroots;
        for (int ri = 0; ri < show; ri++) {
            char abuf[512];
            render_action(&my_acts[roots[ri].idx], abuf);
            fprintf(stderr, "  #%d worst=%.1f mean=%.1f act=%s\n",
                    ri, roots[ri].worst, roots[ri].mean, abuf);
        }
    }
    /* Dump per-bird state */
    for (int i = 0; i < s->nbirds; i++) {
        if (!s->birds[i].alive) continue;
        const Bird* b = &s->birds[i];
        Coord h = bird_head(b);
        int f = bird_facing(b);
        const char* side = (b->owner == owner) ? "MY" : "OP";
        fprintf(stderr, "  %s bird%d len=%d head=(%d,%d) facing=%s",
                side, b->id, b->len, h.x, h.y,
                f == DIR_UNSET ? "?" : dir_word(f));
        /* Nearest apple */
        int best_ad = 999;
        for (int a = 0; a < s->grid.napples; a++) {
            int d = manhattan(h, s->grid.apples[a]);
            if (d < best_ad) best_ad = d;
        }
        fprintf(stderr, " apple_d=%d", best_ad);
        /* Danger */
        if (f != DIR_UNSET) {
            int ttw = turns_to_wall(s, b);
            if (ttw <= 3) fprintf(stderr, " DANGER(wall=%d)", ttw);
        }
        fprintf(stderr, "\n");
    }
    /* Eval breakdown */
    {
        int bs2[2]; body_scores(s, bs2);
        double bd2 = (double)(bs2[owner] - bs2[1 - owner]);
        double ld2 = (double)(s->losses[1 - owner] - s->losses[owner]);
        fprintf(stderr, "  EVAL: body=%.0f loss=%.0f mob=%.1f apple=%.1f stab=%.1f brk=%.1f frag=%.1f dng=%.1f grav=%.1f\n",
                EVAL_BODY * bd2, EVAL_LOSS * ld2,
                EVAL_MOBILITY * mobility_score(s, owner),
                EVAL_APPLE * apple_race(s, owner),
                EVAL_STABILITY * support_stability(s, owner),
                EVAL_BREAKPOINT * breakpoint_sc(s, owner),
                EVAL_FRAGILE * fragile_attack(s, owner),
                danger_score(s, owner),
                gravity_danger(s, owner));
    }
    /* Scripted birds info */
    for (int i = 0; i < nscript; i++) {
        fprintf(stderr, "  SCRIPT bird%d -> cmd=%d (%s)\n",
                script_ids[i], script_cmds[i],
                script_cmds[i] == -1 ? "keep" : dir_word(script_cmds[i]));
    }
#endif
    fflush(stderr);
    return sr;
}

int main(void) {
    init_weights();
    BotIO io;
    read_init(&io);
    nn_enabled = (CNN_CH1 > 0);
    srand(42);
    State prev_state;
    Action last_my = empty_action();
    bool has_prev = false;
    Frame frame;
    while (read_frame(&frame)) {
        State state = reconcile(&io, &frame, has_prev ? &prev_state : NULL,
                                has_prev ? &last_my : NULL);
        double deadline;
        clock_gettime(CLOCK_MONOTONIC, &search_start);
        if (state.turn == 0)
            deadline = (double)SEARCH_FIRST_MS;
        else
            deadline = (double)SEARCH_LATER_MS;
        SearchResult sr = choose_action_maximin(&state, io.player_idx, deadline);
        char cmd[512];
        render_action(&sr.action, cmd);
        fprintf(stderr, "T%d: %s ms=%.1f score=%.1f\n", state.turn, cmd, elapsed_ms(), sr.score);
#if DEBUG_VERBOSE
        {
            int bs2[2]; body_scores(&state, bs2);
            fprintf(stderr, "  body: me=%d opp=%d diff=%d  losses: me=%d opp=%d  apples=%d\n",
                    bs2[io.player_idx], bs2[1 - io.player_idx],
                    bs2[io.player_idx] - bs2[1 - io.player_idx],
                    state.losses[io.player_idx], state.losses[1 - io.player_idx],
                    state.grid.napples);
        }
#endif
        fflush(stderr);
        printf("%s\n", cmd);
        fflush(stdout);
        last_my = sr.action;
        prev_state = state;
        has_prev = true;
    }
    return 0;
}

// WEIGHTS_PLACEHOLDER_START
static void init_weights(void) {
}
// WEIGHTS_PLACEHOLDER_END
