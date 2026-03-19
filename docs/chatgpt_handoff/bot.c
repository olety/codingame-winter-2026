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
static int enum_joint(const State* s, int owner, Action* out, int max_actions) {
    int bids[MAX_BIRDS_PP], nb = 0;
    for (int i = 0; i < s->nbirds; i++)
        if (s->birds[i].owner == owner && s->birds[i].alive && nb < MAX_BIRDS_PP)
            bids[nb++] = s->birds[i].id;
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
static float EVAL_STABILITY = 10.0f, EVAL_BREAKPOINT = 9.0f, EVAL_FRAGILE = 8.0f, EVAL_TERMINAL = 10000.0f;
static int SEARCH_FIRST_MS = 850, SEARCH_LATER_MS = 40;
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
        double w = (double)s->birds[i].len; /* weight by body length */
        if (s->birds[i].owner == owner) total += n * w;
        else total -= n * w;
    }
    return total;
}
static double apple_race(const State* s, int owner) {
    /* Each bird gets its own apple incentive: sum of 1/(dist+1) to nearest apple.
       Short birds get a per-bird bonus so the heuristic cares about moving them. */
    double total = 0;
    for (int i = 0; i < s->nbirds; i++) {
        if (!s->birds[i].alive) continue;
        Coord h = bird_head(&s->birds[i]);
        /* Find nearest apple to this bird */
        int best_d = 999;
        for (int a = 0; a < s->grid.napples; a++) {
            int d = manhattan(h, s->grid.apples[a]);
            if (d < best_d) best_d = d;
        }
        /* Each bird contributes proportional to its proximity to food.
           Weight by (10/len) so short birds have stronger apple drive. */
        double w = 10.0 / (double)(s->birds[i].len > 0 ? s->birds[i].len : 1);
        double v = w / (double)(best_d + 1);
        if (s->birds[i].owner == owner) total += v;
        else total -= v;
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
        double v = (supported ? 1.0 : -1.0) * (double)s->birds[i].len;
        if (s->birds[i].owner == owner) score += v;
        else score -= v;
    }
    return score;
}
static double breakpoint_sc(const State* s, int owner) {
    return (double)(break_count(s, owner) - break_count(s, 1 - owner));
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
            double penalty = -30.0;
            if (s->birds[i].len <= 3) penalty = -80.0;
            if (s->birds[i].owner == owner) total += penalty;
            else total -= penalty;
        }
        /* Extra: penalize facing UP for short birds (fighting gravity) */
        if (s->birds[i].len <= 4 && bird_facing(&s->birds[i]) == DIR_N) {
            double up_penalty = -15.0;
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
        if (ttw <= 1) penalty = -200.0;       /* about to die */
        else if (ttw <= 2) penalty = -80.0;   /* 2 turns from death */
        else if (ttw <= 3) penalty = -20.0;   /* getting close */
        /* Short birds get higher penalty (death = total loss) */
        if (s->birds[i].len <= 3) penalty *= 2.0;
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
    /* Per-bird alive bonus: each alive bird is worth at least 10 body units.
       Short birds (len 3) get bonus 10, long birds (len 15+) get 0 extra. */
    double alive_bonus = 0;
    for (int i = 0; i < s->nbirds; i++) {
        if (!s->birds[i].alive) continue;
        int bonus = 10 - s->birds[i].len;
        if (bonus < 0) bonus = 0;
        if (s->birds[i].owner == owner) alive_bonus += bonus;
        else alive_bonus -= bonus;
    }
    return EVAL_BODY * (bd + alive_bonus)
         + EVAL_LOSS * ld
         + EVAL_MOBILITY * mobility_score(s, owner)
         + EVAL_APPLE * apple_race(s, owner)
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

/* Joint-Action PUCT with Additive Log-Prior
   - Enumerate all our joint actions (up to 81)
   - Prior = softmax(sum of per-bird logits) — additive, not multiplicative
   - PUCT selects which joint action to evaluate
   - Evaluate against opponent top-K (pre-ranked)
   - Backprop to the joint action's stats */

#define PUCT_C 20.0f /* scaled to relative heuristic (~±10-100) */
#define OPP_K 5

static SearchResult choose_action_puct(const State* s, int owner, double deadline_ms) {
    /* Enumerate our joint actions */
    Action my_acts[MAX_ACTIONS];
    int nmy = enum_joint(s, owner, my_acts, MAX_ACTIONS);
    if (nmy == 0) {
        SearchResult sr; sr.action = empty_action(); sr.action_id = 0;
        sr.action_count = 1; sr.score = sr.mean_score = -1e18;
        return sr;
    }

    /* Compute additive log-prior: sum per-bird logits for each joint action */
    float prior[MAX_ACTIONS];
    {
        int slot_ids[MAX_BIRDS_PP];
        int ns = bird_slot_ids(s, owner, slot_ids);
        NNPred pred;
        bool have_nn = nn_enabled;
        if (have_nn) {
            encode_features(s, owner);
            pred = nn_forward(s->grid.h, s->grid.w);
        }
        float scores[MAX_ACTIONS];
        float mx = -1e18f;
        for (int a = 0; a < nmy; a++) {
            float sc = 0;
            if (have_nn) {
                for (int si = 0; si < ns; si++) {
                    int cmd = cmd_for_bird(&my_acts[a], slot_ids[si]);
                    int pi = policy_idx_for_cmd(cmd);
                    int idx = si * POLICY_APB + pi;
                    if (idx < POLICY_OUT) sc += pred.policy[idx];
                }
            }
            scores[a] = sc;
            if (sc > mx) mx = sc;
        }
        /* Use raw additive scores as weights — just shift so min=0 and add floor */
        float mn = scores[0];
        for (int a = 1; a < nmy; a++) if (scores[a] < mn) mn = scores[a];
        float sum = 0;
        for (int a = 0; a < nmy; a++) {
            prior[a] = (scores[a] - mn) + 1.0f; /* shift to positive, +1 floor */
            sum += prior[a];
        }
        float inv = 1.0f / sum;
        for (int a = 0; a < nmy; a++) prior[a] *= inv;
    }

    /* Enumerate opponent actions and pre-rank top K */
    Action opp_acts[MAX_ACTIONS];
    int nopp = enum_joint(s, 1 - owner, opp_acts, MAX_ACTIONS);
    int opp_top[OPP_K];
    int nopp_k = OPP_K < nopp ? OPP_K : nopp;
    {
        int def_idx = 0;
        for (int a = 1; a < nmy; a++) if (prior[a] > prior[def_idx]) def_idx = a;
        Action def = my_acts[def_idx];
        double opp_sc[MAX_ACTIONS];
        for (int oi = 0; oi < nopp; oi++) {
            State nx = sim_state(s, owner, &def, &opp_acts[oi]);
            opp_sc[oi] = evaluate(&nx, owner);
        }
        bool used[MAX_ACTIONS]; memset(used, 0, sizeof(used));
        for (int k = 0; k < nopp_k; k++) {
            int bi = 0; double bv = 1e18;
            for (int oi = 0; oi < nopp; oi++)
                if (!used[oi] && opp_sc[oi] < bv) { bv = opp_sc[oi]; bi = oi; }
            opp_top[k] = bi; used[bi] = true;
        }
    }

    /* Baseline eval for relative scoring — removes "already losing" saturation */
    double baseline = evaluate(s, owner);

    /* Per-bird PUCT stats — each bird selects independently, evaluate jointly */
    int bids[MAX_BIRDS_PP]; int nb = 0;
    int bcmds[MAX_BIRDS_PP][5], bncmds[MAX_BIRDS_PP];
    float bprior[MAX_BIRDS_PP][5];
    int bvisits[MAX_BIRDS_PP][5]; float btotal[MAX_BIRDS_PP][5];
    int btotal_visits[MAX_BIRDS_PP];
    {
        int slot_ids[MAX_BIRDS_PP];
        int ns = bird_slot_ids(s, owner, slot_ids);
        for (int i = 0; i < s->nbirds; i++)
            if (s->birds[i].owner == owner && s->birds[i].alive && nb < MAX_BIRDS_PP)
                bids[nb++] = s->birds[i].id;
        NNPred pred;
        bool have_nn = nn_enabled;
        if (have_nn) { encode_features(s, owner); pred = nn_forward(s->grid.h, s->grid.w); }
        for (int b = 0; b < nb; b++) {
            bncmds[b] = ordered_cmds(s, bids[b], bcmds[b]);
            memset(bvisits[b], 0, sizeof(bvisits[b]));
            memset(btotal[b], 0, sizeof(btotal[b]));
            btotal_visits[b] = 0;
            /* NN prior per bird */
            int slot = -1;
            if (have_nn) for (int si = 0; si < ns; si++) if (slot_ids[si] == bids[b]) { slot = si; break; }
            /* Uniform prior — 8ch NN policy puts 91% on keep, useless for search */
            {
                float u = 1.0f / bncmds[b];
                for (int i = 0; i < bncmds[b]; i++) bprior[b][i] = u;
            }
        }
    }

    int iterations = 0;
    for (;;) {
        if ((iterations & 15) == 0 && expired(deadline_ms)) break;

        /* Each bird selects via PUCT independently */
        Action my_act; my_act.n = nb;
        int chosen[MAX_BIRDS_PP];
        for (int b = 0; b < nb; b++) {
            int best_a = 0; float best_sc = -1e18f;
            float sq = sqrtf((float)btotal_visits[b] + 1e-8f);
            for (int a = 0; a < bncmds[b]; a++) {
                float q = (bvisits[b][a] > 0) ? btotal[b][a] / bvisits[b][a] : 0.0f;
                float u = PUCT_C * bprior[b][a] * sq / (1.0f + bvisits[b][a]);
                float sc = q + u;
                if (sc > best_sc) { best_sc = sc; best_a = a; }
            }
            chosen[b] = best_a;
            my_act.bird_id[b] = bids[b];
            my_act.cmd[b] = bcmds[b][best_a];
        }

        /* Stochastic opponent: 1 random + 1 top → pessimistic */
        int oi_rand = rand() % nopp;
        State n1 = sim_state(s, owner, &my_act, &opp_acts[oi_rand]);
        double v1 = evaluate(&n1, owner) - baseline;
        State n2 = sim_state(s, owner, &my_act, &opp_acts[opp_top[0]]);
        double v2 = evaluate(&n2, owner) - baseline;
        float val = (float)(v1 < v2 ? v1 : v2);

        /* Backprop to EACH bird's chosen action */
        for (int b = 0; b < nb; b++) {
            bvisits[b][chosen[b]]++;
            btotal[b][chosen[b]] += val;
            btotal_visits[b]++;
        }
        iterations++;
    }

    /* Compose: PUCT result for long birds, greedy apple-chase for short birds */
    Action best_act; best_act.n = nb;
    for (int b = 0; b < nb; b++) {
        const Bird* bird = NULL;
        for (int i = 0; i < s->nbirds; i++)
            if (s->birds[i].id == bids[b]) { bird = &s->birds[i]; break; }

        if (bird && bird->len <= 4 && s->grid.napples > 0) {
            /* Short bird: greedily pick command that moves closest to nearest apple */
            Coord head = bird_head(bird);
            int best_cmd = bcmds[b][0];
            int best_dist = 9999;
            for (int a = 0; a < bncmds[b]; a++) {
                int cmd = bcmds[b][a];
                /* Compute new head position for this command */
                int facing = bird_facing(bird);
                int new_dir = cmd;
                if (cmd == -1) new_dir = facing; /* keep = continue facing */
                if (new_dir < 0 || new_dir > 3) continue;
                Coord delta = dir_delta(new_dir);
                Coord new_head = coord(head.x + delta.x, head.y + delta.y);
                /* Check if it's a wall/invalid — skip */
                if (!grid_valid(&s->grid, new_head) || s->grid.walls[new_head.y][new_head.x]) continue;
                /* Find distance to nearest apple */
                for (int ap = 0; ap < s->grid.napples; ap++) {
                    int d = manhattan(new_head, s->grid.apples[ap]);
                    if (d < best_dist) { best_dist = d; best_cmd = cmd; }
                }
            }
            best_act.bird_id[b] = bids[b];
            best_act.cmd[b] = best_cmd;
        } else {
            /* Long bird: use PUCT result */
            int ba = 0;
            for (int a = 1; a < bncmds[b]; a++) if (bvisits[b][a] > bvisits[b][ba]) ba = a;
            best_act.bird_id[b] = bids[b];
            best_act.cmd[b] = bcmds[b][ba];
        }
    }

    /* Debug */
    fprintf(stderr, "PUCT: iters=%d nb=%d nopp=%d\n", iterations, nb, nopp);
    for (int b = 0; b < nb; b++) {
        fprintf(stderr, "  b%d(%d):", b, bids[b]);
        for (int a = 0; a < bncmds[b]; a++) {
            float q = bvisits[b][a] > 0 ? btotal[b][a]/bvisits[b][a] : 0;
            fprintf(stderr, " %d:N=%d,Q=%.1f,P=%.2f", bcmds[b][a], bvisits[b][a], q, bprior[b][a]);
        }
        fprintf(stderr, "\n");
    }
    fflush(stderr);

    SearchResult sr;
    sr.action = best_act;
    sr.action_id = 0;
    sr.action_count = nmy;
    sr.score = 0; sr.mean_score = 0;
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
        SearchResult sr = choose_action_puct(&state, io.player_idx, deadline);
        char cmd[512];
        render_action(&sr.action, cmd);
        fprintf(stderr, "T%d: %s ms=%.1f\n", state.turn, cmd, elapsed_ms());
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
