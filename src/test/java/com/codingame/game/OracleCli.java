package com.codingame.game;

import java.io.BufferedReader;
import java.io.InputStreamReader;

public class OracleCli {

    public static void main(String[] args) throws Exception {
        if (args.length < 2) {
            throw new IllegalArgumentException("Usage: OracleCli <seed> <leagueLevel>");
        }

        long seed = Long.parseLong(args[0]);
        int leagueLevel = Integer.parseInt(args[1]);
        Game game = OracleSupport.buildGame(seed, leagueLevel);

        BufferedReader reader = new BufferedReader(new InputStreamReader(System.in));
        System.out.println(OracleSupport.serializeState(game));

        String line;
        int turn = 0;
        while ((line = reader.readLine()) != null) {
            line = line.trim();
            if (line.isEmpty()) {
                continue;
            }
            game.turn = turn;
            game.resetGameTurnData();
            OracleSupport.applyTurnCommands(game, line);
            OracleSupport.DO_MOVES.invoke(game);
            OracleSupport.DO_EATS.invoke(game);
            OracleSupport.DO_BEHEADINGS.invoke(game);
            OracleSupport.DO_FALLS.invoke(game);
            System.out.println(OracleSupport.serializeState(game));
            if (game.isGameOver()) {
                break;
            }
            turn++;
        }
    }
}
