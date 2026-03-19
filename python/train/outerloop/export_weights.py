from __future__ import annotations

import argparse
import json
from pathlib import Path

import torch

from python.train.outerloop.model import TinyHybridNet


def export_weights(model_path: Path, config_path: Path, output_path: Path) -> dict:
    training_config = json.loads(config_path.read_text(encoding="utf-8"))
    num_conv_layers = int(training_config.get("num_conv_layers", 2))
    model = TinyHybridNet(
        input_channels=int(training_config["input_channels"]),
        scalar_features=int(training_config["scalar_features"]),
        conv_channels=int(training_config["conv_channels"]),
        num_conv_layers=num_conv_layers,
    )
    state_dict = torch.load(model_path, map_location="cpu")
    model.load_state_dict(state_dict)
    payload = {
        "version": 2 if num_conv_layers >= 3 else 1,
        "input_channels": int(training_config["input_channels"]),
        "scalar_features": int(training_config["scalar_features"]),
        "board_height": int(training_config["board_height"]),
        "board_width": int(training_config["board_width"]),
        "conv1": export_conv(model.conv1),
        "conv2": export_conv(model.conv2),
        "policy": export_linear(model.policy_head),
        "value": export_linear(model.value_head),
    }
    if model.conv3 is not None:
        payload["conv3"] = export_conv(model.conv3)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return payload


def export_conv(layer: torch.nn.Conv2d) -> dict:
    return {
        "out_channels": layer.out_channels,
        "in_channels": layer.in_channels,
        "kernel_size": layer.kernel_size[0],
        "weights": layer.weight.detach().cpu().reshape(-1).tolist(),
        "bias": layer.bias.detach().cpu().reshape(-1).tolist(),
    }


def export_linear(layer: torch.nn.Linear) -> dict:
    return {
        "out_features": layer.out_features,
        "in_features": layer.in_features,
        "weights": layer.weight.detach().cpu().reshape(-1).tolist(),
        "bias": layer.bias.detach().cpu().reshape(-1).tolist(),
    }


def export_weights_int4(model_path: Path, config_path: Path, output_path: Path) -> dict:
    """Export model with int4-quantized weights + per-channel scales."""
    training_config = json.loads(config_path.read_text(encoding="utf-8"))
    num_conv_layers = int(training_config.get("num_conv_layers", 2))
    model = TinyHybridNet(
        input_channels=int(training_config["input_channels"]),
        scalar_features=int(training_config["scalar_features"]),
        conv_channels=int(training_config["conv_channels"]),
        num_conv_layers=num_conv_layers,
    )
    state_dict = torch.load(model_path, map_location="cpu")
    model.load_state_dict(state_dict)

    def quantize_conv_int4(layer: torch.nn.Conv2d) -> dict:
        w = layer.weight.detach().cpu()  # (out_ch, in_ch, kH, kW)
        reduce_dims = list(range(1, w.ndim))
        absmax = w.abs().amax(dim=reduce_dims).clamp(min=1e-8)
        scale = absmax / 7.0
        w_q = (w / scale.view(-1, *([1] * (w.ndim - 1)))).round().clamp(-8, 7).to(torch.int8)
        return {
            "out_channels": layer.out_channels,
            "in_channels": layer.in_channels,
            "kernel_size": layer.kernel_size[0],
            "weights_int4": w_q.reshape(-1).tolist(),
            "weight_scales": scale.tolist(),
            "bias": layer.bias.detach().cpu().reshape(-1).tolist(),
        }

    def quantize_linear_int4(layer: torch.nn.Linear) -> dict:
        w = layer.weight.detach().cpu()  # (out_features, in_features)
        absmax = w.abs().amax(dim=1).clamp(min=1e-8)
        scale = absmax / 7.0
        w_q = (w / scale.unsqueeze(1)).round().clamp(-8, 7).to(torch.int8)
        return {
            "out_features": layer.out_features,
            "in_features": layer.in_features,
            "weights_int4": w_q.reshape(-1).tolist(),
            "weight_scales": scale.tolist(),
            "bias": layer.bias.detach().cpu().reshape(-1).tolist(),
        }

    payload = {
        "version": 3,
        "quantization": "int4_per_channel",
        "input_channels": int(training_config["input_channels"]),
        "scalar_features": int(training_config["scalar_features"]),
        "board_height": int(training_config["board_height"]),
        "board_width": int(training_config["board_width"]),
        "conv1": quantize_conv_int4(model.conv1),
        "conv2": quantize_conv_int4(model.conv2),
        "policy": quantize_linear_int4(model.policy_head),
        "value": quantize_linear_int4(model.value_head),
    }
    if model.conv3 is not None:
        payload["conv3"] = quantize_conv_int4(model.conv3)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return payload


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--training-config", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    payload = export_weights(args.model, args.training_config, args.out)
    print(json.dumps(payload, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
