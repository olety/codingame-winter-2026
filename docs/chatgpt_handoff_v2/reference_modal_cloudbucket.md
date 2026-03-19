---
name: reference_modal_cloudbucket
description: Modal CloudBucketMount can natively mount R2/S3 buckets as read-only filesystem in containers
type: reference
---

Modal's `CloudBucketMount` mounts S3-compatible buckets (including Cloudflare R2) directly as filesystem paths in containers. Recommended by Modal docs for large training datasets.

```python
r2_mount = modal.CloudBucketMount(
    "bucket-name",
    bucket_endpoint_url="https://<ACCOUNT_ID>.r2.cloudflarestorage.com",
    secret=modal.Secret.from_name("r2-creds"),
    read_only=True,
)

@app.function(volumes={"/data": vol, "/r2": r2_mount})
def train(...):
    # /r2/data.bin is accessible as regular file
    data = np.memmap("/r2/data.bin", ...)
```

**Key facts:**
- R2 endpoint: `https://70ae12731f3e503777d9f59a6c2c18da.r2.cloudflarestorage.com`
- R2 bucket: `snakebot-data` (already exists)
- R2 egress: $0 always (Cloudflare's key advantage)
- Storage: $0.015/GB/month (~$0.60/month for 40GB)
- Upload: use `aws s3 cp` (NOT wrangler — 300MB limit). AWS CLI handles multipart automatically.
- Need: R2 API token (S3-compatible Access Key + Secret Key) from Cloudflare dashboard
- Need: `modal secret create r2-creds AWS_ACCESS_KEY_ID=xxx AWS_SECRET_ACCESS_KEY=yyy`
- For perf: if CloudBucketMount is slow, copy to local SSD at startup with `ephemeral_disk=100_000`
