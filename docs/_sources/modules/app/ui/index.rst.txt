UI
==

The user interface layer provides the visual components of the Qt
application.

It includes application windows, dialogs and reusable widgets used to
build image and video processing workflows while remaining separated
from the underlying processing and controller logic.

Image Viewer Architecture
-------------------------

The ImageViewerWidget delegates user interactions to an
ImageViewerInteraction implementation. Interactions are composed
from independent behaviors such as panning, color picking,
drag-and-drop and fullscreen support.

The viewer is passive: it draws the image and the point layers it receives
from the controllers, and displays them according to a view configuration.

.. raw:: html

   <div style="text-align:center;">
     <a href="../../../_static/image_viewer_behaviors.svg"
        target="_blank">
       <img src="../../../_static/image_viewer_behaviors.svg"
            style="max-width:300px; width:100%; height:auto;">
     </a>
   </div>

An input event goes from the viewer to the interaction set, which gives it to
the behaviors in priority order. The first behavior that handles it stops the
dispatch. While a behavior is capturing the mouse (a pan in progress, for
example), it receives the mouse press, move and release events alone. A
behavior acts on the viewer, or reports to the rest of the application through
a signal or the listener.

.. raw:: html

   <div style="text-align:center;">
     <a href="../../../_static/image_viewer_events.svg"
        target="_blank">
       <img src="../../../_static/image_viewer_events.svg"
            style="max-width:900px; width:100%; height:auto;">
     </a>
   </div>

.. toctree::
   :maxdepth: 1

   windows
   components
