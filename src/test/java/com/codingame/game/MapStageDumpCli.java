package com.codingame.game;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.Set;

import com.codingame.game.grid.Coord;
import com.codingame.game.grid.Grid;
import com.codingame.game.grid.GridMaker;
import com.codingame.game.grid.Tile;
import com.google.gson.Gson;

public final class MapStageDumpCli {
    private static final Gson GSON = new Gson();

    private MapStageDumpCli() {
    }

    public static void main(String[] args) {
        if (args.length < 2) {
            throw new IllegalArgumentException("Usage: MapStageDumpCli <seed> <leagueLevel>");
        }

        long seed = Long.parseLong(args[0]);
        int leagueLevel = Integer.parseInt(args[1]);
        Random random = new Random(seed);

        double skew;
        if (leagueLevel == 1) {
            skew = 2;
        } else if (leagueLevel == 2) {
            skew = 1;
        } else if (leagueLevel == 3) {
            skew = 0.8;
        } else {
            skew = 0.3;
        }

        double rand = random.nextDouble();
        int height = GridMaker.MIN_GRID_HEIGHT
            + (int) Math.round(Math.pow(rand, skew) * (GridMaker.MAX_GRID_HEIGHT - GridMaker.MIN_GRID_HEIGHT));
        int width = Math.round(height * GridMaker.ASPECT_RATIO);
        if (width % 2 != 0) {
            width += 1;
        }

        Grid grid = new Grid(width, height);
        double b = 5 + random.nextDouble() * 10;

        for (int x = 0; x < width; x++) {
            grid.get(x, height - 1).setType(Tile.TYPE_WALL);
        }
        for (int y = height - 2; y >= 0; y--) {
            double yNorm = (double) (height - 1 - y) / (height - 1);
            double blockChanceEl = 1 / (yNorm + 0.1) / b;
            for (int x = 0; x < width; x++) {
                if (random.nextDouble() < blockChanceEl) {
                    grid.get(x, y).setType(Tile.TYPE_WALL);
                }
            }
        }
        dump("base", grid, null);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                Coord c = new Coord(x, y);
                Coord opp = grid.opposite(c);
                grid.get(opp).setType(grid.get(c).getType());
            }
        }
        dump("symmetry", grid, null);

        List<Set<Coord>> islands = grid.detectAirPockets();
        for (Set<Coord> island : islands) {
            if (island.size() < 10) {
                for (Coord c : island) {
                    grid.get(c).setType(Tile.TYPE_WALL);
                }
            }
        }
        dump("fill_islands", grid, null);

        boolean somethingDestroyed = true;
        while (somethingDestroyed) {
            somethingDestroyed = false;
            for (Coord c : grid.cells.keySet()) {
                if (grid.get(c).getType() == Tile.TYPE_WALL) {
                    continue;
                }
                List<Coord> neighbours = grid.getNeighbours(c);
                List<Coord> neighbourWalls = neighbours.stream()
                    .filter(n -> grid.get(n).getType() == Tile.TYPE_WALL)
                    .toList();
                if (neighbourWalls.size() >= 3) {
                    List<Coord> destroyable = new ArrayList<>(
                        neighbourWalls.stream().filter(n -> n.getY() <= c.getY()).toList()
                    );
                    Collections.shuffle(destroyable, random);
                    grid.get(destroyable.get(0)).setType(Tile.TYPE_EMPTY);
                    grid.get(grid.opposite(destroyable.get(0))).setType(Tile.TYPE_EMPTY);
                    somethingDestroyed = true;
                }
            }
        }
        dump("destroy_walls", grid, null);

        List<Coord> island = grid.detectLowestIsland();
        int lowerBy = 0;
        boolean canLower = true;
        while (canLower) {
            for (int x = 0; x < width; x++) {
                Coord c = new Coord(x, height - 1 - (lowerBy + 1));
                if (!island.contains(c)) {
                    canLower = false;
                    break;
                }
            }
            if (canLower) {
                lowerBy++;
            }
        }
        if (lowerBy >= 2) {
            lowerBy = random.nextInt(2, lowerBy + 1);
        }
        for (Coord c : island) {
            grid.get(c).setType(Tile.TYPE_EMPTY);
            grid.get(grid.opposite(c)).setType(Tile.TYPE_EMPTY);
        }
        for (Coord c : island) {
            Coord lowered = new Coord(c.getX(), c.getY() + lowerBy);
            if (grid.get(lowered).isValid()) {
                grid.get(lowered).setType(Tile.TYPE_WALL);
                grid.get(grid.opposite(lowered)).setType(Tile.TYPE_WALL);
            }
        }
        dump("sink_lowest", grid, lowerBy);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width / 2; ++x) {
                Coord c = new Coord(x, y);
                if (grid.get(c).getType() == Tile.TYPE_EMPTY && random.nextDouble() < 0.025) {
                    grid.apples.add(c);
                    grid.apples.add(grid.opposite(c));
                }
            }
        }
        dump("random_apples", grid, lowerBy);

        for (Coord c : grid.cells.keySet()) {
            if (grid.get(c).getType() == Tile.TYPE_EMPTY) {
                continue;
            }
            long neighbourWallCount = grid.getNeighbours(c, Grid.ADJACENCY_8).stream()
                .filter(n -> grid.get(n).getType() == Tile.TYPE_WALL)
                .count();
            if (neighbourWallCount == 0) {
                grid.get(c).setType(Tile.TYPE_EMPTY);
                grid.get(grid.opposite(c)).setType(Tile.TYPE_EMPTY);
                grid.apples.add(c);
                grid.apples.add(grid.opposite(c));
            }
        }
        dump("lone_walls", grid, lowerBy);

        List<Coord> potentialSpawns = new ArrayList<>(grid.cells.keySet().stream().filter(c -> {
            if (grid.get(c).getType() != Tile.TYPE_WALL) {
                return false;
            }
            List<Coord> aboves = getFreeAbove(grid, c, GridMaker.SPAWN_HEIGHT);
            return aboves.size() >= GridMaker.SPAWN_HEIGHT;
        }).toList());
        Collections.shuffle(potentialSpawns, random);

        int desiredSpawns = GridMaker.DESIRED_SPAWNS;
        if (height <= 15) {
            desiredSpawns--;
        }
        if (height <= 10) {
            desiredSpawns--;
        }

        while (desiredSpawns > 0 && !potentialSpawns.isEmpty()) {
            Coord spawn = potentialSpawns.remove(0);
            List<Coord> spawnLoc = getFreeAbove(grid, spawn, GridMaker.SPAWN_HEIGHT);
            boolean tooClose = false;
            for (Coord c : spawnLoc) {
                if (c.getX() == width / 2 - 1 || c.getX() == width / 2) {
                    tooClose = true;
                    break;
                }
                for (Coord n : grid.getNeighbours(c, Grid.ADJACENCY_8)) {
                    if (grid.spawns.contains(n) || grid.spawns.contains(grid.opposite(n))) {
                        tooClose = true;
                        break;
                    }
                }
                if (tooClose) {
                    break;
                }
            }
            if (tooClose) {
                continue;
            }

            for (Coord c : spawnLoc) {
                grid.spawns.add(c);
                grid.apples.remove(c);
                grid.apples.remove(grid.opposite(c));
            }
            desiredSpawns--;
        }
        dump("spawns", grid, lowerBy);
    }

    private static List<Coord> getFreeAbove(Grid grid, Coord c, int by) {
        List<Coord> result = new ArrayList<>();
        for (int i = 1; i <= by; i++) {
            Coord above = new Coord(c.getX(), c.getY() - i);
            if (grid.get(above).isValid() && grid.get(above).getType() == Tile.TYPE_EMPTY) {
                result.add(above);
            } else {
                break;
            }
        }
        return result;
    }

    private static void dump(String stage, Grid grid, Integer lowerBy) {
        Map<String, Object> payload = new LinkedHashMap<>();
        payload.put("stage", stage);
        if (lowerBy != null) {
            payload.put("lowerBy", lowerBy);
        }
        payload.put("width", grid.width);
        payload.put("height", grid.height);
        payload.put("walls", grid.cells.keySet().stream()
            .filter(coord -> grid.get(coord).getType() == Tile.TYPE_WALL)
            .sorted()
            .map(MapStageDumpCli::coordList)
            .toList());
        payload.put("apples", sortedCoords(grid.apples));
        payload.put("spawns", sortedCoords(grid.spawns));
        System.out.println(GSON.toJson(payload));
    }

    private static List<List<Integer>> sortedCoords(List<Coord> coords) {
        return coords.stream()
            .sorted(Comparator.naturalOrder())
            .map(MapStageDumpCli::coordList)
            .toList();
    }

    private static List<Integer> coordList(Coord coord) {
        return List.of(coord.getX(), coord.getY());
    }
}
