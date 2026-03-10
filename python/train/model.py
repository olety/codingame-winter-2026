from __future__ import annotations

import torch
from torch import nn


class ResidualBlock(nn.Module):
    def __init__(self, channels: int) -> None:
        super().__init__()
        self.layers = nn.Sequential(
            nn.Conv2d(channels, channels, kernel_size=3, padding=1),
            nn.ReLU(inplace=True),
            nn.Conv2d(channels, channels, kernel_size=3, padding=1),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return torch.relu(x + self.layers(x))


class ValueNet(nn.Module):
    def __init__(
        self,
        input_channels: int,
        scalar_features: int,
        conv_channels: int,
        res_blocks: int,
    ) -> None:
        super().__init__()
        body = [
            nn.Conv2d(input_channels, conv_channels, kernel_size=3, padding=1),
            nn.ReLU(inplace=True),
        ]
        body.extend(ResidualBlock(conv_channels) for _ in range(res_blocks))
        self.body = nn.Sequential(*body)
        self.board_head = nn.Sequential(
            nn.AdaptiveAvgPool2d(1),
            nn.Flatten(),
        )
        self.scalar_head = nn.Sequential(
            nn.Linear(conv_channels + scalar_features, conv_channels),
            nn.ReLU(inplace=True),
            nn.Linear(conv_channels, 1),
            nn.Tanh(),
        )

    def forward(self, grid: torch.Tensor, scalars: torch.Tensor) -> torch.Tensor:
        board = self.body(grid)
        pooled = self.board_head(board)
        if scalars.ndim == 1:
            scalars = scalars.unsqueeze(0)
        return self.scalar_head(torch.cat([pooled, scalars], dim=1))
