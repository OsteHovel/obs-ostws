obs-ostws
==============

This is based upon [obs-websocket](https://github.com/Palakis/obs-websocket) and [obs-ndi](https://github.com/Palakis/obs-ndi)

This module for OBS has 3 functions:
 * Sending current audio volume of all sources having the filter applied to it on a regular basis
   * I use this to display bars on my stream overview
 * Matching rectangles of provided size and colors with different options
   * Checking the video feed of the source the filter is applied to
   * Sending updates to the client of any changes
   * This is the OBS version of the matching engine in [ostsplit](https://github.com/OsteHovel/ostsplit), if you wanted to you could probably adjust this to work as an autosplitter
 * Sending over pixel data for the rectangles that matches
   * I use this to send over pixel data and then process it through OCR to get the score from Just Dance

This also have some extensions that obs-websocket did not have back in the day 
 * GetSourceFilters
 * AddFilterToSource
 * RemoveFilterFromSource
 * ReorderSourceFilter
 * MoveSourceFilter
 * SetSourceFilterSettings
 * SetSourceFilterVisibility
 * TriggerHotkeyOnSource
 * SendCaptions

All of these methods are now present in [obs-websocket](https://github.com/Palakis/obs-websocket) so I suggest use that instead
