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

static const char CONV1_W_ENC[] = "\x01ˠ〳\x15ༀ\x10Ȁ\x10ᄀ쇱ﯼ⃑ༀð\x01\0ᄏ돰飼ᰐǡ쀍ဏǰഐ￟\x0f༁ð༏ðഀῠ\0ð￿܁\x0f⇠퀑ā\x0fᄀሮ᷍࿹ㄲἒ῿丐ꈮ⯡ð﷭쳣ď᧚ᄐ࿲\x11＀༟Ďᄀฎሏᇰ༏ჰĀ࿠쀀￿Ա廒༁\0팟Ò넌Ḁᄜ탡￿ܐ쌐⯂⶞ྲᴀ０ༀĀრ࿯ÿ\0︀︀฀ǰǮ⿿ð\x0fñჰ༑ḏἀǰ刵ㄕư섩ﰁଝഏⰑḬ∑ᇻ഑.뷬þﯿḀ༟ⷰခ\x02ჰ༏\x11࿯Ḁ༁ჿ︐ჿ\0ကᾩ￿Ԑကﾻဏﻰㇿ⻿켞ǽ̫￿ۮᄮ』＄⯿︐ϼ툻෱Ȏ⻿봡\x0bခℯ\0ᄀ⻟괡ᇻ\x11℮\0ᇿഎ㼍\0⼟ఢ\0鸁벾ᰝሑ\x02뺞᷊∾Ȓ글쯏㼞Ȳἑ뺞᷊㈾ሒༀሟＯÿᄁ‟࿱￾ヰ̐ﻱᄂ⃠࿱￾Ā\x1f냿Ự∽῰ἂ췠ㄳ倐瑇ჰᄂȐม\x02십တＯ\x02ďỰヰϱ\x01︟\x1f\x1fჰ῰\x0f\x01°ฐĀᇱǯ‮သ⃰０⿠þ⸠‡Ǿ턙‐⸟༠᏾ﷹàî\x01ჿ탾\x10＀áü\x0fð＀\x10ༀ＀\x10\x0e︀\x01∁\0က췲ﮛᇼﻀဟ鸐刺뀠킰\x1f￰ༀᇟȒἠĐĀ̀‡\x11Ǳᄐ∓ğĐက̐℡ဒ\x01đĀ\0䄮࿦ꧮ¼᪾࿿쿫ð줏⻮ሀ！￿ޯ츀က฀ༀȁ＀တ\x1eᄀĂ\x10ฑἀȁခᄁ\x0e\x0fȂĐᄑἑñ༏ကﳞ⃿ℱȀ선￿ʭℑᄑ겢짚⇮ᄠꈑ￿μ℡ā눂ȏထℏῡ䄍đᄐᄱကĐჁ㌍\x10တ࿲ྐᄁȌāñༀ찝࿃˿묛Ⴁ䴿࿱냞áď찝⻁帎／ῡ郻àḏᇑฝ﻾àỰǠ฀რÿ฀႟ༀ῁ċ⼀ᯣǿ0￿ܿḀïⰞ⇌᧬￿ܑူ伾༐က￿ؐჀ෾︀῀샼쿮￿ܠ⋀ÿჀ싞ỿ샍ടჰዟ<Ā뀟㧑ǛჂ츛ㄳᄳḁˏ⸓㳂༛Ᏺḯ돁ଽ‏༏ǰ１á\x0f༁ㇱฏǰÑĎༀ࿱ༀ࿰꧍ð㈐ༀ㸍퉂⼤ჯ㬯뉁⿀￿ײ̡￿ԍᴁᇰሠ༁Ḁ⼑\x0f\x0fᇰȟ࿿࿰ဢᄂǿÿༀ\0\0Ｂခðჽÿꀁ꾑黽ἱˀ\x1e쀁˒ﴝῠሏกĒȁᇿǱïሀ༁đ\x10Đᄏ\x01\x10ကǱđခā鷪ﴮ⌡≃ē\x01볮\0㑀㍄ᄴĐဏฐ／\x10\x10̐ġჿĀကἏĀďȑĠ\x10Đἀ￰\0鼐ⷡ\x19ᇂἯヰǝꀊ￿۰ᇑထሑ﻾꺽ట섡ﻡს\x01\x10\x01ĒǱကༀˢ\x01ༀĂကἀﻰ　쿮？\x10\x10ະȐ.宵㋏꼉ข↡툮ሁ퀞봒仡逐ⷌ⏡ဎ﫿￿ࠀ￿ࠀㇰဎﰀ／\0ຠ츀ʞᇰÿⴿ\x1e῿ꈐఁೠዀ묜᫼쇒ﻠအȠဒልñÁñ໡\x01àĎǡ︀\x10 Ἥ￿ࠀ￿֝໼တ∟￿ۓﳬᇾတ￿Ӿ￿׽đ⏿￳ෝჰ２⏰ợ﷼࿿ฏ⌐ﻣ﷬￱༏̣ༀ\0\x01ἁĀ０໿᏿ೢ쑁ᾡ맬ᤢ쒡㵔\x02⇂ǡ퇱῰ð\x0e\x11\x1eༀᄀḟ￰ༀǠༀἀᇱ\x1fሐကﴢჰ￾\x02벰뀡⌾찌ⳡǿऍ࿡뺽⋴￿Л얝밊ᯁோþዠሐሎ￢໲฀༠ደ\x01탱òအ\0ᄐ\x11ððἀ⋲ᄜ틒לּ턏Ἦ⿠－￿ߟ໲．ጂ鸍῿툎⸢ﴒĎ\x10Ā༎῿\0ðḁď\0\0എ῰\x01က༎ༀĐჰ\0Ȁ삿㻡놯￿Яﷀ̏￿ܮ௢뻋ü⼃降⸐ð퀏ᰐᰐ↡턝⸀რꄝᴑǡ฀＀\0῱ᴏ￠\x10ⵏ㨀࿝‰∠ฒÑ⼾૱䨎ㄱሯ༒밫࿃࿿ༀༀ￡ᴁ࿿ď︁\x0f\x01ऀἐ჻ༀῡð༊ᗱ࿿਀࿲ðḊĂ切༟Ⴏ\x01࿰ᇰ긞įǰ＀Ḑ⾿Ā῰꼏ဠĀđ츎ﯾỡǾဟ℣￿ࠀ㿰ʿ뀁ྟğဠ⊲꼂༐\x11夳ጓ㷌ꇩŁ\x10ﯱ෮』ሐ\x0e.ﯱ෮刏ᄔ￰￿۪︍ᆑ༎ഉ퀁ခⷰᇀ턟뀀ἑ㳻čᆰ탱￿ؽꇰἑ\x0f࿿￰࿰ༀ\x0f࿰\x0f透Ḁ\x10⿱\x0fȀῡⰏ〽ዱ༟０﴿กᬀဏ㸽싟￿Ѝ෱￱ðǰàഀǱ\0႐\x0fዮİ\x01\0῀ḟ¡⃿ðÐἀ⻁ฏ±℞ð໠\x0e\0ð\x0e༁ð﻿ᄁ\x0f\0１༏฀ခ\x0f\x0fༀ\x10㏿෢ჿᄐ뤙᫦ӻ꯹묃⌫첼ዌò﷐လ뷒ሜ￿μዝ\x02బ￡/Ȱ⿟ﻰମ︂\x1eⷡምἐᄐༀฎㄏįǿ？＀ᇁἑ⬀쇯ထ뀏ℤᴐ᧽뼊ထီ⋲ἀ￿П￮\x0eကﻯἐÑﴠ￐\x0eð鼀\0တก\x1cฏഀ举ἀ໱탼⿾༾\x1f︞䴿ကἀ￿߾㻮༿ᄟ഍ñଁđ０ༀ\0ဋ\0ÿ\0\x10ༀ＀ჼĀ࿿ðĀༀᄀ\x11ༀ\0∀\x12섐쳩ကᄁ￿یǯđ䄐㍄∲ဒ\x01㑀ሔĀﻡ俾㌤\x10￿׮∠ℒᄀﻱ⿾ሓဠ༏რ／겐ලჩ\0ﲐዒ透㐯ﻰᄀ༁ᾰᄁ뼟┮ჰ\x01⾠෰Àꇾḟáꀍฐᾱ฀À뀏἟࿱뀍ฐ\0ྐ⿰Ͽ༿븾⃯กႱ䳯튾-ꄌ츽Ⳓऀᇁ໮ﳤ﷯῱ǼǑȜฟᄀ턞ḁ\x02࿿༁ǁȝ︀Đ퀏ᷱ\x01࿰༑Āᄐℑᄑฑ\x10ḡกಚ쿽á෰ﻼï껭ﴠ\x10တ탠ჰတ︐Ǡ￰Ðထᇠഏ̀⃰꼀￿ױ࿾ᄀُ㖶တ༿돪﫭됐ﯢຐ섪ᰀ⏵짠ᇟᄐỠἀ”ᄠðฏởġ＀ဏ༏∍？ď\x0fǯჰğ‐ĠỰ＀ꨍ쀀ᄍတภರðဟ귾鿐ဝခ༑⿐ᄐ\0쯾ϰ컢㌏︠῍ဲϱ￿ף∐￿ܐíῌᄲἀ¬ＮĐ／‮쾰꼏³ÑﺰꄎἛ￿נ鸙ᤃ⌡ㄣ༏￿ۼༀ෰￿߾എǡß�໰῿퀁﷯ð༎ဟﰙℐఒ\x10ᔂ偠⏰첺꾥⤯돽ᇡ‡츌￿޲㰠쐤ß￰／\0Ð࿱Ǿ￿נĐ༏\x10￿ܡ࿭໱Îᄑ藺ꀐ⃠༑Đကǜༀጟჰ︭̏ᇰЁ０἟↿\0︁ჰ긐ဟခ₭Āð꼑\x10ü\0\x1e\0᳾čĀ￿ܐ⸱툎ᨀ࿿ῑഽခ༌퇰㰠ᬟḏ⃟ᴬჱ་Ǒဌᴁ턀ఀĐ“àᇽᴁﳱȐ‍ðÐ\x01ሐ෍ༀď＀㈎쿡ᴝ츣틝퇯￿؞ዏ￁￿߾ﻓ췎ꉒྜȀ⃠฀ð໰༐\0￿܏ῐ￰໰Ģ฀ฏἏ௮꿼Ḟ⸝ȠǞ쌃ᯡ쏡쾲︯⻱퇞숃⿐\x0f⏡೒ฮ鼀ĮỮᄎ￿ׁᾐ￿؁ḝ鿭ĝỞᄟ㺟Ἆǰༀ뼂ð῞\x11ȏ︍⇯ೲჰ༂￿߯숲￫∠⇐ﳑ⃰ฒ\x0e\x0f\0\0\0ďñ\0ḟǰ༁࿿\x1eĀð\x0fÿ\x1d￿ۼ＀ᄀ༐ꄲແꫛᴑⴠ컢⫱Î༭Ēﷰ폰Ⱐ쯒㧱࿞\x01Ḁá༏＀ñ０\0Ḁ\0ðༀ‏ǰᄟ\0ༀ？턛ﰑᨁ슭ᄝ쀌ﰁ쮟࿯ā㈐ᬁƭᄛ뇾㸂Ͽ༟！Āဂჿ\x0f　⇢༟༐Ā༒ဏ\0퀀ñတ⃡ထᄀᄀჱኰㄲሡ￭켛℠ā᳽⃍ᄰǱ뀏㈡အȑტð༐\x1dȟ༂ჰ　⇲đ０Ȳēἁì\0 섎숟༁쇮⿟뿑᎐Ⳅ⌜￿׼ﻳ⸮㇂⬠￿Ӝ￿ׁ췳\r￾\x0eฐഏữð\x0eñༀḀḎἀ\x0fð．࿰ᄀĐ单䌧ဏ＀ꋠᰠ컟㫲´ûî！쏐ఐ⬁῾２ÿ\0ᄀð࿿\0ǰကჰ฀\0\0Ǯ\x01ďℏက０췪ဒĐᄠᄀሑሐ\x11ထ\0切ᇱက\x0f／௎\x0fᄟ⼐큀ἒ￠΅ᄎ℁\x03\0ჿ\0쇺ఀ\x10䈀￿ܝ팃ᇾᴐ峳㊢ს㊱鐬ରᇠʝǿ＀\x01ﻢ࿾\x11༞čༀ෢ÿ\x01ð\x10ༀ퀀⃎Đđ￿܁툞ď∁Ǭð騂〭ȓ￿ܭᬟ‐⌠⸢ựĎ￾þมሞ໽῱\x0fภ￰Āჱഀ\0ༀàሠἀǰ쬫￿˳⸪뎓㮽ⴐዂ⬐퓛ﴭ㸄ᰐ䋺ጒā＀쇏ἁ༞ï࿡\x1f︎\0Ỿဎ￰Ǳ༟\x0e\0ĎﻰĀ︟ⰏḊ㏑ᴭ∟ሢ⼀⌢ㄢȐ༐ġ뾹Ǿ』ῢჾ￿ۭ῿촯ໞ℀໣￮￿ۍჿ섑？ﴟ￞\x10＀\x0eð＀\0ǰ㼀￿Ӓጢ퀠￿ԭⷔఝዂ⸍ᄒ숟￿ے퉌ꄂﴀༀ\x0f／í￱\x0fဏﻰ\x0f༏༏ჿ༏\0\0࿰ðđﴀ\0\0﫠ྟċⴐ뇪ฟꃾ໱ﻠ΅āတ췭༎ကሑáᇠĐ࿱༏Ǳ\0\0࿐\x1e\0\0\x0f\x01\x10ďǰà\x0fᄀဎἐĀ⃢သ༐\x0e쇰Ḣᄯ\x0f㻣မ섛ยဠḏ＀࿿Āჰ༟༏āༀჰď\0Ἇ༟༏\x01༟ෲဟßğˁ໠༑ሀ턀ჿ\x11˞ﻰ࿰ᄐ\x01뀑ခἁïﻰǰ／ᄀð\x01￠ᇰ／῰ð⼁ﴀုﺾ᳡캫￿ܙῲა⼠ﲾᴀ噶敇ǲ \x01ዿჿ\x0fĀขༀ\0\x11˿\x0eď\x01︓＀ 녏\0︐丿鼡༝퀌뫠ഛヰ㬠჌ˋ︁Ǐဎᄓჰఏ࿟\x0e༏࿰Ǿ￐⼏￾ȟༀ༏\x0fༀȠကༀ敥㕜ം༐ḁዏ῾퀞넜෍ð댐Ｂ킰⧲᳞퀐฀က\0ðༀ！࿯Á\x0fǰခ༏Ā†ณꀐ䏜ﮪ䲚䍱搷ℴđ뿯㯡඼⋡﻾퇀ྑ↡￿؀ñ῿Ġï？࿰ဟ\x01ကတༀ．ༀ༁\x01\0\x01￿۝ý\x01ἐ￿܏⯰࿽ᇱတ퇐బ쌎ἡ탱⤐ᇡ﻿￿ۮỞ\x11က￿߿컟Ğ\0ༀ쿯Ở\x11ༀ￿ۮĞđༀǡ섉Ḱ︎࿱쁌ﳡ἟ဎჰ．틡퀀⻱ອἏጐ 켏ฏᇠ༣ﻰￏþጁ￿߿ฏðἒ࿰࿟\x01턬ഁ\0\x01Ἕ⹀ﻐ㋾఑￿؍ὁǠฎ෡䋮ჰǰëခÿþ଀\0ฐ\x0f\x1b\0àďਁ\x01ฐ＀ᄐȑ\x10\0\0෿￿ߍ‑\x11ﻰ﷬ዿᄑෟ茶ℂഡ෽숏￬ማἏᄀ࿿ď༠༑༏Āἀᄏ＀ÿ￰တ༑ᄟ\0￰￿ࠀ἟ᇰ퀀蘿⃐섟༁ˁრǱ⌍℣⌔턬\x10ჟꀒ†⋼ᄐဟ\x01ð1ἑğ\0䈀Ȑ἟\0Āေ\x11đခ\0ÿတ༏\0〰ⴢ\x1e\x10㇟̱⼳໠﻿ᇾ༁䃿瘗䝕\0ῠ\0Ā＀à\x10༐ᇱāÿἀ茶￿܀鿮ἀⴁဍ↟༡ㄿﳡ༮⯲틌￿Ӽ㻲밻஢༎᯳ắ턞ംዢ⤍뎹냰⯑醫￿Ě쇢븞쮱ⴟǡกĀ\0ሑỠ\x01　⌢༣쀠Ḁ⿡ㇰ痖␴\x10눠⌰ⳢđğȎჱĀ︀ခ\x10ሎ\x10࿱Ā﷠ \x10\x01蘿뿱Îᄀ\x10ἀ￰﬐Ďഝǰฌᴁﰁďꆠഞ࿱ሟ࿐Ⴐ퀐／à뼀ᄑÀ\x0f¿퀀ഀñĀऑ ⳱퇿ἁ놰బ⿢ꊠ﬛＃ Ȓᰂ승ᬒ케ฑ⺰ᾰ켛ℐბ쀞ᯁῡ뇌⫂⓻￿Л᫢ऊÐ￿נྟ\x0e￰Ǖā촀﻿섀隣ﷀ퇹헡ǰ༑᷎⼏ﻰ࿰븋༞ए⺾Ἇﻰ࿠븊༞ଏð\tḞἏ￿ӽ೟ℛ᫻௏ማ￿ӱᨊ넟ℐāἐႰ\x11\x01ခ널ᄐ\x11ďἑ↱Ģāðက\0ᄐ˾\x01ကฟဍ₿ᄯ䍓⌳\x02ᰑⳢἝෝ鬀လ턑࿝ကáᇰ\x10Ă\x11฀Ā\0\0\x11ဏ\0\0\0૰ ᄏǠ\0섀ᴄೱᆰ툎㏀쿒뼰⺮ఒ‑퇠️￿ܐ̜ﴁ\tÀñ０ð츏\0\x0f\0ßǰ\0￰￿܏ༀခ\x0eऀÐဌ샀\x1fက\0￿̛გ໡ዼฎ࿱ﰜ༂᳡쏜༟ᯰ¡샽਀đ넌ᴑÀ\nḁ¡쀍ᨏᄑꄍᴁ¿ᄊᴑ\0ﳰựჿā⼏ᄐ낱갺䑀≃츀ā￿۰ ര᳡༛໢༎／\0༎༐ﻰဟ️\0퇠\x0f࿰\0ꀀ\0\x1fﰢ\r\x10︁༯\x0f￠⻿㼟ഀǰἿ ĝ໱㿾‑ༀ⳿슼ꄱⲢ醛찭⇒놰갬ⲡ슼뀡Ⲳ醬ᆱᄛᴑᇑĐḁđက\0숡࿩℀媲ᇰ࿡䕑囆￿قȟ䄐吵ﰕဏ\0＀︑ÿჿ\0ÿᴐ࿿တﴮ／༐⇯￰ÿတༀ￑฀Ɏ・ዲ䀌ⴟ༠⋽ሏ︉ዢ㸌㸝Ｏ틾￿ȡ၏⿽￿؂ヲ／ḿᄎ⿿ñ㿲￿ؐ＀Ŀ࿮⇿ᇱༀÿﻰ￿ߏÿဏሃﷱྣ’⸀ꋬ⻟ıǢ낳〯Ḁ달೰Ự﻾￯＀෰﻽࿿࿿︍฀ჿð ⃱Ⱏà섀⨓῟︊༁Ⓞ쀪切ñ쌀⨓ༀ㏣턫ৱơ턏༐łᇿȐგㄎ＂ᄑ툂༑ĲဎȐს㈏！ᄁ送ᰐྯᨁĊ﻾ñᴠ࿰쳏⼞【뼞ῌῠ첿἞䀜뼏ᾼ﨏Ȃḑ\0ǻഁჰဝ﫿˒ḑǫംჰသༀ࿠ᬀ\x01ǱĐÿȡ⃬＀넡ኹ＀ᷯì−ჾ￟ᇠ턢？೾ǜ໿ᄏ＀\0Ḏ༐\0ฏဟ／ð\0Ḏ︑＀\0༂⃠\x11ᄟ￿߾ㆲ⏋鸏ྺ!໬ﬁም᲻ം￾ᄉ\x02༞Ǳ⼀Đ\x01\x1f\x01ᄑༀ\x01 Ḁ\x10\x01\x1fༀထ⼀༏㈒אָက꤁⼭ﾒ‏ᄏ剒덝￳ȁ￿ȏ䀾ჰĎ幚酂⋐໽἟Ｂ\x01῏ခဏ컰ℯᄒ＀\0⾠ီ 透Ḑᇻđ탐༾ဎ￿߯⸌ഀ\x10ğ탑༮‍⻠ฎჯਁȂ＝฀\0ȋᰑ࿿\x0eଐȂ－༏\0∏ഁ￿ࠀÿ\0ﾠ츐Ïჱ\x0fā퇢ﻯ￿̎츑┾῁￿đĉⷍ섔ሎ響ᄑᯢῡ࿿ἐð⃿\x1eༀỠ￿ࠀἑð턟ჰ฀ÿ\0໿ༀᄐ뷲ም໭︁Ͽ⁏$\0⃰∠ᾟ῿\x10０໾ǰ῾ǿđ\0༏ᄑ１ðༀÿ\x01\0쿿 \0ℑ㌣ā\0ᄀ궯￿߭Ý\x1f쀠ﻯ෿ༀဏ＀\r\0￿ܠﶽ\x01ἀ˰Ȳༀက㌑\x02\0ᄐȢ\0\0㈂Ǳჰ༏藺ჰሑჰ\x01Üᄑˮ컡Āሏﻢ\0Āﳰჱሑခ⿿!ð℀ᄐð༐`\"＀℠\0Ā\0ༀἀ\x0c＠რñ჏넏툡ሜ⋐ం툲კἐሳ⌫￿ǂထ㌟ଂ싂ခ\0ð฀ჯĀ\x0f\x0e\x01ჰḏ໰ჰ\0";
static const char CONV1_S_ENC[] = "羓㼈⪞㺚こ㻋劽㺰览㻌㟟㺹銱㺏䑗㻽븠㻈ԑ㻝䊛㹳綱㻚廑㺞￿؉㺕妳㺰㴂㻔ꮯ㺠Ｘ㻇⮞㺊಍㺓ꧯ㹭꯿㺧￿ݳ㹯듸㻂ఛ㻌蠷㺷¸㻺㢨㹴糞㼒穹㻽㼣쐎㺂Ꙧ㺼裸㼓寈㺫￿د㺨䯖㻪뙊㺫᥹㹸随㺞Ḳ㺛폗㹏븊㻏랍㼀暭㺜ꅖ㺪Ԇ㻈좺㺬깡㻡ሕ㺙鮫㺺쉁㺢ẇ㺟ࠈ㺝酞㻕￿˭㺕￿ʫ㺕瘉㻊湊㻟꿑㼂뵒㼃鲗㺃졉㺬Ⲛ㺡勒㺇鞳㻱颓㺫㹞㻵쟆㼋➟㺶譣㻅剽㺗撆㺭᎗㻴트㺞抛㺸ཛ㻼㈚㺯Ქ㻞ꄈ㻥퐭㺍ꙇ㻖ꛡ㺣﹨㺡홂㺹꜠㺻걎㻁㻎⧺㺨鐿㹰⺂㼅룹㻂Ͳ㻤裃㻃ꎱ㻅踱㻓";
static const char CONV1_B_ENC[] = "䰅뺖ዊ뵇묁셣㷔뛌븷踴빱뚐뺐⩴빴苴뵏ﶏ㺲챏㺆鄷㲥騉붶봹僠뭲㭡붗꽡뺇杰빊굠빏ࣶ뷦提㹀╅빾焗㻐寔㶁￿-빇煂㶕眸뺆舔㸛嬤㶲覫뺥㴏꤈먆鳶북褑뻕뢩븈뀨㺄嚏븮䌩㺮賧빩榳㺍뿣㺜븧빶⼄뺀諄㹂䉰뵤푁㲔犽㸖쫁뷒筫븍悆볋톭㱴햯㸉Ꮵ뷏睧뺁瑭미⵾㷨ᘩ봿䉢㻥ᶪ뻽벫븸￿֝㷒㶢븐◿븻￿ʎ㼋轻붌맛뺹⯨㰽摊㶇۾뺺ᨺ뺑喸뻜￿b뺵쯻붰븯⽦뷨댡북萔뺇싧뷰㻛㷟ࣥ붢캾뻎旑뱣줽㺸띦붙綾빲寚뵗䆒㹳븐쉲뻘䛋뺬붴諭㺤﹈뵨⛛뺇";
static const char CONV2_W_ENC[] = "\0℁‑ℓﻰ࿯㄀∣ခ䌁啓╕ሢ∱﻿ğἐā࿰ჰḡþᄑ㏳⌡ጳခ￰ሒℒℒℑ−∢∴￿ࠀÿ⌠㍃ཅ༐༐ðﻱ⃡ﻯ컼⃠̯️\0￿߿ဏĐ\"ჰ섞ﳏℓ⌀ခ⼐￿Ӟﻞ￾ฑ䌀￿ࠀ໮ﻠ࿯＀탮࿞Ǳ†ሂጰჰ볏㯾༖༐￞༏ﻯ㋮Ē섒ﻮᄑᄁ㌰∢ᄣ︀Ⅎ㌢Ữက』⌒ᅁ\x02ďꮫົ￿ࠀ㈲䌳￤퇍೻‐☰࿿\0῰ğ￿׽ᐲ㈳ﻳ᷾ǟￒ턟ﻟ῿䗿ሓ∲ิĀᅂ︎℞ᄑㄢᄃ￾䈎␢￿ۮჼ탞㈍쇿留ᄿỠ㔿呔ཆ﾿냺ሡ䌒／à࿰℀ㄣ㌢\0ȟᄑℑሑᄡ∁ᄡ쇮\x01῿ሁĐ．￿ࠀ໯ÿ\0⼀\x10༁ထ༑ﻠᇾሡġǰÿ\0ð\x0f‑ᄑᄑ࿰ༀሟđ１࿿࿰ჰ︀ขခĀ࿰῿／῰ခခဏ\0᧰ໜ\x01\0￿ۮ￯﻾\0āᄐᄑȑ῰＀ဏထᄑༀàﻯᄏ\x01＀￿ۮﻟℑᄁĀ\0\0\0ᇰሐ\x11ᄑᄑðᄏሡĒᄑ\x0fᴀ￿ࠀ࿰ჰᄑđﻰ￰໠ᇿሠᄑℑሒ／\x01℀ᄡᄁ༐\x10／￿ࠀ\x0fĀ\0ᄁ\x10ဟ\x01ᄑᄒ￮࿿ᇱĐ꼊༎￯ǿ\x0f﻾﻿က༏က\x11ᄀđ∢ሒǰἁတሑೋďခထ\x11ð￿ࠀ️￿ࠀ`āထᄡ∣Đ\0\0ἀထ\x01￿ࠀრ໰ Ȃ1ḁ࿁‮ﻐ￮ἑჱ℡ἑ＀෰῱滿㍄∢⌡Ｒ෰⼏∁ဏက\0ໞďᄀᄡ㼃⍂−췍ꯊᄩ∣໮？þ⇱጑ﷴᇿﴑრໍ༢￰ᴰᇱﴎᄁထ∢ㄒ퀞﻿࿾Ḯ༁ßᐞᄢ￿߮￿ࠀ\0⿰㋱㐒탾ჿ἞⃓Ｂ᳋\x11⇰\x01တဒกð∭〣ℂሠჰฐ ﻯῠÿᇰ㄄໱␏‒ἁ䌁￿ӎ࿿᐀ᄣ㌳⌏– ğထ\x0f℁℠ሐ⼣ᄑ•䉓Ȓ⋰ᄑģ࿲￿߁἟လက䄀ㅓ䈳∟⌡ဳᄐȲခ࿯⌾ℑᄑ㌂푃ỿฏḐᄠ⌂ἑᇱုᄑ∠ሐᄡ༂ﻟǾဠ⃰ᄁℂ ϿဏༀĀတဏ༐\x10Ἆď᷏\x0e\x0fတ\x12ď\x0f！＀ǿﻠ⃿︔ထ\x10༎\0ð\0Āက\0\0ༀñ\0ᄑᄀᄀ\x10\x11￰\x10࿿đ༁῾／ð뻢భဎᄀ༁ฐ０\x0f！ \0\0ༀďï࿠ဏĀð࿿ ᇾ︂⼀ἂð０⼁ἑ￱༟ǰñᄀ῿ᄀġﴯ῰ༀ\x1f\x11ခǰ\0\0ሀ￮ÿ῰အᄠ༡ༀ\0︀ฏ\x01￰\0\x10ༀ࿿\x0f￲ ໱￯ÿ\x0fကကတᄟᄑᄀ\x11ထđထðﴯỮ㿐\0ကĂฟᇰ\0ᄀ／ÿᄐတခ０࿿ᇿሞਠဟĐñ\x10đ∮\x0fǿက\x11ༀ\0î༁ကခ\x1fᷰ\x0e࿿༁\x0f\x11ჱ\0︎ﶜ\0\0ဏ༒ā﷮ﻰᄟǭ￰Ἇᄒထጢ\0ĐỰ￰￰\0༐￿ࠀ／\0Ā῿ðđ\x02ሐ탰￿׮ᇾᄑᄑñÿ࿰ďĲሁἀ뷞ﴟ࿿࿰ǯĀď࿿þᄁᄑ\x11ᇱﷺﴐ\x0eဏĀď\x1fᄏ \x12＀ﻮðༀ！þတ‒Đ ￿ࠀȟ∢ĳ＀ﻮðကﻯ￿ࠀð ᄒ\0\x10\x0fÿ\x1f\0\x02￱༏ĂĀþð\0￰ἀĂ℠ℐġ\0༏\0ကā࿿ğ༏\x10\0đฑᄀ￾＀﻿￾ᄀ\0 ᄣ℡\x10ခ㐰ሲℒሒȡကð￿܏䯏⃰ℑ㈣−\0ǿ⌱ထ༑\0໡\x0fᇠก嗰㐵ቃ̠＀ༀ∯ထᄑ́‐ℒ⌡༏ჱđ췞￿ӝ�ဏჱ\0༐Ȃ℡Ȃ⼑仢ჾǡ༑￡⿱퇎࿿ჰ\x0fï\0￰⿿ἡˡ？ḁჿ？⸂༼￰＀￿ࠀ＀ༀတༀ⿯ሐဟ퀟໿ḯ⏣ἑǡ ȡ㸁ሐ숁ఱჁĀ༐\x1fĀ켟Ǒἀ⃰ı࿰丟ˣﻮ໾༁？⿱฼￰໿ἐ\0\0０ﻡÿǰ࿱ñﴞ῿Ȁἐ\x0f\0ბ̎ﻱἰǂĀ ＀﻿@áᄠἀတ\x11ā\x11\x0e\0ン̎⋣Ĝ￰໰ዿฝ㈂ጁခ˰⃱ሟ︐࿾ﻯďထ⸀Ἶ༓ℑᄑ鏁亂኱ᄑĐ턟ⰱ￰ฏᄏ팁⸢Ⳓ\x1fἒሐሒ㼲˳ᄀđ⼁‟Ȃ⸠싌ज䈀ჰ጑ℱⰒ샎മāภ⇡＀໰￾Đ、ဏᄐ⼑ጟἐ`ᄀ῱ჽᄐĐჰᄑ￿߿ჾༀჰ\x0f࿰ᄐ\x01ကò\x10\0ሟတฟ\x0f\x10\0\x10ᄐထဒāðᄀᄐᄑ༒\x0fḀዯĐༀჱ἞āǿā￾ခ༟Ȑ﻿ď\x0fᄐ ก࿟＀\x0fﻞ￿ࠀ￿ࠀ\x0f༏︐\x0f฀῟༎ᄏ‐Đကሑ\x01！\x01༏\x0f\x01þ⌠\0ἀ\x11Āġထ∐\x0fᄐခༀĀༀჿሁထᇰ\x0f\x0f￿ࠀ﻿〠\x0fထ࿱ჱ＀＀˽༰࿮༏Ā\0\0ἀ࿰ᄐđ෰﻾ꀀ༎Ựā\x11‑ᄎကᄁᄑᄐđሀñༀတ\x1f༝ïǱဂ࿯ð\x0fထฑ￿ࠀ࿱ሟ࿱ကā／ༀခĀᄁ\0య\x01\x01ሑအĀༀἁჿༀဎĀ\0āﻰ࿾ฏ０໰ἀᄁ࿱ἐ\x02က\x1fᄀ\0Ā\x10\0\0Ā἟ᄀᄂ탾️ā༐ðကထᄁᄒ⇱℡ሒዒ\x01ༀ퇟￱ἀ฀ᇿ／༁ჰሟ−！￿Ϯᇱñÿ￿۾ༀ️ÿ໰⃰Ġᄁᄏ‐Ḁ＀ဏℐ￰￿ۯ໾Ἇð！࿡ᄀᄟᇱ\x0fĀ\0\0đ\x01ሐ༁ჿᇰ\x11ༀð༟đðヿዱༀ໾Ḏ￰￠ℑȐĐ\0\x01＀ကĀĀȀ∀ฐ\x10\0ᄀ\x10ĂᇱĐ༐∑໿တ鄠㧫\x11닱\0\0ကㄑჰ！￰฀／？℁℠đᄐĠἀೠǯሟ℁㈑ሏጣ１Ḃ／ᄀᄑĀﻡ⃿⌀ℰ\0ༀÿ턎\x0f℀Đ\x01Đἀ∡﷓ჯ̓༡ğ‑‣∂ð￿׭᳜ᄀ␡ĀကﻰȀἁἡựðἀᄠā࿾￿ߌༀჰ⌡\0ൂ￿ۯÿℐ䑃≄ἂÿကàጐ\x01勱ሴ＀ℐ࿯䌀ခჰĂ￿܍\x1e∀ሁᄲᇲﻰ ￡￼぀\x04\0ഁᄏ\0﻽ᄡ℀ሑð㈢༁þ㿾㍃ᄒ\x01ᇱ˯࿟⌏ጣ῾탿￿׽ᇭᄡဢđსတခ༁℡Ģ࿱ჰ⋱ጲ༁ဏđ＀⻞ß∀볐\0\0？仯ᄐ￿ࠀ￰ÿ‐￿؀﷿㉂䈵—˽ﻠῠを㐭ሐ㔟￰ㇰሂထ﷮﻾뷜ဝက㄁㐲͂ဿ∡ἁðῠğ໡ﶽᅀ \0\x01∟−ශ⃯ἔ￿߿Āㄡ\x02\0ǰ໰Ǿ ∑\0Ḑᄀᄑﴑᇰ\0ǰ￿ࠀà࿿ဏ\x10༏\x0f༏ထᄀἀ\x0fက\x01\x01\0ᄁထ\x10Ā\x10＀￾ \x01￿ࠀༀ\0\x0fᇮđ\x11ထĐ￿׮Č\0\0\0࿿￰∲∢\x02ကḁሟ‑ༀﻰ￮／ჿďတᄍ！ဢ︀ð﻿།ð︀ༀ\x01＀￰\0\0࿠\0က￠\x01Ā㌰￿۱໿\0\0đ￿ࠀᇾđတ\0༐\0\0ሟ‑กᄏᄑℑက\0＀１á\0࿿āဟðÿဟĠἀ！￰က℀ሒ℡∂ሡð！\x0e￮‐ᄢ\x11\0༁∰!რ섑\x1f﻾ჿ\x0e\x10℀ሒᄒဢđ\0ༀတᄑ༡\0쀀﻿￿ׯဠ\x12Ā\0ğ␑\x0eሠ\0\0ခᄀǱờ༁\x01\x01／\0\x01࿰࿮࿿\x0f￾ᄏᄀᄐ ᄁ／￰￯࿿ᄀᄀᄑထ\x01တ\x01\0\0℁⌡༏ခ\x10￮࿿\0\0￿ࠀ￮￾ᇿᄐༀༀﻰἏ\x11Āđကāတℂ\x01\x10က⌑ÿ\0ༀ￰／ð\0￿ࠀ⇰ሑ࿿ༀĀ℡\x12῿℡ᄒḁᇱ\x0fတ\x11ᄐ\x10\0\0Ā\0đတ\x11Đㄑ㌣Ē\0ထ\x01ÿᄐđᄑĀ໰ჰ\0／￯ÿဏᄑᄀ༏ༀ￰￿ࠀကᄒᄁ\0ကᄐðက\0＀༏ᄐ\"࿰ကđဒ\0\x10༏\0ထ￾\x10∀ሠ／￰ᄐ\0￰￿ࠀᇯĀ࿰ÿᄟ㌳၃ሀᄠℑᄑဒ￱￰ďℐ唒睥￿ࠀကሐȡ！￿ࠀ࿯࿰ðᄑᄑ￱ༀ䐀啔ф\0ᄁ\0ᄑ\x01\0Āሠဏᄁ￿ࠀď\x10\0￿ࠀÿ\x0fĀȑ⋰ㄑᓰḑᄍ￿ࠀ\x10℁ဂ샮࿿\x0f\0ကā』༱쮠ဠဣ￿ۜხȞူᄀďထᄁဟခ⃰″Ǳက\x11㈡ሂ﷿￞￿߿ﯰ￿߭ဎỐထ\x10ð¼．໠ᄀ\0탱틱ý\0￿׾ß￿ܭ\x11ñ࿿ἯĀထ️ᄀᇮჲﻭ໠ᄟ￿ࠀ￿ۿ卮ᄑ￿לෝༀðďā\0ጐġ￡＀ᄑġሰℒ⌁•탲췝ﻮ\0໰ᄟሑတ⇳틠㈃̯༓￰ᄀဒ࿰ἁ\x0f\x01㓯ဒ⻐병ဏ\0\0Ā？䄀㈤℁1㉂￿ܠᇱ￿ϝ￿ۭ￿ώ匳࿿ሀᄡ ἑ￿ߏ컭̍쀀⼐Ǡ∠﻿ᄯĂ⼍ሯ⃣⃡ထ\x01ﴀà⋿Ē‐ᄢ⻱ℂ̑ᄁᄑჿထ℀ሡထÞᄏ\0đ∡࿰༟ჰ﵀⌁�ༀဏ∐℡•ჰ࿠Đð！￿߯࿿ᄿჱᄠāတᄑ턀＀ዮ䄒ጢ。࿯ༀℐ∣༂\0ༀℑ⌑ἁ࿰￿ܐଠዠ퀏ǰ＞ā￿׮Þ᰿䃳ჯ︠༎￿ԝ﷯﻿⃿‐༏￰ဂ⇰贈￿֬⸏⋰ȰＣ`℟ሡᴑ￿۟࿾묏\x1fǰတ ༑ñ\x0fᄁℑ̑⌡￿ܒỠ탽\0ထ￿۞￿܀￿ࠀ﻾ữ℡ȁįἱﻠ택븎ྐā‑/‒࿱࿰０ﻮỰ．࿰ฏ？༐ἑ˰῿ᄑ̱＂᷑셎㼱﷏Ȓᄡ\x12ᄓワĐဂ℟đ\x10⃰ጡ༑\x0fჿᄠᄁĞ㌂༮⇒࿰Āᄡȁㄟ᷿⅏ფᄀĀዿༀᄁဓ༁࿯켟ᄏ\x10ἑĠȀ༏儃ጒ༂ﺰǽ\x0fༀἢȁ༐\x01က１ﻯ⿰䌣‒>༳\x10￱ဂ°￿ࠀ\0￰ﻯ／ǭ턱အደ˰ሡἐ࿡࿿ሣဏ\x0f∑㐥１￰΅࿾ǰ\x0f︟ဒᄑ྿ǯ㰿⃰촡ï෿\0ˮ฿ခ︎࿰！໮️ϡĀჰ 츝ῑ∞ሢခ\x01\0ᄀ！￿ӿௌ᷿ᄟḁñ／\x1fဣ༒℡\x1f࿱ﷰჲ티ฟ￠໯ÿ\0．໾￿ࠀ⋻ﴎ￰ᄏ㌀ˠ仼ᄒĐﴜကခ』⸃༏ሠȟᇮᄟ䃰᷑ሿ䔓瘳ᄒㄱἤ촃ༀ￱༮\x01ᄏ\x0fအက༁ἐሁ䄑䗳Q㍁匔ἐñᄁȑტ涠ཁﷱ\x10༁\x0f퀏ﷰÿჲᄒ￯ἀကሠᄣထ\0㈢∣ﳝￚ࿰࿿\x11အሒð＀￯࿿ℑð℀０ჱȏ༑ᄁĂǯðༀð࿿￯෭ထñᄁ໮\x10ကñ࿿āᄀ‒Ā䔑噓ᕔ༁ữ࿯Āἒ￠ÿ䀾ㄒ︡࿿ჿā/\x02࿿ᄐ＀／\0 ℁ᄑ‟ᅁ⅂０⇓ñကᄐĐ෠켍￿׮Ởᄀ̑\0\0￿߾ǿℒሂᄁჿ\0ἁသጂ１﻿`ထ†ხ￾ðဏ∢̀ᄀȁထ\0\x01ሁ㼐㌱ㄳ️￠ᇾᄁ﷯üༀ‐Ｍḁ\0࿰탯唳啷\x07฀ခ㿲ἁğ ဢĐᄑ⋱≂⼑ȟထᄁ㴑ℱ․㈣\0Āሏā⼒෯ἑȁ༎ᄀ\x01ð︀￿ۭෝჱď໱ㄐ\x10ἀထ\x01༐\0࿿﻾﻾ữᄒ\0ခ\x01ကĀ\0\0༁ǿᄑ\0\0฀ğ࿰\x0f／࿰￿ࠀ⻮áჿ࿣Ā\x01￿߾ἀᄁāကခ\x01༐ ᇲğ㐑ጰＣἀāÿრ\x0f༏ჱᇿဣᄑ\"＀ሟ࿱þǯĐἡᄎ\0ༀ Ă\0࿰ÿ￿ࠀ΅\0Ἇሒ∢ᄡ\x11ȑḑᄀထǾĠ︀￿ࠀ㿿ǣჿ\x10\0࿿ༀ\x0f࿠ď฀ἐჱ\x01\0ໟ\x0eďǱď⼏ñ￿ࠀ／ༀ`⼑Ǳᄏᇱàﻰð\0໰㈑໿༑㏿ሲ㈢ሓ℠ሁ∡ᄡἒď\0⼀ᄁ\x1fჲ∁࿰ď℡ᄁ︐／ༀ\x02Ỏď\x01ἁ　⌡ᄢᇲ\0\0ᄀ฀ČĐῲǰ\x01\x0f\0︀￾῿đ￿ؐတñကᄁဟ\x1fခ⸢ᄡᄀￍĎğ㄀ᄑ＀￾ￏþ／\0༁\x02\x11\x0fđ˿\x01\x0f༐ჱሐ῰ﻱᄎ Āᇠ࿲῿ᄀĀ࿡ጫ\x01∁Ȑ０ฐ‑ደ༏ℎෟ\0ᄀ\x12တȳ࿳￱⃿⋡́￿Ԁ�ༀ῰Ȑ／￿ۮĐℑᄑခ‟῰‐ð`∁ℂᄀჿ\x0f῿∀ἂෟကကᄀĐㄑჳᄟ∁đἑခ̰ሢ༑໰ᇿἑခḏ\x01￿ײ￿۞Ǟ툑ᇰ\x10အထ？︁໰Ǿ\x0fჿ\x01\x10－༡퀏ἐဢựÿ⋲￢㳮⊢ጂÞ＂\0】℃ï￰ᄑ\0\x01ჾሟሡ ሐð！﻿⼀鿰ḯỰༀ￿ࠀğℑขἐ￾\0က฀ሎခďခༀ￮\x0fℒဒ Đ￿ԟﻰ䈴ȓ\x0f㋬㐱⌳⇱Đ\x0f＀ﻠ‏ℒሐฐခᄡဢሡ﻾Ⅎဓዯℐ\x10\x11ဠᄒîထð ሐ‒ခထȏ℀䀁嘤ł﻾\x10༁℀ထð\0တᄁ￰ጱ퐤￿ԑ﯎⿟ᄒᄑሑᄰ༄࿐＀￰／\x12ﻐ﷠ワሑ™ᄡ࿮ℎሑ\x10︯﷢࿏þ￿ࠀༀ\x1f︁ሑđ\0Ā￿Ӿಮ﻿ဂǿᄰᄣᇰထ㄁ᐂအ༁ᇰ￿ӭ໿︑㈲㈳\x10＀࿿ᇿϿ﻾ῠᇾ™ᄃ໡ğ￿ߝ⻞ᄢȣἒ∑‡ሒ∲‣ℂ⿰Ģ㈲™⌓Ȃᄰဒ￿߮︟࿲ ỿᄟ∠᝷༡ᇱÿༀᄐ䈢ก∐ဢǲ\x0fÿ︀﷟ჯ！＞\x0fþᄐĒĐ⼐\x11࿱࿟\x0fĀတđἁ\0︀࿠\x0e⃰ℑǱ໾\x01āဟ\0\0\x11\0໱ᇰထā࿱ᇠᇿȑᄑ︀ༀ\0￠\x01ᄜÿ⼑΅\x10࿿\0／\0တ￞\x0f︐〡툀\x0fðÿñ\x01＀￰࿿ÿ＀\0\0ā\x0fĞ•ကĐñ\x01ሀ΅ﻱ￿ࠀ\0\x1f．ဏჰ῰į\x10\0\0ကᄑᄑ༑ကð\x0fð༐ကþ࿰ༀ\x1fđ༒þ\0\0 ‐\x0fကἁǿ\x0f⿿ጟò／ჯༀÿ\0ခ\0ðᄐĀༀჯ!ᄡǰＱ\x01\0\x01ሑ켐⇏﷠ᄐထกþᄟ℀ȡ㌡Ǡထ−￿̢ᄙßሐℂ\0ÿ\0ἀď︀ᄀက０ᄐ\x11ᄑ࿿đĐက\0ထ\x12\0\x11\0ÿ⿲༏࿯ဏᄁ\x10\x10ᄑ࿿ༀď༐ï￠῿Āᄑ\x11ÿ\0༑࿰\0\0⸀ϰᄟĒ!ခᇿᄑ\x11\0က\0ᄂတĀက\x12 ᄡ！￿ࠀ\x01Ā\0\0ჿ\x10\x01῰ď\x11ဂﰒ￿׍à\x10\0⿱\x0eἎ࿰￠︀￯Āഝ῿‏ᄐ℠ᄁ\0က\x10Ē\0໿\0⿱ก࿾ď\x10ሑ￰Ā\0\0က‑\x02༁⼀\0ÿ폐︭\x11ခ⃰ǿတℒđĀ퇾᳢တ\x10\x01ðἀǰ℀࿿\x01ကḁჰထĀğ̡０Ǡ\0섁뾹⇠㈐ဏāāခတဂ\x11ခᄠᄑᄒ０࿿\0\0က\x0f℀ᄑ\"\x10ထ\0＀အᄁἑဟ⃯ġā༟ᄁāༀ࿰ᄀĐ‑\0࿿đ\0Ā‑̀࿡ˠༀἁ\x10\0\x0fἀ࿐￿߯ﻭဟĀ\x01Āἑ⃡ᄀတခ？ဟ\x10\x0f\0\x10\x10\0ༀ\x01ༀᇱ∑ǰ\x10\x01༏ñ༏ðğ＀￾໭đ⼁\0\x11\x11།ý￰\x10 Ȁༀ℁Ȁ￿װ퇯ჳ\0ฐ￑࿿ༀᄁἐ༒à\x0eက０ﻱÿᄠǮ０෯ก￿ࠀ＀࿰\0ᄐﻮထāሡᄁ\x11ᄁﻰჽἁ\x01἟\x1fကᄀሑထ࿰࿿￿װﳐ퇿￿׼‏ခᄀ\x10\0ðà섏ภขༀÿကāĒ껿က\x10＀ﻯ￯鼠ﳍჭȀ℡\x02 \x10࿱\x0fༀ￿ࠀဟđထĀကȁ‑ﻰဏđĢሐᄣ\x10㄂\0\0ᄁđ\x0f⿰ጀ剂\0῰ĐᄐĠတ\0ခခĀ￿ࠀ`⃲Ȑထđㄳ㌒ﲽዝ￡⼐ᄒȠ฀ἁᄏñ￰ჿሓđ‒∁ḁ࿐ñ༞＀￰༎ﻰ⇯ༀᄢ༃ᄁÿǰᄐℐñတ༁ģᇿ愁戄ﻮრ\x11ðā\0ฑàᐿỠ࿱￿ࠀ࿿ ጡἡᇱĎ˰\x10Ἇ＀ༀᇱᄀ‑∂࿰ሏȡ‑ᄀ퀐ἁჱ／ﻯ⇯ᄑđ\x11ðÿ‐āထဒ﷾ﻎ\"ᇢ࿱⃰\x12ȯð∁òď\x1f\0ᄑǱἁȐ￿ࠀ༏ÿ㸏̃ÿ￾࿰ᇿᄁ』ስ℡ༀ䑏đ℀ሒḀሲ琲￿ࠀ㇯∤℣ℓ＀ÿሠđထက\0àğ\x01＀﻾䏰㌳ℱ㈓ሒ\0Ȑထ䈂Ēữ−ȃ／\x10￠﷮\0Ē쿼\x0fᄑᄁᄀ￿ࠀἐᄑက\0\x0fĐĐᄑᄑ\0\0࿰\0\x0fༀჰ\x0f＀￠￿ࠀ￿ࠀď￿؟໿တထ༑໎∟Ƞထ\0ð\x0fᄐĠ\x11\0࿿￿܏﻿ထ\x11￿ࠀ￿ࠀÿ￿؀﷡￾ĀﻰĀ\0က\x0f￿؏ð\x0f￰\x0fᄀ\0\0࿰ჱﻰ࿯＀￿ࠀༀ\x0f\0\x10ἁﳱﺾ\r\x11࿰\x10Đ￾ᇿᄐＡ￮﻾\x10\x01ሀጡ\0ĀĐġထᄀ\x0f࿰ā\0ༀ\x1fༀ⃰ℑȂðᄑāĐ\x01က￿Ԑက\0￱\x10ἁခÐༀ￿ࠀᄀᄑđᇰ\0တᄑℑĢğ１ᄁ༁鸍￿ࠀ\x0fሀĐกÿༀကᄀတတခᄀᄑ−࿿￿ࠀ༏ā༏ჿတ℁ﻯïĀĐ!\x10℀ᄑ\x01\0\x01￰ÿ\0ᄀℂᄑữǾဏ!⇱đ\0ဒေဢ／출༌⻽ἣ̱ﻰ０ሏ켏Ǟ໱∓ထ†ᄁ༠⿲ﻮ\0￾ﻯﳿ࿰⼀\x1fሒñ℁턀໾∂ℒ࿲ï￰ďEს\x10凤＀࿿턡ðÁᄏ໰ď⼂\x10⁀凲ｂ￿ࠀℒἀȏďჰ\x1f￿ԀȐ？῰ヱǱㄡሢᄱ\x0e℡Ṅἐ࿟༁\x1e࿡࿮ÿĒȏàď﻿ğ℀ḣ\x01㻱ﴀ῟？ᄂ࿱㄁ሓ㉀㿟\"\x01Ἆ１ჰᄐ켝ഀᄯ／　ሂı쿮￿לဟฏᄀထ仮Ȥ㄰ထጡሳℑጢἐ῿ጀ̡挢￳㇮⃴˲䏴ȁ࿡\x0eሰ‑ᄒ‡ༀđĠÿထ㌑㐴⅁ㄳ℣\x1fခᄯᄒ\x12ȿἎ−⌡⌏အ￿߯໯ᄏȐထ\x01Đᄑᄐ\x10\x0f￰āĐ￿ࠀÿ\0ကထᄐ\0\0ༀð／￰\x0fᄑ\0ༀ\0\x01\x10\0＀\0\0\0⋿ᄑđ\0\x01\0ᄐᄑ\0\0\0\0ᄑđ\x11\0฀\x01\0ðàကďᄀༀ\0／￿ࠀက\0ᴐက\x11\0༏\0￠ð࿿！￿ࠀÿ\x0f\0ༀ\0\0￰ﻞþ\0Đ\x11ခ\x0f\0ထ༁￰࿿\x0f࿿￿ࠀ￿ࠀዿ∑đ￿ࠀ￰ༀༀğđခāက\0\0\0\0࿰\x0f༏ჰᄁĐ\0\0\0\x10ကĀကༀ\0က￰\0ĀခĀထሑ￿ࠀ\x0f\0ကကᄑᄐᄑ\x01Đခ\x10ᄑ\x11ကခሐ鼌\0\0Āᄑā\x0f\0\0\0\0ကᄐᄑတℑᄑ࿿ðᄀĐ\0ဏĐထༀ＀ᄐđ０༏ჰᄑ\x01\0\x01\0Āÿ￰῿ᄑāñᄐđကက\x0eÿༀᄑ￿؀Ἇð\x0fတက\0\0￿ࠀ༟࿰ༀခထ￱\x10ခ\x01ჿℒ／༏\0\0\0ĀĀထ\x11ᄑā\0ðđﻰðἐȀᇱﴀǟဒ෰༏\x01ǰÿ\0ༀ\x11ༀ\0ð\x0fﳰခ\0က⃾︀á༏ǳༀ\0￿ࠀ�ἀჰďခÿဠò￟࿿þ!ჰἀðĀ࿰ᄏᄑ탮\x0f\x01\0ᄐĐĐก／Ā\0ð༞ð࿰ ᄐ\x11\x10\x11༁໿ฏကဟ\x10\0࿰ïḁሠď\x01\0\x0f\x01Ā℀ȀᄀĐđဒ\x10＞￯ᄑ鼡\x0f\0ᇰᄡༀတ\0ༀဟĀကတĀᄟกﻰðሒ‟ሁἠð\0Āᄑ༁ᄀ∡퇱က\x11￰ā\0\0ðᇰထ\x01쇯℀\0ἁሑı\x01ဟ䈂ሢထû\0\0ဢᄁ￯℀໱࿿⿠ሣἑñ\x12ခ\x10\0ကāàā\0ჿàᄎ༣ᴑ㌟ᄐ\x11￡ð໰Ḑ￿؏\x1d＀㈀䔣﻿༎￿ࠀ෰࿽ā\0ༀတ༡ထЎ∴/￡\x0f﻿ደ„\x10à￯࿱￿ࠀ？⇱∂ǰñ ＀Ďἀ\0࿰ဏ༁￯ᄐῠ⋿༑︀\0Ā＀ἒ！ဢȒÿð῰\x0fĐ༑Ò฀`ἁá༏࿿༏༏\0\0￠\x0f฀⼐⋣ﴠĀ࿠￾\x01\0āᄂﻰ\x10༑΅／ムㄡ꓿㌢♄࿵㄀∑퓠ῑﷰ༁ༀ\0ǽđ\x10ᄃጐ༏\x0f∂ฑጏİ刣\x0f!đḐ἞ເဂ⤀Ā\0ḏༀᇠༀȒἒ῱ἀđ\x1fἐ．\x0f࿱ð\x1eďℐထༀĀ\x01ထ\x01࿠￿܃฀ᇱġༀༀကက\x11￿܀࿰\x0fတဂǱ࿿đ༑ༀ\x01\x10∑\x11￰࿠＞ഁ⼁āÿჿ￾Ā＀ﴏ￮ÿ\0\0ǰĠȟ￰ß⸎á\x0fༀĀ༏ǰ＀໰ደᄠǾ０ȑ༁！฀️￿܎ἀðჰခġ️／\x10\0Đᄀ༑ᄀჰကïἏጐđἡð\0ἀ￿߾῰￿܎࿰῰\x11໰ï－༎\x10ဂഏ＀／ðကᄁሑᄐĐđ༁ሡ−ထ퀀￿ؙᷱ￡샽ñက℀Ēༀ∀ÿἁᄠᄀᄐᄑ༑ἀ￠࿰￾섀ጠใǿᄑ࿿ဟᄀᄁሒༀȎᄢ\0\0က࿿ᄀထထ༂⿏ჭď∑ጣ㑇︢àຜ⇿￲ﻰㄡ࿲ዱ￿܀༟０﷠ ဦĿဟȀက‐ု῱ዿ Đ࿏⇾᷾ἣȐሡတ໡￿Ԑ￿׭ᇿခ㈑䐒䍇ข츏﻿⿭ฑῠ࿾\x1eᄢ￿ࠀ㉐ᄳ\x04တჱÏ￿̞ခໟþ￿߯ᄁჲᄑ＀ㄟƿ㇐㈳⯀⏯툱䀾탿ᇠḁ෭ﷀ￿۾￿߮᷾Ȟ￿Ѐ￿Ҿ￱ဎȢ〳㈒ᄒᷮ\0℀⼂￿۰Ἕ틱ༀ\0ℒ༢ሎ∡ᄐᄀἀ쀢볝໾\x0fāﻮ⋜ē⼁∢츎ᄮ⼑ℑခ︎ጝᄒഠᄴữᄂ￿ܟ䍄㕖⍃ᄐ᷿′༃샮༁＂༏ᄒᏠ∱ģㄱ䑒ﴀ￿߾䀳‥̐⅂ᅆሓ\x0fᄯဂ༒ȏ ℐกᇿ2ခ∟̰￭⋾Ģÿ￿ܐ￬ǯĐỀခᄎ￮\x1f̓\0ā\x11ဟ섐／ฏþᄑ␑ဂༀ\x1f\x11༏\0︀Îჰ῰㌐\x03ሞᇿĐﶼǾᄡ턑Ĝሀ⋰\x01 ∡ခᄀฑ￰넒᾽Ự\x11ㄑǿሎ࿱⋯ሑ∀￣ᄑက￮ჿ∑⻿ᄒ༂ᄁ￿ࠀ￿ࠀį⿾#Ā￿ؐᄜ\0∐ሑ!ခ℀\x0f쨐ﻮᄀĀ\x10ჰᄑ１켐﷮　ሳ\x01ဏđ℡℁Ａᄟ∑Ĳᇱထကñ\x11준༏༂ကàᄏ\x01Ȣꨁ༉Ġἀā⌱đĀ／῱Ǿ￿ࠀ먏㿿ቄᄒﻙĀﻲā널ºñþᄑ첡࿰ჰá⃰㄁∣ñÿ\0‏턢ℑကᄑﻡ￾ď࿠\x10￿׮ῠď￿ࠀ￿۟က\x11\x0fက\0\0ༀ\x0f\0￰Ā￿ࠀ℀đﻱ࿞\x10Đᄁ\x01Đჿ༏à໰ð￿ؑ﷮ሲ＀ð\x10￰ဟāကᄐĀༀἀ࿿῰￰ჿǮð\x0f￱⿿ထ\x0fᄟǱἁ﷾ༀ\x10ǿ￰＀ᄁỰဒჱ\x10Ḑ￿ࠀﻠက༏ఀဎჿà༟︎တᄁထℏðᄑ\x11࿿\x0eἏ∁\x02\x01\0ᇯ．ữ＀ÿ\0０࿰\0Ā\x11ခခ༃￿ࠀ⇱ሡðĐ጑Ǳༀἀᄐℑἁ！ý℮턐Ý￾༏\x10￰ÿ࿠ᄀᄑ￐῎\x10\0\x10ᄀ\0တ\x11ᄠȑ༠ᄠሒ℡\x01ꫮ￿ƾሀ넞\x0f\0１ༀჿ\" ༀ\x10\x0fᄀᄐᄑကထ￿ࠀ\0ÿ\0⋰⌡ሠฐðሠāἂᄁῢ\0Ā︐⇮࿲ÿï\x10\0Ǳāđ༐ሐጢᄑ༁฀ထ\0￿ࠀᇿက∲\x13\x10\x11￿ࠀ࿿／࿰￯ဠ\0ጒȢý Ē\0ᄀ\0\0ዾ＃ᄝ໭ဂ\0ခ￿ࠀ࿿븀\x01Ở\x0f鳑ሏ＂／༏Ἇဏჿℑ໾ကသ໮Ǟᄏ༁\x0f⃰〢￿Б￰\0℀￰Ƞ！ༀġሯÿ−Ģ⿲㈐턒෭￱쳰ჿℓ￿ӑ῾＀\x0e⃾㑏⼱ᄢ\x0fᄐㄢ∓㌢໱￰￿ࠀ️ğ￾‏℁໔⇿Ā／þሀᅄ\x11ᄠđ＀ሐﻯዿ퀁တꯠ‌﻾⋟\x02ᄑ︢선⇞ሀ뿱↻∠\x02￰ဏñ⇾ᄒ￿ܑᄁ\x01Ỿሞ츀．ἑ￿ܒ꿽῰∁ჱ\x0f\x0fෞჰ︎\x0e℮༏\x10໿༑࿰ἑ︀ဠğヿ.‒ᄁ™￣￿۾Ǯ\x01　̠‱āဏﻰ－\x0f࿮℁ㇳ⌱ô࿿ሏℒĒǱ／￱ˠ‡\x01 ðﻯ￿ࠀტༀༀሲȡò࿾䈟嘤àï῰ဏ￿܍༁ヤȀﷰ␾䅔Đထ￿׮？ð༑ñㄡ￿ࠀ໿／ℐℰ〣́∠㈴ȡ\x01̐ᄡᄐሂðᄀĐ￰༏\x01ကďᇡĐ∏ğḢ퇿พ⼑￮‟俐ቁအĐ⼒໮\x0eĐဓℐℒῠ⿿伐㐔Ȱ￿ࠀð฀ǿခ䈒ᔱ㽅ᇰĐ̑ᐣㄒā〟㉂㈵ဒခ㄀ሓᄐđ•ጐ@\x01\0℁ሱ㌒།￿ߠ\x1e\0ሀ䌣ሲ\x02༠⇳ሁ\x1fㄏጠအᄠ–ᄲ㈂࿰\x0f⿯!†ሤᄂﻐ쿼ð＀῰\x0f￿،⻐ﻯ￰⃾ဢჱဢ \x10࿿က\x01࿿ﻠ㈐〢 ＀೾ˠᇰǠ ∢ﷲ￿ԏ௠῱퀐ᇾȐἲ﻿ﻯ냰봝ǲༀ⋱ВἁĀခḑ뼮⇾á῰ᄐῢ탾㾿순ሰሡ∳吤␓︱໯þ ⌢︀∡㈢ﴑǲþဒ⌡㈲อဓတ⇠รἑǱ﷿Ǳ῾ဟ㼑࿜∀ᄡ뿿Ἥဂ⓱㍂̴⋀\0ခᅁ∣༒￯俾㈳ሁᄂᄒ጑㈲⸴ἃက⌱ሳﯰĎ탫ᴟᄂ￯搀㙒쨏ඡᄑအề퇭\x0fᄁ㈯䔣ТĐ䐰ﻦ￿߾໠ᇑ쇭쿹䈢ᄿ퀐⌍㍂㍅\0ℑᄑ෿ᄟཐ⻡⓮㍂￿߾ﻮ∞㈲䷃ጬďȠ쿜￿׬쳜￯\0℀‡﷽ჰ࿰﻾㻰ˡဠ༁￿߮ჱက‡Ǡ\0ᇡ!\x01Ā\x01⿱ā\0廿卆␲−Ĳỡÿᄀᄑ⃾ᇱ࿰\0 퇱ฑ㼂ㅃ∂￿ۮ髌ș∢༤￿۠໾︐ㄒ␢︄ðﴀსᄍ\"໱컭︝໿탐ñဒሢđㄠ︤ÿ﻿￿߮ḃǠȟ•Ă뻾￿ϯ࿿\x10ñ䃰ϠᴁഞǱ￿۝ᷛ༒℀\0ᇾᄢȢ￱￿ࠀልထ㄁∱ቄ\x0f︀ሢ퀟컐Ⴠﻮჿ㈄쀟༯Џ℡Գ⇿⌑﷯ဏ䈒Ȃ㉂−ሳ῰ሯđဏሀ䄟ፂℳďℲ呢̓㏰ȡ￿ϐ￿׎༏煆\x01탾༐ㇰ䅆㌒࿿䑀䌳U∀ᄎ￿ߟ﯍჎ሽခ㄀㐂ỿˮတℑἑℂ〡ሀἮ℃ሡ༏ἐ０ﻮᄀ༐⿯ሑĀတ!ခ ᄱᄑ/ჯᄟᄑ\x10Ā἟ ⋱࿼︐რ\0က༁ð￱ﻡ༟㈒ᄐ︀\x10đᇱᄏሢᄡᄡሂ῰\x10ᄁፃĢ\x10￿ࠀ택̳༐￿נ﷎Ꮿူϲ\0࿱￮࿰῰Ġﻰ༐ℑð༁￯￿ࠀﻯǟ︟ༀñ️ǿ＀ༀ‏ǰἏȟ켁ဎ䈒￿۠￿ۍᄝጔ\x02༁῿ǲ퀍\0￿ࠀᇿ䐲ġﻯခထሐġ༐∠\0࿽ǰ༁ἀð༏ჰ∐\x01ğ༂ﻰ￠￿؏ㄯ￱￿܍﷯ༀထā⼑Ȃİ\x12ÿĀༀฏ㻿3ἂđ∡ጣ￿ؑ࿮ꄁ맫﷬⼢ༀⓠ༲\x12တᄀ⿿\x11￿ې＀သᄀ˿ð￿˝ċ‑ᇰℂ̀㈑ȓᇰġđᄒ⼐ᄁĂ⌰䈳တ™ᄁÿ\0０∏ ໿ༀᄁĢĀ㊲ð탾᳿⻽༡\x10ༀā️ჰñဏᄠ䈢℡−῰Ǡฎ.\0࿰／ༀ\x01ĒုĠ샿짞⸉㈂ᄱ῱༏ðᄁጯ༅ჰ໱༠༡ကዡ฀àሞ༎\x0f∑ñጠᄯ䌕￝῾\x0fတ฀ဏჰᄀ!￿Ԁ￿םď῱ǰ\x1f\x02Ǳğ〰\x10࿾῾\x01\0ď໾ዮĒ\x1f࿾ჿထ２㸂 ñ／￿׿àð㸠倂ᐑ἟ᇱ℟Ἧď￿ࠀ￿ࠀฎà࿱ሁ㼒ÿ\0\x10\x10῾ᄒ㈱ༀ༁䑀∳∑ሒሒჰሑ\0ჱ，ᇾ\0ℐ‰䈡Ǳğ㐱∡턤ထ￿܁࿯༏\x0e\x01ჰ㌑䌳༐ἁᄡ∔༁˱ဏᄃ∐῿\0\x0f￿׮￭⿰ἂ༏￿ࠀð\x10࿱ᄐ࿱ဏンᄀဟ￰ჿ\x0f\0ჲᄀ࿿ð＀ð￰⃱ð\x01༂\0ထĀ℁ἀ￿ࠀက\0ð‏Đ１໰＀\0Đἠ༏￿ࠀ༏ἱ\x11\0ḁ㈁Ġ！ð￿۾༎＀\0࿿ᄀဏǰď／໰\x0f෰턌．Ǒ༁￰턏️ᄐᇠᄐ\0ᇱก．ἁ\0￿ࠀ\x1f\0Āǰ￾༏⃽ğ෱༏ā༐ᄟထ！\x1fჰထ\0 áༀð\x11ïฐഏ࿿࿿ï탽\0\x01ð\x1fȀȁဠ⇂ခ\x0f\x10ἀḁ‐\x01ἑ툡⌂ฏᄁ︍﾿ฌᄑ\0⼁ᄁ툁ᇁ‒ᄁ\0Āāᄠᄐā༯\0ჲ༟\0⤎ｓ‑ჱ༁ᄁတἯァǱ༐\x11\x01￰\x11︀`\x01\x10ðἀἏሡᄣᄁ\x10㈳၃￡࿜࿰࿿!!ȑჰ０ﻯ῿ℑထ℁ᇠሐ０ᄁāďခༀ\0࿿￰໭တ￱ñ￮တ\x1f࿰࿿\x01ð℞ᄀ唠噥ᗤ！／￰࿮Ỡïđ\0ἂþĐ䁎㈢︿࿰῿￡ďĿ\x01／ℑༀ\0＀\0 ခᄁ‏İŁ\0ď￿ࠀᇂñဎğᄐﻠ퀎컯﷞ᷟďȐ\0\0ǿĒᄂᄂ\x0fတဂ￿ࠀသጁ１ﻰ㿰ထထï／࿰⃿∡̀ᄁāထༀ\0\0ሀ〟㈱″￼️￯ᇿᄑ퀐ෟἌༀထＯ࿰ἀð唴ᕶ༆༏ჰ⻱쇾ἁ\0ကᄀᄐđᄀፂℑἡሁ㼑ἲℳ℣ðሏā⸑ﻑༀĐἎ℀\x01︀ໝñďဌჰ㌁Ē༡／ᄁዱ∡℁\x11\x01。⌢ሑጒሀ￳첽退춚\x12⇰ﷰ￮ⓞ∳ℑĀကἂ∀ıἐȑᄠတȐἡ℁ᄑﻯﮚ匐\x14￿ۮỮġ≏ᄠ℡﻾ᇮ￿ߠ⻞တ࿾˾骠̒ᇱ༁℀Ē∰탊ﶽ໯䗞∣࿿໠࿠ჾĀတ−㌲ሡ•ᐒ࿱ﳞ෌∟࿮ ࿰˯ᄑ∐đñỽℑ￿ࠀ\x0e呀䉃䄳\x03ῠᐣ℣￲Ἇ༡ÿ\x10℡⌱䀒䘁ﻵჰ㌠㷿ဓǿ\x10️ﻞ￿߮믟⇭ȑ\x11Ā\0䀢ሲሑ댳↼吲ᄑㄢ༃　Ｔဠᄤ㈡췪䷟\x0e￰Ἇ쀑࿌䔿�⼑䐔⌤䈤∣ᑂ￿׋ም倢⌴༑？￐䕠䈴Ｅ﻿﻾∟⌳䐲\x15⇾ㄢ␤#ᄏ탎￿ߺ㈐ሡῠሯ＀ῠ\x1f∡ἂàဏሐሡ‑\0\0ā῰\0໰ヰ⅂ᄂğ︐࿿﻿ð컾࿯ᄑ‒1仠䈡␲\"／￰ༀ\x0fϰ!ထက࿟໿⍂ร༟⼀￿Ԃﳞ\x0eāဏრᄐᄒἀ밐쮽ðĎㅁ\x01ᄀ\x10﻿￿Ϗტ￿۽ᴎ．࿠￾ð！ﳿ᾿༝ā．ထ⃲ÿ￑\x10￱ￎ컽༝ā\x0e\x11\x01℀⑂ἀ퀏ဟ\x0fĀĠခᄂþ\0Ā＀ﻠ．༏￯἟ā༑ǱĠခď⇰￿ܯฐကအ༁ﻰữἒȐïğﻡ⇿∢Ģሢༀ\0ዯሢᄢ\x1fǡ໱׳鷾࿼တ㌒ጣﴏတﻱᄀ\"∐ჾᄏက㌁ﻯïᄞ̡\x01ᄀ⻾ጁ㈒∐̱\x0fჾ∑\x12༎︁ǿİᇰ\x01\0⃱ᄂ໐໰࿰ဢĢ༏￾⽔ㅃဏ忿Ⅎဂ\x11ᄁ࿯ﻀ㻝틾ᄡĞကἠȂßÿ\0￿ס࿡⼟￱￮ฏ㈃™࿲ἀ̑ℑ‡‐Т䩧Ѕ\x0fჾ⼠⸒￿܏฀໱⇿\x10䵿嗖︢\x0f℀０⌠ฌᄛÿ˾ැ༐ᇰ釀﫪ᰄ䋱℠␲䈲㽓ữ℠ử볟﷞⏞ᄁ︞￿؏℮ဂጐ︰ï࿱ïÿﻱ　⌰忑ሄༀ࿰ἐï￿߯￿ǐ༏တ‐༁淾4⃰？῰ᄀ༂ȑᄟБㄐℓĠã툞㔃ᙍ＄\x0fဃт℁༑ǿ〰モ ᄐ\x01ሠေ‑ഒ㇣䄐⌏ȡጰ㈴ขℿထ拓䄛ᷭȎℓðༀ‰෾რ℠\0ᄐ\0ᄀðð￰ჰἀāဟ\0࿿༱ჲ﻾ÿ༏က＀ༀ．໰\x0f\0ȁကကℐခ࿿࿿\x10ᄀሑġ •\0ïĀ̑༂\0 Đ\x11ጯဂἀထ࿱‑āထđð༏༁࿠တༀ∀ȡ！＀࿯ǰ￱퇿ﻢ￯﻿￠ﻠ\x0f\0༏\x01ೠ྾ဍā\x0f\0\x0f∯ᄑāထȀĀ￱￯þᄀð\0ᄡᄒ㄀Ȁḟ࿿ᄑ\x11⼐℁ĠတđကĀ￠࿿￿؏ﳠ탞컾෾\0Đ￰\x0fᄀ\x10￰ေ\x02ῠᄠᄡჿဢ￱ᄑ\"ᄑ\x11þ！黡￞⇠ā\0Ựᄀἁ༏ༀᄏ⇰ሑĐℂᄐ\x1e ȁġ￿ܒἎጤ໠༏༁ကð\0\0⿠ሒㄢ\0\x11\0Đ￰῿\x01\x01\x11Ǡ！ༀðဏ＀－‣−တඝǾက』!ᄠĀ࿱￿߰ﳟ⿿Ĕ‡ထ\x0f༐ༀĀîက\0﷠á\0/đἁáϱᄀထﻰခ\0࿿ÿ\x10‐伄Ἇ࿾ฑ￰ā！\x0f࿱ð┿‏ \0ဏ０ℐðĝ\0࿰῿\x01໰／\x0f￾⇰︁ヰᄁᄡġℱᄢñༀဏ﻿༐࿠ℏﴏ࿿ÿ\x0f\x1fᄐ༁ᄁñ༁ሀ￰ჰ℠ዡ︢\0\0༏\r＀\x01\0\x10ἀýဎ⼠℃ḏἀᄐ\x01ᄀĒἑထᐡᄱ℁\x11\0︐˱ጡ㍂\x13ÿᄀ༑ᇱ℄\x11ǰတ࿠ᄟᄐĀჰ\x10āༀခ䈀⌡༁ᄒℒༀ༟ᄟἀ⌯Ð˱༑\x01က༐Ự️ᇰ\x11ġ퀞\x01đᄐሡÿ￰\0ᄐᄑ￿ࠀÿĀတᄁᄐ\0က༁ð\x0eﻰ࿠？ᄀ０ကတĀġ／\x10ထထðሏ∢ᄑĐ\0＀￿ࠀᄏᄡ\x11တ࿿ༀȢᄑ\x02࿰￿Ӱÿð࿟ဏ\x0f／࿰￯Ā\x0f‏\x0f\x01\x01\0￿ࠀༀ￯\x0fð࿿\0\0฀ﻍǽ\x11\x11‑þထ\x12￞ÿ\0\x0f\x01ሏ∢￿ࠀ\0ðğġ\x11ᄁ\x0fༀ\0\0῰࿾\x1fༀ῿ሁᄠ\x0f\x10āď\0\0Ā￠ðĀᄐāထ∑￰\0ᄑᄑတᄑ\x01ᄑ\x12Āሢတāñ˱逋࿰\x0fሀđđ\x0fἁကĀ\0࿰ဏሑᄑሑ\x10ﻭðᄟđ￰ἐ῟Đထ࿿ᄐȡＡᄐჯሑ\x01\0ᄁခ\x10ჿ\0῿ᄁᄑ἟ჱ￾￯ﻠ\x10āἃ2ᄟǻ ȢĂÿထĀ༟à￰⿾팮ထ㼒㐑ခဏᄐს࿠－ༀǑሟሃ퀒Ἇ￿؎￿ۯဎ−ဒ࿱\x10\0∡!ခ턀Ᏹ怕ฑ￿ࠀ퀠ᇰ࿰⸟티 ヱȮሱ⊳︃￿۰ტȯựჰüÿ℠Ġ췍ﻮ/ᇲ㄀ἁᄑဢ࿰῿࿰đ ༂ᄀዠḢ࿠ሢð\x0f탿 \0Ă턒俁팍Ｒῢ⼾ǡ䃿Ȁ㿢ሑ࿱￠ἏხℐᄞȀฐò\0︐㿰̂‱／ǯတခ￿ԁ.ဂဢሑℲሑᄀ？黱ೞ㌁産༤ခ０两ǲðℑ－∯ġ\x03∐ሢǾ￯Ḝ탽ℭဳ༞ỰＯ⿲ἠ∃ሤ㈡ㄡ￿߾ǭﻱ⃰ĠȁȢ￿ԟ࿿﻿῿ᇱĠ\0\x01ﴠༀฏဟℑđ￰￰\0࿠ÿ￿ࠀჿ࿰!༒\x01đฑÿ\x11\0ᄐﻱĀሀ`＀\0츏∞āᇯ࿱࿠ǿ\x01ĳሐđ！ðฏ࿱အ䏠Ȣ∑︂࿯￿ࠀ༏】ἒℯ\x10\x02\0ဏ∁ȒȲ️༎￿ࠀ⼏⌑￾\0\0＀Đအ￿ࠀ⇰∱鿡༐༁ữႼሀĀထခ﻾ÿ䌑ᄡ﷎ჰ\0Ğ\x11ñď∂ᄑ\0ᄑሑĢ༑࿰࿿ᄑሒỮἓđᄁ໿∑ሁ＀က༏믽೎࿿탮￮ဏ࿲࿿ჰሐ〡퀴＀༐အ℠ﻟ⋞\x01俰䌅⌣ﴍༀ\x10‑ò࿰쿯ﴞ\0Āℐჾሯ℡™ᇲℑဒ﷠퀀찏￿ҽ￿۽ༀ∐ıἐà￿ۿĐ༐ဏჿဒᄁ༢ﻰᄀሐȡᄁđᄠ⼞ຝ︁ℒ㈡＃࿯฀࿯༁က℣༂ℐ〲ῲﻟÿᄁđ\0Ȑð＀῰∀\x11\x01ȏခ\0￿܎␟Ȣ࿰츀﷯ȁ￿ԑ�à┎㈴຾༐࿠탱࿼ﰠđﷰ＀ᇱᄡ\0呐ꈒúကǡ\x0fð⌐ȡÿ\x0f\x0f༐\x10㈒đ㌢ሐ∐ã￯뿜ሯ༑️ñჿჰ\x01\x01\0℀ሠ໿Ā￮ǰㄓሡሢ࿰\0࿿࿿￿ࠀ༏῰℡࿰ᄁ\0ᄢ༡℀ā￿ࠀ￿׮￿ࠀ‏į‡ሐ༁ᄏሑℑᄐᄑ℣တ࿰တᇱ∂㌤Ĳฏﾾዾᄑ༎ဂĐ뻺ἎÝ⌟∱ሐ•￰／Ē⼑ጟ≃∃ð\0ᄐḏᄀ\0ᄟဟᄣჿ２က\0ℂሁðጏ´ᄀ쯾￿μ㼑䏲Ā࿿࿾∐ሡሁ™\x1f༒\0\0︁修ሡ℡\x11ðတ™ﴒ࿮䈲㈵￡\x12１῱þ\x10＂ﻯﻰ﻾㌔ጢ̣အ㄃ጂв\0ď࿿˿ဓሀဟ１Ḁက†㄁ဣ⌠ðȏĠĐ﷢￿ۍ﷬ữ༁ġထ໿ༀ℀ℒጡ࿰⼀㈢ȑȲ༡Ǡỿ￿ࠀðဏȟ쬍࿿ǿ　̐′ℂᄀ￿ࠀ﻿ğ℁༢﻿΅∂™ἐ˱㌰㍔ဴà컽἟ğἁ࿰ჰ\x01㿿≂ጢþ￿ۑ﻿痠坔ᑵ໰ሒᄠᄲ！㇠퀡ා̍ᄒ⌑\x04\0Ġሑ༏\x0f࿱ℿ⌓∡␱ᄲᄒጠ\x01ˠჯ\0㻿∳㌣ﻮ␯䍓5\x0f䃿⌢␱ ༁ﻰ΅㈒⌢\x10ထᄑሣᄑ⌱Ģ℁ἐ껱⼝࿏ഁტ℡ჱ￞ἀ࿟༐ ̡༂įḢñ࿏ကℒ\0Āﻰᇯဢ⿱ℒሢ␑ቂﷰ븏෿ǰᯰጟ〳༂ἁñǱ\x0fᴀ⿰™̡ᄐ\x0f㈿ÿ＀뷬￿ם⸌\x02đ\0ᄐ￾໿ญȀᄑခ∐ȯሠᇴﻮġย￢￿ׯ೏က㋱ðþ\x0fðက⌐ༀᄐ\x01ᄐ䈤ℒሱḋðﻰༀ￿ࠀ￿ࠀ⃰∑ༀĀ￿Ԁᮮㄒ℀ᄒ탰￿׭．à∏ሐ〠㌡฀Ăༀÿሑ℡ᄡᄢ\x11༂Ǒ™턒࿠`⇲Ġ༏࿯ሏ℡\x10\x01Ð쿺Ἇሁἀሐᄱሁ\x01ï༎Āﴀ࿯️∁က\"ጀჽ∐Ἆ㈢̒‐ㄳﻱ\x0f\x10ခခㄒ⋲ჰကĀ\x11\0Ā탿໽࿰ලༀဏ῿︢ᄀ￰ðĀჾ࿱／\x10\0Đ＀῿\x0f\0က\0\0＀\0ჿခ\x11\x11Ā\x0eᄒထ￰ð￰ďȱᄀ༏１鷏￿ࠀ\0ñἏ\x0fﻰďᄀ\x11\x01࿮᫙༁\x0fက\0⇾\x0f\x0fက\x01￿ࠀÿ￰ǰĐἏñﻰᄑḢĐ࿿？™\x12ﻰ෯\0ᄟฐ\x01＀\0က\x02Ƞကᄀဏǰįἀ༒\x11ðჿ￠ð￰ἀＢ∯ \x0fðༀჰĀ퀏ჯ\0༏\0\x11ﴡĐﻮ￿ࠀ．࿿ᄀ\x01ᄐᄢထတထ∱Ȳ℁\x02ჰ￿ߐ뀏廌ñ⇰℠ℒĂ\0ဏ∠࿰\x11ᄐ\0໡ᇐ￰䓿㌣Ἦ\x02ሠༀ１ᄟ\0\x01⌯Ἇ℁āĐက\x01࿾⃰\x01\x11đ￿ࠀሯဏȑ\x0f༑က￿ؠ－ð࿏༎ğ !ᄀ\x11ǰ࿱\x01ခ࿱⃯￟Ἇ\x10ℑხ\0ᄀ࿱ကℒሀᇱĀༀᄑĒ！῰０￰ሢ︐‎췑ऌ༏࿰ℐđဠ￱．တഞ껌Í\0\0ðሠﻮ࿿ß༎\0ðďð￠탟Ǭကဒ￢￾ကＡ﻿\x02ༀ῰ᄐဟ࿿ ခထ︀က\0ᄏᄁﴑÿༀ￯༏Ā츍Ìð῿ሡġῠἢñĀༀ㸀탱ỿ\0ᄑༀ￿ࠀ࿽ﻰ⼀️Ñ￿؞î＀\x0fðĐᄑအ\x11ဏᄑñ∢ℒ࿱︀῰컯䫛\x0b\0⃰\x03ﴎዱ\x11ā\0ℐïᄟထ\x12࿡რ\x1d∬\x01\x10＀ဎ\x10ġ￿ԁሬሡĀ༁đ\0ထí￱⃰အ\x11ﻠï࿰\x0fἑᇱᄢĀ\0ბ\x10￰໮⼎༡Ā️ñༀ῰ᄁñ』጑㉁∑ᄡༀ\0ༀတñ࿾ༀï‐ဢᄐ케즽\t℁\x12໱àÿ㌒␡\x04ဟﻮ⸐\"ἐ\x01ﻰ￿ࠀǿ἟Ȑᄡഁ\x1e㈂ﴡ\x0eༀĀഠကǠ༟ñᄑ타＝ༀἒᄂğ‰ġ︓῿\0တ０ᇯȂሢ\0\x10．໠ǿ／ᇱ０༑ဏ\x10࿰ÿĀﻭဠ℀Āဟñᄀီༀᄀ￿ࠀ༏ჰȐခ∑‡῿０༏\0\0㼏ሢ㈲￾ð䕏ᅁ∁đĢዢሑ￿׽㻐／⇿ᄐㄓㄱㄢᄒď⌱䄢￮뻿໐￿߿\x10ᄁᄐ\x0f䌁䔳﻾ď\0ခđᄒ!ሀ⃿℣∱﻾࿠／ﻮ￿ࠀ￯⻮ἁ໡黌∝ǰ∿ᄱ．໰ぁ!ﷰ￟\x0f∐ㄡᄀㄢ˱ﻱﻠฏჰℂựＮĀĐ\x02ᄐ툡탛＞ဏခǰ࿠⋱ㄡ̣ðȣဣ༁Ἇﻡòᄁ℠ῠᄐ\x03࿰／ༀ﻿ℏĞᄒဃ０￿ϝ࿰偟ሟﻰỮ\x01ÿ︂￿׽︀ﻟ෾ǿ࿟࿰ﴏ༁à೿Čᄂ䀁‡右 ༎ጏㇰḂ￭샯︝틠︍က࿿⋿∰ഏ࿿ᄑᄑđ￠࿾\x1f＀Ð／︎῰‰ᄁ￿߯༓࿱︍\0ᳯĐᄑựïᄏ￿Ў䷯ℐ༟＀࿾Ġฏ໿ᄀᇲჰ῞Ń䐲У턢ᄃᄑ⌁∤ÿሑ‡ထ\x10\x1e伏㐐⋢ǰ‱ጣ￿؃ℯ㈐ῲ࿰\x1fġฐāᄀ\x01ထ∡ᄁđ໯ዱฟ⸂﵍ᴁ㽁㈒ā഍￻ჿსᄢ\"༏ഁ࿐俾ጤᄱ㋱Ợ໿ဒăༀ࿿ᰏϬὃ ⌒﷟ሑဒ￿߰⸎￿܏̑ሒ࿠︎㴁勤ۼḟ켐￿ߝⰀﻰჱヒ̟ᳰȏ‿໿ໃğð࿿˿།ᇡἝ∢켎೰ሟ ∑ဠ㼁퀰ᨮāန\x0e༯ሀ︞︑àᄏǲ턡῰\0ჱ틞íĠȂᄑ἟﻾࿢ďሃᄰı䏲İ￰⋿愵ᄌᄁ퀏໡－℁ᐢごጰぃ￿؞࿑．ǡဍℂㄐ䀲Ᏹ‱㑂≄ℓĀ鼡﮾㐒ถﻯሏჳሐ䄤␒￿ࠀ࿯ᄝ−ᄑ∱쌢ỡ⬒﻿Ἇ⏲䈰☞፲弄ϡ໰–ᄟᄒ、㌠ᄯ∠␲།ဂᷰ︀⃰⌟∤฀࿰ǿᄞ㌢Ȑᄁ︐ᴞỎﻰჰሐ︢ïฏǯ\x10\0ㄑᄲἒ∐Ⅎ࿢￿ڽ‐ā￰\x0f\0／༟⃱ሑįᄁğἂ␐ȑ﷿ÿ켏෿ǰ븟჏ᐞ䄤ʒ／ခ샿￿߬隣ﻱဎሂ∢㍟∓*\x10￿߾Ǡ༏တጠġ\x0fက＀ሐℑတ䌓Рğȃ샬Đȣ＀퀏￮⃱ÿĞ！ခヿㄿÞ\x11āÿჰ䈥ጡᄢ＀￮࿡࿮﷠࿡￯￿ࠀ⃱ሑἏዱᄑ༁∁ᄂ\0ﻱ`Ἆ㼯ሐ࿾ထჱᄏᄡᄁğᄑℱሒ￿܀⼒༟က⼁䈤됣ᷱﾾሏᄐĎᄀȐ￿۝껙ဎᇾሯġ⌡ἤ﻿￿߽ฎჱᄟᐮ⅃㌥\x11\x0f\x10∁ἐ\0἟ᄑἎ໰ď\x11\x10ā㄂ሁḐ࿒ℑᄡᄑ!ჰđ⼁ሂᄟ\0￠\x10\x0fǲ︟࿱࿰ကāð ⇓༝ᷱᇱ༑သ࿟ༀð￯⇜ᄒỰထ￿߿쯝‑\x10￰῿⃱퇿အȂᇲ．㇟戥௿＀ἁ￿́Ἆ⃰ﻐ탽đ⑌Ĵဟ\0฀￰℀༊ÿἏ\x11︐￿ࠀ\x12⇱ȡ\0ዱ￿ԏྰÿഐ༐\0Ă촟￰ᇮ℀1ကᄑᇰȯ༡⇱ᄑ⼡ტ￡࿽ഀāჿ⇰ȁ￾ἀᄀတတ】㐓သတሐ㼐∴㼒ༀ ⑁\x11、툎Ａ⿲ꌁȟഐ）℀ℐℑ℃\x11ᄒᄁĐㄒ\rἁò℡Ăᄐሀሐ⋯㉃ἑᄁ︁⋰þ⿞℁ሢထ⇱`\x01ༀ⻮ǡ࿠‏ð\0\x01ἁჿἡĀჿခĀȀđჰ￰ༀﻰ￿ࠀ\0āđ\x0f\x1fǱ\x10ᄀခက༏\0０ď\x10！༡ñﻮᄏĐ༑ǿ\x0f＀￿ࠀ໿ğᄁđ‑˰\x01ð࿿ȏ\x12 \x01ༀ࿿ǿ\0ᄐ\x10ᄑༀ\0က໿ᄁ￱ῠ\x0e０࿰\0\x1f\0【ǰ༐တ\0ዮሰ\0⼐Ā\x01\0\0ჿက࿮￿ࠀ︟＀࿾ᄐༀჰကἐတ\x01ထĐခāတတᄑကᄀᄂ⼀ǰ\0￿܀࿲\x0fဏ\0\x10ððḎĀအ？／\x10ကကᄁ￿ࠀﻰ\0က\x0f逑﷟἞‑đĂ\0က࿿ჽ༐\0￰ကᄑᄑđ/ᄑ\0\0ἀჰȐ⸠Ă℡໾Āđ︁ď탿༐༐\x01￾\0\0ကᄁ\x11ğāđ\x11ȟ／ᇰǰ￞￿߾ഀðဟᄁ\0ᄁᄂﻏ￭༏က０︀࿱／ก༂￿ࠀ∀ሑ\x11།\x10က\0℀−℠⌳ሒð\x1f⌁Ȳ２\0ჿἀሐအ࿲࿰퇰︮໡\0\0ǰð\x01\x01ﻢ῿︀࿿ဏ༁῟퀁﷿ßĀᄐ໮ﴁñǿ༞ð\0က＀ᄀఀ຾ဝĀሑ㈣༏﻿Ǿ∢#￿۰￰\0\x10ÿ\x0f￯ჽ−\x11đᇱᄡㄑሐ ￿ࠀᄐᄁ໡Ȁ༏ჰᄁခက﷐￭긏ﺾญĐ\x11࿰ჰ\x0e\x10Ȁༀ\x0f＀﻿ဏ∑࿳တᄁĠက넀໰〟﷞췽\x0e\0ǰကğ∀ĂထခᄐሑူȀ∀﻾༏ÿĐĀ ğἀᄐđᄀđ⃰጑Ǳက\x11࿡\x0f\0⇱ñĀ\x10აሠ༏ð࿾ᄀ\x01￿ࠀဏሑᄑᇰĐ\0က\x01ñﻰ￰ᇰ\x10Āđ\0\x0f／ᄐဒෞᇿᄑ／￿ࠀÿ\x0fĐထထᄐༀﻰᄁᄑġ턏꯯﷡ðჰ\x0fďĀ༐࿰︀ÿⷯ࿟\r\x0fက\x01\0︀￿׾￿ࠀ࿯࿰＀က\0ﳠ／\0ခᄮÿ＀ð\0\x01࿱࿿࿰ༀ＀ᄏሡđ０﻿ᄐ\x01ᄐġ\x11Đ\x11\0࿰\x10\x01ჿ�﻿῰∑ĐခĀ\x01\x11\x01ᄞ﷿Đထð\0ᄏ\0฀\x0f\x10Ǡ\0ჱᄐ∑ᄑĐᄀᄑđ‑ā\x10＀༐퇑껉ᄏĐሐđ\0\x0f￿ࠀᄐᄑ\0ἀထሐđ℃ᄀ\x0eᄐđ＀༏ỿሀခሐ∢࿰ကᄑ\x01\0Āðÿ￰࿰ᄁᄐ\x01￟℁䈳ㄐᄒᄐဣ￿۠￯뷾‎ȁ￿Ϭ෾ð࿿࿱⃿껜ē￿߯ǰฑ⃯ßĀ￿ό￿ם䔀ℤ㌑\x11￿ࠀ〢ℓ\x01⼀\x12﫮໏ᄝㇿ⿾￿׾？ခᄀ\x10ġāᄀတ倁Ĥ￿Ԏ︡Ἇအခﴐ\x10＀ǽကā＀ǿ＀뀞࿟ℐ−ñሳ\x01㸁㉃ሒàကᄑ컍컮ﳾ䗯༂ᄁġᄑ\x10ˠȠ\"ď쀑ﻝ\x1eￜ\0တ−\x12პㄯ퀠ടᄠ\x01⓰⿀﷯？࿡\x0fᄀᄠ⼒Ӂ／àჰᄐḡዟ냼∮೯℀퀡Ḓ㓯�℀ဢ࿰㐀ሡခñ뷫ðĀ￱컝ნ℁ကᇳᄐᄡ\x02࿰\x0f턐⻰⼀တᄑĐ฀ǻ̀ǲㄴ㌢ᄢ\x01￮⿭ᄓༀ ĂḀ⇠ᇱᄁἒጀᄞം࿞༂\x0fༀ ခ࿾￮？﻿㄁\x11Ďᄏ\"ᄁሐ∳āȟðฑ ༁Đ㼣ᄁ\x10＂\x0f쀍á೮ᇿጁ䅇⿰＠ñℐȐỰက￿؏￯ထ＀匰䌔ﻠāﻯ⃱Čﻴ࿯ℎﻱ\x01￰Ἇ￞೮¼ᄟᇴ༑အჰჿ࿲￢￾⼁რ／ကတ∑ฏ컿ฎ༁ကȡ䀢•Ǳἀᄀᄟ丠\x11တᄐ℣ ༟ᴂᷰ࿯턀ྰ？ğ\x0fﳐﴏ퇝ฮሿ㍂Ἆ áༀᇁℳ∓냱Ⰿￎ￱ထἡ䋴䐓ᄯ\x01㄀ጁ섡໎￞﻿⃾ဏ∂㈐™အᄁẐᄏထ쇝ᄲ⼒ခ༐ጐᄡ\x11ÿ℀ᄁȱ\x12༠ᄁᄏđ퇼ᄑ㈓ᄢ￿ܑ볬ﻮᷬ∢⼃ჽȐဠ䌁㍂Ḏ‑ῑ﷿㿠⏞␳탾￮ﴏხ껼︝ထℒᄁﺽ㘭㕑͔෱`┟İဢ\0`츞︎㔳၄ﻮ봑࿿컻췠㄁℡−ሡ먟쯌툠ᄱဂᄐ￿ԏ﷟⺲쏋︍ྰ－﻿\x11îတ\x1fဒ৑ಞဋ∟\x1e㌲벬ᇰሐဲﳃ탯／￿ܞ༞ā␏䕒셂໯࿰ǿ\x0fᄳḑ጑࿾ďȐ\0ຼฏჟ᏾퇰ᴐȁıᄒċ጑鯫ﴍǿᄐﻡȎူℓ㐂ሱ﻿㈀㌲ሢ⓰℣〒ጂ啅͔Ȯᄣ￿ް뼋￿בᷮ━ѐ︢Ȟ⼃ჿ∑༁ ‑ሐȢ⌒ሁ`ἀ ñ ᄏ\x10䈃﷏ᄀᄠ3 ï∡℄ჿሀ‑\x01\0ð㋱㌡￿߾￯ℐℐᄢሠġ０༏࿾࿯\x0fĀༀထሐἁᄀᄁĀ０໰￿߭‐ထ！￯ÿǰĐ￿ࠀᄀᄑ０ﻠ㌎℥\0＀ðďထ\x11࿰\0￿ࠀȢ༁໯﻾໯鷝切．࿿࿿\x0fჿဟā￿ۿ￿׮Ǯòကကðᇭﻮ￯࿾\x01Ā\x0fᄑထﷱﻞ໮ჱሯല࿰￿߮￿߬ᄞĢﴏ࿠\0￿ࠀ῰ᄀሢ\"／\x10\0Đ℡‒ᄐġĀ\0ༀ\0ﳏￍ\x0f༐ð∑\x1fğထထᄑᄁ￿ۿ﷯\0တ༁࿰￿ࠀ༏ഀ컝\x0fðᄏ∑ᄑᄑ\x0fᄒᄑȰሣ\x11ď\0Ā쳼䃟﻿￿ࠀჰⴏ㈃࿱ðĀȐǰᄐđᄡဂ！︀ሏℒǿထ∁ðÿℐđᄑሟ⻽ሒ⌒ðἀ\x1f\0đᄑᄁἏᄏ\x11ἀᄁ\x11࿿\0﷬⃽฀à\x0fᄐᄐ\x01ᄠ\0đĒ࿱ﻠ໲ሐ１＀တᄑ℁Ā࿰ခᄑĀ㋿ģℑ‒࿿ዿđጲ＀\0Ȣ࿰ﳿ️鷬 Ȑ쀟⇿턟！ᄟ￿מ῝￿ࠀ껟⦹Ġ\0Āༀ῰ﴀᄏ！࿐῾ð໾ǰᇿÿ／ĒＲ￿ܑ￞\x11￭ǿ༟ﻮ﻾ༀÿ⇿ᄑ￿Џဏ࿿ðᄠ‒⼐ሯ￟\0῰\x10￀ໍ\x12￰ἀጢĠ툮డ\0\0＀ῼဏ\x0f\x11\0῟－￿܀︐ฑİﻭ\x0f\x0fᄐᄢ\x10℀ᄑᄑ⌱ဢჱ￱￮총㋟က、ሡ ǰༀ‐āᄟሐ‐ሁĠ츁￿ࠀἏዿ\x1eᇞᄞༀ∑㈣⿠đ\0ᄑ\x01༐￠￿ࠀက℀\x11ᄁ＀ᄏﻠ⋰ᄐ\0∡ȡ켍￿Ԏ￿ڿ컛￿ӭ㻯ሢ︒Ā０က࿯ჯ＀῿റ⇳\"ᄂ￿׿࿠⼀ጢȠāğ／ዱ컮࿮Ȑ﻿∮ð￰ༀǰ\x10ȁ∏ᄀ℡￿۰\x02ჱᄱ䔟́$ἀ\0ð ⌀ﻱ῭\x11\0\x0f\0⌢ȁက\x0f㈀㐒￿׮￿ם\0\0／ሁထေ﷣ሲ䌒￿ױ￝ď\x11℁đ℁ሟ໱༎匀∤ො℁−컞鲺̫ዐ\x01ᄁℑ￿ࠀĀġ༏༏ℑĂ῿ᄁᄑ℁Ȑ⼟䌣㐣ﻐ￮༎＀ﰀ﷾มℒ﷟￞ď\x0fℐᄒℑᄠĠ−࿀໭Ꮿᄑ㼀∤̱닟￿׬ﻲฏ෡￾촎ฎ༟￰Ğတတሀ】㐃āထᄑ\0＀￿πಭ￿׎\x10࿱㈐０﷏į༣ༀ￿۾￿۟￿ۮ࿿‣῱ကᄂ\0đ！༏ᄾ∡ﻲຝᄏ⿿༲ᄠ࿰\0Āð﷐࿮ᄟ∢＀ \x12ñđ໿￿ۯ࿿\0⿮ἑℑჳᄀℒ༁\x01࿱̳ℐǰတＢ﷯ဟကᏱĠ༏︁︀ခဎ︲࿿ဏ\x10ď￿ԏýᄏჰ⸀￱෯Ā\0ᄀᄀ￿סﳭ࿡℀ဒሑᄲĀ㈢ᄳﻯༀဏﯿ￿ܟ췯࿯ㇿ༂퐲ฏď࿯༐ༀ￱\x11ჱ\x01ကᇰ ĳጯ⼏Ｂ\x0f\0က￰࿎췠？\x10ἀက༑\0\x10\x11ᄢ￿ם﻿￮༎က\x01ℐሣđကጡጲ℁\x02ဏခğ샿『￲⇰\x02ሲ㈃\x01ᄀ℡࿰ἑāကဂခခᄀἀᄀ䐑␣ሡᇱ∑\0\x10∐\x10༁⌽ᷮ⋱ᄒ༏ñᄠﻟ쳾࿽ჿ!∡ጏ࿡￾\0￾\x0fထሀᄑĐ℀−ကᄑﻯ﻾ᄏခကＲ쳏￾ℐ࿿ďﻯ\x10ἑﻰᄏထထᄡ\x01ჿሑᄒđက\0￿ࠀ￿؏ﻯᙶรﻞ⋭Ģ஡Āခခ￾\x1fò．﷠ᄐအ࿰\x0f／࿰￲ȓༀༀᄠ＞ༀﻮ࿮ጰ\x01￿؎㍎䈣\x0fᄏ\x11／\0တ⌑∳ฑᄑ!ᄁȒ༐/\x02ﻯđ＀【༏က℁ᄒሠ∢ă\x10︀／쏱￿߽Ἆဠᄡᄑ˰!㿱ĳ븀\x10࿲࿿တ倀≅\x02\x0f呐⍄￿۽ﻯ᳾ሟᄡ∑\x03㸀Ĳᄁ＀￠ďďᄠǽዿሡ̓ ࿰ᄁᄑñℑጡሡㄒĳñℑĒỠ໮ÿ\0℀ᄒÿčခ⼑⌁ሢᄐ ⸑℄￿ࠀ￿߯Ί䷝∣ሡﳾ\0ကïἎဟἁ∀\0࿰ᄟï￿ּ￿ۜ﷮ï∑1ሒ⇱쳯໼儢℣ò\0／ሳȐ〃\0㌏㈅Ƀ１တ턯﷞ᇝ῿Ġჰ턠ﾽ\x1fᷰᄏ㽟䋅￿бỳ෰￯ğĞĐ쳍￿ࠀﻯჰ㋰ℂ℡ሒİ￰໭丁ἣð\x0fဂề－ﳍ⏞⌴ð\x1f⇭̡ἢ⃡༟￠\x0e‐ǲ￱ヰ∁≁ടሀ࿰࿰Đ˿ሀ༾ሞ⌏倀Ŏဎჿ༑ȍ‐㼰ሱㄳ３ᄢℐā뀀⺑ᐞ䌒퇰．ㇾ儕⿑ℐǡ．ሐ῿㈑ሲ⌑㉃̠\x10•ሁጝሲ㌡匓\x11㄂ဠℓ䃯∓ሡℏ∑ᄡāᇮ῟−ሡñð∀ᄁ໿ခį⌶￿߿ǭ\x0fᄑᄢ∡∑ᄲ＂ﻰî\x01ခᄁȡᄡᄐ‣℀ဢሑÿ＀ဟ\x01⿿⌁࿾༁ῠሏȡ＠￿ࠀ\x10￰\x0f伲䐥ñ㐀儵턁࿳ⷱＱ\0ﻯሠጃＡ\x0f῰￿ܞᴀ￿۾\x0c༏⿿ᄁጓ췝췜䐮č⌱‒䇱㍄吴ﳝ῿㏮ﻳℐᄑ\x01１ᇮ\x12༡ℐက＀Ŀāဠ﷑￿ۏპǰﻠ༟볼Ñ\0㿰ġ⼒ﻭ␏ᴂ\x01ဠ￿ࠀ￿ࠀ῿\x10℁䄑␒⍂ā\0\x10\x11샱儔㔔῰ǰጁ㈳ᄣ!ģﴌ䑄￾\0∀Ŏĕℂ∢ﻞฏ䌯̢Ȣ？ヰሡ℡쳋ሠᄢ吁䐵⑔Ｒ࿲ሣďဒሒ༟ㄠ༏ﻢᄡℑထȒ￰ỿᄑတ༐༏‑༳ᄁȲ\x0f༂￿Ӑ໎Ĝ\0က!ሠố＀ဎ࿮ﻠ⻟ሠဂ㈑ဢǰ\0\x11ἁ턑ßༀ\0ﶰǿༀ\x1f༁\x01￾ዠሑᄒ\x01đ￰￾ထဂᄠ⼟ȳ⌤Ĳ\x0fက？đ\x10ᄁ࿱῱ሀἡს쨑῎＀￿ࠀဎᄐÿḏ＂￠＀￿܏ꂮﻞữﻰ‏ခ㐰\x11ᇰ㈡″ñÿ༁∟ෟ鯐஽î\x10\x0e༏ἑထฃრĀ࿡ÿᴟ\x01￰ℐĐᄐ\x0e１༏࿐ᇽᄭ탿ￏ쯮༏ᄐ\x01က！【ᄐĐ﷛࿱ﻯ\x0f\"ჱỮჱ／ᄀ⌑⇿ġ￿ς逊ጂㄦ￰Ā\x11㄂\x01ჰ∠ℑ\"ð℀ῢ\x10Đฏǰㄐ␢ሲሯ ༀ\x10ᄠထ︣〞⻝ሐᄁ／ᄀᄑ￿׽ﻮÿ༡∡ᄀĐǱ\0\x01＀ကℐ㏱ข ᄑ⼁ሐÿ￱ﻞ﻿῿Ḑ ᇱðჰᄑἏᄀတ\0ჯᄀ\0ဒ\x01⸡ฏሁထჰā\x01ğ\x1f䌂䔳уﴏༀ἟˱ ༀჰ ἑ！Ā♀㍳￰ð\x0e໿ðༀÑ෰￿ࠀἢ⋲−ሡđ∢༞\0́༟⃰\x10ሡ＀໯ḃðﻯǿñကＡฐခ\x10đᄁἑ໡ᇯሰ໿Āđဠ࿥￰Ỿထ‑ ﷮Ữ⃿⌎\0\x11／ᰀ⇑∣1࿮Ğ\x01\x01ჿ\0\x1fòἿ\x01ἑ剑⌃︐ē༎㈑ȱǳတℑℂ㈂〲ἀ‐ခğ\x01἟∐∑㈢ጳ\x1fįàဝᄐᄁ̐ḡ㋲\"ἑሒᄀဏ￱ǰᄐᇮÿ\x10 ሐ\x01ǱĐǰ＀သ࿿ჾᄀჰȑ༎\x10໯ÿ￰ðထ༑／ᄁ\x10\0ሐᄐကခ\x11ჰ\x10ᄟ࿰࿰￠\x0fđ\x11\0đတȁĐༀကἏ࿰ခ퀏￿ߞĐĀἐ\x0fကကဠ﷠￿׾￿ώ\0࿰ဏǰȡ࿱ÿༀ῞\x0eሯ퀑\x1fჰᄀǠ\x01࿿ဟ\x0eἀ\0ሐđༀᄀༀðĀ ᄐ￰ༀ\0ༀༀჰ\x10\0＀ǰ\x10．ﴏ࿱༟ℐ\0ခ\0\0က࿱∯ᄁကĀခက￯ﻱᄑđđ￰໿鄁Ā\x11Āᄀᄐ\x11àကခ࿱࿎ᄎāđ＀༏ğကကჾ‏\x02\0ÿᄐℑထༀÿကကᄀ\0ༀǰĐȢ\0Āထᄑᄢ\x10ฑ℁ᄒ∠ဟ￟ﷱ⇢™ℒ０໠࿿\0ἏǠἐἑ༁ï‏∑ἀĀá￿ࠀዿ•ထ￰⌑ȱ﷿켏໿ᄀ೭ᾟ‟ㄢˡༀ￠ฑ㿲⇲ᄏ/ሿሓဟð봜೎กჽ\x10đἀሀༀ￿ߝǰက∀∰℔ﻞ１︑îἀ⋰ā\x0fĀ ∠တǱ\x01ᄏᄢထሠĐἝï￰\x01￿ࠀ⃰ሑༀ츐᦯⋞㈢℁ᄁ￿ۮ࿿ï\0ሐ㼯㌣⌲￰໰Ȁ༟￠ሑကᄠ™ᇰȐ１Ἇῡအ̢！࿯ሏ℡෱￿۟￐￿ߺ༎ᄁἎሀĠዱđ࿾༞℡഑ǮฮᇲᄑကဒȀἌ⌐ฟㄒ【ℳỰ￿ࠀကჰ〒ㇳ쾮䈲⌢ȣＳ쳮ℎ巿︟ሮ⌱đ㐑䈳ỽက︟῰￿؀ഞᇱᄐ㈱ñက탿￿ˬက‑ဒ෰₭⌯⍒⿱ỐĞ１ု⃠턝ภєശⰟ⿀ᐎㄠǃ￿ࠀ뀝ัýȐℓ⋲븎ῠ\r∰ûĐ－ⷀბ⿀ሎ\x11ǭༀ＠ﯰ༛㈱㋰瘲ﷀᇽ㴣㿾퀦ﳰ퇋껚တǰ㈞啃Ｏ쳬￾ᳰȐᄲ㄃⌑￼﻿௯ჯ켮ฏဎฒฏ㼑ᄔအ⌓찑⼐/༠ﷁǮϼȡᄣ㌱̱ැĀ࿒㈟⌲đ⇻âǡတᴒ∲ⅎጲᳯẞᄲ༂တ唒ጣῼᇰ℀\x11Ȓᄲሁሡ⋢⌲໫鼜ዿ㑂ำℏ㕱℃剄Ა㌢⑒녃ﳠჰሱ∓⃱ᄐㄒ\x11ထ⃡㌒Đᄁ￿ӑ￿ύ໌Ǡ턞﻿Ỡヂ\x10￿؁῿ð༏࿰￾†ᄓሂ儠㌄／ก턀ⴡ￿ӢဟℂἑℂḂǱဂǰༀïÿ℠ㄡ２￯ﻮﴏﰒ￿ן켟嗟䐤ݕàȀ냼￿۟️﷐ᄌĐሒ㋿㐲࿏ﻰ῿ﻠ༏￰ȿ㼓∓đ\x10ขğအ∂ሡℲáသﻬ࿡ㄟĂ궻쳽￿߭༐￰ȯအ￿ؑ࿾ထ\x01／တᄑሒ搑㐴썂ໍï\x01턍ῑჱㇾ⌃⍂ᄣȐሲ∓㌱䐤⑂￿׍΅￿ߡ︟ˠ퀏渐㑄㉂℡࿲þ挐䝓呤ሴ໡㈳䌤ሣᄎ퇽෾໱ጳ䐲커໯\0‑볎￿ιﻟ쿽﻾㏮㌲ᑄჲᄯ࿽퇍뼉⼌༑ữﻰㇰĀㄢ∂ℑò탫￿ۮ쿮⋭ᄂ⃿ሢ㍁῰ᄏጒ ȁ㴒ϯအ⼁ῐ‐ሐ1༐ᄀ！ჱḀᇑ﷿῱Ͽჰჿ⃣̀၏ခခᄑ⼑Ā⸁퇜￿حᴃ０࿱㻡᯲⿠ď￿ܭⳓᏀ͌ༀǿ\x10퀀⼟\x11⿲ሐ俢컞묎﻿ñā︎休︮ᳳ༎ༀጏ὏Ᏻ⼲Ăᷱဎ⻢￿۝꫹ῑḭἑ틯︠ﷰ．ᴁ퇛⼟ကĀခ℀ᄡḂḐ∂ġᄢȡȀ䈣⸰ḞḀᇿᷳა틮⼮̑⼱Ġ⼡￿؝ᗡㄥ᷑࿾ȑ\0ကϲ﷝သሡℲჲ￿܍ð\x1fጕก೬惲ᄂĠᰓ튮ﰺǱဠȒℑ\x01⻿℟∢섒ᰂ῰️㷲＾㨁포￿ܾೱℂϱ฀냞﫯꾪㈢໴ð฀⃰⼣ጟﰞ\x1fᇿ℡ġᄡ∣ㄴⴔ෹ￎ￿Ӟᷮ∑￿۱Ǳ￠０／တ２￿߾἟〢ā￿۾￮!ĐĀ＀༏ᇐ\r\x11ℎȿ༤￰þñ෰ჰ༁！￿Ǳက㐟䌥퐄￿׾໾탟ǽ￿۱￾໯໯Ự‑景⍄ᇮᄐ\0\0ヲ⃰⌱ဒᄀထᄑ１ሠ⌒ሠ㌓∐⌑ἢ￿ۼㄯ턁ደㄢ໽༏༏ကﻰ†ໞĐᄁ￿߾Ġ̢Ġ‒Ǳ￿ۡ￾￾ñก⃡⌲ဟ∁\x11ᇰ༢㄂ሂ￿߭෿ฏ￠伐⌒찀﻾\x0f`တሐ䄄̑ℒϰᄠᷱᄁ⼟㸃䈴㜣࿲￞\x0e™티 섀￿܏࿮ᇜᄟℑ㐳〣￿ࠀĞἁ䰁㐲Ṕᄒခᄑሟဂđ䄀ሒ⸂༞\x1e\x0f\x01ᄁሀ༎ဟġℐᄐ\x0f™Ȓﳾ࿮\x0f\x10 ᄑ෰．༏﻿俰䌳ฃီ༣࿰༟ჰÿ࿿࿯ð฀အ∢ᄁ∑\x10￠ༀༀ฀ᄝ༟∑⌤ȡ༏ໟ࿯︐￠⻯ဒ࿠ჰ㌯∡켰ฐჰᇡĎ\x0f࿿༁ሒ０Ǒ\x10⃰Đကᄑā㈐℡ᄐǰကāĐະ￿ߡ௝࿟တ༐ༀÿฏĀဒ⸁\0࿰\x0fỠď\x0e￠࿰ďĀခᇲἏฏî⼎ዠ！໰￿ࠀༀ\x10\x11ᄀĀⷰሑ!﻾ༀ\0āǱÿထ!\x0fሒዿᄑ⻰\x13댞⌢㌳￳\x0f＀\x10ℑჲ뼁῰\0\x10＀\x01\0ᄑ￠Ȑ\x10ᄀ˱∑ᐑలℲ⌂ကတ༐\0āἎ仐ᄠ༁\x10ထ\0῾￾༏\x11ဂȒ∑\x02＀༁Ἇ℡⌢︟ヰฏǬ\x10™ီ\x0fက࿰ᇮĀ\0뻬ﰎă？ሢㄢሑဟñ࿮ῲᐟကჿ￿߿࿮ᯟ࿝ỿሟ㌳ǲǰ⌡䈢≕ㄴؗᄐ﻿∎ℱ￱\0컯￿ӍᄂᐏĲ∣ď༁℀ℒ∑̢␐࿱㈀㐲є＀￿۞퀀￭⌯̡￿ࠀ㿿ᄑ䑠⼒−쯁㲽ᄴ\x01ዾᏮሯᄒᄀﻰ\x02䈓䐣￿ࠀḏỰÎ⋰⌱￠ፎ焒෡ჭＲñﻠ＀ᄠ࿟đÿ㈰搵噄ᄑ∑￢က︑컌㯬ጒ䅂࿾￿ڬëﻲ－Ͼ＠ā￿؎Ï쳀Ďუᄡᄢㄤ퐳￟\x10⋰㄁㿿ဲ￿ۭ῏ἒጯἒ́﯑ǫ̯￿Ͽ ༁ሟ∡ự뷭븏¾ကჿ἟℁࿱༐ᇱℑሢ￿߰˿\x0fÿ￠￿ࠀ෰ñ\x0f借ဠĠ。ဟ໰ǿ⼀༐ð￿֝￿ۛⅎᇳሠฐ￯î⼰∃ก΅ᄐ́ჽĠ\x1fဃݓ\x0fᇰჿᇲ༁￠ ᄐ\0\x01㄰ā１℀တÿ⇰˿Ｏ䇱ጠቒ࿰﻿̟ᄡ̓ᄣĀᇯ⍁∃ฐ⿰ሂ༑�⼒ᄏတ￿ؑ￯῾㈑⌢￾༏㄁\x01တ∄‡ሡＡ̐䄢Ｅ࿿ჰ⼃\x10‑㿞ǽတ挔㘢Ǳ퀏ἁሁ༂䀏ሢခሀ\0ﳯýȂ⸒⸓ğ̂ဒ\x01䈐˲唏༅\0／ῡᇰฑ໰သᄑဒ˿⿡ဒခﻟǽሂพ࿯⋢ᄰǲ\x10Ē￰쀟࿮﷞ď໰￿߮￭ðༀᄑᄑđሠ\x12ჽġထဏāက㄂ሡ࿿က０῰༏໠࿯ᄐᄒ＀ÿ࿿Ἇᄁ\x0fတထက￿ࠀ∏−ᄢĐ࿱࿰⌐ı\x11\x10῟༐ሢ༢ༀ`ȁﻐðǡ？\0က＀\x02찏￿׭Āď\x10ༀĀ／￿׀໲Ýༀ࿐Ǿ＀ð￿ࠀༀ෯ﺽဍᄟđအℒîအ༃￞／\x0f\x0f∏⌳￿׾\0ĀĢᇱᇱ\x1fᄐခ໰ხ\x0eༀ⃿⌀!＀Ā\x10Ā℀ጁ￿߯ﴍ\x0fᄏﻰዿအᇲᄑሑ࿟໠ÿᄑᄑထᄡ༂ᄐሑᄁሢđᄐǞἐ鸌￮࿿ሀሢ࿱\x0fༀက\x11\x10āℎሑᄑ⏱\x10ᄟđ\0က⃰Ȑġ࿰∐ᄑ ထက∑ဏထāအÿༀἀℒℒ１ĝ\x10ᄒᷱခ\0ġἁ\x10࿿ༀ￰\x10࿱ġ！￰\x0f࿿ကဂ\x0b‐ᄂ\0࿠࿠༏\x01ᄀ탿⇽ĒḏǱ췞č\x11ᄀ＀ἀခ\0ထ\x11ᄀ⇰︣༟ᄁ\x0fᄁ༏ðℏĒ\x11￿۰ĭﻰ࿯／Đ＀￯Ğ\0က\x11／ÿᄐñ￿ࠀ⿿ð￿ࠀ đ\0Ȑ℣ᄴ\x11တᄑđ뷿￼￿ࠀ㋾എᄑ\x11＀￿ࠀ໿ᇠᄑ＀\x0fἀ\0ᄑༀ࿿￰þðᄀ\0㄀ᄁ\x0fđā฀ǿಱᄏㄢ⌒℡ᄒ￿׮ဝĀᄏ\x01\0༑\0༏ἁ⌠!㎲ȡ⼟ἠ䌡\x12࿠ĉ\x0f℀ā⸍‒쿠Đ\x11Ȱἁ࿿ðထǲ켍üℑᄢ\0ℏऐᇑĐက￰༏᳠‒ሲ༁ༀ\x01⿾\x12\x10닶㋟䄼⯼Ṅ㦠᳻ꮠ컫䶺샫찭﫳ುꋕ㏝Ⰾ밻˰⺣⨂됀쫒쏾Ȟ䅄巑퇺崡켊ኴ㶯῁ƪ䳝ᴯ￿׊돼н଎䪤꽄뭐댜퉟ਜꗯ놼㻯뫎俴￿ԅᐰ믞겮㐮ﺮ쳌싃ꏊ솬ཌྷჰ￿ȣï⨱￿ߢ↫㱌ꃜ볬࿾쿞䋮⯬ꊱ⶝䵂쐓㼃䀘귤됌쌍뒡껔䳢￿έᆺḟ∡껋吃퇿మ겝팿㼺㌜ᄢ䫍뻾늭탣ゾ䬊숞ȝ⪞‴鬃넠￿ܵሱ䋛郼먼㌭컬ර⺬츌俼﷡鰰밽䴥䁎絛퓹遼￿ۚ寁껚ᶱ﫜⩂וֹ⭄킴폓Ӳ긞쬒맣￿ޔ⒫￿Ƚ쪮ᬲ﫻￿޳⸰⑃﬍鯡￿Ċ폳쮾㻻㬾철￿Գ䴡༁䏑톯늲䎽ꍄﾮ괻⬓뾿\x1cꯜሒ˴䎥⬳ሑ㻢䲱뽌ແ뫮⏚䮱ꌾⴡ㷠飼䏒ᷣǮ῱ሎፏď\x0f䌂ᔲ체￿۝⃾ဂ䇡−ဏ￿ԏῐ࿯̀Ḥጡᇾ【∁Ｐ῰࿿！췟䋋ሤဏ쿽〿∂‰ᄐĀ\"㐲ｆ㉏匄ℱሏ́ᇰዱȰ∱⌀ཏⷲ가￿ӌတ∭Ᏻ࿾ởĠ™﷮￿߽￿ףﻞ˾ἒ࿝ựἀ!ჱ￮†ዿ䌢䕂ሐ㌀န㈢Ĕ\0︟￿ۋ귚￭㇬䙃〒Ġฑﻰÿတ⌳㄁␢ဴℑğᄡഃሁ봞Ჽ␢ ㉁⏱ἑᄑℏ䈔䈒œ儤ďðﷰđ␀⅓˴!ሲ￯ðď⌡㈡㈡ȑᄡፁ⍄︣ℏ⃯≑挣￞￿ܝက῰䌀䑅ჰℑㄐȡǰ⌲㉓㌢䌃㌒ᇯ∢∂༁Ȯက˰῰ậ䌰−￿Բ䃿㑃\x13῟￠ðý﷿ỏ㌐∑໯쳟Ἂༀ⋿∢ἴĀȐူᄒໟჿﻢヰ∢Āǿ︀ﲜ࿭췪ἒ㏰ჯï⋠ፏȱðᄐàἀÞ⌟ฒዲ\x10ﻝ匭ᄁሏ￿ӡ￿ࠀﻰხ␐ᄥ෱탏ጽＱᄀሎ샾쳭㷟측Ǿ̑쿠ᔵခ￰㼏㑁심༛ﻟ㏮㐱﻾￿ן￾ᄀℐȔ㈱ ㈒Ȱἑ️￿ϝ캻㏑￿ң⼞＀˾࿰Ā㐀෰đđ컭﻽㈿䈳〓∃䋡ሳ′⇱Ȟ໯ᄒ−℡困\x05ჿ䔀쌯ᰰ㄃́ﻮǰ﻿퇾믟ೞሐ༡０／\x0f䄣3ሐ텄㈎㐲‐•ĳ㼁ဥ￿ߠ⼁ံ̱귋燮༃Ȁဟ㑀︎㴑㈄⌱㌳㌃퇼ﴟሮ儣⌓į밑ￚදȡᇭ㕠㈴︦ᄢ﻿ሢ䌒ﴓﶮ㏿–㈂ሔ෿㏱ﻐ༎ჰß￿ࠀრ‒ฑᄟ⃿ᇲ퀀⃿Ȣἐᄑ࿱￿ࠀ￰ೠ㿠ȓ༟\0Đ\x10ሂဂ‐퀡Ďက\x11Ā\x0f༑︐？ᇽ︣ǰ῾￡ᳯ༁ሀ℁㄄ෞༀ拝＂＠⇀༁ĎȟĒ.\0￿םᷟᄡ￣﻿￰฀Ā㐿À`㈒⊝ÿ〟ᇯđ︍῿à᳟ðđīᅆฑᇡ༁ሐğကĐထȑဢ‑￿؀įᄒ\0ᇯሐ∀℠\0툰\x1dďĀᇰマሒ⼐⌁࿞️ሐ⋠ᄒ\x01Ἆḑሑḑᄲﻰᄠ逡『⌥̢ᄔ\x10ჱℂ™￿܁࿰ༀǰ\0⋐ሒđḠ⌡\x12ჿàï【␳専ဴĢ΅Ａဟ\x12`ጯ퇱︟Ģ༁￰\0℁\x10Ǳ\x0fÿᄐ\0\x0fჿဒሑ༁༏ဏᄁჰȁ෾î ࿰༐ﻰ༏༑ \x1f༑Ă\x10ᄀက\x0e\x0f\0༁ᇿᄐℒ⇐ÿ\x0fďđတĀ\0￰ሯ⼒ℐကሱအခ༁ï／đᄑ\x10࿾\x0f\0‐ᄁﻯÿༀ࿿ᄏ໽\0အἀḁ࿽ℐ＀တ￰ကℑ\x02Ἇ／．῾ᄁđ＀ༀ\0ᄐℐᄑ\x10\x0fἀᄑ\0༐．࿿ℑ༁\x1fἁÐ￾＀ﻱ໾ℐ\x10\x10ကðጏŁğဂဏༀတḁá\0༐뀤࿿뀁ﳝÿခȠā\x10ကᄁἝㇱᇒᄐ\x01\0ထᄏᄀ\x01἟\x10\x0fἀᄀထ㌟ᄂ™￮ÿđတ\x10￿߯⃰→\0ခက\x01\0ð\x11\x01픃￿ׂᷳ퓏넪츢ඛ䌲伮˞ྼ￿̪쏎￿ϒǎǟꈤက㆛ፁ쳮∯촄엞Ⴣ￡ꉃ퀟⒣촏뭎Ꮀ伋ﻚ⌐㊱໋Ă꾿⌢볓ᄯ␮퍏탺갂뷌丣뿕᫱䮯돟ᭂ￿׎ଠ侪쀴ﴃ㭋䑎﵋㷡뷐䷺䯴⊯ᳪⲳ넟䋿њﳑᏌᾴ㳁￿פ㰝ü쬐㼑㋽됋፜ⴏ⺠ḭ닒ꐣ⨂쨍ᮡ괢及͏ዊ໐ꈃꋲᬰ뺱킡ழ㼺ӱꇣ༄㇜꼟끎￿ܒￚ侫∂㬚⸡챀䎽ᬰﺲ츃ⳤ㬳쇋껺乔괡찠⏌쬼귺ఠ퇲䀀￿н㯓䵄￿ࠀᬞ귀쏋굛돢ᑕ﮺䏎￿ޯ⯱셕Ḵ￿ɏⶢӫ䓃仏￿ϫ뇎㿄쿀䈁냣턓ꉍ쯟䷃컌ḏꯐﲤ㨝ᰋ崫뺰俽∽눊Ⱁ銠댛 쌝먝Ⱎ녎￿܍컝㰎￿ֻ廠帑ค\0∐࿠ဏȀＡ࿱\0㄁ᄐÿကǱ⇱̐Ｏ༂\0ðഀრ\0ḀǠđတā༑ᄑༀ￾￞ฏ⇱ﻰǾȐ￿ܿ⼃༯턞Ḓ\x10ā０䇱숎ἰ\x10\0＀ᄐ⼡₳ḑ༯ンᄰ￱෰﻿⼑￰ð︀῰ñ＀༏༎໠\x1e￿ࠀἡ˲ჿᄠ\x10ჱ丟აሐ℡\x01ༀȀဏ‐ĀĲ퀏﷠໠သᄁတĀㄡꌼᨢ༁ḯ㇒ༀက섭ḑ\x11ฟ￰฀ᇢ턍⸐က턝࿱︁⿡အȁᄁȁ\x10鼀ᤂᆰ퀁଀™ᄑ㸑À‚\x1f뀀ᰟ￿ܮ⃡Ｏῳȑ켠ﷰ＀\x0eတ〠ỡᄟἒᇲฏဎ︎àᄁ̟￿ࠀðĀ\x02ഁ࿏࿱︿⿳࿿ð\x0f࿰\x0fᇱȁȠ༄༐∀㈓ጡǰ\x0f卆ጱἔ￯﻿\0䈃䌲﻿￮︀ﻞ￿ׯໞ˿ἑ팑〣ἃ໮∐ℐ￞\x1f￿ۮသỰᄀ틯Ｂ∡ဣﻭ￿ۭ䌾ቂ⃰ﴀዡﻲ⻯㛰煖ﻰ￯￿ۍﻀˮ Ỡᄁ\x01￿ࠀÐ︎㌱甦á￞⏢ᄎ฀࿯༏Ḁϱﻠ༎ï䄎刲䔴∢⌓䍃ﻞዐ␑1ዲ໱Ý˰￿ܒໝ쿢﷮ﻯᄐ︐\0￠Đ⌳ἵ⇱ğ῿ጯခ∯\x11．Ỿ⇲į\x0e㴑﷞␎ﻰ༟￿ۯ﷮㋿∢㈂∔ㅁ￿ߏခ\0ༀ퀐㿍ᐯ㌣ᄢ㐲⑄ᄢ\x11㻮☕뉟ໟἏᇰ㈠̢ㄲ￿ۮ\x1fᄟ⍃㐡∱⽢㍂⃝îༀ吀㙃䑥ᅁ㈢ῠ∀Ⅎሃጏ䅄䐤⌢ἢ−Ġﴁἠ࿠࿿\x10㌑Ἇā\0ထᄐ\0Ǳ῰ဟ\x0fဏᄀ\0ჰā￰ჽ\0ð／ༀჰĐἁ༟\x01\0ᄏᄑတ\0༏\0\x01ကခᄐထᄒđð\x1fĐĐ\x11\0\0฀ȁā\0࿠ሀᄐ༠ༀ￰ðď︀ǰ￰ď\x0f\x1fñ฀àﻰ˽！ðကฑༀð？ဏฐῠဏ໿\x01\x0fᄐሡđðကďþ࿰ᄏȐဠ\x0f\0\0ᄀሁሠ\x0f￿ࠀ\x0fတခ￰།\x10\0ჰᄁက￿ࠀ࿎￾ď\x10\x01\0ðༀ／ᄟĀ￰\x0f\0\0࿰ᄀကༀĀ\x01Đༀꀡﳙ࿞ñĀđ\0ሀᄐခတ\0က\0\0ÿခā༁က＀἟\0ᄏတ\0đခ࿠\x0fሑ\x10\x01ǰჰℑā\x10ā\x10āခẠ̄ἠﻢ㇮⌲༡༐࿡Ď̌ᄠ㈑ģᇲ＞ ￿߮Ì￿Ӑ`괡∤ᄂ⃰ᄒﻠ∰ĢĐᄑ࿿တἁᇰ⌠῱︓࿿섐ᇭ⌂ሓ⋱࿳ⷍဒĐᐌ㐣⸂ȁ༟ั⇰኿∐￿Ӱ`￿؁ฏ㼟⌓ḡĂﻯ࿰䀀ሓሠ판ﻮ\r㇮ጢ츟﻽༏￿ۿ？⋿ခヿ⌑㐡Ḁ￿ױ￿׬㦫㈓ሄ໾\x10໏ჰ﷿`甐\x14෬ÿ￠탾≓#တ⇠⼂ᄁ̠အòﻠ ㈓ጔ儵㌒￳\x10䄁ȄഏㇰȂ\x12ༀ￰鯏ফ￰￭\x10￱　\x12ሐቂ츯℞∰䁕ᄕ໿䌁ငℏ䋓ᐡò㴑ΐ⇰뷰„\0￿ࠀℏ㔰㍤㌓ሒ೏￿ߞ໻ሀ￿ܠ￿׭໠ༀ￿ׯ䙞䍔Ķ︟⌒í．ᄁတ￿׏Éΰ∏ℴȁခတ￿۠ༀခ̀სǰ὏︁ᄿ\r࿡ᇲ⌑３࿾﷯࿿倀㌵ㄣ⇱′ᷴ￿ࠀ⻰‐ဂༀ\0\0／￾⋰ㄠȒᄲ䌾ጲ！췐ჿ￿ܞ࿿Ò⃿!〔ฑ࿯팓ŏ⇰\x12ﴂႡ㈏∏Ȑ承ᐃ\x0bဏ췼ෞáฎἎ∠Ģ໰ᴀĎ⇱တ䐒ဏã￿߭ᄯ⼑￿۱쇎ᴑ⃿￭㿰ᄐ컲ḑ￰ǰᄳ㌣⌣Ỡ￠ð️￯㈁⇲\x0fЀ䴴ᄏዲ￭໾￿ࠀ℁〱ጡ2﻾️᷿ჾሐ䀠⅁ᄒ∠ㄔ␰⸒\x10\x0c ჰî㈂푀ữ￭᏿ℱ−ㄴ∿༞ȒἎ⌠ᇱⷲﻯἏḮᇱⴐᑃㇳἁ˰ᴑ℃∯἟ဢ⿞\x0fğ ㈢㈃ďထ∁ሡ\x11ᄁ\0\x0f෰࿮\0က\0က\0ထ\0࿰﻿⿰.ᄐ￯໰ဏᄑᄀᄐ\0\0࿿ᇿᄐတǰ\0ሏᄡ\x12ༀ＞࿰\0ð\x1fᄑ퀀²က\0ǰ༟\x1f\x11\0က／ǯ\0？௰ᄀ༯ÿༀĀ\0ༀ＀ᄀ࿰￰ἁခﻐðĀ༐￰ခāĀက︀￾࿾ā\x10￿۝࿿໰︎⇿ȑ\0ༀ￰࿰ᄯġሑᄑĐ\x10￰\x10ခἀ༎Ἇᄁā￿ࠀ\0Ā\x10ခ\x0f\0\0\0\x01ᄀĀༀ\x1fༀဎđᄐᄐ\x0f￿ࠀĀᄑ￿߰฀\x0fǰᄑᄒ타都\x01\x11\0\0Ā／࿯ᄐထ\x10\x10ᄎሐđᄁ\x01Đ\x11ᄐᄑ￰⻬ȑ\x10\0\0Āᄐἐᄀတᄑðༀ￰￿ࠀ῿ᄁĐ￟￿Ͽ⇿⌑∢㈳༁ᄐᄴᄐ໰ÿ\x01\x11℀ሢ＀Ἆğ༁ā࿿ကā࿾ā컬\0\x11ﻰ㋮䐴у༁რ\x10Ġဒ೰￯．ကย￱࿿⋿＀໯ﻏ￮໰￿߮￿Ԏﻮ￿߮坟≒ᄓᄠ\0ฎ￿Ͽ﻾￯༎ჿအခℒ༁￿ࠀ⃰−∑ℑᄱ췬췛∝ፁ﷠￰￿߮볌༌ကሀ㐲\x11࿿ǰဏᄡ⍃∳ᄡÿáฏჰ첰ꃽༀ⃰㈳ȑༀ、\x03ᄑሑ쯰ხჰ￾／ð퀏⸁䈒∃ß？༐ᇯ∡\x10−㌱အ䐡䑃ÿ1 ⏳㈂ﻑﻭᄒᄐㄢခ฀ჿ턟￡ᄞ•㍂∃༑︁Đ⍂ሳ␲⑄∠⌓āğሀ\x10̑ᄣ㄃䉤㈤ༀĀ฀˰ \x10⃱\0";
static const char CONV2_S_ENC[] = "뭈㷁遹㹼踓㶌臢㹵뤐㸔汏㸫钲㹎ꭽ㹝ꝑ㷟[㸛焈㸺뭿㷮뱆㸤㸊Ꮝ㷲﷝㷛搂㸐ሒ㷐ꚥ㹟燾㹎㘂㹒ꚏ㸇￿Ɓ㹑猁㷜榲㺉킥㺍얉㸻齠㹍ꝓ㷀䠣㸥兏㹄陪㺀뗉㷙‛㷁㉗㶓뚋㸍᰿㷂㾿㸾궜㸃㵘鐆㸁ꏾ㷌泽㸗㸪⒦㸼㷚ᅹ㸝ッ㸀￦㷬迪㸘㸸࿍㹖䬑㷂쪺㸢짡㶤㷹鮷㷑凉㹭䎱㸺宐㹂㮯㶰᱉㸄ꭲ㷌砂㹊硶㸭ᄉ㸡藌㸌ԅ㷍ᘪ㶵谵㶭זּ㸁᪈㸋鮉㺇禃㸠￿ő㶬딱㶘ꅳ㷌癁㷜ꢯ㸵脲㷣ឭ㷜磒㸏鐷㶸￿Ƀ㯃堰㶛￿О㵆㸖좯㹁⧕㯈⢒㹸㶡硫㺎ꬦ㶝墫㶝䛗㺐਍㷴";
static const char CONV2_B_ENC[] = "붳⵳밨୯뷦븕Ӛ븖筩뱎鯯뱺啟빮煓뵞इ븬￿ͩ㸡㸉㶧啕㱠⼰븀셒븓䎏붖ᎍ븂걷볏鏽뵈븈㓑뵧︥배䶓㴼詟북왳벂꼼븕ׇ빌ꚕ뷷곿븘煉㺂ᙳ뷡◷븾匿볎翇벱뵁뷼鉶밐쁆붾㪐븋㖗븈絸븠봺봻슽뷵뇱㮅礁븡珛뱰전붕ㅂ붞둨붭棇뷠鸋뷻ㅻ븍붙嫓뷿尒뵹￿뷠䠜뷋걁뷪ꊲ뷘屹㴚ꕱ㷝燛볻↋뷰ꃍ㲖鼹뵜俤붍竧뵷큱붑箠㵡䶖뵀콷봃唆뷳⮈빀浖붤뵯뷜ᄪ봅뵓渾븷ߴ붵ꕟ븬⯦뷤멪붎淟뱀￿ڂ붡矐봯煚봑紽븚꫟븲￿ߏ뷅ﯲ봣퍙븆่븓뵃罋뷮䦄붕㹝礷㲗";
static const char CONV3_W_ENC[] = "￿ࠀ￿ࠀ㌯䐳C\0\0ᄑሡ࿿࿰࿾￿ࠀ༁Ā￰ᄑᄑ\x11တ１࿰ﻮﻯ﷿ǮဟĀ？῱\0ᄀ\x01ༀᇱᄑď！￯῰Ā￰࿿￿ۮ￿ۭ໿／￿ࠀðတđď\x01\x10ð．ჿᇾℑ鄁ꮙ볊￿ࠀ￿ࠀჰჱ\x0fༀༀÿ℠ሁ༡῱༏￿ࠀ\rჿ\x01\0ᄀ!\x01￿ࠀ࿿￿ࠀÿᄐ\0\0￿ࠀ￿ࠀᄟᄑ༑￿ࠀἏᄀሠ࿰ÿ﻿￮\x0fᄑ￿ࠀ￿ࠀ࿿ကđ\0\x01Ȁ\0￿ࠀﻯໝÿᄠȑ∢ᄒ㈢ᄳထᄑĀ\x10࿰\x0fᇰထĐထༀ︀℟ሡခᄁჰᄀ￿םￜﻯ＀￿ࠀ￿ࠀ＀ÿ\x0fༀĀ\0\0\0ᄑñ\0෯＀࿰\0\0฀ßᄎᄑ\0￿ࠀ￿ࠀÿð\0\0촀／￿ࠀ￿ࠀ༏\0ჰ\x10쀎￿Ӎ뷋ഏ⃡ㅃĔ⿱㌓㌢\x10ထıဒ︁￮໽℁⌡ბ㴑￿؃췮뷾ర ᄑ￿߮ﻠ෮ჰሐሡ㈢﻿쿿￿ϜᲽᄁሑἐ ᄁđ䔑䑔䑅∢ሢđἑ컲෠﻿ď†︒Ố￿ࠀ︁¾췎ἀ℁鷰紐췯ﳿ￯￿ࠀ쇜갞໼ǰᄏĐ쀡곋뷪﻿￰／ĀሁᄡĤထ∂ᄁᄁ∐∢3︟တ࿰ჱ࿮￮️\x10⃱ᷲ໰㈀⌲ጳ︁༐㌠㈣︣ﻐ⃾∢ȁ￿׭￿Ӎ￼ჿ１\0ā༏ ࿡￿ۮ탽﷿ ㌂짝첼ᄌĐ쬡첼￿כ￞ᄏฐᇱ￿۞췭껊븚ἑᇰ໿￡໭\0\0က\0／䋿∐ㄑ䌔㕂\0\0໰⇜∑∢ዱȠℑᄢĀĐ!໿ထᇱÿ\0ﻰᇾᄁ‐™ᄒ\0฀ᄐđሐᄡ∡ሢᄑ\0Ā\x01က︁￿ࠀ࿿／￰ထሁ༁\0ᄐᄐð\0ÿﻯÿ໿￿ࠀ\0တ￱￿ࠀ쳿￿םෞ\0／￿ࠀᄟđ￾⿰™ᄑᄑđခ\0တ\x10 ሢ∢\0\0￿۠Þ\0\0ခ￰\0ကჰᄀᄐÿ\0ꪙ쮻\x1dက\0\0\0\x11ထ\0\0࿰࿰＀﻿࿯ᄁ\x01￿ࠀᄐđကᄁ‏™đ￿ࠀðî\x01ÿ\0\0￿ࠀ￾ℯℑ\"က￿׮￿ࠀÿ\x1fĀ︀￯࿮\x10࿱ᄐᄑ￱￯\x0e\0ÿ￿ࠀ\0\0\0\x10༏࿰㌳㌳ᄓᄑ༑￿ࠀ﻾￿ࠀ\x0fÿ￰༏퀀￿ۍ￿߬တᄑ\x01\x0f\0\x01\0\0\0\0࿰ÿ∀∢∢ᄒᄒ\0\0ᄠሡᄒᄑℐ™ᄒ￿ࠀ\x0fကĀ￿מ﷌࿰ÿ\0\0ﳀ꾩ëခ￿׮쳝ﻯÿᄐℲēတᄑᄑᄐ໾ﻰ\x0fí\0࿿ကĀᄑᄐ￱￰࿯￿ࠀ࿾＀￿ࠀÿ\0\0\0ð／￾࿿ᄑᄑ\x01ἀကᄑ￮ğℑ−\x01က\x01﻿￿׮／￾￾￿ࠀ￿ࠀÿðđ０\0\0ĀᄑđĀĐ﻾￿׮ￍ﻾࿿ðᄑထ￾ჿ\0﻿ᄑᄑﻞ\x0fᄐᄑℑ໱퇮ﻞ￿ࠀ￿ࠀ\x11\0ကထᄂð\0\0ༀᇿᄑᄑᄑሑí０ďᄐထā￯⌮㑃ỿđ\x11ﻮჿ\0\0￿Ԁ￯က\0ကကჼ\0\0\0\0\0࿰ﻯ￿۽ﻠ￿ࠀÿ￿׾ᄎđ\x01\0\0\0\0\0\0\0\0\0\0῰ᄑᄑ\0\0\0￿܎ǝခሑᄁĐကℑñᇿሡ￿ࠀ￿ࠀð馟ꪙ⊪∣Đ\0ᄀხ\x01တﻯ￿ࠀ\x0f࿯￰췮뮼ᄑᄐî\0Đ\x01࿰ༀ￰￯￿ࠀက\x1f༁￮￿ࠀ￾￿ࠀÿ\x0f\0￿׮෍\0￿ࠀ࿿﻿￿ࠀ￿ࠀǟĐ￿ࠀ￿ࠀ࿿∀ሒༀþ࿰￿ࠀÿ\x10đ\0\x0f\x11\0ﻞǿ\x10￰￿ࠀ\x0f＀ð／\0\0ᄑᄑ／\0\0\0ᄑᄀᄐāĀ༁ðᄀᄑሢခᄐ￿ࠀ￿ࠀ\x0f࿰⇿\x01\x0fༀ＀￯ï\0\x10\x0fက\0＀࿿࿿î￰\x11\x0f\0\x0f\0\0￰⌢∢＂࿿ﻯð\0\0\0\0ༀ℀ᄑġ\0\0ðð\x10\0က\x01ÿ\0\0\0\0à\x0f＀﻿῿\x12Ā࿿\0ሐᄑ￿Ԣ࿿￿ࠀ￿ࠀ￿Ӿ．ďᄑ㄁䐤㍃／몰ꪪ㎺ፁ鸏몺쮜﻿ÿ⿰＀￿ࠀ࿿／ﻠ￿ӿﴎỒì࿾ð￿Ӯ￾⋮㍂툲ﻮᷞ˯ဎ\0￿ࠀ\0\0†−ằ퇫￿ԏ컫쳞ﰟ῱⿠ሀ࿠\x0f ∐㓰́က\x11ﻠ�\x01ᄀ˯ȿ￿ҰÞ\0￿ࠀﻯ‒\x01０쫭ï῰ကᄐġ೮\0\0쿬\x10ᄑ１ჰခ࿮‒ᄁā턏ﻯㄢ⌒⑑㉅რ섏￯ဂ＀릟벺￿מ࿞༠⃲࿠ÿ＀㛯∱ჿﷰကᄁ༁ᄁ\x11ÿ＀儳䐤⌳㌳ȳ༓\0࿰∁℡￿۞￿ۭ⾽䌣⌣∢∲ﻒሏđ\x11\0\0™đ\0\0⌀ቁ️\0\0 ᄑထ섀໾ ￿ۋ\x0f\0ﴀ￮￾￿܎໿ထᄀ￱﻿ᇯᄑ￿מༀ\0￿ࠀ\x0f\0Đᄒ\0\x10ᄑᄑ￱￿߮ￜ࿰῿ထᄀ첼볛ﻬ￿߮⇭ሑĢ༁\0\0࿰￿۾āĀ￱／ǿฑ\x01ကᄐ∢㌢ᄓĀ\x10컯믜쳌￰￿ࠀ࿿￿ࠀ￿ࠀ￰࿿ᄑထ￰￿ࠀᇮᄑᄑᄡ−ကĀ＀￿ۿ⇝ሡġ\0\x0f／￿ϟნခ∢㈢\x02တခᄑᄑᄑᄁ￾ᄟᄑᄑᄐ뀑꧋ꦛð￿ࠀ﻿ໟ⇾ᄑᄑᄑā\x11༁￯￾ჿᄑ∢ሡ㍁ထĀ\0\0Āῲ턁쳭쳝￿ם￿ם쳮￿Ӎᮬﻯ\x10ᄁ￱￿ࠀ￿׿ﻮᄑሑሑᄡᄑဒ࿰￿ࠀ\x10\x01Đᄑ￿Ёﻞ￿ࠀ￠ᄑ\x11∰∢ᄢ\0\0\0\0\0\x0f\0\0＀໮／￰\0\0Ā\x10！࿰ÿĀᄐ\0ᄀᄑđ䐑䑄﻿࿿\0㼏䈤ጒ༁∟ᄡ２️ðခᄑሐā！￿ࠀ࿿ÿᄢဒāᇿᄁ\x11\x11ñ∢ሑ￱￿ۮዝ㈢￿ࠀ￿ࠀ\x0fတ쳯￿ל﻿࿿\x0fတ∁∢ခሑတ\0\0࿯⇿ᄑ￿۝﻿ﻯ༏／ð\0ď\x10ᄁᄁ\0\0ᄐ릚鮩ᄚ∑ဢ\x01ကထñ￿ࠀ￿ࠀ࿿࿿ᄏထđᄑᄑᄞ℡ℒሑㄑ䌴䕄／\x0fჾĀ\0Āᄐ￿ࠀ￿ࠀỰ༏쀐쳍￿׬Ē\0\0\0쬀￿ל࿟∁㑁㈲㈤￿׌㋜−툢ﻞ\0Ā￱ℏ∑ﻯ\0ďᄑ１ÿ\x0f￿ࠀ ἁᇿ\x01퀏ￍ￿۾တā￰￿ࠀᄏ\x10\x11\0\0￾\x0fက\0／ﻠ\0\0ሠᄑ\x11\0ð\0ᄑᄀĀ\x10￮࿿\x0fðﳞ໎⋽ℓ℁䉃䐴뷿ᄭခïတ⿰ᄂﻱ첽￿׋Ŀ￿ࠀ뷛໽ฒ㄂￿ڻᇭ㌡̲₮ȑ／￿߿࿰︀￿ࠀ＀️￿Ӟﻝ￿ې￭＀﷿￿ם￿ی駭뮩෭ဒကᄢဒ\0í⃿∑∢￿ۭ\x0f\0ᄐℑ∢Āሁ︐ᆽ∠\x0fᄑ﻿ÿ￿ࠀĀෲ࿿ሁ\0\0쮰﷜￿ԏ΅ᄑ\0တᄑḁ⇭\x02⻞Ⅎ∢ဓ쿰컽ựༀᄀᄠᄀĐ￿ࠀ\0㌱䌳㍄⍃ñ＀／￿Ӎ뷌⋜ሑ￿ࠀ／ሡሢꪱ﻾þᄀﻠ￿߭￾ﻯ췝컭ফ／￿ۯ㌣ȡ෠￰࿭ÿကᄐᄡ￿ࠀ／ðĀ￿̑￿ڻᇮ∠ဎ￲࿿＀ĎĐ\0\0￿ࠀ\0￠ÿ唀啅ф࿐퀌\0\0∠ῡ￟﷜￿ԍﻮ￮∯ᄢðᄐ\x01\0ༀ￿۠໿࿰⼀㈢㌳／\x0f\0༐\0‑ሒ\x01\0\0￱ÿ\0￿ࠀ￯ሢᄢᄁᄏñ\0ǰ퀞ᄑထ︀ᇯĀ￿ࠀ༏ð￰／ﻯÞᄑ／／ဎ\0ᄀ\x01ᄑᄑᄐ️ÿǯ\x0f\0đ＀￠￿ࠀ／＀︀￯࿰ကđÿကᄑ\x01\0\0ựᄁ\x10ÿ\x0f\0\x10＀က࿿ð＀࿿ᇰሑᄐᄑᄑ\0฀ᄏထㄑ−㈡䑅䐴뮩쾻\x0b\x0f\0\0\0＀ÿ\x0fჰ＀\x0f￾က\0က\x0f＀ÿġᄑሡ⌡㈳￿܏￮탰࿿ÿ\x10࿰ჾ࿰￿ࠀ￰ᄑᄑāတȑà\0ñ\x11ခ￰ჿáჿჱ\x10ခ\x01ჰ༁\0\0＀ÿ༏ༀ࿱＀／／\0\0Āᄐ\x11\0࿰ᄀᄐ\0\0Đထ￿Ԁ￿ࠀ￿ࠀĀĀ￠àခᄐℒሑ\x10ခ\0࿰￿ࠀ￿ࠀ࿰￿ࠀ﻿Ď\x11\x01￰￿מ\0\0∠ᄑ!ᄀ\x11\x10ခ∣⌢ဓက∐∢ȑ\0\0＀￿ࠀჰĀ\x10࿰\x0f\0\0\0\0㌀㌳ȳ\0Ā￿ࠀ￿ࠀ﻿࿯ℏᄒ숑쳌쯌﻾￿ࠀ쳏췜Ý\0\x01\0\0\x11\0ሠᄢᄒᄑđĀᄐထᄁ\0\0＀￿ࠀ࿿\0Āༀ࿰༏∀∢ヰ\0\0ÐįဂĀတᄑᄑ\x10ᄁᄐᄑ\x11\x0f쀐⸟༏ༀ￿ࠀ￾￿ࠀကᄑᄐđ\0\0ᄀᄑﻮ￾ሡ∢鮲쯬ᆭ\0\0\x01\0￿ࠀ࿿／\0∐ᄑ̢\x11\0／￿ࠀ췟﷮ß\0퀐ﻯ￿מᄑᄑ뷁⇟ᄑᄢᄑ∡￿ʺĎ\x10ᄁ\0ā\0\0\x10ᄀᄐᄑෝ\0\0ꦐ￿μ࿜￿ࠀ￿ࠀᄑᄑ／＀￿ࠀ￾࿿￿ࠀ꪿몫䎻㌳ðÿᄒ㈒ﻯᇾ０ჱ\0\x1f￾／\x0f࿿ကက￿ۯ￿מĀ쳑쮻ႫᄑሲᄑĐ\x0f\x01\0￿ࠀﻯ࿿﻾ἁﻯïđ\0ဟထᄁ\x11ð餐ꪪ᪪ᄑᄠ\0ᄀﻰ￮／ﻰ࿎\x0f／ÿဟĀ㌀㌳￿ࠀđᄀ໱ჽᄑġ／\0ᄑᄀ༁࿿ÿ༁￮ᄑᄁ\x11က\x10࿰⿮∢⌲\x10\0ᄐℑ™ሡሳᄑᄑᄢ∁༁ჰ㌐⌢⌳ሑ∡\0ÿﻰﻮď༁Ā＀\x1fĐ\x01￿ࠀðĀℑ−ጱ￿߾\x0e࿾￿ࠀ＀￿ۮ￿ࠀÿ￰￿۾໮／￰￿ࠀĐℑ２／\0￿ࠀ￿ࠀሲᄢἀ￿؁볝ᯌᄑᄒđᄀĐ\0㈐㌢Ȣ\0\0\x01ᄀ\x11ခတāᄐ\x11ᄑ\0\0쳀ﻯ῿\x11\0က\x01∠㈢#\0ð\0\x1f\0ðÿ\0￯￾໯￯Ğက\0ကᄀᄑĀᄑ\x11\0￿ࠀ￿ࠀ\0\x01ༀ﷎￭ု‒ Ē\x01Ā\0﷠࿞＀\x0fሡ\x11￰ἀ℁ᄁ໮￿ࠀ\x0f\x11༑∠∢⌑∢﻿￿ן﷮ÿ＀컮꫌ꦛ￿ࠀÿ∯∢ᄲᄀက\x01࿰ༀ\0က￾\0\x01\x01﻿ᄑᄢᄑ\x11ဏ∠\0︀\x0fက∀∢Ȣ\0ကᄑᄑđﻮ￿ࠀ\x01\x0f￰ༀ\0⋾∢￿ࠀ\x11／ᄟĀᄀ￡Ⳍᄑ￰đ＀ᄐᄑ\x12\x0f￿ࠀ￿ࠀđ\x01\0಼￿ࠀ\x0fᄟ＀࿮\0\0ð\x0fﳮïᄐ࿿ðᄀᇿ\x01\0\0\0￮ďĀໝĀᄀ\0\0ﻰይᄑᄀ\x01ᄀᄑ∑Ă∀∢ᄢ\x01\0\0ᄐᄑℑ∡Ģ￿ࠀ￿ࠀÿ／\0ကﳿ￿ࠀ￿ࠀฏ\0ကᄐᄑ∣㌡￿˓￿ҜÛက￿ࠀ￿ࠀ￯￿ࠀ℟ᄀ༐῰\x12āﻯ࿿ð￿ࠀ／༐ð￿ࠀ￿ۿﳮ࿟ထ࿰￿ࠀþ∀∢ᄢ\x01\0ᄑᄑ\x01\0ᄀᄑ￿׮﻿ᄟᄀ\x11\0\x01\x0f\0\x0f\x0fĀ\x10￿Ϟ෍ᄁሑ﻿\0 −∢ð／⌠∳ᄢᄑ࿰￿ࠀ￰／࿿ÿ∀∢ሢ−∢￿ࠀ￿ࠀᄟᄑ￿ם෍\x01෯￭￿ࠀ￿ࠀ࿿\0￯\x0eᄑ\x11ᄑ\x10\x10\0\x1e\0ᇿ︀㼀ሢ∢™∐∢∢ᄢĐ\0࿰￯￾／\0鴀짌ල\0\0ᄑᄑ￿ࠀÿ\0\0\0\0ᄑᄑ\x01\0\0\x10 ሒ\x10\0\0㌰−ġ\x10￿ࠀ￯￿ࠀ￿ࠀ∟∢찢ﻞÿ\x0fᄐ™ᄒđ￿ࠀ∟鄀ﾪČᄐ￿ԑĝ\x10ᄁﻡ￿ׯ༏ᄀ℡\"࿰\0\0＀⌳Ĳ‡༒\0༐\x10\x11ᄐᄡᄑሐᄐᄀᄑﻯက∁ᄑ࿿\x0f∣쳍쿭î∮눢믫믜￿מ໮ −툡駞쳊༏＀ᄐᄑ\x0fက￿ࠀ\0ሡ༢ĎĐကᄑༀﻰ!︒ÿ￿߿ᇞ∠\0촀\x0cက℀ᄑđ⇰\x01တ−ᄂᄑ฀က￿ם￮ ℒሡđ１￰￿ࠀǯ／ἁထ쳐￿ӌ⇬ﻢ΅ᄁ￱ȑ︀￯ჿ￰\x0fခ࿰⋿ሑ￯༏Đထᄀሡÿᄐ﷟묏჻໭ᄀāᄑ∑ᄡሑȐ￿ࠀဏﻮÿ\0ꃿº\0༏\0\0Ďđ￿؀￿ם෌\0\0﻾ﻯ￯࿾￿ӟᄡᄑ\x01đ唀㑄ༀကᄁĐ\0￰ხᄑℑ∢⌢\x10ᄁᄀĀ＀￿ࠀ໯ထĀᄑᄐāᄑ\x12\0쀀￿۞ထᄁᄐᄑ\x11\0ᄑခ∢−Ē\0℀ᄑᄒ\x01\0／﻿ïༀ￰༏＀￿ࠀሯᄢ−∲̢\x01Đ￿ࠀ￿ࠀ࿿ÿ∀ሒ鈢骙ꦪ￮î\x01Ā\0\x1f\0\0ሐሡĢတđ\0\0ᄡᄑ￱࿰\0\0ჿခᄀ／￰ဟ\0ᄀᄑ\x11ထᄑ\0\0ᄐᄑℑĐㄑ㈣⌲\0ᄁᄠᄑᄑᄑ￿ࠀ\x0f༏\0￰ᇮ\x01\x11\0ሢ\x11\0\x0f࿰㈲㈣﻿ᇿĀ\x11ჰĐĀĀ￰࿿∀ሢጳđĐ￰ᄟᄑ\x11ď퀑ᄑᄑ￿בî\0￯﻾\x0f\0ᄀᄐ\x11\0\0đᄁ\x01\0＀￿ࠀ໭\x01\0\0\0ĀᄀᄁĀ\0￿ࠀ\0\0Ā\x11￿؁￿Ӟ﷍￯\0\0က࿮\0￿ׯ￿۝\rထᄁĐထᄑĐ￿Ԁ\x0eí\0࿿＀￿ࠀခᄁ￱ༀෝༀ￰／￰࿿\0\0\0ᄑ\x01\0\0∀ሑ￿ࠀ−㌢ᄓᄐ\x01￿ࠀ￿ࠀ￿ۯ￿ࠀﻮ￾￿ࠀ￮￯ᄑᄑ࿱\x10ᄐကᄑᄡ췾ý﷝＀￿ࠀᄑ∑￿ӂ￿לᇭᄁℒ∢ĂðĀđ\x11￿ࠀ￿ࠀᄟ￿ԏ￿מ﷝￿ࠀ￿ࠀ￰컮ᄝℑ\"ᄐ￿ӎ\x0f\0ᇰđ ∢㌲ကᄏᇿᄑꈒꪪ馚\x0f\0ᄐᄡ￿ԁờሑ™￰ᄞā￿Ԁ࿮\0ᄑℑᄑᇿᄑȒ＀࿰࿿뷜ￋ\x0fđ℁ሑ℡∢∢∡\"\x11đ\0\0ℑ∡\x02ÿ\0῰ᄑᄑ\0\0￠탾ჾ\x01ကᄑĐ\0\0\0⋞∡숣쳌쮼／⳿⏏ဿĀᄁ™ሒᄡခᇝđℑ∢ሒ\0￾\0＀￿ࠀ￿߭﷿\0\0ကᇿ\0ÿĐ\x01\0ᄀ\x01\0ᄀ\0Đ\0\0Ā\x10／࿿࿰࿿ༀ￿ࠀ￿ࠀ﻿∯ሢđᄑᄀᄒ⌱\0＀\x0f࿰ᄀᄑ\x11＀\0￿۝￿ۜ￾￿ࠀ\x0f\0\0\x01\0\0Āᄐᄑᄑāÿ\0တ\0ༀ\0\0ༀ\0က\x01\0＀￿ࠀ\x0f\0\0Đđᄀᄑ\0\0￿π￿֭⇛ᄑတᄒ∡\0ᄀ\x01\0\x0f￰࿰\x0f￿ࠀ￿ࠀᄟတဒ\0\0＀\x0f\x01ǿဏ\0ༀ￰ÿ\x11\0ᄑᄑ\x01⃰အāကᄁᄑ／ᄞᄑ\x01\0\0ခ\0ᄡᄑ쪱첛Û\0࿿ÿ\0ᄁᄑ￱\0ð\x0fÿᄀ\0Ā\x10\0\0\0\0\0\0\0ᄀāက\x01\0\0\0\0࿿ᄀᄑ῿ထᄁ\0\0\0က찁뮼ﳌ￿ࠀ￿ࠀ࿰ðﻠﻯჾခᄀāĐᄑထᇭሑĢ∁ሡ\x0f\0￰￾࿿￿ࠀ﻿ᄡᄡ￿ס췍ᇋᄑđ\x10\x01ÿ\0ကĀ\x10\0ကᄑ\0\0\x01\0＀฀à\0\0\0Āᄑ￿Ԁ￿מᷝᄑᄒ\x0e\0\0࿿ᄐᄑᄑᄑ￡￿ࠀÿ\0Ā＀￿ࠀ\0ༀ￰／ᇿᄑđ\0\0￿ࠀÿ￰￰／\0\0\0\0ကထᄁ￿ࠀᄞᄑ∑∢ሑᄐᄒ\x0fༀᄐᄐထᄁᄀᄑᄑ\0\0＀࿰\0\0ÿﻰ\0\0￿ࠀ࿰༏￰￿ࠀ฀က\0￰￿ࠀ⇿ᄑ\x01﻿\0\0࿰￿ࠀ／￿ۮထ−ᄑᄑ１ÿ\x0f\0\0ᄀ\x10ᄑᄑ밑뮼஫\0\x10ခᄑñ\0\x0f\0\0\0\0đ\0\0\0ᄀĐ\x11\x01\x01\0\0￰࿿ໝ￿ࠀð\0\0Ā￿ࠀ￿ࠀ\x0fპጠᄡℱ㈣ﻱᇭᄑġ℁Ȑÿတ\x01￿۞ￜ￿ࠀ࿾ᄐ⌳㐠︃￿Үᇊሡ\x10\0\x01\0ᄐ㈱ᄁĐﻰ⿿Ĳ༑쿮￿םǭ⼂ﻮ￿۞ﻝ－༎⺾愳ህ￿ۯ첼\x0b໿￿מᄐᄑ￰\0ჰȟ췯컫∽ᄁᄑ∡đ\0Đ탱￿כ￾﻿췯립஫췿￿ࠀ￮￿ם췝Ìᄑℑ䌣⌣＀î␿㍔匴坔⑥!￱༏ခ㈰ሁㄲ⌡ᄱ㌓⌲￿ࠀğༀ￿ם಼￿߿ﻯỰ쳜￿ʜËỾ\x0fð\0ჱ\x0f＀ᄐ￿ϱỏ\x0f\x01ㇰጡ≃#ﻮ࿿\x0f￿ࠀ﻿￾￿܀짞තᄐ\x01ᄁ\x11＀￿םპ\0ༀ\0\0đᄏ＀࿰ሀ\x12、㈒ᐠ\0\0ðဎ￿ۭ໿ჰȁ࿿Āሐሲ밓ꪻ몫ﻮ쳟￿׌ჭ\x11ᄑ∢∢ᄑ\x11ᄀᄑ１໮ကᄀᄁ\x10ñထ２￿ࠀ￾췭쳝�࿿ᇿᄑ\0\0က\0þ\0\x0fÿ￰￿ࠀက໯\0࿰￯ğđ￿ࠀỞሒᄑᄑထ￱￿ࠀþᄐ쀁￿׍￿ӎĐ\x01￰￿ࠀÿ\0￯￿ࠀༀ࿰࿿ᄀᄑĐༀἀ\x01ᄐđ１༏⃰∢ᄒ﻿￿ࠀ\x0fÿကကᄒᄑ\0\0ᄐᄑᄑᄑᄑᄑᄑༀð￿ې컝ᇜĐᄑᄑထ￿ࠀ￾\x0f\0ð탿￿ם￿ם\0ð／\0\0\0\0ကᄀᄑᄑ㋾∢￾\0\0￿ࠀᇿᄑ\0﻿￿ࠀ￰ᄏᄐထĐÿ￾／\0Đ＀￰￯﻿\0\0￰ð\x0f\x11\0\0ༀ\0ༀ\0ကကÿð\0\0℠ሒ\x11\0￿ࠀက\x10ᄐᄑꬑꦫ஫ð\0ༀ骿릺«\0‏ℒሒ࿰༡㇢⌓ༀð\x10ခ￿ہ࿿࿰￞\0\0໠ᄏᄑĒ\x10\0￿؎\rÿℑđჱ￿ࠀ탭ÿ\x0f\0￯￾￿ࠀ／︎￿ࠀÿ࿿\0\0\0＀࿿￿ԏ￿ם￿׌볝￿ό췜￿Ӎ\x0cð\0\0￿ࠀ췏෾⋯ᄑᄢđᄑ\x10\0ᄐℑﰢ￮ကᄒᄑ￿ࠀ￿ࠀ￿׏⋞ᄒᄑᄑᄑđĀ￿ߑ￿Ԏ჎ᄀတℑ∢ༀ\x0fကჰĀ\0\x11\0\x11\0￰￯￿Ӿ﷏￿ࠀ￿ࠀ\x01￿׏￿ۭï\0\0\0\0ᄑ∡ᄒ\0ᄀᄁð\x0f﻿￯࿿ဏ￿ӝ༐ð뿠ᇏᄁĀ\0\0\0ᄐᄑốხᄑ∡￿Ӌ\x0e\0ᄀတ\x11\0\0\0\0ᄐᄑ︑ﻯໞ＀࿿\0\0￿א￿ۍ༏ခᄑᄑā\x10＀໯\0\0￿׮ĝᄑ∡ሣℑ￿۝쿜∬∱༳￟﷎ﷰ￞฀￰￿߮￭࿰ữဒñ뷊ೞ﻾ကἢ⇱ᄀ￿ࠀ㻠\0ᄐ﻿￿ۿ쿭￭￿ࠀ￿׮￿ӮÿᄀᄐĐༀð￿߿︯\x10\0࿰￾ᄏᄁ툞쳍骛ዮ⌲ȳ\0࿡쿮∜Ġขჰ﻿￿מĀᄀ࿰⿰ℏᄑĐထℂ￰ÿ﻾࿰⻰∑\x11￿ࠀ\0ሯሡＡ༏￿߲︎뿭࿭ሀ㈱ሣ\x01￾￿ࠀሏġàð탿⼂ğ⇯∢đༀ࿰•ᄂက฀࿠༏︀ကကෑᄐᄡฑᄑđ∐ሢﻯ￿מ￿םĞ࿯냰첼쳌ༀℐᄑāတȢ\0\0−㐢￴￿߾í︓ÿ\0\0ᄐᄑᄁሑ﷿쳮ᄬሡ￮࿰ᄑ\x01\0\0⇯ᄠထℑℒ∡ᄡℒᇭĀᄑᄑĐ￿߮컭Ďᄐ１￿ࠀ\0\0ᄀĀᄀᄑ\0\x10\x01￿ࠀ￿ࠀĀᄑ\x11Ā\0ခĐ\0\0\0\0＀￿ࠀ࿿\0Ā￿ࠀ￿ࠀğတထကကᄑᄑ\x11ᄁ༁ðð\0ᄑđ\x01￿ۭ￿Ӯǭတđ\0\0ထᄁᄐᄡᄑᄑ\0\0\0\0ðĐᄑ\x01＀ကᄑĀ￿ࠀ￿ࠀ\0＀￿ࠀ࿿∀ᄒ\0\0࿠ᄀℐᄑᄑᄁᄑᄑᄑ\x01\0က℡™༏ᄐထ\x11￿ࠀ\x0f\x01\0＀࿿\0\0ᄑᄁ￿ˑອᄌᄑđထĐༀð\x0fထ\x01\0\0ခ\x10ᄑᄑ￡ჿ\x0f\0\0ကᄑᄑ￿ࠀÿ\0\0ÿ￰￯￿߽\0༏ᄀ\0\x11\0\0\x10\0\0\0℀ᄑđ\x01Ā\0\0￿ΰ겚É\0\0\x01ᄐ\0\0\0က￿ԁ￿ל﷝￿ࠀ￿ࠀ\0\0ꦰﻞÿᄀတထĀᄑထ￱\0ᄀᄁကᄀ\0တထ\0\0＀￿ࠀ࿿\0ÿ\x0f\0ༀð࿰／࿯༏\0\0／￿ࠀÿ\0\0࿰\x0fĐ\0\0\0\x01\0\0ﻯÿ\0￿ࠀ￿ࠀ῿ᄑခ\0\0༏￿ࠀဏđᄐ\0ༀ￰ð\0\x10\0ð\0Āᄑ\x01￰࿿\0\0ÿ\x0f￰￿ࠀÿ\0က\0￰￿ࠀÿᄐđ＀\x0f\0\0\0／ᇰ￮þ༏￰࿿Ā\x01က\x11࿰\0\0ᄐđ＀\x0f\0￿ࠀ࿰ᄟđᄑဂ\x0f＀￿ࠀ༏\0￿ࠀ\x0f\x10\0\0\x01\0／￿ࠀ῿ခ\x01ᄑ\x11￰／ÿ\0ÿ\x0f\0\0ᄁᄐ\0＀￿ࠀ￿ࠀ࿿／￿ࠀ\x0f\0\0\0\0\0\0\0\0\0ሀᄑᄑ\0\x01\0\0∠∲ᄣᄑတ\0\0\0\0\0\0＀\x0fĐ\0\0￿ۭ﻿þ\0퀀￿׭￿ם￿ם༎࿿￿Ӟᶼᄒထ༐\x0fༀ࿿\x0f겼ꮹလᇀ/\0￿߿￿׮／ÿ￰￿ԍ쳍ᮼ∢㈤ᄑĐ￿ӑ⋝∢∢∢࿿ሏ⸒팁￿ם￿ۮ࿽㌀㈳̢\0\0\0\0\0\x01퀀볝￿Ӝ￿۞￿םም−\"￰￾￿ࠀ∢∢￿גîđ퀀￿ӝ￿ם∢∢༂ÿ༐\0\0Ā\0ᄀđ￾￿ۯ쳝췝￿׮쳍벺Ⴚထᄑᄑᄑ￿۾￾ﻮ﻾⿮∢∢ⲭȐᄐ\0∏∢∳∢∢￿ۭ￿םᄝ∡찢￿לⷝᄢሡ￿ם\x1fሏ䀱㐢ጳ℣\x10\x0f\0∏∢Ȣ\0\0￿ࠀᄟᄑđ㴡Ᏻ∑ᄂ\x10＀∠∢Ｂ༏࿰\0\0∢∢￲༏￿ם쳝￿ל\0\0ሐ‡⌒™￯﷯￿ࠀÿᄟ\0ᄁᄑ∢ᄢ∢\0뷐쳽ᆾᄁကℒሑ໰￮îༀ࿿ð\x0f࿐＀\0\0퀁쳞￿ל\x10\0ᄀĀတ\x01Đā\x1f༑\x01⇿ሡđ\0\0﷞ฏǰ\0ﻯ￿߭က\0ᄐℑထℑﻰ￾￮࿿ÿ༟ᄀꄐꮺ뫊ﻞ\x0e࿿ဏ\0\0\0\x10﻿∝ሡᄒᄑđ＀\0ᄡ−￱࿿￮Ữ∢∡\0\0Ǡᄀᄁᄐᄑᄑđ\x10￿ߐŏ༂／\0\0／￿ࠀᄟᄑထĀༀ\0\0\x10ထ￰￿ࠀ\0\0∢ሢ⋮ሑ﻿￮∢⌳㐳㈲#\0\0࿠﻿ჰĀđ\x11ñ∢ሒ麱ഞዡᄡĒ\x11\0ᄐ\x11\x10\x01\0ကㄣ−ﻯᄞᄑᄑᄠĢ\0\0\x11ᄁ\0ᷞā\0\0\0몰￿ߝﻭ໮\0\0\0ā\x10\x01໿࿰\0\0რ\rတﻮÿ\x0f〒ᄒᄑ™㌢㌳￿۞뷻\x10￿ࠀ￿ࠀ￿ࠀ༏ᄑ∑！﻿￯￿ۮ﻾￿ןﻮᇿ\0က\x01\0ÿ￿ࠀᄡ∑￿ג￿׍Ýᄐ턑￿۝﻽ခᄑ⌠㌳︳໬Āᄐ\0༏࿿ༀ\0ༀ촁볛∢￿םǌᄐ℀™ᄑ࿾쳎﮻￿ࠀᄑᄑ﻿ǯĐᄑᄑ￿ב쳾￿כỾ퀀ﳠᄑሒ￡ď࿿\0ᄐ﻿﻿ᄞᄑᄑခǰ뿾￠࿿ᄐ\x11∀∡ሳđᄑﳍ컮ማሡ\"\0￿ࠀ\0က‑￿ʭ￾⿿ሢ∡쳌춻ᄝᄑ￿؀࿿ᄀᄑ−ᄢ￡ᄏᄑ\0༏།쿮￿׮ﻮჿ̣ᄑᄑ\x11ሡȢ\0\0ᄒ∑￿ࠀჿᄐ ሑ∡\0\0ሠ⃠ᄠᄑ℁∢∢￮࿿฀\0\x10\0 ⇂∾ᄒℑ∣−ሢℒ\x01\0＀￿ࠀༀ￿ࠀ￾컾￯／＀￿ࠀ￿߿췝쯍￿ࠀ⋮ᄑ\x10\x10ခထ∑\x01＀࿰ÿ༁ð／￿ࠀ\0￿ࠀ\0\x01\0￮￾￿ࠀἏᄒᄑᄐ\x10࿿ჰ￿߿￿۝쳍볌ﻼ￯⋾ᄒđ︀\x11Ā￰\0∁ሢထᄑᄑ\0\0Đထ﻿ჿထ\x01／࿿￿ࠀ࿰\0\0 ∢∢ᄑᄐā⇲㈯∢∢ᄢᄒ\x01ᄀ⋮∢ㄑ㌣㌳\0\0\0࿿ﻯ￮￾࿿ᄀᄑℑ∢∡ሡᄢ쮲￿ּ⋬ᄑđ\0ðༀ￰￿ࠀᄀᄁ༏ခĀ￿׮췮໐\x0e\0\0\0∢∲ᄒᄑ１ฏᄀሑ몚궹\r\0ᄀ\x11\x11\0\0\0\0ᄐᄁထᄀ﻿\0\0므à࿽￿ࠀ໠\0ᄐ∡ሢꬢꪺᮼᄑᄑᄞᄁĐ턢￿׮ﲼĝ‑１༏⃰㌢䌳￿ࠀხ\0 ᄢ\x01￿ࠀ￿ࠀሯ䉃Ｔ\x0f￿ࠀ￯쳌볋\x0c\x01\0\0ჿᄐᄑ\x0f￰￭໮／ℑ㌣༂\0꤀뮪઩∀ȡĀ\x11￰༏︀￿ל몞ᄡထ￱￿ࠀĐ﻿￯￿מ쳌ﻫ\x0f￿ם쳝ꪬᄑᄑ\x01࿿ℏđ￿ࠀ\x0fကᄑﻡﻮ￿׾￿ם໭Ā\0တᄑᄡ−￿Ԣ￿۰췭쯜컍üᄀ∡￰￿μ￿۝∮ᄑ＀／ကᄐᄑ￰￿߮\x0eî\0￾￿ׯ￮\0 ᄳĐ ᄢ￿ࠀðሐġᄐကℑሒ\0\x10ထĀሐ\x01ༀ￰￿ࠀ￾\x0f\x1f\x02ðကခሑᄑℑᄐȢ\0\0㈢䐳￿׭ዌሡĒᄀሡ\0\0Đတ℁∡ÿ\0￿ۮ곌ᄊᄑᄑᄑđ\0\0\0￿۠ჾခက−ሒထ℁āကĀ\x01\0\x01Āကᄑāက１￿ࠀ\x0f￰￿ࠀሢᄢĂ\0ᇰđ\0\x10ᄀကᄀ￰îᄐ\x11\0Āᄑༀ￿װໞ\0\x0f￿ࠀ￿ࠀ\x0f\0ð\x10ကℑᄑတᄁ￰࿰ð\0\0က＀ð￯ჾ\x01\0\x01\0\0\0\0\0ᄑᄑĀ\0က뷍鮻ᄛထ\x11\0\0ᄁ\0￿ࠀ￿ࠀ\x0f\0\0\0\0ခᄐ￿ࠀᄞတᄑሑ℡㈢㌣\x11တ／ჿĀ\x10\0\0￰￿ࠀğ\x01\x01퀀췭￿ӝ\0ð\x0f\x01ờᄑᄑᄡᄢ￱￿ࠀ⇿ሑđ￿ࠀ\0\0\0\0က\0￿ࠀ￿ࠀᄑᄡᄑᄑ\x11\0࿿ᄐထ﻿ï\0\0ðᇿ\0đ\0\0￿ࠀ\x0f\0ᄐᄑđ\x01\0\0\0ᄐđ\x11\0\0ခ\0\0Āā\0￿׮ﻮ￿ࠀ࿿ༀ䇣Ｐ༏ကထ￡༏࿰ﻯÿĐ\0ထ\0ကℑ\x01\0＀￿ࠀ￾໰\0\0ᄀᄑ！Ἇ１\x11\0\0￿ࠀᇿᄁ\x0f＀￿ࠀ\x0f\x11က\x01\x10࿰ကĀ￿ࠀ￿ࠀ∮ሢᄑᄑ\x11ᄀ！￿ࠀᇿᄒ￿מ￿۝￿ۭ\x0e\0！༏῰ᄁ\x01\0ᄁ∐ᄑ１༏ᄀခ\0ༀ\x0f\0ð\0\x10ထĀ࿿＀ᄏᄡĒခﻯ￿ࠀĀᄑ몱몫ྪỿᄡ￱\0\0ᄐđ￯￯\0ကð࿿ÿ\0\0\0￿ࠀĎထ༂﷮\0ሡᄢ㌱㌳ᄴĐ\x10ð\0\0ခᄀđᄑāကᄑ\x01ᄐ\x11릐鮙¹\0࿿\x11\x01ÿ\x0fĀ\0\x11Ā﻿﻿￿׌\x0eᇿ\x01\0\0\0\x0f\0ᄟđ༑￿ࠀ࿿\x11\0\0\0ἐ∑ဢሐ\x11\0\0ᄑᄑ￾￾࿿\0\0ခခꪠ쮻⋋∢䈒䍔⌤∢㈢￿߱﷿⋞∢−∂ሡᄐ\x12\x01ༀ￿ۿ쳞볋￰ﻯ\x0fᄀĐကᄁሐ໯﻿\x0fༀἏᄁᄀ￰￿װ໌ǰምထ︁ヮ䍄⌳ᄡᄒ\0\0仿㌳䕃￿ࠀ࿰믞￿ξƻᄑĐ\0ሒሡᄑĐ∡∑ğ\0\x01￿۞볋\x1a\0\0က\0\x11\x0f￿ࠀ\x0e\0￿؀ờᄑ\0￰Ğᄡ㈒⌣Ⅎ≃ጣ⌢∡⇞Ƞ༑\0\0ጾ‣ဣ￿܁쳭췜ༀàᄞᄒ찑쮮᮫ဲ！ጡဢ￿Ӏ￿םይᄑ༐／ထ\x01＀࿿￯ﷰ࿰ð∢㍂ᄔℑ༒￿ࠀ༎㌳㈳￿ߢￍ뻿첛ꮺ࿿ᄀ\x01\x0fᇰďđ\0\0Đ࿾ÿ䐟䕄⍓∂Ġ\0\0∠ሲဢᄑထᄓȡༀ\0ሐထ￿Б볌ᄑᄑ￱ༀ\0Ā퀀￿׍￿םဂĀᄑĐᄑሑሑᄐᄁᄑᄑ༁￿ࠀ࿯\x0f\x0fᄑᄑﻟ￿ۭ﻿Þ껿ﺫ￿ࠀ쳟￿۞￿Ӟ᳍Đထἁᇰ\x10ထ\x11\x10ခᄑℑ\0ༀ￿ࠀ￿ࠀ￿׿췍㯌ሒ™đ\x01\0＀＀Ữ∑∢Đ༑￰\0ᄁÿ\x0fሑထ￿ӝǍﻰ＀￿ࠀ️Ỡჰ\x01＀￿םᄐထᄑᄑ１￿ࠀ࿿\0\0\0ð턑췌ሳĀ﷠￿׍Ì￰࿿\0\0\0ðሞᄑ\"\0ᄑ∑ሢ�ကᄁ\0＀￿ࠀ࿿ðĀ릪첫￬ȑ༏ᄐᄑ￡￿߾￭￿ࠀ￰ℑᄢ￱￿ࠀ￿ࠀﻠ࿭\0\0\x10ခ\0\0컾\0＀ÿ༏\0\0ᄑ\x11\x01℀ሑ™㈲∢\0\0⌠∲ဣᄁက∑∑Đ\x01／㏿㌲̓\0\0໮࿰￿ࠀᄀᄡ™Ē⑐ꫮ\x0c㋾⌢\x10ㄑ￮တ࿠︍￿ࠀÿ／໰　∳⇾∑숡첼￮໿\0ᄁ￿܀໿࿿￿ࠀ\x0fđᄏﻱ탿곮￿ϋ⿟ሲ\x01뷎ꮪ\x0bĀက‏䌑ሓ∡™࿱ᇮᄡ췽￿Ӟ￿ࠀℐ࿯ჰᄁÿ\0￯þāﻰ΅℡ᄑ\x0f\x0f￱ἀ￿Ԁ￿ם￿ࠀ￾\x0f￿ࠀﻰ༂ğ뷽ǿ\x12đᄡထ\x01ሯℑ＂﷍￿׿￿מோǰ\x0f￾￯࿯＀༏ﻮﻮ࿿쳿ĝ`Đᄑ﷝ÿﻰ￿߮࿯／啵䉇ᄑ\0ሀჱ໮\0ᄁ\x0f\x01\0\0븀쳋ໞᄁᄑᄡ∢࿲࿰č\0\0\0∢∲࿲ကℐ༒＀ÿ\0\0Đ⃱￿܏￿ԍﻟÿ＀￿ࠀ༏ð컰쫍ﮬ￿ׯ\0\0쮰벻ᇌᄑᄑᄁĐᄑᄁ㐱㈳ﻮ࿿ሡ∑∢㈢\0﻿ሑ０ᄑထᄁᄑđ༐\0∢∣￢￭ကᄀā\0\x0f￿ࠀ￿ࠀÿ\0\0\0￿ࠀ࿿￰\0ᄁᄑđ\0\x0f￿ࠀ\x0fﻮᇿᄒ눑骪ꦚ￿ࠀᄟĀᄀᄑ࿿￰\0\0ᄐᄑᄒᄑȑĀᄐༀᇰ\x10ခᄑᄒᄀᄁᄐ￿ࠀ￿ࠀᄟᄁ∀ᄒđᄁᄑ\x10\0ᄀᄑᄑĀđᄁᄑđᄀ￯ᄏᄑđ\0ᄀ\0က￮ðÿ࿰ᄒ\0\0Ā\x11\0\x11\x01Āༀ\0ༀ⇿ᄑ￯ᄑᄑ￱￿ࠀ䐀㌳ф\0\0￿ם￿׮\x0e\0\0\0\x11\0\0Đༀༀ∀ሢ섑￿ּ쳜￿ࠀÿ\0ᄀ\0đ\0\0\0\x0fĐ\0０ﻮ￿ࠀ￾\0\0㌰ሢအခ￿ࠀ￿ࠀᄑᄑāထ\x11໮\0\0￿ν믋⑜单F࿰ἀ︢㋾Ȁﻠ￿܋㋮෋￿Ԁ뮯ௐꮰ*﷏﻿࿿\0\0ðﯰ릮￰༏㐑ᄟ￰￿߿෱＜\x11鬀ㄑȑ῟ℒ＀귟＊﻿컍ఏ￿۠￿۽࿿ᇯฏС쵃\r�샾သ\x13⏯\x01껏ᷜ￿ό￿֬쳛﻾ï￿ࠀđ༁쿮Ȯ\x0f༐㌒ᄟ∀ᄒℏს༑﷑⿾ᅃ挥￿ۿ΅뻞￿׮Ğ\x10ﳰﺱ\0０뷐ᇯ￿۠透붭，톼Ἅጠ℁刃Ā￿Ӱ﷞Ǯヿ╒∡἞Āự∁쳟겼ƻ∐•ㄒ탰đＯ䄲ｕğကἢ￿ӎﻬ\0㌯ሢ`გ儀ሢꪰ￿ϝￎ⻑̒\x0fñἯ∡∳츪⯀䓱㐱\x1cᇰāďȑ\0\0ሁ㈲Đᄁ㌁䐠ጲ\0\0ἀÿ፟၅￾௞သ꫿ﶻᲽℑā\0\0ᄀᄑℑ㈢⌲￿ࠀï\x1fတ㌀⌲⌳⌢㌳\x10ᄑþ￰￿ࠀ￿ࠀ￿ӌ￿׌쯍벫ዊሡሢ\x01￿ࠀ￿ࠀîခ쀀￿׌ᄑᄑ￱࿿￿ࠀ﷮￿ࠀ＀￿ࠀ⌿䌳￿لﻝ࿰ༀ\0￯￿ۮ벼쳛￮\x1fðᄏᄑᄑሑ∢／￿ӏ췌ዬ∡Ȣ\x01\x01ᄑကᄐ䄒#ᄑℑ∣⌢\0ð￰￿ࠀ⏿ᄢℑሒᄑđᄀᄑℑ\x12\x10က\0\0ð뮰쮫ᇋᄑđ\0\0đထ\x11\0∀⌢팳쳝쳌￿ࠀ\x0fᄁᄑ\x01\0￿ࠀ∲∢ᄒကᄐᄑ\0\0髿ꪪ￿Ҽ￿۝￿ۮ㈳㌳ᄒሡᄒđđ／￿ࠀðᄐ\x01餀ꦪ஻\x0fᄐ﻿￿ࠀᄟ\x10ሁᄑđ\0\0\0ÿᄐတ０￯Ữᄑሑ\0\0￠￮⼀㌢䐳ᄑᄑᄡᄡໟ\x0f𢡄Ά　℀ᴼ㲲폝㰞㳝⼱쳡㼓ྰዎ【ೡ벰⼢ꂲႛⷾ㼁ﻃ뼀ﳭᴏሞ™௑㻢￿۠찐Ḃ뇼쯰쀎￿۰ⷯ︍샯ᆿ틋࿼눭볢῟ᴍȬỢ㳏ß￿ӟ뷮\x11ଭ￿ϻ켋⇠￿۲￿Ԁƽ໣ሑ볂ⷲﳑ฾뻂쀀쿌ⳮ밢Ἓ﬿춾\x12Òዱဋ찒뻡츫﷍ΰ￿׋￐ௐⴃ῭⯝ᯡ⃍᳼⴬퀌⴯탍㋓Ǽ틂쌛폜퇰퀫ᄃ틿Ⴜ᳼넰ⷎ켒Ჽ닝Ǭ⇏쌐ฏ븑ሮ￿ס숾⻾ﳭ밂ȋ쀞ﴼ￿Կￛ퀢뷠᳃붱￿܃섡∼ㆿ곱￿؝ⴢ⏯ꀯ귙ീೱ￿ӝ쏒눞ㄾ໼⼡쿯틿툌￿܍ﳞﰣ㯲ሑ∡⌢ȏ﬜￿߲뻾￿Ю숎₳￿ߝฝȒ탍바Ჽ⿞퇾쀜︌䋰ᄀ￿؞ผဏ㼭．࿿쮿뷋⇜ᄒᄑ‣ᄡ。￿ׁ췜ნሁἁĀ\0ༀĀတ࿾ခ＀࿐﷞ḀϠတā￰໿ð\x01\x1fခ\x10ᄁ￿׭￿ېẽခ￿ࠀሟ™༒ᇱᄟ\x12à\x1fሑ뷲೟ༀ\0쀀∿￾﻿ỰᇿᄁĐထ\0\x10ᄑ℁ᄐ＂್\x01ᄐ캽뷙Ğ࿱ïĐἑ￱︀ÿ\0ሑ\x02ထခ췝컌∭∲ㄢሒㄠ∳ခ∀໒￞‎ᄁ\0\0Ā￯ᄎđ′ሑ턡쳮쯜ᇽᄠⶻ≃༃ሠ∳⃫ā／￿߮쳝\0︀￾က️࿿￿ࠀȠ‒⌑㈲￿Ӟ⌱ㄣ￡࿝ﻰርἐǱဏἀ༎\0\0￿۞\x0eကἐ∁ሐဂሀ\0\0￿ΰíĀကㄢሢġᄑሁᄑ\x11\x11໠／／쳏࿮ሀሒᄐℒဏＱ뫏揍̣Ȳ⃱ტ쿾￿ࠀ／￿ࠀ࿿\x0f￰\x1e༒㸳䈅ጓ．￯️￿ۜﻝ췜ᇜ퀀췾￿׭／ðᄏ฀쿞￰ﻮⷿﾬ곜￿λ￿ӽ쮭ἀð￿ࠀ࿾￿؀￿ʽಽ⇠đĀ༐࿰ᄏᄑℏȐဢ넟￿ӿ勮⌓༟\x0f\0ἀ\x01￰뻮쳝໮ᇱᄑðℒᄒāฎ￿װﻮ῰\0ᄁᄑ\x01℀䐳퀏￿ӽ￿׎믾ဎğ⇱á໬ÿ￰ဏ\0ᄟ™긣೰️࿠￿۽ﻰﮠ￿ۋ⋾ᄑ￝￿ࠀ໯࿰࿠\x0f฀￐ý︐켎໠\x0e￿߾﷽￿۝￿ۭ䑵㌤ሢᄒ༎ÿĐᄑሑအ\x01￿˰컾೰ℑ⌢က∑Ē\x12\0တđ\0\0＀\x1fℑ∑ሲﻐ￭\0\0ﬀẟ￿ج￿Ӯ೮ዟဏ￯ğðĐက\0\0￰ÿĀ ㈢−࿰࿰￿ࠀ￿ࠀ࿿\x01\0ᄑᄑñ\x10༁￰＀／￿ࠀ￮༏ᄏခ\0\0\x10ᄑᄑ＀ᇰᄑĀ\0\0￾﻿￯￿ࠀ࿯\0ༀᄑᄑ㌱㌳ᄳᄑ￰￿ࠀÿᄏᄑ섁쳜쳌\x0f࿰\x0f\0\0\0 ∣∢\0ᄐᄑᄑᄁĀ\0\0鮩뮩ရ\0\0\x01 ᄢℒᄑđ\x01\0ሀᄑᄑᄑᄑ￿ࠀÿᄟᄑထ\0 ∣∢￿ࠀ￿ࠀ／￿ࠀᇿᄑ\x11\0\0\0ༀ￿ࠀ￯⇾ᄑ숒쳌￿Ӝခ\0\0\0℀ሡđ\0\0䑔呕\x05\0ᄀᄑ￿ࠀᄑđ࿿ᇿᄒ￿ࠀ￿ࠀ\0＀∯∢༢\x1f￿ࠀ￿ࠀđ\0￰ﻯໝ￰￯\0\0ሠᄢđတđ\0\0\0\0\0ﻝ／￿ࠀ\0\0\0ᄏ\x11\0\x01\0\0\0ᄐᄑ﻿￿ࠀ쿛￭ხ︯쳱ჟᇽဍﳍ켐Ǐ￿Ӡᳯÿ쳟췡탍컝ⷽ겫ட࿡ĝ࿭퇍️⸝პ￿А쀢컰넏ᴀ᷽퇍㿠￿Џ촜ﯾ￿ۯ￿׎ჟﻯ﻿￿מ￿ۡဎ️￿۱틑￿̍￿׫췀췮㳯믬￿ϞῠǬ＞﻽ᄠ뼏상찜￑￿א￿Ӡ￿׭ೌ⯢ሌ컼係āﬓ໼￿ߞ⼃\x1c೮⇍￿גﻞᇁǝ밎ᳰ￿ߐġುďⷰᐎ໋︠଎ო￿בිໍ⸎뀎￿۝툁쿍￿߯쇍ℋā࿟ೲᰑ턐퇡２࿠￿ܐ췼쮭￿۠ෞ퇾￿Ӭᰝﴓꃼữ쿞붾틍᳻￿כ￿߀໿೽￿ߜⴌﰀ태퇑ỡ츁⿰໑ﻌ￿۽ﳠý໰쿿샛᳢῰묂༌⇱쀜࿰ﴒﰁั퀞ഐ뇮췐ఝᄁᄑ\x12đᄑခ￿׮볍컮\x0c\0１￯￿ࠀ￿׭∢∢ᄒÿ\0࿰ჿ\0࿰췌췜\x0eတခ\0က༑￰ထᄑ༁￿ࠀ໮\0￿ࠀ﻿\r\0∀™ሣᄁᄑ∢∢ﻳ￿ۭᇭĀ쀐쳌믌￿ۮ￿םï\0￮￯ထᄑ໱ÿᄀᄑđ༐￠ᄑᄑᄑ\x10ჰĀ\0\0\0￱￿ࠀ￿ࠀ￿ࠀð\0⌢㈢ᄓᄡᄑᄐ\x11\0ðð\0ᄐđ컎쳌骬￾￮\x0eခ\0\0￰đ\0í\0ᄀℐᄒđᄑ￱\x0f＀￿ۮ￿ӝༀð\0⿿đ࿱ᄑᄑ￡￿ࠀ⇿ሑđ࿰\x0f탿ᄞﻯï￿ࠀ￿ࠀ\x10ᄁ\x01\0ကĀ퀀￿םﻮ\0\0∠ሑᄡñ࿿\0\0\0\0￿Ԁ￿ם￿ӭ\0\0ሐ∡\"\0ﻮကᄑᄑĐ唀噔／﻿\0\0￿ࠀ\0ᇰℑㄐ㈳␳အᇲ몿ꪫ↙ሑဢ∢ሡༀ﻿အᄁ࿰￿ࠀᇾ࿰ἡᄁÿᄐᄑ∁ᄡĒ／ᄐᄑ﻿￿ׯ￿Ӟ﷞￰￿߾￯ሮ™ἀ㇯㌳⌲℡∁＀࿯\0Đ﷮￿ࠀༀ༏ǯ၀ℑȑက\0\x01\0ကᄐđðကᄐ\0ᄯဒð༁\0／￿ࠀ￯﻾ༀ＀ÿ ￾ᄑℑȑ〡㌴㌲\x11ᄁĐကñ／￾࿰ﻯᄮℒᄡĐ１．﻿￮\rက￿Ԁ㻝㌴⌣⌲䍃⇿Đ༑\x01Čက＀￿߾೯ðሢℒℑ\x12탰￿۝ﻭ∣−￿ב￿۝￿׬쳮ᯝ\x10ჰ໯ÿ／⇰ᄂĐ\0\0ထᄁ\0က∀ሢℒሢᄡ\0\0몰춻￭Ⰰ−ġ\0ᄐ\x1f￿؁췝᳞ᄒ\x10\0ก￰／ჾℏ＀\0∏∡࿿∑Ăᄞﻮ￿ۯﻭﻯ퀏￿۞ﻭ￿ࠀ췮﻿།퀐½퀀￿ࠀက༏￿؀࿮෮、ሏāအꬁ\n￿؀\x0f뿿Ì￿ࠀᄀ묡￿۞đᇰ໱ჿđ໐㿭≄⌑￿ۭ\x1f\x0f\0￰﻿ჰ\x01\x11뮡뷬\x0eᄐ༑ကﻯï￿ӏỠᄐ༁탿￿ࠀ￿ۿÝ￠࿯࿿ᄁĒ༏\x01ᄐ⻰︒ዿ℡Đ\x11℃∑ﻲ࿾ď\x01ǰ\0ﻮ￿Ԁᄍ️∏ℒȐ퀏ᄏ￱໾￿װ\r῿\x1fÿ༁쳐￿ࠀﻮ῰࿿࿰ໝ찐ἀð⼀＀Ğ༏ȑ‑￰￿Ӟ￞࿰ǿ￰໛￿Ӱ\x10\0\0\x0fℐ￿ࠀ༏˿〠ሲℂ\0\0￰뻮⼀∢℡Ȣ\x12Āက￿߯\0\0￱࿾⇿ᄒሑ−∡ᄡℑÜ\0\0\0\0￿߮ℝᄑ\x11ༀ￿׾쳍\0Āᄁᄐ\x11\0\0\0\0ᄐ\x11\0\0ᄀᄑᄑᄁᄐā\0\x0f￿ࠀ￿ࠀ￿ࠀ࿰\0တ￿ࠀÿ\0ခထခ῿ᄑሡༀ\0ﻠ￿ࠀᇿᄑ숒\0\0￰⇿ᄑđ\0\0đĀሡᄡထ\0࿿\0တ\0\0Ā\0￰࿿က\0ÿ\0\0\0\0࿯\0ᄐ࿿￰\x0fÿ℀ᄑᄑᄑ℡ሢ∑\x02\0ᄐ\0\0ༀÿༀ￿ࠀÿ\0đ\0\0đ\0￿ࠀ࿰ð\0ကခﻰᇯāᄀ\x11\0ༀ\x0f＀\0ᄀᄑȢ\0\0\0\0࿿\x0f\0ÿሢᄑ￱࿿ჿ\x01ðĐ\0\0\0\0Ā\x10đ\0\0đᄀ\x11ခༀ࿿\0\0\0\0린￿Ҭ࿋￰\0\0\0ᄐĐᄐထ￯￿ࠀ༏ထᄁ￱＀ደℑ컝￿׌쳜\x0cᄑᄑᄑሲĀĐတℑ࿱࿰ဏဒᄑ곌骹ﻻþ\0࿿\0ﻰ￿ࠀ࿰ﻠ￞￿מ﮻￿۾\0ᄑ⋮ᄢ￰￿ࠀ／∢⌢\x03က︁￿ۮ￿׭﻿\x0e￿ࠀ￿ࠀﻠᄡᄒ￱ᇿ∡࿿\0￿ࠀ￿ࠀ￿߾î／ﻝ￰￿ࠀሢᇬᄑ￿ࠀ￿ࠀ㈢㌳\x0fကᄀတᄑ\x01\0\0∠∢￿Ԣ뷌꫋ꪫ￿۞컜鶺ᆹሑᄑᄑ\x01￿ࠀ／ᇯ∑⌳∢ᄢ῰Ā\0ᄁᄂ￿ۮ￿ӝ뮼췝쳝ᄑ￿Ԓ￿םọဒ࿱\0ᄑ\x01ð￿ԏ￿מ￿ࠀᄑ−∢∢ᄢ\x01ðᄑ\x01ﻯ￿׮￿ࠀÿᄑᄑ∡∢Ｂကđ\0\0ሒሡ\0￰෮ༀ￰\0\0Ā\x01\0Āᄀሡ\0ကᄐᄑ䐁䕃ﻯဏď∡Ģฑᄀ\x0eĐ㈑в鈡ꪻꮻ￿׽࿯컮ྻༀﻞ࿿쯀￿ʬ჋㌢ሢñﻠ︐⋏ᄑ툢￿Ѝ඿骬ꪩﻮ￮ÿ뀀쳜ᄑကﷀ￿Ӯ￿߼쯟￯￭ÿ￿׿쳞겺벚뷫\x0f０￿ӟ￿םᇝ∡ᄣ￿Ӟ￿߮￿׽＀႞⌢ÿ／™∑ñက഑ﻑữဢ∢ထð྿탽᏿Đᄒ∢∣ᄑᄑ໡퇾￿Ԟﻟᄏ￿׾⑎ထℒ㈢ℐሢ‐∁ჰᄑ￱￠\0⸂뮻쯌ᆼሢﻯ￮∑㌳ቃ〒２ကğ༑ᄁ￿ࠀﻯ컭￿ࠀ￿ۯ™ᄂﻐ⋾∢￿ࠀ／￿׾∝∢Ữ㐀ጰ￿߮￿Ӿሠ™ሒᄡĒ\0\0ሑᄢ࿱\x10໯ᄑℂ\0\0⃰Į\0﻾㌀⌣࿿ᄏᄀᄑༀ\0ခᄀđ࿰￿߾ჾᄑᄐ⋲Ġဢᄂ／ჰ࿱￿߯တᄑ￿ۡ༏ༀ￿ࠀ￯༑༐ჰ\x10０￰༏ༀ࿿Ýဟ퀁￿ࠀ컏ĉခ෯ᄁᄑ\x10\x01ჾሐ￿ࠀ￿ࠀ໮ᄀℑ턀믝첼￿ࠀ／∟ᄑᄑᄑđ￿Ԁ쫍Đက￿בᇿĐထᄑᄑ／￯ር™༢ᄀ℁㈒⌲čᄐ∑ሢ࿿\0ሢ∢￢ᇿĀ－ᄡሁﻱ℀1￮℡ℒ췁⇞࿲࿞\x01Ģ︀\x10￿Ԁ컝￿ۮᄢá／䓰ሢထℂﻡ／￯￿ࠀ﻿﻾￿Ԁ￾Ữᄑᄑༀ\0ሟᄡᄒﻮ\x0f￾࿿￠Ǟ\0ༀ\0\0ሞᄑ෮\0\0ἀᄑ༑Ā￿ӟ∢∢\x02\x11㌁ጡ\"ထð\x0f\0￿אᇾခℑ㈣⌳℡∢⌢∢\x12\0ကᄒᄑ\0\x01∠㈢￿ࠀ￿ࠀ￿ۮထ\x11ñᇿᄑᄑ\x10ထ࿿￿ࠀﻭ￾⻮ᄑᄡ\x0f\0￿؏￿ם෮\0\x10￮ď\0︀⻞∣∢ကā￿۠࿿ᇿကî\0࿰ÿᇿ\x01࿿￿ࠀᄑĐခᄁᄑᄡđĀᄐ￿ם췌ᄝᄑᄑᄁ℀∢ᄑ￿ࠀ\x0fᄁ￿ࠀỮᄑᄑ\0ༀᄟᄑᄑሑ∢㌣㈲ᄒሐ쳑췝ᇜᄑđ\0တ\x1f\0\0ð퀀￿ם￿׭đ＀\x0fā￿ۮ໮ðᄡᄒ⋮ᄒထ\x10ༀð∐∢Ȣ\0￰ᄑᄑ\x02\0\0ð࿿\0\0\0\0\0\0ð\0ꀀ겚쯚࿿ÿ￿ࠀǿ\0Ā\0\0￿ࠀ￿ࠀ\x0f\x01ᄀᄑđထā\0\0∠ሢဢᄁကᄑᄑက\0ᄐᄑ￿ԁ￿׭໮\0\0\0\0ꪯꦩ⊪™Ȣခᄐđᄁဒ\0Ā\0ကᄁĀ\0\0ᄐᄁ１￿ࠀð￿ӝđᄐā\0\0\0\x11\x0f࿿\0ༀ￿ࠀ￿ࠀကđᄐ\0￿ࠀÿ￿ࠀ࿿\x1f࿿တᄀ！ÿ῿\x11ᄑༀ\0ﻮ࿿࿰뀀ﾽÿ࿠⏿™ጢðđᄀᄑᄑထĀ\x10\0\0\0\0ༀ\x0fᄀ\x11\0\0༏ÿ\x10က０￿ࠀ࿿\0ᄀༀ￿ࠀ\0࿰ᄀ\x01đĀᄐሒሑ༂ကᄀᄑȢ\0\0￿ࠀÿ\0༏\0ð\0／\x0f\x10＀ༀ࿰ÿ＀￿ࠀ\x11\0￿۠﻿ð￰\0＀￰\0ᄀ！࿿\0\x10Ģༀᄑ\0࿿\0ÿ￿ࠀሡထ﻾ÿ퀀ﻞ￰\x0fðᄀāĐ\0\0\0\x0f\0\0ᄀ\x01က\x11ᄀ\0\0ᄐሢ\x12\0က\x11Ā\0\0ᄐကﻮ／࿿￿ࠀℾ䈓ᄲᄂㄑ攴㐥ġ−￢༏⃲ထሂဒჱᄑ!￱\0\0￰췽￿Ϟ￰༏￿ۀ￿ϬპĀđ０\0￰ÿༀ￿܏໠￾￿ࠀ츏Čတ쬑첻ﻰ\x0e컽ﻮἑༀ㿿䈴ሒ\x10ᄂ￿נ￿ࠀÿðᇿﻠ췬Ďᄐ∑−Ē༟Āď࿱ဏǰ\x10Āᄑ뷭￿Ӎሜ℡\"ḁ⃐㈓ሃ췭།ჰ０\x0f༁Ā㈲㌳ἃメ㈰⌓∡㍃ℕ∢∑／ℎȑ\x11\x01\0ᄢᄑ㍀ጴﳐ\x11￿۝￿ۭ￿Ӟ￿ל啖≆鮢￿Ӝ㇞ሑሢ\x11࿱ᄐ㈢￲࿯\0໠ခﴏ㿐？￮⻾∢ሢሁဢᄐ￿ߍ￿מß\x1f￿ά뻜￿۽ዯđ\x11\0\0ﻮထ㈀㌳ბ\0\0髀￿׎࿜￠࿾﷮တ∑ሒᄑ묑붫ﻮ︀￯\x10ᄀ췐뷌࿬⼏２ȡﻭᇯဟᄐ￿ԡ쳎໿ᄑ∐￱ᄑ￿ࠀÿ໰\x0fကï\0࿰က༐￰℡ᄒᇞᄡ．ḑ￲ﻮ췿￝＀\x0e︀༏∀ᄒᄡᄀᄑ퀏﷾໮㏿㐲곮림￰￿ࠀ\x1fᄢ\x11／ﻰრÿ࿰＀￿ࠀༀ\x02ñᄁᄁတ￿ۮᄍᄏᄑᄑ\0༑ǰ\x0fჰ℁⌡ġ\0￰￿ࠀ￯ğအﴑ•ἒ㌢Ự￿߿ᄝî῰ᄁ\0￞῾ď࿾࿰Ē\0ᄀ\x10∀࿱￿׭㈐﷝࿾︎⏯ဂ턢￾\0ᄀ࿱\x0fᄐℑ￿ࠀ￿ࠀ궻뮚ċ࿿࿡࿮\0ﻰᄑᄁ\0탭ᇮñ퀐Þ\x10ᄀﻡǾခÿ\0\0＀ᄀထ￿ˏ￿μ￿۾\0\0ᄐထᄑ\x01쀀쳬﷝ထᄁ￱\0ᄀᇰᄑᄀတᄑกǡ\x1eခ￿ࠀထᄁ\0\x10℁ᄒℑ∣−￿־뷛ဍက＀￾￿܏þༀက＀ï＀ჰᄞᄑ￾⿿−ᄑĐᄑ＀ﻰ࿿ð\0\0༏￾`ခᄀ∢∢\x01࿰㻿䌳㑃￿ӯාþ༐￿ࠀ\x01Ā＀\0\0／࿰ༀð￿ࠀÿ\0ᇰᄁ￿ࠀ\0\0ༀ￿ࠀ￿ۿ샽ﳰᄑᄑ໱ᄎ\x10퀑￮\x0fᄑᄑ￡￿ࠀ＀฀탽ﷰ￿ࠀ∯™ᄢĀ￯￿۫︀ᄎ\x10￿ԑ￿םⷮᄢሡ￿ם෽-\0\x10\0Ā࿰￿ࠀ￿ࠀ￯￿ࠀỿᄑᄑĐ\x01໰탯࿽ð￾ℐ™∢𣏕￿ޭ￼﻾￿ࠀ．ñ￿ࠀ໿ÿༀ\0\0Đထ༁￿ࠀ㌏∢ሢᄒᄑ\0\0ﻰჽ⼀™ሑༀ\0ༀ／\x0fက\x01\0﻿￿ׯ￿ם⋜ᄑሡ∂ሡ㍃㌣￿ף￿Ӎ㏋∣ ㌣∲∢∢＀퀀￿מ㌳⌱췍㏋⌲̳\x10က㐳㌳ဃ\0\x10ď︀퇽봍\r\0ᄀ༑῰ᄢ∑Đ̑ဢ！￾ᷮሒᄠ࿿＀￿׮￿Ӎᄡᄒ\x01ℏሑ턢￞ሡሢ첲㏞㌳ጳᄁᄑ﷠쳬췬ﲾ￿ۭ\0ᄐᄑሑᄒ −∡￿ӌᄞℑ䐑㐳ፄ∑ᄒᄑᄑ∡䌳ℳሑᄡᄒᄑ\x11ᄁ￿ࠀÿﷰ￿߮\x11\0믏쮼º฀ခ༑\0ðǮ࿰™ሑ￡￮췝෍ༀ￿ࠀሠ™︒࿾ÿ\0\0ᄡᄑᄑሡ￿׭್\0\0∢ሑðᄏāđ\0\0￿ࠀဟခထ￱ﻮ࿿\0\0馠￿ל￿ם್！༏ℑᄑᄑᄑฑ컞￿ࠀ￮￿ࠀ㑏呕V\x0f‏㈳㌣တထ໯ÿ\x10ခሒℒ῿\0ᄐ\0＀￿ࠀ࿰ᄑတℑȑ︐￯࿮＀￿ࠀ࿰\0ခ\x10ᄀᄑā\x01\0\x0f\x10\0ჰခ\x0fကĀ∮⌢ᄲℒ℀㈢㌳\0\x10ﻮ㋿⌢팳￿۝￿ࠀ￿ࠀ￿ࠀ\0༏ ᄢᄒ\0တ㌱ሲＢ࿰࿮ᄀᄑđထℑᄡﻮ΅ᄑᄒ￿ࠀ￰༏ᄀ\x0f퀀췍￿׌￿ࠀ／￿ן﷮ხᄁ ⌢⌢\x10\0ᄐā뿮벫쳋￿ࠀ￿ࠀဏĀ\0\0\x11ༀ\x12￿ࠀﻞ￾ÿ\0ჰᄑᄁ㌳㍃⌴䍃⅄−ᄑဒ\0໿࿰ᄏᄑᄑᄑᄑᄑℑ∢ᄢ췑￿׭îĀﻮ࿿변￿ӌ࿍ð⼏㈢⌲／࿰￿ࠀ￿ࠀď￱࿿\0\0＀\x0fကᄀခ\0တခ\0\0릠첛࿚\0ခ\0࿿\0￿۠믞못ફðᄐ￿׮໽⃠\x11\0\0\0\0ထခ\0\0￿ׯở∑䑂￿۟컋∮đ＀￿ࠀ￿߿￿׌�\0\0\0\0\0\0ကᄀሑᄑ￲￾￿ࠀ￿ࠀ췞ကℑ￲￿ࠀ￰\0\0\0\0\0\0￿ࠀ࿾￿ࠀ῿ᄑᄑᄑᄑ\x01\0ༀ⻿㌢㌳￿ࠀ￿ࠀᄟᄡ\x11\0＀￿ࠀ\0\0ᄠ−\"\0࿿ð￰￿ࠀǿ༁￿ࠀ￿ࠀ\0\0\0￿ࠀ\x0f\0\0\0\0ᄑထἁჰᄀĐ\x10ᄀᄑ\x01\0ᄀ\0༏\0\0￿ࠀ￿ࠀğᄑ\x11＀༁\0\0컛﻿ᄞᄑ!\0\0\0ᄀꦞꪪª\0࿿＀￿ࠀÿᇮ\x01໯＀￿ࠀ￿߿췝࿽ჯ༏￰\x0fကᄐ\0\0\0༏ༀ￠࿮\0\x10￿߬ÿ\x0fༀð\0\0\0đခ￱￿ࠀᇿሡȢ\0\0\0＀რ\x10\0￿ۿ￰\x0f\0\0\0\0\0\0\x0f\0ሀ\0\x0f࿰￰\x1e෿რį‒༒ကðᄁᄐ−ሑĂ\x11＀࿿࿿ჰ\0\x1eჰ໰༑ჱሑᄐჱሏĐ￞\x0fď\x10ﻠ࿿࿯￰\x0f️ÿĐᄑ吁䑅툢Þğἁÿ￰῰ð쀀髋쪪￿߾෯ჿခﻯ࿰ǿᄀď࿰đᄐ￿ӭ췝 ထ\x01\0／\x0f\0ကᄀ\x01ᄢᄒᄒ฀ð⌠∢Ｂ턡î∡῿ᄐ１῰\x01￾῰ᄑခ＀ჿ\x01턑\rĀ\0ᄟᄁ�￿ࠀ￰ᄟჾ\x01ᄁ\x11\x01ခᄀჱďĀĢ\0\0￿׿∮ሢ￿ࠀ\x0fġဒ뷐￿םÌ케벻￿۪￿ࠀ\x0fᄀ\x01\0\0\0෰ǯᄏက＀þ࿮à\x0e\0\0࿰￯ᇽĐ￯တᄑđĐ࿿\x01\0\x01ༀ＀࿿ÿȑༀ￰￿ࠀîᄡ−‑２ဏထሓℑ̲㼳＃ỿတᄁﻰ￿מ￿Ӝ￿ם￾\0ð﷟ሬᄑ㐑ㅄᙆℒđ￿ҽ￿׌ﻝჯ㼀™ᄓﻮ￯﻾ᇯᄑ\0\x01Āကထ\x01Ā０\x0fᄀ\x11ﻱĐထ•＂￿߽໿က\0㐂㝍\x07＀༏\0ဏᄒ\0⌯∲Ｃﻮ￿۞￿ם￿۝￿ۮǮ￿۰ﻞ￿ׯ\x01\0﷑ﻮ？﷭￿ࠀထ∑️췠⻭ሱ䈲﻿ď\x10∁伤┓ἐ췯ማ∲䐣∤췮￿׭ᄡℒ࿱㈏ሡሢᄑ\x01ȑ＀￿ۮἁᇯᄑᄁကᄑ࿰￿ࠀ㏿̰ℴ−∢ᄐᄑ￿ӑ￿׍Ý࿱\x0f\0\0ᄟᄑ㈁∢￮\0\0䍀䑄ᄴሑ￿۝ᄐᄑ￿ׯ￿߬ỿđ༁࿿࿿쳏쮻⊼ᄒ䄐䔴╓Đደℑ∂!ğᄲ−ጡᄀ\x10Ēထ１／￿׭῞`ญᄁᄐ\0＠ჰ∁￿۱￿׮⇍䈣༏￞ﻝ￿׮￿۾﷮￿ࠀ㌯℣༏໯\0þဏ\x10໱\x0f켁Ǯ∑￿߾￿ӝ￿׾﷮΅\x01࿠῿ď࿰\0⼀ㄓȐ\0တ￿۾ﻮ࿠⿠\x10ခ︟࿰\x10\0⼂⌢ሡ췝￿ӝ⌭㈳Ⅎሱ戳圴啤ဟሀ￰ÿ℀∑῾ሀ༢ð넑￿Ϝ￿ֽဂༀ\0￿Ѐරǰď刣䐴쯓뮼Ë࿱쳌\x0f﷞࿮တ￿۞췍㍃㌤䐲⌳3Ā￞／Đἑ첑볋￿ӌ쯞춮￿Ӟ￿ֻဟℂᄡ\x11ሑ∑Ȥ\0\0ༀ￰࿰\x11ἀဠā\0\0ㄠጢ￰ဏሑ∠\x01ðሐᄢ쬑￿ύ췋�ﻮᴞ￮ༀ￠࿿ᇰ￿א￿׍ᇌᄁ⼀㌳∣\0？ℑ／￿ࠀ￿߿￿ۭﻮ∢ᄒ￰Ā︀﻽࿠￯￭￿ࠀïᄐ컝ℑሢༀჱᄑïğᄡℒ䕀啕ｅᄏĀℑā\0ðﻯ￿߮ᇮᄑ‑ᄒȟĞ℡Ｂ￾῿−ℒðဟሑ１ﻭ࿿ðĐ\x11︀ð￰࿰ðᄑℒ࿿￾뿿뮻첻∱㈣䑒吶﹃ỿℑ\0ÿሐሢ뿾벫쳋\0ကĀ\x10⇱∡‑㈣∢쪺첽ĉᄡ２ÿ뼏쳌￿׌ᄐဒ༏￰ἐ™ᄒ￝ሯᄢ෮ᄐᄑ∢ᄢ啠琷ၕᄀÿ\0￿۽쯎뮻￾ฏက⌳ᄀĐༀĀ\x10\0\0ሡᄢ￱⇿ሑᄲ∑đ\0\0໠궚ëᄐကℑĒ￾￿׎쳌￿߭츏쯌쯌ᄑ\0쳀첼჌\x01\0ᄐĠᄑᄑ쳁￿μ⏋∢ခ㈳∣တ∑\0ༀ￰￿߰￿׮ᄐĐ\x01ჰĀ\x11\x10\0∢∢࿱တ￾༏￿܎\x0eတ\x11＀\0ᄁᄐတᄑ∡ሒ༐ⷮ−ᄠჿሀ࿿￿؀ﳞﻮᄑᄁᄀĐᄀᄑᄑ퀏ἀ\x11\0￿א￿߿ჭĐ\x11\x10ᄐခᄁ໱﻿đဏᄑᄑ\x11\0ༀခ\0༏⋰∢ȡ∐ሡĐ\0‒ထĀ\x10\0\0တᄁℐᄡđ෠￿߭﻿đ\0￿ΰǭༀကĐ῱￿ࠀ࿿⏰ἁ￿߾﻾ထ∁￱￿ࠀ￿ࠀ￿ࠀ￿ۮ￿׮￿םሢ\x11\x01\0\x10ᄁᄑᄑᄡ™\x01\x10ᄀ࿿ᄀᄑᄡℑ\x11ðĀက\0\0\0컽\x0f\0㈀㐳Ģ＀ð\0\0린￿׬￿םໝ࿿ᄑᄑ\x01\0\0탾ἀ࿰쳀벻㊻∣／\x0fđᄀ￿םᇜĀༀ\0\0\x0fༀༀ࿿ÿ\0믝ꦫℑ−￱îðထ\x10\0῿ခ࿰￿ࠀ￿ࠀ\0\0\x10༁�￿ࠀÿ\0￿ࠀÿ／ð\0ᄑ໭\0\0￿ࠀ࿰ﻠ／￿ࠀᇯā\0ð\0∏ሒ／ā\0\0ÿ\x0f\0\x10ကᄑﻮÿð￿ࠀ￿ࠀ￿ࠀ\0\0\x10\0＀῿\x01Ā\0\0\0\0ᄀ\0\0\x10ᄑሑ￱ခ℁ᄒሡ\x01ᄐÿ／ჿ\0\x10\0\x0fĀༀ￿ࠀ￿ࠀᇿ\x01\x01ༀ￰Ā\x0f\0！￯࿿／࿰\0￰ﻮ￿ࠀ\0\0\0\0\0Āတ\x01\0￿ࠀ￰ᄑအ／／ﻰÿĀᄑᄁ\x01\0∀ሒġ\0\0\0Ā＀\x01ကခÿကᄁ\0\0ሐġ\x11ༀ࿿ᄁᄑ﻿￿ࠀᄟထ１￿ࠀ໮／￿ࠀÿ＀￰￿ࠀᇿᄑđ༑ﻠሮᄡℑᄑ㈢㌳㐳\0\0ༀ￰￿ࠀ῿ᄑᄑ￿ӝ￿۝﷮\0\0￰ﻝ∢ሢ￿׮࿯\0Đ™−䐱䑄໮ᄑ\0™đ￰￾\0\0࿿췬\0\0＀ༀᄀᄑᄑᄑᄑ࿰\0﷯￿۝￿ׯﳎ￿ࠀ￿ࠀ쮫붹ﻯ⋭∢️ﻐ∡ᄒ\x01\0￰￿ࠀ῿ᄑᄑ￿ࠀǿ∠∢︢￿ࠀ࿿༑￿ןﻯ￿ࠀꪯ骪ᆪᄑđ\0\0￿ࠀሯᄑ∁㌢팳췜￿םﻯምᄡᄑ℡␴∢ᄢĀ\x11\0က￮ﻮ\0\0ༀ\0ᄟđ￿װ￿םﳍ￰\x0f∲™∑∡\"＀￰﻿￿ۭ췝￿ל￿׮곜￿ӛᄑᄑሠ∢\x12\x01\0\0\0\x01Ā￰／ᇰ\x01⻿ሑᄡ\0\0\0￿ࠀᇿᄑℑ㈳⌳\0\0ᄐᄁᄑሡᄑᄑᄑ\x0f\0￰ðᄀᄑ䄐䑄㐴\x11\x01＀⋰ᄒ＀￯༏࿰￠￿׮\0￿Ϗꮫᆹᄑġ\0\0ༀ࿰／￿ࠀᇿሡđ\x01\0췝￼￿ࠀÿ￿ࠀ༏ð￿ࠀ￿ࠀ࿿\x0f︐࿿῾ᄑᄑ\0\0࿰࿿／￿ࠀ컯볝￿Ӝ￿ࠀ￿ࠀ\x0fᄏထ﻾\0\0샭⇽ℒሑᄑᄐᄡᄑᄑℑ︑\0　㈳∣\0\0ðᇿᄑᄑᄑāđ\0\0\0ᄐĀ‐㈣⌢\0\0￰ᇿᄑđ\0\0Đထ\0￰￿ࠀෑ\x01Ā﻿ᇯᄑđ\0\0㌳㌳\x03\0∀ᄑđ\0\0\0ༀ￿ࠀ￿ࠀ￿ࠀﻯﻝ￰￿ࠀÿ￰࿰ᄀᄑđ\0ༀ\0\0ᄐထ쬑￿ξ಻\0ᄀ￿ࠀ￿ࠀᄟᄑđ\x01\0\0\0\0\0ᄐđ１￰￿ࠀ￿ࠀ￿ࠀ\0\0￿א￿ࠀ￿ࠀ໮༐ ကကᄑᄑ￿ԑ￿םෝ\0\0﷭￿ࠀ￰￿ࠀᇿĐ￿؟ༀᄏတ︑￿ӏ㸏䌣䐳\0ğ༁￾￿ࠀ\0\0⼑㈣崕á！\x10ᄁ࿿\0࿰ď︠￾࿮ğ￿ࠀ￿׿෭／⻿ᏯᄎတĮ∢ዲĐȡġ\0\x01\0ჱĐ＀ထᄑ￿۱￰ᇿ￿Ԁ쮾῰ἏỠÿ໰ÿ\x10鶹쫋༎ǰ!\0촀ﻮﻮ￝￯ကᄁℑ∑ﻟሞሢ\"ữ￿܎\0ÿ\x0fġကᄑℑ࿰￿ࠀþ￿ۿ￿ۿ\x11￿߰࿮ᄢĀ턏�￿ۯ໯￿߮．￰\0\0࿿ÿ﻿࿿\x0f\0ༀ\0Ğđ묑ﻌï\0﷯Ğထđ1ȣðༀᄑᄑ￱￮࿮\x11\0\0\0＀\0ᄐ\x1fကđ\0\0ༀ\x0e\0\x10\0࿰\0ᄑ￿؎ᷯᄁᄁሑᄡሡ℣\x02\0＀ﻠ\rတ໿￿׾＀￿ࠀ\x0eᄁ１࿰࿰\x11ﻠ∢ဢ\0ἑỿ\0\x01ᄐᄑ༑Āᄀᄑᄑ༁￟\x0cထ℀ᷭᄁĀﳛǮကîᄐ붼骻ﻮ￿ࠀ﻾㏰㌳̢ᄀခĀတ￡￿ࠀ\0\0\0\x01ð\0ကༀတ໿\0ĀﻰĎ\x10！￰\x10ᄁᄝᄑ\x11\0퀀￿ם￿۝\0\x11￮෭ᄒ\x01\0\0ð︀￿ۭā\x10ﻝ＀⿿㈢ሒ＀\0࿰쫿￿ߋ⼀㈑㑂￿Ӯᄜᄑ∑đ\x11\x10Āထခက\0∀∢ᄢℑᄒ쳍쯍￿Ϯ෋ༀðᄑ−﻾ᇯሠᄢℒ∡\x10ᄑ춱￿ϋÝ￱࿿\0\0ﻮ\x0fĐ￮￮\0\0철￿۝ￜ￾\x10\x01࿮ጯ≃ሢᄑð\0￠࿿ᄀᄑ￿߽đ༁ሟ∡ဢᄐℐ∑ᄒℯ࿠．￰／\0℡Ē＀ခ\x11＀࿯￿ם쳞￿מ﷠￿߽௠ᇐሡᄢ￿ܑฏᄀᄒᄁ⃱တሒ￿ؐ쳭ⷞሑ０໿\x01\0\0Ā０仞啄啕ᄑᄑ\0\0ᄑđ．࿰ᄑᄑℐ－￿ࠀ࿿ༀ￰ჯ\x10ﻱ췞࿯რሡሢ￿ؑ￿׭﷞／࿰￯￿߾þ\0\0ἏሀĀ࿿ᇾᄑကထ\x01﻾࿿️탽ဟ귉⌿㌳￿ܢഀༀð﻾귾￿׾໯ðÿ\0＀࿯෱෍ခðᄑℑ￡﻿ෑ࿭\0ᄐℑ∢ሒᄡ‑−∠∢∢ᄑᄒฑÑﴏÿÿ\0\0Đ\x11￱ÿᄟ￮༏\0\0ﻰჿᄑᄑ࿿＀\x0f\0ฎᇱ\0￰췭î࿰俿䐴⌣／￠췝⃎ሁㇰ卅⌣\x0f\0춠￾￿ࠀ쿿￿׍ฏ￿ӎ췍ᇜᄑᄒ! ðᄐȐဠᄐᇰﻠ퇽ﰟ\0ð༏࿰﻾အð\x10⿰ሒᄡᄑ\x11༁\0︀￮￞\0／\0\0\0￰࿿῰﻾￾￿ۯ•ᄁᄡᄑĀ\x10Đ\0ᄠ㈒￿̲⿿ሢထ￿ࠀ﷞ᇮᄑĒđထᄁĐༀᄀᄑ㄁㌳㌳ကðༀðĐထ\0\x0f쳾뮼￾ā￿۟î\0ᄀᄑᄑ㈳㌢\x02࿰ᇿဂ￯\x0f￰￰ዾሠ￿ࠀ￿ۮ￿ӭฐ໿ÿ\0Ā\0\0ᄏᄑ찑￿Ӝᷝᄀᄠ￾࿿\0\x0fď\x11\0\0\0\x0f\0ခ\0࿰ῐ\0\0췐￿ם﷍࿰àᄑĐခ\0餀馪஼ᄑ℡࿰＀ἀǟᄟᄑတሑᄐထჱခᄀᄐᄑ−∡캼컪\x10＀￿ࠀ￿߿￿۝﻾ༀ\0＀࿰ᇿĠ\x12\x11ñἀᄀ\x01\0ༀĞༀð／ᄐℑ﻿î\x0f￯﻿￰ᄟ™．῰ℂāထᄁ\x10࿰＀\0\0￿ࠀ￿ࠀ￿ۮ￾￰ᇿᄑđðĀ\0ༀ／ༀ⋰ሠ\"\0\0\0ခĐ\x01\0ᄐð／࿰￿ۮ￭\0က−ሡထᄑ⇿ȑထ∁ሑༀက\x10Ā\x10 −ሑἀကἑခᄑ࿰／￿ۯ\x0cခ!\x0fကᄁᄑἐခﳑￎသĐ\x10ᄀĐ￿מ\rÿ￿ࠀðᄐᄑ￿ۍí\0\x0f\0࿰ÿ\x10ᄑ／ữᄑ\0ñ\0ᄀđ\x01\0\0ထᄁ\0\0ᄀᄑ\x11ᄀĀ\0\0ﺝ༊࿿ᄑᄑ࿿／ᄐᄑ￿ԑ쳜෌ð\0࿰￰⇮ሑđ\x10\x01Ü\0࿰\0\x10ﻰ￬￿ࠀ࿿\x0f\x10㑄䌳￿ӣ￿ҽ⋛∢Ā\0\0ᄁᄑ∡ሑሢထđ\x0f￿ߝￜ￿ࠀჾ࿱￿ࠀ＀￿ࠀ️￝ď？￿۰⺽䅆Ă볍벪\x0bðကᄡᄡ￱￿ࠀᇿᄑ￰￿ࠀ￿ࠀĞ\x10∏∢đ\0\0￿ࠀ￯쯞ﻞ￿ۯ？￿ࠀ￾ᇾᄑℑ∢ᄒ￿ࠀþ⌠∲㈢㌣ሢ\x11ᄁ\0ༀ䐰ጢℳ−ᄐℒᄑ\0ༀ﷟￿׮￿ۭﻮ￿߿\x0fἁ˰￿ׯﳍჭﻡ໮ဒ￮\x1f\0℀\x01༏￰￿ࠀ￿ࠀໞ＀࿿ðᇾ⌠ሲ２࿯\0\0／\x0f\0鰀쫋ඬĀ\x01ᄑᄑﻮᇮ\x11\0\0\0đ\x0f\x01\0＀ởĂĐ\0\0∠∡￿Բ쳜￿ࠀက\0ሐᄡ밑믋￿׍က\x11⋰ñ쿭믾\x01㓿−ᄱ\x13ď￿߿볬￿μﻠ．ოကĐခĀ䑃㍅￣췌ﻜ﷟쿮쳍￯ďġထĀတༀ￿ό쳌ሬᄏሡ㄀ሲℒ\x01ༀჾ﻽\x1e\x01￰㈀\x02鄑ﶫđༀ️ᇰ및￮￿߿ﻭ࿰￿ࠀ\x01\0ᄀ‐™ᄑ\x11＞\0￰ሐ࿮\x0f\0＀℀ༀ࿿￿ࠀĀထᑅ㈑䑂췮\x0eġ\x01࿠\x0e\x01ᄀﻮ￿ࠀᇿ\x11Ģ∁ሢ턟￿ןî\0￿׿ﻮȢ༑ðᄠ̴ခĀჿĐ￿ࠀğ\x01！￯ﻞ쳾－໭က༏ἁ∯ᄢ༏࿰℀\x01ἐ‒\x01뫾￿ߛ།\0\0໿\x0f￰ﻟ໛\0\0ቒ３ﷀ￭ༀ\0ᄠð∿ሲ\x01\0\0\0骟쪚࿌࿰ჿđᄁ\x01ÿༀ࿰\x01\0∡∢ሢℒ༢￿ࠀð＀￰ကᄑሑထ￱ỿတᄁ￱ﻯî￿ࠀ\0￿ࠀ\x0f\x11\x0e\x10ሀᄡሒᄁሡ࿮ǰĐℐđᄑ\x11\0﻿ÿ﻿࿿ᇰሒﻯ࿿￿ࠀ࿿ᄀ∢\"\0\x0f ᄑᄑሑထ\x12໿\0\0࿿\x0f\x01࿰ᇰā\0༁࿮∯∢Ｂ\x0f\x0fခᇿༀ￿؁đĐ໿က࿱￮＀ကᄀđ\0︀ༀďℑ￿ࠀᄏᄑđༀჿༀđကℑđ￿ࠀ￿ۮᄑ∑\x02Āᄐᄟሢℑᄁሑ࿿￿ࠀ࿿ᄀ∑ကခ￰ჿ퀒￿׮￿Ӎ༏\0Ǯñ\x0f\0\0￿ࠀ࿿\x0f\0℀ᄑ￰\0\0㐰㍃ᄴᄑđတᄑ￡￿ࠀ￯∢∢＂㿿#ကÿ༐￾\x0f￰࿯￿מﳜ ￡ð￿߾ሯ∣￰￿ࠀ컾볜쯍ጫℳ＃￿ࠀỰ０﻿ᄁᄑ⌱㌳㻽䐳㑃쳾／ჰĀđༀ\x11༁￿߽㋿␱Ĳ໱ﻮ﻿ﻮ䏿啃уǯ\x10￿߮￿ӽĞ\x11\0Āﻰā㐰䑔Ｕ￿ࠀỿᄀ\x01\0ခ￿ࠀ￿Џ￿Ӎﲺ࿿\0༁ᄐ\0ÿ\0ကခ䈰珢ｓ\x01숀쯍\x01Ā췀쳛Ý\x01￰￯࿿ᄀ쳿ꪜ஺ᄁā̳０￰￿߾ᇮā\x10\x10\x01﻿ᄞሡᄑ\0ကᄑᄑ\x01ď䈀⌲㌴吣㐴￿׿Ἤ懂ေĀĐð\0∲㈣ﻣ㋿ጡတ춼곚\x0eĀ\x01࿰\0\0\x10\x11\0\0ဏሐ￿׎\0\0￿Ӡￎ⇾ᄑ￿ӎ\0ထ￡￿ࠀዿᄑ࿰\0ကᄑ쪱⃿ᄳℒ㄀䑃䌵Đ࿿ᇿ∢ሒđ\0췝￿׌ምđ＀ð￿؏\0￰ᄏᄟ!\0쀀ﾛﻮ\x0f\x10１\x0fđကတ࿿ἀ＀￿ۯ෮￰￿ۿ￿׌Ĝༀ＀ÿ 㐢ᄓ༏\0\0\0\x0fတ∡⌢đ\x01ﻠ⃮℁ሐ\x11ᄐ㌮⌲\"ð\x0fﻯ㻮™ሡ￰\x1d\0︀\0Āﻯဏ࿱ﻟ࿿က／￿ࠀ￿ࠀ￿ࠀ㈯㌢3ෞ＀￿ࠀ￯ﻮ￿ࠀ໮Ġ⌡㌢\x03\0\x10ထᄁ\x10က￿ࠀ￯\x1eༀ￯／࿿\0တ䑄呄￤ሀᄑđ\x0f\x10ሑ∡￲﻿￯\x01ᇰℑሒᄑᄑ\x11໿ð\0\0\0\0ᄒ\x11\x01\0℀∢ሳတā\0\0\x10ᷞ\0￰῰ᄑđ﻿ﻭ￿ۮ쳭￿ࠀ࿿ÿᄐ\x11ᄐ턑鳋릻ༀð⌠㍂ｄỞ㌑㌢￿ࠀ࿿℡\x01࿿ð\x0fᄑ\x11\x10ᄑ㈒Ｓ﻾࿯࿿￿׮ﻮ￿ۯ￿ӏﻝ．ﻱ︀ÿༀ༏ð\0ᄟ⃱０ခ㻮ᏰĿ﻾ༀ\x0f\0￿ࠀ⃰㌢㌳\x10\x11\0တ\x11\x10￿ࠀℯȁ？໮\0￰ð\x1f￿ۮ࿰࿯ﻡǱğ\0＀ﻯ\0က￟￿߽࿾\0０￰／￿ࠀ￿ׯ쳝ǌ\x10āᄑℑ．ﻮ࿰တ췚﻾∰∣ﴀ࿠／￿ࠀ￿ࠀ곿Ēð\x10￿ࠀ￿ࠀﻯþđ࿿ༀ￿ࠀᄁ\0ﻠ￿ࠀ﻿ﷁí\0\x10တခ\0\x11　䐲ጳᄑℑᄑℑขá︎\x0f\0\0\0ᄑ\x01￿ࠀ\x0f／＀\0\0༐ᄁᄢ\x01\0\x0f\0＀\x0fက￿܀﷮რᄑℑက\x01ꦠᇮᄒℑᄢᄒሢᄑñ\x0fက\x01ἀđ\0\x10ထ￰࿿ÿ\0ﻮ￿ם⋞ᄒđ\0\0࿿\0\0＀￿ࠀ\x0f￰￿ࠀ\0\0￿ࠀ￿ࠀÿ࿰\0\0\0\0\0￰￿ࠀ﻿￾⿿ᄢᄑ\x10\x10￿ࠀ￿ࠀ￯￾\x1fﻯ￾￿ࠀᇿᄒĀ\0\0∀ᄑđ\0\x10ᄑᄐđက\0\0ဏᄑ\x01\0\0\0\0￿ࠀ⿰ᄢᄒᄁᄐ￿ۮ⋮ᄒᄢᄑᄑᄁᄀﻯ⋿∢ℑ∢ᄒ\0\0တ\0＀﻿￾￿ࠀ￿ࠀ￰ďĐᄁᄑᄑᄑᄡđ\x10ᇾ\x01đ\0\0က࿰￿ࠀ﻿࿿\0\0\0￿۟￿ۭᇞᄑđ༏ᄑᄑᄑᄑ￿۝﷾￿ࠀ／￿ם\x1e\0ᄀᄑđ\0\0\x01\0\0\0ᄀ\x11\0\0\0\0\0ᄐ∢\"Ā\x0f＀／\0\0∠ሡ￿ԑ΅đᄑ\x01ᄁ໰ᇠ\x10ခ￮ᄢĐđခ\0￿ࠀ\0\0\0ဏ０￰\0￰Ā０ထ１༏０＀࿿ğ\0က\0῿ခ\0໰￿ߞ\x1bက\0￰᳝ᄑ\x10\0＀\x0eᇾᄐ넑볛몽￿ࠀﻮᇿ∑￰﻾￿ࠀÿᄐᄑ\x11ကကĀ\0\x10\0\0က༑༏\0ᄐ￿ࠀ￿ׯČတ＀ðခĀÿༀကᄁ\x0f࿰∢∢ ⋡＠࿿쮾ༀᄀ\x01￾ÿ࿿￰\x01\0ﻯﻮﻰ￿ࠀȢ︐ﻮ￿ׯ﯌￿ࠀ\x0f\0⌀ĠȢÿ￰∳ℒ࿲ÿᄐĐကᄑᄑ￿ۮ￿׭Ỿǯොခ\x01ᄑᄑ\x01ᄀᄐﻯ\x0e￾￰￿߮Ýð࿿\0\0\x0e\0\0￿ࠀỿ￿ࠀ\0\0즠춬࿉࿰Ā￿ԀᄑᄑခĀ℀ሡđ\0￰\0\0῰１Ỿထᄑᄑ༑࿿￿ࠀﻰ\0\0\0Ā\0뻾ￍ\x0fᄟ￿ࠀ\x0f\x01ñ\0Ỿ\0ᄑđ\x01\0ᄁ\x01\0\0\0Ā\0\0￿Ӱ￾ð\0ðༀ\0\0ᄀထခᄁ∑Đ\x10࿿ď\0\0＀\x0f\0\x0f︀᷽\0\x01\x1d\0\0\0ᷮ\0\0\0\x01࿰ÿ\0\0\0\0࿾\0\0\0\x11đ\0Đ\0\0\0ကđሠ\x10ᄀᄑ￿׭짯\x1b\0ᄀĐ\x11\0\x10࿰️༎࿯￾đᄁက\x01\0\0￿ࠀ࿿ÿᄐໝᄀ\x01\x0fက\0Āတ\0\0Ā\0\0ᄐ༏\0￰࿿\0ἀᄁ\x01\0\0đ༁ဏĀༀð\x10\0࿰\0\0\0\0ခ\0\0\0\0\0\0＀ð￰࿿＀\x0f\0\0࿿\x0e\0＀ဏĐ\x01\0\0＀￿ࠀ\0\0\0ᄑᄑﻱĎᄑ냿ꪼ骹\x02ð∐∲#ᄐ￯࿾￿ۮ췝\x0c￰￿ࠀ\x0f\x0f\0ᄐᄑᄑ￱￿מǍ／￿ۯﾜሞ∢⻮∢㈢﷯ĝထ\x01ᄀᄑ༏∡ဳ￿ג￿Ӝ￿מ￿׌춫\0ð\x0f\0ờᄑᄑ\x10ༀ﷠ﻯÿ\x01Đထ\x01ȡ￿ࠀî／໠＀￿ࠀ\0\x10Ỡ\x0f\0ᄑᄢ\x11þ\0\x0f῰ᄐထ༁￿܎￾￿ۮᄡ\x0fက！\0ฏā\x0f\x10\0ďđကᄑ‐￮ÿ￿ۮ￿۬쳌￿׌￿׮￿ߝ\x01\x01\0\0࿾﻿ð\0࿰ᄐဢ࿱༐ð\x0fᄑᄑထā￿׿ෞကℑ\0࿰￿ࠀ࿿\0\0\0\0\0ᄀ∑đ\0\0\0\0\0\x11\0￱ໝ℁ሑĐ\x11ခ㌎㑃̓ဏ\0ð㌰∳￯￾쳾\r︀ᄀ\x1f\x10ᄁ໿ð\0༏＀붪￿Ӝ㕝䍄Ｔ࿿０∢−ﻲ￿۞￿ם໠츍ᴀǁᄜᄑ쀒ﻰကᄒᄑᄏℑ⿱ǯᄀğ\x10／࿯ǿﻮ￿מ￿׍ᄑᄑ∡ሒﻮ࿿∡㌲Đထ즑￿ל￯῿ထĀሂ퀍ﻰ￯￿Ӿ﻿．࿰ሑ∡ሒሡ⌢∢벬곉￿׮￿ӎ﷋컽￿ώထ\0￿׮￿׭￿ם∣∢῰༁\x01\x10Ȣ\0￿א틯఑䇱ȡ‡㌾㌳ﷰﻝᄑℒ￱￿ԏ࿯￯ᄐ￱￾컿ฏሒ\x11\0ᄑᄑ￿ۮ目㕤㍅㌳㐳ᄑᄑခᄐđ\0\0\0\0ÿက∡ᄑ밑￮￿߰?ᄒ\0\0몠ﻭ￾￿ࠀ℡∢ᄢᄑዮᄡㄢሢᄢༀ\0몠쪙ᇜᄑᄑᄑᄑĀ∐ဒ\0ᄀ\x01က\x11\0ᄑᄑ℡ℑༀ\0／ᄀ\x11ᄑ\0ခᄐᄑ\x10က໯\0￠￿ࠀ࿿\0ᄐ\0￿ࠀ\x0f\0℀\x01࿿\0Đ￾\0￿ࠀ\0＀῿ᄑᄑ࿰\0￿ࠀ￰ﻰဏᄁᄑ\0\0￠￾ᇿᄑđ＀࿿āᄐᄑᄑᄑᄑ\x11\0\x10ð￰\0ᄐᄁđ\x01Đ￮ᄐᄑ￿ؑ࿿\0\0ༀ￿ࠀ￮ᇿ\x01\0ᄑᄑᄑ￿סﻞᄏᄑ\0\0࿰ÿሐĐ＀ﻠ༏￰ᄏᄑđ࿱ÿ＀￿ࠀᄡ\x11쯀ﻮᇰ࿰ᄀᄑ༁ð\0\0Ȓ\0\0ሢđ￱￮ÿ\0\0ĀĀᄀᄑ￱\0￰퀑쳞\0\0\0￿ࠀǰ\0Ā\0\0\x0f\0\x01㈀∢đ\0က\0\0啐啕ᅕᄑᄑ０\x0fༀĀ\x01\0︀ﻯ￿ࠀ￿ࠀ\0∢㈣䑂㑔༳ď뀏￿Ҫ￿ԏምᄡ췮￿ֻ￿ӝᄑℒ\x01℡Ｂ￿ࠀ￿ࠀჰ∡ဣ⌱〲￿۝ᷮ\x11ᄡᄒāĐ༁ď‏㈓∢\x0f໱\r༁⇰⌑\x10ခ\0ℑဢ췀췜⇜ሠ숢뷋ꦽ໮㈀㌣숡ￌ￾༏\x01￰\0\0ᄐ︀췏\x10༁အ￿۬໮ခđ໾︁ÿ￾࿿ď／ကð！ሟᄡ\x12ď￮￯ကᄑ￿߮￿ӎ곍￿ֻ￿Ӎᄑᄑ￰༏￿ࠀ￮￿ࠀᄑᄒ￿Ӂ￭ᷝ㈁Ă⌲ᄢᇰ∢ ∑ሡ￿ۯꯝĊĐ⌑Ē￿ࠀ￯ᄑᄑ￰䐏⌣ጳℑሒ쮼쳝￼ﻮໝð\0㈲䌳￿ࠀ㏿⌲￿ࠀ\x0f\0￿܏ხ\0༏\0\0ᄎđ￿ԁﻮ\0\0\0ༀ￰￿׮ᄑᄒ￱㋿␱∲∢Ē࿿ðꪯ鷉⿌Ā᷑ȏȯℒሡᄡ\x13\x11ἀ\x0fခᄑဒＡ￿ࠀ￿׬ḁထ༒ᄀ࿰ကἐᄒᄎĀĐ࿰￿ߞ`ï￯࿿＀ᄠ\x10Ǡ༎ᄁ༁໾࿯῱￿׬Ǿðÿ︀￯Ā\0ᇱထခ℡࿿ð\x0fÿð\0︎à࿿ÿ\0༏\x0f‒ℑᄐဒሁġ\x12\x10ἑᄁđℑ￱ĐထሁĠကĀﻞ￮࿿⇯ᄂ︀￠ȑᄡ￢\x1fༀ࿡þ⃢ሀ﷯ꪝꪪ໋þ༏ﻰ￟ÿကﻡ�ကሐ\x0f\0࿿\x0f＀࿰Ā࿯￯\x10 ￠／ᇰሒ턢볞ﻞ໽ἀᇱĐ\0\0\0༐￰￿ࠀ￰＀ჰ\0\0࿰āༀð\x0f\0ð༎\x01℀Đﻯﻭ࿰ÿခā쳟￿םÝ\0췾￿׭ᄑᄑ췐￿ӝ⋝∢ခ⌑™ༀᄀ￾￿ࠀ￿ࠀ࿿／ﻰȯထà\0ἀ༑￰ᄑᄁ￰￯￿ࠀ／\0탭ᄜᄑđᄐᄑℒሡ‑−㌲㐤︱෾฀￾￿ࠀ﷯ჯᄁᄑᄑᄑ\x01\0ကĐ℁⼑ထĀ첰췜Üༀ\x10\0ༀ\x01ﻠ￿۝ကᄀ⌱∲Ｂ￿ࠀ࿰／ð￿ࠀ￿ࠀ⋿ሒ￿؎෯\x01Ā\0က￿ࠀ￿ࠀ\0ᄀ࿠￮ˮกǱᄀᄑȒ＀ÿ\0ÿ\0\0က\x01￿ࠀ￿۾￿ࠀ⋿ဂ࿿＀￰အ∁࿱️໑ထ\0＀࿿\0ð\0\x01ℑℒ\0∀Đ⼀∢∢ᄑℒ༂\0ᄐ\x01\0\0\0￿ࠀ\x0f\0\0ðἏĀℑ\0\0￿ư－￿ࠀ໯０⼁ᄐထ￿ࠀ⏿⌲\x0f\0࿿∠∢ᄢᄑ탭﻿￿׍￿ם䑍啥뱖쫍￾︀℠−\"\0ဏℒ∡∂ጞ⌳⍔Ｃ￯￮￿ࠀ༏￿ܠ￿Ͼ໎တ￿ן￿۝ჭḁ티￿׭￿ם￿׍껙￿ώ䏽㑃㌲̐℡\x10\0￰￿ࠀ∏㈣￿ࠀ￾ሡℑခĐᄑᄁ섐볊￿Ӝ￿߮ᄯ／￿ࠀﻜ￰￿ࠀᄡᄠ࿲Ȑ￿ؑ᲼㌢㌤鷪￿߮⇮∡ሢᄑᄑሯ∢찢쳌ೌ꿾ﯮ￿ۮ￿׭￿ڮü\0ᄑᄑᄑ￿׿︎ῠ￿Ԑ￿ǎ಼켑Ḁﻝ￿םᇝฐǯ퀀တᇱ∠㌳퀳〠ሂᄀ∑￿ۭ￿Ϯ�＀￿ۿ࿯　⍃∣㄂ሠ−ᄡ⌒༢Ă༏\0￿ۮ￿מ￿ם㏝㑁ፃᄑᄑᄑᄑ￿ھ￪￿ࠀ࿿\0\0ᄑᄟñȀໟฟ\0\0ሐ࿰þᄐ￿߿ﳾ\x0fᄑခᄑꨑ몺ﲬ\0\0\0ﻯჾðဏㄤሒĠ\x01ሐᄡ\x12ༀðခᄐᄐā！࿿￿ࠀ\x0fሲဢǲᇰĐ\x11\x11\x01ᄑᄑ￱ዮᄡ࿿\x0fďထñ／ﻮ￿ࠀ\x0f￿ࠀ\x0f\0ᄀᄑခᄑခ\0\x01࿯ကခ﻿／￰ﻯ￮Ἇð／࿰\0￿ࠀ\0\0ჰᄑā\0တ캽ဌℑ\x11\x01ကထñ￿ࠀ￿ࠀ࿰ကထ\x01ᄁᄐ￿ࠀ﻿ᄟℑ‑ሑᄑ㈳㌳＀\0đကĀ\0\x10￿ࠀ\x1fðฏ\x12\0\0\0￮ᄁ∡ᄲဢ⇮∂￯\x0f\0ᄀ￰ကሐ￿ࠀﻯ\x11＀ᄏᄐ\x11\0\0ðᄡဂ࿯\x01퀀￿ۭ\0\0￠ᇰ\0đ\0\0￿ࠀ\0က\0\0￿ࠀ࿰￿ࠀ\0\0ሐᄑ\x01\0\0ༀ\x0fᄀĀā\0︀￯\x0fတᄑꦐꪙ㎪∣̳ကĐᄑ℁㐱㈲￿ࠀ࿿\0\0\0ᄐက\x10\0￿ׯ췜／﻿ᇯᄁᄒ\x01\0℡ሒ࿱￿ࠀ￿ࠀༀ\0\0＀\0﻿￿ࠀ\0\0\x0fðÿ\0\0＀\0࿿＀\x0f࿿࿿￮／࿰퀏￿ࠀ࿿㏿⌣￰￿ࠀāᄀ\x01\0ကᄑđ࿿\0\0\0\0࿿ÿ\x01࿿￰ðÿĐခ\0\x10က∢∲￿ࠀ￿ࠀሟᄡᄒāᄡᄑᄑᄢ∑࿰㏿⌳ጳᄑሡ࿰ÿ￰￿ࠀÿ\0\0￿ࠀ￿ࠀሢထ＀\0\0\0ᄀ\0Đ\x10\0ĀĀ\0\0＀￿מ\x01ᄀ￰ﻯ⋿ᄑđ￰༏／﻿ༀ\x01\0\0࿿⌲™\x02ကᄁā ∢⌲đᄁ\x11ကᄀᄑđ\0\0\0\0\0\0︀￿ࠀ໮࿰\0\0￰þ＀\0ༀ\0Āሠℑໞ\0\0࿿￿ࠀ￿א￿םǭ\0、㍃⍃ℐđက\0ȀĠᄀᄁĀᄁᄑ﻾ㄡ⌒໡ᇾᄑဒထ￿ܐᄏℐ￱í℡﻾／／ﻡ￮ᇯခ༏ď\0က㌒ᄑ༐໱ༀ\0đ￠ༀ﷐￞Ď\x11࿯\x10\x01ሁᄑđĀတ많춚ᄋ∁!\0ကဢ\x01໯࿿ᄏဢဒᄁᄁ￿ۮ￿Ӯᄮᄒ−㑂㈳啄㑄﷭탯ဝᇿĐ\x11ကð￿ࠀ￾ဏñ￿ࠀဏ﷿ᄒ\0\0\0℀ðム∢㈲呖䌵￿ӓ췌⋮㈡툡쳍ďﻡ￾࿿ჰ﻿–ā∯ᄡ∢ሐÿ\x10Đᄑﻑ￿߭ﳀ￿ߞໞ￿ߞ\0ᄁ˿ထđ\0\0﻿\x0fက∑ᄢ￰￿ࠀ￿ࠀ\0\0∐ᄡ\x11࿰ဏĀ\0ᄀထ\x11ထ￿ؑ໯࿮￰Ⴍǰሏေ㈂ÿ἟ℒď℠ሡȑ῰퀍쿾ﳭﻯჰထ⃰Ǳᄏﻐ￿׿컬￿Ҿ／ﻠ볎ﳞ￿؜＀῰쳾ဝἯ췞ഀ탿﻿∐￾༎ﻟἎ㋰⌑．ထἀ㈐믮껋탰࿾℡ᄁ໱￿۟￿܏￿ןሾ￿܎ෟᄀက냿ῲ⸱ᇠᄑ　Ĳ䄑῿໰⼁ﰒ΅ሏ΅￿ࠀ㌯࿿ ჰ吃촏몾ᇞሒ쬀￿ۜ볜髚ปဏῢხﻟ黜ည탮쳞෭˰＀凱 ࿲໿࿠ᄁ\x02ἒà￿Ӯᇼ∡༟ﻰ￿߭￿܏໾∭탿쳻ໍ༑ðᇝ춼⋻༏뻻ထဂ￯￿ߌ࿫\0\0￰￾￮￰￿ࠀᄟ＀＀ᄑ\0\0쯀￿ڮ⇫òထ０！ðǯᄠ촀뻽෮ﳱ\0༁࿿\x0fᄀ널뮻볊﷍࿯ďထ\x01ἀሑᄡ＀\x0e\0ᄁ\x10ကᄒ\0ခĠᄑ﷍￯ﻯ￿۾￿׮čđ１ửᄑᄁ࿰￿ࠀ\x0f༏ðᄐᄀᄑᄡᄑ༁볿ꪬᦪðğᄢ\x11\x0fÿ＀￯Ữℑሑᄑᄑᄀđ＀\0ᄀ\x11￰￿۝\x0f\x0fﳍ￰࿿\0ﻰ￿מÜ\0쳾쳍ᄑ∑ò￿߾￿ࠀ￿ࠀ﻾＀㈢㈳\x02−츢￿ۼ췞￮\0￿ࠀÿᄁĀ࿮∯ሢ１＀\0ℑ⌡？￱࿿￾￿ࠀ￿ӌᄍď\0ðခᄑ!đ\x11￰￿ࠀ῿ᄁᄁሑᄢ\x01ᄑ︑￿ࠀἏᄑā\x10ခ￰．࿰ༀﻠကထ࿱\0︁ď\0\0\0ᄐᄑ￱／㏿䑃̳\x11\0\0￰ကᄂ∡Ȣ\0￰\x0eჿ\0￿ࠀ️ÿĐ︑￯ﻠ￿߮ﴏაἏᇠᄂ‏㌣∢ༀ\x01\0\0￰Ğㄑ︡￿ࠀ\0༁췜쳝čხ\0ༀđð︀⋱ሐ࿯＀࿰䐿⌳ᄳℒሢᄁ\0ÿဏ\x01￿߿볝첼￿ל⌿∢￞⿾∢ᄒ﻾￯℟ሡ￰\x01ခᄐအ\x01ȟ】ဒᄀ﷯￮￝㏿⌳ᑄ∢∢ġခᇰሡဢḡᄁထሁ∠Ēﴀ￾\0ā༏ჱ￿۝ჭကကð￿߾ȑ\x10࿡췌䳜䐴㑃￿߽￿ۭᇾအᄁ\x10．\0࿰᷌đ\0⌳ဢᄁ∢ဳ￿ࠀÿ༏\0ğအ촂쳝￾ﻭ￯￿߾î༏\0\0ထᄑñ\0쳌⪻−đ\0\0쮐￿۬໿ðďခ￱™∢￿ࠀ￿םᳮᇱሠ￿ࠀᄟᄑ\x01\0￿מ\x01．࿰ð\x01＀￿ࠀༀðတ࿰／ð\0\0\0Ā\0ሠ刣Ｔ￿ࠀ࿿＀￿ࠀ࿿ð￰࿮ð￿؎ﳯခǠĜထᄁᄑđᄁĐထ∑ᄒᄁᄟ\x10￿׮ꪽ￿ࠀ￿ࠀ￿ࠀ￿ࠀᇿℑ￯￿ࠀ\0\0ᄐᄑ\x11\0\0\0Ā\x10\x01￿ࠀ￯￿ࠀ࿿\0\0\0\0\x01\0＀ﻰ࿯ༀðᄀᄑ\x01\x10ထĀﻯ￯ᄑᄑ\x01￿מ￿׍쯝ꦜ\x01ðð︀￯￯￯ကᄀ￰þ໱ჟᄑ∡ﴀ￰￿ࠀÿð࿰￿ࠀ￿׾∭∢ᄢ࿰＀￰တᄑ࿰／໱\0ᄀĀ\x10＀￿ࠀ࿾ᄀᄑ\0\0￰￿ࠀዿጱɄ\0Ā\0\x01\0\0\0\x0f\0\0￿ࠀ￰\0\0\0￰῿ᄑခ\0\0￿π첛࿛ကȑ０฀ᄐᄑ￱￿ࠀ⋿⌲̈́\0\0ሡ™∢￿܀ﾻð￿߾ﻮ⃳컿\x0e\x10１￿ۮ￿ύ곍∢㌢™Ｂ￿ࠀ࿿က￰䌳ሲထ！༏ჿ１ᄑᄑሑအ\x0eỰℂ￿׎\x1aᄑ䈀晥ᔶᄐ℡￿ۿ￿׎￿߾პሠ￿ӏ￿׍￿ם￿ם﷞⋿㈣࿰\0ༀð࿿＀ᄁᄑđ∳３ÿᄀခ￿׊ᄯȑᄐᄑ‐㈒∢￿ۿ℮∡Ģᄐ￾ﻯ\0\0￰￿߾？￿ߞ￝쳏ñ\x0f￡࿠￿߮￿۬﷮ℑᄢ￿ב￿׮Í쯮Ȣ\0\0Đꬑⷡ؀긋㼯\x12⌭∲唓⌳ÿ\0ᄑℑ￢Ā㌑⌲ጳ∂ሑﻮ췿￻쳮￿ל࿮Ā＀㈢⌲໳㄁ᄱȐ쀡￿׌쳍ÿᄀﻡ췝ǋ\x01໮\0\0￿׮ᄞĐ︀￾￿׾￿׌쳌\0\0⌠∲#ᄀ￿׍骫ᄑᄑ༁䏾␲⼀ᄲဂð࿠ᄀᄑᄑᄑሑ\x11ᄁ￮￿ࠀ࿿／ༀ／ᄎᄐ\x11\0\0\0\0\0ᇰᄑđ\0ð\0\x01ᄀ\0ᄀᄑᄀᄑᄑ￿ࠀ￾￿ࠀ＀￿ࠀༀ\0ð\0ခ\x11ᄀ\0ð\0￿ࠀ￿ࠀﻮ\x0fᄐꄑ뮫쮻￿ࠀ￿ࠀᄟ\0ᄐ\x11\x0f\0\0\x0fᄐᄑᄑᄑđĀᄐ\0\0\x10‑１࿰῿ᄁᄑÿ\0࿰∏ᄒ\x01ᄐᄑđĐ∑ȁတĀđကĀᄑᄀ\x11ᄑထĐđ\0\0\x10ᄁ￱￯￾࿰ÿ／đ＀￿ࠀÿð￿ࠀ￿ࠀတᄑ\x12ༀᇿ\0đ\0ༀ\0࿰ðᄐᄑሒᄐᄐ\0\0￞þ\0ထđ\0đ\x11Āကā\x01ÿᄟ￿۝\x0f\0ᄀđđ\0\0ĐĀခᄑ￿ࠀ໮\0\0\0\0릠￿׍ￜ༏／\x0fᄑᄑᄑᄑ１￿ࠀ໮\0\x0f";
static const char CONV3_S_ENC[] = "ֻ㸋㷴翯㺏ᒐ㺀㷅仵㸚ႏ㹟懣㹀ᩃ㸮룒㸪䒏㺢鮗㷇୧㸻㾸㻹ᄷ㹕㧊㸃妠㹪诈㺈똞㼢殈㹋켮㸘폣㻄맻㸑軿㻇让㻖꒥㻙馾㸒줞㸜鈕㹥幁㹀긨㹯麥㺂쫂㶦苢㺬죃㺘᝝㸶묑㷞▀㷧鈗㰈㴛㷉蟲㸅畓㹢苩㱁펋㺪❚㷡጖㷍艀㹋㣕㹅䖩㸃걹㸻￿Ԙ㸖毋㸙餦㹡哻㹪﷍㺙ꖰ㺚ԓ㸀ᶲ㺯ߘ㸰셩㹀畸㶍ꮗ㸒㻖⮕㸊￿͈㹞淽㺀崷㹧쨣㺁雝㺣㼁㹲됍㹚㻀᪣㸒쏖㹟㺕䊁㹮쵻㺦튮㹻䔱㺠ኹ㺁㺶뤾㺐￿Ɉ㹏꡻㸨䇉㶵ꢯ㺁뵹㺸㹏權㸅홦㸷⻛㶬᝻㹷很㶿퍳㻒걅㸺섉㸭";
static const char CONV3_B_ENC[] = "烵븦賸븙䜮뻬ꔟ㵍炨㬓蟯뺚骕빹ȸ뺃킬빅ﺛ㲰෮붘寯㵸晸뷭랳븴༊벳릜ᰬ뵲⎙뻆厓뻋䰱뷑︄빟痹븡비븗뺼呜뻭뤊빹㶳媚뺏￥븯봀씴뺆㌔빪⡱븽껲븓ྩ뻒부퉿빧榼붅뽍붛䵩븮悂븃븚￿٤봺郕븆祥븏ꔨ븼竝붌딷밻쫥붴ɒ붱뽢뺅Ṩ뵹뻑垿뺤뻤珌뺇䔕뵮䘙뺭硌뷦ઘ뺼㡧봌斨빗弅뺹觷㵔绐뻃g비읠뺏ರ뵉䃵뻓蟜븍ⵈ뺗ꂭ뺻观㶢疫뷆绐뺙决뺤倔븵뷈֢빢嚩븙懹붸閭빭쳹븳￿ݦ㬦趇붰⇙뺷블藗빧㜺㴗醷뻭ঁ뷠술㶼￿ә봬￿׶붇諬㵎￿ޏ벙";
static const char POLICY_W_ENC[] = "\x10đဟ\0◿￿ࠀ\0¿Ȁ\r\x02ༀ倀？␐＀ ‌àऐ￱\0\0ðĀ뀀Āￛༀð\t฀\0\0\0Ā \x03༟༏䀀က\x01က\x0f\0ÿ¿\x10Ḁ\x01\00ഀ\x01\rἀ\x01豈\0\x03ÐπဏĀ\x0e\x10က＀쀀\0Ⴟ\0\x0f\0ăȀ\0\0\0ß\0\0Н̐ðက\r\x01\0\0\x01༠ Ḁ\x01\00Ȁ\x02\x0fἀ\0豈฀ဌ\x01－ðӀဏ\0þ\0Ðჰ\x02ⴞ฀O탱\x10༏\0\x03တȁက\x1f฀!ॏðჿğǰ\0\0āЏ¿냰退탱ď\r໽ð໰䀀฀퐀ሀ \x01\x01\x0f ＀\x0fďᄀð \0ā\0က\x0fð\x02က\x01\x01āÙǯð\0\x11໰\0\0ऀî\x10글῱\x0fമ\x01̀ý \0ฐ⋰퀟\x14\0⼐\x01\0༐！ȀÑ㼠\x10ȏ\0Ďð\x0b‐ðÁĀ\tˏ࿰\0️ሁ㴀ေကÿ︁⾲‎뀡⏰Đဒð\0\x1f 䀁฀￱\0ༀ\0ༀ\x0e༞＠က㼏\0ഐऀÿက\x0f\x0f\x10\r\x03\0턏À༁\x01\0\x0bั\x01\0ဠྰ티ᄃ\0Ǒﴀ켐ᆟ଀\x0e관\0෱ჯἀ̎\x10償놰࿰\0退︪切༏ကᄀñ\x01￿߀Ἆğༀ\0ā\x01װ\nఱ\x11〯　︀￿܀\x01Ē±켍ዟༀ\x0eဏư㓱ภȏ僰뇿ꔒ࿿ጐð＀῰Ԁ 퀀䀠瀐䀏ŎǱ邿ጰ⃰\0）́ဏ༟ჺ\0\x0c\r\x10=ò\x10à෰἟ဏ\0\0ḱ\x01\0ჰഀ\0ď턀㼀‏\0\x01ఀက￿Ԁऀ‏\0ā\x01ü\0က໱ༀ\x01\0༏\x0fྐ࿟ÿ\x02ðď༂İ\x10ǐ࿀\x10Ð\0\0　\0\0\x10ð༏ᄀ、 \x10Ȁ̀\0ऀ퀏\0ကĀ\x01þༀကༀ༁\x01";
static const char POLICY_S_ENC[] = "￿ݹ㻞莈㼲䮁㼉㌫㼾䁷㼃槅㼋趒㻹饮㼠疖㼆厗㻩ꦊ㻵퓿㼷䑪㻠ŷ㼑蘪㻎쑲㺎蜴㼎ᦋ㼈༾㼴旙㼊";
static const char POLICY_B_ENC[] = "织뻙鲮㭹ꗫ㼆ʲ㷂뛦㹸뚕뺍㸯㧻㺔뫔붾縐㵿ꥠ빊Ⴅ붔ᇧ㺉橾븲蜯㸻ꪚ빞吇븒ᚫ㼅㘼봘㲿";
static const char VALUE_W_ENC[] = "\x01ဏ\0\0\0Ā\0\0Āခ\x0f\x10\x01\0\0\0\0\0က\0\0Ā\0ð";
static const char VALUE_S_ENC[] = "譗㺵";
static const char VALUE_B_ENC[] = "꾌벜";
static void init_weights(void) {
    CNN_CH1 = 96;
    CNN_CH2 = 96;
    CNN_CH3 = 96;
    decode_int4(CONV1_W_ENC, conv1_w, 16416);
    decode_f32(CONV1_S_ENC, conv1_s, 96);
    decode_f32(CONV1_B_ENC, conv1_b, 96);
    decode_int4(CONV2_W_ENC, conv2_w, 82944);
    decode_f32(CONV2_S_ENC, conv2_s, 96);
    decode_f32(CONV2_B_ENC, conv2_b, 96);
    decode_int4(CONV3_W_ENC, conv3_w, 82944);
    decode_f32(CONV3_S_ENC, conv3_s, 96);
    decode_f32(CONV3_B_ENC, conv3_b, 96);
    { int8_t _tmp[2040]; float _sc[20];
      decode_int4(POLICY_W_ENC, _tmp, 2040);
      decode_f32(POLICY_S_ENC, _sc, 20);
      for (int o=0; o<20; o++)
        for (int i=0; i<102; i++)
          policy_w[o*102+i] = (float)_tmp[o*102+i] * _sc[o];
      decode_f32(POLICY_B_ENC, policy_b, 20); }
    { int8_t _tmp[102]; float _sc[1];
      decode_int4(VALUE_W_ENC, _tmp, 102);
      decode_f32(VALUE_S_ENC, _sc, 1);
      for (int o=0; o<1; o++)
        for (int i=0; i<102; i++)
          value_w[o*102+i] = (float)_tmp[o*102+i] * _sc[o];
      decode_f32(VALUE_B_ENC, value_b, 1); }
    EVAL_BODY = 120.0f;
    EVAL_LOSS = 18.0f;
    EVAL_MOBILITY = 7.5f;
    EVAL_APPLE = 16.0f;
    EVAL_STABILITY = 10.0f;
    EVAL_BREAKPOINT = 9.0f;
    EVAL_FRAGILE = 8.0f;
    EVAL_TERMINAL = 10000.0f;
    SEARCH_FIRST_MS = 850;
    SEARCH_LATER_MS = 40;
}
