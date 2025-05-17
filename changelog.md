# v1.0.4
## Backend Changes
 * Fixed randomizer bug
   * If this change breaks existing seeds please enable "Compatibility With Old Seeds" option in mod's settings.
 * Fixed memory leak when resetting level rand state
 * Added endpoint for getting mod news
 * Added endpoint for getting mod statistics
 * Fixed invalid seed bug (NaN)
## Frontend Changes
 * Hopefully fix crash when going to next levels
 * Added a checkbox for enabling/disabling LDM
 * Added error handler for the music/SFX downloader
# v1.0.3
 * Reverted 1.0.2's FMOD change, mod now should work more stable on mobile phones
# v1.0.2
 * Progression data is now reset when leaving from the minigame
   * Previous seed is also being reset when changing to newer one if this behaviour is enabled.
   * Change behaviour using "Reset Previous Seed" option.
 * Fixed cursor being shown while playing levels
 * Fixed two possible crashes due to UB
 * Fixed crash when changing seed
 * Fixed usability issues when chaning seed
 * Fixed music information synchronization
 * Fixed issues with playing music after beating a level
 * Improved FMOD manipulation
 * Skipping levels will increase next level difficulty if this behaviour is enabled
   * Change behaviour using "Update Difficulty After Skip" option.
 * Added debug mode
# v1.0.1
 * Bugfixes
 * Levels can be skipped now
  * Pressing the Skip button costs 10 lives by default.
 * Added changelog
# v1.0.0
 * Initial release
