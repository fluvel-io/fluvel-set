Python Bindings
===============

The Python bindings provide access to Fluvel IP from Python through a
thin pybind11-based interoperability layer.

They expose the main image processing, segmentation and analysis
capabilities of the library while preserving the performance and API
semantics of the underlying C++ implementation.

The bindings are organized into two complementary areas:

- **Python Interoperability**: type conversions and integration utilities
  used to bridge Python and C++.
- **Python Bindings**: public Python API exposing Fluvel IP features to
  Python applications and scripts.

Image conventions
-----------------

Images are ``uint8`` NumPy arrays of shape ``(H, W)``, ``(H, W, 3)`` or
``(H, W, 4)``. The package does not depend on any imaging library.

For color images, the functions where the order of the channels matters
(``active_contour``) take a ``channel_order`` argument, which can be
``fluvel.ChannelOrder.BGR`` (default) or ``fluvel.ChannelOrder.RGB``.
The image is read in place: no pixel is copied or reordered.

.. list-table::
   :header-rows: 1

   * - Image comes from
     - ``channel_order``
   * - OpenCV (``cv2.imread``, camera capture)
     - ``BGR`` (default)
   * - Pillow, matplotlib, scikit-image, imageio
     - ``RGB``
   * - An OpenCV image converted with ``cv2.cvtColor(..., cv2.COLOR_BGR2RGB)``
     - ``RGB``
   * - Grayscale
     - ignored

When does the order matter?
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The channel order changes the result only if the algorithm treats the
channels differently:

- with the default settings (RGB color space, equal weights), the result
  is the same whatever the order;
- with unequal per-channel weights (``RegionModelParams.weights``), the
  weights are applied to the wrong channels if the order is wrong;
- with another color space (YUV, Lab, Luv), the conversion from RGB is
  computed on the wrong channels if the order is wrong.

Grayscale images have no channel order. When in doubt, give the order of
the library that produced the image.

Example with an image loaded by Pillow:

.. code-block:: python

   import fluvel
   from PIL import Image
   import numpy as np

   image = np.asarray(Image.open("input.png").convert("RGB"))
   result = fluvel.active_contour(image, channel_order=fluvel.ChannelOrder.RGB)

.. toctree::
   :maxdepth: 1

   python_interop
   python_bindings
