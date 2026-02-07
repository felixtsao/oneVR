#!/usr/bin/env bash

# Setup script for runpods with Nvidia CUDA supported GPUs
# e.g. RTX 3090 from https://console.runpod.io/deploy


set -euo pipefail

echo "=== OS info ==="
uname -a
if [ -f /etc/os-release ]; then
  cat /etc/os-release
else
  echo "WARNING: /etc/os-release not found"
fi

ARCH=$(uname -m)
if [ "$ARCH" != "x86_64" ]; then
  echo
  echo "ERROR: Unsupported architecture: $ARCH (expected x86_64)"
  exit 1
fi

echo "Kernel version:"
uname -r

echo
echo "=== Installing system dependencies (runpod config) ==="

if command -v apt-get >/dev/null 2>&1; then
  apt-get update
  apt-get install -y \
    build-essential \
    clang-format \
    clang-tidy \
    pkg-config \
    git \
    curl \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libswresample-dev \
    libyaml-cpp-dev
else
  echo "ERROR: apt-get not found (expected Ubuntu/Debian)"
  exit 1
fi

echo
echo "=== Installing Bazelisk ==="

BAZEL_BIN=/usr/local/bin/bazel
BAZELISK_URL=https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-linux-amd64

curl -fsSL "$BAZELISK_URL" -o "$BAZEL_BIN"
chmod +x "$BAZEL_BIN"

echo "Bazel version:"
bazel version

echo
echo "=== Verifying CUDA environment ==="

echo
echo "-- nvidia-smi --"
if command -v nvidia-smi >/dev/null 2>&1; then
  nvidia-smi
else
  echo "WARNING: nvidia-smi not found (GPU driver may not be present)"
fi

echo
echo "-- nvcc --"
if command -v nvcc >/dev/null 2>&1; then
  which nvcc
  nvcc --version
else
  echo "ERROR: nvcc not found (CUDA toolkit missing)"
  exit 1
fi

echo
echo "-- /usr/local/cuda sanity --"
if [ -d /usr/local/cuda ]; then
  echo "/usr/local/cuda -> $(readlink -f /usr/local/cuda)"
  ls -la /usr/local/cuda/include/cuda_runtime.h >/dev/null
  ls -la /usr/local/cuda/lib64/libcudart.so* >/dev/null
else
  echo "ERROR: /usr/local/cuda not found"
  exit 1
fi

echo
echo "-- GPU info --"
if command -v nvidia-smi >/dev/null 2>&1; then
  GPU_NAME=$(nvidia-smi --query-gpu=name --format=csv,noheader | head -n 1)
  echo "GPU detected: $GPU_NAME"
else
  echo "WARNING: nvidia-smi not found"
fi

echo
echo "======================================="
echo " oneVR dev environment ready LFG 🔥 🚀"
echo " Detected GPU: $GPU_NAME"
echo "======================================="
echo ""
