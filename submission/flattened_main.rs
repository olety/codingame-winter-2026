#![allow(text_direction_codepoint_in_literal)]
mod engine {
 pub mod coord {
  use std::cmp::Ordering;
  #[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
  pub struct Coord {
   pub x: i32,
   pub y: i32,
  }
  impl Coord {
   pub const fn new(x: i32, y: i32) -> Self {
    Self { x, y }
   }
   pub const fn add(self, dx: i32, dy: i32) -> Self {
    Self::new(self.x + dx, self.y + dy)
   }
   pub const fn add_coord(self, other: Coord) -> Self {
    Self::new(self.x + other.x, self.y + other.y)
   }
   pub fn manhattan_to(self, other: Coord) -> i32 {
    (self.x - other.x).abs() + (self.y - other.y).abs()
   }
   pub fn to_int_string(self) -> String {
    format!("{} {}", self.x, self.y)
   }
  }
  impl Ord for Coord {
   fn cmp(&self, other: &Self) -> Ordering {
    match self.x.cmp(&other.x) {
     Ordering::Equal => self.y.cmp(&other.y),
     ord => ord,
    }
   }
  }
  impl PartialOrd for Coord {
   fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
    Some(self.cmp(other))
   }
  }
 }
 pub mod direction {
  use super::Coord;
  #[derive(Clone, Copy, Debug, Eq, PartialEq)]
  pub enum Direction {
   North,
   East,
   South,
   West,
   Unset,
  }
  impl Direction {
   pub const ALL: [Direction; 4] = [
    Direction::North,
    Direction::East,
    Direction::South,
    Direction::West,
   ];
   pub const fn delta(self) -> Coord {
    match self {
     Direction::North => Coord::new(0, -1),
     Direction::East => Coord::new(1, 0),
     Direction::South => Coord::new(0, 1),
     Direction::West => Coord::new(-1, 0),
     Direction::Unset => Coord::new(0, 0),
    }
   }
   pub const fn alias(self) -> &'static str {
    match self {
     Direction::North => "N",
     Direction::East => "E",
     Direction::South => "S",
     Direction::West => "W",
     Direction::Unset => "X",
    }
   }
   pub const fn opposite(self) -> Direction {
    match self {
     Direction::North => Direction::South,
     Direction::East => Direction::West,
     Direction::South => Direction::North,
     Direction::West => Direction::East,
     Direction::Unset => Direction::Unset,
    }
   }
   pub const fn from_coord(coord: Coord) -> Direction {
    match (coord.x, coord.y) {
     (0, -1) => Direction::North,
     (1, 0) => Direction::East,
     (0, 1) => Direction::South,
     (-1, 0) => Direction::West,
     _ => Direction::Unset,
    }
   }
   pub fn from_command(command: &str) -> Option<Direction> {
    match command {
     "UP" => Some(Direction::North),
     "RIGHT" => Some(Direction::East),
     "DOWN" => Some(Direction::South),
     "LEFT" => Some(Direction::West),
     _ => None,
    }
   }
  }
 }
 pub mod map {
  use std::collections::{BTreeSet, VecDeque};
  use super::{Coord, Direction};
  #[derive(Clone, Copy, Debug, Eq, PartialEq)]
  pub enum TileType {
   Empty,
   Wall,
  }
  #[derive(Clone, Debug, Eq, PartialEq)]
  pub struct Grid {
   pub width: i32,
   pub height: i32,
   pub tiles: Vec<TileType>,
   pub spawns: Vec<Coord>,
   pub apples: Vec<Coord>,
  }
  impl Grid {
   pub const ADJACENCY: [Coord; 4] = [
    Direction::North.delta(),
    Direction::East.delta(),
    Direction::South.delta(),
    Direction::West.delta(),
   ];
   pub const ADJACENCY_8: [Coord; 8] = [
    Direction::North.delta(),
    Direction::East.delta(),
    Direction::South.delta(),
    Direction::West.delta(),
    Coord::new(-1, -1),
    Coord::new(1, 1),
    Coord::new(1, -1),
    Coord::new(-1, 1),
   ];
   pub fn new(width: i32, height: i32) -> Self {
    let len = (width * height) as usize;
    Self {
     width,
     height,
     tiles: vec![TileType::Empty; len],
     spawns: Vec::new(),
     apples: Vec::new(),
    }
   }
   pub fn is_valid(&self, coord: Coord) -> bool {
    coord.x >= 0 && coord.x < self.width && coord.y >= 0 && coord.y < self.height
   }
   pub fn index(&self, coord: Coord) -> Option<usize> {
    self.is_valid(coord)
     .then_some((coord.y * self.width + coord.x) as usize)
   }
   pub fn get(&self, coord: Coord) -> Option<TileType> {
    self.index(coord).map(|idx| self.tiles[idx])
   }
   pub fn set(&mut self, coord: Coord, tile: TileType) {
    if let Some(idx) = self.index(coord) {
     self.tiles[idx] = tile;
    }
   }
   pub fn coords(&self) -> Vec<Coord> {
    let mut coords = Vec::with_capacity(self.tiles.len());
    for y in 0..self.height {
     for x in 0..self.width {
      coords.push(Coord::new(x, y));
     }
    }
    coords
   }
    pub fn opposite(&self, coord: Coord) -> Coord {
    Coord::new(self.width - coord.x - 1, coord.y)
   }
        }
 }
 pub mod state {
  use std::collections::{BTreeMap, BTreeSet, VecDeque};
  use super::{Coord, Direction, Grid, TileType};
  #[derive(Clone, Debug, Eq, PartialEq)]
  pub struct BirdState {
   pub id: i32,
   pub owner: usize,
   pub body: VecDeque<Coord>,
   pub alive: bool,
   pub direction: Option<Direction>,
  }
  impl BirdState {
   pub fn head(&self) -> Coord {
    *self.body.front().expect("bird has at least one segment")
   }
   pub fn facing(&self) -> Direction {
    if self.body.len() < 2 {
     Direction::Unset
    } else {
     let head = self.body[0];
     let neck = self.body[1];
     Direction::from_coord(Coord::new(head.x - neck.x, head.y - neck.y))
    }
   }
   pub fn length(&self) -> usize {
    self.body.len()
   }
  }
  #[derive(Clone, Copy, Debug, Eq, PartialEq)]
  pub enum BirdCommand {
   Keep,
   Turn(Direction),
  }
  #[derive(Clone, Debug, Default, Eq, PartialEq)]
  pub struct PlayerAction {
   pub per_bird: BTreeMap<i32, BirdCommand>,
  }
  impl PlayerAction {
   pub fn command_for(&self, bird_id: i32) -> BirdCommand {
    self.per_bird
     .get(&bird_id)
     .copied()
     .unwrap_or(BirdCommand::Keep)
   }
  }
  #[derive(Clone, Debug, Eq, PartialEq)]
  pub struct GameState {
   pub grid: Grid,
   pub birds: Vec<BirdState>,
   pub losses: [i32; 2],
   pub turn: i32,
  }
  #[derive(Clone, Debug, Eq, PartialEq)]
  pub struct StepResult {
   pub game_over: bool,
   pub body_scores: [i32; 2],
   pub final_scores: [i32; 2],
  }
  #[derive(Clone, Copy, Debug, Eq, PartialEq)]
  pub enum TerminalReason {
   ApplesExhausted,
   PlayerEliminated(usize),
   TurnLimitReached,
  }
  #[derive(Clone, Debug, Eq, PartialEq)]
  pub struct FinalResult {
   pub terminal: bool,
   pub reason: Option<TerminalReason>,
   pub body_scores: [i32; 2],
   pub final_scores: [i32; 2],
   pub losses: [i32; 2],
   pub body_diff: i32,
   pub loss_diff: i32,
   pub winner: Option<usize>,
  }
  impl FinalResult {
   pub fn body_diff_for(&self, owner: usize) -> i32 {
    if owner == 0 {
     self.body_diff
    } else {
     -self.body_diff
    }
   }
   pub fn loss_diff_for(&self, owner: usize) -> i32 {
    if owner == 0 {
     self.loss_diff
    } else {
     -self.loss_diff
    }
   }
  }
  impl GameState {
   pub fn new(grid: Grid) -> Self {
    Self {
     grid,
     birds: Vec::new(),
     losses: [0, 0],
     turn: 0,
    }
   }
   pub fn add_bird(
    &mut self,
    id: i32,
    owner: usize,
    body: Vec<Coord>,
    direction: Option<Direction>,
   ) {
    self.birds.push(BirdState {
     id,
     owner,
     body: body.into_iter().collect(),
     alive: true,
     direction,
    });
    self.birds.sort_by_key(|bird| bird.id);
   }
   pub fn body_scores(&self) -> [i32; 2] {
    let mut scores = [0, 0];
    for bird in self.live_birds() {
     scores[bird.owner] += bird.body.len() as i32;
    }
    scores
   }
   pub fn final_scores(&self) -> [i32; 2] {
    let mut scores = self.body_scores();
    if scores[0] == scores[1] {
     scores[0] -= self.losses[0];
     scores[1] -= self.losses[1];
    }
    scores
   }
   pub fn live_birds(&self) -> impl Iterator<Item = &BirdState> {
    self.birds.iter().filter(|bird| bird.alive)
   }
   pub fn live_birds_mut(&mut self) -> impl Iterator<Item = &mut BirdState> {
    self.birds.iter_mut().filter(|bird| bird.alive)
   }
   pub fn birds_for_player(&self, owner: usize) -> impl Iterator<Item = &BirdState> {
    self.birds.iter().filter(move |bird| bird.owner == owner)
   }
   pub fn legal_commands_for_bird(&self, bird_id: i32) -> Vec<BirdCommand> {
    let Some(bird) = self
     .birds
     .iter()
     .find(|bird| bird.id == bird_id && bird.alive)
    else {
     return Vec::new();
    };
    let facing = bird.facing();
    let mut commands = vec![BirdCommand::Keep];
    for direction in Direction::ALL {
     if facing != Direction::Unset && direction == facing.opposite() {
      continue;
     }
     if direction == facing {
      continue;
     }
     commands.push(BirdCommand::Turn(direction));
    }
    commands
   }
   pub fn legal_joint_actions(&self, owner: usize) -> Vec<PlayerAction> {
    let birds: Vec<_> = self
     .birds_for_player(owner)
     .filter(|bird| bird.alive)
     .map(|bird| bird.id)
     .collect();
    let mut result = Vec::new();
    let mut current = PlayerAction::default();
    self.enumerate_actions(&birds, 0, &mut current, &mut result);
    if result.is_empty() {
     result.push(PlayerAction::default());
    }
    result
   }
   fn enumerate_actions(
    &self,
    birds: &[i32],
    idx: usize,
    current: &mut PlayerAction,
    output: &mut Vec<PlayerAction>,
   ) {
    if idx == birds.len() {
     output.push(current.clone());
     return;
    }
    let bird_id = birds[idx];
    for command in self.legal_commands_for_bird(bird_id) {
     match command {
      BirdCommand::Keep => {
       current.per_bird.remove(&bird_id);
      }
      BirdCommand::Turn(direction) => {
       current
        .per_bird
        .insert(bird_id, BirdCommand::Turn(direction));
      }
     }
     self.enumerate_actions(birds, idx + 1, current, output);
    }
    current.per_bird.remove(&bird_id);
   }
    pub fn step(&mut self, p0: &PlayerAction, p1: &PlayerAction) -> StepResult {
    self.turn += 1;
    self.reset_turn_state();
    self.apply_moves(p0, p1);
    self.apply_eats();
    self.apply_beheadings();
    self.apply_falls();
    let body_scores = self.body_scores();
    let final_scores = self.final_scores();
    StepResult {
     game_over: self.is_game_over(),
     body_scores,
     final_scores,
    }
   }
   fn reset_turn_state(&mut self) {
    for bird in self.live_birds_mut() {
     bird.direction = None;
    }
   }
   fn apply_moves(&mut self, p0: &PlayerAction, p1: &PlayerAction) {
    for idx in 0..self.birds.len() {
     if !self.birds[idx].alive {
      continue;
     }
     let command = if self.birds[idx].owner == 0 {
      p0.command_for(self.birds[idx].id)
     } else {
      p1.command_for(self.birds[idx].id)
     };
     match command {
      BirdCommand::Keep => {
       if self.birds[idx].direction.is_none()
        || self.birds[idx].direction == Some(Direction::Unset)
       {
        self.birds[idx].direction = Some(self.birds[idx].facing());
       }
      }
      BirdCommand::Turn(direction) => {
       let facing = self.birds[idx].facing();
       if facing != Direction::Unset && direction == facing.opposite() {
        self.birds[idx].direction = Some(facing);
       } else {
        self.birds[idx].direction = Some(direction);
       }
      }
     }
     let direction = self.birds[idx].direction.unwrap_or(Direction::Unset);
     let new_head = self.birds[idx].head().add_coord(direction.delta());
     let will_eat = self.grid.apples.contains(&new_head);
     if !will_eat {
      self.birds[idx].body.pop_back();
     }
     self.birds[idx].body.push_front(new_head);
    }
   }
   fn apply_eats(&mut self) {
    let mut apples_eaten = BTreeSet::new();
    for bird in self.live_birds() {
     if self.grid.apples.contains(&bird.head()) {
      apples_eaten.insert(bird.head());
     }
    }
    self.grid
     .apples
     .retain(|apple| !apples_eaten.contains(apple));
   }
   fn apply_beheadings(&mut self) {
    let alive_indices: Vec<_> = self
     .birds
     .iter()
     .enumerate()
     .filter_map(|(idx, bird)| bird.alive.then_some(idx))
     .collect();
    let mut to_behead = Vec::new();
    for idx in alive_indices {
     let head = self.birds[idx].head();
     let is_in_wall = self.grid.get(head) == Some(TileType::Wall);
     let intersecting: Vec<_> = self
      .birds
      .iter()
      .filter(|bird| bird.alive && bird.body.contains(&head))
      .collect();
     let is_in_bird = intersecting.iter().any(|other| {
      other.id != self.birds[idx].id
       || other
        .body
        .iter()
        .skip(1)
        .any(|segment| *segment == other.head())
     });
     if is_in_wall || is_in_bird {
      to_behead.push(idx);
     }
    }
    for idx in to_behead {
     let owner = self.birds[idx].owner;
     if self.birds[idx].body.len() <= 3 {
      self.losses[owner] += self.birds[idx].body.len() as i32;
      self.birds[idx].alive = false;
     } else {
      self.birds[idx].body.pop_front();
      self.losses[owner] += 1;
     }
    }
   }
   fn apply_falls(&mut self) {
    let mut something_fell = true;
    while something_fell {
     something_fell = false;
     while self.apply_individual_falls() {
      something_fell = true;
     }
     if self.apply_intercoiled_falls() {
      something_fell = true;
     }
    }
   }
   fn apply_individual_falls(&mut self) -> bool {
    let mut moved = false;
    let alive_indices: Vec<_> = self
     .birds
     .iter()
     .enumerate()
     .filter_map(|(idx, bird)| bird.alive.then_some(idx))
     .collect();
    for idx in alive_indices {
     let body: Vec<_> = self.birds[idx].body.iter().copied().collect();
     let can_fall = body
      .iter()
      .all(|coord| !self.something_solid_under(*coord, &body));
     if can_fall {
      moved = true;
      self.shift_bird_down(idx);
      if self.birds[idx]
       .body
       .iter()
       .all(|coord| coord.y >= self.grid.height + 1)
      {
       self.birds[idx].alive = false;
      }
     }
    }
    moved
   }
   fn apply_intercoiled_falls(&mut self) -> bool {
    let groups = self.intercoiled_groups();
    let mut moved = false;
    for group in groups {
     let meta_body: Vec<_> = group
      .iter()
      .flat_map(|idx| self.birds[*idx].body.iter().copied())
      .collect();
     let can_fall = meta_body
      .iter()
      .all(|coord| !self.something_solid_under(*coord, &meta_body));
     if !can_fall {
      continue;
     }
     moved = true;
     for idx in group {
      self.shift_bird_down(idx);
      if self.birds[idx].head().y >= self.grid.height {
       self.birds[idx].alive = false;
      }
     }
    }
    moved
   }
   fn intercoiled_groups(&self) -> Vec<Vec<usize>> {
    let alive_indices: Vec<_> = self
     .birds
     .iter()
     .enumerate()
     .filter_map(|(idx, bird)| bird.alive.then_some(idx))
     .collect();
    let mut groups = Vec::new();
    let mut seen = BTreeSet::new();
    for idx in alive_indices.iter().copied() {
     if seen.contains(&idx) {
      continue;
     }
     let mut group = Vec::new();
     let mut queue = VecDeque::from([idx]);
     while let Some(current) = queue.pop_front() {
      if !seen.insert(current) {
       continue;
      }
      group.push(current);
      for other in alive_indices.iter().copied() {
       if current == other || seen.contains(&other) {
        continue;
       }
       if birds_are_touching(&self.birds[current], &self.birds[other]) {
        queue.push_back(other);
       }
      }
     }
     if group.len() > 1 {
      groups.push(group);
     }
    }
    groups
   }
   fn shift_bird_down(&mut self, idx: usize) {
    for coord in self.birds[idx].body.iter_mut() {
     coord.y += 1;
    }
   }
   fn something_solid_under(&self, coord: Coord, ignore_body: &[Coord]) -> bool {
    let below = coord.add(0, 1);
    if ignore_body.contains(&below) {
     return false;
    }
    if self.grid.get(below) == Some(TileType::Wall) {
     return true;
    }
    if self
     .birds
     .iter()
     .any(|bird| bird.alive && bird.body.contains(&below))
    {
     return true;
    }
    self.grid.apples.contains(&below)
   }
   pub fn is_game_over(&self) -> bool {
    self.grid.apples.is_empty()
     || (0..=1).any(|owner| self.birds_for_player(owner).all(|bird| !bird.alive))
   }
   pub fn terminal_reason(&self, max_turns: i32) -> Option<TerminalReason> {
    if self.grid.apples.is_empty() {
     return Some(TerminalReason::ApplesExhausted);
    }
    if let Some(owner) =
     (0..=1).find(|owner| self.birds_for_player(*owner).all(|bird| !bird.alive))
    {
     return Some(TerminalReason::PlayerEliminated(owner));
    }
    if self.turn >= max_turns {
     return Some(TerminalReason::TurnLimitReached);
    }
    None
   }
   pub fn is_terminal(&self, max_turns: i32) -> bool {
    self.terminal_reason(max_turns).is_some()
   }
   pub fn final_result(&self, max_turns: i32) -> FinalResult {
    let body_scores = self.body_scores();
    let final_scores = self.final_scores();
    let body_diff = body_scores[0] - body_scores[1];
    let loss_diff = self.losses[1] - self.losses[0];
    let winner = if body_diff > 0 {
     Some(0)
    } else if body_diff < 0 {
     Some(1)
    } else if loss_diff > 0 {
     Some(0)
    } else if loss_diff < 0 {
     Some(1)
    } else {
     None
    };
    FinalResult {
     terminal: self.is_terminal(max_turns),
     reason: self.terminal_reason(max_turns),
     body_scores,
     final_scores,
     losses: self.losses,
     body_diff,
     loss_diff,
     winner,
    }
   }
  }
  fn birds_are_touching(a: &BirdState, b: &BirdState) -> bool {
   a.body
    .iter()
    .any(|left| b.body.iter().any(|right| left.manhattan_to(*right) == 1))
  }
 }
 pub use self::coord::Coord;
 pub use self::direction::Direction;
 pub use self::map::{Grid, TileType};
 pub use self::state::{
  BirdCommand, BirdState, FinalResult, GameState, PlayerAction, StepResult, TerminalReason,
 };
}
mod bot {
 pub mod config {
  #[derive(Clone, Copy, Debug, PartialEq)]
    pub struct EvalWeights {
     pub body: f64,
     pub loss: f64,
     pub mobility: f64,
     pub apple: f64,
     pub stability: f64,
     pub breakpoint: f64,
     pub fragile_attack: f64,
     pub terminal: f64,
    }
    impl Default for EvalWeights {
     fn default() -> Self {
      Self {
       body: 120.0,
       loss: 18.0,
       mobility: 7.5,
       apple: 16.0,
       stability: 10.0,
       breakpoint: 9.0,
       fragile_attack: 8.0,
       terminal: 10000.0,
      }
     }
    }
    #[derive(Clone, Copy, Debug, PartialEq)]
    pub struct SearchConfig {
     pub first_turn_ms: u64,
     pub later_turn_ms: u64,
     pub deepen_top_my: usize,
     pub deepen_top_opp: usize,
     pub deepen_child_my: usize,
     pub deepen_child_opp: usize,
     pub extra_nodes_after_root: u64,
    }
    impl Default for SearchConfig {
     fn default() -> Self {
      Self {
       first_turn_ms: 850,
       later_turn_ms: 40,
       deepen_top_my: 6,
       deepen_top_opp: 8,
       deepen_child_my: 3,
       deepen_child_opp: 3,
       extra_nodes_after_root: 5000,
      }
     }
    }
    #[derive(Clone, Debug, PartialEq)]
    pub struct HybridConfig {
     pub weights_path: Option<String>,
     pub prior_mix: f64,
     pub leaf_mix: f64,
     pub value_scale: f64,
     pub prior_depth_limit: usize,
     pub leaf_depth_limit: usize,
    }
    impl Default for HybridConfig {
     fn default() -> Self {
      Self {
       weights_path: None,
       prior_mix: 0.0,
       leaf_mix: 0.0,
       value_scale: 48.0,
       prior_depth_limit: usize::MAX,
       leaf_depth_limit: usize::MAX,
      }
     }
    }
    impl HybridConfig {
     pub fn is_enabled(&self) -> bool {
      self.weights_path.is_some() && (self.prior_mix != 0.0 || self.leaf_mix != 0.0)
     }
    }
    #[derive(Clone, Debug, PartialEq)]
    pub struct BotConfig {
     pub name: String,
     pub eval: EvalWeights,
     pub search: SearchConfig,
     pub hybrid: Option<HybridConfig>,
    }
    impl Default for BotConfig {
     fn default() -> Self {
      Self::embedded()
     }
    }
    impl BotConfig {
     pub fn embedded() -> Self {
      Self {
       name: "hybrid-value-mid-12ch-distill01".to_owned(),
       eval: EvalWeights::default(),
       search: SearchConfig::default(),
       hybrid: Some(HybridConfig {
   weights_path: Some("__embedded__".to_owned()),
   prior_mix: 0.0,
   leaf_mix: 0.08,
   value_scale: 48.0,
   prior_depth_limit: 0,
   leaf_depth_limit: 0,
  }),
      }
     }
    }
 }
 pub mod eval {
  use crate::engine::{Coord, GameState, TileType};
  use super::config::EvalWeights;
  pub fn evaluate(state: &GameState, owner: usize, max_turns: i32, weights: &EvalWeights) -> f64 {
   if state.is_terminal(max_turns) {
    let final_result = state.final_result(max_turns);
    return weights.terminal * f64::from(final_result.body_diff_for(owner))
     + f64::from(final_result.loss_diff_for(owner));
   }
   let body_scores = state.body_scores();
   let body_diff = f64::from(body_scores[owner] - body_scores[1 - owner]);
   let loss_diff = f64::from(state.losses[1 - owner] - state.losses[owner]);
   let mobility = mobility_score(state, owner);
   let apple_race = apple_race_score(state, owner);
   let stability = support_stability_score(state, owner);
   let breakpoint = breakpoint_score(state, owner);
   let pressure = fragile_attack_score(state, owner);
   weights.body * body_diff
    + weights.loss * loss_diff
    + weights.mobility * mobility
    + weights.apple * apple_race
    + weights.stability * stability
    + weights.breakpoint * breakpoint
    + weights.fragile_attack * pressure
  }
  fn mobility_score(state: &GameState, owner: usize) -> f64 {
   let mut total = 0.0;
   for bird in state.birds_for_player(owner).filter(|bird| bird.alive) {
    total += state.legal_commands_for_bird(bird.id).len() as f64;
   }
   for bird in state.birds_for_player(1 - owner).filter(|bird| bird.alive) {
    total -= state.legal_commands_for_bird(bird.id).len() as f64;
   }
   total
  }
  fn apple_race_score(state: &GameState, owner: usize) -> f64 {
   let my_heads = state
    .birds_for_player(owner)
    .filter(|bird| bird.alive)
    .map(|bird| bird.head())
    .collect::<Vec<_>>();
   let opp_heads = state
    .birds_for_player(1 - owner)
    .filter(|bird| bird.alive)
    .map(|bird| bird.head())
    .collect::<Vec<_>>();
   state
    .grid
    .apples
    .iter()
    .map(|apple| {
     let my_dist = min_distance(*apple, &my_heads);
     let opp_dist = min_distance(*apple, &opp_heads);
     f64::from(opp_dist - my_dist) / f64::from((my_dist + opp_dist + 1).max(1))
    })
    .sum()
  }
  fn support_stability_score(state: &GameState, owner: usize) -> f64 {
   let mut score = 0.0;
   for bird in state.birds.iter().filter(|bird| bird.alive) {
    let supported = bird
     .body
     .iter()
     .any(|coord| has_solid_below(state, bird.id, *coord));
    let value = if supported { 1.0 } else { -1.0 };
    if bird.owner == owner {
     score += value;
    } else {
     score -= value;
    }
   }
   score
  }
  fn breakpoint_score(state: &GameState, owner: usize) -> f64 {
   let own = state
    .birds_for_player(owner)
    .filter(|bird| bird.alive)
    .map(|bird| usize::from(bird.length() >= 4) as f64)
    .sum::<f64>();
   let opp = state
    .birds_for_player(1 - owner)
    .filter(|bird| bird.alive)
    .map(|bird| usize::from(bird.length() >= 4) as f64)
    .sum::<f64>();
   own - opp
  }
  fn fragile_attack_score(state: &GameState, owner: usize) -> f64 {
   let my_heads = state
    .birds_for_player(owner)
    .filter(|bird| bird.alive)
    .map(|bird| bird.head())
    .collect::<Vec<_>>();
   state
    .birds_for_player(1 - owner)
    .filter(|bird| bird.alive && bird.length() <= 3)
    .map(|bird| {
     let closest = min_distance(bird.head(), &my_heads);
     1.0 / f64::from(closest + 1)
    })
    .sum()
  }
  fn has_solid_below(state: &GameState, bird_id: i32, coord: Coord) -> bool {
   let below = coord.add(0, 1);
   if state.grid.get(below) == Some(TileType::Wall) {
    return true;
   }
   if state.grid.apples.contains(&below) {
    return true;
   }
   state
    .birds
    .iter()
    .filter(|bird| bird.alive && bird.id != bird_id)
    .any(|bird| bird.body.contains(&below))
  }
  fn min_distance(target: Coord, coords: &[Coord]) -> i32 {
   coords
    .iter()
    .map(|coord| coord.manhattan_to(target))
    .min()
    .unwrap_or(99)
  }
 }
 pub mod features {
  use crate::engine::{BirdCommand, Coord, Direction, GameState, PlayerAction, TileType};
  pub const SCALAR_FEATURES: usize = 6;
  pub const MAX_BIRDS_PER_PLAYER: usize = 4;
  pub const POLICY_ACTIONS_PER_BIRD: usize = 5;
  pub const HYBRID_GRID_CHANNELS: usize = 19;
  #[derive(Clone, Debug)]
  pub struct EncodedPosition {
   pub grid: Vec<Vec<Vec<f32>>>,
   pub scalars: Vec<f32>,
  }
  pub fn encode_hybrid_position(state: &GameState, owner: usize) -> EncodedPosition {
   let width = state.grid.width as usize;
   let height = state.grid.height as usize;
   let mut grid = vec![vec![vec![0.0_f32; width]; height]; HYBRID_GRID_CHANNELS];
   for coord in state.grid.coords() {
    let view = canonical_coord(state, owner, coord);
    if state.grid.get(coord) == Some(TileType::Wall) {
     mark_channel(&mut grid, 0, view);
    }
   }
   for apple in &state.grid.apples {
    let view = canonical_coord(state, owner, *apple);
    mark_channel(&mut grid, 1, view);
   }
   for bird in state.birds.iter().filter(|bird| bird.alive) {
    for segment in bird.body.iter().copied() {
     if !segment_has_support(state, bird.id, segment) {
      let view = canonical_coord(state, owner, segment);
      mark_channel(&mut grid, 2, view);
     }
    }
   }
   for (slot_idx, bird_id) in bird_slot_ids(state, owner).into_iter().enumerate() {
    let head_channel = 3 + slot_idx * 2;
    let body_channel = head_channel + 1;
    if let Some(bird) = state.birds.iter().find(|bird| bird.id == bird_id && bird.alive) {
     let head = canonical_coord(state, owner, bird.head());
     mark_channel(&mut grid, head_channel, head);
     for segment in bird.body.iter().skip(1) {
      let view = canonical_coord(state, owner, *segment);
      mark_channel(&mut grid, body_channel, view);
     }
    }
   }
   for (slot_idx, bird_id) in bird_slot_ids(state, 1 - owner).into_iter().enumerate() {
    let head_channel = 11 + slot_idx * 2;
    let body_channel = head_channel + 1;
    if let Some(bird) = state.birds.iter().find(|bird| bird.id == bird_id && bird.alive) {
     let head = canonical_coord(state, owner, bird.head());
     mark_channel(&mut grid, head_channel, head);
     for segment in bird.body.iter().skip(1) {
      let view = canonical_coord(state, owner, *segment);
      mark_channel(&mut grid, body_channel, view);
     }
    }
   }
   EncodedPosition {
    grid,
    scalars: scalar_features(state, owner),
   }
  }
  pub fn policy_targets_for_action(state: &GameState, owner: usize, action: &PlayerAction) -> Vec<i64> {
   let mut targets = vec![-100_i64; MAX_BIRDS_PER_PLAYER];
   for (slot_idx, bird_id) in bird_slot_ids(state, owner).into_iter().enumerate() {
    let Some(bird) = state.birds.iter().find(|bird| bird.id == bird_id) else {
     continue;
    };
    if !bird.alive {
     continue;
    }
    let command = action
     .per_bird
     .get(&bird_id)
     .copied()
     .unwrap_or(BirdCommand::Keep);
    targets[slot_idx] = i64::from(policy_index_for_command(command) as i32);
   }
   targets
  }
  fn scalar_features(state: &GameState, owner: usize) -> Vec<f32> {
   let body_scores = state.body_scores();
   let live_diff = live_bird_count(state, owner) as f32 - live_bird_count(state, 1 - owner) as f32;
   let breakpoint_diff =
    breakpoint_count(state, owner) as f32 - breakpoint_count(state, 1 - owner) as f32;
   let mobility_diff =
    mobility_count(state, owner) as f32 - mobility_count(state, 1 - owner) as f32;
   vec![
    (state.turn as f32 / 200.0).clamp(0.0, 1.0),
    ((body_scores[owner] - body_scores[1 - owner]) as f32 / 32.0).clamp(-1.0, 1.0),
    ((state.losses[1 - owner] - state.losses[owner]) as f32 / 32.0).clamp(-1.0, 1.0),
    (live_diff / 4.0).clamp(-1.0, 1.0),
    (breakpoint_diff / 4.0).clamp(-1.0, 1.0),
    (mobility_diff / 16.0).clamp(-1.0, 1.0),
   ]
  }
  fn bird_slot_ids(state: &GameState, owner: usize) -> Vec<i32> {
   let mut ids = state
    .birds
    .iter()
    .filter(|bird| bird.owner == owner)
    .map(|bird| bird.id)
    .collect::<Vec<_>>();
   ids.sort();
   ids.truncate(MAX_BIRDS_PER_PLAYER);
   ids
  }
  fn policy_index_for_command(command: BirdCommand) -> usize {
   match command {
    BirdCommand::Keep => 0,
    BirdCommand::Turn(Direction::North) => 1,
    BirdCommand::Turn(Direction::East) => 2,
    BirdCommand::Turn(Direction::South) => 3,
    BirdCommand::Turn(Direction::West) => 4,
    BirdCommand::Turn(Direction::Unset) => 1,
   }
  }
  fn canonical_coord(state: &GameState, owner: usize, coord: Coord) -> Coord {
   if owner == 0 {
    coord
   } else {
    state.grid.opposite(coord)
   }
  }
  fn live_bird_count(state: &GameState, owner: usize) -> usize {
   state
    .birds_for_player(owner)
    .filter(|bird| bird.alive)
    .count()
  }
  fn breakpoint_count(state: &GameState, owner: usize) -> usize {
   state
    .birds_for_player(owner)
    .filter(|bird| bird.alive && bird.length() >= 4)
    .count()
  }
  fn mobility_count(state: &GameState, owner: usize) -> usize {
   state
    .birds_for_player(owner)
    .filter(|bird| bird.alive)
    .map(|bird| state.legal_commands_for_bird(bird.id).len())
    .sum()
  }
  fn segment_has_support(state: &GameState, bird_id: i32, coord: Coord) -> bool {
   let below = coord.add(0, 1);
   if state.grid.get(below) == Some(TileType::Wall) || state.grid.apples.contains(&below) {
    return true;
   }
   state
    .birds
    .iter()
    .filter(|bird| bird.alive && bird.id != bird_id)
    .any(|bird| bird.body.contains(&below))
  }
  fn mark_channel(grid: &mut [Vec<Vec<f32>>], channel: usize, coord: Coord) {
   if coord.x < 0 || coord.y < 0 {
    return;
   }
   let Some(plane) = grid.get_mut(channel) else {
    return;
   };
   let y = coord.y as usize;
   let x = coord.x as usize;
   if let Some(row) = plane.get_mut(y) {
    if let Some(cell) = row.get_mut(x) {
     *cell = 1.0;
    }
   }
  }
 }
 pub mod hybrid {
  use std::sync::OnceLock;
    use crate::engine::{GameState, PlayerAction};
    use super::config::{BotConfig, HybridConfig};
    use super::features::{encode_hybrid_position, policy_targets_for_action, HYBRID_GRID_CHANNELS, POLICY_ACTIONS_PER_BIRD, SCALAR_FEATURES};
    #[derive(Clone, Debug)]
    pub struct HybridPrediction {
     pub policy_logits: Vec<f32>,
     pub value: f32,
    }
    impl HybridPrediction {
     pub fn action_prior(&self, state: &GameState, owner: usize, action: &PlayerAction) -> f64 {
      let targets = policy_targets_for_action(state, owner, action);
      let mut total = 0.0_f64;
      for (slot_idx, target) in targets.into_iter().enumerate() {
       if target < 0 {
        continue;
       }
       let index = slot_idx * POLICY_ACTIONS_PER_BIRD + target as usize;
       if let Some(logit) = self.policy_logits.get(index) {
        total += f64::from(*logit);
       }
      }
      total
     }
    }
    struct ConvLayer {
     out_channels: usize,
     in_channels: usize,
     kernel_size: usize,
     weights: &'static [f32],
     bias: &'static [f32],
    }
    struct LinearLayer {
     out_features: usize,
     in_features: usize,
     weights: &'static [f32],
     bias: &'static [f32],
    }
    struct TinyHybridWeights {
     input_channels: usize,
     scalar_features: usize,
     conv1: ConvLayer,
     conv2: ConvLayer,
     conv3: Option<ConvLayer>,
     policy: LinearLayer,
     value: LinearLayer,
    }
    const CONV1_W_LEN: usize = 2052;
  const CONV1_B_LEN: usize = 12;
  const CONV2_W_LEN: usize = 1296;
  const CONV2_B_LEN: usize = 12;
  const CONV3_W_LEN: usize = 1296;
  const CONV3_B_LEN: usize = 12;
  const POLICY_W_LEN: usize = 360;
  const POLICY_B_LEN: usize = 20;
  const VALUE_W_LEN: usize = 18;
  const VALUE_B_LEN: usize = 1;
    static WEIGHTS_F16: &str = "꠼ⶀꤧ꒕ⓝ꯫ꭧ⠬⠎⫪⽕〟㄂⧺ヮ⨽〉ⴁ뢢㔬뚬뙫㑦뀗껉ㅇꋹⰘ뛛▽㪫롔㦌ベ녝Ⰽ뭩ㄌ릷룩㎦먅뢷フ먌백㴺몙랤㱮㯨㫋㱇㮠붒㮕붟붊㬒뺡뱓㦄듸㛩㘋㘼㡈Ⲩ㦌㢸㓽㣮료㚛뮚릐㚺밤뙋㏞뢋㲪먗㨛㬚렩㡇ㅏ뉼Ⲷ뤁ꉑ떭멋ꀖ룺뛢⹱땆딶❢㑍⣎ꩥⴿ⾛냄⯏꺻딠⩏⭒됎⯮꤇둢⽘묻㯖롧멣㰃뒿㜉㪩㚟밡㗔뢸밒㖱몛밙㖖멨뚟㌪떝㓍Ⱈ⎒㗬〗㑟롍⯙딀똬ⰼ떐댞࿶늼ぎꁅ둞㓴냌㒗㈂꩐ꤾ깤뜃굇ぃ꿑⥘ⷉ뎳끺꒷꼯⡕ꁭ뇉ॷ⬭⣇ᬕ귐ㄆ⦺긜㉁⣭똳긠뚯냝⡃녈갹⿥냌ℱ겄ꟸ먦㵳맲럙㷱럩〖㬯⟠뮆ㅫ먁뤀㓌뢽멕☣멎렠㨻뜢뗸㪫뗒ㅛ㢙ㄭ륁᷅랖뙏・딧랮ꀨ뛷뚪㰔룙뛥㸃뢹⋙㬦⿾믌⼾밙멦⽫륝몕⪻멘륕㳋매룚㽎룿ㅲ㲛⸐뱀ⶱ뭩멞ヤ륄밅ⷧ밻㒛륒㟊㝼뙞㔗㐊Ṿㆁㆎ뜸㕲걞럸㍇갥땄⅛㉶㆕⸅ⷕ⌙Ⲻ㑔ꡇ⾾⽨뛫㒭⹜둋㇔ꙁ눏ꕭ㐍㕍㑞㇥㙋㒂ⶲ㖡㑋늷늮뗂꽚꼳꣱듶뀂뉩㕰㔌㒟㉂㍺㑠わ㔄㇇됆늯괣⏮놁ꀨ뉦눟닷⡣둵ꕼ▏ꈫ⦠鴩⣟ᜏ⹎㊃⸒뙛㉂⼍럢됙ꩩ뀅⚜똩꫉⾓뀱ꌼ鷎궷㥱㔔㜚⸚㢠롞Ꟈ㠤⨸⭻맬뗄⸤ꭂ꘿걜꾡룞봯㭍뱊둧㰾몯㡥㬕떷뱶㣍뮯묉㕅룣멼㓣뭐㛔렷들㟋㩩랥ⷕ㤚㔄덮맍뀙덲뤭똞깈꨿뤺띎㳛뢲똍㱚릣。㣱こ략ꈭ뭳랱⑅뗑륜ꭈ륯러㝂렩゠㗗騞べ㘺ꨲ뚻ㆧ럇뜭⥯룜럥걟럶뗶㟕뫥ꤣ㢚뤡㐖㣅꭯먜㑾렶뤩ㆁ릕뤼ヽ뭸⹖㌧᩿⪊㏵᭫〻㎳ꨣほ렽뉸끅Ꚉ둯놃⵲듾㝪귖㕨㈄렾㙈Ⱶ긳㖆⼒둿◁〱롨⽄⮭뢰⢩년㇤닳덡✓댡꫹Ꞻ궎Ⲹ㍺し꯼⿍곋덱됛덚겄녦鹸〖긦ⱪ⳱⌶⹻ㄏ됎㢗㢛뒐㡵㢙㛙㠉렷ⶄ땜뢜나겦늯롽뀤㢓ꓽ㩻㧧럁㬀㧗궠㦌땓띜렑ᾕ렸を㈎맧㑨㑆뎞㠾㚋뛚㥭㟑⻕㡠귗렛뎐낽뛯굄겜롕ꐻ㜗두㠘㣭똎㣱㡜⫯㠜걏똽댳ꝼ뙫ⱹ☁뒍곇㙲땹㓺㏓륰ㇱ㌾ụ㕷됡⸄뗉괝랃갟겊렵ꀜ㡡ꚙ긟㝮㎏㗀㓓ꎻ㜾뚿ㄱ땄ꦑ꾺떁⻵딈◵ロ닼⽖㇘㉧㒇㖑㋥㕌낤〻꽔뙗⻨둹꽟뇊꺖ㅶ딘ㆀⵗ뜩㊤㋇⬪㓹ꉬ뜼ꪫ놄뙏ꖓ댂뛃긐飮냮ⱳꖔ뤨ꍍ꜃鰸꒟ㄚⵏ⼀ㅣㇴ렿⭬⿣뤞둏ꩊꘔ뜾⹺ᰀ꧔ⰽꑟ㓫㤚㧽㝦㥅㚒㑠㠚ㇻ놊먹럔롁뀏ꌃ렠껒껍눟㨒⠹떦㪡謯ぺ㤅㕵뙛델뜞떮Ⳅ뛱룍⧮뢨똚㦹ㆿ둠㭫㈺⩯㤰➨룶끜룲뤰깟뙣륍꯰뤂ꟈ㩠⼻뗳㮸⩯⬂㣡⶜롋ᷬ렶뛛ꭰ럇룹갪뢞鲎㊪⿭렪⿣⥴뇷㗆Ⱏ㈁뚿⥁눤깏꓅롿ꤛ뀵딇ꜱ⸚ꖿ㋖⪹넰㚞⨖돮닆⯃던될ꊥ닔₯댉담㒙㐜룑㓺㆝⯷㡀ㆮ꿓ⴏ뇁떠Ⲵ돋릳⾟뗍땉㋦⵩롥㋒キ⩀㜓Ⱉ땵⤈✜뚗긽╏맓꾯둪ꠝꫠ鵠ꍚⱥꧠꪶ⺸ꦒ⪽⼦⸸麛ꄼꧺ겝ᑥꙝ롗》놣꥝㌁⯷⺼ヿ⧅ꦛ딤꫚㢻둎㨊겖듏◓땕눝룆넲⋾렪룁녲뫏뤵㷆㲏㴤㶩㲺㱝㱡㮓볔㛄볔뵇㐮뗛놠㉇゜⺪ㄯ떠㨡㪸㧌㛳㛋㔳‥몕뇫뀠ℏ뇊꼄끽댛✷멃닦㲷띸㫆꧿뚕■맺됵뢯땋〷꿃벺➤묫굓됗⯱⿌⏃ꕎꓮ낽놎ꦀ댟⥭꫗넍ⶹ꿫뗢ꠖ밑㴳밽Ꞓ㴣닑㡷㪬㟲볦㞶뱬볳㓿뵒뫜ㆌ므롞㚽뢛⻙㔕ꆱ㗺㍡㔝맒㒠먣룡㍟뤮뢳モ렷궒괹놏ⱂ닌㑄闰뀰굺뛔ꨨ둳껾굲꺭꧔냣땋饞⼠韉뒜깽되ꪣ꯯ꑜ껉Ⲿ긛⯜⺥ꢎꭗ〗ꌔ⏬각⳸ㆢⳈ㈠⹔ഠ⦠㩥뒚㩟㯻렲㬀㧘뙢㦴랰륦롔⸫듳㊋㒨뙍㊬㕆㤸똚Ɱ㧫룲닒곧뛻괾록귦ꪃ⸑딝꼲ゥ뜣㨾㝭㮁㢋렯㨥㞯ꟹ㣏뇬멳롏㍅ꢃ㐶꒽뒤ⳳ㯛뇅㳓㰦먰㲓㫧렽㮿렡뮿려㎹뚱㇭㓋랆㒕㝮㐋㔱㌌㒞ꅖ㒙ぐ⦘넳⾑겼꡶➔넠끺ꏲ걾레㡊넶㉝㡲ꯚ⧢㈤묏ꢒぺ떸꺛ォ떋놠㌮떭㖌㗑㙴㌖㗔⿳㒻㐞㟗꽂ヾ눾눭セ냄꿞⌡꿫៬㥾뤲㎱あ⻩ⳉ㍈⥃깱゠⯲둥ㅽ뉿녶겴렢Ⱇ똙⳶⏺릜⡇⨪Ⱟ鬠⭚⽳⻹ⴆ㌂돭둧⽾롘뙥⹸멮ꨏ⣲멯꽷⩶Ꚙꡱ㥙릘ら㲁륮㎃㨢댇뚐돿룹렗꽕뤖릳꼇몯밾㪑뱌묣㭉뭍㚡㱫뒣믺㓓벢밼㠃뱿밦㑢벺뤾㪖믋렣㴃먖㒦㯟걩밉㒓밼묳㒝뮎믆ㅧ뱂띿㣃먴럸㴉뫅⣱㪭⵸멐낕뭚뢐굀륍묌⥺뭋듆㠍릅㕡㞑먫ꠖ㥷귪넷꽱뚼맠℗릉롩궗묇넭㢰름⪚㢙먑ꚸ㦢Ṧ룔뀡럟맹㎼뭙렂鿟묎㙲㢭멅㓸㠷멛㉩㥚㇘딹땚맻랧⥩릣렠✍뭭룚㑩륻㒭㕆먟㈥㡚ⷕ뮚⨿랶멦⼆몛럐ꎐ뱓ⲇ갤☒ꥣ궑꬧갆⥇ⱒ┏ヱ⮄鬙⎻⣨듧ꄱ귟뀰テ鱎녲ㆈㆸ⻳⺥ㄇ⬹ꑭ꼽㍦ⳁ㊗녢꾲낁⫛놤뒘㌕닑뒀끾딥룊㉖㸹㴯뎀㼶㳤㲧㲭㩲맾뵡붂뫱뀏떣뱠㒮⭱뗧㮟뗙Ⳓ㫻㜼㛀㟢㓙몢ㄐ멘륧㛇밸볠㖾렲㌈녍⫑㛙ⲭ␕ꡌ꠿궞널둬닛ꡆ귒뉩뒑둒뒡겈냍ꪝ⿕눳놖ꥱ꿦덼꺥꼰Ꝫⴢ냃겺갬꾬나밃㳩몉뢲㰹㠉㛭㧫㡋볲㓗뵊뱟㍸봥밊⺳뜲ⶬ뒘⸙㏊㑤⣊〷㒱㇎〉뚩꫞Ⱕ땭끲낇귀땖⡾꺘꠹㉷걌궚꡸꣊꽚눳뇧ꑩ⬔낶굻긮낭뇕ꡦ굲⠚긮⹋굫⃟⪈᱇⾕⶧⿚Ⲭ⪉ⵢ⡷꣮ꕆ륡㌸롿갓㒙꘡⡜㆗⒲㘍뒓과㝨㒮㚐⌞녢⠴뜝듍⨆꼤„鲷뙈력땫볊㹪뫐㱾㸁㳑㰊㰡㲌뻝㲾빰뺇㪤뻥떽㟎똢롮㏃녀㩊㇧㱩㢱㈝㨟뫕㟯뱻뢠㡸멼론㚅뤼⪭묶べ㰤듮㮀⹎딛⻿멯ꙹ뢗듦㉦뒉뮃鶹뫿놱ⴅ뗈⭀걂ᩥ⫯끠⨥롆〤륃뤕ꛌ뙴륗꡿떍⸖됼㋏㫽㦣㪊㤏㣅㤺鵬룻㓲럎묚딚Ჭ뙤ꃟ뎫㐃뚢㓣ㅃ⥯㓌㋋㗣덩ⵎ뗗딚Ⲥ럈넦꠩딼꩔꼂갟ㅺ됰㒆ꭦ꽳ꤲ둨꧒녕ꚭ긢ꓞ겍넀땲⎶Ⰽ곕ꢀ⭀꿁ꓶ꼐鮸ㅐも㋛⸣ⳓ⼑㊶㉝「ツꤠꮐㅯ┥꠼㇊⯑✹㒉㠥㢄㉾㖁㞪㉟㑼㞪ⶰ㍻뤳☲ぞ먱⣰⺚렑㖲㡊⓫㔱㛎㣩㈿㍨㔺⤙⽐뚦⸺㎩띤゠㐒르㖬㢈㡑㙀㚍㠷㒸㠴㛇⺂〴Ⲱ⴦⸲⻓㇖ぎ⻄괐㝿㑡㌯㛎㋍㔈㙸㐧ㅒ룄ⶇⲴ롙⩝⵿둝⾵⽌ⰻ끂゠ⳁㅿㅚꩅ⠻ꛄ〮렼ꌇ⿚떼⣯げ뀲㖦땈㙊㎡㒉㚝へㄎ㎓⭇띦⻻⠅룳㉗⩗댞ャ⾏㋤ⶏ⬰뎱ㄨ錽㌰㏑⇶뗋Ⲑ궗뤞Ⲣ鵛똄⿺ゝ됑㚵⻃㒈㖙ぶㆥぎⰅ띊멋⦏땣딯ꆇ넸랱거⦧⪮ꆜ궳ꂌꢍⰍ࿋ㅌⷣ둾⟡⦵끉괫괄ⵂ鵪ㅎꧦ℡⹐鵴⢑꼽ᣨ멆㶴뭾뤻㵪렜ㅟ㭙け묁ㆲ림뤅㕍롭먠⓬먐똪㦳뙺딶㢠돪떡㙁だ룺넝뗽늹ꄘ덬뗶꠼딌떓㲃띦뜽㯟롮ꌀ㤹⤪뮱등맅뚎㎽뗎롚⡎뜽랳㺜맻릕㵴뢷⹽㬚ᯫ목끶뮤루㚦뢼맜⭛맘㈴ꭺ㐻⸆괚⭬괹〆㓶괮뗏⺰ꠏ냒⡌늰끎ꔤ✵㑮Ọ궹ⱀ⺶㇧ⴺ㔯㉨둲⿑ず냁⡅ꭡ돆괨⹓㐇㓭⫪ꥂ㈾㈦⠢⨼㑕뛸⴪ㆤ듭ⵓⵕ됙⡓㔡㇟ニ⺖㏸シ໨ぁ㊡Ⲑ덭ㄶ㈞긭〃꣨ꭑ괴꫻꺟ꭤ궈ꎃꮟ늘걚겮갲╱궙⍴ꌻ鉛냌ꑎ궟놲ꭣ뎙㟀㟑㘯㜥㟌㚩㟂㛎㝜ㆧ㍈㋾㐮ㇿぉ㐝㉦㎖〳㊪㌕ⷚ⿦㈆ㅛㆬⴙ㐏㎌ㄗ゙㍅㍃ㅭ㕑ㆺ갴⠜갘괦ꃛ颕ꨩᾥꗁ㛧㟈㚷㙁㠘㜷㚃㚲㛶㓛㠰㛬㛶㜢㓷㝖㛭㛓Ⱶꦁ⑂ꑲ⻦꫘Ɱ⹐⩃⤯▙껚ꍸꕻꦰ뉀곩꾚⫐⧗ᗣ釘껭낖냯끖괰㗡㝢㑞㎙㖅㔼㘪㙻㎕㏭㊄㓨㒟㒧㓤㑀㓪㑥듸돂땝듕녖뒅떽뙏뚌㈌㎻㊒㈂㔁㆐リ㉞㊦㔠㙻ホㆢ㘸㈜㇀㒷㊵⿷⌆ᨫ⸝ꆕ⺉껵ꬊ꿞㋡㌜㒿㖰㐫㎠㏔㋴㏩떃ꪌ넧뒌ꪦ눪땲늚뉤㌝㔥㗕㈗ㆄ㔬ル㌜㆛㑿ㄎ⺣㊷㐸ㄬ⫀。ⱺ㓦㙜㗲㙬㗘㖴㔔㔋㕸㈡⿫お⳾ㄩ⬨㈜㑍き뒹댈뒣둈뗗뚍랠땢떯⢸⺆ⓙⷜゕ⚃えⴒ⛺㔷㙹㊻㇈㑍㌾㔛㘎㇜ズ㌆㍟㍥㐎ㆶ㏟㍃㇜⟩〬㌂マじソ㆕ㄷⵅ⸉ゅポⲾ㈦⿜㐗㒬㌄ツゕㄺⰬ⽿⺴⺿ⱔ㄄㈈⼳ざⶄゝ⸙ㆶヹ➾㛒㢬㟶㝡㠴㛩㜮㠟㠐㌈㓞㇐㉊㒘㎠㊮㐩㉔⫁㈭〻㉢㐒ヰ㇦⽍㊆⒀꯶뇄꺦꼐뀝뇯깼늒ⷙ⹎✛Ⓧⴱ⧈ㆌ㋷ꠟ⢡ꂨ군ꛭꣻ꾢낪궄ꃟ㙄㘡㏯㐙㖷㐭㚐㝦㑫㑁㉣〛㋣㈾㍋㒘㋃ぱはⲹ㋐⩰㈍⩡⭱ㅍㆫ㌰㓏ヹズ㎶んズなⶇ⳧鮽ⴙ⌛⭍ⴽꪊῨ駾㕜㑌㕚㌣㖅㕞㐵㔕㏟㜻㠟㙜㔌㙿㜼㝁㡈㔖ゕ⹷わマ⼽⠘㄃⭗⿋괌⥹ꦯꫵ⳵꩸꺽⵨⡋꤬굅굽귚ⓑ꾧끥넦눡㓪㗕㋞㊄㘃㉌㉾㐄ㅇ⨉ⵦⰰꙄ⺹걮껸ꪶ넗㒬㖤㕎㖥㚒㑏㝄㛃㙎㑜㊋㊺㐎㖨㊂㑬㉀ㄭ㋧ㆋ㏦な〝㎅㍼㈬⽉㄂㈲㐃㆟㐸㏃㈢ㆭ㐯⵾ṷ⸛⤶✴ꯂꐙꭎⰁ㘖㕜㙍㐁㖪㋏㑦㗖㓧㚃㝎㟝㞎㠜㘒㜇㠠㠰⸿ぇ⹒〺⾧ㅦ「⩥ⵡ꯼ᶉ⪲ℑ⤲⻨괼默ꑧꡡꕱ┱깐귓귃꣘곱껣㑤㙅㐻㓹㑺⻵㉤㔹㐢ꅽイ␢髠⥛ꤻ⤖ᵒᱥ㐵㘠㕑㚓㝶㕆㏭㛧㐗ェ㔐㇤㓇㍵㐱㑉㋚㒁〠㍣㎦ⶱ〃㋆ヵㅺ㍈⽿㌽㍷㊋ㅳ㈝ⴕ㆚㌦Ⓥ❰⡌⪁魩꩟굒ⱍ꧲㑤㓗㉇㊣㌠ㆴ㌁㉣〔㟓㠥㘩㚂㢔㕤㗵㜸㚾⸴Ⱍㄩ▶㋿⟙Ⲯウ⬇ぐ⦮⩉ꯤ⹲⽼ⰱⱙ꘎⡨꣄ꡋ끤꾚닀꩸꽅됎㎧㏒ㆲ㏠㋔ㅱプ㑪Ⱉⱻㆧ⿖Ⱀ〾⺰♋ⱛ黽㚦㞵㐧㔜㖁㕌㏠㜖㒍㒏㕮㋽㒝㕪ㆩ㕹㘳㔦〹㍉⽊⻭㌲㊄㊐⼣㐬《㓬㏹㊽㔭㍞㍆㉟㑓⽰ㅵ⫪ゖ㇠ꇰア➫鲰〹㒭ほモ㈩㊨ⷲ㈢㆑㞆㟎㠀㙖㜟㜰㛞㣟㚽ぴ㌚ⰿ㋼ぃ⿏⶙ス⼔Ꮧㄻ⸫ダㅮㄏ⯢゘ズ᳌끹끯뇻귱놾꧑꿧눣ぅㄬオぼ㆐㇇ぺㆫ㈦ꢭ겉ꬄꦑ뀗긦꿗길겺㗘㢊㗂㖉㢞㗕㝔㢴㖡⹇㒌㒊ㅳ㔂㍑㍊㈰㒡ㇾ㋫㎺⭪㊒⾶ㄗ㎍㌳㆓ㄡ⾃ㅖ㍁⹖㐚㐭⾋ꐈ⟯껻ꋔ굓ꦡờ⎞ꩺ㚐㠋㝵㛖㜠㚪㗏㙱㓢㒗㙳㝵㘙㗲㔲㙹㜺㗛ꟓ⡥⑒⎕⽕ⰷⴀ⧛꫟겳ꧨẎ꺽ꨃ낀곀᳍鱺괷⏇ꭖ뀑긔ꣿ녗ꐯ껓㙯㞻㗞㙔㝇㐩㓯㙆㖺㛂㞅㔌㕕㔭㙀㑽㖊㙴ㄇ㆓⦍⨬㑌鹖㉘㑂ⶖ㗃㠨㛻㜑㠛㔟㚗㟊㔖㎲ㄹ㊫㋸㇘㌤ㅦ㐢㖖けわネⰼ⿌⽼㉙⾅〬㘡㘘㙤㚜㛽㞨㝒㠜㜩뀘곅냁꺫꿓넡낞뎥늃㥋㨬㤳㦀㥫㣫㧵㥭㦟㡛㢰㠖㗼㠟㛴㛺㢖㛹㖨㙼㟆㚛㢅㜋㔠㟳㠑괟꽳둊눲꽷뒢뀦댼덮곏껡놚ꡋ걥꾸냛ꃼ눍뇷낒뉣됔됡돝똎듀뛨㤈㦙㢙㡃㠾㢩㥜㨆㢾㒉㖗㓒㈙ㄡ㓼㑙㕤㎶㈤㏙㉞㌆ㅭ㓃㑙㊙㔤ㅜ㐠㉐⻼㑉㑭㊘㑍㐴뉫댰둪둳돡눣뉖뒧돨㦃㣡㣖㥋㦍㣭㠰㥎㟬㚹㛔㛔㐳㗫㙾㜴㚬㟉뇑놔놗냤네꺨됔늋듯껰냏늊둤덀뇜떫뎦듖갹ᰅꑨ⟇ᾟ◬ꙶ꺒꽛㟜㣯㞿㡤㢚㚰㝊㞲㛻ꩽꘕ꒿닋깐뎑눷뀫듚㠕㝬㞧㚄㡠㞲㙤㞹㜭ⷿ㓀け㐭ㅤエ㎚㆖㐋㈫㐧⫗〱ぁㅀⶼ㊇ぷ⾉ㄐ㌶〦㔎ㇼㆢ㌊キ뀱뀾꤂뇯꠿녁굵꣩닾㠩㟢㖸㝒㠮㟋㔭㡉㝅㜔㞩㓐㚘㠏㓓㔽㠇㚲➀ⶨ굻ꮤꦌ霃꤅껼깉녥귩➖뇏ꑑ꿖늞꽱괎꧋ⵗᦊ鷬뀜냀눭꽄놗㙅㟧㖩㒛㕚㗧㗫㛐㏨곩ᬹ넟ꮚꛖ뉺덦녷늖㛃㤑㕼㙞㟖㝜㞯㡅㠯⽺㓃まㆀㅶ〔⻈㓇ㄣ㋀ヰィㅙ㑧ヂ㍈㍑㍲㎓㓣ㅁノ㓪㎮ョ㓟〇꒿ꪔ넑냎끎깪눒뀃넟㚬㡐㞯㟂㢰㙡㟨㟻㘕㔰㟚㘸㖳㚠㚍㗭㘏㖭ꥦꇒ귆꼉⽛뀄⡼귒귞꾉꧰ꘪ걍깽낾끄껂낅⟶ꑚ긘ꘊꬥ낭냥꾯꧷㛅㠙㘏㔵㠹㕓㗫㡃㓂꿴㒟놆뀩놲낻눏납뒆ꤑ껇꿛갈꯷♗⎴걛⳩⧆ꨎ⣾겧ꯄꓓ꾊뀐국ꏂ깾굛꓇꛷᧦ᕏ℞駸ꂣꎏ⚒⭄ꭅꮼ⠠곿⫆⡟ᴜꩵ骃꿘낂ⴿ긠귵꬜ꉚ᥇ꬶꓯꡊⲈⳆ⟉ꉼⰅ굼➪걜⯏긟♱궽⦌ἽⲜ굁⫐곌갫✶ꛮ궏귕ꡥ␼Ⱳ⫿ꂧ걷긨Ⲓ갗ꛨ궊계ℴꡐᢘꁶ鴢ꛔꈉ⠽⯇Ⲍ궡귍ꘟꒈ⦞⴯♛겁ꙵⰦ⡁괆ῒ깿ꪞ㚇㗜㚈㏿㞦㘊㙯㛭㖏몭몌몶멂뫸몸믴뫘뭛◿㆚⡓Ⱨ⬵ヌ᭖ㅅゖ㍸㛟㙙㍇㗅㔷㘦㜩㍰㍂㑣㑳㑄㓱㗞㌧㒗㎢ㆵ㙘㐠㈬㏬ゼ㈗㖐㒩ㇸ㇬⵰⢙㐁◲々ㄳ㊙㝑㜀㕙㙴㘼㙘㜋㡍㙰닲놼댇뇁눟녺돱늬꿇㪜㭮㫿㦛㩸㦤㫂㭦㫦㙸㠎㢚㘗㞘㠃㘨㠴㠫㢼㢱㛲㡳㥫㛽㠘㡆㞏ꞼⒷ⟆끑ꢾ☏꫆ꩇ겔⧀꽿꼞╯꼱᪌꺰갚귦∞⡎鶕ꬳꩊꭃ궙⑚ꠂꍴ꘩Ქ꾲♏껭꛹ᵯ鴩顁齯ꟓ꾪깫⠹ⅻ긘ἁ꼩⦰ꢶ⒓⫴⨻↝ᯚῈ낋꩘ꗍ鰷꫱☸☥⑌⚪긲␴ꡨ⢬ꇥꔿ꿔Ⱶ꛴ᷮ긠곛갍깣ₚꧽꬤ⥝≫꽏괠➂겐⏶ꮳ閟ꯌꯁ꬯꨿격ᲊ⥽꣄꘵⮍鰹꧶궛ꞣ␺귔꿲ꂔⳭꞑ⬞◆ꠜ⣽〕〛➃⬳궓ⴥ걚ꫠ鉝⏼⯽ᬨ╡㐐㌥㈄㇉㌧㍯㍊㆜㑋ㅭル㉒⽓㌫✉㉜㐞⤼⨦ョⲋヴ㋽⼾㉛㇍⹘⧔㋞⣚⿤㈯Ⲃー㉮㈢㋘⿘⴩⽏》ㆴ⶚⿡ㆬす⹭꘵ⶏㆅるⵛ㈤ꑓ㖝㏈㈡㔫㑶㔀㈌㘁㖍끩ꠀ뎘눭군꾠댃꨹뉬ꨪⳈ곛궲ⵕ➐⤌⥬걹갨ㅋ걠⣗サⵦ꨼⥗꯼⾉␏ꝳꐀ㆜⼸⿛㇊⏬≾⤯⻛⹓⽒駾ⵡ⭪ⰼ㒲㗛㍐㘥㔆㕣㘐㙻㘨㌘㒵㈙㌁ㆁ⻵㋢㐅ㅧ⺊㐸㏩ㄼ㏜㌄ⷒ㌅ポㅿ㕢㔞㍋㕄ィ㎵㑍ㅣ㒙㖒㑂㓔㖶㖟㑫㔰㑜⵿Ⰻ⮘꒗⩂ꔟグⲴⰹ㞴㣔㠥㙛㣛㘌㟖㣦㢃뛅뚋떳땪뚆뛏띝듮똏鍜ꚾ끶깜괎괢ꊀ겛ꩧ냬갽꿆꺉ꬾ꩎ឣ⳺ꕩꋔꟘ⪓걖⬪껍굡≗꫐꠴⎿ꖝⅽ꾄껟꽣꿐곲ꓪⲇ걵⤀⛒⢻Ⅴꫩ⓼ꬱ겎⳨ⱡ굅Ⱔ⁖⇋ⴊꕴꩩ☙ꤑꣳ갋⴮⊙ꮡꊴ鵯거␊궓ꑮꞓꪧⳙ⍊⠰곲᥮≑⢛ꤹꠛ귢⨐⫏⡁⤄⨛ⴛ▗♗⒘걾ẩ↝글ꨑ괊괎⨌⬔ꘙⒷꡉ⃍꜓ꯡꪔⵝ᳣갓锰껦ꅬ⯔镁⇯꺖꟧⥚꨽궁⩲ꂐꆮ껁ꎺ꿱〧㎫┰〥ⰷㆃぬㅻⱽ갔꼘굼ꎈ뀝뀊곟ꦊ깮㍨㇄㆐㈬ㆹ㊑㊷㏘⿢㍷㄃㇅ㄟ㑐Ⰰㅙヿ㉴㇆　どㅊ㍤㌎㈑㋩㈱⫥㑕ぇ㈜㌱〵㋒ヌㅾ㆖ナㅌ⟕〔ㅸ㇀ⷢㄪつ㑿➪⸵⿥⭅⽃⹒㉤⾥㎖⸀ゲ〕㌙㍔㑞⼾ㆦ⺺ㄣⷯⷵ⛦❱㊍ⵒ⵳㒄⹏ムㄸㆤㄙ㐈ヱ⼕㒓『Ⱞ㌌⼱Ⰾ㋠⽁⿽㇀⿶⽼㍂㐄⻰㇑⺌ꛍ╪굌╿겐ⲧ᥽≵ꦜ㉥㒼⹯〕㒧みⶻ㉚ㅵㆦ㉯は㋉㔯㑱㇭㓣⿯㎴㖅㎲゛㔬ⷝㆼ㍞⸜〾㈙㌂ㆆ㓂ㅆ㑒㑤㑩㌨】⵭㉖㒱⯪テㆍ㎾㒉㕺㐑㈅㖩ビ㋔㐇ちⰳ㉙⻼⦑ㄏ〪⧛㎅⣕㑅㐹㇣㏗㚷㖡㓝㚻㌖㑝㖑〠ち㔧㌗ヿ㋦㒮㎾㓷ア㓰㎦㑡㌰㔘㒢ꤾ겂⣹궮鵼갬꽦ꮗ굧㢚㪑㥘㨌㤟㪟㦐㤗㩂⩃ꌨⶁ⽇⍀《ꡲ뀌긿⨯괇┊껋ꤸ깫걯갾겐ꡨ꺔곓갠ⱍ괜곚ꭡ겭걑ꐌꡬⶽꀚ鈽⟂걏ꖱ⎈⳹⠫☌✘ꮁꜬꛙ끦굌⁇걒ꦱꭄ꼙곀뉀눜グꀳⲇ⺫⢫へꪑ꟤ꇷꖎ귲댈괤내갛넼딄낷ꮴ긣ꪰ⫆꩎녜녫귤뇛ꩍ꾬굹굽ꕬ⁜갬껽뀬⫊⨞ꭏ⸠ꡚ⾹ꡛ鰅ꅪ㢳㞫㚹㢅㡵㞆㠖㞚㟠⾂〿ㅅカ㊶ㆵⶖⲬ⮟饮ꯋ⵻⊑⻹㄄ꔒ꠷꣦⣕Ⱛ⡰って⑵ⰲ꧝Ứ〬⬮⣯㉃◹⸼〲ァㅪⷱ『⦯〴ⷱㅪ⦑⶞ぎⴓ黴⾬ꢤ⎿ⷔ⟨⒥곅㕊㆕㏒㓟㉦㍦㈣㔛㒛뒶됺듷뒛둔뒐놉듄둮뀹꩜꥔ꮕ⥻ⴏ꺼ꗕ결굴Ꝿꨑ⹷⪥ꨡ꬀늢걸귪귖ꮨ뀡꼈⳰⁣⥏겝걺꯸ꆸ관ḁ굚ᩒⳁꃎ⧦귢ꢄ꺌꧴ꭗ긱끶걚ᱼ꜃꺤⤅⦯걬꟒▕꽐귿Ⱜ⏫錤ꬋ₃║걏ꩆ꼶꤬⥁꽧꧈⧍갊겣ꨕꬽꍙ꼼ꂠ걠ꦛꪷ≨깛긍강꬛계⭊겘긊꽲곴ꙕ괚ꐻ鬻ꪂ⊮ꢃ⢷ꩡ⵮Ⲇ⡅ⓕ굙갹鲴ᘐⴎꓝꙸⱦꖈ⯕갷괤ꤹ➪굔ꛤⳒႡ⫊꠱기꺻갲ೞヸ⺯⅐㈹⺱⽒ⲟパ놏ꉭ놋겘뀈걍뉼꾰녁⨇ⷹⵜ⽈㊟ⷮ㉶⸧Ⰻ⶷⽾〼㇑ㅍ┆ⱃ⾳⓻⾳㊯゗ⶣ⽬⩊ⶂ㋘⪖⼂Ⰺⲿ⹢⮱〃⒤⹠ㆈㆎ⼇⓻Ჶⷷ⣱ㄻ⪕ネ⸣ⶃㄫ〣⼍␚⬦ユㆎⰢⶥᶢヿ㌏ㅇ⧖㉓Ⲳ⴨〮ꢨ⪈⼒꒰⿾ヺ〚ⴏⱣ⢗⼳⹕ㄷㅜぉ⑄⾖㋁ⱛ❪⹴⺦⒋㇧ゅꨂ덾깰꼔뒈Ḍ꺘꧋㍼㖆겺ꐵ굣㠴꾍⼔⯈끅㔍㙜뜯땯놑ョ㞟㤿꾄뒊㕂딗⡮뤲チ낽⣅ⱌ곊닄㚄㣍겓뗗㥙먫깔렳뫛맥ⵉ㍍곲ぷョㅬ둊넶낄꬜麞ᬥ륋ㆮ꼞㭢㟏㫛뎤롡ゃ낁⡬늩ⱴ뒾㔂㐆껰뙇늢돔ㅸ㇕Ⲉ㈠꨻ⶐ㉀긚ꖩ㈂⒃Ẕ╩냕ᆊꣳ릏㉹뎑㧐㠉㭸늙렺둗㖡㡹닷㈭ㅜ놓⯿ㆲ㒡㎬ㆶ㦇㖲ㆧ㛛ㄻㅼꄗ듾뗇㍪굣겨㋳깼눨Ⳕ㡴곭띲볳눇봧Ⲕꗋꥆ㌙끅㊉깪걙⨆ꧏ㈊뀴뱘♉궞㳠㝩㳝까㐾돾똤뙋녛ꖓꥩ긘냹㊞꽳㝏㘧롞뤬᥺롣㋨鲣Ⲡ⫥뀗ㆈ닶꽌꾣⫝⸭ㆵ륓㍢륽㱲㔶㴜ꮲ〪⨋㏐㜙⧖㒻Ⲳ레꺴ⴽ㓑㥥뀝㡪뢉㚞냖꛴됶⸞뒭깑뉉땿늏㙶㚽ザ뒟몑⯹뒙㞯㑰㚊⼯ㅲ⯳걽늑녚꺆Ꞻ⤴괻ㄆ꿆뢜ྲྀ높㥪㗺㟞긙냘냀뚶뀟깰뉁뗪㈧㗖ⴃ냑㱸㌑뗯벡뱜벵녾㍴뉕낇⨻㋭됊뉕눹Ⱑ⺆き멁Ɱ⾩㭩㢁㫷⻾㣭ꭢ㚅ⶰⱛ㛧㘛뗆뢣⢔㌩㑥㡴뚐뤩꾵뒤ꢥ묐노鴲ꗜ끿띬ꞯ㥘㢜낷뇝ⲝ닕₪띑늽ꡐ㊐〨㈂겺늝⹾ㇻヷ늽겡덽㈑뤃〴⩼㤎㋁㨇を릝ⶠ⪓뇚뎻ၯ뙬㓣㠊ꎂ걕㏃뀴㞇떣⒁늠녊㊀ㄦ⺭꫃ⶻ긕ꮦ뇸シㅨ냋릐ꚍ곇㭵됝㥈⛍㌖꿳ⴃ⥱떃㨦㉗㘵㇚똯㒖㕝㊒꾤됉ꖝ⹲꿤Ⳏ꾓⥖냁㌴Ꚉ⠌⳹괂ⱀ귵⤼닂ꌜ㼉㟐둳㔄걤ⓒ";
    fn f16_to_f32(bits: u16) -> f32 {
     let s = ((bits >> 15) & 1) as u32;
     let e = ((bits >> 10) & 0x1F) as u32;
     let m = (bits & 0x3FF) as u32;
     if e == 0 {
      if m == 0 { return f32::from_bits(s << 31); }
      let mut mm = m;
      let mut ee = 1u32;
      while mm & 0x400 == 0 { mm <<= 1; ee += 1; }
      return f32::from_bits((s << 31) | ((127 - 15 + 1 - ee) << 23) | ((mm & 0x3FF) << 13));
     }
     if e == 31 { return f32::from_bits((s << 31) | (0xFF << 23) | (m << 13)); }
     f32::from_bits((s << 31) | ((e - 15 + 127) << 23) | (m << 13))
    }
    fn decode_f16_weights(input: &str) -> Vec<f32> {
     let mut out = Vec::with_capacity(input.len());
     let mut it = input.chars();
     while let Some(ch) = it.next() {
      let v = ch as u32;
      let u16v = if v == 0xFFFF {
       let off = it.next().unwrap_or('\x00') as u32;
       if off == 0x0800 { 0xFFFFu16 } else { (0xD800 + off) as u16 }
      } else {
       v as u16
      };
      out.push(f16_to_f32(u16v));
     }
     out
    }
    fn get_embedded_model() -> &'static TinyHybridWeights {
     static MODEL: OnceLock<TinyHybridWeights> = OnceLock::new();
     MODEL.get_or_init(|| {
      let all = decode_f16_weights(WEIGHTS_F16);
      let mut off: usize = 0;
      let take = |all: &[f32], off: &mut usize, n: usize| -> Vec<f32> {
       let s = all[*off..*off+n].to_vec(); *off += n; s
      };
     let conv1_w = take(&all, &mut off, CONV1_W_LEN);
   let conv1_b = take(&all, &mut off, CONV1_B_LEN);
   let conv2_w = take(&all, &mut off, CONV2_W_LEN);
   let conv2_b = take(&all, &mut off, CONV2_B_LEN);
   let conv3_w = take(&all, &mut off, CONV3_W_LEN);
   let conv3_b = take(&all, &mut off, CONV3_B_LEN);
   let policy_w = take(&all, &mut off, POLICY_W_LEN);
   let policy_b = take(&all, &mut off, POLICY_B_LEN);
   let value_w = take(&all, &mut off, VALUE_W_LEN);
   let value_b = take(&all, &mut off, VALUE_B_LEN);
      TinyHybridWeights {
       input_channels: 19,
       scalar_features: 6,
       conv1: ConvLayer {
   out_channels: 12,
   in_channels: 19,
   kernel_size: 3,
   weights: Vec::leak(conv1_w),
   bias: Vec::leak(conv1_b),
  },
       conv2: ConvLayer {
   out_channels: 12,
   in_channels: 12,
   kernel_size: 3,
   weights: Vec::leak(conv2_w),
   bias: Vec::leak(conv2_b),
  },
       conv3: Some(ConvLayer {
   out_channels: 12,
   in_channels: 12,
   kernel_size: 3,
   weights: Vec::leak(conv3_w),
   bias: Vec::leak(conv3_b),
  }),
       policy: LinearLayer {
   out_features: 20,
   in_features: 18,
   weights: Vec::leak(policy_w),
   bias: Vec::leak(policy_b),
  },
       value: LinearLayer {
   out_features: 1,
   in_features: 18,
   weights: Vec::leak(value_w),
   bias: Vec::leak(value_b),
  },
      }
     })
    }
    pub fn predict(state: &GameState, owner: usize, config: &BotConfig) -> Option<HybridPrediction> {
     let hybrid = config.hybrid.as_ref()?;
     if !hybrid.is_enabled() {
      return None;
     }
     let model = get_embedded_model();
     let encoded = encode_hybrid_position(state, owner);
     if encoded.grid.len() != model.input_channels || encoded.scalars.len() != model.scalar_features {
      return None;
     }
     Some(model.forward(&encoded.grid, &encoded.scalars))
    }
    pub fn leaf_bonus(prediction: &HybridPrediction, hybrid: &HybridConfig) -> f64 {
     f64::from(prediction.value) * hybrid.value_scale
    }
    impl TinyHybridWeights {
     fn forward(&self, grid: &[Vec<Vec<f32>>], scalars: &[f32]) -> HybridPrediction {
      let height = grid.first().map(|channel| channel.len()).unwrap_or(0);
      let width = grid
       .first()
       .and_then(|channel| channel.first())
       .map(|row| row.len())
       .unwrap_or(0);
      let mut flat_input = Vec::with_capacity(grid.len() * height * width);
      for channel in grid {
       for row in channel {
        flat_input.extend_from_slice(row);
       }
      }
      let conv1_out = self.conv1.forward_flat(&flat_input, height, width);
      let conv2_out = self.conv2.forward_flat(&conv1_out, height, width);
      let (last_out, last_channels) = match self.conv3 {
       Some(ref conv3) => (conv3.forward_flat(&conv2_out, height, width), conv3.out_channels),
       None => (conv2_out, self.conv2.out_channels),
      };
      let pooled = global_average_pool_flat(&last_out, last_channels, height, width);
      let mut features = pooled;
      features.extend_from_slice(scalars);
      let policy_logits = self.policy.forward(&features);
      let value = self
       .value
       .forward(&features)
       .first()
       .copied()
       .unwrap_or_default()
       .tanh();
      HybridPrediction {
       policy_logits,
       value,
      }
     }
    }
    impl ConvLayer {
     fn forward_flat(
      &self,
      input: &[f32],
      height: usize,
      width: usize,
     ) -> Vec<f32> {
      let hw = height * width;
      let mut output = vec![0.0_f32; self.out_channels * hw];
      if self.kernel_size == 1 {
       for oc in 0..self.out_channels {
        let out_base = oc * hw;
        let bias_val = self.bias[oc];
        for i in 0..hw {
         let mut acc = bias_val;
         for ic in 0..self.in_channels {
          acc += input[ic * hw + i] * self.weights[oc * self.in_channels + ic];
         }
         output[out_base + i] = acc.max(0.0);
        }
       }
      } else if self.kernel_size == 3 {
       for oc in 0..self.out_channels {
        let bias_val = self.bias[oc];
        let out_base = oc * hw;
        for i in 0..hw {
         output[out_base + i] = bias_val;
        }
       }
       for oc in 0..self.out_channels {
        let out_base = oc * hw;
        for ic in 0..self.in_channels {
         let in_base = ic * hw;
         let w_base = (oc * self.in_channels + ic) * 9;
         let w = &self.weights[w_base..w_base + 9];
         for y in 1..height - 1 {
          let row_off = y * width;
          for x in 1..width - 1 {
           let out_idx = out_base + row_off + x;
           let mut acc = 0.0_f32;
           acc += input[in_base + (y - 1) * width + (x - 1)] * w[0];
           acc += input[in_base + (y - 1) * width + x] * w[1];
           acc += input[in_base + (y - 1) * width + (x + 1)] * w[2];
           acc += input[in_base + y * width + (x - 1)] * w[3];
           acc += input[in_base + y * width + x] * w[4];
           acc += input[in_base + y * width + (x + 1)] * w[5];
           acc += input[in_base + (y + 1) * width + (x - 1)] * w[6];
           acc += input[in_base + (y + 1) * width + x] * w[7];
           acc += input[in_base + (y + 1) * width + (x + 1)] * w[8];
           output[out_idx] += acc;
          }
         }
         for y in [0, height - 1] {
          for x in 0..width {
           let out_idx = out_base + y * width + x;
           for ky in 0..3usize {
            for kx in 0..3usize {
             let iy = y as isize + ky as isize - 1;
             let ix = x as isize + kx as isize - 1;
             if iy >= 0
              && ix >= 0
              && iy < height as isize
              && ix < width as isize
             {
              output[out_idx] += input
               [in_base + iy as usize * width + ix as usize]
               * w[ky * 3 + kx];
             }
            }
           }
          }
         }
         for y in 1..height - 1 {
          for x in [0, width - 1] {
           let out_idx = out_base + y * width + x;
           for ky in 0..3usize {
            for kx in 0..3usize {
             let iy = y as isize + ky as isize - 1;
             let ix = x as isize + kx as isize - 1;
             if iy >= 0
              && ix >= 0
              && iy < height as isize
              && ix < width as isize
             {
              output[out_idx] += input
               [in_base + iy as usize * width + ix as usize]
               * w[ky * 3 + kx];
             }
            }
           }
          }
         }
        }
       }
       for v in output.iter_mut() {
        *v = v.max(0.0);
       }
      } else {
       let pad = (self.kernel_size / 2) as isize;
       for oc in 0..self.out_channels {
        for y in 0..height {
         for x in 0..width {
          let mut acc = self.bias[oc];
          for ic in 0..self.in_channels {
           for ky in 0..self.kernel_size {
            for kx in 0..self.kernel_size {
             let iy = y as isize + ky as isize - pad;
             let ix = x as isize + kx as isize - pad;
             if iy < 0
              || ix < 0
              || iy >= height as isize
              || ix >= width as isize
             {
              continue;
             }
             let w_idx =
              ((oc * self.in_channels + ic) * self.kernel_size + ky)
               * self.kernel_size
               + kx;
             acc += input[ic * hw + iy as usize * width + ix as usize]
              * self.weights[w_idx];
            }
           }
          }
          output[oc * hw + y * width + x] = acc.max(0.0);
         }
        }
       }
      }
      output
     }
    }
    impl LinearLayer {
     fn forward(&self, input: &[f32]) -> Vec<f32> {
      let mut output = vec![0.0_f32; self.out_features];
      for (out_index, slot) in output.iter_mut().enumerate() {
       let mut acc = self.bias[out_index];
       let base = out_index * self.in_features;
       for in_index in 0..self.in_features {
        acc += input[in_index] * self.weights[base + in_index];
       }
       *slot = acc;
      }
      output
     }
    }
    fn global_average_pool_flat(input: &[f32], channels: usize, height: usize, width: usize) -> Vec<f32> {
     let hw = height * width;
     let norm = hw.max(1) as f32;
     (0..channels)
      .map(|c| {
       let start = c * hw;
       input[start..start + hw].iter().sum::<f32>() / norm
      })
      .collect()
    }
 }
 pub mod input {
  use std::collections::{BTreeMap, BTreeSet, VecDeque};
  use std::io::{self, BufRead};
  use crate::engine::{BirdState, Coord, GameState, Grid, TileType};
  #[derive(Clone, Debug)]
  pub struct FrameObservation {
   pub apples: Vec<Coord>,
   pub live_birds: BTreeMap<i32, Vec<Coord>>,
  }
  #[derive(Clone, Debug)]
  pub struct BotIo {
   pub player_index: usize,
   pub my_ids: Vec<i32>,
   pub opp_ids: Vec<i32>,
   pub template_grid: Grid,
  }
  impl BotIo {
   pub fn read(reader: &mut impl BufRead) -> io::Result<Self> {
    let player_index = read_line(reader)?.parse::<usize>().map_err(parse_err)?;
    let width = read_line(reader)?.parse::<i32>().map_err(parse_err)?;
    let height = read_line(reader)?.parse::<i32>().map_err(parse_err)?;
    let mut grid = Grid::new(width, height);
    for y in 0..height {
     let row = read_line(reader)?;
     for (x, ch) in row.chars().enumerate() {
      if ch == '#' {
       grid.set(Coord::new(x as i32, y), TileType::Wall);
      }
     }
    }
    let birds_per_player = read_line(reader)?.parse::<usize>().map_err(parse_err)?;
    let mut my_ids = Vec::with_capacity(birds_per_player);
    let mut opp_ids = Vec::with_capacity(birds_per_player);
    for _ in 0..birds_per_player {
     my_ids.push(read_line(reader)?.parse::<i32>().map_err(parse_err)?);
    }
    for _ in 0..birds_per_player {
     opp_ids.push(read_line(reader)?.parse::<i32>().map_err(parse_err)?);
    }
    Ok(Self {
     player_index,
     my_ids,
     opp_ids,
     template_grid: grid,
    })
   }
   pub fn read_frame(&self, reader: &mut impl BufRead) -> io::Result<Option<FrameObservation>> {
    let Some(apple_count_line) = read_optional_line(reader)? else {
     return Ok(None);
    };
    let apple_count = apple_count_line.parse::<usize>().map_err(parse_err)?;
    let mut apples = Vec::with_capacity(apple_count);
    for _ in 0..apple_count {
     apples.push(parse_space_coord(&read_line(reader)?)?);
    }
    let live_bird_count = read_line(reader)?.parse::<usize>().map_err(parse_err)?;
    let mut live_birds = BTreeMap::new();
    for _ in 0..live_bird_count {
     let line = read_line(reader)?;
     let (id, body) = parse_bird_line(&line)?;
     live_birds.insert(id, body);
    }
    Ok(Some(FrameObservation { apples, live_birds }))
   }
   pub fn build_visible_state(
    &self,
    frame: &FrameObservation,
    losses: [i32; 2],
    turn: i32,
   ) -> GameState {
    let mut grid = self.template_grid.clone();
    grid.apples = frame.apples.clone();
    let mut state = GameState {
     grid,
     birds: Vec::new(),
     losses,
     turn,
    };
    for bird_id in self
     .my_ids
     .iter()
     .chain(self.opp_ids.iter())
     .copied()
     .collect::<Vec<_>>()
    {
     if let Some(body) = frame.live_birds.get(&bird_id) {
      state.birds.push(BirdState {
       id: bird_id,
       owner: self.owner_of(bird_id),
       body: body.iter().copied().collect::<VecDeque<_>>(),
       alive: true,
       direction: None,
      });
     } else {
      state.birds.push(BirdState {
       id: bird_id,
       owner: self.owner_of(bird_id),
       body: VecDeque::new(),
       alive: false,
       direction: None,
      });
     }
    }
    state.birds.sort_by_key(|bird| bird.id);
    state
   }
   pub fn visible_signature(
    &self,
    state: &GameState,
   ) -> (BTreeSet<Coord>, BTreeMap<i32, Vec<Coord>>) {
    let apples = state.grid.apples.iter().copied().collect::<BTreeSet<_>>();
    let birds = state
     .birds
     .iter()
     .filter(|bird| bird.alive)
     .map(|bird| (bird.id, bird.body.iter().copied().collect::<Vec<_>>()))
     .collect::<BTreeMap<_, _>>();
    (apples, birds)
   }
   pub fn observation_signature(
    &self,
    frame: &FrameObservation,
   ) -> (BTreeSet<Coord>, BTreeMap<i32, Vec<Coord>>) {
    (
     frame.apples.iter().copied().collect::<BTreeSet<_>>(),
     frame.live_birds.clone(),
    )
   }
   fn owner_of(&self, bird_id: i32) -> usize {
    if self.my_ids.contains(&bird_id) {
     self.player_index
    } else {
     1 - self.player_index
    }
   }
  }
  fn read_optional_line(reader: &mut impl BufRead) -> io::Result<Option<String>> {
   let mut line = String::new();
   let bytes = reader.read_line(&mut line)?;
   if bytes == 0 {
    return Ok(None);
   }
   Ok(Some(line.trim().to_owned()))
  }
  fn read_line(reader: &mut impl BufRead) -> io::Result<String> {
   read_optional_line(reader)?
    .ok_or_else(|| io::Error::new(io::ErrorKind::UnexpectedEof, "unexpected EOF"))
  }
  fn parse_err(err: impl ToString) -> io::Error {
   io::Error::new(io::ErrorKind::InvalidData, err.to_string())
  }
  fn parse_space_coord(input: &str) -> io::Result<Coord> {
   let mut parts = input.split_whitespace();
   let x = parts
    .next()
    .ok_or_else(|| parse_err("missing x"))?
    .parse::<i32>()
    .map_err(parse_err)?;
   let y = parts
    .next()
    .ok_or_else(|| parse_err("missing y"))?
    .parse::<i32>()
    .map_err(parse_err)?;
   Ok(Coord::new(x, y))
  }
  fn parse_bird_line(input: &str) -> io::Result<(i32, Vec<Coord>)> {
   let mut parts = input.split_whitespace();
   let id = parts
    .next()
    .ok_or_else(|| parse_err("missing bird id"))?
    .parse::<i32>()
    .map_err(parse_err)?;
   let body = parts.next().ok_or_else(|| parse_err("missing body"))?;
   let coords = body
    .split(':')
    .map(|segment| {
     let mut pieces = segment.split(',');
     let x = pieces
      .next()
      .ok_or_else(|| parse_err("missing body x"))?
      .parse::<i32>()
      .map_err(parse_err)?;
     let y = pieces
      .next()
      .ok_or_else(|| parse_err("missing body y"))?
      .parse::<i32>()
      .map_err(parse_err)?;
     Ok(Coord::new(x, y))
    })
    .collect::<io::Result<Vec<_>>>()?;
   Ok((id, coords))
  }
 }
 pub mod search {
  use std::time::{Duration, Instant};
  use crate::engine::{BirdCommand, Direction, GameState, PlayerAction};
  use super::config::BotConfig;
  use super::eval::evaluate;
  use super::hybrid::{leaf_bonus, predict, HybridPrediction};
  const CONTEST_MAX_TURNS: i32 = 200;
  #[derive(Clone, Copy, Debug, Eq, PartialEq)]
  pub enum SearchBudget {
   TimeMs(u64),
   ExtraNodesAfterRoot(u64),
  }
  impl SearchBudget {
   pub fn budget_type(self) -> &'static str {
    match self {
     SearchBudget::TimeMs(_) => "time_ms",
     SearchBudget::ExtraNodesAfterRoot(_) => "extra_nodes_after_root",
    }
   }
   pub fn budget_value(self) -> u64 {
    match self {
     SearchBudget::TimeMs(value) | SearchBudget::ExtraNodesAfterRoot(value) => value,
    }
   }
  }
  #[derive(Clone, Debug, Default, Eq, PartialEq)]
  pub struct SearchStats {
   pub elapsed_ms: u64,
   pub root_pairs: u64,
   pub extra_nodes: u64,
   pub cutoffs: u64,
   pub tt_hits: u64,
   pub my_action_count: usize,
   pub opp_action_count: usize,
   pub deepened_actions: usize,
  }
  #[derive(Clone, Debug)]
  pub struct SearchOutcome {
   pub action: PlayerAction,
   pub action_id: usize,
   pub action_count: usize,
   pub root_values: Vec<f32>,
   pub score: f64,
   pub mean_score: f64,
   pub stats: SearchStats,
  }
  #[derive(Clone)]
  struct RootResponse {
   opp_index: usize,
   score: f64,
  }
  #[derive(Clone)]
  struct RootAnalysis {
   action: PlayerAction,
   action_id: usize,
   worst_score: f64,
   mean_score: f64,
   responses: Vec<RootResponse>,
  }
  pub fn live_budget_for_turn(config: &BotConfig, turn: i32) -> SearchBudget {
   if turn == 0 {
    SearchBudget::TimeMs(config.search.first_turn_ms)
   } else {
    SearchBudget::TimeMs(config.search.later_turn_ms)
   }
  }
  pub fn choose_action(
   state: &GameState,
   owner: usize,
   config: &BotConfig,
   budget: SearchBudget,
  ) -> SearchOutcome {
   let started = Instant::now();
   let deadline = deadline_for_budget(started, budget);
   let allow_cutoff = matches!(budget, SearchBudget::TimeMs(_));
   let mut extra_nodes_remaining = match budget {
    SearchBudget::TimeMs(_) => u64::MAX,
    SearchBudget::ExtraNodesAfterRoot(nodes) => nodes,
   };
   let my_actions = ordered_joint_actions(state, owner);
   let opp_actions = ordered_joint_actions(state, 1 - owner);
   let my_action_count = my_actions.len();
   let opp_action_count = opp_actions.len();
   let default_action = PlayerAction::default();
   let root_prediction = prior_prediction(state, owner, config, 0);
   let mut my_order = (0..my_actions.len())
    .map(|idx| {
     (
      idx,
      action_prior(
       state,
       owner,
       &my_actions[idx],
       &default_action,
       config,
       root_prediction.as_ref(),
       0,
      ),
     )
    })
    .collect::<Vec<_>>();
   my_order.sort_by(|left, right| right.1.total_cmp(&left.1));
   let mut opp_order = (0..opp_actions.len())
    .map(|idx| {
     (
      idx,
      action_prior(
       state,
       owner,
       &default_action,
       &opp_actions[idx],
       config,
       None,
       0,
      ),
     )
    })
    .collect::<Vec<_>>();
   opp_order.sort_by(|left, right| left.1.total_cmp(&right.1));
   let mut stats = SearchStats {
    my_action_count,
    opp_action_count,
    ..SearchStats::default()
   };
   let mut root_analyses = Vec::with_capacity(my_actions.len());
   let mut root_values = vec![f32::NEG_INFINITY; my_actions.len()];
   let mut best_shallow_worst = f64::NEG_INFINITY;
   'outer: for (my_index, _) in my_order {
    if expired(deadline) {
     break;
    }
    let my_action = my_actions[my_index].clone();
    let mut worst_score = f64::INFINITY;
    let mut mean_score = 0.0;
    let mut evaluated = 0_usize;
    let mut responses = Vec::with_capacity(opp_actions.len());
    for (opp_index, _) in opp_order.iter().copied() {
     if expired(deadline) {
      break 'outer;
     }
     let next = simulate_state(state, owner, &my_action, &opp_actions[opp_index]);
     let score = evaluate_with_hybrid(&next, owner, config, 1);
     stats.root_pairs += 1;
     worst_score = worst_score.min(score);
     mean_score += score;
     evaluated += 1;
     responses.push(RootResponse { opp_index, score });
     if allow_cutoff && worst_score < best_shallow_worst {
      stats.cutoffs += 1;
      break;
     }
    }
    if evaluated == 0 {
     continue;
    }
    mean_score /= evaluated as f64;
    let mut analysis = RootAnalysis {
     action: my_action,
     action_id: my_index,
     worst_score,
     mean_score,
     responses,
    };
    refresh_root_analysis(&mut analysis, &mut root_values);
    best_shallow_worst = best_shallow_worst.max(analysis.worst_score);
    root_analyses.push(analysis);
   }
   if root_analyses.is_empty() {
    stats.elapsed_ms = started.elapsed().as_millis() as u64;
    return SearchOutcome {
     action: PlayerAction::default(),
     action_id: 0,
     action_count: my_action_count.max(1),
     root_values,
     score: f64::NEG_INFINITY,
     mean_score: f64::NEG_INFINITY,
     stats,
    };
   }
   root_analyses.sort_by(|left, right| {
    right
     .worst_score
     .total_cmp(&left.worst_score)
     .then_with(|| right.mean_score.total_cmp(&left.mean_score))
   });
   let deepen_top_my = config.search.deepen_top_my.min(root_analyses.len());
   for analysis in root_analyses.iter_mut().take(deepen_top_my) {
    if expired(deadline) || extra_nodes_remaining == 0 {
     break;
    }
    stats.deepened_actions += 1;
    let mut response_indices = (0..analysis.responses.len()).collect::<Vec<_>>();
    response_indices.sort_by(|left, right| {
     analysis.responses[*left]
      .score
      .total_cmp(&analysis.responses[*right].score)
    });
    for response_index in response_indices
     .into_iter()
     .take(config.search.deepen_top_opp.min(analysis.responses.len()))
    {
     if expired(deadline) || extra_nodes_remaining == 0 {
      break;
     }
     let next = simulate_state(
      state,
      owner,
      &analysis.action,
      &opp_actions[analysis.responses[response_index].opp_index],
     );
     let child_score = deepen_branch(
      &next,
      owner,
      config,
      deadline,
      &mut extra_nodes_remaining,
      &mut stats,
      1,
     );
     analysis.responses[response_index].score = child_score;
    }
    refresh_root_analysis(analysis, &mut root_values);
   }
   let best = root_analyses
    .iter()
    .max_by(|left, right| {
     left.worst_score
      .total_cmp(&right.worst_score)
      .then_with(|| left.mean_score.total_cmp(&right.mean_score))
    })
    .expect("non-empty root analyses")
    .clone();
   stats.elapsed_ms = started.elapsed().as_millis() as u64;
   SearchOutcome {
    action: best.action,
    action_id: best.action_id,
    action_count: my_action_count.max(1),
    root_values,
    score: best.worst_score,
    mean_score: best.mean_score,
    stats,
   }
  }
  pub fn choose_action_unlimited(
   state: &GameState,
   owner: usize,
   config: &BotConfig,
  ) -> SearchOutcome {
   choose_action(
    state,
    owner,
    config,
    SearchBudget::ExtraNodesAfterRoot(u64::MAX / 4),
   )
  }
  fn deepen_branch(
   state: &GameState,
   owner: usize,
   config: &BotConfig,
   deadline: Option<Instant>,
   extra_nodes_remaining: &mut u64,
   stats: &mut SearchStats,
   depth: usize,
  ) -> f64 {
   let my_actions = ordered_joint_actions(state, owner);
   let opp_actions = ordered_joint_actions(state, 1 - owner);
   let default_action = PlayerAction::default();
   let root_prediction = prior_prediction(state, owner, config, depth);
   let mut my_order = (0..my_actions.len())
    .map(|idx| {
     (
      idx,
      action_prior(
       state,
       owner,
       &my_actions[idx],
       &default_action,
       config,
       root_prediction.as_ref(),
       depth,
      ),
     )
    })
    .collect::<Vec<_>>();
   my_order.sort_by(|left, right| right.1.total_cmp(&left.1));
   let mut opp_order = (0..opp_actions.len())
    .map(|idx| {
     (
      idx,
      action_prior(
       state,
       owner,
       &default_action,
       &opp_actions[idx],
       config,
       None,
       depth,
      ),
     )
    })
    .collect::<Vec<_>>();
   opp_order.sort_by(|left, right| left.1.total_cmp(&right.1));
   let mut best_followup = f64::NEG_INFINITY;
   for (my_index, _) in my_order
    .into_iter()
    .take(config.search.deepen_child_my.min(my_actions.len()))
   {
    if expired(deadline) || *extra_nodes_remaining == 0 {
     break;
    }
    let mut child_worst = f64::INFINITY;
    for (opp_index, _) in opp_order
     .iter()
     .copied()
     .take(config.search.deepen_child_opp.min(opp_actions.len()))
    {
     if expired(deadline) || *extra_nodes_remaining == 0 {
      break;
     }
     *extra_nodes_remaining -= 1;
     stats.extra_nodes += 1;
     let next = simulate_state(state, owner, &my_actions[my_index], &opp_actions[opp_index]);
     let score = evaluate_with_hybrid(&next, owner, config, depth + 1);
     child_worst = child_worst.min(score);
    }
    if child_worst.is_finite() {
     best_followup = best_followup.max(child_worst);
    }
   }
   if best_followup.is_finite() {
    best_followup
   } else {
    evaluate_with_hybrid(state, owner, config, depth)
   }
  }
  fn refresh_root_analysis(analysis: &mut RootAnalysis, root_values: &mut [f32]) {
   if analysis.responses.is_empty() {
    analysis.worst_score = f64::NEG_INFINITY;
    analysis.mean_score = f64::NEG_INFINITY;
    root_values[analysis.action_id] = f32::NEG_INFINITY;
    return;
   }
   analysis.worst_score = analysis
    .responses
    .iter()
    .map(|response| response.score)
    .fold(f64::INFINITY, f64::min);
   analysis.mean_score = analysis
    .responses
    .iter()
    .map(|response| response.score)
    .sum::<f64>()
    / analysis.responses.len() as f64;
   root_values[analysis.action_id] = analysis.worst_score as f32;
  }
  fn action_prior(
   state: &GameState,
   owner: usize,
   my_action: &PlayerAction,
   opp_action: &PlayerAction,
   config: &BotConfig,
   prediction: Option<&HybridPrediction>,
   depth: usize,
  ) -> f64 {
   let next = simulate_state(state, owner, my_action, opp_action);
   let mut score = evaluate_with_hybrid(&next, owner, config, depth + 1);
   if let (Some(hybrid), Some(prediction)) = (config.hybrid.as_ref(), prediction) {
    if hybrid.prior_mix != 0.0 {
     score += hybrid.prior_mix * prediction.action_prior(state, owner, my_action);
    }
   }
   score
  }
  fn simulate_state(
   state: &GameState,
   owner: usize,
   my_action: &PlayerAction,
   opp_action: &PlayerAction,
  ) -> GameState {
   let mut next = state.clone();
   if owner == 0 {
    next.step(my_action, opp_action);
   } else {
    next.step(opp_action, my_action);
   }
   next
  }
  fn evaluate_with_hybrid(state: &GameState, owner: usize, config: &BotConfig, depth: usize) -> f64 {
   let mut score = evaluate(state, owner, CONTEST_MAX_TURNS, &config.eval);
   if let Some(hybrid) = config.hybrid.as_ref() {
    if hybrid.leaf_mix != 0.0 && depth <= hybrid.leaf_depth_limit {
     if let Some(prediction) = predict(state, owner, config) {
      score += hybrid.leaf_mix * leaf_bonus(&prediction, hybrid);
     }
    }
   }
   score
  }
  fn prior_prediction(
   state: &GameState,
   owner: usize,
   config: &BotConfig,
   depth: usize,
  ) -> Option<HybridPrediction> {
   let hybrid = config.hybrid.as_ref()?;
   if hybrid.prior_mix == 0.0 || depth > hybrid.prior_depth_limit {
    return None;
   }
   predict(state, owner, config)
  }
  fn deadline_for_budget(started: Instant, budget: SearchBudget) -> Option<Instant> {
   match budget {
    SearchBudget::TimeMs(ms) => Some(started + Duration::from_millis(ms)),
    SearchBudget::ExtraNodesAfterRoot(_) => None,
   }
  }
  fn expired(deadline: Option<Instant>) -> bool {
   deadline.is_some_and(|deadline| Instant::now() >= deadline)
  }
  fn ordered_joint_actions(state: &GameState, owner: usize) -> Vec<PlayerAction> {
   let birds = state
    .birds_for_player(owner)
    .filter(|bird| bird.alive)
    .map(|bird| bird.id)
    .collect::<Vec<_>>();
   let mut result = Vec::new();
   let mut current = PlayerAction::default();
   enumerate_actions(state, &birds, 0, &mut current, &mut result);
   if result.is_empty() {
    result.push(PlayerAction::default());
   }
   result
  }
  fn enumerate_actions(
   state: &GameState,
   birds: &[i32],
   idx: usize,
   current: &mut PlayerAction,
   output: &mut Vec<PlayerAction>,
  ) {
   if idx == birds.len() {
    output.push(current.clone());
    return;
   }
   let bird_id = birds[idx];
   for command in ordered_commands_for_bird(state, bird_id) {
    match command {
     BirdCommand::Keep => {
      current.per_bird.remove(&bird_id);
     }
     BirdCommand::Turn(direction) => {
      current
       .per_bird
       .insert(bird_id, BirdCommand::Turn(direction));
     }
    }
    enumerate_actions(state, birds, idx + 1, current, output);
   }
   current.per_bird.remove(&bird_id);
  }
  fn ordered_commands_for_bird(state: &GameState, bird_id: i32) -> Vec<BirdCommand> {
   let Some(bird) = state
    .birds
    .iter()
    .find(|bird| bird.id == bird_id && bird.alive)
   else {
    return Vec::new();
   };
   let facing = bird.facing();
   let mut commands = vec![BirdCommand::Keep];
   if facing == Direction::Unset {
    for direction in Direction::ALL {
     commands.push(BirdCommand::Turn(direction));
    }
    return commands;
   }
   commands.push(BirdCommand::Turn(turn_left(facing)));
   commands.push(BirdCommand::Turn(turn_right(facing)));
   commands
  }
  fn turn_left(direction: Direction) -> Direction {
   match direction {
    Direction::North => Direction::West,
    Direction::West => Direction::South,
    Direction::South => Direction::East,
    Direction::East => Direction::North,
    Direction::Unset => Direction::Unset,
   }
  }
  fn turn_right(direction: Direction) -> Direction {
   match direction {
    Direction::North => Direction::East,
    Direction::East => Direction::South,
    Direction::South => Direction::West,
    Direction::West => Direction::North,
    Direction::Unset => Direction::Unset,
   }
  }
  pub fn render_action(action: &PlayerAction) -> String {
   let mut commands = action
    .per_bird
    .iter()
    .filter_map(|(bird_id, command)| match command {
     BirdCommand::Keep => None,
     BirdCommand::Turn(direction) => {
      Some(format!("{} {}", bird_id, direction_to_word(*direction)))
     }
    })
    .collect::<Vec<_>>();
   commands.sort();
   if commands.is_empty() {
    "WAIT".to_owned()
   } else {
    commands.join(";")
   }
  }
  fn direction_to_word(direction: Direction) -> &'static str {
   match direction {
    Direction::North => "UP",
    Direction::East => "RIGHT",
    Direction::South => "DOWN",
    Direction::West => "LEFT",
    Direction::Unset => "UP",
   }
  }
 }
}
use std::io::{self, BufRead, BufReader, Write};
use bot::config::BotConfig;
use bot::input::{BotIo, FrameObservation};
use bot::search::{choose_action, live_budget_for_turn, render_action};
use engine::{GameState, PlayerAction};
fn main() -> io::Result<()> {
 let stdin = io::stdin();
 let stdout = io::stdout();
 let mut reader = BufReader::new(stdin.lock());
 let mut writer = io::BufWriter::new(stdout.lock());
 let bot = Bot::new(BotIo::read(&mut reader)?);
 bot.run(&mut reader, &mut writer)
}
struct Bot {
 config: BotConfig,
 io: BotIo,
 state: Option<GameState>,
 last_my_action: PlayerAction,
}
impl Bot {
 fn new(io: BotIo) -> Self {
  Self {
   config: BotConfig::embedded(),
   io,
   state: None,
   last_my_action: PlayerAction::default(),
  }
 }
 fn run(mut self, reader: &mut impl BufRead, writer: &mut impl Write) -> io::Result<()> {
  let debug_stats = std::env::var_os("SNAKEBOT_DEBUG_STATS").is_some();
  while let Some(frame) = self.io.read_frame(reader)? {
   let state = self.reconcile_state(&frame);
   let budget = live_budget_for_turn(&self.config, state.turn);
   let outcome = choose_action(&state, self.io.player_index, &self.config, budget);
   if debug_stats {
    eprintln!(
     "turn={} elapsed_ms={} root_pairs={} extra_nodes={} cutoffs={}",
     state.turn,
     outcome.stats.elapsed_ms,
     outcome.stats.root_pairs,
     outcome.stats.extra_nodes,
     outcome.stats.cutoffs
    );
   }
   let command = render_action(&outcome.action);
   writeln!(writer, "{command}")?;
   writer.flush()?;
   self.last_my_action = outcome.action;
   self.state = Some(state);
  }
  Ok(())
 }
 fn reconcile_state(&self, frame: &FrameObservation) -> GameState {
  let observed_signature = self.io.observation_signature(frame);
  let fallback = if let Some(previous) = &self.state {
   self.io
    .build_visible_state(frame, previous.losses, previous.turn + 1)
  } else {
   self.io.build_visible_state(frame, [0, 0], 0)
  };
  let Some(previous) = &self.state else {
   return fallback;
  };
  let opponent = 1 - self.io.player_index;
  for opp_action in previous.legal_joint_actions(opponent) {
   let mut simulated = previous.clone();
   if self.io.player_index == 0 {
    simulated.step(&self.last_my_action, &opp_action);
   } else {
    simulated.step(&opp_action, &self.last_my_action);
   }
   if self.io.visible_signature(&simulated) == observed_signature {
    return simulated;
   }
  }
  if std::env::var_os("SNAKEBOT_STRICT_RECONCILE").is_some() {
   panic!("reconcile_state fell back to visible reconstruction");
  }
  fallback
 }
}
