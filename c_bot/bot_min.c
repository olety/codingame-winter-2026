#pragma GCC optimize("O3,unroll-loops,fast-math")
#pragma GCC target("avx2,fma")
#define S static
#define SI static inline
#define R return
#define FI for(int
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
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
typedef struct { int x, y; } Coord;
SI Coord coord(int x, int y) { R (Coord){x, y}; }
SI Coord coord_add(Coord a, Coord b) { R coord(a.x+b.x, a.y+b.y); }
SI int manhattan(Coord a, Coord b) { R abs(a.x-b.x)+abs(a.y-b.y); }
S const int DX[5] = {0,1,0,-1,0};
S const int DY[5] = {-1,0,1,0,0};
SI Coord dir_delta(int d) { R coord(DX[d], DY[d]); }
SI int dir_opposite(int d) {
S const int opp[5] = {DIR_S, DIR_W, DIR_N, DIR_E, DIR_UNSET};
R opp[d];
}
SI int dir_from_coord(Coord c) {
if (c.x==0 && c.y==-1) R DIR_N;
if (c.x==1 && c.y==0) R DIR_E;
if (c.x==0 && c.y==1) R DIR_S;
if (c.x==-1 && c.y==0) R DIR_W;
R DIR_UNSET;
}
SI int turn_left(int d) {
S const int tl[5] = {DIR_W, DIR_N, DIR_E, DIR_S, DIR_UNSET};
R tl[d];
}
SI int turn_right(int d) {
S const int tr[5] = {DIR_E, DIR_S, DIR_W, DIR_N, DIR_UNSET};
R tr[d];
}
S const char* dir_word(int d) {
S const char* w[5] = {"UP","RIGHT","DOWN","LEFT","UP"};
R w[d];
}
typedef struct {
int w, h;
bool walls[MAX_H][MAX_W];
Coord apples[MAX_APPLES];
int napples;
} Grid;
SI bool grid_valid(const Grid* g, Coord c) {
R c.x>=0 && c.x<g->w && c.y>=0 && c.y<g->h;
}
SI Coord grid_opposite(const Grid* g, Coord c) {
R coord(g->w - 1 - c.x, c.y);
}
S int apple_index(const Grid* g, Coord c) {
FI i = 0; i < g->napples; i++)
if (g->apples[i].x == c.x && g->apples[i].y == c.y) R i;
R -1;
}
S void remove_apple(Grid* g, int idx) {
g->apples[idx] = g->apples[--g->napples];
}
typedef struct {
int id, owner;
Coord body[MAX_BODY];
int head, tail, len;
bool alive;
int dir;
} Bird;
SI Coord bird_head(const Bird* b) { R b->body[b->head]; }
SI int bird_facing(const Bird* b) {
if (b->len < 2) R DIR_UNSET;
Coord h = b->body[b->head];
int ni = (b->head + 1) % MAX_BODY;
Coord n = b->body[ni];
R dir_from_coord(coord(h.x - n.x, h.y - n.y));
}
SI void bird_push_front(Bird* b, Coord c) {
b->head = (b->head - 1 + MAX_BODY) % MAX_BODY;
b->body[b->head] = c;
b->len++;
}
SI void bird_pop_back(Bird* b) {
b->tail = (b->tail - 1 + MAX_BODY) % MAX_BODY;
b->len--;
}
SI void bird_pop_front(Bird* b) {
b->head = (b->head + 1) % MAX_BODY;
b->len--;
}
SI Coord bird_seg(const Bird* b, int i) {
R b->body[(b->head + i) % MAX_BODY];
}
S bool bird_contains(const Bird* b, Coord c) {
FI i = 0; i < b->len; i++)
if (bird_seg(b, i).x == c.x && bird_seg(b, i).y == c.y) R true;
R false;
}
typedef struct {
Grid grid;
Bird birds[MAX_BIRDS];
int nbirds;
int losses[2];
int turn;
} State;
S int body_scores(const State* s, int scores[2]) {
scores[0] = scores[1] = 0;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].alive) scores[s->birds[i].owner] += s->birds[i].len;
R 0;
}
S void final_scores(const State* s, int out[2]) {
body_scores(s, out);
if (out[0] == out[1]) {
out[0] -= s->losses[0];
out[1] -= s->losses[1];
}
}
S bool is_game_over(const State* s) {
if (s->grid.napples == 0) R true;
FI p = 0; p < 2; p++) {
bool any = false;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].owner == p && s->birds[i].alive) { any = true; break; }
if (!any) R true;
}
R false;
}
S bool is_terminal(const State* s, int max_turns) {
R is_game_over(s) || s->turn >= max_turns;
}
typedef struct {
int bird_id[MAX_BIRDS_PP];
int cmd[MAX_BIRDS_PP];
int n;
} Action;
S int legal_cmds(const State* s, int bird_id, int out[5]) {
int n = 0;
const Bird* b = NULL;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].id == bird_id && s->birds[i].alive) { b = &s->birds[i]; break; }
if (!b) R 0;
int facing = bird_facing(b);
out[n++] = -1;
if (facing == DIR_UNSET) {
FI d = 0; d < 4; d++) out[n++] = d;
} else {
FI d = 0; d < 4; d++) {
if (d == facing) continue;
if (d == dir_opposite(facing)) continue;
out[n++] = d;
}
}
R n;
}
S int ordered_cmds(const State* s, int bird_id, int out[5]) {
const Bird* b = NULL;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].id == bird_id && s->birds[i].alive) { b = &s->birds[i]; break; }
if (!b) R 0;
int facing = bird_facing(b);
int n = 0;
if (facing == DIR_UNSET) {
FI d = 0; d < 4; d++) out[n++] = d;
} else {
out[n++] = -1;
out[n++] = turn_left(facing);
out[n++] = turn_right(facing);
}
R n;
}
S int enum_joint(const State* s, int owner, Action* out, int max_actions) {
int bids[MAX_BIRDS_PP], nb = 0;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].owner == owner && s->birds[i].alive && nb < MAX_BIRDS_PP)
bids[nb++] = s->birds[i].id;
if (nb == 0) {
out[0].n = 0;
R 1;
}
int cmds[MAX_BIRDS_PP][5], ncmds[MAX_BIRDS_PP];
FI i = 0; i < nb; i++)
ncmds[i] = ordered_cmds(s, bids[i], cmds[i]);
int total = 1;
FI i = 0; i < nb; i++) total *= ncmds[i];
if (total > max_actions) total = max_actions;
int idx[MAX_BIRDS_PP];
memset(idx, 0, sizeof(idx));
FI a = 0; a < total; a++) {
out[a].n = nb;
FI i = 0; i < nb; i++) {
out[a].bird_id[i] = bids[i];
out[a].cmd[i] = cmds[i][idx[i]];
}
FI i = nb - 1; i >= 0; i--) {
idx[i]++;
if (idx[i] < ncmds[i]) break;
idx[i] = 0;
}
}
R total;
}
S int cmd_for_bird(const Action* a, int bird_id) {
FI i = 0; i < a->n; i++)
if (a->bird_id[i] == bird_id) R a->cmd[i];
R -1;
}
S bool body_in_list(const Coord* body, int len, Coord c) {
FI i = 0; i < len; i++)
if (body[i].x == c.x && body[i].y == c.y) R true;
R false;
}
S bool solid_under(const State* s, Coord c, const Coord* ignore, int nignore) {
Coord below = coord(c.x, c.y + 1);
if (body_in_list(ignore, nignore, below)) R false;
if (grid_valid(&s->grid, below) && s->grid.walls[below.y][below.x]) R true;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].alive && bird_contains(&s->birds[i], below)) R true;
if (apple_index(&s->grid, below) >= 0) R true;
R false;
}
S void shift_bird_down(Bird* b) {
FI i = 0; i < b->len; i++) {
int idx = (b->head + i) % MAX_BODY;
b->body[idx].y += 1;
}
}
S int collect_body(const Bird* b, Coord* out) {
FI i = 0; i < b->len; i++)
out[i] = bird_seg(b, i);
R b->len;
}
S bool apply_individual_falls(State* s) {
bool moved = false;
Coord buf[MAX_BODY];
FI i = 0; i < s->nbirds; i++) {
if (!s->birds[i].alive) continue;
int n = collect_body(&s->birds[i], buf);
bool can_fall = true;
FI j = 0; j < n; j++)
if (solid_under(s, buf[j], buf, n)) { can_fall = false; break; }
if (can_fall) {
moved = true;
shift_bird_down(&s->birds[i]);
bool all_off = true;
FI j = 0; j < s->birds[i].len; j++)
if (bird_seg(&s->birds[i], j).y < s->grid.h + 1) { all_off = false; break; }
if (all_off) s->birds[i].alive = false;
}
}
R moved;
}
S bool birds_touching(const Bird* a, const Bird* b) {
FI i = 0; i < a->len; i++)
FI j = 0; j < b->len; j++)
if (manhattan(bird_seg(a, i), bird_seg(b, j)) == 1) R true;
R false;
}
S bool apply_intercoiled_falls(State* s) {
bool moved = false;
bool seen[MAX_BIRDS];
memset(seen, 0, sizeof(seen));
int alive_idx[MAX_BIRDS], nalive = 0;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].alive) alive_idx[nalive++] = i;
FI start = 0; start < nalive; start++) {
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
FI j = 0; j < nalive; j++) {
int oi = alive_idx[j];
if (cur == oi || seen[oi]) continue;
if (birds_touching(&s->birds[cur], &s->birds[oi]))
queue[qt++] = oi;
}
}
if (ng <= 1) continue;
Coord meta[MAX_BIRDS * MAX_BODY];
int nm = 0;
FI i = 0; i < ng; i++) {
Bird* b = &s->birds[group[i]];
FI j = 0; j < b->len; j++)
meta[nm++] = bird_seg(b, j);
}
bool can_fall = true;
FI i = 0; i < nm; i++)
if (solid_under(s, meta[i], meta, nm)) { can_fall = false; break; }
if (!can_fall) continue;
moved = true;
FI i = 0; i < ng; i++) {
shift_bird_down(&s->birds[group[i]]);
if (bird_head(&s->birds[group[i]]).y >= s->grid.h)
s->birds[group[i]].alive = false;
}
}
R moved;
}
S void apply_falls(State* s) {
bool something = true;
while (something) {
something = false;
while (apply_individual_falls(s)) something = true;
if (apply_intercoiled_falls(s)) something = true;
}
}
S void step(State* s, const Action* p0, const Action* p1) {
s->turn++;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].alive) s->birds[i].dir = -1;
FI i = 0; i < s->nbirds; i++) {
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
FI i = 0; i < s->nbirds; i++) {
if (!s->birds[i].alive) continue;
int ai = apple_index(&s->grid, bird_head(&s->birds[i]));
if (ai >= 0) eaten[ai] = true;
}
FI i = s->grid.napples - 1; i >= 0; i--)
if (eaten[i]) remove_apple(&s->grid, i);
int to_behead[MAX_BIRDS], nbehead = 0;
FI i = 0; i < s->nbirds; i++) {
Bird* b = &s->birds[i];
if (!b->alive) continue;
Coord h = bird_head(b);
bool in_wall = grid_valid(&s->grid, h) && s->grid.walls[h.y][h.x];
if (!grid_valid(&s->grid, h)) in_wall = true;
bool in_bird = false;
FI j = 0; j < s->nbirds && !in_bird; j++) {
if (!s->birds[j].alive) continue;
if (s->birds[j].id != b->id) {
if (bird_contains(&s->birds[j], h)) in_bird = true;
} else {
FI k = 1; k < b->len; k++)
if (bird_seg(b, k).x == h.x && bird_seg(b, k).y == h.y) { in_bird = true; break; }
}
}
if (in_wall || in_bird) to_behead[nbehead++] = i;
}
FI i = 0; i < nbehead; i++) {
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
S float feat_grid[GRID_CH][MAX_H][MAX_W];
S float feat_scalars[SCALAR_N];
SI void mark_ch(int ch, Coord c, int h, int w) {
if (c.x >= 0 && c.y >= 0 && c.y < h && c.x < w)
feat_grid[ch][c.y][c.x] = 1.0f;
}
S bool seg_has_support(const State* s, int bird_id, Coord c) {
Coord below = coord(c.x, c.y + 1);
if (grid_valid(&s->grid, below) && s->grid.walls[below.y][below.x]) R true;
if (apple_index(&s->grid, below) >= 0) R true;
FI i = 0; i < s->nbirds; i++) {
if (!s->birds[i].alive || s->birds[i].id == bird_id) continue;
if (bird_contains(&s->birds[i], below)) R true;
}
R false;
}
S int bird_slot_ids(const State* s, int owner, int out[MAX_BIRDS_PP]) {
int n = 0;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].owner == owner && n < MAX_BIRDS_PP)
out[n++] = s->birds[i].id;
R n;
}
S int live_count(const State* s, int owner) {
int n = 0;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].owner == owner && s->birds[i].alive) n++;
R n;
}
S int break_count(const State* s, int owner) {
int n = 0;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].owner == owner && s->birds[i].alive && s->birds[i].len >= 4) n++;
R n;
}
S int mobility_count(const State* s, int owner) {
int total = 0;
int cmds[5];
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].owner == owner && s->birds[i].alive)
total += legal_cmds(s, s->birds[i].id, cmds);
R total;
}
SI Coord canon(const Grid* g, int owner, Coord c) {
if (owner == 0) R c;
R grid_opposite(g, c);
}
S void encode_features(const State* s, int owner) {
int h = s->grid.h, w = s->grid.w;
memset(feat_grid, 0, sizeof(feat_grid));
FI y = 0; y < h; y++)
FI x = 0; x < w; x++)
if (s->grid.walls[y][x]) mark_ch(0, canon(&s->grid, owner, coord(x, y)), h, w);
FI i = 0; i < s->grid.napples; i++)
mark_ch(1, canon(&s->grid, owner, s->grid.apples[i]), h, w);
FI i = 0; i < s->nbirds; i++) {
if (!s->birds[i].alive) continue;
FI j = 0; j < s->birds[i].len; j++) {
Coord seg = bird_seg(&s->birds[i], j);
if (!seg_has_support(s, s->birds[i].id, seg))
mark_ch(2, canon(&s->grid, owner, seg), h, w);
}
}
int own_ids[MAX_BIRDS_PP], nown = bird_slot_ids(s, owner, own_ids);
FI si = 0; si < nown; si++) {
int hch = 3 + si * 2, bch = hch + 1;
FI i = 0; i < s->nbirds; i++) {
if (s->birds[i].id != own_ids[si] || !s->birds[i].alive) continue;
mark_ch(hch, canon(&s->grid, owner, bird_head(&s->birds[i])), h, w);
FI j = 1; j < s->birds[i].len; j++)
mark_ch(bch, canon(&s->grid, owner, bird_seg(&s->birds[i], j)), h, w);
}
}
int opp = 1 - owner;
int opp_ids[MAX_BIRDS_PP], nopp = bird_slot_ids(s, opp, opp_ids);
FI si = 0; si < nopp; si++) {
int hch = 11 + si * 2, bch = hch + 1;
FI i = 0; i < s->nbirds; i++) {
if (s->birds[i].id != opp_ids[si] || !s->birds[i].alive) continue;
mark_ch(hch, canon(&s->grid, owner, bird_head(&s->birds[i])), h, w);
FI j = 1; j < s->birds[i].len; j++)
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
S float conv_buf_a[MAX_CH * MAX_H * MAX_W];
S float conv_buf_b[MAX_CH * MAX_H * MAX_W];
S void conv3x3_int4(const int8_t* weights, const float* scales, const float* bias,
int in_ch, int out_ch, int H, int W,
const float* input, float* output) {
int hw = H * W;
FI oc = 0; oc < out_ch; oc++) {
float bv = bias[oc];
int base = oc * hw;
FI i = 0; i < hw; i++) output[base + i] = bv;
}
FI oc = 0; oc < out_ch; oc++) {
float sc = scales[oc];
int obase = oc * hw;
FI ic = 0; ic < in_ch; ic++) {
int ibase = ic * hw;
int wbase = (oc * in_ch + ic) * 9;
float w[9];
FI k = 0; k < 9; k++) w[k] = (float)weights[wbase + k] * sc;
FI y = 1; y < H - 1; y++) {
int ro = y * W;
FI x = 1; x < W - 1; x++) {
float acc = 0;
acc += input[ibase + (y-1)*W + (x-1)] * w[0];
acc += input[ibase + (y-1)*W + x] * w[1];
acc += input[ibase + (y-1)*W + (x+1)] * w[2];
acc += input[ibase + y*W + (x-1)] * w[3];
acc += input[ibase + y*W + x] * w[4];
acc += input[ibase + y*W + (x+1)] * w[5];
acc += input[ibase + (y+1)*W + (x-1)] * w[6];
acc += input[ibase + (y+1)*W + x] * w[7];
acc += input[ibase + (y+1)*W + (x+1)] * w[8];
output[obase + ro + x] += acc;
}
}
FI edge = 0; edge < 2; edge++) {
int y = edge == 0 ? 0 : H - 1;
FI x = 0; x < W; x++) {
float acc = 0;
FI ky = 0; ky < 3; ky++) {
int iy = y + ky - 1;
if (iy < 0 || iy >= H) continue;
FI kx = 0; kx < 3; kx++) {
int ix = x + kx - 1;
if (ix < 0 || ix >= W) continue;
acc += input[ibase + iy * W + ix] * w[ky * 3 + kx];
}
}
output[obase + y * W + x] += acc;
}
}
FI y = 1; y < H - 1; y++) {
FI edge = 0; edge < 2; edge++) {
int x = edge == 0 ? 0 : W - 1;
float acc = 0;
FI ky = 0; ky < 3; ky++) {
int iy = y + ky - 1;
FI kx = 0; kx < 3; kx++) {
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
FI i = 0; i < total; i++)
if (output[i] < 0) output[i] = 0;
}
S void global_avg_pool(const float* input, int ch, int H, int W, float* output) {
int hw = H * W;
float norm = (float)(hw > 0 ? hw : 1);
FI c = 0; c < ch; c++) {
float sum = 0;
int base = c * hw;
FI i = 0; i < hw; i++) sum += input[base + i];
output[c] = sum / norm;
}
}
S void linear_f32(const float* weights, const float* bias, int in_f, int out_f,
const float* input, float* output) {
FI o = 0; o < out_f; o++) {
float acc = bias[o];
int base = o * in_f;
FI i = 0; i < in_f; i++) acc += input[i] * weights[base + i];
output[o] = acc;
}
}
S int CNN_CH1 = 0, CNN_CH2 = 0, CNN_CH3 = 0;
#define MAX_CONV_W (MAX_CH*MAX_CH*9)
#define MAX_LIN_W (POLICY_OUT*(MAX_CH+SCALAR_N))
S int8_t conv1_w[MAX_CH*GRID_CH*9], conv2_w[MAX_CONV_W], conv3_w[MAX_CONV_W];
S float conv1_s[MAX_CH], conv1_b[MAX_CH];
S float conv2_s[MAX_CH], conv2_b[MAX_CH];
S float conv3_s[MAX_CH], conv3_b[MAX_CH];
S float policy_w[MAX_LIN_W], policy_b[POLICY_OUT];
S float value_w[MAX_CH+SCALAR_N], value_b[1];
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
S float pool_buf[MAX_CH + SCALAR_N];
S float value_out[1];
typedef struct {
float policy[POLICY_OUT];
float value;
} NNPred;
S bool nn_enabled = false;
S NNPred nn_forward(int H, int W) {
NNPred pred;
int hw = H * W;
int in_ch = GRID_CH;
FI c = 0; c < in_ch; c++)
FI y = 0; y < H; y++)
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
FI i = 0; i < SCALAR_N; i++) pool_buf[last_ch + i] = feat_scalars[i];
int feat_dim = last_ch + SCALAR_N;
linear_f32(POLICY_W, POLICY_B, feat_dim, POLICY_OUT, pool_buf, pred.policy);
linear_f32(VALUE_W, VALUE_B, feat_dim, 1, pool_buf, value_out);
pred.value = tanhf(value_out[0]);
R pred;
}
S float EVAL_BODY = 120.0f, EVAL_LOSS = 18.0f, EVAL_MOBILITY = 7.5f, EVAL_APPLE = 16.0f;
S float EVAL_STABILITY = 10.0f, EVAL_BREAKPOINT = 9.0f, EVAL_FRAGILE = 8.0f, EVAL_TERMINAL = 10000.0f;
S int SEARCH_FIRST_MS = 850, SEARCH_LATER_MS = 40;
S int min_dist(Coord target, const Coord* heads, int n) {
int best = 99;
FI i = 0; i < n; i++) {
int d = manhattan(target, heads[i]);
if (d < best) best = d;
}
R best;
}
S double mobility_score(const State* s, int owner) {
double total = 0;
int cmds[5];
FI i = 0; i < s->nbirds; i++) {
if (!s->birds[i].alive) continue;
int n = legal_cmds(s, s->birds[i].id, cmds);
if (s->birds[i].owner == owner) total += n;
else total -= n;
}
R total;
}
S double apple_race(const State* s, int owner) {
Coord my_h[MAX_BIRDS_PP], op_h[MAX_BIRDS_PP];
int nm = 0, no = 0;
FI i = 0; i < s->nbirds; i++) {
if (!s->birds[i].alive) continue;
if (s->birds[i].owner == owner) { if (nm < MAX_BIRDS_PP) my_h[nm++] = bird_head(&s->birds[i]); }
else { if (no < MAX_BIRDS_PP) op_h[no++] = bird_head(&s->birds[i]); }
}
double total = 0;
FI i = 0; i < s->grid.napples; i++) {
int md = min_dist(s->grid.apples[i], my_h, nm);
int od = min_dist(s->grid.apples[i], op_h, no);
int denom = (md + od + 1);
if (denom < 1) denom = 1;
total += (double)(od - md) / (double)denom;
}
R total;
}
S bool has_solid_below_eval(const State* s, int bird_id, Coord c) {
Coord below = coord(c.x, c.y + 1);
if (grid_valid(&s->grid, below) && s->grid.walls[below.y][below.x]) R true;
if (apple_index(&s->grid, below) >= 0) R true;
FI i = 0; i < s->nbirds; i++) {
if (!s->birds[i].alive || s->birds[i].id == bird_id) continue;
if (bird_contains(&s->birds[i], below)) R true;
}
R false;
}
S double support_stability(const State* s, int owner) {
double score = 0;
FI i = 0; i < s->nbirds; i++) {
if (!s->birds[i].alive) continue;
bool supported = false;
FI j = 0; j < s->birds[i].len && !supported; j++)
if (has_solid_below_eval(s, s->birds[i].id, bird_seg(&s->birds[i], j)))
supported = true;
double v = supported ? 1.0 : -1.0;
if (s->birds[i].owner == owner) score += v;
else score -= v;
}
R score;
}
S double breakpoint_sc(const State* s, int owner) {
R (double)(break_count(s, owner) - break_count(s, 1 - owner));
}
S double fragile_attack(const State* s, int owner) {
Coord my_h[MAX_BIRDS_PP];
int nm = 0;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].owner == owner && s->birds[i].alive && nm < MAX_BIRDS_PP)
my_h[nm++] = bird_head(&s->birds[i]);
double total = 0;
FI i = 0; i < s->nbirds; i++) {
if (s->birds[i].owner == owner || !s->birds[i].alive || s->birds[i].len > 3) continue;
int d = min_dist(bird_head(&s->birds[i]), my_h, nm);
total += 1.0 / (double)(d + 1);
}
R total;
}
S double evaluate(const State* s, int owner) {
if (is_terminal(s, CONTEST_MAX_TURNS)) {
int bs[2]; body_scores(s, bs);
int bd = bs[owner] - bs[1 - owner];
int ld = s->losses[1 - owner] - s->losses[owner];
R EVAL_TERMINAL * (double)bd + (double)ld;
}
int bs[2]; body_scores(s, bs);
double bd = (double)(bs[owner] - bs[1 - owner]);
double ld = (double)(s->losses[1 - owner] - s->losses[owner]);
R EVAL_BODY * bd
+ EVAL_LOSS * ld
+ EVAL_MOBILITY * mobility_score(s, owner)
+ EVAL_APPLE * apple_race(s, owner)
+ EVAL_STABILITY * support_stability(s, owner)
+ EVAL_BREAKPOINT * breakpoint_sc(s, owner)
+ EVAL_FRAGILE * fragile_attack(s, owner);
}
S int policy_idx_for_cmd(int cmd) {
if (cmd == -1) R 0;
R cmd + 1;
}
S State sim_state(const State* s, int owner, const Action* my, const Action* opp) {
State next = *s;
if (owner == 0) step(&next, my, opp);
else step(&next, opp, my);
R next;
}
S Action empty_action(void) {
Action a; a.n = 0; R a;
}
typedef struct { int idx; double score; } IdxScore;
S int cmp_desc(const void* a, const void* b) {
double da = ((IdxScore*)a)->score, db = ((IdxScore*)b)->score;
if (da > db) R -1;
if (da < db) R 1;
R 0;
}
S int cmp_asc(const void* a, const void* b) {
double da = ((IdxScore*)a)->score, db = ((IdxScore*)b)->score;
if (da < db) R -1;
if (da > db) R 1;
R 0;
}
S struct timespec search_start;
S double elapsed_ms(void) {
struct timespec now;
clock_gettime(CLOCK_MONOTONIC, &now);
R (now.tv_sec - search_start.tv_sec) * 1000.0
+ (now.tv_nsec - search_start.tv_nsec) / 1.0e6;
}
S bool expired(double deadline_ms) {
if (deadline_ms <= 0) R false;
R elapsed_ms() >= deadline_ms;
}
typedef struct {
Action action;
int action_id, action_count;
double score, mean_score;
} SearchResult;
S void render_action(const Action* a, char* buf) {
int ids[MAX_BIRDS_PP], dirs[MAX_BIRDS_PP], nc = 0;
FI i = 0; i < a->n; i++) {
if (a->cmd[i] >= 0) {
ids[nc] = a->bird_id[i];
dirs[nc] = a->cmd[i];
nc++;
}
}
FI i = 0; i < nc - 1; i++)
FI j = i + 1; j < nc; j++)
if (ids[j] < ids[i]) {
int t = ids[i]; ids[i] = ids[j]; ids[j] = t;
t = dirs[i]; dirs[i] = dirs[j]; dirs[j] = t;
}
if (nc == 0) { strcpy(buf, "WAIT"); return; }
buf[0] = 0;
FI i = 0; i < nc; i++) {
if (i > 0) strcat(buf, ";");
char tmp[32];
sprintf(tmp, "%d %s", ids[i], dir_word(dirs[i]));
strcat(buf, tmp);
}
}
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
S char line_buf[8192];
S char* read_line_s(void) {
if (!fgets(line_buf, sizeof(line_buf), stdin)) R NULL;
int len = strlen(line_buf);
while (len > 0 && (line_buf[len-1] == '\n' || line_buf[len-1] == '\r')) line_buf[--len] = 0;
R line_buf;
}
S void read_init(BotIO* io) {
io->player_idx = atoi(read_line_s());
int w = atoi(read_line_s());
int h = atoi(read_line_s());
io->template_grid.w = w;
io->template_grid.h = h;
io->template_grid.napples = 0;
memset(io->template_grid.walls, 0, sizeof(io->template_grid.walls));
FI y = 0; y < h; y++) {
char* row = read_line_s();
FI x = 0; x < w && row[x]; x++)
if (row[x] == '#') io->template_grid.walls[y][x] = true;
}
int bpp = atoi(read_line_s());
io->nmy = bpp;
io->nopp = bpp;
FI i = 0; i < bpp; i++) io->my_ids[i] = atoi(read_line_s());
FI i = 0; i < bpp; i++) io->opp_ids[i] = atoi(read_line_s());
}
S bool read_frame(Frame* f) {
char* s = read_line_s();
if (!s) R false;
f->napples = atoi(s);
FI i = 0; i < f->napples; i++) {
s = read_line_s();
sscanf(s, "%d %d", &f->apples[i].x, &f->apples[i].y);
}
int nlive = atoi(read_line_s());
f->nlive = nlive;
FI i = 0; i < nlive; i++) {
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
R true;
}
S int owner_of(const BotIO* io, int bird_id) {
FI i = 0; i < io->nmy; i++)
if (io->my_ids[i] == bird_id) R io->player_idx;
R 1 - io->player_idx;
}
S State build_state(const BotIO* io, const Frame* f, int losses[2], int turn) {
State s;
s.grid = io->template_grid;
s.grid.napples = f->napples;
FI i = 0; i < f->napples; i++) s.grid.apples[i] = f->apples[i];
s.losses[0] = losses[0];
s.losses[1] = losses[1];
s.turn = turn;
s.nbirds = 0;
int all_ids[MAX_BIRDS], nall = 0;
FI i = 0; i < io->nmy; i++) all_ids[nall++] = io->my_ids[i];
FI i = 0; i < io->nopp; i++) all_ids[nall++] = io->opp_ids[i];
FI i = 0; i < nall - 1; i++)
FI j = i + 1; j < nall; j++)
if (all_ids[j] < all_ids[i]) { int t = all_ids[i]; all_ids[i] = all_ids[j]; all_ids[j] = t; }
FI a = 0; a < nall; a++) {
int bid = all_ids[a];
Bird* b = &s.birds[s.nbirds++];
b->id = bid;
b->owner = owner_of(io, bid);
b->dir = -1;
int fi = -1;
FI i = 0; i < f->nlive; i++)
if (f->live_ids[i] == bid) { fi = i; break; }
if (fi >= 0) {
b->alive = true;
b->len = f->live_lens[fi];
b->head = 0;
b->tail = b->len;
FI i = 0; i < b->len; i++) b->body[i] = f->live_bodies[fi][i];
} else {
b->alive = false;
b->len = 0;
b->head = 0;
b->tail = 0;
}
}
R s;
}
S bool sig_match(const State* s, const Frame* f) {
if (s->grid.napples != f->napples) R false;
Coord sa[MAX_APPLES], fa[MAX_APPLES];
FI i = 0; i < s->grid.napples; i++) sa[i] = s->grid.apples[i];
FI i = 0; i < f->napples; i++) fa[i] = f->apples[i];
FI i = 0; i < s->grid.napples - 1; i++)
FI j = i + 1; j < s->grid.napples; j++)
if (sa[j].y < sa[i].y || (sa[j].y == sa[i].y && sa[j].x < sa[i].x))
{ Coord t = sa[i]; sa[i] = sa[j]; sa[j] = t; }
FI i = 0; i < f->napples - 1; i++)
FI j = i + 1; j < f->napples; j++)
if (fa[j].y < fa[i].y || (fa[j].y == fa[i].y && fa[j].x < fa[i].x))
{ Coord t = fa[i]; fa[i] = fa[j]; fa[j] = t; }
FI i = 0; i < s->grid.napples; i++)
if (sa[i].x != fa[i].x || sa[i].y != fa[i].y) R false;
int nlive_s = 0;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].alive) nlive_s++;
if (nlive_s != f->nlive) R false;
FI fi = 0; fi < f->nlive; fi++) {
int bid = f->live_ids[fi];
const Bird* sb = NULL;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].id == bid && s->birds[i].alive) { sb = &s->birds[i]; break; }
if (!sb) R false;
if (sb->len != f->live_lens[fi]) R false;
FI j = 0; j < sb->len; j++) {
Coord seg = bird_seg(sb, j);
if (seg.x != f->live_bodies[fi][j].x || seg.y != f->live_bodies[fi][j].y) R false;
}
}
R true;
}
S State reconcile(const BotIO* io, const Frame* f, const State* prev, const Action* last_my) {
if (!prev) R build_state(io, f, (int[]){0, 0}, 0);
int opp = 1 - io->player_idx;
Action opp_acts[MAX_ACTIONS];
int nopp = enum_joint(prev, opp, opp_acts, MAX_ACTIONS);
FI i = 0; i < nopp; i++) {
State sim = *prev;
if (io->player_idx == 0) step(&sim, last_my, &opp_acts[i]);
else step(&sim, &opp_acts[i], last_my);
if (sig_match(&sim, f)) R sim;
}
R build_state(io, f, (int[]){prev->losses[0], prev->losses[1]}, prev->turn + 1);
}
S void init_weights(void);
#define PUCT_C 1.5f
typedef struct {
int ncmds;
int cmds[5];
float prior[5];
int visits[5];
float total_value[5];
int total_visits;
} BirdPUCT;
S void bird_nn_prior(const NNPred* pred, int slot_idx, const int* cmds, int ncmds, float* prior) {
float logits[5];
float mx = -1e18f;
FI i = 0; i < ncmds; i++) {
int pi = policy_idx_for_cmd(cmds[i]);
int idx = slot_idx * POLICY_APB + pi;
logits[i] = (idx < POLICY_OUT) ? pred->policy[idx] : 0.0f;
if (logits[i] > mx) mx = logits[i];
}
float sum = 0;
FI i = 0; i < ncmds; i++) {
prior[i] = expf(logits[i] - mx);
sum += prior[i];
}
float inv = 1.0f / (sum + 1e-8f);
FI i = 0; i < ncmds; i++) prior[i] *= inv;
}
S int bird_puct_select(const BirdPUCT* bp) {
int best = 0;
float best_score = -1e18f;
float sq = sqrtf((float)bp->total_visits + 1e-8f);
FI a = 0; a < bp->ncmds; a++) {
float q = (bp->visits[a] > 0) ? bp->total_value[a] / bp->visits[a] : 0.0f;
float u = PUCT_C * bp->prior[a] * sq / (1.0f + bp->visits[a]);
float score = q + u;
if (score > best_score) { best_score = score; best = a; }
}
R best;
}
S SearchResult choose_action_puct(const State* s, int owner, double deadline_ms) {
int bids[MAX_BIRDS_PP], nb = 0;
FI i = 0; i < s->nbirds; i++)
if (s->birds[i].owner == owner && s->birds[i].alive && nb < MAX_BIRDS_PP)
bids[nb++] = s->birds[i].id;
if (nb == 0) {
SearchResult sr; sr.action = empty_action(); sr.action_id = 0;
sr.action_count = 1; sr.score = sr.mean_score = -1e18;
R sr;
}
BirdPUCT bp[MAX_BIRDS_PP];
int slot_ids[MAX_BIRDS_PP];
int ns = bird_slot_ids(s, owner, slot_ids);
NNPred pred;
bool have_pred = false;
if (nn_enabled) {
encode_features(s, owner);
pred = nn_forward(s->grid.h, s->grid.w);
have_pred = true;
}
FI b = 0; b < nb; b++) {
bp[b].ncmds = ordered_cmds(s, bids[b], bp[b].cmds);
bp[b].total_visits = 0;
memset(bp[b].visits, 0, sizeof(bp[b].visits));
memset(bp[b].total_value, 0, sizeof(bp[b].total_value));
int slot = -1;
if (have_pred) {
FI si = 0; si < ns; si++)
if (slot_ids[si] == bids[b]) { slot = si; break; }
}
if (slot >= 0) {
bird_nn_prior(&pred, slot, bp[b].cmds, bp[b].ncmds, bp[b].prior);
} else {
float u = 1.0f / bp[b].ncmds;
FI a = 0; a < bp[b].ncmds; a++) bp[b].prior[a] = u;
}
}
Action opp_acts[MAX_ACTIONS];
int nopp = enum_joint(s, 1 - owner, opp_acts, MAX_ACTIONS);
int iterations = 0;
for (;;) {
if ((iterations & 15) == 0 && expired(deadline_ms)) break;
Action my_act; my_act.n = nb;
FI b = 0; b < nb; b++) {
int a = bird_puct_select(&bp[b]);
my_act.bird_id[b] = bids[b];
my_act.cmd[b] = bp[b].cmds[a];
}
double best_opp_val = 1e18;
FI oi = 0; oi < nopp; oi++) {
State next = sim_state(s, owner, &my_act, &opp_acts[oi]);
double v = evaluate(&next, owner);
if (v < best_opp_val) best_opp_val = v;
}
float val = (float)(best_opp_val / (fabs(best_opp_val) + 100.0));
FI b = 0; b < nb; b++) {
int a = -1;
int cmd = my_act.cmd[b];
FI j = 0; j < bp[b].ncmds; j++)
if (bp[b].cmds[j] == cmd) { a = j; break; }
if (a >= 0) {
bp[b].visits[a]++;
bp[b].total_visits++;
bp[b].total_value[a] += val;
}
}
iterations++;
}
Action best_act; best_act.n = nb;
FI b = 0; b < nb; b++) {
int best_a = 0, best_v = -1;
FI a = 0; a < bp[b].ncmds; a++)
if (bp[b].visits[a] > best_v) { best_v = bp[b].visits[a]; best_a = a; }
best_act.bird_id[b] = bids[b];
best_act.cmd[b] = bp[b].cmds[best_a];
}
SearchResult sr;
sr.action = best_act;
sr.action_id = 0;
sr.action_count = 1;
{
double sum = 0; int cnt = 0;
FI b = 0; b < nb; b++) {
int best_a = 0, best_v = -1;
FI a = 0; a < bp[b].ncmds; a++)
if (bp[b].visits[a] > best_v) { best_v = bp[b].visits[a]; best_a = a; }
if (best_v > 0) { sum += bp[b].total_value[best_a] / best_v; cnt++; }
}
sr.score = sr.mean_score = cnt > 0 ? sum / cnt : 0;
}
R sr;
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
printf("%s\n", cmd);
fflush(stdout);
last_my = sr.action;
prev_state = state;
has_prev = true;
}
R 0;
}
// WEIGHTS_PLACEHOLDER_START
S void init_weights(void) {
}
// WEIGHTS_PLACEHOLDER_END