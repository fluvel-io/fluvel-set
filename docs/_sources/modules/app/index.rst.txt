Qt Application
==============

The Qt application provides the graphical user interface and runtime
infrastructure of Fluvel Set.

It is responsible for user interaction, session management, camera and
image workflows, and integration with the underlying Fluvel IP image
processing library.

The application is organized into four main areas:

- **UI**: windows, dialogs and reusable user interface components.
- **Application Core**: controllers, settings management and execution workflows.
- **Media**: video export infrastructure and media backend integrations.
- **Interoperability**: adapters and integration layers connecting the application
  to external libraries and processing backends.

Architecture
------------

Fluvel Set follows a mixed MVC/MVP architecture. The controllers drive the
session (sources, threads, workers, settings) and also play the role of
presenter: they turn the results of the algorithm into what the view
displays. The view is passive and the windows only connect the components
together. The algorithm itself, in Fluvel IP, does not depend on Qt.

.. raw:: html

   <div style="text-align:center;">
     <a href="../../_static/architecture_layers.svg" target="_blank">
       <img src="../../_static/architecture_layers.svg"
            style="max-width:420px; width:100%; height:auto;">
     </a>
   </div>

Video session
-------------

Commands go from the window to the algorithm. Frames go the other way: the
video source delivers a frame, the processing thread keeps only the latest
one, runs the filters and the active contour within a time budget, and the
controller prepares the display data for the viewer.

The colored frames show where each component runs. The interface, the
controller and the viewer live in the GUI thread. The video source belongs to
the Qt Multimedia backend, which delivers frames from its own threads. The
processing thread runs the algorithm. A dashed arrow that crosses a frame is
a signal delivered across threads: the receiver handles it later, in its own
thread.

.. raw:: html

   <div style="text-align:center;">
     <a href="../../_static/video_session_sequence.svg" target="_blank">
       <img src="../../_static/video_session_sequence.svg"
            style="max-width:760px; width:100%; height:auto;">
     </a>
   </div>

Image session
-------------

The image worker is driven by a timer: each tick runs a short time slice of
the active contour, so the interface stays responsive while the contour
evolves.

Unlike the video session, everything runs in the GUI thread: the worker is a
plain object driven by a timer, not a thread. The preprocessing filters run
synchronously when the image is initialized or restarted, so the interface
waits until they are done. Only the active contour iterations are split into
time slices.

.. raw:: html

   <div style="text-align:center;">
     <a href="../../_static/image_session_sequence.svg" target="_blank">
       <img src="../../_static/image_session_sequence.svg"
            style="max-width:700px; width:100%; height:auto;">
     </a>
   </div>

Recording
---------

Recording is handled by the capture controller, independently of the video
session: it only receives the frames that the window gives it, so it can be
reused with other sources. The window draws the contour layers on the
displayed image before submitting it, so the recorded video shows what the
user sees.

The frames go through a buffer protected by a mutex: the GUI thread pushes
them and a dedicated recorder thread pops and encodes them with FFmpeg. The
buffer keeps frames in memory and spills them to disk when its limit is
reached. When the recording stops, the recorder thread first drains the
buffer, then closes the file and, if requested, converts the segments to
standard MP4. Its signals reach the controller in the GUI thread.

.. raw:: html

   <div style="text-align:center;">
     <a href="../../_static/recording_sequence.svg" target="_blank">
       <img src="../../_static/recording_sequence.svg"
            style="max-width:900px; width:100%; height:auto;">
     </a>
   </div>

Snapshot
--------

A snapshot is simpler. The window copies the displayed image, draws the
contour layers on the copy and gives it to the capture controller. The
controller puts it in a small bounded queue, and a dedicated thread writes the
files one at a time, so saving never blocks the interface. The result, a file
name or an error, comes back to the controller and is shown to the user.

.. raw:: html

   <div style="text-align:center;">
     <a href="../../_static/snapshot_sequence.svg" target="_blank">
       <img src="../../_static/snapshot_sequence.svg"
            style="max-width:900px; width:100%; height:auto;">
     </a>
   </div>

Configuration
-------------

Settings do not follow the data path. They are stored in the
``ApplicationSettings`` singleton, which emits a signal when they change, and
three listeners react independently:

* the **controller** forwards the processing settings to the thread or worker
  (they are applied from the next frame) and restyles the current layers, so
  a display change is visible immediately, even when the video is paused;
* the **window** applies the view settings (mirror, smoothing, minimap,
  overlay) directly to the viewer, without going through the controller;
* the **window** also refreshes its own widgets.

.. raw:: html

   <div style="text-align:center;">
     <a href="../../_static/config_flow.svg" target="_blank">
       <img src="../../_static/config_flow.svg"
            style="max-width:620px; width:100%; height:auto;">
     </a>
   </div>

.. toctree::
   :maxdepth: 2

   ui/index
   app_core
   app_media
   app_interop
