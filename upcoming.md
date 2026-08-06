# Upcoming Features

### Renderer

* Added One-pass rendering for simultaneous audio and video rendering *(TEST)*.
* Completely overhauled the rendering system.
* Added Zero-Copy and CUDA acceleration.
* Added dedicated hardware acceleration for AMD GPUs.
* Rendering is up to 10× faster.
* Optimized rendering for extremely large macros *(tested at 248k TPS and 2M+ inputs)*.
* Added real-time 4K mobile rendering *(currently locked at 1× speed)*.
* Added New FFmpeg API Render *(TEST)*.
* Added FFmpeg library loading and fixed `ERROR_MOD_NOT_FOUND`.
* Added automatic resolution selection for mobile rendering.
* Added proper codec information for Windows and Mobile architectures.
* Added unlimited render presets.
* Added Render HUD with real-time rendering speed.
* Added render preview with custom arguments *(PC only)*.
* Added fast video filters.
* Improved rendering smoothness and performance.
* Fixed rendering crashes on heavy levels.
* Fixed rendering freezes and finalization issues.
* Fixed rendering stopping abruptly at the end of macros.
* Fixed robot animations during rendering.
* Fixed numerous mobile rendering issues.
* Fixed mobile AutoResize resolution detection.
* Fixed mobile resolution flipping through the Help button.
* Fixed various rendering and stability issues.

### Macro System

* Added new compressed `.cml` macro format.
* Updated `.cml` compression.
* Fixed `.cml` corruption at high frame counts and far coordinates.
* Fully fixed `.slc` format.
* Added export support for `.cml`, `.json`, `.gdr`, and `.gdr2`.
* Added support for tcBot and Silicate macro formats.
* Completely updated Vanilla macro handling.
* Fixed sub-frame macros when using CBS.
* Fixed random deaths and missed initial inputs.
* Fixed ghost Release inputs on restart and respawn.
* Fixed Practice Fix ghost inputs breaking Wave Trail.
* Fixed Practice Fix breaking bot execution.
* Fixed input holding on level restart and eliminated high-CPS bursts.
* Fixed massive 1-frame CPS bursts when playing macros from StartPos.
* Heavily improved AutoSave.
* Added Macro Button in the Editor.

### Botting & Gameplay

* Added AutoStraightFly.
* Added AutoWaveSpam.
* Heavily improved PathFinder.
* Added automatic checkpoint placement to PathFinder *(PC only)*.
* Fixed PathFinder freezing/hanging on mobile devices.
* Added FakeTaps during macro playback.
* Reduced the FakeTaps circle size for more realistic-looking clicks.
* Updated TPS Bypass.
* Added Sync TPS With FPS.
* Fixed TPS Bypass status display.
* Updated Frame Stepper with proper Hold support.
* Restored Show Trajectory with several fixes.
* Added support for all game modes *(Wave and Ball may still have issues)*.
* Dual Portals are currently unsupported and may break.

### Interface

* Updated the interface with a more colorful design.
* Added animated gradient glow balls inspired by HyperOS 3.0.
* Added menu background opacity settings.
* Updated Blur with configurable target areas.
* Added official Discord server button.
* Added Version Manager with version downgrading support.
* Added a new Dev-beta/Main release system.
* Added proper codec information for different architectures.

### Platform & Compatibility

* Added Windows and Android 32/64-bit builds.
* Added iOS build support.
* Updated to Geode SDK 5.8.2 *(loader update required)*.
* Fixed numerous GitHub issues and general stability bugs.
* Improved compatibility across supported platforms.

### Removed

* Removed Ghost Playback as obsolete.
