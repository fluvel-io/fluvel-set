// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "active_contour_py.hpp"
#include "filtering_py.hpp"
#include "hausdorff_py.hpp"
#include "noise_py.hpp"

// Provided by CMake (see src/bindings/python/CMakeLists.txt).
#ifndef FLUVEL_VERSION
#define FLUVEL_VERSION "unknown"
#endif

#ifndef FLUVEL_GIT_COMMIT
#define FLUVEL_GIT_COMMIT "unknown"
#endif

PYBIND11_MODULE(fluvel, m)
{
    m.doc() = "Fluvel Set Python bindings";

    bindNoise(m);
    bindFiltering(m);
    bindActiveContour(m);
    bindHausdorff(m);

    // Traceability of the build: version of the project and commit it was built from.
    m.attr("__version__") = FLUVEL_VERSION;
    m.attr("__commit__") = FLUVEL_GIT_COMMIT;
}