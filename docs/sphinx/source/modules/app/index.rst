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

The image worker lives in its own thread. The controller only posts commands
to it, so the interface never waits for the filters or for the contour
iterations. In interactive mode the worker is paced by the clock: every 16 ms
(about 60 frames per second) it computes one iteration and publishes the
contour, so the evolution stays visible even on small images that would
converge in a few milliseconds. It never computes ahead of the display: the
controller acknowledges each frame it has consumed. When one iteration takes
longer than a frame, the worker simply runs as fast as it can. In converge mode
the worker runs at full speed, without pacing, and publishes only the final
result.

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
* the **window** also refreshes its own widgets;
* the **capture controller** keeps its own copy of the snapshot and recording
  preferences. It never reads the singleton: the window gives it the values at
  start-up and whenever they change, and each snapshot request carries a copy
  of them.

The controllers do not know the singleton either. It is the window that
connects its signals to their slots, so a controller can be used with any
source of settings.

.. raw:: html

   <div style="text-align:center;">
     <a href="../../_static/config_flow.svg" target="_blank">
       <img src="../../_static/config_flow.svg"
            style="max-width:620px; width:100%; height:auto;">
     </a>
   </div>

Settings dialogs
~~~~~~~~~~~~~~~~

The session dialogs (image and video) never read or write the singleton. A
dialog receives its configuration once, when it is created, and keeps two
copies: the *committed* one, which is what the application uses, and the
*edited* one, which follows the widgets while the dialog is open.

* **OK** copies the edited configuration to the committed one and emits a
  signal. The window connects that signal to the setter of
  ``ApplicationSettings``, which then notifies the controllers.
* **Cancel** restores the edited configuration from the committed one and
  refreshes every widget, so the dialog always shows the configuration that is
  really applied when it is opened again.

The dialog keeps its own copy, and nothing refreshes it afterwards. This is
correct as long as the dialog is the only code that changes these settings,
which is the case today: the only writers of the session settings are the two
windows, and each of them only forwards the signal of its own dialog. If a new
path starts to modify them (loading a preset, resetting the settings, another
window), the owner of the dialog must give it the new configuration, otherwise
the dialog would show and later commit a stale copy.

The preferences dialog (language, snapshots, video recording, recording buffer)
follows the same user-visible behavior, with a different mechanism. It is a
single global dialog with no owning window to relay its values, so it reads and
writes ``ApplicationSettings`` itself.

* **Opening** the dialog reloads every widget from the application settings, so
  it always shows what is really applied. Opening it again while it is already
  visible keeps the pending edits.
* **OK** writes the values to the singleton, which notifies the windows, and
  saves them to disk.
* **Cancel** (or closing the window) writes nothing. The edits are discarded
  by the reload of the next opening.
* **Restore Defaults** fills the widgets with the factory values, like the
  same button of the session dialogs. Nothing is applied until OK, and Cancel
  discards it. The default folders depend on the platform (standard pictures
  and movies locations given by Qt), so they come from
  ``ApplicationSettings::defaultSnapshotPreferences()`` and
  ``defaultVideoRecordingPreferences()``, which the loading of the settings
  also uses. The language goes back to *System*, which, as any language change,
  takes effect after a restart.

Reloading at each opening replaces the *committed*/*edited* copies: there is no
second copy that could become stale, so the limit described above does not
apply to this dialog.

.. toctree::
   :maxdepth: 2

   ui/index
   app_core
   app_media
   app_interop
