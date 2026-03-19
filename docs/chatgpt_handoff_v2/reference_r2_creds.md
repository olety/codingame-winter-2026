---
name: R2 and cloud credentials
description: R2 bucket credentials, vast.ai API key locations in skate, Modal workspace info
type: reference
---

## Credential Locations

### R2 (Cloudflare)
- Stored in `skate`:
  - `skate get snakebot-r2-accesskey-id`
  - `skate get snakebot-r2-secret`
  - `skate get snakebot-r2-endpoint`
  - `skate get snakebot-r2-token`
- Endpoint: `https://70ae12731f3e503777d9f59a6c2c18da.r2.cloudflarestorage.com`
- Bucket: `snakebot-data`
- AWS CLI configured locally with these creds
- Also configured on Contabo (`ubuntu@62.146.173.45`) and Mac Mini (`oletymac`)

### Vast.ai
- `skate get vastai` → API key
- CLI installed: `pip3 install vastai`
- Config: `~/.config/vastai/vast_api_key`

### Modal
- **Active workspace**: `oneiron-dev` ($200 credits, Starter plan)
- **Old workspace**: `hfmcp` ($143 remaining credits, ~$120 spent)
- Secret: `r2-creds` (AWS_ACCESS_KEY_ID + AWS_SECRET_ACCESS_KEY) — exists on BOTH workspaces
- Switch: `modal profile activate oneiron-dev` / `modal profile activate hfmcp`
