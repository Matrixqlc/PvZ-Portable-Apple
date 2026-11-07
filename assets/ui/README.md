# Level-selector artwork

These are original AI-generated UI assets produced for this project with the
built-in imagegen tool, using the approved level-selector concept as a style
reference. They are separate from the user's game resources and contain no
text, progress values or level illustrations. Runtime covers still use the
game's existing plant/zombie art; user state is drawn separately.

- `level-select-wood.png`: transparent, blank painted wood panel with irregular
  dark outline and corner nails; plain plank center for nine-slice scaling.
- `level-select-paper.png`: transparent, blank cream parchment strip, uneven
  edges and corner nails; empty center for selected-level details.
- `level-select-garden.png`: 4:3 suburban garden backdrop with house edge, sky,
  fence and lawn, no characters or UI.
- `level-select-chapters.png`: transparent atlas of five tactile chapter props:
  golden sun, cratered crescent, stone-rimmed pool, ground fog and red-tile roof
  with a chimney. Source rectangles preserve the objects' proportions; no labels
  are baked in. Generated as an original sibling asset after reviewing the
  game's existing scene/Almanac fragments for small-icon readability.

The renderer crops only transparent canvas margins using source rectangles;
the generated files are kept intact. CMake packages these under `ui/` in the
iOS app and beside the native executable. Loading reuses SDL's bundle resource
path and the existing `SexyAppBase::GetImage` PNG decoder, with the original
game-art rendering retained when these optional skins are absent.
