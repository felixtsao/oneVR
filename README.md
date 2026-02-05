# One VR
<img src="img/doc/banner.jpg" alt="Banner" width="900px"/>
<h3> A DIY VR180, 360 video workflow </h3>
<p>
A camera hardware and software framework, forms an end-to-end solution; from capture, processing and playback.
<p>
Designs based around using off-the-shelf hardware/software. Parameterized 3D model files allow for easy generation of camera mounts to fit most small, rectangular cameras like GoPro, SJCAM, Xiaomi and NoPro. Parameterization also allows scalability to reach 4K resolution and beyond.
<p>
Video stitching methods include using accessible open-source software and custom scripts provided by this repository.
<p>
This project started Spring break 2015 with some good friends and further developed through a Computational Photography class, CS 534. Hardware resources/funding by Garage Physics at University of Wisconsin, Madison.
<p>
<h4> Example results </h4>

[4K 30fps VR180 stereoscopic video](https://www.youtube.com/watch?v=hhlstimBSec&list=PLbWNpbTQem_h6UcD7EMUGMVDucTkWlxWI) <br>
[4K 60fps 360 monoscopic video](https://www.youtube.com/watch?v=hQ7O1qrUY-8) <br>

<h3> Motivation </h3>

360 video is an exciting medium that was recently supported by video sharing sites like [YouTube](https://www.youtube.com/channel/UCzuqhhs6NWbgTzMuM09WKDQ), [Facebook](http://newsroom.fb.com/news/2015/09/introducing-360-video-on-facebook/) and [Vimeo](https://join.vimeo.com/360/?vcid=30830). Unfortunately, content creation has remained limited due the high cost-of-entry in acquiring 360 cameras and video stitching software. Professional solutions range from thousands to tens of thousands of dollars and has low accessibility. Cost-friendly, consumer 360 cameras exist on the market but are limited to sub 4K resolutions, which suffer from pixelation in 360 playback.
<p>
This repository aims to provide a high-quality solution while being accessible to 360 filmmakers on a budget. The optimal target cost for a full 360 video solution is under or around $500.
<p>
Additionally, concensus in the creative community is that a different approach from traditional filmmaking techniques must be used in developing meaningful 360 video. This is because applying panning motions and camera dollys like in traditional film may cause excessive disorientation in 360 video. Another challenge is that users may look in directions that are not intended for by the director/news-anchor.
<p>
Increasing accessibility of 360 cameras will allow more creative individuals to contemplate the artistic representation and navigation issues of 360 video. It is only beneficial to allow more people to address the challenge which can improve the collective understanding of art, filmmaking and journalism.

<h3> Pipeline </h3>
<img src="img/doc/pl.jpg" alt="Pipeline" width="900px"/>

<h3> Directories </h3>

`/vr180` - VR180 software tooling
`/360warper` - <b>(Deprecated)</b> A standalone image/video warper to be used with video compositing tools like AFX, Blender, Nuke<br>
`/cam` - Camera hardware and software configuration. 3D printable 360 camera rig files (`.stl`), generator files (`.scad`) and camera control commands. Camera intrinsics/extrinsics characterization for use in software tooling.<br>
`/cpp` - <b>(Deprecated)</b> Automated video stitching (in development, see video tutorial below for manual solution) <br>
`/img` - Documentation, reference and source images <br>
`/viewer` - <b>(Deprecated)</b> Basic VR 360 video/image player for web browsers

<h3> Multi-camera system overview </h3>

<img src="img/doc/sj4000_6x_01.jpg" alt="Version 2" width="420px"/>
<img src="img/doc/sj4000_6x_v3_01.jpg" alt="Version 3" width="420px"/>

Version 2 left, pictured without upper mount. Version 3 on right with optional top camera expansion module.

Captures monoscopic video in 360 degrees horizontal and ~135 degrees vertical. Six individual streams are stitched and blended together into a cohesive panoramic video.
<p>
6 SJ4000 cameras arranged outwards is just barely enough information to create a 360 video and requires a decent amount of user input to stitch in After Effects. Using 5 SJ4000 cameras seated horizontally would help automated stitching a lot at the cost of some vertical field of view. Using more cameras is ideal, but drives up cost and requires a more powerful computer for stitching. Modifying lenses of the cameras with wider angle replacements is also a solution. If planning to build with out-of-box SJ4000s oriented vertically, using 8 cameras is a safe option, 7 will be bare minimum.
<p>


<h3> Bill of Materials - Hardware & Software </h3>

Listed below are items I have tested but any of them can be swapped out with other items that serve a similar purpose. Search [3DHubs](https://www.3dhubs.com) to find access to a nearby 3D printer. Some libraries have printers as well. Or build your own [RepRap](http://www.reprap.org) as a support project.

<h5> Hardware </h5>

<img src="img/doc/bom.jpg" alt="Bill of Materials" width="900px"/>

<b>a.</b> 6 or more SJ4000 action cameras (~$60 each), Alternatives include GoPro, Xiaomi (may need to modify 3D models). Can use less cameras by replacing stock lenses with wide angle lenses for Xiaomi and SJ4000.
<br>
<b> b. </b> A lower bracket/apparatus to seat cameras. Modify existing CAD designs in `/cam` directory to meet camera choice.
<br>
<b> c. </b> A matching upper apparatus to secure cameras. Likewise, modify existing CAD designs in `/cam` directory to meet camera choice.
<br>
<b> d. </b> (Optional) Zenith module for housing a single additional zenith camera i.e. "Sky Camera."
<br>
<b> e. </b> Standard Tripod
<br>
<b> f. </b> Standard computer, relatively powerful GPU for image processing preferred
<br>
<b> g. </b> Any mobile device with a modern web browser
<br>
<b> h. </b> A headmounted display to house mobile device for VR viewing like [Cardboard](https://www.google.com/get/cardboard/) or print one from [Thingiverse](http://www.thingiverse.com/search?q=vr+headset).
<p>
<b> h1 </b> - 1x 1/4"-20 bolt for securing top mount to bottom mount. For SJ4000s, length of bolt must be > 3". May be different for other camera selections. Ideally, choose a length that allows bolt to stick past bottom mount an inch or two to connect to tripod with coupling bolt 'h7.'
<br>
<b> h2 </b> - 2x Washers for 1/4"-20 bolt.
<br>
<b> h3 </b> - 1x 1/4"-20 nut for tightening under bottom mount.
<p>
<i>h4 - h6 only applies if using top camera module</i>
<br>
<b> h4 </b> - 2x 6-32 screws (> 1.5" length) to mount top camera expansion module.
<br>
<b> h5 </b> - 4x #6 washers.
<br>
<b> h6 </b> - 2x 6-32 nuts.
<p>
<b> h7 </b> - 1x 1/4"-20 coupling nut for joining entire setup to tripod head.
<p>

(Optional) Switch to gen-lock/sync record controls for cameras, necessary to mitigate rolling shutter when capturing scenes with fast motion, also provides convenience for starting/stopping recording, requires soldering
<center>
<img src="img/doc/genlock_switch.jpg" alt="Genlock Switch" width="420px"/>
<center>
<p>

<h5> Software list </h5>

 * [OpenSCAD](http://www.openscad.org) for rapid, parameterized prototyping of 3D printable VR camera mounts (Free)
 * [After Effects CC](http://www.adobe.com/products/aftereffects.html) for basic stitching using warp plugins (Free trial, then monthly subscription)
 * (Optional) Stitching plugins for After Effects like [Skybox Mettle](http://www.mettle.com/product/skybox/), [PTGui](https://www.ptgui.com/), [Kolor](http://www.kolor.com/) or [VideoStitch](http://www.video-stitch.com/) for professional stitching results (> $90)
 * [Blender](https://www.blender.org/) for alternative stitching method to After Effects, also includes capabilites to composite text and 3D. (Free)
 * [YouTube 360 Injector](https://support.google.com/youtube/answer/6178631?hl=en) for tagging proper metadata to final 360 video for properly uploading to YouTube (Free)

<h3> Stitching Techniques</h3>

[Video Tutorial: Hand-stitching in After Effects (Quick)](https://www.youtube.com/watch?v=5elOFvyL4KA) <br>
[Video Tutorial: Hand-stitching (Detailed using 360warper script)](https://www.youtube.com/watch?v=F78drmyd21I)<br>
<i>or</i> <br>
For fully automated process,compile and run stitcher in `/cpp` (still in the works) <p>

Stitcher uses ORB feature descriptor pairs to register overlapping images. ORB is also free from patent restrictions.
<img src="img/doc/r8match.jpg" alt="Feature Matches" width="900px"/>
<img src="img/doc/r8.jpg" alt="Feature Matches" width="900px"/>

<h3> Viewing </h3>

Web VR 360 Player - [Demo Link](https://cdn.rawgit.com/felixtsao/oneVR_devel/master/viewer/index.html)
<p>
Visit `/viewer` directory for usage and source code.

<h3> 3D Printing </h3>

Print camera mount yourself or find a local printer through [3DHubs](https://www.3dhubs.com) or the local library.

Older version of `mono_lower_6x_sj4000.stl` printed on a Makerbot Replicator 2 with Red PLA. The mount takes about 4 hours to print if things go smoothly! Newer version of mount printed on an open-source Prusa i3 design, right, with a layer height of 0.27mm through a 0.4mm extruder nozzle.

<img src="/img/doc/sj4000_6x_makerbot.jpg" alt="Makerbot" width="420px"/>
<img src="/img/doc/sj4000_6x_v3_prusai3.jpg" alt="Prusa i3" width="420px"/>

<h5> Creating a new camera mount </h5>

Files are located in `/cam` directory, organized by camera model. To create a new camera mount, open the closest existing `.scad` file and adapt it by changing the camera trench dimensions. Camera dimensions are listed as variables at the top of the `.scad` files and should globally change the trench sizes across the mount. To add more cameras, simply increase the distance of the optical center and make additional copies of the trenches and assign each trench with the appropriate angle. The numbers are technically unitless but they default to millimeter for most printers. Be sure to add 1 to the value used for the camera dimension for a little breathing room.

<h3> // TODO </h3>

 * Create automated video stitching (Implement feature detector)
 * <strike>Top/bottom camera transform for AFX stitch method using Blender or script</strike><br>
 * <strike>Make video tutorial for hand-stitching 360 videos in AFX</strike>
 

<h3> Technical References </h3>

1) [Course: Computational Photography CS534, UW-Madison, Dyer](http://pages.cs.wisc.edu/~dyer/cs534/) <br>
2) [Text: Chapter 9, Computer Vision, Szeliski](http://szeliski.org/Book/) <br>
3) [Paper: ORB Feature Descriptor, Rublee](http://www.vision.cs.chubu.ac.jp/CV-R/pdf/Rublee_iccv2011.pdf) <br>
4) [Docs: OpenCV](http://docs.opencv.org/3.1.0/) <br>
5) [Docs: ThreeJS](https://threejs.org/) <br>
6) [Docs: RepRap](http://www.reprap.org/)
