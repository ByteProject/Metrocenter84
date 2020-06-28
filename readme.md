
# Metrocenter '84

Metrocenter '84 is an Inform 6 library optimized for classic 8-bit and 16-bit computer systems. It allows you to create very small Z-code files while retaining the typical Inform experience. Z-machine v3 is the standard target but the full range of v3 to v8 is supported. Metrocenter '84 is squeezed down to 20k for a game containing one object and one room. The library's name derives from [this song by Sunset Neon](https://www.youtube.com/watch?v=wKK4HIkepuY). See it as a journey back to the glory days of cheeky pop culture, where everything was still measured in kilobytes. We love the 80s and the text adventure heritage the decade brought to this world. If you ever felt the desire to create an Infocom style interactive fiction game for one of the home computers of your childhood, you came to the right place. Some stories are not told yet and we can't wait to see yours.

![Hibernated 2 running in Ozmoo on the C64](https://p196.p4.n0.cdn.getcloudapp.com/items/wbuWYXJb/hib2_ozmoo.png "Hibernated 2 running in Ozmoo on the C64")

## Origins

Metrocenter '84 started as an in-house project at [Puddle Software](http://puddlesoft.net/), with the purpose of writing a sophisticated sequel to our award-winning adventure [Hibernated](https://8bitgames.itch.io/hibernated1). We used the wonderful mInform library by Dave Bernazzani as a base and then ended up adding new features, backporting routines and fixing bugs. The changelog in this repository helps you understand the progress. We also added a customized version of Andy Kosela's amazing i6 utility.

## Status

Metrocenter '84 library: `v1.3` codename `Arcturus`.

We are actively working on the library, so feel free to come back every now an then and check if we pushed a new version. The project is considered stable and usable.

## Features

* low memory footprint compared to the standard Inform 6 library, optimized for retro computers
* modular routines for various things like clothing and parsing of numbers
* metro_scenery lib function for flexible, memory efficient, non-object consuming scenery
* library feature for on / off flags that only take a single bit of memory
* several useful lib features were backported from current Inform
* by standard the library uses a supplied set of abbreviations but you may add your own as well
* it's possible to completely disable Inform's built-in inference
* when compiling, an Atari 8-bit disk image is created too, allowing instant testing on 8-bit

## Documentation and examples

Please head to the [wiki](https://github.com/ByteProject/Metrocenter84/wiki) for the library documentation. In addition, we provided a reasonable set of tutorial files, literature and references in the `tutor` folder. A template (shell) as a starting point for your own adventure, a sample game (the famous Cloak of Darkness) and various files showcasing features unique to Metrocenter '84 can be found in `demos`.

## Inform 6 compiler requirements

You need to use compiler version `6.34` or newer for Metrocenter '84 to work. Thanks to Andrew Plotkin for the bugfixes that make the compiler properly target z3 targets again, also for the static array feature, which is highly benefitial on 8-bit machines:

`Array arrayname static --> 1 3 5 7 9 11 13 15;`

## Building and installing

If you are neither interested in using the wrapper utility nor the installer, you find the library files in the `metro84` folder, which likely is the scenario on a Windows-machine. You need a Unix-like operating system (Linux is recommended) to take full advantage of Metrocenter '84. Download the compiler sources from [here](https://github.com/DavidKinder/Inform6) and put them in the `src` folder. Then, you only need to:

  `$ make`
  
  `$ make install`

## Usage

  `$ i6 game.inf`

This will create game.z3 and game.atr. Yes, it's that simple.

Please refer to the synopsis of the compiler in case you're not using the i6 utility.

## Credits

Thanks to Graham Nelson, words can hardly express his gift to the interactive fiction community. Furthermore, the following amazing human beings have contributed directly or indirectly to this project: Dave Bernazzani, Andrew Plotkin, Fredrik Ramsberg, Sean Barrett, Hugo Labrande, Andy Kosela, Roger Firth, Sonja Kesserich, Lea Albaugh, Gareth Rees, Will Crowther, Don Woods, Donald Ekman, David M. Baggett, Scott Adams, Joe W. Aultman, David Cornelson.

## Resources

[Inform 6 | http://www.inform-fiction.org](http://www.inform-fiction.org)

[Inform 6 compiler | https://github.com/DavidKinder/Inform6](https://github.com/DavidKinder/Inform6)

[The Brass Lantern | http://www.brasslantern.org](http://www.brasslantern.org)

[The Interactive Fiction Archive | http://www.ifarchive.org](http://www.ifarchive.org)

[IFDB - https://ifdb.tads.org/](https://ifdb.tads.org/)

[IF on Wikipedia | http://en.wikipedia.org/wiki/Interactive_fiction](http://en.wikipedia.org/wiki/Interactive_fiction])

## License

Just like the original Inform 6, the contents of this repository are published under the Artistic License 2.0. See file `license` for details.
