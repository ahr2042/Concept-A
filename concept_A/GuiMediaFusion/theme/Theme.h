#pragma once

// VISION_OS design tokens, lifted from the Stitch design system
// "Industrial AI Vision Platform" (docs/stitch/DESIGN_TOKENS.md). One place for
// every color/font/metric the GUI uses; buildStyleSheet() turns them into the
// app-wide QSS. The accent hue is switchable at runtime (Settings → ACCENT HUE).

#include <QColor>
#include <QFont>
#include <QString>

namespace theme {

// ── Surfaces ──────────────────────────────────────────────────────────────────
inline constexpr const char* kSurface           = "#131315";
inline constexpr const char* kSurfaceLowest     = "#0e0e10";
inline constexpr const char* kSurfaceLow        = "#1b1b1d";
inline constexpr const char* kSurfaceContainer  = "#201f21";
inline constexpr const char* kSurfaceHigh       = "#2a2a2c";
inline constexpr const char* kSurfaceHighest    = "#353437";
inline constexpr const char* kOnSurface         = "#e5e1e4";
inline constexpr const char* kOnSurfaceVariant  = "#b9cacb";
inline constexpr const char* kOutline           = "#849495";
inline constexpr const char* kOutlineVariant    = "#3a494b";

// ── Fixed roles (accent-independent) ─────────────────────────────────────────
inline constexpr const char* kSecondary         = "#b600f8";   // magenta (deploy/AI)
inline constexpr const char* kSecondaryLight    = "#ebb2ff";
inline constexpr const char* kTertiary          = "#7318ff";
inline constexpr const char* kError             = "#ffb4ab";
inline constexpr const char* kErrorContainer    = "#93000a";
inline constexpr const char* kWarn              = "#ffd54a";
inline constexpr const char* kOk                = "#38e07b";

// Selectable accent hues (design: Settings → UI CUSTOMIZATION → ACCENT HUE).
enum class Accent { Cyan, Magenta, Lilac, Peach };

struct Palette {
    QColor accent;        // bright accent (primary-container in the tokens)
    QColor accentDim;     // primary-fixed-dim
    QColor onAccent;      // text on accent fills
};

Palette   palette();                       // current palette
Accent    accent();
void      setAccent(Accent a);             // does not repolish; caller reapplies QSS
QString   accentName(Accent a);

// ── Typography ────────────────────────────────────────────────────────────────
// Families resolve to the bundled fonts when loadBundledFonts() found them,
// otherwise to system fallbacks.
QString   displayFamily();                 // Space Grotesk
QString   bodyFamily();                    // Geist
QString   monoFamily();                    // JetBrains Mono
void      loadBundledFonts();              // idempotent; call before widgets

QFont     displayFont(int pt, int weight = QFont::Bold);
QFont     bodyFont(int pt, int weight = QFont::Normal);
QFont     monoFont(int pt, int weight = QFont::Medium, qreal letterSpacingPercent = 100.0);

// The generated application stylesheet for the current accent.
QString   buildStyleSheet();

} // namespace theme
