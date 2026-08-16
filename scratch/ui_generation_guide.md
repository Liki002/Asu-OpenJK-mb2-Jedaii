# Star Wars UI Asset Generation Guide

This guide documents the AI prompts used to generate the modern, minimalist Star Wars console HUD elements for the Ranked Duel project.

## Prompts Used

### 1. Quest Background (`quest_bg.tga`)
Used to generate the rectangular modal frame with a top-center extended title tab:
- **Prompt:** `Minimalist Star Wars UI console border frame with a top-center extended tab outline for a title banner, sleek simple line design, futuristic clean outline, pure black background, no textures, no environment, flat 2D vector game sprite asset, glowing cyan border lines, completely empty and blank, 1024x1024`

### 2. Shop Background (`shop_bg.tga`)
Used to generate the vertical empty list container frame (taller than it is wide, no inner grids/dividers):
- **Prompt:** `Minimalist Star Wars UI vertical outer border outline, no internal lines, no dividers, completely empty inside, sleek simple line design, futuristic clean outline, pure black background, flat 2D vector game sprite asset, glowing cyan border lines, 1024x1024`

### 3. Green BUY Button Outline (`buy_btn.tga`)
Used to generate the interactive BUY button border:
- **Prompt:** `Minimalist Star Wars UI button border frame, glowing green outline, pure black background, completely blank inside, flat 2D vector game sprite asset, clean sleek lines, 512x256`

### 4. Red SELL Button Outline (`sell_btn.tga`)
Since model quota limits make multiple calls challenging and we need the buttons to be **100% geometrically identical**, we create the red version by programmatically shifting the red and green color channels of the BUY button image using Python.

---

## Programmatic Transparency & Packaging (TGA Conversion)
Since JKA requires an alpha transparency channel for TGA assets to mask out the black areas outside the borders, a flood-fill Python script is used (`scratch/convert_with_alpha.py`). It:
1. Flood-fills from the corners to locate all pixels outside the border frame, setting their alpha to `0` (100% transparent).
2. Keeps the glowing border outline pixels at `255` (fully opaque).
3. Identifies the interior of the frame and sets it to a translucent dark overlay (`alpha = 160`) so the game world remains subtly visible behind the overlay text.
