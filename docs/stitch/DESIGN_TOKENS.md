---
name: Industrial AI Vision Platform
colors:
  surface: '#131315'
  surface-dim: '#131315'
  surface-bright: '#39393b'
  surface-container-lowest: '#0e0e10'
  surface-container-low: '#1b1b1d'
  surface-container: '#201f21'
  surface-container-high: '#2a2a2c'
  surface-container-highest: '#353437'
  on-surface: '#e5e1e4'
  on-surface-variant: '#b9cacb'
  inverse-surface: '#e5e1e4'
  inverse-on-surface: '#303032'
  outline: '#849495'
  outline-variant: '#3a494b'
  surface-tint: '#00dbe7'
  primary: '#e1fdff'
  on-primary: '#00363a'
  primary-container: '#00f2ff'
  on-primary-container: '#006a71'
  inverse-primary: '#00696f'
  secondary: '#ebb2ff'
  on-secondary: '#520072'
  secondary-container: '#b600f8'
  on-secondary-container: '#fff6fc'
  tertiary: '#fcf5ff'
  on-tertiary: '#3c0090'
  tertiary-container: '#e3d4ff'
  on-tertiary-container: '#7318ff'
  error: '#ffb4ab'
  on-error: '#690005'
  error-container: '#93000a'
  on-error-container: '#ffdad6'
  primary-fixed: '#74f5ff'
  primary-fixed-dim: '#00dbe7'
  on-primary-fixed: '#002022'
  on-primary-fixed-variant: '#004f54'
  secondary-fixed: '#f8d8ff'
  secondary-fixed-dim: '#ebb2ff'
  on-secondary-fixed: '#320047'
  on-secondary-fixed-variant: '#74009f'
  tertiary-fixed: '#e9ddff'
  tertiary-fixed-dim: '#d1bcff'
  on-tertiary-fixed: '#23005b'
  on-tertiary-fixed-variant: '#5700c9'
  background: '#131315'
  on-background: '#e5e1e4'
  surface-variant: '#353437'
typography:
  display-lg:
    fontFamily: Space Grotesk
    fontSize: 48px
    fontWeight: '700'
    lineHeight: 56px
    letterSpacing: -0.02em
  headline-md:
    fontFamily: Space Grotesk
    fontSize: 24px
    fontWeight: '600'
    lineHeight: 32px
    letterSpacing: 0.01em
  body-base:
    fontFamily: Geist
    fontSize: 14px
    fontWeight: '400'
    lineHeight: 20px
    letterSpacing: 0em
  data-mono:
    fontFamily: JetBrains Mono
    fontSize: 12px
    fontWeight: '500'
    lineHeight: 16px
    letterSpacing: 0.02em
  label-caps:
    fontFamily: JetBrains Mono
    fontSize: 10px
    fontWeight: '700'
    lineHeight: 12px
    letterSpacing: 0.1em
rounded:
  sm: 0.125rem
  DEFAULT: 0.25rem
  md: 0.375rem
  lg: 0.5rem
  xl: 0.75rem
  full: 9999px
spacing:
  unit: 4px
  xs: 4px
  sm: 8px
  md: 16px
  lg: 24px
  xl: 40px
  container-padding: 24px
  gutter: 16px
---

## Brand & Style

This design system is engineered for high-stakes industrial environments where precision, speed, and data integrity are paramount. The brand personality is authoritative, technical, and cutting-edge, evoking the atmosphere of a futuristic command center or a high-end AI research lab. It prioritizes "functional cinematic" aesthetics—balancing the visual depth of a dark-mode workstation with the rigorous clarity required for engineering workflows.

The visual style is a hybrid of **Modern Glassmorphism** and **Technical Minimalism**. By utilizing matte dark surfaces contrasted against razor-sharp glowing accents, the system directs the user’s focus toward critical vision data and camera performance metrics. The emotional response is one of total control and professional confidence, ensuring that even under high information density, the UI feels breathable and structured.

## Colors

The palette is anchored in a "Cinematic Dark" foundation. The primary background uses a **Matte Black**, while functional surfaces are built with **Dark Graphite** layers to create depth without relying on traditional shadows. 

