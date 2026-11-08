Groovy OpenMW
=============

Groovy OpenMW is OpenMW with Groovy MiSTer output. It sends the game from your PC over a network cable to a MiSTer FPGA running the [Groovy core](https://github.com/psakhis/Groovy_MiSTer), which shows it on a real CRT with very low lag (a few milliseconds). It's built the same way as my [Groovy Dolphin](https://github.com/zander2k01/groovy-dolphin) port.

**Status:** early. Picture, sound and controllers plugged into the MiSTer all work over the network.

## Download

Windows build (updated automatically): **[groovy-openmw-windows-x64.zip](https://github.com/zander2k01/groovy-openmw/releases/download/latest/groovy-openmw-windows-x64.zip)**

## What you need

- A copy of Morrowind (OpenMW uses its data files)
- A MiSTer with the Groovy core installed
- A wired network connection between the PC and the MiSTer (a gigabit switch or a direct cable)
- A CRT. CRT TVs use 480i or 240p; PC CRT monitors (VGA) can use 480p

## Setup

1. Unzip the download anywhere. Run `openmw-launcher.exe` once and point it at your Morrowind installation, like normal OpenMW.
2. In the launcher, go to **Settings > Groovy MiSTer**, tick **Enable MiSTer Output**, enter your MiSTer's IP address and pick a video mode.
3. In the Groovy core's menu on the MiSTer, turn **Audio** on (it's off by default). To use controllers plugged into the MiSTer, also set **Server > Joysticks** to **Analog**.
4. Start the Groovy core on the MiSTer, then start OpenMW. A message in the corner confirms the connection. If the MiSTer isn't ready yet, OpenMW keeps retrying in the background.

**Sound** goes to the MiSTer together with the picture and also plays on the PC, so screen sharing still has audio. Both can be turned off on the Groovy MiSTer settings tab.

**Controllers** plugged into the MiSTer show up in OpenMW as "MiSTer Controller 1" and "MiSTer Controller 2", with an Xbox-style layout: the MiSTer's buttons 1 to 4 are the right, bottom, top and left face buttons, 5 and 6 the shoulder buttons, 7 Select, 8 Start, and 9 and 10 the triggers. Controllers on the PC work too.

**Video modes:** `480i` (default, sharpest, best for reading text), `240p` (no interlace flicker, more of a retro look), `480p` (31 kHz, PC CRT monitors only, never a CRT TV).

**Tips:**
- A 4:3 window looks best, for example 1280x960 or 1440x1080. Other shapes get black bars.
- Text is small on a TV at 480i. Raising the interface scaling in OpenMW's settings helps a lot.
- Vsync is turned off automatically while Groovy is on, since the MiSTer sets the pace.

## Credits

- psakhis (Sergi Clara), who created Groovy_MiSTer and its PC library. He passed away in 2024.
- The OpenMW team, for OpenMW.

Everything below is OpenMW's original README.

---

OpenMW
======

OpenMW is an open-source open-world RPG game engine that supports playing Morrowind by Bethesda Softworks. You need to own the game for OpenMW to play Morrowind.

OpenMW also comes with OpenMW-CS, a replacement for Bethesda's Construction Set.

* Version: 0.51.0
* License: GPLv3 (see [LICENSE](https://gitlab.com/OpenMW/openmw/-/raw/master/LICENSE) for more information)
* Website: https://www.openmw.org
* IRC: #openmw on irc.libera.chat
* Discord: https://discord.gg/bWuqq2e


Font Licenses:
* DemonicLetters.ttf: SIL Open Font License (see [files/data/fonts/DemonicLettersFontLicense.txt](https://gitlab.com/OpenMW/openmw/-/raw/master/files/data/fonts/DemonicLettersFontLicense.txt) for more information)
* MysticCards.ttf: SIL Open Font License (see [files/data/fonts/MysticCardsFontLicense.txt](https://gitlab.com/OpenMW/openmw/-/raw/master/files/data/fonts/MysticCardsFontLicense.txt) for more information)

Current Status
--------------

The main quests in Morrowind, Tribunal and Bloodmoon are all completable. Some issues with side quests are to be expected (but rare). Check the [bug tracker](https://gitlab.com/OpenMW/openmw/-/issues/?milestone_title=openmw-1.0) for a list of issues we need to resolve before the "1.0" release. Even before the "1.0" release, however, OpenMW boasts some new [features](https://wiki.openmw.org/index.php?title=Features), such as improved graphics and user interfaces.

Pre-existing modifications created for the original Morrowind engine can be hit-and-miss. The OpenMW script compiler performs more thorough error-checking than Morrowind does, meaning that a mod created for Morrowind may not necessarily run in OpenMW. Some mods also rely on quirky behaviour or engine bugs in order to work. We are considering such compatibility issues on a case-by-case basis - in some cases adding a workaround to OpenMW may be feasible, in other cases fixing the mod will be the only option. If you know of any mods that work or don't work, feel free to add them to the [Mod status](https://wiki.openmw.org/index.php?title=Mod_status) wiki page.

Getting Started
---------------

* [Official forums](https://forum.openmw.org/)
* [Installation instructions](https://openmw.readthedocs.io/en/latest/manuals/installation/index.html)
* [Build from source](https://wiki.openmw.org/index.php?title=Development_Environment_Setup)
* [Testing the game](https://wiki.openmw.org/index.php?title=Testing)
* [How to contribute](https://wiki.openmw.org/index.php?title=Contribution_Wanted)
* [Report a bug](https://gitlab.com/OpenMW/openmw/issues) - read the [guidelines](https://wiki.openmw.org/index.php?title=Bug_Reporting_Guidelines) before submitting your first bug!
* [Known issues](https://gitlab.com/OpenMW/openmw/issues?label_name%5B%5D=Bug)

The data path
-------------

The data path tells OpenMW where to find your Morrowind files. If you run the launcher, OpenMW should be able to pick up the location of these files on its own, if both Morrowind and OpenMW are installed properly (installing Morrowind under WINE is considered a proper install).

Command line options
--------------------

    Syntax: openmw <options>
    Allowed options:
      --config arg                          additional config directories
      --replace arg                         settings where the values from the
                                            current source should replace those
                                            from lower-priority sources instead of
                                            being appended
      --user-data arg                       set user data directory (used for
                                            saves, screenshots, etc)
      --resources arg (=resources)          set resources directory
      --help                                print help message
      --version                             print version information and quit
      --data arg (=data)                    set data directories (later directories
                                            have higher priority)
      --data-local arg                      set local data directory (highest
                                            priority)
      --fallback-archive arg (=fallback-archive)
                                            set fallback BSA archives (later
                                            archives have higher priority)
      --start arg                           set initial cell
      --content arg                         content file(s): esm/esp, or
                                            omwgame/omwaddon/omwscripts
      --groundcover arg                     groundcover content file(s): esm/esp,
                                            or omwgame/omwaddon
      --no-sound [=arg(=1)] (=0)            disable all sounds
      --script-all [=arg(=1)] (=0)          compile all scripts (excluding dialogue
                                            scripts) at startup
      --script-all-dialogue [=arg(=1)] (=0) compile all dialogue scripts at startup
      --script-console [=arg(=1)] (=0)      enable console-only script
                                            functionality
      --script-run arg                      select a file containing a list of
                                            console commands that is executed on
                                            startup
      --script-warn [=arg(=1)] (=1)         handling of warnings when compiling
                                            scripts
                                            0 - ignore warnings
                                            1 - show warnings but consider script as
                                            correctly compiled anyway
                                            2 - treat warnings as errors
      --load-savegame arg                   load a save game file on game startup
                                            (specify an absolute filename or a
                                            filename relative to the current
                                            working directory)
      --skip-menu [=arg(=1)] (=0)           skip main menu on game startup
      --new-game [=arg(=1)] (=0)            run new game sequence (ignored if
                                            skip-menu=0)
      --encoding arg (=win1252)             Character encoding used in OpenMW game
                                            messages:

                                            win1250 - Central and Eastern European
                                            such as Polish, Czech, Slovak,
                                            Hungarian, Slovene, Bosnian, Croatian,
                                            Serbian (Latin script), Romanian and
                                            Albanian languages

                                            win1251 - Cyrillic alphabet such as
                                            Russian, Bulgarian, Serbian Cyrillic
                                            and other languages

                                            win1252 - Western European (Latin)
                                            alphabet, used by default
      --fallback arg                        fallback values
      --no-grab [=arg(=1)] (=0)             Don't grab mouse cursor
      --export-fonts [=arg(=1)] (=0)        Export Morrowind .fnt fonts to PNG
                                            image and XML file in current directory
      --activate-dist arg (=-1)             activation distance override
      --random-seed arg (=<impl defined>)   seed value for random number generator
