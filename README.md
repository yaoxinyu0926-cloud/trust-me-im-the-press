# Trust Me, I'm the Press!

A 2D journalism and newsroom management game for GitHub Game Off 2026. Shoot photos around town, pick a reporting angle, lay out the front page, and try to make the rent.

## Play the Windows build

Visit the [game page on itch.io](https://xiya0161.itch.io/trust-me-im-the-press). Extract the whole Windows ZIP and launch `Reporter.exe`.

## Open the project

Install Unreal Engine 5.8 and a Windows C++ toolchain supported by the engine. Open `Reporter.uproject`, allow Unreal to build the Reporter module if prompted, and press Play in the `ParkStreet` map.

The public snapshot contains the C++ modules, configuration, map, and gameplay art for the Rev2 build. The editor's Android file-server settings were excluded. The optional soundtrack is supplied with the itch.io build. The source project runs without those tracks. The ending-music assertions in `Reporter.Endings.Settlement` require the original tracks.

## Controls

- WASD to move.
- C to toggle the camera and arrow keys to aim. Use the mouse wheel to zoom and click or press Enter to take a photo.
- Return to the newsroom computer to edit and publish the story.

## Theme

SHIFT is a shift in perspective. The same photo can become a straightforward account, a question, or a sensational headline. What goes on the front page changes how the town reacts.

## AI disclosure and fonts

ChatGPT and Codex assisted with design, implementation, writing, and illustration. Suno generated music used in the playable release. See `Content/Reporter/RuntimeArt/LICENSE_DEJAVU` for the bundled DejaVu fonts.