The accent strategy uses **Neon Cyan** as the primary action and focus color, representing active AI processing and connectivity. **Neon Purple** serves as a secondary accent for specialized data sets or secondary telemetry. Status indicators utilize high-saturation emeralds, ambers, and crimsons, enhanced with subtle outer glows to ensure they "pop" against the dark background, mimicking the physical LEDs of industrial hardware.

## Typography

Typography in this design system is selected for technical precision and legibility at high densities. **Space Grotesk** is used for primary headlines and display elements, providing a geometric, futuristic character that aligns with AI and robotics aesthetics. 

**Geist** serves as the primary body face, offering exceptional clarity for documentation and tooltips. For critical telemetry, coordinate data, and code-level camera logs, **JetBrains Mono** is utilized to ensure every character is distinct, preventing errors during rapid data scanning. High-density labels use an all-caps monospaced format to maximize scannability within tight UI constraints.

## Layout & Spacing

This design system utilizes a **Dense Fluid Grid** optimized for 1440p and 4K engineering displays. The layout is built on a 4px base unit, allowing for tight alignment of technical components. 

The dashboard architecture follows a modular "Pane & Slot" approach, where vision feeds occupy primary real estate, surrounded by collapsible sidebars for telemetry and controls. Margins are kept tight (24px) to maximize the "Information-to-Pixel" ratio, while gutters are maintained at 16px to ensure visual separation of complex datasets. On smaller screens, the system reflows into a single-column stack, prioritizing the live camera feed and critical alerts.

## Elevation & Depth

Elevation is conveyed through **Tonal Stacked Layers** and **Glassmorphism** rather than traditional drop shadows. Surfaces are defined by their opacity and backdrop filters:

1.  **Floor:** The base matte black layer (#0A0A0B).
2.  **Panels:** Dark graphite (#1E1E20) at 80% opacity with a 20px backdrop blur, creating a "frosted glass" effect.
3.  **Accents:** Thin, 1px borders using semi-transparent Cyan or Purple with a subtle `box-shadow: 0 0 8px [color]` to simulate a glowing edge.
4.  **Overlays:** High-contrast modals use a 95% opaque graphite surface to pull focus, surrounded by a distinct glowing "active" stroke.

This approach creates a sense of digital depth suitable for a high-performance workstation while maintaining the "flat" industrial feel.

## Shapes

The shape language is "Soft-Industrial." While the aesthetic is sharp and precise, all corners utilize a **Level 1 (Soft)** rounding (4px for standard components, 8px for containers). This slight curvature prevents the UI from feeling "brutal" or dated, providing a modern, polished finish to technical panels. 

Interactive elements like buttons and input fields maintain this consistent 4px radius, while circular elements are reserved exclusively for status indicators and camera lens icons to ensure they are immediately distinguishable from structural UI.

## Components

### Buttons & Inputs
Buttons are rendered with a "Ghost-Glow" style: a transparent body with a 1px glowing cyan border that fills with a cyan-to-purple gradient on hover. Input fields use a recessed dark graphite background with monospaced text for data entry, flashing a cyan underline when focused.

### Data Visualizations
Charts and graphs are high-density, using "Wireframe" aesthetics. Line graphs utilize the Neon Cyan primary color with a subtle outer glow on the data points. Heatmaps for AI vision focus areas use the secondary Purple-to-Cyan gradient to show intensity.

### Status Indicators
Critical for camera testing, these are small circular "LEDs." They feature a core color and a diffused radial glow. "Active" states should pulse slowly (2s duration) to indicate live processing without distracting the user.

### Cards & Panels
Panels are the primary container. They must feature a "Glassmorphic" header containing the panel title in `label-caps` typography and an optional glowing "active" bar at the top edge to signify the currently focused tool or camera feed.

### Vision Feed Overlays
Overlays on live video feeds use ultra-thin (0.5px) vector lines for bounding boxes, with the label positioned at the top-left corner in `data-mono` font. These must be semi-transparent to ensure the underlying video data remains visible.