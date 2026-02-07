# VR180

For general information about VR180 video format, see this blog post [here](https://blog.youtube/news-and-events/the-world-as-you-see-it-with-vr180/).

<h3>Hardware</h3>

2 cameras can be oriented in the same general viewing direction, separated by a typical inter-pupillary distance, to emulate the distinct perspectives each eye (left and right) sees when viewing a scene. The slight parallax formed between the varying perspectives creates a sense of depth when the left camera is shown to the left eye, and likewise with the right eye in a device like a VR headset. There are some commercial and prosumer cameras designed with dual lens/sensors but any fixture with 2 cameras works as well, like 2 GoPros placed side by side.

<h3>Software</h3>

The `warp_encoder/` directory contains code which transforms video captured from 2 wide-angle cameras into a standard VR180 video format for playback in a VR/AR headset and/or other interactive display which supports variable viewing angles.

Input videos representing the left camera/eye and right camera/eye can be concatenated side-by-side and projected/warped into an equirectangular/spherical encoding and stored in a single traditional 2D video format i.e. mp4 h.264/265 by command line interface.

<h4>Getting Started</h4>

To build and run this software requires a Linux OS.
Optionally, Nvidia GPUs supported by CUDA tookit can be used for hardware acceleration.
Clone this repository and install/verify dependencies with the `scripts/setup_linux.sh` on a Linux distro (e.g. Ubuntu 24.04) or containerized cloud instance like [Runpod](https://console.runpod.io/deploy).
```
./setup_linux.sh
```
Once setup has been completed, build the warp software with:
```
bazel build //vr180/warp_encoder:warp_encoder
```
Configure desired input/output file locations and other settings in the `warp_encoder/config.yaml`
```yaml:warp_encoder/config.yaml
```
Note the `camera_config` file which can be found the top-level `/cam` directory. This contains camera extrinsics/intrinsics parameter data to accompany the input left and right videos. Use an existing defined camera `config.yaml` or create a new one to use. Example for GoPro Hero 12:
```yaml:../cam/gopro_hero_12/config.yaml
```
After configuration, run and specify the full path to the config file:
```
bazel run //vr180/warp_encoder:warp_encoder vr180/warp_encoder/config.yaml
```
After warp encoding has finished, a generated `composite_3D_SBS_LR.mp4` can be found in the specified `output_directory` which can be viewed in a VR headset video player.