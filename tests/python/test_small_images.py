# SPDX-License-Identifier: CeCILL-2.1
# Copyright (C) 2010-2026 Fabien Bessy

"""
Edge cases of the Fluvel Python bindings: tiny images and kernels larger than
the image.

These are the cases where indexing mistakes show up (a neighborhood that goes
outside the image, a width or a height of 1, a contour with a single pixel).
They are tedious to check by hand in the graphical interface, so they are
checked here.

The expected values do not depend on how the borders are handled: they are
properties that hold for any reasonable border rule (constant images stay
constant, a kernel that covers the whole image gives a global statistic,
erosion <= image <= dilation).

Run from the repository root, with the directory containing the compiled
module in the Python path:

    PYTHONPATH=build-bindings/python python -m pytest tests/python
"""

import numpy as np
import pytest

fluvel = pytest.importorskip("fluvel")


# Width and height of 1 or 2 are the usual sources of out-of-bounds accesses.
TINY_SHAPES = [(1, 1), (1, 2), (2, 1), (2, 2), (1, 7), (7, 1), (3, 3), (2, 5), (8, 8)]

# Radii from "inside the image" to "far larger than the image".
RADII = [1, 2, 5, 50, 1000]

CHANNELS = [None, 3, 4]

FILTERS = {
    "mean": fluvel.mean,
    "median": fluvel.median,
    "dilate": fluvel.dilate,
    "erode": fluvel.erode,
    "opening": fluvel.opening,
    "closing": fluvel.closing,
    "top_hat": fluvel.top_hat,
    "black_top_hat": fluvel.black_top_hat,
    "gradient": fluvel.gradient,
}

# Filters whose output is the input itself on a constant image.
IDEMPOTENT_ON_CONSTANT = ["mean", "median", "dilate", "erode", "opening", "closing"]

# Filters whose output is zero on a constant image.
ZERO_ON_CONSTANT = ["top_hat", "black_top_hat", "gradient"]


def random_image(shape, channels, seed=0):
    full = shape if channels is None else (*shape, channels)
    return np.random.default_rng(seed).integers(0, 256, full, dtype=np.uint8)


def constant_image(shape, channels, value=77):
    full = shape if channels is None else (*shape, channels)
    return np.full(full, value, np.uint8)


def planes(image):
    """The channels of an image, each of them as a 2D array."""
    if image.ndim == 2:
        return [image]

    return [image[..., c] for c in range(image.shape[2])]


#
# Filters on tiny images with any radius
#


@pytest.mark.parametrize("channels", CHANNELS)
@pytest.mark.parametrize("radius", RADII)
@pytest.mark.parametrize("shape", TINY_SHAPES)
@pytest.mark.parametrize("name", FILTERS)
def test_filters_accept_tiny_images_with_any_radius(name, shape, radius, channels):
    image = random_image(shape, channels)

    out = FILTERS[name](image, radius)

    assert out.shape == image.shape
    assert out.dtype == np.uint8


@pytest.mark.parametrize("channels", CHANNELS)
@pytest.mark.parametrize("radius", RADII)
@pytest.mark.parametrize("shape", TINY_SHAPES)
@pytest.mark.parametrize("name", IDEMPOTENT_ON_CONSTANT)
def test_constant_image_is_unchanged_by_the_filters(name, shape, radius, channels):
    image = constant_image(shape, channels)

    out = FILTERS[name](image, radius)

    assert np.array_equal(out, image)


@pytest.mark.parametrize("channels", CHANNELS)
@pytest.mark.parametrize("radius", RADII)
@pytest.mark.parametrize("shape", TINY_SHAPES)
@pytest.mark.parametrize("name", ZERO_ON_CONSTANT)
def test_constant_image_has_no_top_hat_or_gradient(name, shape, radius, channels):
    image = constant_image(shape, channels)

    out = FILTERS[name](image, radius)

    # The alpha channel of a 4-channel image is copied, not filtered.
    assert not out[..., :3].any() if channels else not out.any()


@pytest.mark.parametrize("channels", CHANNELS)
@pytest.mark.parametrize("radius", RADII)
@pytest.mark.parametrize("shape", TINY_SHAPES)
def test_erosion_is_below_the_image_and_dilation_above(shape, radius, channels):
    image = random_image(shape, channels)

    eroded = fluvel.erode(image, radius)
    dilated = fluvel.dilate(image, radius)

    assert (eroded <= image).all()
    assert (image <= dilated).all()


