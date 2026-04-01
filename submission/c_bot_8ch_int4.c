#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
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
    out[n++] = -1;
    if (facing == DIR_UNSET) {
        for (int d = 0; d < 4; d++) out[n++] = d;
    } else {
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
static float EVAL_BODY = 120.0f, EVAL_LOSS = 18.0f, EVAL_MOBILITY = 7.5f, EVAL_APPLE = 16.0f;
static float EVAL_STABILITY = 10.0f, EVAL_BREAKPOINT = 9.0f, EVAL_FRAGILE = 8.0f, EVAL_TERMINAL = 10000.0f;
static float PRIOR_MIX = 0.0f, LEAF_MIX = 0.08f, VALUE_SCALE = 48.0f;
static int PRIOR_DEPTH_LIMIT = 0, LEAF_DEPTH_LIMIT = 1;
static int SEARCH_FIRST_MS = 850, SEARCH_LATER_MS = 40;
static int DEEPEN_TOP_MY = 6, DEEPEN_TOP_OPP = 6, DEEPEN_CHILD_MY = 4, DEEPEN_CHILD_OPP = 4;
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
    Coord my_h[MAX_BIRDS_PP], op_h[MAX_BIRDS_PP];
    int nm = 0, no = 0;
    for (int i = 0; i < s->nbirds; i++) {
        if (!s->birds[i].alive) continue;
        if (s->birds[i].owner == owner) { if (nm < MAX_BIRDS_PP) my_h[nm++] = bird_head(&s->birds[i]); }
        else { if (no < MAX_BIRDS_PP) op_h[no++] = bird_head(&s->birds[i]); }
    }
    double total = 0;
    for (int i = 0; i < s->grid.napples; i++) {
        int md = min_dist(s->grid.apples[i], my_h, nm);
        int od = min_dist(s->grid.apples[i], op_h, no);
        int denom = (md + od + 1);
        if (denom < 1) denom = 1;
        total += (double)(od - md) / (double)denom;
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
static double evaluate(const State* s, int owner) {
    if (is_terminal(s, CONTEST_MAX_TURNS)) {
        int bs[2]; body_scores(s, bs);
        int bd = bs[owner] - bs[1 - owner];
        int ld = s->losses[1 - owner] - s->losses[owner];
        return EVAL_TERMINAL * (double)bd + (double)ld;
    }
    int bs[2]; body_scores(s, bs);
    double bd = (double)(bs[owner] - bs[1 - owner]);
    double ld = (double)(s->losses[1 - owner] - s->losses[owner]);
    return EVAL_BODY * bd
         + EVAL_LOSS * ld
         + EVAL_MOBILITY * mobility_score(s, owner)
         + EVAL_APPLE * apple_race(s, owner)
         + EVAL_STABILITY * support_stability(s, owner)
         + EVAL_BREAKPOINT * breakpoint_sc(s, owner)
         + EVAL_FRAGILE * fragile_attack(s, owner);
}

/* Hybrid */
static double nn_leaf_bonus(const NNPred* pred) {
    return (double)pred->value * VALUE_SCALE;
}
static double eval_with_hybrid(const State* s, int owner, int depth) {
    double sc = evaluate(s, owner);
    if (nn_enabled && LEAF_MIX != 0.0 && depth <= LEAF_DEPTH_LIMIT) {
        encode_features(s, owner);
        NNPred pred = nn_forward(s->grid.h, s->grid.w);
        sc += LEAF_MIX * nn_leaf_bonus(&pred);
    }
    return sc;
}
static int policy_idx_for_cmd(int cmd) {
    if (cmd == -1) return 0;
    return cmd + 1;
}
static double action_prior_nn(const State* s, int owner, const Action* act, const NNPred* pred) {
    int slot_ids[MAX_BIRDS_PP], ns = bird_slot_ids(s, owner, slot_ids);
    double total = 0;
    for (int si = 0; si < ns; si++) {
        int cmd = cmd_for_bird(act, slot_ids[si]);
        bool alive = false;
        for (int i = 0; i < s->nbirds; i++)
            if (s->birds[i].id == slot_ids[si] && s->birds[i].alive) { alive = true; break; }
        if (!alive) continue;
        int pi = policy_idx_for_cmd(cmd);
        int idx = si * POLICY_APB + pi;
        if (idx < POLICY_OUT) total += (double)pred->policy[idx];
    }
    return total;
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
typedef struct { int idx; double score; } IdxScore;
static int cmp_desc(const void* a, const void* b) {
    double da = ((IdxScore*)a)->score, db = ((IdxScore*)b)->score;
    if (da > db) return -1;
    if (da < db) return 1;
    return 0;
}
static int cmp_asc(const void* a, const void* b) {
    double da = ((IdxScore*)a)->score, db = ((IdxScore*)b)->score;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
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
static double action_prior_score(const State* s, int owner,
                                  const Action* my, const Action* opp,
                                  const NNPred* pred, int depth) {
    State next = sim_state(s, owner, my, opp);
    double sc = eval_with_hybrid(&next, owner, depth + 1);
    if (pred && PRIOR_MIX != 0.0) {
        sc += PRIOR_MIX * action_prior_nn(s, owner, my, pred);
    }
    return sc;
}
static double deepen_branch(const State* s, int owner, double deadline_ms,
                             long* extra_remaining, int depth) {
    Action my_acts[MAX_ACTIONS], opp_acts[MAX_ACTIONS];
    int nmy = enum_joint(s, owner, my_acts, MAX_ACTIONS);
    int nopp = enum_joint(s, 1 - owner, opp_acts, MAX_ACTIONS);
    Action empty = empty_action();
    NNPred pred_buf;
    NNPred* pred = NULL;
    if (nn_enabled && PRIOR_MIX != 0.0 && depth <= PRIOR_DEPTH_LIMIT) {
        encode_features(s, owner);
        pred_buf = nn_forward(s->grid.h, s->grid.w);
        pred = &pred_buf;
    }
    IdxScore my_ord[MAX_ACTIONS];
    for (int i = 0; i < nmy; i++) {
        my_ord[i].idx = i;
        my_ord[i].score = action_prior_score(s, owner, &my_acts[i], &empty, pred, depth);
    }
    qsort(my_ord, nmy, sizeof(IdxScore), cmp_desc);
    IdxScore opp_ord[MAX_ACTIONS];
    for (int i = 0; i < nopp; i++) {
        opp_ord[i].idx = i;
        opp_ord[i].score = action_prior_score(s, owner, &empty, &opp_acts[i], NULL, depth);
    }
    qsort(opp_ord, nopp, sizeof(IdxScore), cmp_asc);
    int child_my = DEEPEN_CHILD_MY < nmy ? DEEPEN_CHILD_MY : nmy;
    int child_opp = DEEPEN_CHILD_OPP < nopp ? DEEPEN_CHILD_OPP : nopp;
    double best = -1e18;
    for (int mi = 0; mi < child_my; mi++) {
        if (expired(deadline_ms) || *extra_remaining <= 0) break;
        double worst = 1e18;
        for (int oi = 0; oi < child_opp; oi++) {
            if (expired(deadline_ms) || *extra_remaining <= 0) break;
            (*extra_remaining)--;
            State next = sim_state(s, owner, &my_acts[my_ord[mi].idx], &opp_acts[opp_ord[oi].idx]);
            double sc = eval_with_hybrid(&next, owner, depth + 1);
            if (sc < worst) worst = sc;
        }
        if (worst < 1e17 && worst > best) best = worst;
    }
    return best > -1e17 ? best : eval_with_hybrid(s, owner, depth);
}
typedef struct {
    int action_id;
    double worst, mean;
    double resp_scores[MAX_ACTIONS];
    int resp_opp_idx[MAX_ACTIONS];
    int nresp;
} RootAnalysis;
static void refresh_analysis(RootAnalysis* a) {
    if (a->nresp == 0) { a->worst = a->mean = -1e18; return; }
    a->worst = 1e18; a->mean = 0;
    for (int i = 0; i < a->nresp; i++) {
        if (a->resp_scores[i] < a->worst) a->worst = a->resp_scores[i];
        a->mean += a->resp_scores[i];
    }
    a->mean /= a->nresp;
}
typedef struct {
    Action action;
    int action_id, action_count;
    double score, mean_score;
} SearchResult;
static SearchResult choose_action(const State* s, int owner, double deadline_ms) {
    Action my_acts[MAX_ACTIONS], opp_acts[MAX_ACTIONS];
    int nmy = enum_joint(s, owner, my_acts, MAX_ACTIONS);
    int nopp = enum_joint(s, 1 - owner, opp_acts, MAX_ACTIONS);
    Action empty = empty_action();
    bool allow_cutoff = (deadline_ms > 0);
    long extra_remaining = 100000000L;
    NNPred pred_buf;
    NNPred* pred = NULL;
    if (nn_enabled && PRIOR_MIX != 0.0 && PRIOR_DEPTH_LIMIT >= 0) {
        encode_features(s, owner);
        pred_buf = nn_forward(s->grid.h, s->grid.w);
        pred = &pred_buf;
    }
    IdxScore my_ord[MAX_ACTIONS];
    for (int i = 0; i < nmy; i++) {
        my_ord[i].idx = i;
        my_ord[i].score = action_prior_score(s, owner, &my_acts[i], &empty, pred, 0);
    }
    qsort(my_ord, nmy, sizeof(IdxScore), cmp_desc);
    IdxScore opp_ord[MAX_ACTIONS];
    for (int i = 0; i < nopp; i++) {
        opp_ord[i].idx = i;
        opp_ord[i].score = action_prior_score(s, owner, &empty, &opp_acts[i], NULL, 0);
    }
    qsort(opp_ord, nopp, sizeof(IdxScore), cmp_asc);
    RootAnalysis analyses[MAX_ACTIONS];
    int nanalyses = 0;
    double best_shallow_worst = -1e18;
    for (int mi = 0; mi < nmy; mi++) {
        if (expired(deadline_ms)) break;
        int my_i = my_ord[mi].idx;
        RootAnalysis* ra = &analyses[nanalyses];
        ra->action_id = my_i;
        ra->nresp = 0;
        double worst = 1e18;
        int evaluated = 0;
        bool cutoff = false;
        for (int oi = 0; oi < nopp; oi++) {
            if (expired(deadline_ms)) { cutoff = true; break; }
            int opp_i = opp_ord[oi].idx;
            State next = sim_state(s, owner, &my_acts[my_i], &opp_acts[opp_i]);
            double sc = eval_with_hybrid(&next, owner, 1);
            ra->resp_opp_idx[ra->nresp] = opp_i;
            ra->resp_scores[ra->nresp] = sc;
            ra->nresp++;
            if (sc < worst) worst = sc;
            evaluated++;
            if (allow_cutoff && worst < best_shallow_worst) { cutoff = true; break; }
        }
        if (evaluated == 0) continue;
        if (cutoff && expired(deadline_ms) && mi == 0) { }
        refresh_analysis(ra);
        if (ra->worst > best_shallow_worst) best_shallow_worst = ra->worst;
        nanalyses++;
    }
    if (nanalyses == 0) {
        SearchResult sr;
        sr.action = empty_action();
        sr.action_id = 0;
        sr.action_count = nmy > 0 ? nmy : 1;
        sr.score = sr.mean_score = -1e18;
        return sr;
    }
    for (int i = 0; i < nanalyses - 1; i++)
        for (int j = i + 1; j < nanalyses; j++) {
            bool swap = analyses[j].worst > analyses[i].worst
                || (analyses[j].worst == analyses[i].worst && analyses[j].mean > analyses[i].mean);
            if (swap) { RootAnalysis tmp = analyses[i]; analyses[i] = analyses[j]; analyses[j] = tmp; }
        }
    int dtm = DEEPEN_TOP_MY < nanalyses ? DEEPEN_TOP_MY : nanalyses;
    for (int ai = 0; ai < dtm; ai++) {
        if (expired(deadline_ms)) break;
        RootAnalysis* ra = &analyses[ai];
        int ridx[MAX_ACTIONS];
        for (int i = 0; i < ra->nresp; i++) ridx[i] = i;
        for (int i = 0; i < ra->nresp - 1; i++)
            for (int j = i + 1; j < ra->nresp; j++)
                if (ra->resp_scores[ridx[j]] < ra->resp_scores[ridx[i]])
                    { int t = ridx[i]; ridx[i] = ridx[j]; ridx[j] = t; }
        int dto = DEEPEN_TOP_OPP < ra->nresp ? DEEPEN_TOP_OPP : ra->nresp;
        for (int ri = 0; ri < dto; ri++) {
            if (expired(deadline_ms)) break;
            int resp_i = ridx[ri];
            State next = sim_state(s, owner, &my_acts[ra->action_id], &opp_acts[ra->resp_opp_idx[resp_i]]);
            double child_sc = deepen_branch(&next, owner, deadline_ms, &extra_remaining, 1);
            ra->resp_scores[resp_i] = child_sc;
        }
        refresh_analysis(ra);
    }
    int best_i = 0;
    for (int i = 1; i < nanalyses; i++) {
        if (analyses[i].worst > analyses[best_i].worst
            || (analyses[i].worst == analyses[best_i].worst && analyses[i].mean > analyses[best_i].mean))
            best_i = i;
    }
    SearchResult sr;
    sr.action = my_acts[analyses[best_i].action_id];
    sr.action_id = analyses[best_i].action_id;
    sr.action_count = nmy > 0 ? nmy : 1;
    sr.score = analyses[best_i].worst;
    sr.mean_score = analyses[best_i].mean;
    return sr;
}

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

/* Main */
int main(void) {
    init_weights();
    BotIO io;
    read_init(&io);
    nn_enabled = (CNN_CH1 > 0 && (PRIOR_MIX != 0.0f || LEAF_MIX != 0.0f));
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
        SearchResult sr = choose_action(&state, io.player_idx, deadline);
        char cmd[512];
        render_action(&sr.action, cmd);
        printf("%s\n", cmd);
        fflush(stdout);
        last_my = sr.action;
        prev_state = state;
        has_prev = true;
    }
    return 0;
}


/* --- Weight decoder --- */
static int _next_cp(const char **p) {
    unsigned char c = (unsigned char)**p; (*p)++;
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0) { int r = (c & 0x1F) << 6; r |= (*(*p)++ & 0x3F); return r; }
    int r = (c & 0x0F) << 12; r |= (*(*p)++ & 0x3F) << 6; r |= (*(*p)++ & 0x3F); return r;
}
static int _next_u16(const char **p) {
    int cp = _next_cp(p);
    if (cp == 0xFFFF) { int cp2 = _next_cp(p); return (cp2 == 0x0800) ? 0xFFFF : cp2 + 0xD800; }
    return cp;
}
static void decode_int4(const char *data, int8_t *out, int count) {
    const char *p = data;
    int w = 0;
    while (w < count) {
        int u = _next_u16(&p);
        int b0 = u & 0xFF, b1 = (u >> 8) & 0xFF;
        int n0 = b0 & 0xF, n1 = (b0 >> 4) & 0xF, n2 = b1 & 0xF, n3 = (b1 >> 4) & 0xF;
        out[w++] = (int8_t)((n0 & 8) ? (n0 | 0xF0) : n0); if (w >= count) break;
        out[w++] = (int8_t)((n1 & 8) ? (n1 | 0xF0) : n1); if (w >= count) break;
        out[w++] = (int8_t)((n2 & 8) ? (n2 | 0xF0) : n2); if (w >= count) break;
        out[w++] = (int8_t)((n3 & 8) ? (n3 | 0xF0) : n3);
    }
}
static void decode_f32(const char *data, float *out, int count) {
    const char *p = data;
    /* 2 u16 values per f32 */
    for (int i = 0; i < count; i++) {
        int lo = _next_u16(&p);
        int hi = _next_u16(&p);
        unsigned int bits = (unsigned int)lo | ((unsigned int)hi << 16);
        memcpy(&out[i], &bits, 4);
    }
}

static const char CONV1_W_ENC[] = "\0\0￰࿿῰Āက−∢\x10ခ⇱⌢ĢĀᄡᄁᄑ\x10ᄁ￿Ǒ쪺࿀\x0fȂကĀðà\x1f࿱\x10ༀခĀð？￲\x10ñ鄀ༀ\x1fਁĊā伀퓞＿ự탞ⷞĐ㳰쌌๞Ⱑꊪഎ⿲\x0f￿܀ꇐﬁà฀￪ఏǑꄍﬁǡ퀍กơఏრ\x0e਀à\x01낱\0ᄁ˱ტ쿀㻡Ỡᇾ꾟￿М䷡ᷠఎþ฀რ໰\0଀\x0eༀჰ쀜ﻰðð\0\0ﴀက\0쵽揇༯︠\0࿟信￞￿ܮＯ￟￐\0ÿༀ\0￱？\x0fðἏ࿰＀ð\0꼐ð\x10ἁԎ༐ਏἉ？ἂ༏﷼ἂԏ＀ৰ˹พ︃ǿﰋ࿱ሟ\0ༀǿ\x0f\x11／῱ሟ\0ༀ﻿῟\0࿯\0෱\rခᰀ폝Ḿ넚ḑ⌳￿ˑᄑ㌏ೳ쇱ᇉ༁⿾￿ࠀ࿾￿܏ༀð\0ᇱ\0ༀༀ\0／ðༀ\0＀ༀï\0\x01폠틟￡ﰌữ￿ࠀ￿ࠀ坐啦텅က컜︀Ἔð\0ǿက\0ἐ\0\0\x1fჰ\0\0¡\0\0ਚ\x01ᄐ念἟켎㯑폫཯⸁ꆟ魺沒쾽뽟￿ߏἿ촁ċ¡퀋฀ჰ逎૱ÒฑƑ฀ǡ￿܎ฟà༁";
static const char CONV1_S_ENC[] = "ᨸ㽖܈㽘지㽋悃㽹⴮㾀唟㽳㽞崂㼅";
static const char CONV1_B_ENC[] = "㻣㼰盪뼔Ź뼪ၝ뽋￿с뽛뼼麌뼏惥뽻";
static const char CONV2_W_ENC[] = "ㄲ∄\x11ᇰ῱俞甥♃\x10ᄒጱሳ攂䕄≃\x11ခ\0∯ℏሡ귭ꦻ∢™ᄡ∢Ēထㄒ㌣⌢￿ࠀ࿿⌐࿿匀ᐱ∠䕶晖ą࿯ℏ∡刑南㑃﻿Ȯ元䉅̑\0㋰䕒啶䑆㄃￿ۭᇌȐ㈳䌵ሒ࿿ᄟ⇽ĐĢ꼎’∢∁∢ကሒ\x02㄀∣∡\0／ğ℡吢㐴‑㌢㌲⍃㈣몙ᆩჲ䄀㌴￿܏ሞ䄲῾\x11ᄐ䌳㈴剢䐤瘳噤㑦ᄡထ￿ۮ┿䅂䍆à∠噶䑆ῤȢㄟ㔢匳㌷ဂ";
static const char CONV2_S_ENC[] = "楋㹓浦㻶㸥ᄰ㸩㻐噳㺜ኻ㹞逺㸞";
static const char CONV2_B_ENC[] = "琰㺪胉뼪搳뾌뾉ᢟ뾍ᩘ뼡ﾳ뾐￿׊뽰";
static const char CONV3_W_ENC[] = "／࿿믎뮻ƻⳭᄑ￱骫뮩︋ﻯ䏮䔳㍃䌣Ȓ￟ဏჿ ￿ې㌳⍂鮹ኩ㈳ሣሲ℔࿿ሟ∢䌒⌳Ȳ샾ᄡ™궫볉ﳍ냽\0\0짐ￛ࿰࿿တထ∯⌢￿آ컞춮귉⌬ሲ\x11ᄑကሑထᄑĒሐᄢ഑Ⴠᄡᄂ￮ÿ￿א겚㋊∣̳ထᄡđᄀᄑᄐ༑࿰\0đခ췜￿ۯ￿Ҿ￿Ӎ哭㈵䑵−䑲∳吡ጣ㄀ℴᄐℤȤ䐲鳋뮫Ȭ䀢ሴ\x0f䄒∔∲ဣᇰđ㌏ဂ㑔ሤ䐱";
static const char CONV3_S_ENC[] = "傅㺬ప㸿큘㺨돍㼈枺㻒侞㺡㠯㶦䪡㷶";
static const char CONV3_B_ENC[] = "ࠞ뾈￿ȏ뼘勪뾑沲뿓땺㻑ᝠ뻺妞뼬ꢤ뾇";
static const char POLICY_W_ENC[] = "邐？တ瀀ⷐ邠ğĀ缀ⷐðဇတऀ\0\0ἀ鼽ᄡऀ\0\0 鼫뮐㈏Ē瓭鏡ᄀǟ\09\x0f鈀ᄀ\0)\x0f\0܁ဂ\0\t\x0fက㸀ჰ\0ù\0⼀㼟쨉⿠";
static const char POLICY_S_ENC[] = "쒋㼀㓭㽄㜁㺻튗㽦ㅚ㻊j㼈踁㾦͕㶖⭠㾪吚㶉躊㻲㽔㽊鋅㽕槟㽵ᣒ㻕百㾅纯㶬췙㾕棪㶈";
static const char POLICY_B_ENC[] = "\x1e㵺٤㿌䩐뽷亼㵆Ό뾎汻뼚χ㾖㒟빂椖븓쯖빯鲮뼓黶뺭썙㼼뽓딮㽧터뻀탨㾘ꁒ㸌⮅붘쩻㲍";
static const char VALUE_W_ENC[] = "\x01\0pð";
static const char VALUE_S_ENC[] = "᪙㺷";
static const char VALUE_B_ENC[] = "裎㬴";
static void init_weights(void) {
    CNN_CH1 = 8;
    CNN_CH2 = 8;
    CNN_CH3 = 8;
    decode_int4(CONV1_W_ENC, conv1_w, 1368);
    decode_f32(CONV1_S_ENC, conv1_s, 8);
    decode_f32(CONV1_B_ENC, conv1_b, 8);
    decode_int4(CONV2_W_ENC, conv2_w, 576);
    decode_f32(CONV2_S_ENC, conv2_s, 8);
    decode_f32(CONV2_B_ENC, conv2_b, 8);
    decode_int4(CONV3_W_ENC, conv3_w, 576);
    decode_f32(CONV3_S_ENC, conv3_s, 8);
    decode_f32(CONV3_B_ENC, conv3_b, 8);
    { int8_t _tmp[280]; float _sc[20];
      decode_int4(POLICY_W_ENC, _tmp, 280);
      decode_f32(POLICY_S_ENC, _sc, 20);
      for (int o=0; o<20; o++)
        for (int i=0; i<14; i++)
          policy_w[o*14+i] = (float)_tmp[o*14+i] * _sc[o];
      decode_f32(POLICY_B_ENC, policy_b, 20); }
    { int8_t _tmp[14]; float _sc[1];
      decode_int4(VALUE_W_ENC, _tmp, 14);
      decode_f32(VALUE_S_ENC, _sc, 1);
      for (int o=0; o<1; o++)
        for (int i=0; i<14; i++)
          value_w[o*14+i] = (float)_tmp[o*14+i] * _sc[o];
      decode_f32(VALUE_B_ENC, value_b, 1); }
    EVAL_BODY = 120.0f;
    EVAL_LOSS = 18.0f;
    EVAL_MOBILITY = 7.5f;
    EVAL_APPLE = 16.0f;
    EVAL_STABILITY = 10.0f;
    EVAL_BREAKPOINT = 9.0f;
    EVAL_FRAGILE = 8.0f;
    EVAL_TERMINAL = 10000.0f;
    PRIOR_MIX = 0.0f;
    LEAF_MIX = 0.08f;
    VALUE_SCALE = 48.0f;
    PRIOR_DEPTH_LIMIT = 0;
    LEAF_DEPTH_LIMIT = 1;
    SEARCH_FIRST_MS = 850;
    SEARCH_LATER_MS = 40;
    DEEPEN_TOP_MY = 6;
    DEEPEN_TOP_OPP = 6;
    DEEPEN_CHILD_MY = 4;
    DEEPEN_CHILD_OPP = 4;
}

