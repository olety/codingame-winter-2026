from __future__ import annotations

import torch
from torch import nn


class TinyHybridNet(nn.Module):
    def __init__(
        self,
        input_channels: int,
        scalar_features: int,
        conv_channels: int,
        birds_per_player: int = 4,
        actions_per_bird: int = 5,
    ) -> None:
        super().__init__()
        self.birds_per_player = birds_per_player
        self.actions_per_bird = actions_per_bird
        self.conv1 = nn.Conv2d(input_channels, conv_channels, kernel_size=3, padding=1)
        self.conv2 = nn.Conv2d(conv_channels, conv_channels, kernel_size=3, padding=1)
        self.pool = nn.AdaptiveAvgPool2d(1)
        feature_dim = conv_channels + scalar_features
        self.policy_head = nn.Linear(feature_dim, birds_per_player * actions_per_bird)
        self.value_head = nn.Linear(feature_dim, 1)

    def forward(self, grid: torch.Tensor, scalars: torch.Tensor) -> tuple[torch.Tensor, torch.Tensor]:
        x = torch.relu(self.conv1(grid))
        x = torch.relu(self.conv2(x))
        pooled = self.pool(x).flatten(1)
        if scalars.ndim == 1:
            scalars = scalars.unsqueeze(0)
        features = torch.cat([pooled, scalars], dim=1)
        policy = self.policy_head(features).view(-1, self.birds_per_player, self.actions_per_bird)
        value = torch.tanh(self.value_head(features))
        return policy, value
