<p align="center">
  <img src="assets/logo/fluvel.svg" width="96">
</p>

<h1 align="center" style="color:#6b6fcf;">
  Fluvel Set
</h1>

<p align="center">
  <strong>
    Real-time image segmentation and active contour experimentation platform
  </strong>
</p>

<p align="center">
  Modern C++ framework for active contour algorithms,
  real-time visualization and reproducible experimentation.
</p>

<p align="center">

  <img src="https://img.shields.io/badge/license-CeCILL--2.1-6b6fcf">

  <img src="https://img.shields.io/badge/C%2B%2B-23-6b6fcf">

  <img src="https://img.shields.io/badge/Qt-6-4fc98a">

</p>

---

# Demo

## Real-time tracking

<p align="center">
  <img src="assets/gifs/phone_tracking.gif" width="900">
</p>

<p align="center">
  <a href="assets/videos/phone_tracking.webm">
    Open full video
  </a>
</p>

## MRI contour evolution

<p align="center">
  <img src="assets/gifs/mri_segmentation.gif" width="900">
</p>

<p align="center">
  <a href="assets/videos/mri_segmentation.webm">
    Open full video
  </a>
</p>

## 🚀 Install

### Desktop application

Latest builds for Windows, macOS and Linux (with per-platform instructions) are
available on the [download page](https://fluvel-io.github.io/fluvel-set/).

On Linux, two packages are provided:

- **Flatpak** (recommended): runs on any distribution that supports Flatpak.
- **AppImage**: uses the host's C and C++ libraries, so it needs glibc 2.39 or
  newer (for example Ubuntu 24.04, Debian 13 or Fedora 40 and later). On older
  systems, use the Flatpak.

### Python package

Pre-built wheels (Linux x86_64 with glibc 2.34 or newer, Windows x64, macOS 13+) are attached to the
[latest-python release](https://github.com/fluvel-io/fluvel-set/releases/tag/latest-python):

```bash
python3 -m venv .venv && source .venv/bin/activate   # Windows: .venv\Scripts\activate
pip install fluvel-set --find-links https://github.com/fluvel-io/fluvel-set/releases/expanded_assets/latest-python
pip install "fluvel-set[examples]"   # adds OpenCV, used by the scripts of examples/
```

Then:

```python
import fluvel
```

From a clone of the repository, `pip install .` builds the package from source
(a C++23 compiler and CMake ≥ 3.22 are required).

---

## 📚 Documentation

The online documentation is generated automatically from the source code
and follows the latest development version.

Documentation is versioned and tied to specific commits
for reproducibility and long-term experimentation.

[Documentation site](https://fluvel-io.github.io/fluvel-set/docs/)

---

## ✨ Features

- Region-based active contour evolution
- Real-time image processing
- Video stream support
- Modular architecture for feature extensions
- Qt-based visualization interface
- Reproducible builds (CMake, Flatpak, AppImage)

---

## 🧠 Overview

Fluvel Set is a research-oriented image segmentation platform
focused on region-based active contour methods.

The project emphasizes:

- Modern C++ architecture
- Real-time experimentation
- Separation between processing engine and UI
- Reusable image processing components
- Reproducible scientific workflows

The processing core does not depend on Qt.
Qt is used only for visualization and interaction.

---

## 🏗️ Architecture

The project is organized into:

```text
fluvel-set/
├── src/                           # Source code
│   ├── app/                       # Desktop/mobile Qt application
│   │   ├── ui/                    # Windows and reusable UI components
│   │   ├── qml/                   # Experimental UI layer
│   │   ├── core/                  # Application logic and orchestration
│   │   └── interop/               # Application integration and adapters
│   │
│   ├── bindings/                  # Language and scripting integrations
│   │   └── python/                # Python bindings (pybind11)
│   │
│   └── image_processing/          # Image processing library
│       ├── filtering/             # Filtering, denoising and preprocessing
│       ├── segmentation/          # Active contours and segmentation models
│       ├── analysis/              # Shape analysis and image metrics
│       └── core/                  # Shared processing infrastructure
│
├── examples/                      # Python examples demonstrating Fluvel IP features
│   ├── filtering/                 # Image filtering examples and experiments
│   ├── hausdorff/                 # Shape comparison and Hausdorff distance examples
│   ├── segmentation/              # Active contour segmentation examples
│   ├── workflow/                  # Complete processing pipeline examples
│   ├── resources/                 # Shared datasets and images used by examples
│   └── utils.py                   # Shared utilities used across examples
│
├── tests/                         # Automated tests
│   └── python/                    # pytest suite of the image processing library (via the bindings)
│
├── resources/                     # Icons and application assets
├── translations/                  # Application translations (.ts, .qm)
│
├── scripts/                       # Development, packaging and utility scripts
│                                  # Includes local testing and maintenance helpers
│
├── docs/                          # Documentation configuration and assets
├── web/                           # Project homepage and download portal
│
├── cmake/                         # Shared CMake modules and configuration
├── packaging/                     # Flatpak, AppImage and distribution files
│
├── .github/                       # CI workflows, releases and automation
│
└── CMakeLists.txt                 # Main build configuration
```

### Main modules

- **fluvel_app** — Qt desktop application (user interface and orchestration)
- **fluvel_ip** — Image processing and segmentation library (C++)
- **fluvel_bindings** — Python bindings for Fluvel IP (pybind11/C++)

---

Fluvel follows a hybrid architecture:

- **Technical organization** for application layers (UI, integration and execution flow)
- **Functional organization** for image processing modules (filtering, segmentation and analysis)

Build, release and deployment automation are intentionally kept outside the source tree and are handled through repository workflows and helper scripts.

Some components (such as additional UI technologies and language integrations) are experimental and may evolve over time.

## 🛠️ Build

### Requirements

#### Core library

- CMake ≥ 3.22
- C++23 compiler (tested with Clang, GCC and MSVC)

#### Desktop application

Additional dependencies:

- Qt6 (Core, Gui, Widgets, Multimedia, MultimediaWidgets, Svg, LinguistTools),
  Qt 6.7 or later (required for `QIcon::ThemeIcon`), tested with Qt 6.9 and 6.11

Optional:

- FFmpeg development libraries (`libavformat`, `libavcodec`, `libavutil`,
  `libswscale`) to enable video recording and export.
  Off by default; enable it with `-DFLUVEL_ENABLE_FFMPEG=ON`.
  FFmpeg is found with pkg-config on Linux and macOS, and through Conan
  on Windows.

#### Python bindings

Additional dependencies:

- Python ≥ 3.11
- pybind11
- NumPy ≥ 1.23

Optional (examples only):

- OpenCV

---

### Build desktop application

```bash
cmake -S . -B build \
    -DFLUVEL_BUILD_APP=ON \
    -DFLUVEL_BUILD_BIND=OFF

cmake --build build
./build/src/app/fluvel-set
```

---

### Build Python bindings

```bash
cmake -S . -B build \
    -DFLUVEL_BUILD_APP=OFF \
    -DFLUVEL_BUILD_BIND=ON

cmake --build build
```

Generated module:

```text
build/python/fluvel.*
```

Install runtime dependencies for examples:

```bash
pip install numpy opencv-python
```

Example:

```bash
python examples/filtering/showcase.py
```

Image conventions of the Python functions:

- Images are `uint8` NumPy arrays of shape `(H, W)`, `(H, W, 3)` or `(H, W, 4)`.
  Other types (`float`, `uint16`, ...) are refused with an explicit error.
- Color channels are in **BGR** order by default, like OpenCV. For an RGB image
  (Pillow, matplotlib, scikit-image, ...), pass
  `channel_order=fluvel.ChannelOrder.RGB`: the image is read in place, without
  being copied. The order only matters for color segmentation (`active_contour`),
  and only when the per-channel weights are not all equal or when the color space
  is not RGB: with the default settings, the result is the same either way.
- Results have the same shape as the input image.
- Segmentation results provide the contour points and the region mask (`result.mask`).
- `hausdorff_metrics` returns exact distances (Hausdorff, modified, quantile) in absolute
  pixel coordinates, like MONAI or MedPy. Use `align_centroids=True` to ignore the
  translation between the contours and measure the difference of shape only, as the
  Hausdorff window of the application can do. The quantile is taken in each direction
  (nearest rank) and the larger value is kept.

Run the tests (requires `pytest`):

```bash
PYTHONPATH=build/python python -m pytest tests/python
```

---

### Build everything

```bash
cmake -S . -B build \
    -DFLUVEL_BUILD_APP=ON \
    -DFLUVEL_BUILD_BIND=ON

cmake --build build
```

---

## 📜 License

Fluvel is licensed under the CeCILL 2.1 license
(CEA · CNRS · INRIA).

CeCILL is a French free software license compatible with the GNU GPL,
widely used in research and academic software.

[CeCILL 2.1 License](https://cecill.info/licences/Licence_CeCILL_V2.1-en.html)