def reference_filter(plane, radius, reduce, clipped=False):
    """
    Brute-force square window.

    The borders are replicated, or the window is clipped to the image (clipped=True).
    """
    height, width = plane.shape
    padded = np.pad(plane.astype(np.int64), radius, mode="edge")
    out = np.empty_like(plane)

    for y in range(height):
        for x in range(width):
            if clipped:
                window = plane[max(0, y - radius) : y + radius + 1, max(0, x - radius) : x + radius + 1]
            else:
                window = padded[y : y + 2 * radius + 1, x : x + 2 * radius + 1]

            out[y, x] = reduce(window)

    return out


REFERENCES = {
    "dilate": lambda w: w.max(),
    "erode": lambda w: w.min(),
    "median": lambda w: np.sort(w, axis=None)[w.size // 2],
    "mean": lambda w: int(round(w.mean())),
}

@pytest.mark.parametrize("name", ["dilate", "erode", "mean", "median"])
@pytest.mark.parametrize("radius", [1, 2, 3, 5, 50])
@pytest.mark.parametrize("shape", [(1, 1), (2, 2), (3, 3), (2, 5), (5, 2), (1, 6), (6, 1), (8, 8), (9, 13)])
def test_filters_match_a_brute_force_reference(name, shape, radius):
    image = random_image(shape, None, seed=4)

    out = FILTERS[name](image, radius)
    # The median uses the window clipped to the image, the others replicate the borders
    # (identical for dilate and erode).
    expected = reference_filter(image, radius, REFERENCES[name], clipped=(name == "median"))

    # The mean is computed in two rounded passes: one level of difference is allowed.
    tolerance = 1 if name == "mean" else 0

    assert (np.abs(out.astype(int) - expected.astype(int)) <= tolerance).all()


@pytest.mark.parametrize("channels", CHANNELS)
@pytest.mark.parametrize("name", ["mean", "median"])
@pytest.mark.parametrize("radius", RADII)
@pytest.mark.parametrize("shape", TINY_SHAPES)
def test_mean_and_median_stay_within_the_input_range(shape, radius, channels, name):
    # Whatever the border rule, an average of input values cannot leave their range.
    image = random_image(shape, channels, seed=6)

    out = FILTERS[name](image, radius)

    for plane_in, plane_out in zip(planes(image), planes(out)):
        assert plane_out.min() >= plane_in.min()
        assert plane_out.max() <= plane_in.max()


@pytest.mark.parametrize("channels", CHANNELS)
def test_a_single_pixel_image_is_unchanged_by_the_smoothing_and_morphology(channels):
    image = random_image((1, 1), channels)

    for name in IDEMPOTENT_ON_CONSTANT:
        for radius in RADII:
            assert np.array_equal(FILTERS[name](image, radius), image), (name, radius)

    for name in ZERO_ON_CONSTANT:
        for radius in RADII:
            out = FILTERS[name](image, radius)

            # The alpha channel of a 4-channel image is copied, not filtered.
            assert not out[..., :3].any() if channels else not out.any(), (name, radius)


@pytest.mark.parametrize("name", FILTERS)
def test_filters_do_not_modify_their_input(name):
    image = random_image((5, 6), None)
    copy = image.copy()

    FILTERS[name](image, 1000)

    assert np.array_equal(image, copy)


#
# Anisotropic diffusion
#


@pytest.mark.parametrize("channels", CHANNELS)
@pytest.mark.parametrize("shape", TINY_SHAPES)
def test_anisotropic_diffusion_accepts_tiny_images(shape, channels):
    image = random_image(shape, channels)

    params = fluvel.AnisoParams()
    params.iterations = 20

    out = fluvel.anisotropic_diffusion(image, params)

    assert out.shape == image.shape
    assert out.dtype == np.uint8


@pytest.mark.parametrize("channels", CHANNELS)
@pytest.mark.parametrize("shape", TINY_SHAPES)
def test_anisotropic_diffusion_leaves_a_constant_image_unchanged(shape, channels):
    image = constant_image(shape, channels)

    params = fluvel.AnisoParams()
    params.iterations = 20

    assert np.array_equal(fluvel.anisotropic_diffusion(image, params), image)


#
# Noise
#


@pytest.mark.parametrize("channels", CHANNELS)
@pytest.mark.parametrize("shape", TINY_SHAPES)
@pytest.mark.parametrize(
    "noise",
    [
        fluvel.gaussian_noise,
        fluvel.impulsive_noise,
        fluvel.speckle_uniform,
        fluvel.speckle_gamma,
    ],
)
def test_noise_accepts_tiny_images(noise, shape, channels):
    image = random_image(shape, channels)

    out = noise(image)

    assert out.shape == image.shape
    assert out.dtype == np.uint8


#
# find_contours
#


def all_points(result):
    return np.concatenate([result.outer, result.inner])


def test_find_contours_of_a_single_pixel():
    on = fluvel.find_contours(np.full((1, 1), 255, np.uint8))
    off = fluvel.find_contours(np.zeros((1, 1), np.uint8))

    assert (all_points(on) == [0, 0]).all() and len(all_points(on)) >= 1
    assert len(all_points(off)) == 0


@pytest.mark.parametrize("shape", [(1, 2), (2, 1), (2, 2), (1, 7), (7, 1)])
def test_find_contours_of_a_full_thin_mask_stays_in_the_image(shape):
    result = fluvel.find_contours(np.full(shape, 255, np.uint8))

    points = all_points(result)

    assert len(points) >= 1
    assert (points[:, 0] >= 0).all() and (points[:, 0] < shape[1]).all()
    assert (points[:, 1] >= 0).all() and (points[:, 1] < shape[0]).all()


#
# Active contour
#

ACTIVE_CONTOUR_SHAPES = [(1, 1), (1, 2), (2, 1), (2, 2), (1, 9), (9, 1), (3, 3), (4, 6), (5, 5)]


def check_well_formed(result, shape):
    height, width = shape

    assert (result.grid_height, result.grid_width) == (height, width)
    assert result.mask.shape == (height, width)
    assert result.mask.dtype == np.uint8
    assert set(np.unique(result.mask)) <= {0, 255}

    # Every boundary point is a pixel of the image.
    for points in (result.outer, result.inner):
        assert points.dtype == np.int32
        assert points.ndim == 2 and points.shape[1] == 2

        if points.shape[0]:
            assert (points[:, 0] >= 0).all() and (points[:, 0] < width).all()
            assert (points[:, 1] >= 0).all() and (points[:, 1] < height).all()


@pytest.mark.parametrize("channels", CHANNELS)
@pytest.mark.parametrize("shape", ACTIVE_CONTOUR_SHAPES)
def test_active_contour_on_tiny_random_images(shape, channels):
    image = random_image(shape, channels, seed=3)

    check_well_formed(fluvel.active_contour(image), shape)


@pytest.mark.parametrize("channels", CHANNELS)
@pytest.mark.parametrize("shape", ACTIVE_CONTOUR_SHAPES)
def test_active_contour_on_tiny_constant_images(shape, channels):
    # The mean inside and outside are equal: the model cannot start.
    image = constant_image(shape, channels)

    check_well_formed(fluvel.active_contour(image), shape)


@pytest.mark.parametrize("mode", list(fluvel.FailureHandlingMode.__members__.values()))
@pytest.mark.parametrize("channels", CHANNELS)
@pytest.mark.parametrize("shape", [(1, 1), (2, 2), (3, 3)])
def test_active_contour_failure_modes_on_tiny_constant_images(shape, channels, mode):
    params = fluvel.ActiveContourParams()
    params.failure_mode = mode

    image = constant_image(shape, channels)

    check_well_formed(fluvel.active_contour(image, active_contour_params=params), shape)


@pytest.mark.parametrize("channels", CHANNELS)
@pytest.mark.parametrize("shape", [(1, 1), (2, 2), (3, 3), (1, 9), (9, 1)])
def test_active_contour_with_the_cycle2_disabled_on_tiny_images(shape, channels):
    params = fluvel.ActiveContourParams()
    params.cycle2_enabled = False

    image = random_image(shape, channels, seed=5)

    check_well_formed(fluvel.active_contour(image, active_contour_params=params), shape)


@pytest.mark.parametrize("disk_radius", [1, 5, 1000])
@pytest.mark.parametrize("shape", [(1, 1), (2, 2), (3, 3), (4, 6)])
def test_active_contour_with_a_disk_larger_than_the_image(shape, disk_radius):
    # The initial disk is much larger than the image: it must be clipped.
    params = fluvel.ActiveContourParams()
    params.disk_radius = disk_radius

    image = random_image(shape, None, seed=7)

    check_well_formed(fluvel.active_contour(image, active_contour_params=params), shape)


@pytest.mark.parametrize("shape", [(1, 1), (2, 2), (3, 3), (1, 9), (9, 1)])
def test_active_contour_with_a_tiny_initial_mask(shape):
    image = random_image(shape, None, seed=9)

    for value in (0, 255):
        mask = np.full(shape, value, np.uint8)

        check_well_formed(fluvel.active_contour(image, phi_mask=mask), shape)


@pytest.mark.parametrize(
    "mask_shape", [(1, 1), (2, 2), (3, 4), (4, 3), (10, 10)]
)
def test_active_contour_rejects_an_initial_mask_of_another_size(mask_shape):
    image = random_image((4, 4), None)
    mask = np.full(mask_shape, 255, np.uint8)

    with pytest.raises(ValueError):
        fluvel.active_contour(image, phi_mask=mask)


@pytest.mark.parametrize("space", list(fluvel.ColorSpaceOption.__members__.values()))
@pytest.mark.parametrize("shape", [(1, 1), (2, 2), (3, 3), (4, 6)])
def test_active_contour_color_spaces_on_tiny_images(shape, space):
    params = fluvel.RegionModelParams()
    params.color_space = space

    image = random_image(shape, 3, seed=11)

    check_well_formed(fluvel.active_contour(image, region_model_params=params), shape)


@pytest.mark.parametrize("channels", CHANNELS)
@pytest.mark.parametrize("name", ["dilate", "erode"])
@pytest.mark.parametrize("radius", [1, 2, 3, 7, 40])
@pytest.mark.parametrize("shape", [(5, 5), (17, 9), (30, 64), (96, 128)])
def test_dilate_and_erode_match_the_reference_on_regular_images(shape, radius, name, channels):
    image = random_image(shape, channels, seed=8)

    out = FILTERS[name](image, radius)

    # The alpha channel of a 4-channel image is copied, not filtered.
    for plane_in, plane_out in list(zip(planes(image), planes(out)))[:3]:
        expected = reference_filter(plane_in, radius, REFERENCES[name])
        assert np.array_equal(plane_out, expected)


# Only the pixels whose window is entirely inside the image are compared here.
@pytest.mark.parametrize("radius", [1, 2, 3, 4, 7])
@pytest.mark.parametrize("shape", [(9, 9), (15, 20), (31, 17), (64, 48)])
def test_median_matches_the_reference_inside_the_image(shape, radius):
    image = random_image(shape, None, seed=10)

    out = fluvel.median(image, radius)
    expected = reference_filter(image, radius, REFERENCES["median"], clipped=True)

    inside = (slice(radius, shape[0] - radius), slice(radius, shape[1] - radius))

    assert np.array_equal(out[inside], expected[inside])


@pytest.mark.parametrize("radius", [1, 2, 3, 4, 7])
@pytest.mark.parametrize("shape", [(9, 9), (15, 20), (31, 17), (64, 48)])
def test_median_matches_the_reference_up_to_the_borders(shape, radius):
    image = random_image(shape, None, seed=10)

    out = fluvel.median(image, radius)

    assert np.array_equal(out, reference_filter(image, radius, REFERENCES["median"], clipped=True))


#
# Gray, 3-channel and 4-channel images must give the same result
#

SMOOTHING_AND_MORPHOLOGY = ["mean", "median", "dilate", "erode", "opening", "closing", "top_hat", "black_top_hat", "gradient"]


@pytest.mark.parametrize("radius", [1, 2, 3, 5])
@pytest.mark.parametrize("shape", [(1, 1), (2, 2), (3, 7), (8, 8), (30, 41)])
@pytest.mark.parametrize("name", SMOOTHING_AND_MORPHOLOGY)
def test_each_channel_is_filtered_like_a_gray_image(name, shape, radius):
    rng = np.random.default_rng(12)
    rgb = rng.integers(0, 256, (*shape, 3), dtype=np.uint8)
    rgba = np.concatenate([rgb, rng.integers(0, 256, (*shape, 1), dtype=np.uint8)], axis=2)

    out_rgb = FILTERS[name](rgb, radius)
    out_rgba = FILTERS[name](rgba, radius)

    for c in range(3):
        gray = FILTERS[name](np.ascontiguousarray(rgb[..., c]), radius)

        assert np.array_equal(out_rgb[..., c], gray)
        assert np.array_equal(out_rgba[..., c], gray)

    # The alpha channel is copied.
    assert np.array_equal(out_rgba[..., 3], rgba[..., 3])


@pytest.mark.parametrize("shape", [(1, 1), (2, 2), (3, 7), (30, 41)])
def test_anisotropic_diffusion_filters_each_channel_like_a_gray_image(shape):
    rng = np.random.default_rng(13)
    rgb = rng.integers(0, 256, (*shape, 3), dtype=np.uint8)
    rgba = np.concatenate([rgb, rng.integers(0, 256, (*shape, 1), dtype=np.uint8)], axis=2)

    params = fluvel.AnisoParams()
    params.iterations = 5

    out_rgb = fluvel.anisotropic_diffusion(rgb, params)
    out_rgba = fluvel.anisotropic_diffusion(rgba, params)

    for c in range(3):
        gray = fluvel.anisotropic_diffusion(np.ascontiguousarray(rgb[..., c]), params)

        # Each channel is diffused on its own.
        assert np.array_equal(out_rgb[..., c], gray)
        assert np.array_equal(out_rgba[..., c], gray)

    assert np.array_equal(out_rgba[..., 3], rgba[..., 3])
