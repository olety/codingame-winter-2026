package com.codingame.game;

public class MapDumpCli {
    public static void main(String[] args) throws Exception {
        if (args.length < 3) {
            throw new IllegalArgumentException("Usage: MapDumpCli <startSeed> <count> <leagueLevel>");
        }

        long startSeed = Long.parseLong(args[0]);
        long count = Long.parseLong(args[1]);
        int leagueLevel = Integer.parseInt(args[2]);

        for (long offset = 0; offset < count; offset++) {
            long seed = startSeed + offset;
            Game game = OracleSupport.buildGame(seed, leagueLevel);
            System.out.println(OracleSupport.serializeDumpRecord(seed, leagueLevel, game));
        }
    }
}
