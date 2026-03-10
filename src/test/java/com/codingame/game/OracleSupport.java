package com.codingame.game;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.stream.Collectors;

import com.codingame.event.Animation;
import com.codingame.game.grid.Coord;
import com.codingame.game.grid.Direction;
import com.codingame.game.grid.Grid;
import com.codingame.game.grid.GridMaker;
import com.codingame.game.grid.Tile;
import com.codingame.gameengine.core.AbstractMultiplayerPlayer;
import com.codingame.gameengine.core.AbstractPlayer;
import com.google.gson.Gson;
import com.google.gson.JsonParser;

final class OracleSupport {
    private static final Gson GSON = new Gson();
    private static final Field PLAYER_INDEX_FIELD = getField(AbstractPlayer.class, "index");
    private static final Field PLAYER_ACTIVE_FIELD = getField(AbstractMultiplayerPlayer.class, "active");
    private static final Field ANIMATION_FIELD = getField(Game.class, "animation");
    static final Method DO_MOVES = getMethod(Game.class, "doMoves");
    static final Method DO_EATS = getMethod(Game.class, "doEats");
    static final Method DO_BEHEADINGS = getMethod(Game.class, "doBeheadings");
    static final Method DO_FALLS = getMethod(Game.class, "doFalls");

    private OracleSupport() {
    }

    static Game buildGame(long seed, int leagueLevel) throws Exception {
        Game game = new Game();
        ANIMATION_FIELD.set(game, new Animation());

        GridMaker gridMaker = new GridMaker(new Random(seed), leagueLevel);
        Grid grid = gridMaker.make();
        game.grid = grid;
        game.players = new ArrayList<>();

        Player p0 = new Player();
        p0.init();
        setPlayerIndex(p0, 0);
        setPlayerActive(p0, true);

        Player p1 = new Player();
        p1.init();
        setPlayerIndex(p1, 1);
        setPlayerActive(p1, true);

        game.players.add(p0);
        game.players.add(p1);

        List<List<Coord>> spawnLocations = grid.detectSpawnIslands()
            .stream()
            .map(island -> island.stream().sorted().toList())
            .collect(Collectors.toList());

        int birdId = 0;
        for (Player player : game.players) {
            for (List<Coord> spawn : spawnLocations) {
                Bird bird = new Bird(birdId++, player);
                player.birds.add(bird);
                for (Coord coord : spawn) {
                    Coord bodyCoord = coord;
                    if (player.getIndex() == 1) {
                        bodyCoord = grid.opposite(bodyCoord);
                    }
                    bird.body.add(bodyCoord);
                }
            }
        }

        return game;
    }

    static void applyTurnCommands(Game game, String line) {
        String[] parts = line.split("\\|", -1);
        if (parts.length != 2) {
            throw new IllegalArgumentException("Expected '<p0>|<p1>' command format, got: " + line);
        }
        applyPlayerCommands(game.players.get(0), parts[0]);
        applyPlayerCommands(game.players.get(1), parts[1]);
    }

    static String serializeState(Game game) {
        Map<String, Object> state = new LinkedHashMap<>();
        state.put("width", game.grid.width);
        state.put("height", game.grid.height);
        state.put("walls", game.grid.cells.keySet().stream()
            .filter(coord -> game.grid.get(coord).getType() == Tile.TYPE_WALL)
            .sorted()
            .map(OracleSupport::coordList)
            .toList());
        state.put("spawns", game.grid.spawns.stream().sorted().map(OracleSupport::coordList).toList());
        state.put("apples", game.grid.apples.stream().sorted().map(OracleSupport::coordList).toList());
        state.put("losses", List.of(game.losses[0], game.losses[1]));
        state.put("gameOver", game.isGameOver());
        state.put("birds", game.players.stream()
            .flatMap(player -> player.birds.stream())
            .sorted(Comparator.comparingInt(bird -> bird.id))
            .map(bird -> {
                Map<String, Object> b = new LinkedHashMap<>();
                b.put("id", bird.id);
                b.put("owner", bird.owner.getIndex());
                b.put("alive", bird.alive);
                b.put("body", bird.body.stream().map(OracleSupport::coordList).toList());
                return b;
            })
            .toList());
        return GSON.toJson(state);
    }

    static String serializeDumpRecord(long seed, int leagueLevel, Game game) {
        Map<String, Object> record = new LinkedHashMap<>();
        record.put("seed", seed);
        record.put("leagueLevel", leagueLevel);
        record.put("state", JsonParser.parseString(serializeState(game)));
        return GSON.toJson(record);
    }

    private static void applyPlayerCommands(Player player, String commands) {
        if (commands.isBlank()) {
            return;
        }
        for (String token : commands.split(",")) {
            if (token.isBlank()) {
                continue;
            }
            String[] pieces = token.split(":");
            if (pieces.length != 2) {
                throw new IllegalArgumentException("Bad action token: " + token);
            }
            int birdId = Integer.parseInt(pieces[0]);
            String dirAlias = pieces[1];
            if ("K".equalsIgnoreCase(dirAlias)) {
                continue;
            }
            Bird bird = player.getBirdById(birdId);
            if (bird == null || !bird.alive) {
                throw new IllegalArgumentException("Bird unavailable for action: " + token);
            }
            Direction direction = Direction.fromAlias(dirAlias.toUpperCase());
            if (bird.getFacing().opposite() == direction) {
                throw new IllegalArgumentException("Backward move requested: " + token);
            }
            bird.direction = direction;
        }
    }

    private static List<Integer> coordList(Coord coord) {
        return List.of(coord.getX(), coord.getY());
    }

    private static Field getField(Class<?> type, String name) {
        try {
            Field field = type.getDeclaredField(name);
            field.setAccessible(true);
            return field;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private static Method getMethod(Class<?> type, String name) {
        try {
            Method method = type.getDeclaredMethod(name);
            method.setAccessible(true);
            return method;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private static void setPlayerIndex(Player player, int index) throws Exception {
        PLAYER_INDEX_FIELD.setInt(player, index);
    }

    private static void setPlayerActive(Player player, boolean active) throws Exception {
        PLAYER_ACTIVE_FIELD.setBoolean(player, active);
    }
}
