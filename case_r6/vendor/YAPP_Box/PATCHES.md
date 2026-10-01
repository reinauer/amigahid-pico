Local r6 patch to `printSnapJoins`:

- Adds optional `yappSnapOuterSkin` and `yappSnapRadialSlack` settings.
- Clips lid receiver cutouts to the interior, leaving a continuous outside skin.
- Clips mating base snap bumps to the same inset plus radial clearance.
- Retains the original module as `printSnapJoinsOriginal` and preserves upstream
  behavior when the skin is unset or zero.

r6 uses a 0.8 mm exterior skin and 0.15 mm radial clearance. Both base and lid
must be regenerated together. Upstream revision is recorded in REVISION.txt.
