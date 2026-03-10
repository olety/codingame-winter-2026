use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::io::{self, BufRead};

use snakebot_engine::{BirdState, Coord, GameState, Grid, TileType};

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

#[cfg(test)]
mod tests {
    use std::io::Cursor;

    use super::BotIo;
    use snakebot_engine::Coord;

    #[test]
    fn parses_global_and_frame_input() {
        let input = "\
0
4
3
....
.##.
####
1
0
1
2
0 0
3 0
2
0 0,0:0,1:0,2
1 3,0:3,1:3,2
";
        let mut cursor = Cursor::new(input.as_bytes());
        let io = BotIo::read(&mut cursor).expect("global input");
        assert_eq!(io.player_index, 0);
        let frame = io
            .read_frame(&mut cursor)
            .expect("frame")
            .expect("some frame");
        assert_eq!(frame.apples, vec![Coord::new(0, 0), Coord::new(3, 0)]);
        assert_eq!(frame.live_birds.len(), 2);
    }
}
