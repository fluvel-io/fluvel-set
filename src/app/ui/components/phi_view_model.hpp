// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "contour_types.hpp"
#include "phi_editor.hpp"
#include "point.hpp"
#include "point_layer.hpp"

#include <QImage>
#include <QObject>
#include <QPainterPath>

#include <vector>

namespace fluvel
{

struct Span
{
    int y;
    int xLeft;
    int xRight;
};

/**
 * @brief View-model managing the phi image and its visualization.
 *
 * This class acts as an intermediary between a PhiEditor (user input)
 * and a rendered QImage representation of the level-set (phi).
 *
 * Responsibilities:
 * - Maintain and update the internal phi representation
 * - Extract and manage contour boundaries (inner/outer)
 * - Compose a displayable image combining background, phi, and overlays
 * - React to editor updates and propagate changes to the view
 *
 * The class supports interactive and non-interactive modes and can
 * optionally display an overlay shape.
 *
 * @note The associated PhiEditor is not owned by this class.
 *       The caller must ensure its lifetime exceeds this instance.
 */
class PhiViewModel : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the view-model.
     *
     * @param editor Pointer to the associated PhiEditor (not owned).
     * @param connectivity Neighborhood connectivity (4 or 8).
     * @param parent Optional QObject parent.
     *
     * @pre editor must not be null.
     */
    explicit PhiViewModel(PhiEditor* editor, fluvel_ip::Connectivity connectivity,
                          QObject* parent = nullptr);

    /**
     * @brief Returns the current phi image.
     *
     * @return Reference to the internal phi image.
     */
    const QImage& phi() const
    {
        return phi_;
    }

    /// Enables overlay visualization.
    void showOverlay();

    /// Disables overlay visualization.
    void hideOverlay();

    /**
     * @brief Sets the overlay shape.
     *
     * @param overlayShape Shape to display on top of phi.
     */
    void setOverlay(const ShapeInfo& overlayShape);

    /**
     * @brief Sets the neighborhood connectivity.
     *
     * @param c Connectivity (4 or 8).
     */
    void setConnectivity(fluvel_ip::Connectivity c);

    /**
     * @brief Enables or disables interactive mode.
     *
     * In interactive mode, updates may be performed incrementally
     * or more frequently depending on user input.
     *
     * @param enabled True to enable interactive mode.
     */
    void setInteractiveMode(bool enabled);

    /**
     * @brief Set the colors of the outer and inner contours.
     *
     * Same colors as the ones chosen in the display settings, so the
     * initialization shape is drawn like the final contour.
     */
    void setContourColors(const QColor& outer, const QColor& inner);

    /**
     * @brief Updates the model from the editor state.
     *
     * Recomputes contour lists and updates the phi representation.
     */
    void updateFromEditor();

    /**
     * @brief Clears the model state from the editor.
     *
     * Typically resets internal data structures.
     */
    void onClearFromEditor();

    /**
     * @brief Sets the background image.
     *
     * The background is used when composing the displayed view.
     *
     * @param image Input background image.
     */
    void setBackground(const QImage& image);

signals:
    /**
     * @brief Emitted when the displayed view changes.
     *
     * The image is the background only: phi (see layersChanged()) and the
     * overlay shape (see overlayChanged()) are sent separately.
     *
     * @param image Updated image ready for display.
     */
    void viewChanged(const QImage& image);

    /**
     * @brief Emitted when the boundaries of phi change.
     *
     * The outer and inner boundaries are sent as two layers of points (in
     * the order outer, inner) to be drawn over the image by the viewer.
     *
     * @param layers Layers of points, in image coordinates.
     */
    void layersChanged(const QVector<fluvel::PointLayer>& layers);

    /**
     * @brief Emitted when the overlay shape changes.
     *
     * @param path Outline of the shape, in image coordinates.
     */
    void overlayChanged(const QPainterPath& path);

    /// Emitted when the overlay shape must no longer be displayed.
    void overlayCleared();

private:
    /**
     * @brief Updates boundary lists from the current phi/editor state.
     */
    void updateLists();

    /// Sends the boundaries of phi to the view.
    void emitLayers();

    /// Sends the overlay shape to the view, or clears it.
    void emitOverlay();

    /// Associated editor (not owned).
    PhiEditor* editor_;

    /// Background image.
    QImage background_;

    /// Internal phi representation.
    QImage phi_;

    /// Outer contour boundary.
    std::vector<fluvel_ip::Point2D_i> outerBoundary_;

    /// Inner contour boundary.
    std::vector<fluvel_ip::Point2D_i> innerBoundary_;

    /// Grid size used for contour processing.
    QSize listsGridSize_;

    /// Optional overlay shape.
    ShapeInfo overlayShape_;

    /// Connectivity mode (4 or 8).
    fluvel_ip::Connectivity connectivity_;

    /// True if interactive mode is enabled.
    bool interactiveMode_{false};
    QColor outerColor_;
    QColor innerColor_;

    /// True if overlay is currently visible.
    bool overlayVisible_{false};
};

} // namespace fluvel
