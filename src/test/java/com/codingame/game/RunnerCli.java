package com.codingame.game;

import java.util.LinkedHashMap;
import java.util.Map;

import com.codingame.gameengine.runner.MultiplayerGameRunner;
import com.codingame.gameengine.runner.simulate.GameResult;
import com.google.gson.Gson;

public class RunnerCli {
    private static final Gson GSON = new Gson();

    public static void main(String[] args) {
        ParsedArgs parsed = parseArgs(args);

        MultiplayerGameRunner runner = new MultiplayerGameRunner();
        runner.setLeagueLevel(parsed.leagueLevel);
        runner.setSeed(parsed.seed);
        runner.addAgent(parsed.agentA, "Agent A");
        runner.addAgent(parsed.agentB, "Agent B");

        GameResult result = runner.simulate();

        Map<String, Object> payload = new LinkedHashMap<>();
        payload.put("seed", parsed.seed);
        payload.put("leagueLevel", parsed.leagueLevel);
        payload.put("agentA", parsed.agentA);
        payload.put("agentB", parsed.agentB);
        payload.put("scores", result.scores);
        payload.put("failCause", result.failCause);
        payload.put("errorCounts", summarizeErrors(result.errors));
        payload.put("agentCount", result.agents == null ? 0 : result.agents.size());
        payload.put("passed", result.failCause == null);
        System.out.println(GSON.toJson(payload));
    }

    private static Map<String, Integer> summarizeErrors(Map<String, java.util.List<String>> errors) {
        Map<String, Integer> counts = new LinkedHashMap<>();
        if (errors == null) {
            return counts;
        }
        for (Map.Entry<String, java.util.List<String>> entry : errors.entrySet()) {
            int count = 0;
            for (String value : entry.getValue()) {
                if (value != null && !value.isBlank()) {
                    count++;
                }
            }
            counts.put(entry.getKey(), count);
        }
        return counts;
    }

    private static ParsedArgs parseArgs(String[] args) {
        ParsedArgs parsed = new ParsedArgs();
        for (int i = 0; i < args.length; i++) {
            switch (args[i]) {
                case "--league" -> parsed.leagueLevel = Integer.parseInt(requireValue(args, ++i, "--league"));
                case "--seed" -> parsed.seed = Long.parseLong(requireValue(args, ++i, "--seed"));
                case "--agent-a" -> parsed.agentA = requireValue(args, ++i, "--agent-a");
                case "--agent-b" -> parsed.agentB = requireValue(args, ++i, "--agent-b");
                default -> throw new IllegalArgumentException("Unknown arg: " + args[i]);
            }
        }
        if (parsed.agentA == null || parsed.agentB == null) {
            throw new IllegalArgumentException("Both --agent-a and --agent-b are required");
        }
        return parsed;
    }

    private static String requireValue(String[] args, int index, String flag) {
        if (index >= args.length) {
            throw new IllegalArgumentException("Missing value for " + flag);
        }
        return args[index];
    }

    private static final class ParsedArgs {
        int leagueLevel = 4;
        long seed = 1L;
        String agentA;
        String agentB;
    }
}
